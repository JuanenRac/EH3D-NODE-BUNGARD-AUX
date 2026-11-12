// EH3D-NODE-BUNGARD-AUX host tests: the ring of LEDs around the camera: its settings, the curve of the levels, the ceiling, the sectors, the requests, the timer that turns
// a forgotten light off and the words of the status.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
#include <cstdio>
#include <string>
#include <vector>

#include "../core/aux_config.hpp"
#include "../core/light.hpp"
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

static LightSettings defaults() { return light_of(Settings{}); }

static void the_settings() {
    std::vector<Problem> p;
    Settings d = parse_settings("", p);
    CHECK(p.empty());
    CHECK(d.light_enabled && d.light_leds == 8 && d.light_level == 60 && d.light_max_level == 80 && d.light_tone == Tone::Neutral && d.light_auto_off_s == 600);
    CHECK(!d.light_on_at_start && d.light_order == ColourOrder::Grb);
    Settings s = parse_settings("light_leds = 12\nlight_level = 40\nlight_max_level = 90\nlight_tone = warm\nlight_auto_off_s = 0\nlight_on_at_start = on\nlight_order = rgb\nlight_enabled = off\n", p);
    CHECK(p.empty() && s.light_leds == 12 && s.light_level == 40 && s.light_max_level == 90 && s.light_tone == Tone::Warm && s.light_auto_off_s == 0);
    CHECK(s.light_on_at_start && s.light_order == ColourOrder::Rgb && !s.light_enabled);
    // what cannot be used is reported and the default stays
    p.clear();
    Settings b = parse_settings("light_leds = 0\nlight_leds = 65\nlight_level = 101\nlight_tone = pink\nlight_order = bgr\nlight_max_level = 0\nlight_auto_off_s = 90000\n", p);
    CHECK(p.size() == 7);
    CHECK(b.light_leds == 8 && b.light_level == 60 && b.light_tone == Tone::Neutral && b.light_order == ColourOrder::Grb && b.light_max_level == 80 && b.light_auto_off_s == 600);
    // a start level above the ceiling is brought down to it
    p.clear();
    Settings c = parse_settings("light_level = 90\nlight_max_level = 50\n", p);
    CHECK(p.size() == 1 && c.light_level == 50);
}

static void the_curve_follows_the_eye() {
    CHECK(level_to_byte(0) == 0);
    CHECK(level_to_byte(1) >= 1);  // above zero is never dark
    CHECK(level_to_byte(100) == 255);
    CHECK(level_to_byte(500) == 255);
    CHECK(level_to_byte(50) > 40 && level_to_byte(50) < 70);  // half the level is far less than half the current
    for (std::uint32_t l = 1; l <= 100; ++l) CHECK(level_to_byte(l) >= level_to_byte(l - 1));
}

static void the_ceiling_holds() {
    LightSettings s = defaults();
    CHECK(capped_level(s, 100) == 80);
    CHECK(capped_level(s, 30) == 30);
    CHECK(capped_level(s, 5000) == 80);
    LightState st;
    st.on = true;
    st.level = 100;
    std::vector<std::uint8_t> f = render_frame(s, st);
    const std::uint8_t top = level_to_byte(80);
    CHECK(f.size() == 24);
    for (std::size_t i = 0; i < f.size(); ++i) CHECK(f[i] <= top);
}

static void the_frame() {
    LightSettings s = defaults();
    LightState off;
    CHECK(render_frame(s, off) == std::vector<std::uint8_t>(24, 0));
    LightState st;
    st.on = true;
    st.level = 60;
    st.tone = Tone::Red;
    auto f = render_frame(s, st);
    CHECK(f[0] == 0 && f[1] > 0 && f[2] == 0);  // grb: green first, so red is the second byte
    s.order = ColourOrder::Rgb;
    f = render_frame(s, st);
    CHECK(f[0] > 0 && f[1] == 0 && f[2] == 0);
    s.order = ColourOrder::Grb;
    // a disabled ring is dark whatever is asked
    s.enabled = false;
    CHECK(render_frame(s, st) == std::vector<std::uint8_t>(24, 0));
    // the whites are white-ish, the blue is only blue
    CHECK(tone_rgb(Tone::Neutral).r > 200 && tone_rgb(Tone::Neutral).g > 200 && tone_rgb(Tone::Neutral).b > 200);
    CHECK(tone_rgb(Tone::Warm).r > tone_rgb(Tone::Warm).b && tone_rgb(Tone::Cool).b > tone_rgb(Tone::Cool).r);
    CHECK(tone_rgb(Tone::Blue).r == 0 && tone_rgb(Tone::Blue).g == 0 && tone_rgb(Tone::Blue).b == 255);
}

