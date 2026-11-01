// EH3D-NODE-BUNGARD-AUX host tests of the speed sensor (the TCRT5000 or any other pulse sensor): the filter of noise and of missed pulses, the quality of the train, the Schmitt
// trigger of the analogue input with fixed and with automatic thresholds, and the settings of the input.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "../core/aux_config.hpp"
#include "../core/pulse_ring.hpp"
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

static bool near(double a, double b, double eps) { return std::fabs(a - b) <= eps; }

/// A tiny deterministic generator for the jitter and the spikes.
struct Lcg {
    std::uint32_t s = 7;
    double next() {  // 0..1
        s = s * 1664525u + 1013904223u;
        return static_cast<double>(s >> 8) / 16777216.0;
    }
};

static void speeds_from_400_hz_to_1_khz() {
    // 24000 rpm with one mark: 400 pulses a second; 60000 rpm: 1000
    for (double rpm : {6000.0, 24000.0, 60000.0}) {
        RpmMeter m(1, 1500);
        m.set_limit_rpm(65000);
        const double gap = 60.0e6 / rpm;
        double t = 1000.0;
        for (int i = 0; i < 40; ++i) {
            m.pulse(static_cast<std::uint64_t>(t));
            t += gap;
        }
        CHECK(near(m.rpm(static_cast<std::uint64_t>(t)), rpm, rpm * 0.002));
        CHECK(m.quality() == RpmQuality::Good && m.pulses_rejected() == 0);
    }
    // three marks on the spindle: a third of the turn between pulses
    RpmMeter three(3, 1500);
    double t = 0;
    for (int i = 0; i < 40; ++i) {
        three.pulse(static_cast<std::uint64_t>(t));
        t += 60.0e6 / (24000.0 * 3.0);
    }
    CHECK(near(three.rpm(static_cast<std::uint64_t>(t)), 24000.0, 50.0));
}

static void jitter_does_not_move_the_speed_much() {
    RpmMeter m(1, 1500);
    m.set_limit_rpm(65000);
    Lcg g;
    double t = 0;
    for (int i = 0; i < 200; ++i) {
        t += 2500.0 * (1.0 + (g.next() - 0.5) * 0.04);  // 2 % of jitter either way
        m.pulse(static_cast<std::uint64_t>(t));
    }
    CHECK(near(m.rpm(static_cast<std::uint64_t>(t)), 24000.0, 600.0));
    CHECK(m.quality() == RpmQuality::Good);
}

static void spikes_of_the_variador_are_refused() {
    RpmMeter m(1, 1500);
    m.set_limit_rpm(30000);  // the fastest gap that is believed: 60e6 / (30000 * 4) = 500 us
    double t = 0;
    for (int i = 0; i < 60; ++i) {
        t += 2500;
        m.pulse(static_cast<std::uint64_t>(t));
        if (i % 3 == 2) m.pulse(static_cast<std::uint64_t>(t + 300));  // a spike 300 us after every third real pulse
    }
    CHECK(m.pulses_rejected() >= 15);
    CHECK(near(m.rpm(static_cast<std::uint64_t>(t)), 24000.0, 100.0));
    CHECK(m.quality() == RpmQuality::Noisy);
    // a clean train after the noise makes the quality good again
    for (int i = 0; i < 40; ++i) {
        t += 2500;
        m.pulse(static_cast<std::uint64_t>(t));
    }
    CHECK(m.quality() == RpmQuality::Good);
}

static void an_overspeed_is_read_as_an_overspeed() {
    for (double factor : {1.3, 2.0, 3.0}) {
        RpmMeter m(1, 1500);
        m.set_limit_rpm(24000);
        double t = 0;
        const double gap = 60.0e6 / (24000.0 * factor);
        for (int i = 0; i < 60; ++i) {
            t += gap;
            m.pulse(static_cast<std::uint64_t>(t));
        }
        CHECK(m.rpm(static_cast<std::uint64_t>(t)) > 24000.0 * (factor - 0.1));
    }
}

static void a_spike_in_the_middle_of_a_turn_is_refused_even_without_the_limit() {
    RpmMeter m(1, 1500);  // no limit set
    double t = 0;
    for (int i = 0; i < 20; ++i) {
        t += 2500;
        m.pulse(static_cast<std::uint64_t>(t));
    }
    m.pulse(static_cast<std::uint64_t>(t + 400));  // 400 us after the pulse, 0.16 of the usual gap
    CHECK(m.pulses_rejected() == 1);
    t += 2500;
    m.pulse(static_cast<std::uint64_t>(t));
    CHECK(near(m.rpm(static_cast<std::uint64_t>(t)), 24000.0, 60.0));
}

