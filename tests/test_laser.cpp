// EH3D-NODE-BUNGARD-AUX host tests of the laser: the distance mode, the timing budget and the area of the sensor as a list of registers, the range status, the odd readings, the
// calibration at a known distance, and the answers of the node. The sensor is simulated: readings with noise, spikes and bad statuses from a small generator.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "../core/aux_config.hpp"
#include "../core/laser.hpp"
#include "../core/laser_hw.hpp"
#include "../core/protocol.hpp"

using namespace bungard_aux;

static int failures = 0;
static int checks = 0;

#define CHECK(cond)                                                     \
    do {                                                                \
        ++checks;                                                       \
        if (!(cond)) {                                                  \
            ++failures;                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
        }                                                               \
    } while (0)

static bool near(double a, double b, double eps = 1e-6) { return std::fabs(a - b) <= eps; }

/// A simulated VL53L1X: noise of about `sigma` mm around `truth`, a spike now and then, a few readings the sensor itself refuses.
struct SimSensor {
    std::uint32_t state = 12345;
    double truth = 100.0;
    double sigma = 0.05;
    int spike_every = 0;        // every Nth reading is far off (0: never)
    int refuse_every = 0;       // every Nth reading carries a bad status
    std::uint8_t refuse_status = 7;
    double signal_kcps = 1500.0;
    double spads = 200.0;
    int count = 0;

    double uniform() {
        state = state * 1664525u + 1013904223u;
        return static_cast<double>(state >> 8) / 16777216.0;
    }
    double noise() {  // sum of four uniforms: close enough to a bell
        return (uniform() + uniform() + uniform() + uniform() - 2.0) * sigma * 1.7;
    }
    LaserReading next() {
        ++count;
        LaserReading r;
        r.distance_mm = truth + noise();
        r.signal_kcps = signal_kcps;
        r.spads = spads;
        r.sigma_mm = sigma;
        if (spike_every && count % spike_every == 0) r.distance_mm += 40.0;
        if (refuse_every && count % refuse_every == 0) {
            r.valid = false;
            r.status = refuse_status;
        }
        return r;
    }
    std::vector<LaserReading> take(int n) {
        std::vector<LaserReading> v;
        for (int i = 0; i < n; ++i) v.push_back(next());
        return v;
    }
};

static bool has_write(const std::vector<RegWrite>& list, std::uint16_t reg, std::uint8_t bytes, std::uint16_t value) {
    for (const RegWrite& w : list) {
        if (w.reg == reg && w.bytes == bytes && w.value == value) return true;
    }
    return false;
}

static void budgets_and_modes() {
    CHECK(std::string(distance_mode_text(DistanceMode::Short)) == "short" && std::string(distance_mode_text(DistanceMode::Long)) == "long");
    DistanceMode m = DistanceMode::Long;
    CHECK(distance_mode_from_text("short", m) && m == DistanceMode::Short);
    CHECK(!distance_mode_from_text("medium", m) && !distance_mode_from_text("", m) && m == DistanceMode::Short);
    // 15 ms exists in the short mode only; both have the others
    CHECK(budget_valid(DistanceMode::Short, 15) && !budget_valid(DistanceMode::Long, 15));
    for (int ms : {20, 33, 50, 100, 200, 500}) CHECK(budget_valid(DistanceMode::Short, ms) && budget_valid(DistanceMode::Long, ms));
    CHECK(!budget_valid(DistanceMode::Long, 0) && !budget_valid(DistanceMode::Long, 75) && !budget_valid(DistanceMode::Short, 1000));
    // the timeouts of a few budgets, as ST's driver has them
    CHECK(find_budget(DistanceMode::Long, 100)->a == 0x01CC && find_budget(DistanceMode::Long, 100)->b == 0x01EA);
    CHECK(find_budget(DistanceMode::Short, 50)->a == 0x01AE && find_budget(DistanceMode::Short, 50)->b == 0x01E8);
    CHECK(find_budget(DistanceMode::Short, 15)->a == 0x001D && find_budget(DistanceMode::Long, 500)->b == 0x04A4);
    // the longer the budget, the longer the timeouts, in both modes
    for (DistanceMode mode : {DistanceMode::Short, DistanceMode::Long}) {
        const std::vector<Budget>& list = budgets(mode);
        for (std::size_t i = 1; i < list.size(); ++i) CHECK(list[i].ms > list[i - 1].ms && list[i].a > list[i - 1].a && list[i].b >= list[i - 1].b);
    }
    // the time between measurements is never shorter than the budget
    for (int ms : {15, 20, 33, 50, 100, 200, 500}) CHECK(inter_measurement_ms(ms) >= ms && inter_measurement_ms(ms) >= 20);
}

