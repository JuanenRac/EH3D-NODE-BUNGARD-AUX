// EH3D-NODE-BUNGARD-AUX - core/head_sim.hpp
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
// A simulated drill head: a table that is not flat under a laser, a spindle that speeds up and heats, a ring of LEDs, a camera. It builds the same Status and the same
// answers to a measure as the firmware does, through the same code (core/protocol.hpp, core/laser.hpp, core/spindle.hpp, core/self_check.hpp), so the programs of the
// machine and the tests can talk to a head that is not there. Faults can be injected to see what the self-check says. Nothing here has run on a board; every number
// of the table, of the noise and of the heating is a made-up one of the simulation, not a measurement of any real machine.
#pragma once

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "aux_config.hpp"
#include "laser.hpp"
#include "protocol.hpp"
#include "self_check.hpp"
#include "spindle.hpp"

namespace bungard_aux {

/// What the simulated world is like (the table, the noise, the heating). These are the numbers of the simulation, not settings of the node.
struct SimWorld {
    double table_distance_mm = 120.0;   // the distance from the sensor to the table at the origin
    double tilt_x_mm_per_m = 0.8;       // the table gets farther by this much per metre in X (a table that is not flat)
    double tilt_y_mm_per_m = -0.3;      // and in Y
    double noise_mm = 0.02;             // the spread of one reading
    double spindle_accel_rpm_s = 8000.0;
    double ambient_c = 22.0;
    double heat_c_per_s_at_max = 0.05;  // heating at rpm_max, in degrees per second
    double cool_per_s = 0.002;          // the part of the difference to the ambient lost each second
};

/// The faults that can be injected.
struct SimFaults {
    bool camera_down = false;
    bool laser_fault = false;       // the sensor answers with the range status 5
    bool laser_weak = false;        // the sensor answers with the range status 2 (weak signal)
    bool rpm_noise = false;         // the pulses of the speed sensor are noisy
    bool temp_missing = false;      // the temperature sensor gives nothing
    bool light_missing = false;     // the ring of LEDs was not found
};

class HeadSim {
public:
    explicit HeadSim(const Settings& cfg, const SimWorld& world = SimWorld{}, std::uint32_t seed = 12345u) : cfg_(cfg), world_(world), rng_(seed ? seed : 1u), temp_c_(world.ambient_c) {}

    Settings& settings() { return cfg_; }
    const Settings& settings() const { return cfg_; }
    SimFaults& faults() { return faults_; }

    /// Moves the head over the table (millimetres from the origin).
    void move_to(double x_mm, double y_mm) { x_ = x_mm; y_ = y_mm; }
    void set_spindle_target(double rpm) { target_rpm_ = rpm < 0.0 ? 0.0 : rpm; }
    void set_light(bool on, std::uint32_t level) { light_on_ = on; light_level_ = level; }

    /// Lets `ms` milliseconds go by: the spindle speeds up or slows down, heats or cools, and the last laser reading ages.
    void advance(std::uint32_t ms) {
        const double dt = ms / 1000.0;
        const double step = world_.spindle_accel_rpm_s * dt;
        if (rpm_ < target_rpm_) rpm_ = rpm_ + step > target_rpm_ ? target_rpm_ : rpm_ + step;
        else rpm_ = rpm_ - step < target_rpm_ ? target_rpm_ : rpm_ - step;
        const double max = static_cast<double>(cfg_.rpm_max);
        temp_c_ += world_.heat_c_per_s_at_max * (rpm_ / max) * dt - world_.cool_per_s * (temp_c_ - world_.ambient_c) * dt;
        now_ms_ += ms;
        if (last_laser_ms_ >= 0) laser_age_ms_ += ms;
        // the thermal state machine sees every step, as the task of the temperature does
        if (cfg_.temp_sensor != TempSensor::None) thermal_.update(temp_c_, !faults_.temp_missing);
    }

    /// The true distance from the sensor to the table under the head.
    double table_distance_at(double x_mm, double y_mm) const {
        return world_.table_distance_mm + world_.tilt_x_mm_per_m * x_mm / 1000.0 + world_.tilt_y_mm_per_m * y_mm / 1000.0;
    }

    /// One measure as POST /api/measure answers it: `samples` readings of the table where the head is, through the same filter as the firmware.
    Measure measure(int samples) {
        std::vector<LaserReading> r;
        const double truth = table_distance_at(x_, y_);
        for (int i = 0; i < samples; ++i) {
            LaserReading one;
            one.distance_mm = truth + noise() * world_.noise_mm;
            one.status = faults_.laser_fault ? 5 : faults_.laser_weak ? 2 : 0;
            one.valid = one.status == 0;
            r.push_back(one);
        }
        LaserLimits lim;
        lim.range_min_mm = cfg_.laser_range_min_mm;
        lim.range_max_mm = cfg_.laser_range_max_mm;
        lim.sigma_max_mm = cfg_.laser_sigma_max_mm;
        lim.offset_mm = cfg_.laser_offset_mm;
        lim.cal_offset_mm = cfg_.laser_cal_offset_mm;
        lim.outlier_k = cfg_.laser_outlier_k;
        lim.outlier_floor_mm = cfg_.laser_outlier_floor_mm;
        const Measure m = measure_distance(r, samples, lim);
        last_laser_mm_ = r.empty() ? 0.0 : r.back().distance_mm + cfg_.laser_offset_mm + cfg_.laser_cal_offset_mm;
        last_laser_status_ = range_why_text(range_why(r.empty() ? 255 : r.back().status));
        last_laser_ms_ = static_cast<long>(now_ms_);
        laser_age_ms_ = 0;
        return m;
    }

