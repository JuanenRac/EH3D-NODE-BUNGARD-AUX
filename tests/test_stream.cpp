// EH3D-NODE-BUNGARD-AUX host tests: the video of the node as an IP camera has it: the hashes of the logins, the RTP payload of JPEG, the RTSP session and ONVIF.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
#include <cstdio>
#include <string>
#include <vector>

#include "../core/hash.hpp"
#include "../core/onvif.hpp"
#include "../core/rtp_jpeg.hpp"
#include "../core/rtsp.hpp"
#include "../core/rtsp_session.hpp"

using namespace bungard_aux;

static int failures = 0;
static int checks = 0;

#define CHECK(cond)                                                     \
    do {                                                                \
        ++checks;                                                       \
        if (!(cond)) {                                                  \
            ++failures;                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
        }                                                               \
    } while (0)

static bool has(const std::string& s, const std::string& part) { return s.find(part) != std::string::npos; }

static void hashes() {
    CHECK(md5_hex("") == "d41d8cd98f00b204e9800998ecf8427e");
    CHECK(md5_hex("abc") == "900150983cd24fb0d6963f7d28e17f72");
    CHECK(md5_hex("The quick brown fox jumps over the lazy dog") == "9e107d9d372bb6826bd81d3542a419d6");
    auto s = sha1_bytes("abc");
    CHECK(to_hex(s.data(), s.size()) == "a9993e364706816aba3e25717850c26c9cd0d89d");
    auto e = sha1_bytes("");
    CHECK(to_hex(e.data(), e.size()) == "da39a3ee5e6b4b0d3255bfef95601890afd80709");
    CHECK(base64_encode("Man") == "TWFu" && base64_encode("Ma") == "TWE=" && base64_encode("M") == "TQ==" && base64_encode("") == "");
    CHECK(base64_decode("TWFu") == "Man" && base64_decode("TWE=") == "Ma" && base64_decode("TQ==") == "M");
    CHECK(base64_decode("not base64!").empty());
    CHECK(same_secret("abc", "abc") && !same_secret("abc", "abd") && !same_secret("abc", "ab"));
    // the example of RFC 2617 for the Digest login
    CHECK(digest_response("Mufasa", "testrealm@host.com", "Circle Of Life", "GET", "/dir/index.html", "dcd98b7102dd2f0e8b11d0f600bfb0c093", "auth", "00000001", "0a4f113b") ==
          "6629fae49393a05397450978507c4ef1");
}

static void request_parsing() {
    RtspRequest r;
    CHECK(parse_rtsp_request("DESCRIBE rtsp://10.0.0.5:554/stream1 RTSP/1.0\r\nCSeq: 3\r\nAccept: application/sdp\r\nUser-Agent: VLC\r\n\r\n", r));
    CHECK(r.method == "DESCRIBE" && r.uri == "rtsp://10.0.0.5:554/stream1" && r.cseq == 3 && r.header("accept") == "application/sdp");
    CHECK(!parse_rtsp_request("DESCRIBE rtsp://x RTSP/1.0\r\nUser-Agent: VLC\r\n\r\n", r));  // no CSeq
    CHECK(!parse_rtsp_request("nonsense", r));
    CHECK(!parse_rtsp_request("GET / HTTP/1.1\r\nCSeq: 1\r\n\r\n", r));
    CHECK(auth_param("username=\"a\", realm=\"R x\", nonce=\"n1\", uri=\"/u\", response=\"ff\"", "realm") == "R x");
    CHECK(auth_param("username=\"a\", nc=00000001, qop=auth", "nc") == "00000001" && auth_param("a=\"1\"", "b").empty());
    Transport t = parse_transport("RTP/AVP;unicast;client_port=5000-5001");
    CHECK(t.ok && !t.tcp && t.a == 5000 && t.b == 5001);
    t = parse_transport("RTP/AVP/TCP;unicast;interleaved=2-3");
    CHECK(t.ok && t.tcp && t.a == 2 && t.b == 3);
    t = parse_transport("RTP/AVP;multicast, RTP/AVP/TCP;unicast;interleaved=0-1");
    CHECK(t.ok && t.tcp);  // the first one the node can do
    CHECK(!parse_transport("RTP/AVP;multicast;port=5000-5001").ok && !parse_transport("garbage").ok);
}

