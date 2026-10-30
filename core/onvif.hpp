// EH3D-NODE-BUNGARD-AUX - core/onvif.hpp
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
// The ONVIF side of the node, so that the programs that look for IP cameras (recorders, ONVIF device managers, VLC) find it and take its video: WS-Discovery (the
// probe that is sent to the multicast address 239.255.255.250:3702 and the match the node answers), and the SOAP services of the device and of the media profile S:
// the date, the information of the device, the capabilities and the services, the profiles, the video sources and encoders (with the sizes the camera can do) and the
// address of the RTSP stream and of the snapshot. The login is the ONVIF user token (digest of the password with a nonce) and the node asks for it for everything but
// the few calls that the standard leaves open. The words are built here and tested on a computer; the firmware only carries them.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "hash.hpp"
#include "rtsp_session.hpp"

namespace bungard_aux {

struct OnvifConfig {
    std::string ip = "0.0.0.0";   // the address of the node
    int http_port = 80;           // where the SOAP services answer
    int rtsp_port = 554;
    std::string name = "bungard-aux";
    std::string user;
    std::string password;
    std::string firmware = "0.0.1";
    std::string serial = "AUX-0001";
    std::string uuid = "0e3d0b5c-1f2a-4c8e-9b7d-000000000001";  // the identity of the node on the network; the firmware makes it from the MAC address
    VideoInfo video;
    std::vector<std::pair<int, int>> sizes = {{1600, 1200}, {1280, 720}, {800, 600}, {640, 480}, {320, 240}, {160, 120}};  // what the camera can do
    int quality = 12;
};

namespace onvif_detail {

/// The text of the first element called `local` (whatever its prefix), or empty.
inline std::string element_text(const std::string& xml, const std::string& local, std::size_t from = 0) {
    std::size_t pos = from;
    while (true) {
        std::size_t lt = xml.find('<', pos);
        if (lt == std::string::npos) return "";
        std::size_t gt = xml.find('>', lt);
        if (gt == std::string::npos) return "";
        std::string tag = xml.substr(lt + 1, gt - lt - 1);
        pos = gt + 1;
        if (tag.empty() || tag[0] == '/' || tag[0] == '?' || tag[0] == '!' || tag.back() == '/') continue;
        std::size_t sp = tag.find_first_of(" \t\r\n");
        std::string name = tag.substr(0, sp);
        std::size_t colon = name.find(':');
        if (colon != std::string::npos) name = name.substr(colon + 1);
        if (name != local) continue;
        std::size_t end = xml.find("</", gt + 1);
        if (end == std::string::npos) return "";
        return detail::trim_ws(xml.substr(gt + 1, end - gt - 1));
    }
}

inline bool has(const std::string& xml, const std::string& s) { return xml.find(s) != std::string::npos; }

/// The name of the first element inside the Body of a SOAP envelope (without its prefix).
inline std::string body_operation(const std::string& xml) {
    std::size_t b = xml.find("Body");
    if (b == std::string::npos) return "";
    std::size_t gt = xml.find('>', b);
    if (gt == std::string::npos) return "";
    std::size_t pos = gt + 1;
    while (true) {
        std::size_t lt = xml.find('<', pos);
        if (lt == std::string::npos) return "";
        std::size_t end = xml.find('>', lt);
        if (end == std::string::npos) return "";
        std::string tag = xml.substr(lt + 1, end - lt - 1);
        pos = end + 1;
        if (tag.empty() || tag[0] == '/' || tag[0] == '?' || tag[0] == '!') continue;
        std::size_t sp = tag.find_first_of(" \t\r\n/");
        std::string name = tag.substr(0, sp);
        std::size_t colon = name.find(':');
        return colon == std::string::npos ? name : name.substr(colon + 1);
    }
}

inline std::string xml_escape(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '&') o += "&amp;";
        else if (c == '<') o += "&lt;";
        else if (c == '>') o += "&gt;";
        else if (c == '"') o += "&quot;";
        else o += c;
    }
    return o;
}

}  // namespace onvif_detail

// ---------------------------------------------------------------------------------------------------------------- WS-Discovery

/// Whether a UDP message is a WS-Discovery probe that the node answers (a probe for network video transmitters or devices, or for anything).
inline bool is_discovery_probe(const std::string& xml) {
    using onvif_detail::has;
    if (!has(xml, "discovery/Probe") && !has(xml, ">Probe<") && !has(xml, ":Probe>")) return false;
    std::string types = onvif_detail::element_text(xml, "Types");
    return types.empty() || has(types, "NetworkVideoTransmitter") || has(types, "Device");
}

