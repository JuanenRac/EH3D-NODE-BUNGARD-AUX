// EH3D-NODE-BUNGARD-AUX - core/aux_config.hpp
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
// Every tuned value of the node lives in one settings file (head.cfg), never in the code. The file is `key = value` lines with `#` comments; a value out of range or
// a key nobody knows is reported and the default stays, so a typing mistake never turns a safety limit off.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace bungard_aux {

enum class TempSensor { None, Ds18b20, Ntc, Mlx90614 };

struct Settings {
    // the link: the node has a wired network port; an empty address means DHCP
    std::string name = "bungard-aux";
    std::string address;                    // static IPv4, empty: DHCP
    std::string token;                      // write-only: never shown, never logged; empty: the node refuses every command that changes something
    std::uint16_t port = 80;                // 1..65535

    // the camera (MJPEG over HTTP)
    bool camera_enabled = true;
    std::uint16_t camera_width = 640;       // 160..1600
    std::uint16_t camera_height = 480;      // 120..1200
    std::uint16_t camera_quality = 12;      // JPEG quality, 4 (best) .. 63 (smallest)
    std::uint16_t camera_fps_max = 10;      // 1..30

    // the video as an IP camera sends it: RTSP (JPEG over RTP, over UDP or inside the TCP connection) and ONVIF (discovery and the services of profile S).
    // Both log in with this user and password; without them nothing is served.
    bool rtsp_enabled = true;
    std::uint16_t rtsp_port = 554;          // 1..65535
    bool onvif_enabled = true;
    std::string stream_user = "viewer";     // 1..32 characters
    std::string stream_password;            // write-only: 8 to 64 characters; empty: RTSP and ONVIF serve nobody

    // the laser that measures the height of the table
    bool laser_enabled = true;
    std::uint16_t laser_samples = 7;        // readings of one measure, 1..31 (the median is taken)
    std::uint32_t laser_timeout_ms = 1500;  // 100..10000
    double laser_offset_mm = 0.0;           // -50..50, added to every distance: where the sensor sits above the tool tip
    double laser_range_min_mm = 20.0;       // readings closer than this are refused
    double laser_range_max_mm = 400.0;      // readings farther than this are refused
    double laser_sigma_max_mm = 0.15;       // a measure whose readings spread more than this is refused (0.01..5)

    // the speed of the spindle
    bool rpm_enabled = true;
    std::uint32_t rpm_pulses_per_rev = 1;   // 1..64
    std::uint32_t rpm_max = 65000;          // above this the speed is an alarm, 1000..200000
    std::uint32_t rpm_stale_ms = 1500;      // no pulse for this long means the spindle stopped, 200..10000

    // the temperature of the spindle
    TempSensor temp_sensor = TempSensor::None;
    double temp_warn_c = 55.0;              // 20..150
    double temp_critical_c = 70.0;          // 20..200, always above the warning
    double temp_hysteresis_c = 3.0;         // 0.5..20

    // the safety output: it opens when the spindle is too hot or too fast, so the control unit's stop line can use it
    bool protect_output = true;
};

struct Problem {
    int line = 0;
    std::string key;
    std::string what;
};

namespace detail {

inline std::string trim(const std::string& s) {
    std::size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) --b;
    return s.substr(a, b - a);
}

inline bool to_uint(const std::string& v, std::uint32_t lo, std::uint32_t hi, std::uint32_t& out) {
    if (v.empty() || v.size() > 10) return false;
    std::uint64_t n = 0;
    for (char c : v) {
        if (c < '0' || c > '9') return false;
        n = n * 10 + static_cast<unsigned>(c - '0');
    }
    if (n < lo || n > hi) return false;
    out = static_cast<std::uint32_t>(n);
    return true;
}

