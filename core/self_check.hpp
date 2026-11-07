// EH3D-NODE-BUNGARD-AUX - core/self_check.hpp
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
// The node judges its own state and says it with stable codes, so the server, the panel and a person at the bench read the same words. The judgement is a pure
// function of the status the node already builds and of its settings: it never touches the hardware, so it runs the same on a computer and on the chip. Every
// limit is a setting of head.cfg (selfcheck_*), never a number in the code. The codes are listed in docs/CONTRACT.md; a code is never reused for another meaning.
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "aux_config.hpp"
#include "protocol.hpp"

namespace bungard_aux {

enum class Severity { Info = 0, Warn = 1, Fail = 2 };

inline const char* severity_text(Severity s) {
    switch (s) {
        case Severity::Warn: return "warn";
        case Severity::Fail: return "fail";
        default: return "info";
    }
}

struct Finding {
    const char* code;      // AUX-xnn, stable
    Severity severity;
    const char* what;      // one line in English for the log and the bench
    const char* setting;   // the setting to look at (empty when it is the hardware)
};

/// Every code the node can raise, in the order the documents list them (the documentation test reads this table).
inline const std::vector<Finding>& self_check_catalog() {
    static const std::vector<Finding> all = {
        {"AUX-C01", Severity::Fail, "the camera is switched on but did not start", "camera_enabled"},
        {"AUX-L01", Severity::Info, "the laser has not given a reading since the start", "laser_enabled"},
        {"AUX-L02", Severity::Fail, "the laser sensor reports a fault", "laser_enabled"},
        {"AUX-L03", Severity::Warn, "the last laser reading is old", "selfcheck_laser_age_max_s"},
        {"AUX-L04", Severity::Warn, "the last laser reading had a weak signal or a high spread", "laser_sigma_max_mm"},
        {"AUX-L05", Severity::Info, "the calibration of the laser is allowed", "laser_calibration_enabled"},
        {"AUX-R01", Severity::Warn, "the pulses of the speed sensor are noisy", "rpm_input"},
        {"AUX-R02", Severity::Fail, "the spindle turns faster than rpm_max", "rpm_max"},
        {"AUX-T01", Severity::Fail, "the temperature sensor is on and gives no reading", "temp_sensor"},
        {"AUX-T02", Severity::Warn, "the spindle is warm", "temp_warn_c"},
        {"AUX-T03", Severity::Fail, "the spindle is hot", "temp_critical_c"},
        {"AUX-T04", Severity::Warn, "several temperature sensors on the wire and none chosen", "temp_rom"},
        {"AUX-P01", Severity::Warn, "the protect output is switched off in the settings", "protect_output"},
        {"AUX-P02", Severity::Warn, "the protect output is open: the spindle must not run", ""},
        {"AUX-S01", Severity::Warn, "the ring of LEDs is switched on in the settings but was not found", "light_enabled"},
        {"AUX-S02", Severity::Warn, "the node has no token: it refuses every command that changes something", "token"},
        {"AUX-S03", Severity::Info, "RTSP or ONVIF is on and has no password: nobody is served", "stream_password"},
    };
    return all;
}

inline const Finding* find_code(const std::string& code) {
    for (const Finding& f : self_check_catalog()) if (code == f.code) return &f;
    return nullptr;
}

struct Report {
    bool enabled = true;
    std::vector<const Finding*> findings;
    /// ok (nothing above info), warn or fail; off when the check is switched off in the settings.
    const char* overall() const {
        if (!enabled) return "off";
        Severity worst = Severity::Info;
        bool any_above_info = false;
        for (const Finding* f : findings) {
            if (f->severity != Severity::Info) any_above_info = true;
            if (static_cast<int>(f->severity) > static_cast<int>(worst)) worst = f->severity;
        }
        if (!any_above_info) return "ok";
        return worst == Severity::Fail ? "fail" : "warn";
    }
    /// The codes as the status carries them: separated by commas, empty when there are none.
    std::string codes() const {
        std::string out;
        for (const Finding* f : findings) out += (out.empty() ? "" : ",") + std::string(f->code);
        return out;
    }
    bool has(const char* code) const {
        for (const Finding* f : findings) if (std::string(f->code) == code) return true;
        return false;
    }
};

namespace detail {
inline void raise(Report& r, const char* code) {
    const Finding* f = find_code(code);
    if (f != nullptr) r.findings.push_back(f);
}
}  // namespace detail

/// Judges the status (as built by the firmware, or by the simulator) against the settings. A node that has just started gets `selfcheck_grace_s` seconds before an
/// absent reading counts as a fault, because the sensors take a moment to give the first one.
inline Report self_check(const Status& s, const Settings& cfg) {
    Report r;
    r.enabled = cfg.selfcheck_enabled;
    if (!r.enabled) return r;
    const bool grace = s.uptime_s < cfg.selfcheck_grace_s;

    if (cfg.camera_enabled && !s.camera && !grace) detail::raise(r, "AUX-C01");

    if (cfg.laser_enabled) {
        if (s.laser_age_ms < 0) {
            if (!grace) detail::raise(r, "AUX-L01");
        } else {
            if (s.laser_last_status == "sensor_fault") detail::raise(r, "AUX-L02");
            if (s.laser_age_ms > static_cast<long>(cfg.selfcheck_laser_age_max_s) * 1000L) detail::raise(r, "AUX-L03");
            if (s.laser_last_status == "signal_low" || s.laser_last_status == "sigma_high") detail::raise(r, "AUX-L04");
        }
        if (cfg.laser_calibration_enabled) detail::raise(r, "AUX-L05");
    }

    if (cfg.rpm_enabled) {
        if (s.rpm_quality == "noisy") detail::raise(r, "AUX-R01");
        if (s.rpm > static_cast<double>(cfg.rpm_max)) detail::raise(r, "AUX-R02");
    }

    if (cfg.temp_sensor != TempSensor::None) {
        if (!s.has_temp && !grace) detail::raise(r, "AUX-T01");
        if (s.heat == Heat::Warm) detail::raise(r, "AUX-T02");
        if (s.heat == Heat::Hot) detail::raise(r, "AUX-T03");
        if (cfg.temp_sensor == TempSensor::Ds18b20 && s.temp_guess) detail::raise(r, "AUX-T04");
    }

    if (!cfg.protect_output) detail::raise(r, "AUX-P01");
    else if (s.protect_open && !grace) detail::raise(r, "AUX-P02");

    if (cfg.light_enabled && !s.light && !grace) detail::raise(r, "AUX-S01");
    if (cfg.token.empty()) detail::raise(r, "AUX-S02");
    if ((cfg.rtsp_enabled || cfg.onvif_enabled) && cfg.stream_password.empty()) detail::raise(r, "AUX-S03");
    return r;
}

/// Judges the status and writes the answer into it (`selfcheck`, `selfcheck_codes`), which is how the firmware and the simulator say it.
inline void apply_self_check(Status& s, const Settings& cfg) {
    const Report r = self_check(s, cfg);
    s.selfcheck = r.overall();
    s.selfcheck_codes = r.codes();
}

}  // namespace bungard_aux
