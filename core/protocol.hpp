// EH3D-NODE-BUNGARD-AUX - core/protocol.hpp
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
// What the node says on the network. It speaks HTTP on one port:
//   GET  /api/status            the readings of the node as one line of JSON (the laser and the camera are asked for, never turned on by it)
//   POST /api/measure[?samples=N]  one measure of the height of the table under the head (needs the token)
//   GET  /stream                the camera as MJPEG (needs the token when the node has one)
//   GET  /api/snapshot          one picture of the camera as JPEG (needs the token): what the programs that check the work look at
//   GET  /api/light             the ring of LEDs around the camera (needs the token)
//   POST /api/light?on=1&level=70&tone=warm&sectors=left   turns it on or off and changes how it shines (needs the token)
// The token goes in the header `Authorization: Bearer <token>` or in the query `?token=`. A node without a token refuses every request that changes or measures
// something; it still answers the status. The answers are built here, so the desktop programs, the server and the tests read the same words.
#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

#include "aux_config.hpp"
#include "hash.hpp"
#include "laser.hpp"
#include "light.hpp"
#include "spindle.hpp"

namespace bungard_aux {

enum class Route { NotFound, Status, Measure, Stream, Snapshot, LightGet, LightSet, Calibrate, BadMethod };

struct Request {
    std::string method;
    std::string path;
    std::string query;
};

/// Splits `GET /api/measure?samples=5 HTTP/1.1` (the first line of a request).
inline bool parse_request_line(const std::string& line, Request& out) {
    std::size_t a = line.find(' ');
    if (a == std::string::npos || a == 0) return false;
    std::size_t b = line.find(' ', a + 1);
    if (b == std::string::npos || b == a + 1) return false;
    out.method = line.substr(0, a);
    std::string target = line.substr(a + 1, b - a - 1);
    if (target.empty() || target[0] != '/') return false;
    std::size_t q = target.find('?');
    out.path = q == std::string::npos ? target : target.substr(0, q);
    out.query = q == std::string::npos ? "" : target.substr(q + 1);
    return true;
}

inline Route route_of(const Request& r) {
    if (r.path == "/api/status") return r.method == "GET" ? Route::Status : Route::BadMethod;
    if (r.path == "/api/measure") return r.method == "POST" ? Route::Measure : Route::BadMethod;
    if (r.path == "/stream") return r.method == "GET" ? Route::Stream : Route::BadMethod;
    if (r.path == "/api/snapshot") return r.method == "GET" ? Route::Snapshot : Route::BadMethod;
    if (r.path == "/api/light") return r.method == "GET" ? Route::LightGet : r.method == "POST" ? Route::LightSet : Route::BadMethod;
    if (r.path == "/api/laser/calibrate") return r.method == "POST" ? Route::Calibrate : Route::BadMethod;
    return Route::NotFound;
}

/// The value of `key` in a query like `samples=5&token=abc`; empty when it is not there.
inline std::string query_value(const std::string& query, const std::string& key) {
    std::size_t pos = 0;
    while (pos <= query.size()) {
        std::size_t end = query.find('&', pos);
        if (end == std::string::npos) end = query.size();
        std::string pair = query.substr(pos, end - pos);
        std::size_t eq = pair.find('=');
        if (eq != std::string::npos && pair.substr(0, eq) == key) return pair.substr(eq + 1);
        pos = end + 1;
    }
    return "";
}

/// Whether a request may do what it asks. `bearer` is what follows `Bearer ` in the header (empty when there is none).
inline bool authorized(const Settings& s, Route route, const std::string& bearer, const Request& r) {
    if (route == Route::Status) return true;
    if (s.token.empty()) return route == Route::Stream && false;  // without a token nothing that measures or shows the workshop is served
    std::string given = !bearer.empty() ? bearer : query_value(r.query, "token");
    return same_secret(s.token, given);
}

/// The number of readings a measure asks for: the query, kept inside what the settings allow.
inline int samples_of(const Settings& s, const Request& r) {
    std::string v = query_value(r.query, "samples");
    if (v.empty()) return s.laser_samples;
    std::uint32_t n;
    if (!detail::to_uint(v, 1, 31, n)) return s.laser_samples;
    return static_cast<int>(n);
}

/// What a request to calibrate the laser asks: `kind` is offset, xtalk or reset, `mm` the distance of the flat target from the sensor (not needed to reset).
struct CalibrateRequest {
    std::string kind;
    double target_mm = 0.0;
};

/// False when the kind is unknown, or the target is missing or not a number between 10 and 4000 mm (a reset needs none).
inline bool parse_calibrate(const Request& r, CalibrateRequest& out) {
    out.kind = query_value(r.query, "kind");
    if (out.kind.empty()) out.kind = "offset";
    if (out.kind == "reset") return true;
    if (out.kind != "offset" && out.kind != "xtalk") return false;
    return detail::to_double(query_value(r.query, "mm"), 10.0, 4000.0, out.target_mm);
}

struct Status {
    std::string name;
    std::string version;
    bool camera = false;
    std::string camera_model;  // what the sensor reported (ov2640, ov5640), empty when there is no camera
    bool laser = false;
    bool rpm_on = false;
    double rpm = 0.0;
    Heat heat = Heat::Unknown;
    bool has_temp = false;
    double temp_c = 0.0;
    bool protect_open = false;
    std::uint32_t uptime_s = 0;
    bool light = false;        // the node has the ring of LEDs switched on in its settings
    bool light_on = false;     // and the ring is shining
    std::uint32_t light_level = 0;
    // the laser: how it is set up and the last reading the sensor gave (laser_age_ms is -1 when it has given none)
    std::string laser_mode = "long";
    int laser_budget_ms = 100;
    std::string laser_roi = "16x16";
    double laser_cal_offset_mm = 0.0;
    double laser_cal_xtalk_cps = 0.0;
    bool laser_cal_allowed = false;
    double laser_last_mm = 0.0;
    std::string laser_last_status = "none";  // valid, sigma_high, signal_low, out_of_range, sensor_fault, other or none
    long laser_age_ms = -1;
    // the DS18B20 of the spindle: the addresses on the wire (16 hex digits each, separated by commas), the one that is read, and whether it was a guess among several
    std::string temp_roms;
    std::string temp_rom_used;
    int temp_resolution = 12;
    bool temp_guess = false;
};

inline std::string json_escape(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '"' || c == '\\') { o += '\\'; o += c; }
        else if (static_cast<unsigned char>(c) < 0x20) o += ' ';
        else o += c;
    }
    return o;
}

