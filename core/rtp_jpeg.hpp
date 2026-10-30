// EH3D-NODE-BUNGARD-AUX - core/rtp_jpeg.hpp
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
// The camera gives a JPEG for each picture; RTSP players take the video as RTP with the payload of RFC 2435 (type 26, "JPEG over RTP"). This reads what the payload
// needs from a baseline JPEG (the size, the sampling, the two quantization tables and where the entropy-coded data starts) and cuts the picture into RTP packets that
// fit the network, with the quantization tables in the first one. Nothing here touches the network or the camera.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace bungard_aux {

struct JpegInfo {
    bool ok = false;
    int width = 0;
    int height = 0;
    int type = 0;                         // 0: 4:2:2 (the luminance is sampled 2x1), 1: 4:2:0 (2x2), as RFC 2435 numbers them
    std::vector<std::uint8_t> luma;       // the 64 bytes of the luminance table, in the order of the file
    std::vector<std::uint8_t> chroma;     // the 64 bytes of the chrominance table
    std::size_t data_start = 0;           // where the entropy-coded data starts (after the header of the scan)
    std::size_t data_end = 0;             // where it ends (before the end-of-image marker)
    bool restart = false;                 // the file has restart intervals, which this payload does not carry
};

/// Reads a baseline JPEG with three components (the camera's output). `ok` is false for anything else.
inline JpegInfo parse_jpeg(const std::uint8_t* d, std::size_t n) {
    JpegInfo j;
    if (n < 4 || d[0] != 0xFF || d[1] != 0xD8) return j;
    std::size_t i = 2;
    bool have_sof = false;
    while (i + 4 <= n) {
        if (d[i] != 0xFF) return j;
        std::uint8_t m = d[i + 1];
        if (m == 0xFF) { ++i; continue; }
        std::size_t len = (static_cast<std::size_t>(d[i + 2]) << 8) | d[i + 3];
        if (len < 2 || i + 2 + len > n) return j;
        const std::uint8_t* p = d + i + 4;
        std::size_t plen = len - 2;
        if (m == 0xDB) {  // quantization tables: one or more of (precision and id, 64 values)
            std::size_t k = 0;
            while (k < plen) {
                int prec = p[k] >> 4, id = p[k] & 15;
                if (prec != 0 || k + 65 > plen) return j;
                std::vector<std::uint8_t> t(p + k + 1, p + k + 65);
                if (id == 0) j.luma = t;
                else if (id == 1) j.chroma = t;
                k += 65;
            }
        } else if (m == 0xC0) {  // baseline frame header
            if (plen < 15 || p[0] != 8 || p[5] != 3) return j;
            j.height = (p[1] << 8) | p[2];
            j.width = (p[3] << 8) | p[4];
            int s = p[7];  // sampling of the first component
            if (s == 0x21) j.type = 0;
            else if (s == 0x22) j.type = 1;
            else return j;
            have_sof = true;
        } else if (m == 0xC2 || m == 0xC1 || m == 0xC3) {
            return j;  // progressive or lossless: not carried
        } else if (m == 0xDD) {
            j.restart = true;
        } else if (m == 0xDA) {  // the scan: its entropy-coded data follows until the end of the image
            j.data_start = i + 2 + len;
            std::size_t e = n;
            while (e >= 2 && !(d[e - 2] == 0xFF && d[e - 1] == 0xD9)) --e;
            if (e < 2) return j;
            j.data_end = e - 2;
            j.ok = have_sof && j.luma.size() == 64 && j.chroma.size() == 64 && j.data_end > j.data_start && j.width > 0 && j.height > 0 && j.width <= 2040 && j.height <= 2040;
            return j;
        }
        i += 2 + len;
    }
    return j;
}

/// The RTP packets of one picture: `payload_max` is the most bytes after the RTP header (1400 fits an Ethernet frame with room for the headers). The timestamp is in
/// the 90 kHz clock of video. The marker bit is set on the last packet of the picture. Returns nothing when the JPEG cannot be carried.
inline std::vector<std::vector<std::uint8_t>> rtp_jpeg_packets(const std::uint8_t* jpeg, std::size_t n, std::uint16_t seq, std::uint32_t timestamp, std::uint32_t ssrc,
                                                               std::size_t payload_max = 1400) {
    std::vector<std::vector<std::uint8_t>> out;
    JpegInfo j = parse_jpeg(jpeg, n);
    if (!j.ok || j.restart || payload_max < 200) return out;
    const std::uint8_t* data = jpeg + j.data_start;
    std::size_t total = j.data_end - j.data_start;
    std::size_t offset = 0;
    while (offset < total) {
        bool first = offset == 0;
        std::size_t head = 8 + (first ? 4 + 128 : 0);
        std::size_t take = total - offset;
        if (take > payload_max - head) take = payload_max - head;
        std::vector<std::uint8_t> p;
        p.reserve(12 + head + take);
        bool last = offset + take == total;
        p.push_back(0x80);                                                 // version 2
        p.push_back(static_cast<std::uint8_t>((last ? 0x80 : 0) | 26));    // marker, payload type 26
        p.push_back(static_cast<std::uint8_t>(seq >> 8));
        p.push_back(static_cast<std::uint8_t>(seq));
        for (int s = 24; s >= 0; s -= 8) p.push_back(static_cast<std::uint8_t>(timestamp >> s));
        for (int s = 24; s >= 0; s -= 8) p.push_back(static_cast<std::uint8_t>(ssrc >> s));
        p.push_back(0);                                                    // type-specific
        p.push_back(static_cast<std::uint8_t>(offset >> 16));
        p.push_back(static_cast<std::uint8_t>(offset >> 8));
        p.push_back(static_cast<std::uint8_t>(offset));
        p.push_back(static_cast<std::uint8_t>(j.type));
        p.push_back(255);                                                  // Q 255: the tables are in the first packet
        p.push_back(static_cast<std::uint8_t>(j.width / 8));
        p.push_back(static_cast<std::uint8_t>(j.height / 8));
        if (first) {
            p.push_back(0);                                                // must be zero
            p.push_back(0);                                                // 8-bit tables
            p.push_back(0);
            p.push_back(128);                                              // 2 x 64 bytes
            p.insert(p.end(), j.luma.begin(), j.luma.end());
            p.insert(p.end(), j.chroma.begin(), j.chroma.end());
        }
        p.insert(p.end(), data + offset, data + offset + take);
        out.push_back(std::move(p));
        offset += take;
        ++seq;
    }
    return out;
}

}  // namespace bungard_aux