static void a_missed_pulse_is_mended() {
    RpmMeter m(1, 1500);
    double t = 0;
    for (int i = 0; i < 20; ++i) {
        t += 2500;
        m.pulse(static_cast<std::uint64_t>(t));
    }
    t += 5000;  // one pulse did not come
    m.pulse(static_cast<std::uint64_t>(t));
    CHECK(m.pulses_mended() == 1);
    CHECK(near(m.rpm(static_cast<std::uint64_t>(t)), 24000.0, 100.0));
    // a real slowing down is not taken for missed pulses (a turn takes 5 % longer than the one before; a spindle does not do more than that)
    RpmMeter slow(1, 1500);
    double gap = 2500;
    t = 0;
    for (int i = 0; i < 20; ++i) {
        t += 2500;
        slow.pulse(static_cast<std::uint64_t>(t));
    }
    for (int i = 0; i < 12; ++i) {
        gap *= 1.05;
        t += gap;
        slow.pulse(static_cast<std::uint64_t>(t));
    }
    CHECK(slow.pulses_mended() == 0 && slow.pulses_rejected() == 0);
}

static void start_and_stop() {
    RpmMeter m(1, 1500);
    m.set_limit_rpm(65000);
    CHECK(m.quality() == RpmQuality::None && m.rpm(0) == 0.0);
    // a start: the gaps shrink from 100 ms to 2.5 ms in steps of 15 %
    double t = 0, gap = 100000;
    int n = 0;
    while (gap > 2500 && n < 200) {
        t += gap;
        m.pulse(static_cast<std::uint64_t>(t));
        gap /= 1.15;
        ++n;
    }
    CHECK(m.pulses_rejected() == 0);
    CHECK(m.rpm(static_cast<std::uint64_t>(t)) > 5000.0);
    // the spindle stops: no pulse for longer than the stale time
    CHECK(m.rpm(static_cast<std::uint64_t>(t) + 2000000) == 0.0);
}

/// A TCRT5000 signal: a bright mark in front of the sensor for `duty` of the turn, `low` and `high` levels in mV, a little noise.
static std::vector<int> analog_train(int turns, int samples_per_turn, double duty, int low, int high, int noise_mv, Lcg& g) {
    std::vector<int> out;
    for (int turn = 0; turn < turns; ++turn)
        for (int i = 0; i < samples_per_turn; ++i) {
            const bool mark = i < static_cast<int>(samples_per_turn * duty);
            const double level = mark ? high : low;
            out.push_back(static_cast<int>(level + (g.next() - 0.5) * 2.0 * noise_mv));
        }
    return out;
}

static void the_schmitt_trigger_with_fixed_thresholds() {
    Lcg g;
    Schmitt s(2000, 1000, false);
    int rising = 0, falling = 0;
    for (int mv : analog_train(50, 50, 0.2, 300, 2800, 150, g)) {
        const int c = s.feed(mv);
        rising += c > 0;
        falling += c < 0;
    }
    CHECK(rising == 50 && falling == 50);
    // noise around one threshold does not make pulses: a signal that wanders between the two stays where it was
    Schmitt wander(2000, 1000, false);
    int crossings = 0;
    wander.feed(2500);
    for (int i = 0; i < 200; ++i) crossings += wander.feed(1400 + (i % 2) * 500) != 0;
    CHECK(crossings == 0);
}

static void the_schmitt_trigger_with_automatic_thresholds() {
    Lcg g;
    Schmitt s(2000, 1000, true);
    int rising = 0;
    // a weak signal (800 to 1500 mV) that fixed thresholds would never see
    const auto train = analog_train(200, 50, 0.2, 800, 1500, 20, g);
    for (std::size_t i = 0; i < train.size(); ++i) {
        const int c = s.feed(train[i]);
        if (i > 1000) rising += c > 0;  // after the first turns, where it learns the span
    }
    CHECK(rising >= 175 && rising <= 181);
    CHECK(s.high_mv() > 800 && s.high_mv() < 1500 && s.low_mv() < s.high_mv());
    // nothing in front of the sensor (a flat signal): no pulses at all
    Schmitt flat(2000, 1000, true);
    int any = 0;
    for (int i = 0; i < 5000; ++i) any += flat.feed(1200 + (g.next() > 0.5 ? 10 : -10)) != 0;
    CHECK(any == 0);
}