static int lit(const std::vector<std::uint8_t>& f) {
    int n = 0;
    for (std::size_t i = 0; i < f.size(); i += 3) n += (f[i] || f[i + 1] || f[i + 2]) ? 1 : 0;
    return n;
}

static void the_sectors() {
    LightSettings s = defaults();
    LightState st;
    st.on = true;
    st.sectors = 0x0F;  // the first half
    CHECK(lit(render_frame(s, st)) == 4);
    st.sectors = 0x01;
    auto f = render_frame(s, st);
    CHECK(lit(f) == 1 && (f[0] || f[1] || f[2]));  // only the first LED
    // twelve LEDs: the eighths fall between them, none is lost and none is lit twice
    s.leds = 12;
    st.sectors = 0xFF;
    CHECK(lit(render_frame(s, st)) == 12);
    st.sectors = 0x0F;
    CHECK(lit(render_frame(s, st)) == 6);
    // names
    std::uint8_t m = 0;
    CHECK(sectors_from_text("all", m) && m == 0xFF);
    CHECK(sectors_from_text("left", m) && m == 0xE0);
    CHECK(sectors_from_text("right", m) && m == 0x0E);
    CHECK(!sectors_from_text("up", m));
}

static void the_requests() {
    LightSettings s = defaults();
    LightState now = initial_state(s);
    CHECK(!now.on && now.level == 60);
    LightChange c = apply_request(s, now, "on=1");
    CHECK(c.ok && c.state.on && c.state.level == 60);
    c = apply_request(s, now, "level=40");  // only a level turns it on
    CHECK(c.ok && c.state.on && c.state.level == 40);
    c = apply_request(s, now, "level=100");  // above the ceiling: brought down, not refused
    CHECK(c.ok && c.state.level == 80);
    c = apply_request(s, c.state, "on=0");  // off keeps the rest
    CHECK(c.ok && !c.state.on && c.state.level == 80);
    c = apply_request(s, c.state, "on=1");
    CHECK(c.ok && c.state.on && c.state.level == 80);
    c = apply_request(s, now, "on=1&level=30&tone=cool&sectors=left");
    CHECK(c.ok && c.state.on && c.state.level == 30 && c.state.tone == Tone::Cool && c.state.sectors == 0xE0);
    c = apply_request(s, now, "sectors=0x0f");
    CHECK(c.ok && c.state.sectors == 0x0F && c.state.on);
    c = apply_request(s, now, "level=0");
    CHECK(c.ok && !c.state.on && c.state.level == 0);
    c = apply_request(s, c.state, "on=1");  // from zero the default level comes back
    CHECK(c.ok && c.state.on && c.state.level == 60);
    // what is wrong is named and changes nothing
    for (const char* bad : {"level=abc", "level=101", "tone=pink", "sectors=0", "sectors=0x1ff", "sectors=0xzz", "sectors=up", "on=maybe"}) {
        LightChange x = apply_request(s, now, bad);
        CHECK(!x.ok && !x.why.empty());
    }
    CHECK(apply_request(s, now, "level=abc").why == "level");
    CHECK(apply_request(s, now, "tone=pink").why == "tone");
    CHECK(apply_request(s, now, "sectors=0").why == "sectors");
    CHECK(apply_request(s, now, "on=maybe").why == "on");
    // a ring that is switched off in the settings refuses
    s.enabled = false;
    CHECK(!apply_request(s, now, "on=1").ok && apply_request(s, now, "on=1").why == "light_disabled");
    // a node that starts with the light on
    LightSettings t = defaults();
    t.on_at_start = true;
    CHECK(initial_state(t).on);
    t.on_at_start = false;
    t.max_level = 30;
    CHECK(initial_state(t).level == 30);
}