static void area() {
    int w = 0, h = 0;
    CHECK(parse_roi("16x16", w, h) && w == 16 && h == 16);
    CHECK(parse_roi("4x9", w, h) && w == 4 && h == 9);
    CHECK(parse_roi("10x12", w, h) && w == 10 && h == 12);
    CHECK(!parse_roi("3x16", w, h) && !parse_roi("17x8", w, h) && !parse_roi("8", w, h) && !parse_roi("x8", w, h) && !parse_roi("8x", w, h) && !parse_roi("8X8", w, h));
    CHECK(!parse_roi("8x-8", w, h) && !parse_roi("", w, h) && !parse_roi("008x008", w, h) && !parse_roi("8 x8", w, h));
    CHECK(roi_text(8, 6) == "8x6");
}

static void registers() {
    LaserHw hw;
    std::vector<RegWrite> r = registers_for(hw);   // long, 100 ms, 16x16
    CHECK(has_write(r, 0x0060, 1, 0x0F) && has_write(r, 0x0063, 1, 0x0D) && has_write(r, 0x0069, 1, 0xB8) && has_write(r, 0x0078, 2, 0x0F0D) && has_write(r, 0x007A, 2, 0x0E0E));
    CHECK(has_write(r, 0x004B, 1, 0x0A));
    CHECK(has_write(r, 0x005E, 2, 0x01CC) && has_write(r, 0x0061, 2, 0x01EA));
    CHECK(has_write(r, 0x007F, 1, 199) && has_write(r, 0x0080, 1, 0xFF));
    CHECK(!has_write(r, 0x0016, 2, 0));   // the crosstalk is left alone unless asked
    for (const RegWrite& w : r) CHECK(w.reg != 0x0016 && w.reg != 0x0018 && w.reg != 0x001A);

    hw.mode = DistanceMode::Short;
    hw.budget_ms = 15;
    r = registers_for(hw);
    CHECK(has_write(r, 0x0060, 1, 0x07) && has_write(r, 0x0063, 1, 0x05) && has_write(r, 0x0069, 1, 0x38) && has_write(r, 0x0078, 2, 0x0705) && has_write(r, 0x007A, 2, 0x0606));
    CHECK(has_write(r, 0x005E, 2, 0x001D) && has_write(r, 0x0061, 2, 0x0027) && has_write(r, 0x004B, 1, 0x14));

    // a budget the mode does not have gives no list at all, so a wrong setting never reaches the sensor half applied
    hw.mode = DistanceMode::Long;
    hw.budget_ms = 15;
    CHECK(registers_for(hw).empty());

    // the area: a small one is centred on the optical centre the sensor reports, or on the detector the settings name; a big one is always centred
    hw = LaserHw{};
    hw.roi_w = 4;
    hw.roi_h = 4;
    hw.optical_center = 167;
    r = registers_for(hw);
    CHECK(has_write(r, 0x007F, 1, 167) && has_write(r, 0x0080, 1, 0x33));
    hw.roi_center = 90;
    r = registers_for(hw);
    CHECK(has_write(r, 0x007F, 1, 90));
    hw.roi_w = 12;
    hw.roi_h = 6;
    r = registers_for(hw);
    CHECK(has_write(r, 0x007F, 1, 199) && has_write(r, 0x0080, 1, static_cast<std::uint16_t>((5 << 4) | 11)));
    hw.roi_w = 99;   // out of range is brought back inside
    hw.roi_h = 1;
    r = registers_for(hw);
    CHECK(has_write(r, 0x0080, 1, static_cast<std::uint16_t>((3 << 4) | 15)));

    // the crosstalk: counts per second to the 7.9 format of kilo-counts
    hw = LaserHw{};
    hw.write_xtalk = true;
    hw.xtalk_cps = 5000.0;
    r = registers_for(hw);
    CHECK(has_write(r, 0x0016, 2, 2560) && has_write(r, 0x0018, 2, 0) && has_write(r, 0x001A, 2, 0));
    hw.xtalk_cps = 0.0;
    CHECK(has_write(registers_for(hw), 0x0016, 2, 0));
    hw.xtalk_cps = 1e9;
    CHECK(has_write(registers_for(hw), 0x0016, 2, 65535));
    hw.xtalk_cps = -5.0;
    CHECK(has_write(registers_for(hw), 0x0016, 2, 0));
}

