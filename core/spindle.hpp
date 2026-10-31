// EH3D-NODE-BUNGARD-AUX - core/spindle.hpp
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
// The speed and the temperature of the spindle. The speed comes from the time between the pulses of a sensor on the spindle (a hall sensor or a slotted disc); a
// short average smooths the pulses, and no pulse for a while means the spindle stopped. The temperature has a warning and a critical level with a hysteresis, so
// the state does not flicker around a limit. The node opens its protect output when the spindle is too hot or too fast.
#pragma once

#include <algorithm>
#include <cstdint>
#include <deque>
#include <vector>

#include "aux_config.hpp"

namespace bungard_aux {

/// How clean the train of pulses is: nothing yet, good, or noisy (a fifth or more of the last pulses were refused or had to be mended).
enum class RpmQuality { None, Good, Noisy };

inline const char* rpm_quality_text(RpmQuality q) {
    switch (q) {
        case RpmQuality::None: return "none";
        case RpmQuality::Good: return "good";
        case RpmQuality::Noisy: return "noisy";
    }
    return "none";
}

class RpmMeter {
public:
    RpmMeter(std::uint32_t pulses_per_rev, std::uint32_t stale_ms) : ppr_(pulses_per_rev ? pulses_per_rev : 1), stale_us_(static_cast<std::uint64_t>(stale_ms) * 1000) {}

    /// The fastest the pulses can come: a pulse closer than this to the one before it is noise (the variador and the motor make spikes), not a turn. Derived from the speed the spindle
    /// may reach (`rpm_max`, with a quarter of room above it) and the pulses of a turn; 0 turns the filter off.
    void set_limit_rpm(std::uint32_t rpm_max) {
        min_gap_us_ = rpm_max ? static_cast<std::uint64_t>(60.0e6 / (static_cast<double>(rpm_max) * 1.25 * static_cast<double>(ppr_))) : 0;
    }

    /// A pulse at `t_us` (a clock that only goes up).
    void pulse(std::uint64_t t_us) {
        if (have_last_ && t_us > last_us_) {
            std::uint64_t gap = t_us - last_us_;
            if (gap < min_gap_us_) {  // faster than the spindle can turn: noise
                note(false);
                ++rejected_;
                return;
            }
            const std::uint64_t med = median_gap();
            if (gaps_.size() >= kHistory && med > 0) {
                if (gap * 10 < med * 4) {  // less than 0.4 of the usual gap: a spike in the middle of a turn
                    note(false);
                    ++rejected_;
                    return;
                }
                if (gap * 10 >= med * 17 && gap * 10 <= med * 23) {  // twice the usual gap: a pulse was missed; the two turns are counted as two
                    gaps_.push_back(gap / 2);
                    gaps_.push_back(gap / 2);
                    while (gaps_.size() > kWindow) gaps_.pop_front();
                    ++mended_;
                    note(false);
                    last_us_ = t_us;
                    return;
                }
            }
            gaps_.push_back(gap);
            if (gaps_.size() > kWindow) gaps_.pop_front();
            note(true);
        }
        last_us_ = t_us;
        have_last_ = true;
    }

    /// The turns per minute at `now_us`: zero when no pulse came for the stale time (or none ever did).
    double rpm(std::uint64_t now_us) const {
        if (!have_last_ || gaps_.empty() || now_us < last_us_ || now_us - last_us_ > stale_us_) return 0.0;
        double sum = 0.0;
        for (auto g : gaps_) sum += static_cast<double>(g);
        double mean_us = sum / static_cast<double>(gaps_.size());
        if (mean_us <= 0.0) return 0.0;
        return 60.0e6 / (mean_us * static_cast<double>(ppr_));
    }

    bool turning(std::uint64_t now_us) const { return rpm(now_us) > 0.0; }

    std::uint64_t pulses_rejected() const { return rejected_; }
    std::uint64_t pulses_mended() const { return mended_; }

    RpmQuality quality() const {
        if (recent_.empty()) return RpmQuality::None;
        int bad = 0;
        for (bool ok : recent_) bad += ok ? 0 : 1;
        return bad * 5 >= static_cast<int>(recent_.size()) ? RpmQuality::Noisy : RpmQuality::Good;
    }

private:
    static constexpr std::size_t kWindow = 8;
    static constexpr std::size_t kHistory = 4;  // the gaps needed before a gap is judged against the usual one
    static constexpr std::size_t kRecent = 32;

    std::uint64_t median_gap() const {
        if (gaps_.empty()) return 0;
        std::vector<std::uint64_t> v(gaps_.begin(), gaps_.end());
        std::sort(v.begin(), v.end());
        return v[v.size() / 2];
    }
    void note(bool ok) {
        recent_.push_back(ok);
        if (recent_.size() > kRecent) recent_.pop_front();
    }