    /// The status as GET /api/status answers it.
    Status status() const {
        Status s;
        s.name = cfg_.name;
        s.version = "sim";
        s.camera = cfg_.camera_enabled && !faults_.camera_down;
        if (s.camera) s.camera_model = "ov2640";
        s.laser = cfg_.laser_enabled;
        s.rpm_on = cfg_.rpm_enabled;
        s.rpm = cfg_.rpm_enabled ? rpm_ : 0.0;
        s.rpm_input = rpm_input_text(cfg_.rpm_input);
        s.rpm_quality = !cfg_.rpm_enabled || rpm_ <= 0.0 ? "none" : faults_.rpm_noise ? "noisy" : "good";
        if (cfg_.temp_sensor != TempSensor::None) {
            s.has_temp = !faults_.temp_missing;
            s.temp_c = temp_c_;
            s.heat = thermal_.state();
        }
        s.protect_open = protect_open(cfg_, s.rpm, s.heat);
        protect_log_.update(protect_reason(cfg_, s.rpm, s.heat), static_cast<long>(now_ms_ / 1000));
        s.protect_reason = protect_reason_text(protect_log_.now);
        s.protect_trips = protect_log_.trips;
        s.protect_last = protect_reason_text(protect_log_.last);
        s.protect_last_s = protect_log_.last_s;
        s.uptime_s = static_cast<std::uint32_t>(now_ms_ / 1000);
        s.light = cfg_.light_enabled && !faults_.light_missing;
        s.light_on = s.light && light_on_;
        s.light_level = s.light_on ? light_level_ : 0;
        s.laser_mode = distance_mode_text(cfg_.laser_distance_mode);
        s.laser_budget_ms = static_cast<int>(cfg_.laser_timing_budget_ms);
        s.laser_roi = roi_text(cfg_.laser_roi_w, cfg_.laser_roi_h);
        s.laser_cal_offset_mm = cfg_.laser_cal_offset_mm;
        s.laser_cal_xtalk_cps = cfg_.laser_cal_xtalk_cps;
        s.laser_cal_allowed = cfg_.laser_calibration_enabled;
        if (cfg_.laser_enabled && last_laser_ms_ >= 0) {
            s.laser_last_mm = last_laser_mm_;
            s.laser_last_status = last_laser_status_;
            s.laser_age_ms = static_cast<long>(laser_age_ms_);
        }
        apply_self_check(s, cfg_);
        return s;
    }

    /// Answers one request line (`GET /api/status`, `POST /api/measure?samples=5`) with the body the firmware would send, or an empty text for a route the simulator
    /// does not play. The access token is not checked: the simulator is for the programs that read the answers.
    std::string answer(const std::string& request_line) {
        Request req;
        // `GET /api/status` is enough: the version of HTTP that a real request carries is added when it is missing
        std::string line = request_line;
        const std::size_t first_space = line.find(' ');
        if (first_space != std::string::npos && line.find(' ', first_space + 1) == std::string::npos) line += " HTTP/1.1";
        if (!parse_request_line(line, req)) return "";
        switch (route_of(req)) {
            case Route::Status: return json_status(status());
            case Route::Measure: {
                std::uint32_t n = cfg_.laser_samples;
                const std::string q = query_value(req.query, "samples");
                if (!q.empty() && detail::to_uint(q, 1, 31, n)) {}
                return json_measure(measure(static_cast<int>(n)));
            }
            default: return "";
        }
    }

private:
    // a number between -1 and 1 that is the same for the same seed on every machine (no library generator whose sequence differs between platforms)
    double noise() {
        rng_ ^= rng_ << 13;
        rng_ ^= rng_ >> 17;
        rng_ ^= rng_ << 5;
        return static_cast<double>(rng_ % 20001u) / 10000.0 - 1.0;
    }

    Settings cfg_;
    SimWorld world_;
    SimFaults faults_;
    std::uint32_t rng_;
    Thermal thermal_{cfg_.temp_warn_c, cfg_.temp_critical_c, cfg_.temp_hysteresis_c};
    mutable ProtectLog protect_log_;   // what the status tells about the protect output; a look at the status is what updates it
    double x_ = 0.0, y_ = 0.0;
    double rpm_ = 0.0, target_rpm_ = 0.0;
    double temp_c_;
    bool light_on_ = false;
    std::uint32_t light_level_ = 0;
    std::uint64_t now_ms_ = 0;
    long last_laser_ms_ = -1;
    std::uint64_t laser_age_ms_ = 0;
    double last_laser_mm_ = 0.0;
    std::string last_laser_status_ = "none";
};

}  // namespace bungard_aux