static void settings_of_the_laser() {
    std::vector<Problem> p;
    Settings d = parse_settings("", p);
    CHECK(p.empty() && d.laser_distance_mode == DistanceMode::Long && d.laser_timing_budget_ms == 100 && d.laser_roi_w == 16 && d.laser_roi_h == 16 && d.laser_roi_center == 0);
    CHECK(!d.laser_calibration_enabled && near(d.laser_cal_offset_mm, 0.0) && near(d.laser_cal_xtalk_cps, 0.0) && near(d.laser_outlier_k, 3.0) && near(d.laser_outlier_floor_mm, 0.05));
    // the defaults of the settings are the registers the node used before they were settings: long mode, 100 ms, the whole array
    LaserHw hw;
    CHECK(hw.mode == d.laser_distance_mode && hw.budget_ms == static_cast<int>(d.laser_timing_budget_ms) && hw.roi_w == d.laser_roi_w && hw.roi_h == d.laser_roi_h);

    p.clear();
    Settings s = parse_settings("laser_distance_mode = short\nlaser_timing_budget_ms = 20\nlaser_roi = 6x8\nlaser_roi_center = 100\nlaser_outlier_k = 2.5\nlaser_outlier_floor_mm = 0.1\n"
                                "laser_calibration_enabled = on\nlaser_cal_offset_mm = -2.5\nlaser_cal_xtalk_cps = 3000\n", p);
    CHECK(p.empty() && s.laser_distance_mode == DistanceMode::Short && s.laser_timing_budget_ms == 20 && s.laser_roi_w == 6 && s.laser_roi_h == 8 && s.laser_roi_center == 100);
    CHECK(near(s.laser_outlier_k, 2.5) && near(s.laser_outlier_floor_mm, 0.1) && s.laser_calibration_enabled && near(s.laser_cal_offset_mm, -2.5) && near(s.laser_cal_xtalk_cps, 3000.0));

    // wrong values are reported one by one and the default stays
    p.clear();
    s = parse_settings("laser_distance_mode = medium\nlaser_roi = 2x2\nlaser_roi_center = 300\nlaser_outlier_k = 0\nlaser_outlier_floor_mm = 5\nlaser_cal_offset_mm = 80\nlaser_cal_xtalk_cps = -1\n"
                       "laser_calibration_enabled = maybe\n", p);
    CHECK(p.size() == 8 && s.laser_distance_mode == DistanceMode::Long && s.laser_roi_w == 16 && s.laser_roi_center == 0 && near(s.laser_outlier_k, 3.0));
    CHECK(!s.laser_calibration_enabled && near(s.laser_cal_offset_mm, 0.0) && near(s.laser_cal_xtalk_cps, 0.0));

    // 15 ms belongs to the short mode: with the long one it is refused and the default budget is used
    p.clear();
    s = parse_settings("laser_timing_budget_ms = 15\n", p);
    CHECK(p.size() == 1 && p[0].key == "laser_timing_budget_ms" && s.laser_timing_budget_ms == 100);
    p.clear();
    s = parse_settings("laser_distance_mode = short\nlaser_timing_budget_ms = 15\n", p);
    CHECK(p.empty() && s.laser_timing_budget_ms == 15);
    // a budget that is nobody's
    p.clear();
    s = parse_settings("laser_timing_budget_ms = 75\n", p);
    CHECK(p.size() == 1 && s.laser_timing_budget_ms == 100);
    // changing only the mode on top of a stored 15 ms budget in the short mode: the long mode refuses that combination
    p.clear();
    Settings base = parse_settings("laser_distance_mode = short\nlaser_timing_budget_ms = 15\n", p);
    p.clear();
    s = parse_settings_over(base, "laser_distance_mode = long\n", p);
    CHECK(p.size() == 1 && s.laser_timing_budget_ms == 100 && s.laser_distance_mode == DistanceMode::Long);
}

