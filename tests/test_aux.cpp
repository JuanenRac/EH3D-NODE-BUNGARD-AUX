// EH3D-NODE-BUNGARD-AUX host tests: the settings file, the measure of the laser, the speed and the temperature of the spindle and the words of the network.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "../core/aux_config.hpp"
#include "../core/laser.hpp"
#include "../core/protocol.hpp"
#include "../core/spindle.hpp"

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

static std::vector<LaserReading> readings(std::initializer_list<double> v) {
    std::vector<LaserReading> r;
    for (double d : v) r.push_back({d, true});
    return r;
}

static void settings_file() {
    std::vector<Problem> p;
    Settings d = parse_settings("", p);
    CHECK(p.empty());
    CHECK(d.camera_enabled && d.laser_enabled && d.rpm_enabled && d.temp_sensor == TempSensor::None);
    CHECK(d.token.empty() && d.protect_output && d.laser_samples == 7);
    CHECK(near(d.temp_warn_c, 55.0) && near(d.temp_critical_c, 70.0));

    p.clear();
    Settings s = parse_settings(
        "# the node of the head\n"
        "name = head-1\n"
        "address = 192.168.0.60\n"
        "token = abcdefgh12\n"
        "laser_samples = 11\n"
        "laser_offset_mm = -1.25\n"
        "rpm_pulses_per_rev = 2\n"
        "temp_sensor = ds18b20\n"
        "temp_warn_c = 50\n"
        "temp_critical_c = 65.5\n"
        "protect_output = off\n",
        p);
    CHECK(p.empty());
    CHECK(s.name == "head-1" && s.address == "192.168.0.60" && s.token == "abcdefgh12");
    CHECK(s.laser_samples == 11 && near(s.laser_offset_mm, -1.25) && s.rpm_pulses_per_rev == 2);
    CHECK(s.temp_sensor == TempSensor::Ds18b20 && near(s.temp_warn_c, 50.0) && near(s.temp_critical_c, 65.5) && !s.protect_output);

    // what cannot be used is said and leaves the default
    p.clear();
    Settings b = parse_settings(
        "laser_samples = 99\n"
        "address = 300.1.1.1\n"
        "token = short\n"
        "temp_sensor = lm35\n"
        "camera_enabled = maybe\n"
        "colour = red\n"
        "no equals here\n"
        "rpm_max = abc\n",
        p);
    CHECK(p.size() == 8);
    CHECK(b.laser_samples == 7 && b.address.empty() && b.token.empty() && b.temp_sensor == TempSensor::None && b.camera_enabled);

    // limits that make no sense together fall back to the defaults, and are said
    p.clear();
    Settings w = parse_settings("temp_warn_c = 80\ntemp_critical_c = 60\nlaser_range_min_mm = 300\nlaser_range_max_mm = 100\n", p);
    CHECK(p.size() == 2);
    CHECK(near(w.temp_warn_c, 55.0) && near(w.temp_critical_c, 70.0) && near(w.laser_range_min_mm, 20.0) && near(w.laser_range_max_mm, 400.0));
}