static void the_colours_and_the_cues() {
    LightSettings s = defaults();
    LightState now = initial_state(s);
    // the default light is the plain white of the camera, and the old name still works
    CHECK(now.tone == Tone::Neutral && std::string(tone_text(Tone::Neutral)) == "white");
    CHECK(tone_rgb(Tone::Neutral).r == 255 && tone_rgb(Tone::Neutral).g == 255 && tone_rgb(Tone::Neutral).b == 255);
    Tone t;
    CHECK(tone_from_text("white", t) && t == Tone::Neutral && tone_from_text("neutral", t) && t == Tone::Neutral && !tone_from_text("custom", t));
    // any colour as hex
    LightChange c = apply_request(s, now, "color=ff8800");
    CHECK(c.ok && c.state.on && c.state.tone == Tone::Custom && c.state.custom.r == 255 && c.state.custom.g == 0x88 && c.state.custom.b == 0);
    auto f = render_frame(s, c.state);
    CHECK(f[0] > 0 && f[1] > f[0] && f[2] == 0);  // grb: green byte first, and the green is weaker than the red
    c = apply_request(s, now, "color=%2300ff00");
    CHECK(!c.ok);  // an encoded hash is not a colour: the programs send the six digits
    c = apply_request(s, now, "color=#00ff00");
    CHECK(c.ok && c.state.custom.g == 255);
    for (const char* bad : {"color=abc", "color=gggggg", "color=000000", "color=ff88001"}) CHECK(!apply_request(s, now, bad).ok && apply_request(s, now, bad).why == "color");
    // a tone after a custom colour brings the tone back
    c = apply_request(s, apply_request(s, now, "color=ff8800").state, "tone=white");
    CHECK(c.ok && c.state.tone == Tone::Neutral);
    // the cues
    CueRequest q = parse_cue("cue=nok");
    CHECK(q.present && q.ok && q.cue == Cue::NotOk && q.duration_ms == 5000);
    q = parse_cue("cue=ok&cue_ms=1500");
    CHECK(q.ok && q.cue == Cue::Ok && q.duration_ms == 1500);
    q = parse_cue("cue=busy");
    CHECK(q.ok && q.cue == Cue::Busy && q.duration_ms == 0);
    CHECK(!parse_cue("cue=party").ok && parse_cue("cue=party").why == "cue");
    CHECK(!parse_cue("cue=ok&cue_ms=5").ok && parse_cue("cue=ok&cue_ms=5").why == "cue_ms");
    CHECK(!parse_cue("level=5").present);
    CueRun run;
    run.cue = Cue::Ok;
    run.started_ms = 1000;
    run.duration_ms = 2500;
    CHECK(run.active(3499) && !run.active(3500));
    run.duration_ms = 0;
    CHECK(run.active(9999999));
    CHECK(!CueRun{}.active(0));
    // ok is steady green, shining even with the light off
    LightState off = now;
    auto ok = render_cue(s, off, Cue::Ok, 123);
    CHECK(lit(ok) == 8 && ok[0] > 0 && ok[1] == 0 && ok[2] == 0);
    // not ok blinks red: lit in the first half of the period, dark in the second
    CHECK(lit(render_cue(s, off, Cue::NotOk, 100)) == 8 && lit(render_cue(s, off, Cue::NotOk, 300)) == 0 && lit(render_cue(s, off, Cue::NotOk, 600)) == 8);
    auto nok = render_cue(s, off, Cue::NotOk, 100);
    CHECK(nok[0] == 0 && nok[1] > 0 && nok[2] == 0);
    // alarm blinks faster than not ok; warn is amber
    CHECK(lit(render_cue(s, off, Cue::Alarm, 150)) == 0 && lit(render_cue(s, off, Cue::Alarm, 50)) == 8);
    auto warn = render_cue(s, off, Cue::Warn, 100);
    CHECK(warn[0] > 0 && warn[1] > warn[0] && warn[2] == 0);
    // busy is a spot that turns: one bright LED and a dimmer one behind it
    auto b0 = render_cue(s, off, Cue::Busy, 0);
    auto b1 = render_cue(s, off, Cue::Busy, 90);
    CHECK(lit(b0) == 2 && lit(b1) == 2 && b0 != b1);
    CHECK(b0[2] > 0 && b0[2] > b0[23] && b0[23] > 0);  // the blue byte of the spot; the tail (the last LED) is dimmer
    // a cue never goes over the ceiling and never lights a ring that is switched off
    LightState loud = now;
    loud.on = true;
    loud.level = 100;
    auto top = render_cue(s, loud, Cue::Ok, 0);
    CHECK(top[0] <= level_to_byte(80));
    s.enabled = false;
    CHECK(lit(render_cue(s, off, Cue::Ok, 0)) == 0);
    CHECK(json_light(defaults(), now, -1, Cue::NotOk).find("\"cue\":\"nok\"") != std::string::npos);
    CHECK(json_light(defaults(), c.state, -1).find("\"color\":\"ffffff\"") != std::string::npos);
}