static std::string digest_header(const RtspConfig& c, const std::string& nonce, const std::string& method, const std::string& uri) {
    std::string resp = digest_response(c.user, c.realm, c.password, method, uri, nonce);
    return "Digest username=\"" + c.user + "\", realm=\"" + c.realm + "\", nonce=\"" + nonce + "\", uri=\"" + uri + "\", response=\"" + resp + "\"";
}

static RtspRequest req(const std::string& method, const std::string& uri, int cseq, const std::string& extra = "") {
    RtspRequest r;
    bool ok = parse_rtsp_request(method + " " + uri + " RTSP/1.0\r\nCSeq: " + std::to_string(cseq) + "\r\n" + extra + "\r\n", r);
    (void)ok;
    return r;
}

static void rtsp_flow() {
    RtspConfig cfg;
    cfg.user = "viewer";
    cfg.password = "s3cret-pass";
    cfg.ip = "10.0.0.5";
    cfg.video = {640, 480, 10};
    RtspSession s(cfg, "nonce123", "A1B2C3D4");
    Next next = Next::Nothing;
    const std::string base = "rtsp://10.0.0.5:554/stream1";
    // OPTIONS is open
    std::string a = s.handle(req("OPTIONS", base, 1), next);
    CHECK(has(a, "RTSP/1.0 200 OK") && has(a, "CSeq: 1") && has(a, "Public: OPTIONS, DESCRIBE, SETUP, PLAY"));
    // DESCRIBE without a login: 401 with the two challenges
    a = s.handle(req("DESCRIBE", base, 2), next);
    CHECK(has(a, "401 Unauthorized") && has(a, "WWW-Authenticate: Digest realm=\"EH3D BUNGARD AUX\", nonce=\"nonce123\"") && has(a, "WWW-Authenticate: Basic"));
    // with a wrong password: 401
    RtspConfig bad = cfg;
    bad.password = "wrong";
    a = s.handle(req("DESCRIBE", base, 3, "Authorization: " + digest_header(bad, "nonce123", "DESCRIBE", base) + "\r\n"), next);
    CHECK(has(a, "401 Unauthorized"));
    // with the right one: the SDP
    a = s.handle(req("DESCRIBE", base, 4, "Authorization: " + digest_header(cfg, "nonce123", "DESCRIBE", base) + "\r\n"), next);
    CHECK(has(a, "200 OK") && has(a, "Content-Type: application/sdp") && has(a, "m=video 0 RTP/AVP 26") && has(a, "a=rtpmap:26 JPEG/90000") && has(a, "a=x-dimensions:640,480") &&
          has(a, "a=framerate:10"));
    // a path that is not there
    a = s.handle(req("DESCRIBE", "rtsp://10.0.0.5:554/other", 5, "Authorization: " + digest_header(cfg, "nonce123", "DESCRIBE", "rtsp://10.0.0.5:554/other") + "\r\n"), next);
    CHECK(has(a, "404 Not Found"));
    // SETUP over UDP
    const std::string track = base + "/track1";
    a = s.handle(req("SETUP", track, 6, "Transport: RTP/AVP;unicast;client_port=5000-5001\r\nAuthorization: " + digest_header(cfg, "nonce123", "SETUP", track) + "\r\n"), next);
    CHECK(has(a, "200 OK") && has(a, "client_port=5000-5001;server_port=6970-6971") && has(a, "Session: A1B2C3D4;timeout=60"));
    CHECK(!s.transport().tcp && s.transport().a == 5000);
    // PLAY with the session
    a = s.handle(req("PLAY", base, 7, "Session: A1B2C3D4\r\nAuthorization: " + digest_header(cfg, "nonce123", "PLAY", base) + "\r\n"), next);
    CHECK(has(a, "200 OK") && has(a, "RTP-Info:") && next == Next::Start && s.playing());
    // a keep-alive
    a = s.handle(req("GET_PARAMETER", base, 8, "Session: A1B2C3D4\r\nAuthorization: " + digest_header(cfg, "nonce123", "GET_PARAMETER", base) + "\r\n"), next);
    CHECK(has(a, "200 OK") && next == Next::Nothing && s.playing());
    // PAUSE and PLAY again
    a = s.handle(req("PAUSE", base, 9, "Session: A1B2C3D4\r\nAuthorization: " + digest_header(cfg, "nonce123", "PAUSE", base) + "\r\n"), next);
    CHECK(next == Next::Pause && !s.playing());
    // another session id
    a = s.handle(req("PLAY", base, 10, "Session: FFFF\r\nAuthorization: " + digest_header(cfg, "nonce123", "PLAY", base) + "\r\n"), next);
    CHECK(has(a, "454 Session Not Found") && next == Next::Nothing);
    // a method the node does not do
    a = s.handle(req("RECORD", base, 11, "Authorization: " + digest_header(cfg, "nonce123", "RECORD", base) + "\r\n"), next);
    CHECK(has(a, "501 Not Implemented"));
    // TEARDOWN
    a = s.handle(req("TEARDOWN", base, 12, "Session: A1B2C3D4\r\nAuthorization: " + digest_header(cfg, "nonce123", "TEARDOWN", base) + "\r\n"), next);
    CHECK(has(a, "200 OK") && next == Next::Stop && s.ended());

    // over TCP, with the login of Basic, and a player that asks for multicast
    RtspSession t(cfg, "n2", "SESSION2");
    std::string basic = "Authorization: Basic " + base64_encode("viewer:s3cret-pass") + "\r\n";
    a = t.handle(req("SETUP", track, 1, "Transport: RTP/AVP/TCP;unicast;interleaved=0-1\r\n" + basic), next);
    CHECK(has(a, "200 OK") && has(a, "interleaved=0-1") && t.transport().tcp);
    a = t.handle(req("SETUP", track, 2, "Transport: RTP/AVP;multicast\r\n" + basic), next);
    CHECK(has(a, "461 Unsupported Transport"));
    // the wrong Basic login
    a = t.handle(req("PLAY", base, 3, "Session: SESSION2\r\nAuthorization: Basic " + base64_encode("viewer:nope") + "\r\n"), next);
    CHECK(has(a, "401 Unauthorized"));

    // a node without a user and a password serves nobody
    RtspConfig open = cfg;
    open.user.clear();
    RtspSession o(open, "n3", "S3");
    a = o.handle(req("DESCRIBE", base, 1), next);
    CHECK(has(a, "401 Unauthorized"));
    a = o.handle(req("DESCRIBE", base, 2, "Authorization: Basic " + base64_encode(":") + "\r\n"), next);
    CHECK(has(a, "401 Unauthorized"));
}