static void laser_measure() {
    LaserLimits lim;
    // a steady table: the median and a small spread
    Measure m = measure_distance(readings({100.02, 100.00, 99.98, 100.01, 99.99, 100.00, 100.03}), 7, lim);
    CHECK(m.ok && near(m.distance_mm, 100.0, 0.011) && m.sigma_mm < 0.05 && m.used == 7 && m.rejected == 0);

    // one wild reading (a chip flying by) is thrown away, the rest is kept
    m = measure_distance(readings({100.0, 100.01, 99.99, 140.0, 100.0, 100.02, 99.98}), 7, lim);
    CHECK(m.ok && m.rejected == 1 && m.used == 6 && near(m.distance_mm, 100.0, 0.02));

    // the offset moves the distance
    LaserLimits off = lim;
    off.offset_mm = 2.5;
    m = measure_distance(readings({100.0, 100.0, 100.0}), 3, off);
    CHECK(m.ok && near(m.distance_mm, 102.5));

    // readings the sensor itself marks as bad do not count; with too few left the measure is refused
    std::vector<LaserReading> r = readings({100.0, 100.0});
    for (int i = 0; i < 5; ++i) r.push_back({100.0, false});
    m = measure_distance(r, 7, lim);
    CHECK(!m.ok && m.why == MeasureWhy::TooFew);

    // out of the range
    m = measure_distance(readings({5.0, 6.0, 5.5}), 3, lim);
    CHECK(!m.ok && m.why == MeasureWhy::OutOfRange);
    m = measure_distance(readings({900.0, 900.0, 900.0}), 3, lim);
    CHECK(!m.ok && m.why == MeasureWhy::OutOfRange);

    // nothing arrived
    m = measure_distance({}, 7, lim);
    CHECK(!m.ok && m.why == MeasureWhy::NoReadings);

    // a table that shakes: the spread is more than the setting allows
    LaserLimits tight = lim;
    tight.sigma_max_mm = 0.05;
    m = measure_distance(readings({100.0, 100.4, 99.6, 100.3, 99.7, 100.2, 99.8}), 7, tight);
    CHECK(!m.ok);
}

static void rpm_meter() {
    // 1 pulse per turn at 24000 rpm: a pulse every 2500 us
    RpmMeter one(1, 1500);
    CHECK(near(one.rpm(0), 0.0) && !one.turning(0));
    std::uint64_t t = 1000;
    for (int i = 0; i < 12; ++i) {
        one.pulse(t);
        t += 2500;
    }
    CHECK(near(one.rpm(t), 24000.0, 1.0));
    // two pulses per turn at the same pulse rate is half the speed
    RpmMeter two(2, 1500);
    t = 1000;
    for (int i = 0; i < 12; ++i) {
        two.pulse(t);
        t += 2500;
    }
    CHECK(near(two.rpm(t), 12000.0, 1.0));
    // the spindle slows: the average follows
    for (int i = 0; i < 12; ++i) {
        one.pulse(t);
        t += 5000;
    }
    CHECK(near(one.rpm(t), 12000.0, 1.0));
    // no pulse for longer than the stale time: stopped
    CHECK(one.rpm(t + 2000000) == 0.0);
    CHECK(!one.turning(t + 2000000));
}

static void thermal_state() {
    Thermal th(55.0, 70.0, 3.0);
    CHECK(th.state() == Heat::Unknown);
    CHECK(th.update(30.0, true) == Heat::Ok);
    CHECK(th.update(56.0, true) == Heat::Warm);
    // the hysteresis: it does not go back to ok until it is 3 degrees under the warning
    CHECK(th.update(54.0, true) == Heat::Warm);
    CHECK(th.update(51.9, true) == Heat::Ok);
    CHECK(th.update(71.0, true) == Heat::Hot);
    CHECK(th.update(68.0, true) == Heat::Hot);
    CHECK(th.update(66.9, true) == Heat::Warm);
    CHECK(th.update(40.0, true) == Heat::Ok);
    // a sensor that fails is not a cold spindle
    CHECK(th.update(0.0, false) == Heat::Unknown);
    CHECK(th.update(30.0, true) == Heat::Ok);
}

static void protect_output() {
    Settings s;
    CHECK(!protect_open(s, 20000.0, Heat::Unknown));  // no temperature sensor in the settings: it is never asked
    CHECK(protect_open(s, 70000.0, Heat::Unknown));   // too fast
    s.temp_sensor = TempSensor::Ds18b20;
    CHECK(protect_open(s, 0.0, Heat::Unknown));  // a sensor that is on and silent counts as hot
    CHECK(protect_open(s, 0.0, Heat::Hot));
    CHECK(!protect_open(s, 20000.0, Heat::Warm));
    CHECK(!protect_open(s, 20000.0, Heat::Ok));
    s.protect_output = false;
    CHECK(!protect_open(s, 90000.0, Heat::Hot));  // the output is switched off in the settings
}