    std::uint32_t ppr_;
    std::uint64_t stale_us_;
    std::uint64_t min_gap_us_ = 0;
    std::deque<std::uint64_t> gaps_;
    std::deque<bool> recent_;
    std::uint64_t last_us_ = 0;
    std::uint64_t rejected_ = 0;
    std::uint64_t mended_ = 0;
    bool have_last_ = false;
};

/// The Schmitt trigger of the analogue input of the sensor (the output AO of the TCRT5000 module): two thresholds so that noise around one of them is not a pulse, and, when asked,
/// thresholds that follow the signal (the lowest and the highest level seen in the last turns, 35 % and 65 % of the way between them). Nothing is a pulse while the signal moves
/// less than `kMinSpanMv` (no mark in front of the sensor, or the sensor is off).
class Schmitt {
public:
    static constexpr int kMinSpanMv = 100;

    Schmitt(int hi_mv, int lo_mv, bool automatic) : hi_(hi_mv), lo_(lo_mv), auto_(automatic) {}

    /// One sample in millivolts. +1 when the signal crosses up through the high threshold, -1 down through the low one, 0 otherwise.
    int feed(int mv) {
        if (auto_) {
            if (first_) {
                vmin_ = vmax_ = mv;
                first_ = false;
            }
            vmin_ = std::min(vmin_, static_cast<double>(mv));
            vmax_ = std::max(vmax_, static_cast<double>(mv));
            // the extremes forget slowly, so a change of light or of distance is followed
            const double mid = (vmin_ + vmax_) / 2.0;
            vmin_ += (mid - vmin_) * kForget;
            vmax_ += (mid - vmax_) * kForget;
            const double span = vmax_ - vmin_;
            if (span < kMinSpanMv) return 0;
            hi_ = static_cast<int>(vmin_ + 0.65 * span);
            lo_ = static_cast<int>(vmin_ + 0.35 * span);
        }
        int crossing = 0;
        if (!high_ && mv >= hi_) {
            high_ = true;
            crossing = 1;
        } else if (high_ && mv <= lo_) {
            high_ = false;
            crossing = -1;
        }
        return crossing;
    }

    int high_mv() const { return hi_; }
    int low_mv() const { return lo_; }
    bool is_high() const { return high_; }

private:
    static constexpr double kForget = 0.00002;
    int hi_;
    int lo_;
    bool auto_;
    bool first_ = true;
    bool high_ = false;
    double vmin_ = 0.0;
    double vmax_ = 0.0;
};

enum class Heat { Unknown, Ok, Warm, Hot };

inline const char* heat_text(Heat h) {
    switch (h) {
        case Heat::Unknown: return "unknown";
        case Heat::Ok: return "ok";
        case Heat::Warm: return "warm";
        case Heat::Hot: return "hot";
    }
    return "unknown";
}

class Thermal {
public:
    Thermal(double warn_c, double critical_c, double hysteresis_c) : warn_(warn_c), crit_(critical_c), hyst_(hysteresis_c) {}

    /// A new reading; a reading that is not a number (a sensor that failed) leaves the state unknown, which the protect output treats as hot.
    Heat update(double celsius, bool valid) {
        if (!valid) {
            state_ = Heat::Unknown;
            return state_;
        }
        last_ = celsius;
        switch (state_) {
            case Heat::Unknown:
            case Heat::Ok:
                state_ = celsius >= crit_ ? Heat::Hot : celsius >= warn_ ? Heat::Warm : Heat::Ok;
                break;
            case Heat::Warm:
                state_ = celsius >= crit_ ? Heat::Hot : celsius < warn_ - hyst_ ? Heat::Ok : Heat::Warm;
                break;
            case Heat::Hot:
                state_ = celsius < crit_ - hyst_ ? (celsius >= warn_ ? Heat::Warm : Heat::Ok) : Heat::Hot;
                break;
        }
        return state_;
    }

    Heat state() const { return state_; }
    double last_c() const { return last_; }

private:
    double warn_, crit_, hyst_;
    Heat state_ = Heat::Unknown;
    double last_ = 0.0;
};

/// Whether the protect output is open (the spindle must stop): too fast, or too hot. A sensor that is switched on and does not answer counts as hot, so a broken
/// wire never leaves the spindle unwatched; a sensor that is switched off in the settings is never asked.
inline bool protect_open(const Settings& s, double rpm, Heat heat) {
    if (!s.protect_output) return false;
    if (s.rpm_enabled && rpm > static_cast<double>(s.rpm_max)) return true;
    if (s.temp_sensor != TempSensor::None && (heat == Heat::Hot || heat == Heat::Unknown)) return true;
    return false;
}

}  // namespace bungard_aux