// A JPEG as the camera makes it, with what the payload needs: two quantization tables, the frame header of 3 components and a scan.
static std::vector<std::uint8_t> sample_jpeg(int w, int h, std::uint8_t luma_sampling, std::size_t data_len) {
    std::vector<std::uint8_t> j = {0xFF, 0xD8};
    for (int id = 0; id < 2; ++id) {
        j.insert(j.end(), {0xFF, 0xDB, 0x00, 0x43, static_cast<std::uint8_t>(id)});
        for (int k = 0; k < 64; ++k) j.push_back(static_cast<std::uint8_t>(id * 100 + k + 1));
    }
    j.insert(j.end(), {0xFF, 0xC0, 0x00, 0x11, 0x08, static_cast<std::uint8_t>(h >> 8), static_cast<std::uint8_t>(h), static_cast<std::uint8_t>(w >> 8), static_cast<std::uint8_t>(w), 0x03, 0x01,
                       luma_sampling, 0x00, 0x02, 0x11, 0x01, 0x03, 0x11, 0x01});
    j.insert(j.end(), {0xFF, 0xDA, 0x00, 0x0C, 0x03, 0x01, 0x00, 0x02, 0x11, 0x03, 0x11, 0x00, 0x3F, 0x00});
    for (std::size_t k = 0; k < data_len; ++k) j.push_back(static_cast<std::uint8_t>(k % 200));  // never 0xFF
    j.insert(j.end(), {0xFF, 0xD9});
    return j;
}

