// EH3D-NODE-BUNGARD-AUX - main/stream.cpp: the video as an IP camera sends it (RTSP and ONVIF). GPL-3.0-or-later.
// Written against ESP-IDF 5.5 and NOT built or run on a board: the words (the RTSP requests and answers, the RTP payload of the pictures, the SOAP services and the
// discovery) are in core/ and are tested on a computer; this file only carries them over lwIP sockets.
#include "stream.hpp"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#include "esp_camera.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"

#include "../core/onvif.hpp"
#include "../core/rtp_jpeg.hpp"
#include "../core/rtsp.hpp"
#include "../core/rtsp_session.hpp"

namespace stream {

namespace {

const char* TAG = "aux-stream";
bungard_aux::Settings cfg;

std::string own_ip() {
    esp_netif_t* n = esp_netif_get_handle_from_ifkey("ETH_DEF");
    esp_netif_ip_info_t ip = {};
    if (!n || esp_netif_get_ip_info(n, &ip) != ESP_OK) return "0.0.0.0";
    char buf[16];
    esp_ip4addr_ntoa(&ip.ip, buf, sizeof buf);
    return buf;
}

std::string random_hex(int bytes) {
    std::string s;
    char b[3];
    for (int i = 0; i < bytes; ++i) {
        std::snprintf(b, sizeof b, "%02X", static_cast<unsigned>(esp_random() & 0xFF));
        s += b;
    }
    return s;
}

bungard_aux::OnvifConfig onvif_config() {
    bungard_aux::OnvifConfig c;
    c.ip = own_ip();
    c.http_port = cfg.port;
    c.rtsp_port = cfg.rtsp_port;
    c.name = cfg.name;
    c.user = cfg.stream_user;
    c.password = cfg.stream_password;
    c.video = {cfg.camera_width, cfg.camera_height, cfg.camera_fps_max};
    c.quality = cfg.camera_quality;
    c.sizes = bungard_aux::sizes_of(cfg.camera_model);
    std::uint8_t mac[6] = {};
    esp_read_mac(mac, ESP_MAC_ETH);
    char id[40];
    std::snprintf(id, sizeof id, "0e3d0b5c-1f2a-4c8e-9b7d-%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    c.uuid = id;
    std::snprintf(id, sizeof id, "AUX-%02X%02X%02X", mac[3], mac[4], mac[5]);
    c.serial = id;
    return c;
}

// ------------------------------------------------------------------------------------------------------------------------------ RTSP

bool send_all(int fd, const void* data, std::size_t n) {
    const char* p = static_cast<const char*>(data);
    while (n > 0) {
        int w = send(fd, p, n, 0);
        if (w <= 0) return false;
        p += w;
        n -= static_cast<std::size_t>(w);
    }
    return true;
}

struct Client {
    int fd;
    sockaddr_in peer;
};

// Each connection is a task with a buffer of a picture: only a few at a time, and one that has not started to play within a short time (a player that connected
// and said nothing, or never logged in) is dropped.
constexpr int kMaxRtsp = 3;
constexpr std::int64_t kSetupLimitUs = 20 * 1000000;
std::atomic<int> rtsp_open{0};

void rtsp_connection(void* arg) {
    Client cl = *static_cast<Client*>(arg);
    delete static_cast<Client*>(arg);
    const std::int64_t born_us = esp_timer_get_time();
    bungard_aux::RtspConfig rc;
    rc.user = cfg.stream_user;
    rc.password = cfg.stream_password;
    rc.name = cfg.name;
    rc.ip = own_ip();
    rc.video = {cfg.camera_width, cfg.camera_height, cfg.camera_fps_max};
    bungard_aux::RtspSession session(rc, random_hex(8), random_hex(4));
    int udp = -1;
    std::string inbox;
    std::uint16_t seq = static_cast<std::uint16_t>(esp_random());
    std::uint32_t ssrc = esp_random();
    std::int64_t next_frame_us = 0;
    bool run = true;
    while (run && !session.ended()) {
        // wait for a request, or for the time of the next picture when one is being sent
        timeval tv = {};
        tv.tv_sec = session.playing() ? 0 : 5;
        tv.tv_usec = session.playing() ? 20 * 1000 : 0;
        fd_set rs;
        FD_ZERO(&rs);
        FD_SET(cl.fd, &rs);
        int ready = select(cl.fd + 1, &rs, nullptr, nullptr, &tv);
        if (ready < 0) break;
        if (!session.playing() && esp_timer_get_time() - born_us > kSetupLimitUs) break;  // it did not get to play in time
        if (ready > 0) {
            char buf[1024];
            int n = recv(cl.fd, buf, sizeof buf, 0);
            if (n <= 0) break;
            inbox.append(buf, static_cast<std::size_t>(n));
            if (inbox.size() > 8192) break;  // a request that never ends
            while (true) {
                // interleaved frames of the player (its RTCP) start with '$' and are dropped
                if (!inbox.empty() && inbox[0] == '$') {
                    if (inbox.size() < 4) break;
                    std::size_t len = (static_cast<unsigned char>(inbox[2]) << 8) | static_cast<unsigned char>(inbox[3]);
                    if (inbox.size() < 4 + len) break;
                    inbox.erase(0, 4 + len);
                    continue;
                }
                std::size_t end = inbox.find("\r\n\r\n");
                if (end == std::string::npos) break;
                std::string text = inbox.substr(0, end + 4);
                inbox.erase(0, end + 4);
                bungard_aux::RtspRequest req;
                if (!bungard_aux::parse_rtsp_request(text, req)) {
                    run = false;
                    break;
                }
                bungard_aux::Next next = bungard_aux::Next::Nothing;
                std::string answer = session.handle(req, next);
                if (!send_all(cl.fd, answer.data(), answer.size())) {
                    run = false;
                    break;
                }
                if (next == bungard_aux::Next::Start && !session.transport().tcp && udp < 0) {
                    udp = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
                    sockaddr_in local = {};
                    local.sin_family = AF_INET;
                    local.sin_addr.s_addr = htonl(INADDR_ANY);
                    local.sin_port = htons(static_cast<std::uint16_t>(rc.server_rtp_port));
                    if (udp < 0 || bind(udp, reinterpret_cast<sockaddr*>(&local), sizeof local) != 0) {
                        // the port announced to the player is taken (another session has it): this session cannot send its picture
                        ESP_LOGW(TAG, "RTSP: the port %u for the picture is not free", static_cast<unsigned>(rc.server_rtp_port));
                        if (udp >= 0) close(udp);
                        udp = -1;
                        run = false;
                        break;
                    }
                }
                if (next == bungard_aux::Next::Start) next_frame_us = 0;
            }
        }
        if (session.playing() && esp_timer_get_time() >= next_frame_us) {
            camera_fb_t* fb = esp_camera_fb_get();
            if (fb) {
                auto packets = bungard_aux::rtp_jpeg_packets(fb->buf, fb->len, seq, static_cast<std::uint32_t>(esp_timer_get_time() * 9 / 100), ssrc);
                seq = static_cast<std::uint16_t>(seq + packets.size());
                for (const auto& p : packets) {
                    if (session.transport().tcp) {
                        std::uint8_t head[4] = {'$', static_cast<std::uint8_t>(session.transport().a), static_cast<std::uint8_t>(p.size() >> 8), static_cast<std::uint8_t>(p.size())};
                        if (!send_all(cl.fd, head, 4) || !send_all(cl.fd, p.data(), p.size())) {
                            run = false;
                            break;
                        }
                    } else if (udp >= 0) {
                        sockaddr_in to = cl.peer;
                        to.sin_port = htons(static_cast<std::uint16_t>(session.transport().a));
                        sendto(udp, p.data(), p.size(), 0, reinterpret_cast<sockaddr*>(&to), sizeof to);
                    }
                }
                esp_camera_fb_return(fb);
            }
            next_frame_us = esp_timer_get_time() + 1000000 / (cfg.camera_fps_max ? cfg.camera_fps_max : 1);
        }
    }
    if (udp >= 0) close(udp);
    close(cl.fd);
    rtsp_open.fetch_sub(1);
    vTaskDelete(nullptr);
}

void rtsp_server(void*) {
    int srv = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    int yes = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
    sockaddr_in a = {};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    a.sin_port = htons(cfg.rtsp_port);
    if (bind(srv, reinterpret_cast<sockaddr*>(&a), sizeof a) != 0 || listen(srv, 2) != 0) {
        ESP_LOGE(TAG, "RTSP: the port %u is not free", static_cast<unsigned>(cfg.rtsp_port));
        vTaskDelete(nullptr);
        return;
    }
    while (true) {
        auto* c = new Client();
        socklen_t len = sizeof c->peer;
        c->fd = accept(srv, reinterpret_cast<sockaddr*>(&c->peer), &len);
        if (c->fd < 0) {
            delete c;
            continue;
        }
        if (rtsp_open.load() >= kMaxRtsp) {  // too many: the connection is closed at once
            close(c->fd);
            delete c;
            continue;
        }
        rtsp_open.fetch_add(1);
        if (xTaskCreate(rtsp_connection, "rtsp", 8192, c, 5, nullptr) != pdPASS) {
            rtsp_open.fetch_sub(1);
            close(c->fd);
            delete c;
        }
    }
}

// ------------------------------------------------------------------------------------------------------------------------------ ONVIF

esp_err_t soap_handler(httpd_req_t* req) {
    if (req->content_len == 0 || req->content_len > 16384) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad request");
        return ESP_OK;
    }
    std::string body(req->content_len, '\0');
    std::size_t got = 0;
    while (got < body.size()) {
        int n = httpd_req_recv(req, &body[got], body.size() - got);
        if (n <= 0) return ESP_FAIL;
        got += static_cast<std::size_t>(n);
    }
    char now[24] = {};
    std::time_t t = std::time(nullptr);
    std::tm tmv = {};
    gmtime_r(&t, &tmv);
    if (t > 1700000000) std::strftime(now, sizeof now, "%Y-%m-%d %H:%M:%S", &tmv);
    bungard_aux::SoapAnswer a = bungard_aux::onvif_handle(onvif_config(), body, now);
    httpd_resp_set_status(req, a.http_status == 200 ? "200 OK" : "400 Bad Request");
    httpd_resp_set_type(req, "application/soap+xml; charset=utf-8");
    httpd_resp_send(req, a.body.c_str(), a.body.size());
    return ESP_OK;
}

esp_err_t snapshot_handler(httpd_req_t* req) {
    // the same login as the stream: Basic, with the user and the password of the stream
    char value[160];
    bool ok = false;
    if (httpd_req_get_hdr_value_str(req, "Authorization", value, sizeof value) == ESP_OK) {
        std::string h = value;
        if (h.compare(0, 6, "Basic ") == 0 && !cfg.stream_password.empty())
            ok = bungard_aux::same_secret(bungard_aux::base64_decode(h.substr(6)), cfg.stream_user + ":" + cfg.stream_password);
    }
    if (!ok) {
        httpd_resp_set_status(req, "401 Unauthorized");
        httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"EH3D BUNGARD AUX\"");
        httpd_resp_send(req, "", 0);
        return ESP_OK;
    }
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no picture");
        return ESP_OK;
    }
    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_send(req, reinterpret_cast<const char*>(fb->buf), fb->len);
    esp_camera_fb_return(fb);
    return ESP_OK;
}