inline std::string json_status(const Status& s) {
    char buf[1024];
    char temp[24];
    if (s.has_temp) std::snprintf(temp, sizeof temp, "%.1f", s.temp_c); else std::snprintf(temp, sizeof temp, "null");
    std::snprintf(buf, sizeof buf,
                  "{\"node\":\"%s\",\"version\":\"%s\",\"camera\":%s,\"camera_model\":\"%s\",\"laser\":%s,\"rpm_sensor\":%s,\"rpm\":%.0f,\"temp_c\":%s,\"heat\":\"%s\",\"protect_open\":%s,\"uptime_s\":%u,\"light\":%s,\"light_on\":%s,\"light_level\":%u,\"laser_mode\":\"%s\",\"laser_budget_ms\":%d,\"laser_roi\":\"%s\",\"laser_cal_offset_mm\":%.3f,\"laser_cal_xtalk_cps\":%.0f,\"laser_cal_allowed\":%s,\"laser_last_mm\":%.3f,\"laser_last_status\":\"%s\",\"laser_age_ms\":%ld}",
                  json_escape(s.name).c_str(), json_escape(s.version).c_str(), s.camera ? "true" : "false", json_escape(s.camera_model).c_str(), s.laser ? "true" : "false", s.rpm_on ? "true" : "false", s.rpm, temp,
                  heat_text(s.heat), s.protect_open ? "true" : "false", static_cast<unsigned>(s.uptime_s), s.light ? "true" : "false", s.light_on ? "true" : "false",
                  static_cast<unsigned>(s.light_level), json_escape(s.laser_mode).c_str(), s.laser_budget_ms, json_escape(s.laser_roi).c_str(), s.laser_cal_offset_mm, s.laser_cal_xtalk_cps,
                  s.laser_cal_allowed ? "true" : "false", s.laser_last_mm, json_escape(s.laser_last_status).c_str(), s.laser_age_ms);
    std::string out = buf;
    out.pop_back();  // the closing brace: the sensors of the temperature go after it
    out += ",\"temp_roms\":\"" + json_escape(s.temp_roms) + "\",\"temp_rom_used\":\"" + json_escape(s.temp_rom_used) + "\",\"temp_resolution\":" + std::to_string(s.temp_resolution) +
           ",\"temp_guess\":" + (s.temp_guess ? "true" : "false") + "}";
    return out;
}

inline std::string json_measure(const Measure& m) {
    char buf[320];
    std::snprintf(buf, sizeof buf,
                  "{\"ok\":%s,\"why\":\"%s\",\"distance_mm\":%.3f,\"sigma_mm\":%.3f,\"asked\":%d,\"used\":%d,\"rejected\":%d,\"refused\":%d,\"status\":\"%s\",\"status_code\":%d}",
                  m.ok ? "true" : "false", why_text(m.why), m.distance_mm, m.sigma_mm, m.asked, m.used, m.rejected, m.refused, range_why_text(m.status_why), m.status_code);
    return buf;
}

/// The answer to a calibration: what was found and what the sensor now has stored (the offset in mm and the crosstalk in counts per second).
inline std::string json_calibration(const std::string& kind, bool ok, const std::string& why, double offset_mm, double xtalk_cps, int used, double sigma_mm) {
    char buf[320];
    std::snprintf(buf, sizeof buf, "{\"ok\":%s,\"kind\":\"%s\",\"why\":\"%s\",\"cal_offset_mm\":%.3f,\"cal_xtalk_cps\":%.0f,\"used\":%d,\"sigma_mm\":%.3f}", ok ? "true" : "false",
                  json_escape(kind).c_str(), json_escape(why).c_str(), offset_mm, xtalk_cps, used, sigma_mm);
    return buf;
}

/// The line of the answer to a measure that the desktop programs read as a point of the map: `Z <x> <y> <height>`. The height is minus the distance (the table is
/// lower when it is farther), so a higher table has a higher number. Empty when the measure is not usable.
inline std::string reading_line(const Measure& m, double x_mm, double y_mm) {
    if (!m.ok) return "";
    char buf[96];
    std::snprintf(buf, sizeof buf, "Z %.3f %.3f %.4f", x_mm, y_mm, -m.distance_mm);
    return buf;
}

}  // namespace bungard_aux