inline bool to_double(const std::string& v, double lo, double hi, double& out) {
    if (v.empty() || v.size() > 16) return false;
    std::size_t i = 0;
    bool neg = false, dot = false, digit = false;
    double n = 0.0, scale = 1.0;
    if (v[0] == '-' || v[0] == '+') { neg = v[0] == '-'; i = 1; }
    for (; i < v.size(); ++i) {
        char c = v[i];
        if (c == '.' && !dot) { dot = true; continue; }
        if (c < '0' || c > '9') return false;
        digit = true;
        if (dot) { scale /= 10.0; n += (c - '0') * scale; } else { n = n * 10.0 + (c - '0'); }
    }
    if (!digit) return false;
    if (neg) n = -n;
    if (n < lo || n > hi) return false;
    out = n;
    return true;
}

inline bool to_bool(const std::string& v, bool& out) {
    if (v == "on" || v == "true" || v == "yes" || v == "1") { out = true; return true; }
    if (v == "off" || v == "false" || v == "no" || v == "0") { out = false; return true; }
    return false;
}

/// An IPv4 address of four numbers from 0 to 255.
inline bool is_ipv4(const std::string& v) {
    int parts = 0;
    std::size_t pos = 0;
    while (pos <= v.size()) {
        std::size_t end = v.find('.', pos);
        if (end == std::string::npos) end = v.size();
        std::uint32_t n;
        if (!to_uint(v.substr(pos, end - pos), 0, 255, n)) return false;
        ++parts;
        pos = end + 1;
        if (end == v.size()) break;
    }
    return parts == 4;
}

}  // namespace detail

