// EH3D-NODE-BUNGARD-AUX - core/aux_config.hpp
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
// Every tuned value of the node lives in one settings file (head.cfg), never in the code. The file is `key = value` lines with `#` comments; a value out of range or
// a key nobody knows is reported and the default stays, so a typing mistake never turns a safety limit off.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "camera.hpp"
#include "laser_hw.hpp"
#include "onewire.hpp"

namespace bungard_aux {

enum class TempSensor { None, Ds18b20, Ntc, Mlx90614 };

/// The colour of the light of the ring around the camera. Neutral is the plain white the camera is meant to look at (all three colours of the LED at the same strength); cool and warm are tinted whites; red, green and blue make the resist and the copper stand apart.
enum class Tone { Neutral, Cool, Warm, Red, Green, Blue, Custom };  // Custom is any colour a request asks for; the settings file offers the others

/// The order in which the LEDs of the ring take their three colours: the WS2812 takes green first, a few clones take red first.
enum class ColourOrder { Grb, Rgb };

inline const char* tone_text(Tone t) {
    switch (t) {
        case Tone::Cool: return "cool";
        case Tone::Warm: return "warm";
        case Tone::Red: return "red";
        case Tone::Green: return "green";
        case Tone::Blue: return "blue";
        case Tone::Custom: return "custom";
        default: return "white";
    }
}

inline bool tone_from_text(const std::string& v, Tone& out) {
    if (v == "neutral") {  // the old name of the white
        out = Tone::Neutral;
        return true;
    }
    static const Tone all[] = {Tone::Neutral, Tone::Cool, Tone::Warm, Tone::Red, Tone::Green, Tone::Blue};
    for (Tone t : all) {
        if (v == tone_text(t)) {
            out = t;
            return true;
        }
    }
    return false;
}

enum class RpmInput { Digital, Analog };
enum class RpmEdge { Rising, Falling };

inline const char* rpm_input_text(RpmInput i) { return i == RpmInput::Analog ? "analog" : "digital"; }
inline const char* rpm_edge_text(RpmEdge e) { return e == RpmEdge::Falling ? "falling" : "rising"; }

struct Settings {
    // the link: the node has a wired network port; an empty address means DHCP
    std::string name = "bungard-aux";
    std::string address;                    // static IPv4, empty: DHCP
    std::string token;                      // write-only: never shown, never logged; empty: the node refuses every command that changes something
    std::uint16_t port = 80;                // 1..65535

    // the camera (MJPEG over HTTP)
    bool camera_enabled = true;
    CameraModel camera_model = CameraModel::Auto;  // auto: the sensor says which one it is; the OV2640 gives up to 1600x1200, the OV5640 up to 2592x1944
    std::uint16_t camera_width = 640;       // 160..2592 (the nearest size the camera can give, not larger)
    std::uint16_t camera_height = 480;      // 120..1944
    std::uint16_t camera_quality = 12;      // JPEG quality, 4 (best) .. 63 (smallest)
    // the picture: what the sensor does to it before it is sent (the two cameras take all of these)
    bool camera_hmirror = false;            // mirror left to right
    bool camera_vflip = false;              // turn upside down (a camera that is mounted the other way up)
    int camera_brightness = 0;              // -2..2
    int camera_contrast = 0;                // -2..2
    int camera_saturation = 0;              // -2..2
    bool camera_auto_exposure = true;       // the sensor chooses the exposure (the usual: the ring of LEDs changes the light)
    std::uint32_t camera_exposure = 300;    // with the exposure by hand: 0..1200 (units of the sensor, about lines of the picture)
    bool camera_auto_whitebalance = true;   // the sensor chooses the balance of white
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
    DistanceMode laser_distance_mode = DistanceMode::Long;  // short (to about 1.3 m, less trouble with light) or long (to about 4 m, best in the dark)
    std::uint32_t laser_timing_budget_ms = 100;  // how long the sensor looks for each reading: short 15, 20, 33, 50, 100, 200 or 500; long the same without 15
    std::uint8_t laser_roi_w = 16;          // the area of the receiving array that looks at the table, 4..16 detectors wide ...
    std::uint8_t laser_roi_h = 16;          // ... and high (setting: laser_roi = WxH); a small area looks at a small spot
    std::uint32_t laser_roi_center = 0;     // 0: the centre the sensor reports; 1..255 the detector the small area is centred on
    double laser_outlier_k = 3.0;           // a reading farther than this many spreads from the median is thrown away, 1..10
    double laser_outlier_floor_mm = 0.05;   // the spread is never taken smaller than this, 0.01..1
    bool laser_calibration_enabled = false; // safety: the calibration at a known distance changes what every reading means, so it is off until an administrator switches it on
    double laser_cal_offset_mm = 0.0;       // found by the calibration (-50..50), added to every distance
    double laser_cal_xtalk_cps = 0.0;       // found by the calibration (0..128000), written to the sensor when it starts