static void range_status() {
    CHECK(range_why(0) == RangeWhy::Valid);
    CHECK(range_why(1) == RangeWhy::SigmaHigh);
    CHECK(range_why(2) == RangeWhy::SignalLow && range_why(12) == RangeWhy::SignalLow && range_why(9) == RangeWhy::SignalLow);
    CHECK(range_why(4) == RangeWhy::OutOfRange && range_why(7) == RangeWhy::OutOfRange && range_why(3) == RangeWhy::OutOfRange && range_why(13) == RangeWhy::OutOfRange);
    CHECK(range_why(5) == RangeWhy::SensorFault);
    CHECK(range_why(255) == RangeWhy::Other && range_why(10) == RangeWhy::Other && range_why(11) == RangeWhy::Other);
    CHECK(std::string(range_why_text(RangeWhy::SigmaHigh)) == "sigma_high" && std::string(range_why_text(RangeWhy::SignalLow)) == "signal_low");
    CHECK(std::string(range_why_text(RangeWhy::OutOfRange)) == "out_of_range" && std::string(range_why_text(RangeWhy::Valid)) == "valid");
    CHECK(std::string(range_why_text(RangeWhy::SensorFault)) == "sensor_fault" && std::string(range_why_text(RangeWhy::Other)) == "other");

    // a measure says why the sensor refused what it refused: the commonest reason wins
    LaserLimits lim;
    std::vector<LaserReading> r;
    for (int i = 0; i < 4; ++i) r.push_back({100.0, true});
    for (int i = 0; i < 2; ++i) { LaserReading x{100.0, false}; x.status = 2; r.push_back(x); }
    { LaserReading x{100.0, false}; x.status = 1; r.push_back(x); }
    Measure m = measure_distance(r, 7, lim);
    CHECK(m.ok && m.refused == 3 && m.status_code == 2 && m.status_why == RangeWhy::SignalLow);
    // nothing refused: the status is valid
    r.clear();
    for (int i = 0; i < 5; ++i) r.push_back({100.0, true});
    m = measure_distance(r, 5, lim);
    CHECK(m.ok && m.refused == 0 && m.status_why == RangeWhy::Valid && m.status_code == 0);
    // every reading refused for a weak signal: no measure, and the reason is the sensor's
    r.clear();
    for (int i = 0; i < 7; ++i) { LaserReading x{0.0, false}; x.status = 2; r.push_back(x); }
    m = measure_distance(r, 7, lim);
    CHECK(!m.ok && m.why == MeasureWhy::TooFew && m.status_why == RangeWhy::SignalLow && m.refused == 7);
    // a refusal without a known status counts as "other"
    r.clear();
    for (int i = 0; i < 3; ++i) r.push_back({100.0, false});
    m = measure_distance(r, 3, lim);
    CHECK(!m.ok && m.status_why == RangeWhy::Other && m.status_code == 255);
    // the sigma of the sensor too high on every reading
    r.clear();
    for (int i = 0; i < 6; ++i) { LaserReading x{0.0, false}; x.status = 1; r.push_back(x); }
    m = measure_distance(r, 6, lim);
    CHECK(!m.ok && m.status_why == RangeWhy::SigmaHigh);
}