static void analog_samples_become_the_speed() {
    // the whole chain: 24000 rpm, one mark, the ADC at 20 kHz (50 us a sample): 50 samples a turn
    Schmitt trig(2000, 1000, true);
    RpmMeter meter(1, 1500);
    meter.set_limit_rpm(65000);
    Lcg g;
    const auto train = analog_train(400, 50, 0.2, 250, 2700, 100, g);
    std::uint64_t t_us = 0;
    for (int mv : train) {
        if (trig.feed(mv) > 0) meter.pulse(t_us);
        t_us += 50;
    }
    CHECK(near(meter.rpm(t_us), 24000.0, 240.0));
    CHECK(meter.quality() == RpmQuality::Good);
}

static void the_settings_of_the_input() {
    Settings d;
    CHECK(d.rpm_input == RpmInput::Digital && d.rpm_edge == RpmEdge::Rising && d.rpm_auto_threshold);
    std::vector<Problem> problems;
    Settings s = parse_settings("rpm_input = analog\nrpm_edge = falling\nrpm_threshold_hi_mv = 2400\nrpm_threshold_lo_mv = 900\nrpm_auto_threshold = no\n", problems);
    CHECK(problems.empty());
    CHECK(s.rpm_input == RpmInput::Analog && s.rpm_edge == RpmEdge::Falling && s.rpm_threshold_hi_mv == 2400 && s.rpm_threshold_lo_mv == 900 && !s.rpm_auto_threshold);
    problems.clear();
    s = parse_settings("rpm_input = wired\nrpm_edge = both\n", problems);
    CHECK(problems.size() == 2 && s.rpm_input == RpmInput::Digital && s.rpm_edge == RpmEdge::Rising);
    problems.clear();
    s = parse_settings("rpm_threshold_hi_mv = 800\nrpm_threshold_lo_mv = 1500\n", problems);
    CHECK(!problems.empty() && s.rpm_threshold_lo_mv < s.rpm_threshold_hi_mv);
    CHECK(std::string(rpm_input_text(RpmInput::Analog)) == "analog" && std::string(rpm_edge_text(RpmEdge::Falling)) == "falling");
    CHECK(std::string(rpm_quality_text(RpmQuality::Noisy)) == "noisy");
}

static void the_ring_of_pulses_survives_the_wrap_of_its_index() {
    PulseRing<8> ring;
    std::uint32_t tail = 0;
    std::vector<std::uint64_t> got;
    auto take = [&]() {
        tail = PulseRing<8>::resume_from(tail, ring.head());
        while (tail != ring.head()) got.push_back(ring.at(tail++));
    };
    for (std::uint64_t t = 1; t <= 5; ++t) ring.push(t);
    take();
    CHECK(got.size() == 5 && got.front() == 1 && got.back() == 5);
    // more pulses than the ring holds between two readings: the oldest are lost, the newest come in order
    got.clear();
    for (std::uint64_t t = 6; t <= 25; ++t) ring.push(t);
    take();
    CHECK(got.size() == 8 && got.front() == 18 && got.back() == 25);
    // the indices go through 2^32: no negative place, nothing lost, the order kept
    PulseRing<8> wrap(0xFFFFFFFCu);
    tail = wrap.head();
    got.clear();
    for (std::uint64_t t = 100; t < 106; ++t) wrap.push(t);  // the head goes through 0xFFFFFFFF to 2
    CHECK(wrap.head() == 2u);
    tail = PulseRing<8>::resume_from(tail, wrap.head());
    while (tail != wrap.head()) got.push_back(wrap.at(tail++));
    CHECK(got.size() == 6 && got.front() == 100 && got.back() == 105);
    // and more than a ring behind across the wrap
    tail = 0xFFFFFFFCu;
    for (std::uint64_t t = 106; t < 116; ++t) wrap.push(t);  // the head is now 12: sixteen ahead of the tail
    CHECK(PulseRing<8>::resume_from(tail, wrap.head()) == wrap.head() - 8u);
}

int main() {
    the_ring_of_pulses_survives_the_wrap_of_its_index();
    speeds_from_400_hz_to_1_khz();
    jitter_does_not_move_the_speed_much();
    spikes_of_the_variador_are_refused();
    an_overspeed_is_read_as_an_overspeed();
    a_spike_in_the_middle_of_a_turn_is_refused_even_without_the_limit();
    a_missed_pulse_is_mended();
    start_and_stop();
    the_schmitt_trigger_with_fixed_thresholds();
    the_schmitt_trigger_with_automatic_thresholds();
    analog_samples_become_the_speed();
    the_settings_of_the_input();
    std::printf("%d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