/// The id of the probe, to answer it with `RelatesTo`.
inline std::string probe_message_id(const std::string& xml) { return onvif_detail::element_text(xml, "MessageID"); }

inline std::string scopes_text(const OnvifConfig& c) {
    return "onvif://www.onvif.org/type/video_encoder onvif://www.onvif.org/type/Network_Video_Transmitter onvif://www.onvif.org/Profile/Streaming onvif://www.onvif.org/name/" +
           onvif_detail::xml_escape(c.name) + " onvif://www.onvif.org/hardware/BUNGARD-AUX onvif://www.onvif.org/location/workshop";
}

inline std::string device_url(const OnvifConfig& c) { return "http://" + c.ip + ":" + std::to_string(c.http_port) + "/onvif/device_service"; }
inline std::string media_url(const OnvifConfig& c) { return "http://" + c.ip + ":" + std::to_string(c.http_port) + "/onvif/media_service"; }
inline std::string stream_url(const OnvifConfig& c) { return "rtsp://" + c.ip + ":" + std::to_string(c.rtsp_port) + "/stream1"; }
inline std::string snapshot_url(const OnvifConfig& c) { return "http://" + c.ip + ":" + std::to_string(c.http_port) + "/snapshot.jpg"; }

/// The answer to a probe (also sent by itself when the node starts, as a Hello would be).
inline std::string probe_match(const OnvifConfig& c, const std::string& relates_to, const std::string& message_uuid) {
    std::string x;
    x += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
    x += "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:a=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\" xmlns:d=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\" xmlns:dn=\"http://www.onvif.org/ver10/network/wsdl\">";
    x += "<s:Header><a:MessageID>uuid:" + message_uuid + "</a:MessageID>";
    if (!relates_to.empty()) x += "<a:RelatesTo>" + relates_to + "</a:RelatesTo>";
    x += "<a:To>http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous</a:To><a:Action>http://schemas.xmlsoap.org/ws/2005/04/discovery/ProbeMatches</a:Action></s:Header>";
    x += "<s:Body><d:ProbeMatches><d:ProbeMatch><a:EndpointReference><a:Address>urn:uuid:" + c.uuid + "</a:Address></a:EndpointReference>";
    x += "<d:Types>dn:NetworkVideoTransmitter</d:Types><d:Scopes>" + scopes_text(c) + "</d:Scopes><d:XAddrs>" + device_url(c) + "</d:XAddrs><d:MetadataVersion>1</d:MetadataVersion>";
    x += "</d:ProbeMatch></d:ProbeMatches></s:Body></s:Envelope>";
    return x;
}

// ---------------------------------------------------------------------------------------------------------------- the login

/// The base64 digest of an ONVIF user token: SHA-1 of the nonce (as bytes), the creation time and the password.
inline std::string token_digest(const std::string& nonce_base64, const std::string& created, const std::string& password) {
    auto d = sha1_bytes(base64_decode(nonce_base64) + created + password);
    return base64_encode(d.data(), d.size());
}

/// Whether the envelope carries the user token of `user` with `password` (as a digest, or as plain text).
inline bool token_ok(const std::string& xml, const std::string& user, const std::string& password) {
    if (user.empty() || password.empty()) return false;
    if (onvif_detail::element_text(xml, "Username") != user) return false;
    std::string given = onvif_detail::element_text(xml, "Password");
    if (given.empty()) return false;
    if (xml.find("PasswordDigest") != std::string::npos) {
        std::string nonce = onvif_detail::element_text(xml, "Nonce");
        std::string created = onvif_detail::element_text(xml, "Created");
        if (nonce.empty() || created.empty()) return false;
        return same_secret(given, token_digest(nonce, created, password));
    }
    return same_secret(given, password);
}

// ---------------------------------------------------------------------------------------------------------------- the services

/// What the standard leaves open without a login.
inline bool open_operation(const std::string& op) {
    return op == "GetSystemDateAndTime" || op == "GetCapabilities" || op == "GetServices" || op == "GetServiceCapabilities";
}

struct SoapAnswer {
    int http_status = 200;
    std::string body;  // the whole envelope
};