    // the speed of the spindle
    bool rpm_enabled = true;
    std::uint32_t rpm_pulses_per_rev = 1;   // 1..64
    RpmInput rpm_input = RpmInput::Digital; // digital: the output DO of the sensor on an interrupt; analog: the output AO read by the fast ADC with a Schmitt trigger
    RpmEdge rpm_edge = RpmEdge::Rising;     // which edge of the signal is a pulse (a TCRT5000 module goes low in front of a bright mark: falling)
    std::uint32_t rpm_threshold_hi_mv = 2000;  // analogue input: the signal rises through this level (50..3300)
    std::uint32_t rpm_threshold_lo_mv = 1000;  // and falls through this one (0..3250), always below the high one
    bool rpm_auto_threshold = true;         // analogue input: the thresholds follow the signal (35 % and 65 % between its extremes)
    std::uint32_t rpm_max = 65000;          // above this the speed is an alarm, 1000..200000
    std::uint32_t rpm_stale_ms = 1500;      // no pulse for this long means the spindle stopped, 200..10000

    // the temperature of the spindle
    TempSensor temp_sensor = TempSensor::None;
    int temp_resolution = 12;               // the DS18B20: 9, 10, 11 or 12 bits (a conversion takes 94 to 750 ms)
    std::string temp_rom = "auto";          // the DS18B20 of the spindle: auto (the only one on the wire, or the first found) or the 16 hex digits of its address
    double ntc_series_ohm = 10000.0;        // the NTC thermistor: the fixed resistor of the divider, 100..1000000
    double ntc_r25_ohm = 10000.0;           // its resistance at 25 degrees, 100..1000000
    double ntc_beta = 3950.0;               // its beta, 1000..10000
    double temp_warn_c = 55.0;              // 20..150
    double temp_critical_c = 70.0;          // 20..200, always above the warning
    double temp_hysteresis_c = 3.0;         // 0.5..20

    // the ring of LEDs around the camera (eight WS2812 on one wire): it lights the area under the head so the camera sees well
    bool light_enabled = true;
    std::uint32_t light_leds = 8;           // 1..64
    std::uint32_t light_level = 60;         // 0..100, the level when the light is turned on without saying how bright
    std::uint32_t light_max_level = 80;     // 1..100, the ceiling: whatever is asked, the ring never goes over it
    Tone light_tone = Tone::Neutral;        // white (the one for the camera), cool, warm, red, green or blue
    std::uint32_t light_auto_off_s = 600;   // 0..86400, the light turns itself off after this long (0: never)
    bool light_on_at_start = false;
    ColourOrder light_order = ColourOrder::Grb;  // grb (WS2812) or rgb (a few clones)

    // the safety output: it opens when the spindle is too hot or too fast, so the control unit's stop line can use it
    bool protect_output = true;