static void odd_readings() {
    LaserLimits lim;
    // a sensor with noise and a spike every seventh reading: the spikes are thrown away, the distance is the truth
    SimSensor s;
    s.truth = 120.0;
    s.sigma = 0.04;
    s.spike_every = 7;
    Measure m = measure_distance(s.take(21), 21, lim);
    CHECK(m.ok && std::fabs(m.distance_mm - 120.0) < 0.05 && m.rejected >= 3 && m.sigma_mm < 0.1);
    // readings the sensor wraps around (status 7) and refuses do not move the distance
    SimSensor w;
    w.truth = 80.0;
    w.sigma = 0.04;
    w.refuse_every = 3;
    m = measure_distance(w.take(21), 21, lim);
    CHECK(m.ok && std::fabs(m.distance_mm - 80.0) < 0.05 && m.refused == 7 && m.status_why == RangeWhy::OutOfRange && m.status_code == 7);
    // the same stream over many measures: each one is close to the truth (the filter is not luck)
    SimSensor many;
    many.truth = 55.5;
    many.sigma = 0.05;
    many.spike_every = 5;
    int good = 0;
    for (int i = 0; i < 40; ++i) {
        Measure x = measure_distance(many.take(9), 9, lim);
        if (x.ok && std::fabs(x.distance_mm - 55.5) < 0.12) ++good;   // never a spike (+40 mm), always within a couple of spreads
    }
    CHECK(good == 40);
    // the factor of the filter is a setting: a tight one drops more readings than a loose one
    SimSensor a, b;
    a.sigma = b.sigma = 0.3;
    a.truth = b.truth = 100.0;
    LaserLimits loose = lim, tight = lim;
    loose.outlier_k = 10.0;
    loose.sigma_max_mm = 5.0;
    tight.outlier_k = 1.0;
    tight.sigma_max_mm = 5.0;
    Measure ml = measure_distance(a.take(31), 31, loose), mt = measure_distance(b.take(31), 31, tight);
    CHECK(ml.rejected < mt.rejected);
    // the floor: with identical readings the spread is zero, and a reading 0.04 mm away is not odd
    LaserLimits floor = lim;
    floor.outlier_floor_mm = 0.05;
    m = measure_distance({{100.0, true}, {100.0, true}, {100.0, true}, {100.04, true}, {100.0, true}}, 5, floor);
    CHECK(m.ok && m.rejected == 0);
    floor.outlier_floor_mm = 0.01;
    m = measure_distance({{100.0, true}, {100.0, true}, {100.0, true}, {100.04, true}, {100.0, true}}, 5, floor);
    CHECK(m.ok && m.rejected == 1);
    // the two offsets add: the one of the settings and the one the calibration found
    LaserLimits off = lim;
    off.offset_mm = 1.5;
    off.cal_offset_mm = -0.5;
    m = measure_distance({{100.0, true}, {100.0, true}, {100.0, true}}, 3, off);
    CHECK(m.ok && near(m.distance_mm, 101.0));
}

static void calibration_offset() {
    LaserLimits lim;
    // the table is really at 100 mm, the sensor reads 98.3: the offset is +1.7, and with it the measure gives 100
    SimSensor s;
    s.truth = 98.3;
    s.sigma = 0.04;
    s.spike_every = 11;
    std::vector<LaserReading> r = s.take(50);
    Calibration c = calibrate_offset(r, 50, 100.0, lim);
    CHECK(c.ok && std::fabs(c.value - 1.7) < 0.05 && c.used >= 40 && std::string(c.why) == "ok");
    LaserLimits calibrated = lim;
    calibrated.cal_offset_mm = c.value;
    Measure m = measure_distance(s.take(9), 9, calibrated);
    CHECK(m.ok && std::fabs(m.distance_mm - 100.0) < 0.06);
    // the offset of the settings and an earlier calibration are not in the readings it is given: it does not count them twice
    LaserLimits both = lim;
    both.offset_mm = 7.0;
    both.cal_offset_mm = 9.0;
    Calibration again = calibrate_offset(r, 50, 100.0, both);
    CHECK(again.ok && near(again.value, c.value));
    // the other way round: the sensor reads long
    SimSensor far_sensor;
    far_sensor.truth = 102.2;
    far_sensor.sigma = 0.04;
    c = calibrate_offset(far_sensor.take(50), 50, 100.0, lim);
    CHECK(c.ok && std::fabs(c.value + 2.2) < 0.05);
    // a target that is not where it was said to be (more than 50 mm off) is refused
    SimSensor wrong;
    wrong.truth = 200.0;
    c = calibrate_offset(wrong.take(50), 50, 100.0, lim);
    CHECK(!c.ok && std::string(c.why) == "offset_limit");
    // a noisy sensor is refused
    SimSensor noisy;
    noisy.truth = 100.0;
    noisy.sigma = 3.0;
    c = calibrate_offset(noisy.take(50), 50, 100.0, lim);
    CHECK(!c.ok && std::string(c.why) == "too_noisy");
    // nothing, too few readings and readings outside the range
    c = calibrate_offset({}, 50, 100.0, lim);
    CHECK(!c.ok && std::string(c.why) == "no_readings");
    SimSensor few;
    few.truth = 100.0;
    c = calibrate_offset(few.take(5), 50, 100.0, lim);
    CHECK(!c.ok && std::string(c.why) == "too_few");
    SimSensor out;
    out.truth = 900.0;
    c = calibrate_offset(out.take(50), 50, 900.0, lim);
    CHECK(!c.ok && std::string(c.why) == "out_of_range");
    // a target distance that makes no sense
    c = calibrate_offset(s.take(50), 50, 0.0, lim);
    CHECK(!c.ok && std::string(c.why) == "bad_target");
    c = calibrate_offset(s.take(50), 50, 9000.0, lim);
    CHECK(!c.ok && std::string(c.why) == "bad_target");
    c = calibrate_offset(s.take(50), 50, std::nan(""), lim);
    CHECK(!c.ok && std::string(c.why) == "bad_target");
    // readings the sensor refused do not count towards the offset
    SimSensor refuse;
    refuse.truth = 99.0;
    refuse.sigma = 0.04;
    refuse.refuse_every = 2;
    c = calibrate_offset(refuse.take(60), 50, 100.0, lim);
    CHECK(c.ok && std::fabs(c.value - 1.0) < 0.05);
}

