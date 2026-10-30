// EH3D-NODE-BUNGARD-AUX - core/hash.hpp
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
// The small functions that the logins of a camera need: MD5 (the Digest login of RTSP and HTTP), SHA-1 and base64 (the password digest of the ONVIF user token) and
// hexadecimal. They are the plain algorithms of RFC 1321 and RFC 3174, written here so that the node needs no library for them; the tests use the published vectors.
// MD5 and SHA-1 are not used to keep a secret from an attacker here: they are what the cameras' protocols ask for.
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace bungard_aux {

inline std::string to_hex(const std::uint8_t* d, std::size_t n) {
    static const char* h = "0123456789abcdef";
    std::string s;
    for (std::size_t i = 0; i < n; ++i) {
        s += h[d[i] >> 4];
        s += h[d[i] & 15];
    }
    return s;
}

namespace detail {

inline std::uint32_t rol(std::uint32_t x, int c) { return (x << c) | (x >> (32 - c)); }

}  // namespace detail

/// MD5 of a string (16 bytes).
inline std::array<std::uint8_t, 16> md5_bytes(const std::string& msg) {
    static const std::uint32_t K[64] = {
        0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501, 0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122,
        0xfd987193, 0xa679438e, 0x49b40821, 0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8, 0x21e1cde6, 0xc33707d6,
        0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a, 0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60,
        0xbebfbc70, 0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665, 0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
        0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1, 0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};
    static const int S[64] = {7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
                              4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};
    std::uint32_t a0 = 0x67452301, b0 = 0xefcdab89, c0 = 0x98badcfe, d0 = 0x10325476;
    std::vector<std::uint8_t> m(msg.begin(), msg.end());
    std::uint64_t bits = static_cast<std::uint64_t>(msg.size()) * 8;
    m.push_back(0x80);
    while (m.size() % 64 != 56) m.push_back(0);
    for (int i = 0; i < 8; ++i) m.push_back(static_cast<std::uint8_t>(bits >> (8 * i)));
    for (std::size_t off = 0; off < m.size(); off += 64) {
        std::uint32_t M[16];
        for (int i = 0; i < 16; ++i)
            M[i] = static_cast<std::uint32_t>(m[off + 4 * i]) | (static_cast<std::uint32_t>(m[off + 4 * i + 1]) << 8) | (static_cast<std::uint32_t>(m[off + 4 * i + 2]) << 16) |
                   (static_cast<std::uint32_t>(m[off + 4 * i + 3]) << 24);
        std::uint32_t A = a0, B = b0, C = c0, D = d0;
        for (int i = 0; i < 64; ++i) {
            std::uint32_t F;
            int g;
            if (i < 16) { F = (B & C) | (~B & D); g = i; }
            else if (i < 32) { F = (D & B) | (~D & C); g = (5 * i + 1) % 16; }
            else if (i < 48) { F = B ^ C ^ D; g = (3 * i + 5) % 16; }
            else { F = C ^ (B | ~D); g = (7 * i) % 16; }
            F = F + A + K[i] + M[g];
            A = D;
            D = C;
            C = B;
            B = B + detail::rol(F, S[i]);
        }
        a0 += A;
        b0 += B;
        c0 += C;
        d0 += D;
    }
    std::array<std::uint8_t, 16> out{};
    std::uint32_t v[4] = {a0, b0, c0, d0};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) out[4 * i + j] = static_cast<std::uint8_t>(v[i] >> (8 * j));
    return out;
}

/// Compares two strings in the same time whatever they hold, so the time does not tell how much of a guess was right.
inline bool same_secret(const std::string& a, const std::string& b) {
    std::uint8_t diff = static_cast<std::uint8_t>(a.size() != b.size());
    std::size_t n = a.size() < b.size() ? a.size() : b.size();
    for (std::size_t i = 0; i < n; ++i) diff = static_cast<std::uint8_t>(diff | (static_cast<std::uint8_t>(a[i]) ^ static_cast<std::uint8_t>(b[i])));
    return diff == 0;
}