void discovery_task(void*) {
    int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    int yes = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
    sockaddr_in a = {};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    a.sin_port = htons(3702);
    if (bind(s, reinterpret_cast<sockaddr*>(&a), sizeof a) != 0) {
        ESP_LOGE(TAG, "ONVIF discovery: the port 3702 is not free");
        vTaskDelete(nullptr);
        return;
    }
    ip_mreq mreq = {};
    mreq.imr_multiaddr.s_addr = inet_addr("239.255.255.250");
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof mreq);
    char buf[2048];
    while (true) {
        sockaddr_in from = {};
        socklen_t len = sizeof from;
        int n = recvfrom(s, buf, sizeof buf - 1, 0, reinterpret_cast<sockaddr*>(&from), &len);
        if (n <= 0) continue;
        std::string xml(buf, static_cast<std::size_t>(n));
        if (!bungard_aux::is_discovery_probe(xml)) continue;
        vTaskDelay(pdMS_TO_TICKS(esp_random() % 200));  // the standard asks for a random delay, so that many cameras do not answer in the same instant
        std::string answer = bungard_aux::probe_match(onvif_config(), bungard_aux::probe_message_id(xml), random_hex(8) + "-0000-0000-0000-000000000000");
        sendto(s, answer.data(), answer.size(), 0, reinterpret_cast<sockaddr*>(&from), len);
    }
}

}  // namespace

void register_http(httpd_handle_t h) {
    if (cfg.onvif_enabled) {
        httpd_uri_t device = {"/onvif/device_service", HTTP_POST, soap_handler, nullptr};
        httpd_uri_t media = {"/onvif/media_service", HTTP_POST, soap_handler, nullptr};
        httpd_register_uri_handler(h, &device);
        httpd_register_uri_handler(h, &media);
    }
    if (cfg.onvif_enabled || cfg.rtsp_enabled) {
        httpd_uri_t snap = {"/snapshot.jpg", HTTP_GET, snapshot_handler, nullptr};
        httpd_register_uri_handler(h, &snap);
    }
}

void start(const bungard_aux::Settings& s) {
    cfg = s;
    if (cfg.stream_password.empty()) ESP_LOGW(TAG, "stream_password is empty: RTSP and ONVIF serve nobody until it is set");
    if (cfg.rtsp_enabled) xTaskCreate(rtsp_server, "rtsp-srv", 4096, nullptr, 5, nullptr);
    if (cfg.onvif_enabled) xTaskCreate(discovery_task, "onvif-disc", 6144, nullptr, 4, nullptr);
}

}  // namespace stream
