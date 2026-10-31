// EH3D-NODE-BUNGARD-AUX - core/laser_hw.hpp
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
// What the VL53L1X is told and how it is calibrated, as plain data: the distance mode (short or long, the two the sensor has), the timing budget, the area of the receiving array that
// looks at the table, the crosstalk and the offset found at a known distance. The registers and the values are those of the ST ultra lite driver for the VL53L1X (the same ones the
// drivers of Adafruit and SparkFun carry); this file only lists them, so the firmware writes them in order and a computer can test the list. Nothing here touches the sensor.
#pragma once

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "laser.hpp"

namespace bungard_aux {

enum class DistanceMode { Short, Long };

inline const char* distance_mode_text(DistanceMode m) { return m == DistanceMode::Short ? "short" : "long"; }

inline bool distance_mode_from_text(const std::string& v, DistanceMode& out) {
    if (v == "short") { out = DistanceMode::Short; return true; }
    if (v == "long") { out = DistanceMode::Long; return true; }
    return false;
}

/// The timing budgets the sensor accepts, in ms, and the two timeouts (macro periods of the ranging phases A and B) each one is made of.
struct Budget {
    int ms;
    std::uint16_t a;
    std::uint16_t b;
};

inline const std::vector<Budget>& budgets(DistanceMode m) {
    static const std::vector<Budget> short_mode = {{15, 0x001D, 0x0027}, {20, 0x0051, 0x006E}, {33, 0x00D6, 0x006E}, {50, 0x01AE, 0x01E8},
                                                   {100, 0x02E1, 0x0388}, {200, 0x03E1, 0x0496}, {500, 0x0591, 0x05C1}};
    static const std::vector<Budget> long_mode = {{20, 0x001E, 0x0022}, {33, 0x0060, 0x006E}, {50, 0x00AD, 0x00C6},
                                                  {100, 0x01CC, 0x01EA}, {200, 0x02D9, 0x02F8}, {500, 0x048F, 0x04A4}};
    return m == DistanceMode::Short ? short_mode : long_mode;
}

inline const Budget* find_budget(DistanceMode m, int ms) {
    for (const Budget& b : budgets(m)) {
        if (b.ms == ms) return &b;
    }
    return nullptr;
}

inline bool budget_valid(DistanceMode m, int ms) { return find_budget(m, ms) != nullptr; }

/// The time between two measurements: the sensor needs it at least as long as the budget; a few ms of margin, and never under 20 ms.
inline int inter_measurement_ms(int budget_ms) { return budget_ms + 5 < 20 ? 20 : budget_ms + 5; }

/// The size of the area of the receiving array that looks at the table, as `WxH` (each from 4 to 16 detectors).
inline bool parse_roi(const std::string& v, int& w, int& h) {
    const std::size_t x = v.find('x');
    if (x == std::string::npos || x == 0 || x + 1 >= v.size() || v.size() > 5) return false;
    int a = 0, b = 0;
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (i == x) continue;
        if (v[i] < '0' || v[i] > '9') return false;
        int& t = i < x ? a : b;
        t = t * 10 + (v[i] - '0');
    }
    if (a < 4 || a > 16 || b < 4 || b > 16) return false;
    w = a;
    h = b;
    return true;
}

inline std::string roi_text(int w, int h) { return std::to_string(w) + "x" + std::to_string(h); }

struct RegWrite {
    std::uint16_t reg;
    std::uint8_t bytes;  // 1 or 2 (a word goes high byte first)
    std::uint16_t value;
};

struct LaserHw {
    DistanceMode mode = DistanceMode::Long;
    int budget_ms = 100;
    int roi_w = 16;
    int roi_h = 16;
    int roi_center = 0;           // 0: the optical centre the sensor itself reports; another number is the detector the area is centred on
    int optical_center = 199;     // what the sensor reported (register 0x013E); used when roi_center is 0
    bool write_xtalk = false;     // the crosstalk register is only written when asked (the factory value stays otherwise)
    double xtalk_cps = 0.0;
};

