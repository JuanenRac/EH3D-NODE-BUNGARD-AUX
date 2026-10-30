// EH3D-NODE-BUNGARD-AUX - core/rtsp_session.hpp
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
// The session of one RTSP connection, as an IP camera keeps it: SDP of the video, the transport the player chooses (UDP or interleaved in the TCP connection), the login
// with Digest or Basic, and the states Init, Ready, Playing and Ended. `RtspSession::handle` takes a request and gives the text of the answer and says what the sender
// of the pictures has to do next.
#pragma once

#include <cstdio>
#include <string>

#include "rtsp.hpp"

namespace bungard_aux {

/// What the video is, for the SDP.
struct VideoInfo {
    int width = 640;
    int height = 480;
    int fps = 10;
};

/// The SDP that describes the stream: one video of JPEG over RTP.
inline std::string sdp_text(const std::string& ip, const std::string& name, const VideoInfo& v, const std::string& control) {
    std::string s;
    s += "v=0\r\n";
    s += "o=- 1 1 IN IP4 " + ip + "\r\n";
    s += "s=" + name + "\r\n";
    s += "c=IN IP4 0.0.0.0\r\n";
    s += "t=0 0\r\n";
    s += "a=control:*\r\n";
    s += "a=range:npt=0-\r\n";
    s += "m=video 0 RTP/AVP 26\r\n";
    s += "a=rtpmap:26 JPEG/90000\r\n";
    s += "a=framerate:" + std::to_string(v.fps) + "\r\n";
    s += "a=x-dimensions:" + std::to_string(v.width) + "," + std::to_string(v.height) + "\r\n";
    s += "a=control:" + control + "\r\n";
    return s;
}

/// The transport the player asked for in SETUP.
struct Transport {
    bool tcp = false;  // the pictures go inside the RTSP connection (interleaved)
    int a = 0, b = 0;  // the client's RTP and RTCP ports, or the two channels of the interleaving
    bool ok = false;
    bool multicast = false;
};

inline Transport parse_transport(const std::string& value) {
    Transport none;
    std::string v = detail::lower(value);
    // the player may list several; the first one the node can do is taken
    std::size_t pos = 0;
    while (pos <= v.size()) {
        std::size_t end = v.find(',', pos);
        std::string one = v.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
        pos = end == std::string::npos ? v.size() + 1 : end + 1;
        if (one.find("rtp/avp") == std::string::npos) continue;
        Transport c;
        c.tcp = one.find("rtp/avp/tcp") != std::string::npos;
        c.multicast = one.find("multicast") != std::string::npos;
        std::string key = c.tcp ? "interleaved=" : "client_port=";
        std::size_t k = one.find(key);
        if (k == std::string::npos) {
            if (c.tcp) {
                c.a = 0;
                c.b = 1;
                c.ok = !c.multicast;
            }
        } else {
            int a = 0, b = -1;
            if (std::sscanf(one.c_str() + k + key.size(), "%d-%d", &a, &b) >= 1) {
                c.a = a;
                c.b = b >= 0 ? b : a + 1;
                c.ok = !c.multicast && a >= 0 && a < 65535;
            }
        }
        if (c.ok) return c;
    }
    return none;
}

/// What the sender of the pictures has to do after a request.
enum class Next { Nothing, Start, Pause, Stop };

struct RtspConfig {
    std::string user;
    std::string password;
    std::string realm = "EH3D BUNGARD AUX";
    std::string ip = "0.0.0.0";  // the address the player used (the node's own)
    std::string name = "bungard-aux";
    std::string path = "/stream1";
    VideoInfo video;
    int server_rtp_port = 6970;  // the node's own pair of ports for the UDP transport
    int timeout_s = 60;
};

/// One RTSP session of one connection.
class RtspSession {
public:
    RtspSession(const RtspConfig& cfg, const std::string& nonce, const std::string& session_id) : cfg_(cfg), nonce_(nonce), session_id_(session_id) {}

    bool playing() const { return state_ == State::Playing; }
    bool ended() const { return state_ == State::Ended; }
    const Transport& transport() const { return transport_; }
    const std::string& session_id() const { return session_id_; }

