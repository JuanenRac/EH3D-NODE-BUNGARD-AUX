// EH3D-NODE-BUNGARD-AUX host tests: the settings file, the measure of the laser, the speed and the temperature of the spindle and the words of the network.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "../core/aux_config.hpp"
#include "../core/camera.hpp"
#include "../core/laser.hpp"
#include "../core/protocol.hpp"
#include "../core/thermo.hpp"
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
        "protect_output = off\n"
        "rtsp_port = 8554\n"
        "stream_user = cam\n"
        "stream_password = longenough1\n",
        p);
    CHECK(p.empty());
    CHECK(s.rtsp_port == 8554 && s.stream_user == "cam" && s.stream_password == "longenough1" && s.rtsp_enabled && s.onvif_enabled);
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
        "rpm_max = abc\n"
        "stream_password = short\n"
        "stream_user = a:b\n"
        "rtsp_port = 0\n",
        p);
    CHECK(p.size() == 11);
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

static void camera_picture_settings() {
    Settings d;
    CHECK(!d.camera_hmirror && !d.camera_vflip && d.camera_brightness == 0 && d.camera_contrast == 0 && d.camera_saturation == 0);
    CHECK(d.camera_auto_exposure && d.camera_auto_whitebalance && d.camera_exposure == 300);
    std::vector<Problem> problems;
    Settings s = parse_settings("camera_hmirror = yes\ncamera_vflip = on\ncamera_brightness = -2\ncamera_contrast = 1\ncamera_saturation = 2\ncamera_auto_exposure = no\ncamera_exposure = 800\ncamera_auto_whitebalance = off\n", problems);
    CHECK(problems.empty());
    CHECK(s.camera_hmirror && s.camera_vflip && s.camera_brightness == -2 && s.camera_contrast == 1 && s.camera_saturation == 2);
    CHECK(!s.camera_auto_exposure && s.camera_exposure == 800 && !s.camera_auto_whitebalance);
    // out of range, not whole or not a number: refused and the value stays
    problems.clear();
    s = parse_settings("camera_brightness = 3\ncamera_contrast = 1.5\ncamera_saturation = bright\ncamera_exposure = 1201\n", problems);
    CHECK(problems.size() == 4 && s.camera_brightness == 0 && s.camera_contrast == 0 && s.camera_saturation == 0 && s.camera_exposure == 300);
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
    // the spindle slows (a turn takes at most 5 % longer than the one before: nothing slows from one turn to the next to the half): the average follows
    std::uint64_t gap = 2500;
    for (int i = 0; i < 40; ++i) {
        gap = std::min<std::uint64_t>(5000, gap + gap * 5 / 100);
        one.pulse(t);
        t += gap;
    }
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

static void cameras() {
    using namespace bungard_aux;
    // the sensor says which one it is
    CHECK(model_from_pid(0x26) == CameraModel::Ov2640 && model_from_pid(0x5640) == CameraModel::Ov5640 && model_from_pid(0x3660) == CameraModel::Auto);
    CHECK(std::string(model_text(CameraModel::Ov5640)) == "ov5640" && std::string(model_text(CameraModel::Auto)) == "auto");
    // the nearest size that is not larger; the OV2640 stops at 1600x1200, the OV5640 goes on
    const auto& t = frame_sizes();
    CHECK(t[frame_index_for(640, 480, CameraModel::Ov2640)].width == 640);
    CHECK(t[frame_index_for(700, 500, CameraModel::Ov2640)].width == 640);
    CHECK(t[frame_index_for(1920, 1080, CameraModel::Ov2640)].width == 1280 && t[frame_index_for(1920, 1080, CameraModel::Ov2640)].height == 1024);
    CHECK(t[frame_index_for(1920, 1080, CameraModel::Ov5640)].width == 1920);
    CHECK(t[frame_index_for(2592, 1944, CameraModel::Ov5640)].height == 1944);
    CHECK(t[frame_index_for(2592, 1944, CameraModel::Ov2640)].width == 1600);
    CHECK(t[frame_index_for(2592, 1944, CameraModel::Auto)].width == 1600);  // not known yet: only what both can do
    CHECK(frame_index_for(10, 10, CameraModel::Ov5640) == 0);
    // the list for the ONVIF encoder: the largest first, and only what the model gives
    auto five = sizes_of(CameraModel::Ov5640);
    auto two = sizes_of(CameraModel::Ov2640);
    CHECK(five.front().first == 2592 && two.front().first == 1600 && two.back().first == 160 && five.size() > two.size());
    // the settings file
    std::vector<Problem> p;
    Settings s = parse_settings("camera_model = ov5640\ncamera_width = 1920\ncamera_height = 1080\n", p);
    CHECK(p.empty() && s.camera_model == CameraModel::Ov5640 && s.camera_width == 1920 && s.camera_height == 1080);
    p.clear();
    Settings b = parse_settings("camera_model = ov9999\ncamera_width = 3000\n", p);
    CHECK(p.size() == 2 && b.camera_model == CameraModel::Auto && b.camera_width == 640);
    // the status says which camera it is
    Status st;
    st.camera = true;
    st.camera_model = "ov5640";
    CHECK(json_status(st).find("\"camera_model\":\"ov5640\"") != std::string::npos);
}

static void thermometers() {
    using namespace bungard_aux;
    // the published check values of the two checksums: "123456789"
    const std::uint8_t nine[9] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    CHECK(onewire_crc8(nine, 9) == 0xA1);
    CHECK(smbus_pec(nine, 9) == 0xF4);
    // the scratchpad of the data sheet that a DS18B20 gives before its first conversion: 85 degrees and the bytes of the power-on state, with its own CRC
    std::uint8_t power_on[9] = {0x50, 0x05, 0x4B, 0x46, 0x7F, 0xFF, 0x0C, 0x10, 0x1C};
    CHECK(onewire_crc8(power_on, 9) == 0);
    double c = 0.0;
    CHECK(!ds18b20_celsius(power_on, c));  // not converted yet: not a reading
    // 25.0625 degrees (0x0191) with a good checksum, then with a wrong one
    std::uint8_t pad[9] = {0x91, 0x01, 0x4B, 0x46, 0x7F, 0xFF, 0x05, 0x10, 0};
    pad[8] = onewire_crc8(pad, 8);
    CHECK(ds18b20_celsius(pad, c) && near(c, 25.0625));
    pad[8] ^= 0x01;
    CHECK(!ds18b20_celsius(pad, c));
    // minus 10.125 degrees (0xFF5E)
    std::uint8_t cold[9] = {0x5E, 0xFF, 0x4B, 0x46, 0x7F, 0xFF, 0x02, 0x10, 0};
    cold[8] = onewire_crc8(cold, 8);
    CHECK(ds18b20_celsius(cold, c) && near(c, -10.125));
    // the NTC: 10 k at 25 degrees against 10 k, so half the supply is 25 degrees; 50 degrees of a beta 3950 is about 3.59 k
    CHECK(ntc_celsius(1.65, 3.3, 10000.0, 10000.0, 3950.0, c) && std::fabs(c - 25.0) < 0.01);
    const double r50 = 10000.0 * std::exp(3950.0 * (1.0 / 323.15 - 1.0 / 298.15));
    const double v50 = 3.3 * r50 / (10000.0 + r50);
    CHECK(ntc_celsius(v50, 3.3, 10000.0, 10000.0, 3950.0, c) && std::fabs(c - 50.0) < 0.01);
    CHECK(!ntc_celsius(0.0, 3.3, 10000.0, 10000.0, 3950.0, c) && !ntc_celsius(3.3, 3.3, 10000.0, 10000.0, 3950.0, c));  // a wire off
    // the MLX90614: units of 0.02 kelvin; 15000 is 26.85 degrees; the top bit is the error flag
    CHECK(mlx90614_celsius(15000, c) && std::fabs(c - 26.85) < 0.001);
    CHECK(!mlx90614_celsius(0x8000 | 15000, c));
    CHECK(!mlx90614_celsius(0, c));
    // the settings of the thermistor, and one that is out of range keeps the default
    std::vector<Problem> pr;
    Settings s = parse_settings("temp_sensor = ntc\nntc_beta = 4250\nntc_r25_ohm = 100000\nntc_series_ohm = 5\n", pr);
    CHECK(s.temp_sensor == TempSensor::Ntc && near(s.ntc_beta, 4250.0) && near(s.ntc_r25_ohm, 100000.0) && near(s.ntc_series_ohm, 10000.0) && pr.size() == 1);
}

int main() {
    settings_file();
    laser_measure();
    rpm_meter();
    camera_picture_settings();
    thermal_state();
    protect_output();
    words_of_the_network();
    cameras();
    thermometers();
    std::printf("%d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