/// The registers to write, in order, after the sensor is initialised: the distance mode, the timing budget, the area and the crosstalk. An empty list when the budget does not fit the mode.
inline std::vector<RegWrite> registers_for(const LaserHw& hw) {
    std::vector<RegWrite> out;
    const Budget* budget = find_budget(hw.mode, hw.budget_ms);
    if (!budget) return out;
    if (hw.mode == DistanceMode::Short) {
        out.push_back({0x004B, 1, 0x14});    // phase calibration timeout
        out.push_back({0x0060, 1, 0x07});    // VCSEL period A
        out.push_back({0x0063, 1, 0x05});    // VCSEL period B
        out.push_back({0x0069, 1, 0x38});    // valid phase, high
        out.push_back({0x0078, 2, 0x0705});  // window of interest
        out.push_back({0x007A, 2, 0x0606});  // initial phase
    } else {
        out.push_back({0x004B, 1, 0x0A});
        out.push_back({0x0060, 1, 0x0F});
        out.push_back({0x0063, 1, 0x0D});
        out.push_back({0x0069, 1, 0xB8});
        out.push_back({0x0078, 2, 0x0F0D});
        out.push_back({0x007A, 2, 0x0E0E});
    }
    out.push_back({0x005E, 2, budget->a});
    out.push_back({0x0061, 2, budget->b});
    // the area: a big one (over 10 detectors on a side) is always centred; a small one is centred where the settings say
    const int w = hw.roi_w < 4 ? 4 : hw.roi_w > 16 ? 16 : hw.roi_w;
    const int h = hw.roi_h < 4 ? 4 : hw.roi_h > 16 ? 16 : hw.roi_h;
    int centre = hw.roi_center > 0 ? hw.roi_center : hw.optical_center;
    if (w > 10 || h > 10) centre = 199;
    out.push_back({0x007F, 1, static_cast<std::uint16_t>(centre)});
    out.push_back({0x0080, 1, static_cast<std::uint16_t>(((h - 1) << 4) | (w - 1))});
    if (hw.write_xtalk) {
        double reg = hw.xtalk_cps * 512.0 / 1000.0;  // counts per second to kilo-counts in the 7.9 format of the sensor
        if (reg < 0.0) reg = 0.0;
        if (reg > 65535.0) reg = 65535.0;
        out.push_back({0x0018, 2, 0});
        out.push_back({0x001A, 2, 0});
        out.push_back({0x0016, 2, static_cast<std::uint16_t>(reg)});
    }
    return out;
}

// ------------------------------------------------------------------------------------------------------------------------------ calibration

/// The largest offset a calibration may produce (the same bound as the offset in the settings): a bigger one means the target is not where it was said to be.
constexpr double kMaxCalOffsetMm = 50.0;
/// The largest crosstalk the sensor register can hold, in counts per second.
constexpr double kMaxXtalkCps = 128000.0;

struct Calibration {
    bool ok = false;
    const char* why = "no_readings";  // no_readings, too_few, too_noisy, out_of_range, bad_target, offset_limit, no_signal
    double value = 0.0;               // the offset in mm, or the crosstalk in counts per second
    double sigma_mm = 0.0;
    int used = 0;
};

/// The offset that makes the sensor read `target_mm` where the target really is: the median of the good readings (the range limits are those of the settings) against the target.
/// Neither the offset of the settings nor an earlier calibration is in the readings it is given.
inline Calibration calibrate_offset(const std::vector<LaserReading>& readings, int asked, double target_mm, const LaserLimits& lim) {
    Calibration c;
    if (!(target_mm >= 10.0 && target_mm <= 4000.0)) {
        c.why = "bad_target";
        return c;
    }
    LaserLimits raw = lim;
    raw.offset_mm = 0.0;
    raw.cal_offset_mm = 0.0;
    raw.sigma_max_mm = std::max(lim.sigma_max_mm, 0.01);
    const Measure m = measure_distance(readings, asked, raw);
    c.sigma_mm = m.sigma_mm;
    c.used = m.used;
    if (!m.ok) {
        c.why = m.why == MeasureWhy::NoReadings ? "no_readings" : m.why == MeasureWhy::TooNoisy ? "too_noisy" : m.why == MeasureWhy::OutOfRange ? "out_of_range" : "too_few";
        return c;
    }
    const double offset = target_mm - m.distance_mm;
    if (std::fabs(offset) > kMaxCalOffsetMm) {
        c.why = "offset_limit";
        return c;
    }
    c.ok = true;
    c.why = "ok";
    c.value = offset;
    return c;
}

/// The crosstalk (the light of the sensor's own window that comes back) from a flat target at a known distance, with the ST formula: the signal rate that the sensor sees beyond
/// what the distance explains, per detector. Readings carry the signal in kcps and the number of detectors.
inline Calibration calibrate_xtalk(const std::vector<LaserReading>& readings, double target_mm) {
    Calibration c;
    if (!(target_mm >= 10.0 && target_mm <= 4000.0)) {
        c.why = "bad_target";
        return c;
    }
    double signal = 0.0, distance = 0.0, spads = 0.0;
    int n = 0;
    for (const LaserReading& r : readings) {
        if (!r.valid || !std::isfinite(r.distance_mm)) continue;
        signal += r.signal_kcps;
        distance += r.distance_mm;
        spads += r.spads;
        ++n;
    }
    c.used = n;
    if (n < 10) {
        c.why = readings.empty() ? "no_readings" : "too_few";
        return c;
    }
    signal /= n;
    distance /= n;
    spads /= n;
    if (!(signal > 0.0) || !(spads > 0.0)) {
        c.why = "no_signal";
        return c;
    }
    double cps = 1000.0 * signal * (1.0 - distance / target_mm) / spads;
    if (cps < 0.0) cps = 0.0;  // reading farther than the target: nothing to compensate
    if (cps > kMaxXtalkCps) {
        c.why = "out_of_range";
        return c;
    }
    c.ok = true;
    c.why = "ok";
    c.value = cps;
    return c;
}

}  // namespace bungard_aux