static void the_timer() {
    AutoOff t(10);
    CHECK(!t.expired(100000) && t.remaining_s(0) == -1);  // not armed
    t.arm(1000);
    CHECK(!t.expired(10999) && t.expired(11000));
    CHECK(t.remaining_s(1000) == 10 && t.remaining_s(10500) == 1 && t.remaining_s(11000) == 0);
    t.arm(20000);  // any change gives the light a new lease
    CHECK(!t.expired(25000));
    t.disarm();
    CHECK(!t.expired(99999999));
    AutoOff never(0);
    never.arm(0);
    CHECK(!never.expired(99999999) && never.remaining_s(5) == -1);
}

static void the_words() {
    LightSettings s = defaults();
    LightState st = initial_state(s);
    st.on = true;
    st.tone = Tone::Warm;
    std::string j = json_light(s, st, 42);
    CHECK(j.find("\"on\":true") != std::string::npos && j.find("\"level\":60") != std::string::npos && j.find("\"tone\":\"warm\"") != std::string::npos);
    CHECK(j.find("\"max_level\":80") != std::string::npos && j.find("\"leds\":8") != std::string::npos && j.find("\"auto_off_s\":42") != std::string::npos);
    // the routes
    Request r;
    CHECK(parse_request_line("POST /api/light?on=1 HTTP/1.1", r) && route_of(r) == Route::LightSet && r.query == "on=1");
    CHECK(parse_request_line("GET /api/light HTTP/1.1", r) && route_of(r) == Route::LightGet);
    CHECK(parse_request_line("PUT /api/light HTTP/1.1", r) && route_of(r) == Route::BadMethod);
    CHECK(parse_request_line("GET /api/snapshot HTTP/1.1", r) && route_of(r) == Route::Snapshot);
    CHECK(parse_request_line("POST /api/snapshot HTTP/1.1", r) && route_of(r) == Route::BadMethod);
    // they need the token: a node without one serves none of them
    Settings with;
    with.token = "abcdefgh12";
    Request q;
    q.method = "POST";
    q.path = "/api/light";
    CHECK(!authorized(with, Route::LightSet, "", q) && authorized(with, Route::LightSet, "abcdefgh12", q));
    CHECK(!authorized(with, Route::Snapshot, "nope", q) && authorized(with, Route::Snapshot, "abcdefgh12", q));
    Settings none;
    CHECK(!authorized(none, Route::LightSet, "anything", q) && !authorized(none, Route::Snapshot, "anything", q));
    // the status carries the ring
    Status status;
    status.light = true;
    status.light_on = true;
    status.light_level = 55;
    std::string js = json_status(status);
    CHECK(js.find("\"light\":true") != std::string::npos && js.find("\"light_on\":true") != std::string::npos && js.find("\"light_level\":55") != std::string::npos);
    CHECK(js.size() < 860);  // the status stays a short line for the small boards and the programs that poll it (the four keys that tell why the protect output opened took 110 bytes of it)
}

int main() {
    the_settings();
    the_curve_follows_the_eye();
    the_ceiling_holds();
    the_frame();
    the_sectors();
    the_requests();
    the_colours_and_the_cues();
    the_timer();
    the_words();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