static void rtp_payload() {
    auto jpeg = sample_jpeg(640, 480, 0x21, 3000);
    JpegInfo info = parse_jpeg(jpeg.data(), jpeg.size());
    CHECK(info.ok && info.width == 640 && info.height == 480 && info.type == 0 && info.luma.size() == 64 && info.chroma.size() == 64);
    CHECK(info.data_end - info.data_start == 3000 && info.luma[0] == 1 && info.chroma[0] == 101);
    CHECK(parse_jpeg(sample_jpeg(320, 240, 0x22, 500).data(), sample_jpeg(320, 240, 0x22, 500).size()).type == 1);

    auto packets = rtp_jpeg_packets(jpeg.data(), jpeg.size(), 65534, 90000, 0xA1B2C3D4, 1400);
    CHECK(packets.size() == 3);  // the first holds 1400 - 8 - 4 - 128 bytes of data, the next ones 1392
    std::vector<std::uint8_t> data;
    std::size_t offset = 0;
    for (std::size_t i = 0; i < packets.size(); ++i) {
        const auto& p = packets[i];
        CHECK(p[0] == 0x80 && (p[1] & 0x7F) == 26);
        CHECK(((p[1] & 0x80) != 0) == (i + 1 == packets.size()));  // the marker only on the last
        CHECK(static_cast<std::uint16_t>((p[2] << 8) | p[3]) == static_cast<std::uint16_t>(65534 + i));  // the sequence wraps
        CHECK(p[4] == 0 && p[5] == 0x01 && p[6] == 0x5F && p[7] == 0x90);                                 // 90000
        CHECK(p[8] == 0xA1 && p[11] == 0xD4);
        std::size_t frag = (static_cast<std::size_t>(p[13]) << 16) | (static_cast<std::size_t>(p[14]) << 8) | p[15];
        CHECK(frag == offset);
        CHECK(p[16] == 0 && p[17] == 255 && p[18] == 80 && p[19] == 60);  // type 0, Q 255, 640/8 x 480/8
        std::size_t head = 20;
        if (i == 0) {
            CHECK(p[20] == 0 && p[21] == 0 && p[22] == 0 && p[23] == 128 && p[24] == 1 && p[24 + 64] == 101);
            head = 24 + 128;
        }
        CHECK(p.size() <= 12 + 1400);
        data.insert(data.end(), p.begin() + static_cast<long>(head), p.end());
        offset = data.size();
    }
    CHECK(data.size() == 3000 && data[0] == 0 && data[199] == 199 && data[200] == 0);
    // a small picture is one packet with the marker
    auto small = rtp_jpeg_packets(sample_jpeg(160, 120, 0x22, 300).data(), sample_jpeg(160, 120, 0x22, 300).size(), 0, 0, 1);
    CHECK(small.size() == 1 && (small[0][1] & 0x80) && small[0][16] == 1);
    // what cannot be carried gives nothing
    CHECK(rtp_jpeg_packets(jpeg.data(), 10, 0, 0, 1).empty());
    std::vector<std::uint8_t> progressive = jpeg;
    for (std::size_t k = 0; k + 1 < progressive.size(); ++k)
        if (progressive[k] == 0xFF && progressive[k + 1] == 0xC0) progressive[k + 1] = 0xC2;
    CHECK(!parse_jpeg(progressive.data(), progressive.size()).ok);
    CHECK(!parse_jpeg(nullptr, 0).ok);
}

static std::string envelope(const std::string& op, const std::string& header = "") {
    return "<?xml version=\"1.0\"?><s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\"><s:Header>" + header + "</s:Header><s:Body><tds:" + op +
           " xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\"/></s:Body></s:Envelope>";
}

static std::string token_header(const std::string& user, const std::string& password, const std::string& nonce, const std::string& created) {
    return "<wsse:Security xmlns:wsse=\"x\" xmlns:wsu=\"y\"><wsse:UsernameToken><wsse:Username>" + user +
           "</wsse:Username><wsse:Password Type=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-username-token-profile-1.0#PasswordDigest\">" + token_digest(nonce, created, password) +
           "</wsse:Password><wsse:Nonce>" + nonce + "</wsse:Nonce><wsu:Created>" + created + "</wsu:Created></wsse:UsernameToken></wsse:Security>";
}

