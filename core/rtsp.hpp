// EH3D-NODE-BUNGARD-AUX - core/rtsp.hpp
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
// The RTSP side of the node, as an IP camera has it (RFC 2326): the requests OPTIONS, DESCRIBE, SETUP, PLAY, PAUSE, GET_PARAMETER and TEARDOWN, the SDP that
// describes the video (JPEG over RTP, see rtp_jpeg.hpp), the transport over UDP or interleaved in the TCP connection, the login with Digest or Basic, and the state of
// one session. `RtspSession::handle` takes the text of a request and gives the text of the answer, and says what the sender of the pictures has to do next, so the
// state machine is tested without a network.
#pragma once

#include <cstdint>
#include <cstdio>
#include <map>
#include <string>

#include "hash.hpp"

namespace bungard_aux {

struct RtspRequest {
    std::string method;
    std::string uri;
    int cseq = 0;
    std::map<std::string, std::string> headers;  // the names in lower case

    std::string header(const std::string& name) const {
        auto it = headers.find(name);
        return it == headers.end() ? "" : it->second;
    }
};

namespace detail {

inline std::string lower(std::string s) {
    for (auto& c : s)
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return s;
}

inline std::string trim_ws(const std::string& s) {
    std::size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) --b;
    return s.substr(a, b - a);
}

}  // namespace detail

/// Reads the text of a request (up to the empty line): the first line and the headers.
inline bool parse_rtsp_request(const std::string& text, RtspRequest& out) {
    out = RtspRequest{};
    std::size_t pos = text.find('\n');
    if (pos == std::string::npos) return false;
    std::string first = detail::trim_ws(text.substr(0, pos));
    std::size_t a = first.find(' ');
    std::size_t b = first.rfind(' ');
    if (a == std::string::npos || b == a) return false;
    out.method = first.substr(0, a);
    out.uri = detail::trim_ws(first.substr(a + 1, b - a - 1));
    if (first.compare(b + 1, 5, "RTSP/") != 0 || out.uri.empty()) return false;
    while (pos != std::string::npos && pos + 1 < text.size()) {
        std::size_t start = pos + 1;
        pos = text.find('\n', start);
        std::string line = detail::trim_ws(text.substr(start, pos == std::string::npos ? std::string::npos : pos - start));
        if (line.empty()) break;
        std::size_t c = line.find(':');
        if (c == std::string::npos) return false;
        out.headers[detail::lower(detail::trim_ws(line.substr(0, c)))] = detail::trim_ws(line.substr(c + 1));
    }
    std::string cs = out.header("cseq");
    if (cs.empty() || cs.size() > 9) return false;
    for (char ch : cs)
        if (ch < '0' || ch > '9') return false;
    out.cseq = std::stoi(cs);
    return true;
}

// ---------------------------------------------------------------------------------------------------------------- the login

/// The value of `name` in a list of `name="value", name=value` parameters (Digest).
inline std::string auth_param(const std::string& list, const std::string& name) {
    std::size_t pos = 0;
    while (pos < list.size()) {
        std::size_t eq = list.find('=', pos);
        if (eq == std::string::npos) break;
        std::string key = detail::lower(detail::trim_ws(list.substr(pos, eq - pos)));
        std::size_t vstart = eq + 1;
        std::string value;
        std::size_t next;
        if (vstart < list.size() && list[vstart] == '"') {
            std::size_t q = list.find('"', vstart + 1);
            if (q == std::string::npos) break;
            value = list.substr(vstart + 1, q - vstart - 1);
            next = list.find(',', q);
        } else {
            next = list.find(',', vstart);
            value = detail::trim_ws(list.substr(vstart, next == std::string::npos ? std::string::npos : next - vstart));
        }
        if (key == name) return value;
        if (next == std::string::npos) break;
        pos = next + 1;
    }
    return "";
}

/// The `response` of a Digest login (RFC 2617; with or without `qop=auth`).
inline std::string digest_response(const std::string& user, const std::string& realm, const std::string& password, const std::string& method, const std::string& uri,
                                   const std::string& nonce, const std::string& qop = "", const std::string& nc = "", const std::string& cnonce = "") {
    std::string ha1 = md5_hex(user + ":" + realm + ":" + password);
    std::string ha2 = md5_hex(method + ":" + uri);
    if (qop.empty()) return md5_hex(ha1 + ":" + nonce + ":" + ha2);
    return md5_hex(ha1 + ":" + nonce + ":" + nc + ":" + cnonce + ":" + qop + ":" + ha2);
}

/// Whether the `Authorization` header of a request is the login of `user` with `password` (Digest for the nonce the node gave, or Basic).
inline bool check_authorization(const std::string& header, const std::string& method, const std::string& user, const std::string& password, const std::string& realm,
                                const std::string& nonce) {
    if (user.empty() || password.empty()) return false;
    std::string h = detail::trim_ws(header);
    if (detail::lower(h.substr(0, 6)) == "basic ") {
        return same_secret(base64_decode(detail::trim_ws(h.substr(6))), user + ":" + password);
    }
    if (detail::lower(h.substr(0, 7)) != "digest ") return false;
    std::string params = h.substr(7);
    if (auth_param(params, "username") != user || auth_param(params, "realm") != realm || auth_param(params, "nonce") != nonce) return false;
    std::string uri = auth_param(params, "uri");
    std::string qop = auth_param(params, "qop");
    std::string want = digest_response(user, realm, password, method, uri, nonce, qop, auth_param(params, "nc"), auth_param(params, "cnonce"));
    return same_secret(auth_param(params, "response"), want);
}

}  // namespace bungard_aux