static void calibration_crosstalk() {
    // the formula of ST: 1000 * signal * (1 - distance / target) / spads, in counts per second
    SimSensor s;
    s.truth = 95.0;
    s.sigma = 0.0;
    s.signal_kcps = 2000.0;
    s.spads = 200.0;
    Calibration c = calibrate_xtalk(s.take(50), 100.0);
    CHECK(c.ok && std::fabs(c.value - 500.0) < 1.0 && c.used == 50);
    // reading farther than the target: nothing to compensate, zero
    SimSensor far_sensor;
    far_sensor.truth = 104.0;
    far_sensor.sigma = 0.0;
    c = calibrate_xtalk(far_sensor.take(50), 100.0);
    CHECK(c.ok && near(c.value, 0.0));
    // no detectors or no signal reported
    SimSensor none;
    none.spads = 0.0;
    c = calibrate_xtalk(none.take(50), 100.0);
    CHECK(!c.ok && std::string(c.why) == "no_signal");
    SimSensor dark;
    dark.signal_kcps = 0.0;
    c = calibrate_xtalk(dark.take(50), 100.0);
    CHECK(!c.ok && std::string(c.why) == "no_signal");
    // too few good readings, none at all, a bad target
    c = calibrate_xtalk(s.take(4), 100.0);
    CHECK(!c.ok && std::string(c.why) == "too_few");
    c = calibrate_xtalk({}, 100.0);
    CHECK(!c.ok && std::string(c.why) == "no_readings");
    c = calibrate_xtalk(s.take(50), 5.0);
    CHECK(!c.ok && std::string(c.why) == "bad_target");
    // an enormous crosstalk is not a calibration: the sensor is looking at something else
    SimSensor huge;
    huge.truth = 10.0;
    huge.sigma = 0.0;
    huge.signal_kcps = 60000.0;
    huge.spads = 10.0;
    c = calibrate_xtalk(huge.take(50), 100.0);
    CHECK(!c.ok && std::string(c.why) == "out_of_range");
    // readings refused by the sensor do not count
    SimSensor refused;
    refused.truth = 95.0;
    refused.sigma = 0.0;
    refused.refuse_every = 1;
    c = calibrate_xtalk(refused.take(50), 100.0);
    CHECK(!c.ok && std::string(c.why) == "too_few");
    // what is found goes back into the register list and comes out as the 7.9 format
    LaserHw hw;
    hw.write_xtalk = true;
    hw.xtalk_cps = 500.0;
    CHECK(has_write(registers_for(hw), 0x0016, 2, 256));
}