namespace onvif_detail {

inline std::string envelope(const std::string& body) {
    return "<?xml version=\"1.0\" encoding=\"UTF-8\"?><s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" "
           "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\" xmlns:tt=\"http://www.onvif.org/ver10/schema\" "
           "xmlns:ter=\"http://www.onvif.org/ver10/error\"><s:Body>" +
           body + "</s:Body></s:Envelope>";
}

inline SoapAnswer fault(int http, const char* sub, const char* reason) {
    return {http, envelope(std::string("<s:Fault><s:Code><s:Value>s:Sender</s:Value><s:Subcode><s:Value>") + sub + "</s:Value></s:Subcode></s:Code><s:Reason><s:Text xml:lang=\"en\">" +
                           reason + "</s:Text></s:Reason></s:Fault>")};
}

inline std::string video_source(const OnvifConfig& c) {
    return "<tt:Resolution><tt:Width>" + std::to_string(c.video.width) + "</tt:Width><tt:Height>" + std::to_string(c.video.height) + "</tt:Height></tt:Resolution>";
}

inline std::string source_configuration(const OnvifConfig& c) {
    return "<tt:Name>VideoSourceConfig</tt:Name><tt:UseCount>1</tt:UseCount><tt:SourceToken>video_source_1</tt:SourceToken><tt:Bounds x=\"0\" y=\"0\" width=\"" +
           std::to_string(c.video.width) + "\" height=\"" + std::to_string(c.video.height) + "\"/>";
}

inline std::string encoder_configuration(const OnvifConfig& c) {
    return "<tt:Name>JpegEncoder</tt:Name><tt:UseCount>1</tt:UseCount><tt:Encoding>JPEG</tt:Encoding>" + video_source(c) + "<tt:Quality>" + std::to_string(c.quality) +
           "</tt:Quality><tt:RateControl><tt:FrameRateLimit>" + std::to_string(c.video.fps) + "</tt:FrameRateLimit><tt:EncodingInterval>1</tt:EncodingInterval><tt:BitrateLimit>4096</tt:BitrateLimit></tt:RateControl>"
           "<tt:SessionTimeout>PT60S</tt:SessionTimeout>";
}

inline std::string profile(const OnvifConfig& c, bool as_element) {
    std::string p = as_element ? "<trt:Profiles" : "<trt:Profile";
    std::string close = as_element ? "</trt:Profiles>" : "</trt:Profile>";
    return p + " token=\"profile1\" fixed=\"true\"><tt:Name>profile1</tt:Name><tt:VideoSourceConfiguration token=\"video_source_config_1\">" + source_configuration(c) +
           "</tt:VideoSourceConfiguration><tt:VideoEncoderConfiguration token=\"encoder_1\">" + encoder_configuration(c) + "</tt:VideoEncoderConfiguration>" + close;
}

}  // namespace onvif_detail