inline std::string md5_hex(const std::string& msg) {
    auto d = md5_bytes(msg);
    return to_hex(d.data(), d.size());
}

/// SHA-1 of a string (20 bytes).
inline std::array<std::uint8_t, 20> sha1_bytes(const std::string& msg) {
    std::uint32_t h0 = 0x67452301, h1 = 0xEFCDAB89, h2 = 0x98BADCFE, h3 = 0x10325476, h4 = 0xC3D2E1F0;
    std::vector<std::uint8_t> m(msg.begin(), msg.end());
    std::uint64_t bits = static_cast<std::uint64_t>(msg.size()) * 8;
    m.push_back(0x80);
    while (m.size() % 64 != 56) m.push_back(0);
    for (int i = 7; i >= 0; --i) m.push_back(static_cast<std::uint8_t>(bits >> (8 * i)));
    for (std::size_t off = 0; off < m.size(); off += 64) {
        std::uint32_t w[80];
        for (int i = 0; i < 16; ++i)
            w[i] = (static_cast<std::uint32_t>(m[off + 4 * i]) << 24) | (static_cast<std::uint32_t>(m[off + 4 * i + 1]) << 16) | (static_cast<std::uint32_t>(m[off + 4 * i + 2]) << 8) |
                   static_cast<std::uint32_t>(m[off + 4 * i + 3]);
        for (int i = 16; i < 80; ++i) w[i] = detail::rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        std::uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;
        for (int i = 0; i < 80; ++i) {
            std::uint32_t f, k;
            if (i < 20) { f = (b & c) | (~b & d); k = 0x5A827999; }
            else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
            else { f = b ^ c ^ d; k = 0xCA62C1D6; }
            std::uint32_t t = detail::rol(a, 5) + f + e + k + w[i];
            e = d;
            d = c;
            c = detail::rol(b, 30);
            b = a;
            a = t;
        }
        h0 += a;
        h1 += b;
        h2 += c;
        h3 += d;
        h4 += e;
    }
    std::array<std::uint8_t, 20> out{};
    std::uint32_t v[5] = {h0, h1, h2, h3, h4};
    for (int i = 0; i < 5; ++i)
        for (int j = 0; j < 4; ++j) out[4 * i + j] = static_cast<std::uint8_t>(v[i] >> (24 - 8 * j));
    return out;
}

inline std::string base64_encode(const std::uint8_t* d, std::size_t n) {
    static const char* t = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string s;
    for (std::size_t i = 0; i < n; i += 3) {
        std::uint32_t v = static_cast<std::uint32_t>(d[i]) << 16;
        if (i + 1 < n) v |= static_cast<std::uint32_t>(d[i + 1]) << 8;
        if (i + 2 < n) v |= d[i + 2];
        s += t[(v >> 18) & 63];
        s += t[(v >> 12) & 63];
        s += i + 1 < n ? t[(v >> 6) & 63] : '=';
        s += i + 2 < n ? t[v & 63] : '=';
    }
    return s;
}

inline std::string base64_encode(const std::string& s) { return base64_encode(reinterpret_cast<const std::uint8_t*>(s.data()), s.size()); }

/// Decodes base64; the empty string for an input that is not base64.
inline std::string base64_decode(const std::string& in) {
    std::string out;
    std::uint32_t acc = 0;
    int bits = 0;
    for (char c : in) {
        int v;
        if (c >= 'A' && c <= 'Z') v = c - 'A';
        else if (c >= 'a' && c <= 'z') v = c - 'a' + 26;
        else if (c >= '0' && c <= '9') v = c - '0' + 52;
        else if (c == '+') v = 62;
        else if (c == '/') v = 63;
        else if (c == '=' || c == '\r' || c == '\n' || c == ' ') continue;
        else return "";
        acc = (acc << 6) | static_cast<std::uint32_t>(v);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out += static_cast<char>((acc >> bits) & 0xFF);
        }
    }
    return out;
}

}  // namespace bungard_aux