    /// Answers one request. `next` says what to do with the pictures.
    std::string handle(const RtspRequest& r, Next& next) {
        next = Next::Nothing;
        // the login: every request but OPTIONS asks for it, and a node without a user and a password serves nobody
        if (r.method != "OPTIONS") {
            if (cfg_.user.empty() || cfg_.password.empty()) return reply(r, 401, "Unauthorized", unauthorized_headers());
            if (!check_authorization(r.header("authorization"), r.method, cfg_.user, cfg_.password, cfg_.realm, nonce_)) return reply(r, 401, "Unauthorized", unauthorized_headers());
        }
        if (r.method == "OPTIONS") return reply(r, 200, "OK", "Public: OPTIONS, DESCRIBE, SETUP, PLAY, PAUSE, GET_PARAMETER, TEARDOWN\r\n");
        if (r.method == "DESCRIBE") {
            if (!path_ok(r.uri, false)) return reply(r, 404, "Not Found");
            std::string sdp = sdp_text(cfg_.ip, cfg_.name, cfg_.video, "track1");
            return reply(r, 200, "OK", "Content-Base: rtsp://" + cfg_.ip + cfg_.path + "/\r\nContent-Type: application/sdp\r\n", sdp);
        }
        if (r.method == "SETUP") {
            if (!path_ok(r.uri, true)) return reply(r, 404, "Not Found");
            Transport t = parse_transport(r.header("transport"));
            if (!t.ok) return reply(r, 461, "Unsupported Transport");
            transport_ = t;
            state_ = State::Ready;
            std::string tr = t.tcp ? "RTP/AVP/TCP;unicast;interleaved=" + std::to_string(t.a) + "-" + std::to_string(t.b)
                                   : "RTP/AVP;unicast;client_port=" + std::to_string(t.a) + "-" + std::to_string(t.b) + ";server_port=" + std::to_string(cfg_.server_rtp_port) + "-" +
                                         std::to_string(cfg_.server_rtp_port + 1);
            return reply(r, 200, "OK", "Transport: " + tr + "\r\nSession: " + session_id_ + ";timeout=" + std::to_string(cfg_.timeout_s) + "\r\n");
        }
        if (r.method == "PLAY" || r.method == "PAUSE" || r.method == "TEARDOWN" || r.method == "GET_PARAMETER") {
            std::string given = r.header("session");
            std::size_t semi = given.find(';');
            if (semi != std::string::npos) given = given.substr(0, semi);
            if (state_ == State::Init || given != session_id_) {
                if (r.method == "GET_PARAMETER" && given.empty()) return reply(r, 200, "OK");  // a keep-alive before the setup
                return reply(r, 454, "Session Not Found");
            }
            std::string h = "Session: " + session_id_ + "\r\n";
            if (r.method == "PLAY") {
                state_ = State::Playing;
                next = Next::Start;
                h += "Range: npt=0.000-\r\nRTP-Info: url=rtsp://" + cfg_.ip + cfg_.path + "/track1;seq=0;rtptime=0\r\n";
            } else if (r.method == "PAUSE") {
                state_ = State::Ready;
                next = Next::Pause;
            } else if (r.method == "TEARDOWN") {
                state_ = State::Ended;
                next = Next::Stop;
            }
            return reply(r, 200, "OK", h);
        }
        return reply(r, 501, "Not Implemented");
    }

private:
    enum class State { Init, Ready, Playing, Ended };

    bool path_ok(const std::string& uri, bool track) const {
        std::string u = uri;
        std::size_t scheme = u.find("://");
        if (scheme != std::string::npos) {
            std::size_t slash = u.find('/', scheme + 3);
            u = slash == std::string::npos ? "/" : u.substr(slash);
        }
        std::size_t q = u.find('?');
        if (q != std::string::npos) u = u.substr(0, q);
        while (u.size() > 1 && u.back() == '/') u.pop_back();
        if (track) return u == cfg_.path + "/track1" || u == cfg_.path;
        return u == cfg_.path;
    }

    std::string unauthorized_headers() const {
        return "WWW-Authenticate: Digest realm=\"" + cfg_.realm + "\", nonce=\"" + nonce_ + "\"\r\nWWW-Authenticate: Basic realm=\"" + cfg_.realm + "\"\r\n";
    }

    static std::string reply(const RtspRequest& r, int code, const char* text, const std::string& headers = "", const std::string& body = "") {
        std::string s = "RTSP/1.0 " + std::to_string(code) + " " + text + "\r\nCSeq: " + std::to_string(r.cseq) + "\r\nServer: EH3D-NODE-BUNGARD-AUX\r\n" + headers;
        if (!body.empty()) s += "Content-Length: " + std::to_string(body.size()) + "\r\n";
        s += "\r\n" + body;
        return s;
    }

    RtspConfig cfg_;
    std::string nonce_;
    std::string session_id_;
    State state_ = State::Init;
    Transport transport_;
};

}  // namespace bungard_aux