static void words_of_the_network() {
    Request r;
    CHECK(parse_request_line("POST /api/measure?samples=5&token=x HTTP/1.1", r));
    CHECK(r.method == "POST" && r.path == "/api/measure" && r.query == "samples=5&token=x");
    CHECK(route_of(r) == Route::Measure);
    CHECK(query_value(r.query, "samples") == "5" && query_value(r.query, "token") == "x" && query_value(r.query, "nope").empty());
    CHECK(!parse_request_line("nonsense", r));
    CHECK(!parse_request_line("GET index.html HTTP/1.1", r));
    CHECK(parse_request_line("GET /api/status HTTP/1.1", r) && route_of(r) == Route::Status);
    Request wrong;
    parse_request_line("GET /api/measure HTTP/1.1", wrong);
    CHECK(route_of(wrong) == Route::BadMethod);
    Request lost;
    parse_request_line("GET /secret HTTP/1.1", lost);
    CHECK(route_of(lost) == Route::NotFound);

    // the token: in the header or in the query; a node without a token serves only the status
    Settings with;
    with.token = "abcdefgh12";
    Request m;
    parse_request_line("POST /api/measure HTTP/1.1", m);
    CHECK(!authorized(with, Route::Measure, "", m));
    CHECK(!authorized(with, Route::Measure, "wrong-token", m));
    CHECK(authorized(with, Route::Measure, "abcdefgh12", m));
    Request q;
    parse_request_line("GET /stream?token=abcdefgh12 HTTP/1.1", q);
    CHECK(authorized(with, Route::Stream, "", q));
    Request q2;
    parse_request_line("GET /stream?token=nope HTTP/1.1", q2);
    CHECK(!authorized(with, Route::Stream, "", q2));
    Settings none;
    CHECK(authorized(none, Route::Status, "", m));
    CHECK(!authorized(none, Route::Measure, "anything", m));
    CHECK(!authorized(none, Route::Stream, "anything", q));
    CHECK(same_secret("a", "a") && !same_secret("a", "b") && !same_secret("a", "ab"));

    // how many readings: the query, inside what is allowed
    Request s5;
    parse_request_line("POST /api/measure?samples=5 HTTP/1.1", s5);
    CHECK(samples_of(with, s5) == 5);
    Request s99;
    parse_request_line("POST /api/measure?samples=99 HTTP/1.1", s99);
    CHECK(samples_of(with, s99) == with.laser_samples);
    CHECK(samples_of(with, m) == with.laser_samples);

    // the answers
    Status st;
    st.name = "head-1";
    st.version = "0.0.1";
    st.camera = true;
    st.laser = true;
    st.rpm_on = true;
    st.rpm = 23998.4;
    st.has_temp = true;
    st.temp_c = 41.26;
    st.heat = Heat::Ok;
    st.uptime_s = 90;
    std::string j = json_status(st);
    CHECK(j.find("\"node\":\"head-1\"") != std::string::npos);
    CHECK(j.find("\"rpm\":23998") != std::string::npos && j.find("\"temp_c\":41.3") != std::string::npos);
    CHECK(j.find("\"protect_open\":false") != std::string::npos && j.find("\"uptime_s\":90") != std::string::npos);
    st.has_temp = false;
    CHECK(json_status(st).find("\"temp_c\":null") != std::string::npos);
    st.name = "a\"b";
    CHECK(json_status(st).find("a\\\"b") != std::string::npos);

    Measure ok = measure_distance(readings({100.0, 100.0, 100.0}), 3, LaserLimits{});
    std::string mj = json_measure(ok);
    CHECK(mj.find("\"ok\":true") != std::string::npos && mj.find("\"distance_mm\":100.000") != std::string::npos);
    CHECK(reading_line(ok, 12.5, 40.0) == "Z 12.500 40.000 -100.0000");
    Measure bad = measure_distance({}, 3, LaserLimits{});
    CHECK(json_measure(bad).find("\"why\":\"no_readings\"") != std::string::npos);
    CHECK(reading_line(bad, 1.0, 2.0).empty());
}

int main() {
    settings_file();
    laser_measure();
    rpm_meter();
    thermal_state();
    protect_output();
    words_of_the_network();
    std::printf("%d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