/// Reads a settings file. The result starts from the defaults; every line that cannot be used is listed in `problems` and changes nothing.
inline Settings parse_settings(const std::string& text, std::vector<Problem>& problems) {
    Settings s;
    std::size_t pos = 0;
    int line_no = 0;
    while (pos <= text.size()) {
        std::size_t end = text.find('\n', pos);
        if (end == std::string::npos) end = text.size();
        std::string line = detail::trim(text.substr(pos, end - pos));
        pos = end + 1;
        ++line_no;
        if (line.empty() || line[0] == '#') continue;
        std::size_t eq = line.find('=');
        if (eq == std::string::npos) {
            problems.push_back({line_no, line, "not a key = value line"});
            continue;
        }
        std::string key = detail::trim(line.substr(0, eq));
        std::string val = detail::trim(line.substr(eq + 1));
        std::uint32_t n = 0;
        double d = 0.0;
        bool b = false;
        auto bad = [&](const char* why) { problems.push_back({line_no, key, why}); };
        auto num = [&](std::uint32_t lo, std::uint32_t hi) {
            if (detail::to_uint(val, lo, hi, n)) return true;
            bad("out of range or not a number");
            return false;
        };
        auto real = [&](double lo, double hi) {
            if (detail::to_double(val, lo, hi, d)) return true;
            bad("out of range or not a number");
            return false;
        };
        auto flag = [&]() {
            if (detail::to_bool(val, b)) return true;
            bad("expected on or off");
            return false;
        };
        if (key == "name") {
            if (!val.empty() && val.size() <= 32) s.name = val; else bad("1 to 32 characters");
        } else if (key == "address") {
            if (val.empty() || detail::is_ipv4(val)) s.address = val; else bad("not an IPv4 address");
        } else if (key == "token") {
            if (val.empty() || (val.size() >= 8 && val.size() <= 64)) s.token = val; else bad("8 to 64 characters");
        } else if (key == "port") {
            if (num(1, 65535)) s.port = static_cast<std::uint16_t>(n);
        } else if (key == "camera_enabled") {
            if (flag()) s.camera_enabled = b;
        } else if (key == "camera_width") {
            if (num(160, 1600)) s.camera_width = static_cast<std::uint16_t>(n);
        } else if (key == "camera_height") {
            if (num(120, 1200)) s.camera_height = static_cast<std::uint16_t>(n);
        } else if (key == "camera_quality") {
            if (num(4, 63)) s.camera_quality = static_cast<std::uint16_t>(n);
        } else if (key == "camera_fps_max") {
            if (num(1, 30)) s.camera_fps_max = static_cast<std::uint16_t>(n);
        } else if (key == "rtsp_enabled") {
            if (flag()) s.rtsp_enabled = b;
        } else if (key == "rtsp_port") {
            if (num(1, 65535)) s.rtsp_port = static_cast<std::uint16_t>(n);
        } else if (key == "onvif_enabled") {
            if (flag()) s.onvif_enabled = b;
        } else if (key == "stream_user") {
            if (!val.empty() && val.size() <= 32 && val.find(':') == std::string::npos) s.stream_user = val; else bad("1 to 32 characters, no colon");
        } else if (key == "stream_password") {
            if (val.empty() || (val.size() >= 8 && val.size() <= 64)) s.stream_password = val; else bad("8 to 64 characters");
        } else if (key == "laser_enabled") {
            if (flag()) s.laser_enabled = b;
        } else if (key == "laser_samples") {
            if (num(1, 31)) s.laser_samples = static_cast<std::uint16_t>(n);
        } else if (key == "laser_timeout_ms") {
            if (num(100, 10000)) s.laser_timeout_ms = n;
        } else if (key == "laser_offset_mm") {
            if (real(-50.0, 50.0)) s.laser_offset_mm = d;
        } else if (key == "laser_range_min_mm") {
            if (real(0.0, 1000.0)) s.laser_range_min_mm = d;
        } else if (key == "laser_range_max_mm") {
            if (real(1.0, 4000.0)) s.laser_range_max_mm = d;
        } else if (key == "laser_sigma_max_mm") {
            if (real(0.01, 5.0)) s.laser_sigma_max_mm = d;
        } else if (key == "rpm_enabled") {
            if (flag()) s.rpm_enabled = b;
        } else if (key == "rpm_pulses_per_rev") {
            if (num(1, 64)) s.rpm_pulses_per_rev = n;
        } else if (key == "rpm_max") {
            if (num(1000, 200000)) s.rpm_max = n;
        } else if (key == "rpm_stale_ms") {
            if (num(200, 10000)) s.rpm_stale_ms = n;
        } else if (key == "temp_sensor") {
            if (val == "none") s.temp_sensor = TempSensor::None;
            else if (val == "ds18b20") s.temp_sensor = TempSensor::Ds18b20;
            else if (val == "ntc") s.temp_sensor = TempSensor::Ntc;
            else if (val == "mlx90614") s.temp_sensor = TempSensor::Mlx90614;
            else bad("not none, ds18b20, ntc or mlx90614");
        } else if (key == "temp_warn_c") {
            if (real(20.0, 150.0)) s.temp_warn_c = d;
        } else if (key == "temp_critical_c") {
            if (real(20.0, 200.0)) s.temp_critical_c = d;
        } else if (key == "temp_hysteresis_c") {
            if (real(0.5, 20.0)) s.temp_hysteresis_c = d;
        } else if (key == "protect_output") {
            if (flag()) s.protect_output = b;
        } else {
            bad("unknown key");
        }
    }
    // the ranges must make sense together: the critical temperature is above the warning, and the range of the laser is not upside down
    if (s.temp_critical_c <= s.temp_warn_c) {
        problems.push_back({0, "temp_critical_c", "must be above temp_warn_c; the defaults are used"});
        s.temp_warn_c = Settings{}.temp_warn_c;
        s.temp_critical_c = Settings{}.temp_critical_c;
    }
    if (s.laser_range_max_mm <= s.laser_range_min_mm) {
        problems.push_back({0, "laser_range_max_mm", "must be above laser_range_min_mm; the defaults are used"});
        s.laser_range_min_mm = Settings{}.laser_range_min_mm;
        s.laser_range_max_mm = Settings{}.laser_range_max_mm;
    }
    return s;
}

}  // namespace bungard_aux
