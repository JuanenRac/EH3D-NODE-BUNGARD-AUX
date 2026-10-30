// EH3D-NODE-BUNGARD-AUX - core/spindle.hpp
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
// The speed and the temperature of the spindle. The speed comes from the time between the pulses of a sensor on the spindle (a hall sensor or a slotted disc); a
// short average smooths the pulses, and no pulse for a while means the spindle stopped. The temperature has a warning and a critical level with a hysteresis, so
// the state does not flicker around a limit. The node opens its protect output when the spindle is too hot or too fast.
#pragma once

#include <cstdint>
#include <deque>

#include "aux_config.hpp"

namespace bungard_aux {

class RpmMeter {
public:
    RpmMeter(std::uint32_t pulses_per_rev, std::uint32_t stale_ms) : ppr_(pulses_per_rev ? pulses_per_rev : 1), stale_us_(static_cast<std::uint64_t>(stale_ms) * 1000) {}

    /// A pulse at `t_us` (a clock that only goes up).
    void pulse(std::uint64_t t_us) {
        if (have_last_ && t_us > last_us_) {
            gaps_.push_back(t_us - last_us_);
            if (gaps_.size() > kWindow) gaps_.pop_front();
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

private:
    static constexpr std::size_t kWindow = 8;
    std::uint32_t ppr_;
    std::uint64_t stale_us_;
    std::deque<std::uint64_t> gaps_;
    std::uint64_t last_us_ = 0;
    bool have_last_ = false;
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
