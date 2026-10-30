// EH3D-NODE-BUNGARD-AUX - core/laser.hpp
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
// One measure of the height of the table from several readings of the laser: the readings that are out of the range or that the sensor itself marks as bad are left
// out, the odd ones are rejected (median and median absolute deviation), the median of the rest is the distance and how much they spread says whether the measure
// can be trusted. Nothing here touches the sensor, so it is tested on a computer.
#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace bungard_aux {

struct LaserReading {
    double distance_mm = 0.0;
    bool valid = true;  // the sensor's own verdict (signal strong enough, no phase error)
};

enum class MeasureWhy {
    None,
    NoReadings,  // nothing arrived before the time ran out
    OutOfRange,  // every reading was valid but outside the range
    TooFew,      // less than half of the asked readings were usable
    TooNoisy     // the readings spread more than the setting allows
};

struct LaserLimits {
    double range_min_mm = 20.0;
    double range_max_mm = 400.0;
    double sigma_max_mm = 0.15;
    double offset_mm = 0.0;
};

struct Measure {
    bool ok = false;
    MeasureWhy why = MeasureWhy::NoReadings;
    double distance_mm = 0.0;  // the median of the readings that were kept, plus the offset
    double sigma_mm = 0.0;     // their standard deviation
    int asked = 0;
    int used = 0;
    int rejected = 0;
};

inline const char* why_text(MeasureWhy w) {
    switch (w) {
        case MeasureWhy::None: return "ok";
        case MeasureWhy::NoReadings: return "no_readings";
        case MeasureWhy::OutOfRange: return "out_of_range";
        case MeasureWhy::TooFew: return "too_few";
        case MeasureWhy::TooNoisy: return "too_noisy";
    }
    return "unknown";
}

namespace detail {

inline double median(std::vector<double> v) {
    std::sort(v.begin(), v.end());
    std::size_t n = v.size();
    return n % 2 ? v[n / 2] : (v[n / 2 - 1] + v[n / 2]) / 2.0;
}

}  // namespace detail

/// The measure of `asked` readings (those that arrived are in `readings`).
inline Measure measure_distance(const std::vector<LaserReading>& readings, int asked, const LaserLimits& lim) {
    Measure m;
    m.asked = asked;
    if (readings.empty()) return m;
    std::vector<double> inside;
    bool any_valid = false;
    for (const auto& r : readings) {
        if (!r.valid || !std::isfinite(r.distance_mm)) continue;
        any_valid = true;
        if (r.distance_mm >= lim.range_min_mm && r.distance_mm <= lim.range_max_mm) inside.push_back(r.distance_mm);
    }
    if (inside.empty()) {
        m.why = any_valid ? MeasureWhy::OutOfRange : MeasureWhy::TooFew;
        return m;
    }
    // the odd ones out: farther from the median than three times the usual spread (never less than 0.05 mm, the resolution of the sensor)
    double med = detail::median(inside);
    std::vector<double> dev;
    dev.reserve(inside.size());
    for (double x : inside) dev.push_back(std::fabs(x - med));
    double mad = detail::median(dev) * 1.4826;
    double cut = std::max(3.0 * mad, 0.05);
    std::vector<double> kept;
    for (double x : inside) {
        if (std::fabs(x - med) <= cut) kept.push_back(x);
    }
    m.rejected = static_cast<int>(readings.size() - kept.size());
    m.used = static_cast<int>(kept.size());
    if (m.used * 2 < asked) {
        m.why = MeasureWhy::TooFew;
        return m;
    }
    double mean = 0.0;
    for (double x : kept) mean += x;
    mean /= static_cast<double>(kept.size());
    double var = 0.0;
    for (double x : kept) var += (x - mean) * (x - mean);
    m.sigma_mm = kept.size() > 1 ? std::sqrt(var / static_cast<double>(kept.size() - 1)) : 0.0;
    m.distance_mm = detail::median(kept) + lim.offset_mm;
    if (m.sigma_mm > lim.sigma_max_mm) {
        m.why = MeasureWhy::TooNoisy;
        return m;
    }
    m.ok = true;
    m.why = MeasureWhy::None;
    return m;
}

}  // namespace bungard_aux