/// The answer of the node to a SOAP request. `utc` is the time of the node as "YYYY-MM-DD HH:MM:SS" (the firmware reads its clock), or empty when it has none.
inline SoapAnswer onvif_handle(const OnvifConfig& c, const std::string& xml, const std::string& utc) {
    using namespace onvif_detail;
    std::string op = body_operation(xml);
    if (op.empty()) return fault(400, "ter:InvalidArgVal", "not a SOAP request");
    if (!open_operation(op) && !token_ok(xml, c.user, c.password)) return fault(400, "ter:NotAuthorized", "Sender not authorized");

    std::string b;
    if (op == "GetSystemDateAndTime") {
        int Y = 1970, M = 1, D = 1, h = 0, m = 0, s = 0;
        if (std::sscanf(utc.c_str(), "%d-%d-%d %d:%d:%d", &Y, &M, &D, &h, &m, &s) != 6) {
            Y = 1970; M = 1; D = 1; h = m = s = 0;
        }
        b = "<tds:GetSystemDateAndTimeResponse><tds:SystemDateAndTime><tt:DateTimeType>Manual</tt:DateTimeType><tt:DaylightSavings>false</tt:DaylightSavings>"
            "<tt:TimeZone><tt:TZ>UTC0</tt:TZ></tt:TimeZone><tt:UTCDateTime><tt:Time><tt:Hour>" + std::to_string(h) + "</tt:Hour><tt:Minute>" + std::to_string(m) +
            "</tt:Minute><tt:Second>" + std::to_string(s) + "</tt:Second></tt:Time><tt:Date><tt:Year>" + std::to_string(Y) + "</tt:Year><tt:Month>" + std::to_string(M) +
            "</tt:Month><tt:Day>" + std::to_string(D) + "</tt:Day></tt:Date></tt:UTCDateTime></tds:SystemDateAndTime></tds:GetSystemDateAndTimeResponse>";
    } else if (op == "GetDeviceInformation") {
        b = "<tds:GetDeviceInformationResponse><tds:Manufacturer>EH3D (Electro Hobby 3D)</tds:Manufacturer><tds:Model>BUNGARD-AUX</tds:Model><tds:FirmwareVersion>" + xml_escape(c.firmware) +
            "</tds:FirmwareVersion><tds:SerialNumber>" + xml_escape(c.serial) + "</tds:SerialNumber><tds:HardwareId>ESP32-S3-ETH</tds:HardwareId></tds:GetDeviceInformationResponse>";
    } else if (op == "GetCapabilities") {
        b = "<tds:GetCapabilitiesResponse><tds:Capabilities><tt:Device><tt:XAddr>" + device_url(c) + "</tt:XAddr><tt:Network><tt:IPFilter>false</tt:IPFilter><tt:ZeroConfiguration>false</tt:ZeroConfiguration>"
            "<tt:IPVersion6>false</tt:IPVersion6><tt:DynDNS>false</tt:DynDNS></tt:Network><tt:System><tt:DiscoveryResolve>false</tt:DiscoveryResolve><tt:DiscoveryBye>false</tt:DiscoveryBye>"
            "<tt:RemoteDiscovery>false</tt:RemoteDiscovery><tt:SystemBackup>false</tt:SystemBackup><tt:SystemLogging>false</tt:SystemLogging><tt:FirmwareUpgrade>false</tt:FirmwareUpgrade></tt:System></tt:Device>"
            "<tt:Media><tt:XAddr>" + media_url(c) + "</tt:XAddr><tt:StreamingCapabilities><tt:RTPMulticast>false</tt:RTPMulticast><tt:RTP_TCP>true</tt:RTP_TCP><tt:RTP_RTSP_TCP>true</tt:RTP_RTSP_TCP></tt:StreamingCapabilities></tt:Media>"
            "</tds:Capabilities></tds:GetCapabilitiesResponse>";
    } else if (op == "GetServices") {
        b = "<tds:GetServicesResponse><tds:Service><tds:Namespace>http://www.onvif.org/ver10/device/wsdl</tds:Namespace><tds:XAddr>" + device_url(c) + "</tds:XAddr><tds:Version><tt:Major>2</tt:Major><tt:Minor>0</tt:Minor></tds:Version></tds:Service>"
            "<tds:Service><tds:Namespace>http://www.onvif.org/ver10/media/wsdl</tds:Namespace><tds:XAddr>" + media_url(c) + "</tds:XAddr><tds:Version><tt:Major>2</tt:Major><tt:Minor>0</tt:Minor></tds:Version></tds:Service></tds:GetServicesResponse>";
    } else if (op == "GetServiceCapabilities") {
        b = "<tds:GetServiceCapabilitiesResponse><tds:Capabilities><tds:Network IPFilter=\"false\" ZeroConfiguration=\"false\" IPVersion6=\"false\" DynDNS=\"false\"/>"
            "<tds:Security UsernameToken=\"true\" HttpDigest=\"false\" RELToken=\"false\"/><tds:System DiscoveryResolve=\"false\" DiscoveryBye=\"false\" RemoteDiscovery=\"false\"/></tds:Capabilities></tds:GetServiceCapabilitiesResponse>";
    } else if (op == "GetScopes") {
        b = "<tds:GetScopesResponse>";
        std::string sc = scopes_text(c);
        std::size_t pos = 0;
        while (pos < sc.size()) {
            std::size_t e = sc.find(' ', pos);
            std::string one = sc.substr(pos, e == std::string::npos ? std::string::npos : e - pos);
            b += "<tds:Scopes><tt:ScopeDef>Fixed</tt:ScopeDef><tt:ScopeItem>" + one + "</tt:ScopeItem></tds:Scopes>";
            if (e == std::string::npos) break;
            pos = e + 1;
        }
        b += "</tds:GetScopesResponse>";
    } else if (op == "GetHostname") {
        b = "<tds:GetHostnameResponse><tds:HostnameInformation><tt:FromDHCP>false</tt:FromDHCP><tt:Name>" + xml_escape(c.name) + "</tt:Name></tds:HostnameInformation></tds:GetHostnameResponse>";
    } else if (op == "GetNetworkInterfaces") {
        b = "<tds:GetNetworkInterfacesResponse><tds:NetworkInterfaces token=\"eth0\"><tt:Enabled>true</tt:Enabled><tt:Info><tt:Name>eth0</tt:Name></tt:Info><tt:IPv4><tt:Enabled>true</tt:Enabled>"
            "<tt:Config><tt:Manual><tt:Address>" + c.ip + "</tt:Address><tt:PrefixLength>24</tt:PrefixLength></tt:Manual><tt:DHCP>false</tt:DHCP></tt:Config></tt:IPv4></tds:NetworkInterfaces></tds:GetNetworkInterfacesResponse>";
    } else if (op == "GetProfiles") {
        b = "<trt:GetProfilesResponse>" + profile(c, true) + "</trt:GetProfilesResponse>";
    } else if (op == "GetProfile") {
        b = "<trt:GetProfileResponse>" + profile(c, false) + "</trt:GetProfileResponse>";
    } else if (op == "GetVideoSources") {
        b = "<trt:GetVideoSourcesResponse><trt:VideoSources token=\"video_source_1\"><tt:Framerate>" + std::to_string(c.video.fps) + "</tt:Framerate>" + video_source(c) + "</trt:VideoSources></trt:GetVideoSourcesResponse>";
    } else if (op == "GetVideoSourceConfigurations") {
        b = "<trt:GetVideoSourceConfigurationsResponse><trt:Configurations token=\"video_source_config_1\">" + source_configuration(c) + "</trt:Configurations></trt:GetVideoSourceConfigurationsResponse>";
    } else if (op == "GetVideoEncoderConfigurations") {
        b = "<trt:GetVideoEncoderConfigurationsResponse><trt:Configurations token=\"encoder_1\">" + encoder_configuration(c) + "</trt:Configurations></trt:GetVideoEncoderConfigurationsResponse>";
    } else if (op == "GetVideoEncoderConfigurationOptions") {
        b = "<trt:GetVideoEncoderConfigurationOptionsResponse><trt:Options><tt:QualityRange><tt:Min>4</tt:Min><tt:Max>63</tt:Max></tt:QualityRange><tt:JPEG>";
        for (const auto& s : c.sizes) b += "<tt:ResolutionsAvailable><tt:Width>" + std::to_string(s.first) + "</tt:Width><tt:Height>" + std::to_string(s.second) + "</tt:Height></tt:ResolutionsAvailable>";
        b += "<tt:FrameRateRange><tt:Min>1</tt:Min><tt:Max>30</tt:Max></tt:FrameRateRange><tt:EncodingIntervalRange><tt:Min>1</tt:Min><tt:Max>1</tt:Max></tt:EncodingIntervalRange></tt:JPEG></trt:Options></trt:GetVideoEncoderConfigurationOptionsResponse>";
    } else if (op == "GetStreamUri") {
        b = "<trt:GetStreamUriResponse><trt:MediaUri><tt:Uri>" + stream_url(c) + "</tt:Uri><tt:InvalidAfterConnect>false</tt:InvalidAfterConnect><tt:InvalidAfterReboot>false</tt:InvalidAfterReboot><tt:Timeout>PT60S</tt:Timeout></trt:MediaUri></trt:GetStreamUriResponse>";
    } else if (op == "GetSnapshotUri") {
        b = "<trt:GetSnapshotUriResponse><trt:MediaUri><tt:Uri>" + snapshot_url(c) + "</tt:Uri><tt:InvalidAfterConnect>false</tt:InvalidAfterConnect><tt:InvalidAfterReboot>false</tt:InvalidAfterReboot><tt:Timeout>PT60S</tt:Timeout></trt:MediaUri></trt:GetSnapshotUriResponse>";
    } else if (op == "GetAudioSources" || op == "GetAudioEncoderConfigurations" || op == "GetMetadataConfigurations") {
        b = "<trt:" + op + "Response/>";
    } else {
        return fault(400, "ter:ActionNotSupported", "Optional action not implemented");
    }
    return {200, envelope(b)};
}

}  // namespace bungard_aux