static void answers() {
    Request r;
    CHECK(parse_request_line("POST /api/laser/calibrate?kind=offset&mm=100 HTTP/1.1", r));
    CHECK(route_of(r) == Route::Calibrate);
    Request get = r;
    get.method = "GET";
    CHECK(route_of(get) == Route::BadMethod);

    CalibrateRequest cr;
    CHECK(parse_calibrate(r, cr) && cr.kind == "offset" && near(cr.target_mm, 100.0));
    Request xt;
    xt.method = "POST";
    xt.path = "/api/laser/calibrate";
    xt.query = "kind=xtalk&mm=600&token=abc";
    CHECK(parse_calibrate(xt, cr) && cr.kind == "xtalk" && near(cr.target_mm, 600.0));
    xt.query = "mm=140";   // no kind: the offset
    CHECK(parse_calibrate(xt, cr) && cr.kind == "offset");
    xt.query = "kind=reset";
    CHECK(parse_calibrate(xt, cr) && cr.kind == "reset");
    xt.query = "kind=offset";
    CHECK(!parse_calibrate(xt, cr));   // no distance
    xt.query = "kind=offset&mm=5";
    CHECK(!parse_calibrate(xt, cr));
    xt.query = "kind=offset&mm=abc";
    CHECK(!parse_calibrate(xt, cr));
    xt.query = "kind=format&mm=100";
    CHECK(!parse_calibrate(xt, cr));

    // calibrating needs the token, like measuring
    Settings s;
    CHECK(!authorized(s, Route::Calibrate, "anything", r));
    s.token = "token-12345";
    CHECK(authorized(s, Route::Calibrate, "token-12345", r) && !authorized(s, Route::Calibrate, "wrong", r));

    std::string j = json_calibration("offset", true, "ok", 1.7, 250.0, 48, 0.04);
    CHECK(j.find("\"ok\":true") != std::string::npos && j.find("\"kind\":\"offset\"") != std::string::npos && j.find("\"cal_offset_mm\":1.700") != std::string::npos);
    CHECK(j.find("\"cal_xtalk_cps\":250") != std::string::npos && j.find("\"used\":48") != std::string::npos);
    j = json_calibration("xt\"alk", false, "too_noisy", 0.0, 0.0, 12, 3.0);
    CHECK(j.find("\"ok\":false") != std::string::npos && j.find("\"why\":\"too_noisy\"") != std::string::npos && j.find("xt\\\"alk") != std::string::npos);

    // the measure says why the sensor refused readings
    std::vector<LaserReading> bad;
    for (int i = 0; i < 5; ++i) { LaserReading x{0.0, false}; x.status = 1; bad.push_back(x); }
    std::string mj = json_measure(measure_distance(bad, 5, LaserLimits{}));
    CHECK(mj.find("\"ok\":false") != std::string::npos && mj.find("\"status\":\"sigma_high\"") != std::string::npos && mj.find("\"status_code\":1") != std::string::npos);
    CHECK(mj.find("\"refused\":5") != std::string::npos);
    mj = json_measure(measure_distance({{100.0, true}, {100.0, true}, {100.0, true}}, 3, LaserLimits{}));
    CHECK(mj.find("\"status\":\"valid\"") != std::string::npos && mj.find("\"refused\":0") != std::string::npos);

    // the status carries how the laser is set up and the last reading
    Status st;
    st.laser = true;
    st.laser_mode = "short";
    st.laser_budget_ms = 33;
    st.laser_roi = "8x8";
    st.laser_cal_offset_mm = 1.25;
    st.laser_cal_xtalk_cps = 300.0;
    st.laser_cal_allowed = true;
    st.laser_last_mm = 87.654;
    st.laser_last_status = "signal_low";
    st.laser_age_ms = 120;
    std::string sj = json_status(st);
    CHECK(sj.find("\"laser_mode\":\"short\"") != std::string::npos && sj.find("\"laser_budget_ms\":33") != std::string::npos && sj.find("\"laser_roi\":\"8x8\"") != std::string::npos);
    CHECK(sj.find("\"laser_cal_offset_mm\":1.250") != std::string::npos && sj.find("\"laser_cal_xtalk_cps\":300") != std::string::npos && sj.find("\"laser_cal_allowed\":true") != std::string::npos);
    CHECK(sj.find("\"laser_last_mm\":87.654") != std::string::npos && sj.find("\"laser_last_status\":\"signal_low\"") != std::string::npos && sj.find("\"laser_age_ms\":120") != std::string::npos);
    CHECK(sj.size() < 1000 && sj.back() == '}');
    Status fresh;
    CHECK(json_status(fresh).find("\"laser_age_ms\":-1") != std::string::npos && json_status(fresh).find("\"laser_last_status\":\"none\"") != std::string::npos);
}

int main() {
    budgets_and_modes();
    area();
    registers();
    settings_of_the_laser();
    range_status();
    odd_readings();
    calibration_offset();
    calibration_crosstalk();
    answers();
    std::printf("%d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