    // the self-check: the node judges its own status and says it with codes (core/self_check.hpp); the limits are here, never in the code
    bool selfcheck_enabled = true;
    std::uint32_t selfcheck_laser_age_max_s = 600;  // 10..86400, a laser reading older than this is reported
    std::uint32_t selfcheck_grace_s = 30;           // 0..600, after the start a reading that is not there yet is not a fault
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

/// Reads a settings text on top of `base` (what the panel sends changes only what it names); every line that cannot be used is listed in `problems` and changes nothing.
inline Settings parse_settings_over(const Settings& base, const std::string& text, std::vector<Problem>& problems) {
    Settings s = base;
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
        int sn = 0;
        auto inum = [&](int lo, int hi) {
            if (detail::to_double(val, lo, hi, d) && d == static_cast<double>(static_cast<int>(d))) {
                sn = static_cast<int>(d);
                return true;
            }
            bad("a whole number in the range");
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
        } else if (key == "camera_model") {
            if (val == "auto") s.camera_model = CameraModel::Auto;
            else if (val == "ov2640") s.camera_model = CameraModel::Ov2640;
            else if (val == "ov5640") s.camera_model = CameraModel::Ov5640;
            else bad("auto, ov2640 or ov5640");
        } else if (key == "camera_width") {
            if (num(160, 2592)) s.camera_width = static_cast<std::uint16_t>(n);
        } else if (key == "camera_height") {
            if (num(120, 1944)) s.camera_height = static_cast<std::uint16_t>(n);
        } else if (key == "camera_quality") {
            if (num(4, 63)) s.camera_quality = static_cast<std::uint16_t>(n);
        } else if (key == "camera_hmirror") {
            if (flag()) s.camera_hmirror = b;
        } else if (key == "camera_vflip") {
            if (flag()) s.camera_vflip = b;
        } else if (key == "camera_brightness") {
            if (inum(-2, 2)) s.camera_brightness = static_cast<int>(sn);
        } else if (key == "camera_contrast") {
            if (inum(-2, 2)) s.camera_contrast = static_cast<int>(sn);
        } else if (key == "camera_saturation") {
            if (inum(-2, 2)) s.camera_saturation = static_cast<int>(sn);
        } else if (key == "camera_auto_exposure") {
            if (flag()) s.camera_auto_exposure = b;
        } else if (key == "camera_exposure") {
            if (num(0, 1200)) s.camera_exposure = n;
        } else if (key == "camera_auto_whitebalance") {
            if (flag()) s.camera_auto_whitebalance = b;
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
        } else if (key == "laser_distance_mode") {
            DistanceMode dm;
            if (distance_mode_from_text(val, dm)) s.laser_distance_mode = dm; else bad("short or long");
        } else if (key == "laser_timing_budget_ms") {
            if (num(15, 500)) s.laser_timing_budget_ms = n;
        } else if (key == "laser_roi") {
            int w = 0, h = 0;
            if (parse_roi(val, w, h)) { s.laser_roi_w = static_cast<std::uint8_t>(w); s.laser_roi_h = static_cast<std::uint8_t>(h); } else bad("WxH with each side from 4 to 16");
        } else if (key == "laser_roi_center") {
            if (num(0, 255)) s.laser_roi_center = n;
        } else if (key == "laser_outlier_k") {
            if (real(1.0, 10.0)) s.laser_outlier_k = d;
        } else if (key == "laser_outlier_floor_mm") {
            if (real(0.01, 1.0)) s.laser_outlier_floor_mm = d;
        } else if (key == "laser_calibration_enabled") {
            if (flag()) s.laser_calibration_enabled = b;
        } else if (key == "laser_cal_offset_mm") {
            if (real(-kMaxCalOffsetMm, kMaxCalOffsetMm)) s.laser_cal_offset_mm = d;
        } else if (key == "laser_cal_xtalk_cps") {
            if (real(0.0, kMaxXtalkCps)) s.laser_cal_xtalk_cps = d;
        } else if (key == "rpm_enabled") {
            if (flag()) s.rpm_enabled = b;
        } else if (key == "rpm_pulses_per_rev") {
            if (num(1, 64)) s.rpm_pulses_per_rev = n;
        } else if (key == "rpm_input") {
            if (val == "digital") s.rpm_input = RpmInput::Digital;
            else if (val == "analog") s.rpm_input = RpmInput::Analog;
            else bad("digital or analog");
        } else if (key == "rpm_edge") {
            if (val == "rising") s.rpm_edge = RpmEdge::Rising;
            else if (val == "falling") s.rpm_edge = RpmEdge::Falling;
            else bad("rising or falling");
        } else if (key == "rpm_threshold_hi_mv") {
            if (num(50, 3300)) s.rpm_threshold_hi_mv = n;
        } else if (key == "rpm_threshold_lo_mv") {
            if (num(0, 3250)) s.rpm_threshold_lo_mv = n;
        } else if (key == "rpm_auto_threshold") {
            if (flag()) s.rpm_auto_threshold = b;
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
        } else if (key == "temp_resolution") {
            if (num(9, 12)) s.temp_resolution = static_cast<int>(n);
        } else if (key == "temp_rom") {
            if (temp_rom_setting_valid(val)) {
                s.temp_rom = val;
                if (val != "auto") for (char& ch : s.temp_rom) if (ch >= 'a' && ch <= 'f') ch = static_cast<char>(ch - 'a' + 'A');
            } else {
                bad("auto or the 16 hex digits of an address");
            }
        } else if (key == "ntc_series_ohm") {
            if (real(100.0, 1000000.0)) s.ntc_series_ohm = d;
        } else if (key == "ntc_r25_ohm") {
            if (real(100.0, 1000000.0)) s.ntc_r25_ohm = d;
        } else if (key == "ntc_beta") {
            if (real(1000.0, 10000.0)) s.ntc_beta = d;
        } else if (key == "temp_warn_c") {
            if (real(20.0, 150.0)) s.temp_warn_c = d;
        } else if (key == "temp_critical_c") {
            if (real(20.0, 200.0)) s.temp_critical_c = d;
        } else if (key == "temp_hysteresis_c") {
            if (real(0.5, 20.0)) s.temp_hysteresis_c = d;
        } else if (key == "light_enabled") {
            if (flag()) s.light_enabled = b;
        } else if (key == "light_leds") {
            if (num(1, 64)) s.light_leds = n;
        } else if (key == "light_level") {
            if (num(0, 100)) s.light_level = n;
        } else if (key == "light_max_level") {
            if (num(1, 100)) s.light_max_level = n;
        } else if (key == "light_tone") {
            Tone t;
            if (tone_from_text(val, t)) s.light_tone = t; else bad("not white, cool, warm, red, green or blue");
        } else if (key == "light_auto_off_s") {
            if (num(0, 86400)) s.light_auto_off_s = n;
        } else if (key == "light_on_at_start") {
            if (flag()) s.light_on_at_start = b;
        } else if (key == "light_order") {
            if (val == "grb") s.light_order = ColourOrder::Grb;
            else if (val == "rgb") s.light_order = ColourOrder::Rgb;
            else bad("not grb or rgb");
        } else if (key == "protect_output") {
            if (flag()) s.protect_output = b;
        } else if (key == "selfcheck_enabled") {
            if (flag()) s.selfcheck_enabled = b;
        } else if (key == "selfcheck_laser_age_max_s") {
            if (num(10, 86400)) s.selfcheck_laser_age_max_s = n;
        } else if (key == "selfcheck_grace_s") {
            if (num(0, 600)) s.selfcheck_grace_s = n;
        } else {
            bad("unknown key");
        }
    }
    // the ranges must make sense together: the critical temperature is above the warning, and the range of the laser is not upside down
    if (s.rpm_threshold_lo_mv >= s.rpm_threshold_hi_mv) {
        problems.push_back({0, "rpm_threshold_lo_mv", "must be below rpm_threshold_hi_mv; the defaults are used"});
        s.rpm_threshold_hi_mv = Settings{}.rpm_threshold_hi_mv;
        s.rpm_threshold_lo_mv = Settings{}.rpm_threshold_lo_mv;
    }
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
    // the timing budget must be one the distance mode has (15 ms is for the short mode only)
    if (!budget_valid(s.laser_distance_mode, static_cast<int>(s.laser_timing_budget_ms))) {
        problems.push_back({0, "laser_timing_budget_ms", "not a budget of this distance mode; the default is used"});
        s.laser_timing_budget_ms = Settings{}.laser_timing_budget_ms;
    }
    // the level of the light cannot start above its own ceiling
    if (s.light_level > s.light_max_level) {
        problems.push_back({0, "light_level", "above light_max_level; the ceiling is used"});
        s.light_level = s.light_max_level;
    }
    return s;
}

/// Reads a settings file. The result starts from the defaults; every line that cannot be used is listed in `problems` and changes nothing.
inline Settings parse_settings(const std::string& text, std::vector<Problem>& problems) { return parse_settings_over(Settings{}, text, problems); }

inline const char* temp_sensor_text(TempSensor t) {
    switch (t) {
        case TempSensor::Ds18b20: return "ds18b20";
        case TempSensor::Ntc: return "ntc";
        case TempSensor::Mlx90614: return "mlx90614";
        default: return "none";
    }
}

}  // namespace bungard_aux