static void onvif() {
    OnvifConfig c;
    c.ip = "10.0.0.5";
    c.user = "viewer";
    c.password = "userpassword";
    // the example of the ONVIF core specification for the digest of the token
    CHECK(token_digest("LKqI6G/AikKCQrN0zqZFlg==", "2010-09-16T07:50:45Z", "userpassword") == "tuOSpGlFlIXsozq4HFNeeGeFLEI=");

    // discovery
    std::string probe =
        "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:a=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\"><s:Header><a:MessageID>uuid:1234</a:MessageID>"
        "<a:Action>http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe</a:Action></s:Header><s:Body><Probe xmlns=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\"><d:Types>dn:NetworkVideoTransmitter</d:Types></Probe></s:Body></s:Envelope>";
    CHECK(is_discovery_probe(probe) && probe_message_id(probe) == "uuid:1234");
    CHECK(!is_discovery_probe("<s:Envelope>hello</s:Envelope>"));
    std::string other = probe;
    other.replace(other.find("dn:NetworkVideoTransmitter"), 26, "dn:PrinterService");
    CHECK(!is_discovery_probe(other));
    std::string match = probe_match(c, probe_message_id(probe), "abc");
    CHECK(has(match, "<d:XAddrs>http://10.0.0.5:80/onvif/device_service</d:XAddrs>") && has(match, "RelatesTo>uuid:1234<") && has(match, "NetworkVideoTransmitter") &&
          has(match, "onvif://www.onvif.org/name/bungard-aux"));

    // the calls that are open
    SoapAnswer a = onvif_handle(c, envelope("GetSystemDateAndTime"), "2026-10-30 14:05:09");
    CHECK(a.http_status == 200 && has(a.body, "<tt:Hour>14</tt:Hour>") && has(a.body, "<tt:Year>2026</tt:Year>"));
    a = onvif_handle(c, envelope("GetCapabilities"), "");
    CHECK(a.http_status == 200 && has(a.body, "http://10.0.0.5:80/onvif/media_service") && has(a.body, "RTP_RTSP_TCP"));
    CHECK(has(onvif_handle(c, envelope("GetServices"), "").body, "ver10/media/wsdl"));
    // everything else asks for the user token
    a = onvif_handle(c, envelope("GetProfiles"), "");
    CHECK(a.http_status == 400 && has(a.body, "ter:NotAuthorized"));
    a = onvif_handle(c, envelope("GetProfiles", token_header("viewer", "wrong", "LKqI6G/AikKCQrN0zqZFlg==", "2010-09-16T07:50:45Z")), "");
    CHECK(a.http_status == 400);
    std::string good = token_header("viewer", "userpassword", "LKqI6G/AikKCQrN0zqZFlg==", "2010-09-16T07:50:45Z");
    a = onvif_handle(c, envelope("GetProfiles", good), "");
    CHECK(a.http_status == 200 && has(a.body, "token=\"profile1\"") && has(a.body, "<tt:Encoding>JPEG</tt:Encoding>") && has(a.body, "<tt:Width>640</tt:Width>"));
    a = onvif_handle(c, envelope("GetStreamUri", good), "");
    CHECK(has(a.body, "<tt:Uri>rtsp://10.0.0.5:554/stream1</tt:Uri>"));
    a = onvif_handle(c, envelope("GetSnapshotUri", good), "");
    CHECK(has(a.body, "http://10.0.0.5:80/snapshot.jpg"));
    a = onvif_handle(c, envelope("GetVideoEncoderConfigurationOptions", good), "");
    CHECK(has(a.body, "<tt:Width>1600</tt:Width>") && has(a.body, "<tt:Width>160</tt:Width>") && has(a.body, "<tt:Max>30</tt:Max>"));
    a = onvif_handle(c, envelope("GetDeviceInformation", good), "");
    CHECK(has(a.body, "BUNGARD-AUX") && has(a.body, "ESP32-S3-ETH"));
    a = onvif_handle(c, envelope("GetScopes", good), "");
    CHECK(has(a.body, "onvif://www.onvif.org/type/video_encoder"));
    for (const char* op : {"GetVideoSources", "GetVideoSourceConfigurations", "GetVideoEncoderConfigurations", "GetProfile", "GetHostname", "GetNetworkInterfaces", "GetAudioSources"}) {
        CHECK(onvif_handle(c, envelope(op, good), "").http_status == 200);
    }
    // a call the node does not do
    a = onvif_handle(c, envelope("ContinuousMove", good), "");
    CHECK(a.http_status == 400 && has(a.body, "ter:ActionNotSupported"));
    CHECK(onvif_handle(c, "nothing", "").http_status == 400);
    // a node without a password: every call that needs the token is refused, whatever it carries
    OnvifConfig nopass = c;
    nopass.password.clear();
    CHECK(onvif_handle(nopass, envelope("GetProfiles", good), "").http_status == 400);
    CHECK(onvif_handle(nopass, envelope("GetSystemDateAndTime"), "").http_status == 200);
}

int main() {
    hashes();
    request_parsing();
    rtsp_flow();
    rtp_payload();
    onvif();
    std::printf("%d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
