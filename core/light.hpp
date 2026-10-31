// EH3D-NODE-BUNGARD-AUX - core/light.hpp
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
// The ring of LEDs around the camera of the head (eight addressable LEDs of the WS2812 kind, one wire). Everything that decides what the ring shows lives here: how
// bright it is (a level from 0 to 100 that follows the eye, not the current, and never goes over the ceiling of the settings), the tone of the light, which sectors of the
// ring are lit (one side only gives a raking light that shows the edge of a hole or the depth of a groove), when it turns itself off, and the bytes that go to the LEDs.
// Nothing here touches a pin, so it is tested on a computer.
#pragma once

#include <cmath>
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

#include "aux_config.hpp"

namespace bungard_aux {

struct Rgb {
    std::uint8_t r = 0, g = 0, b = 0;
};

/// The colour of a tone at full strength (`Custom` has no colour of its own: it is in the state). A WS2812 makes white from three small LEDs that are not equally bright, so the tinted whites are balanced by eye and the plain white is left alone: the camera balances it itself.
inline Rgb tone_rgb(Tone t) {
    switch (t) {
        case Tone::Cool: return {205, 225, 255};
        case Tone::Warm: return {255, 190, 120};
        case Tone::Red: return {255, 0, 0};
        case Tone::Green: return {0, 255, 0};
        case Tone::Blue: return {0, 0, 255};
        case Tone::Custom: return {255, 255, 255};
        default: return {255, 255, 255};  // the white of the camera: the three LEDs at the same strength
    }
}

struct LightSettings {
    bool enabled = true;
    std::uint32_t leds = 8;           // 1..64 LEDs on the ring
    std::uint32_t level = 60;         // 0..100: the level used when the light is turned on without saying how bright
    std::uint32_t max_level = 80;     // 1..100: the ceiling, whatever is asked; eight LEDs at full white take close to half an ampere
    Tone tone = Tone::Neutral;        // the tone used when the light is turned on without saying which
    std::uint32_t auto_off_s = 600;   // 0..86400: the light turns itself off after this long (0: never), so a forgotten light does not cook the head
    bool on_at_start = false;         // the light is on when the node starts
    ColourOrder order = ColourOrder::Grb;
};

/// The settings of the ring, as the settings file of the node gives them.
inline LightSettings light_of(const Settings& s) {
    LightSettings l;
    l.enabled = s.light_enabled;
    l.leds = s.light_leds;
    l.level = s.light_level;
    l.max_level = s.light_max_level;
    l.tone = s.light_tone;
    l.auto_off_s = s.light_auto_off_s;
    l.on_at_start = s.light_on_at_start;
    l.order = s.light_order;
    return l;
}

/// What the ring shows now.
struct LightState {
    bool on = false;
    std::uint32_t level = 60;         // 0..100, already under the ceiling
    Tone tone = Tone::Neutral;
    std::uint8_t sectors = 0xFF;      // bit k lights the k-th eighth of the ring, counted from the front of the head and clockwise
    Rgb custom{255, 255, 255};        // the colour when the tone is Custom: the ring is RGB, so any colour can say something
};

/// The colour the state shines with at full strength.
inline Rgb colour_of(const LightState& st) { return st.tone == Tone::Custom ? st.custom : tone_rgb(st.tone); }

/// The sectors by name: the four sides (each two eighths) and all of them.
inline bool sectors_from_text(const std::string& v, std::uint8_t& out) {
    if (v == "all") { out = 0xFF; return true; }
    if (v == "front") { out = 0x83; return true; }   // eighths 7, 0 and 1
    if (v == "right") { out = 0x0E; return true; }   // 1, 2, 3
    if (v == "back") { out = 0x38; return true; }    // 3, 4, 5
    if (v == "left") { out = 0xE0; return true; }    // 5, 6, 7
    if (v == "front_back") { out = 0xBB; return true; }
    if (v == "left_right") { out = 0xEE; return true; }
    return false;
}

/// How bright a level is, from 0 to 255. The eye sees the light on a curve, so 50 looks half as bright as 100 and not much dimmer.
inline std::uint8_t level_to_byte(std::uint32_t level) {
    if (level == 0) return 0;
    if (level > 100) level = 100;
    double v = std::pow(static_cast<double>(level) / 100.0, 2.2) * 255.0;
    if (v < 1.0) v = 1.0;  // a level above zero is never fully dark
    return static_cast<std::uint8_t>(std::lround(v));
}

/// The level under the ceiling of the settings.
inline std::uint32_t capped_level(const LightSettings& s, std::uint32_t asked) {
    if (asked > 100) asked = 100;
    return asked > s.max_level ? s.max_level : asked;
}

/// The three bytes of every LED of the ring, in the order the LEDs take them. An LED belongs to the sector its position on the ring falls in.
inline std::vector<std::uint8_t> render_frame(const LightSettings& s, const LightState& st) {
    std::vector<std::uint8_t> out(static_cast<std::size_t>(s.leds) * 3, 0);
    if (!st.on || !s.enabled || st.level == 0) return out;
    const Rgb c = colour_of(st);
    const double k = level_to_byte(capped_level(s, st.level)) / 255.0;
    const std::uint8_t r = static_cast<std::uint8_t>(std::lround(c.r * k));
    const std::uint8_t g = static_cast<std::uint8_t>(std::lround(c.g * k));
    const std::uint8_t b = static_cast<std::uint8_t>(std::lround(c.b * k));
    for (std::uint32_t i = 0; i < s.leds; ++i) {
        const std::uint32_t sector = static_cast<std::uint32_t>((static_cast<std::uint64_t>(i) * 8) / s.leds);
        if (!(st.sectors & (1u << sector))) continue;
        std::uint8_t* p = &out[static_cast<std::size_t>(i) * 3];
        if (s.order == ColourOrder::Grb) { p[0] = g; p[1] = r; p[2] = b; }
        else { p[0] = r; p[1] = g; p[2] = b; }
    }
    return out;
}

/// The state a request leaves: it starts from `now` and changes only what the request names.
struct LightChange {
    bool ok = true;
    std::string why;  // the key that could not be used, when it could not
    LightState state;
};

/// Reads `on=1&level=70&tone=warm&sectors=left` (or `sectors=0x0f`). A request that only names a level turns the light on; `on=0` turns it off and keeps the rest, so
/// the next `on=1` brings the same light back. A level above the ceiling is brought down to it, not refused.
inline LightChange apply_request(const LightSettings& s, const LightState& now, const std::string& query) {
    LightChange c;
    c.state = now;
    if (!s.enabled) {
        c.ok = false;
        c.why = "light_disabled";
        return c;
    }
    auto value = [&](const std::string& key) {
        std::size_t pos = 0;
        while (pos <= query.size()) {
            std::size_t end = query.find('&', pos);
            if (end == std::string::npos) end = query.size();
            std::string pair = query.substr(pos, end - pos);
            std::size_t eq = pair.find('=');
            if (eq != std::string::npos && pair.substr(0, eq) == key) return pair.substr(eq + 1);
            pos = end + 1;
        }
        return std::string();
    };
    const std::string on = value("on"), level = value("level"), tone = value("tone"), sectors = value("sectors"), color = value("color");
    bool turned_on = false;
    std::uint32_t n = 0;
    if (!level.empty()) {
        if (!detail::to_uint(level, 0, 100, n)) { c.ok = false; c.why = "level"; return c; }
        c.state.level = capped_level(s, n);
        turned_on = n > 0;
    }
    if (!tone.empty()) {
        Tone t;
        if (!tone_from_text(tone, t)) { c.ok = false; c.why = "tone"; return c; }
        c.state.tone = t;
    }
    if (!color.empty()) {
        // any colour as six hexadecimal digits (ff8800, with or without a leading #)
        const std::string hex = color[0] == '#' ? color.substr(1) : color;
        if (hex.size() != 6) { c.ok = false; c.why = "color"; return c; }
        std::uint32_t v = 0;
        for (char ch : hex) {
            int d = ch >= '0' && ch <= '9' ? ch - '0' : ch >= 'a' && ch <= 'f' ? ch - 'a' + 10 : ch >= 'A' && ch <= 'F' ? ch - 'A' + 10 : -1;
            if (d < 0) { c.ok = false; c.why = "color"; return c; }
            v = v * 16 + static_cast<std::uint32_t>(d);
        }
        if (v == 0) { c.ok = false; c.why = "color"; return c; }  // black is off: say so with on=0
        c.state.tone = Tone::Custom;
        c.state.custom = {static_cast<std::uint8_t>(v >> 16), static_cast<std::uint8_t>(v >> 8), static_cast<std::uint8_t>(v)};
        turned_on = true;
    }
    if (!sectors.empty()) {
        std::uint8_t m = 0;
        if (!sectors_from_text(sectors, m)) {
            std::string hex = sectors.rfind("0x", 0) == 0 ? sectors.substr(2) : "";
            std::uint32_t v = 0;
            if (hex.empty() || hex.size() > 2) { c.ok = false; c.why = "sectors"; return c; }
            for (char ch : hex) {
                int d = ch >= '0' && ch <= '9' ? ch - '0' : ch >= 'a' && ch <= 'f' ? ch - 'a' + 10 : ch >= 'A' && ch <= 'F' ? ch - 'A' + 10 : -1;
                if (d < 0) { c.ok = false; c.why = "sectors"; return c; }
                v = v * 16 + static_cast<std::uint32_t>(d);
            }
            m = static_cast<std::uint8_t>(v);
        }
        if (m == 0) { c.ok = false; c.why = "sectors"; return c; }  // no sector lit is the same as off: say so with on=0
        c.state.sectors = m;
        turned_on = true;
    }
    if (!on.empty()) {
        bool b;
        if (!detail::to_bool(on, b)) { c.ok = false; c.why = "on"; return c; }
        c.state.on = b;
        if (b && c.state.level == 0) c.state.level = capped_level(s, s.level);
    } else if (turned_on) {
        c.state.on = true;
    }
    return c;
}

/// A signal in colour that the ring gives for a while on top of the light, whether the light is on or not: the head says how a check went without anyone looking at a screen.
enum class Cue { None, Ok, NotOk, Busy, Warn, Alarm };

inline const char* cue_text(Cue c) {
    switch (c) {
        case Cue::Ok: return "ok";
        case Cue::NotOk: return "nok";
        case Cue::Busy: return "busy";
        case Cue::Warn: return "warn";
        case Cue::Alarm: return "alarm";
        default: return "none";
    }
}

inline bool cue_from_text(const std::string& v, Cue& out) {
    static const Cue all[] = {Cue::None, Cue::Ok, Cue::NotOk, Cue::Busy, Cue::Warn, Cue::Alarm};
    for (Cue c : all) {
        if (v == cue_text(c)) {
            out = c;
            return true;
        }
    }
    return false;
}

/// How long a cue lasts when the request does not say (0: until another cue or `none`).
inline std::uint32_t cue_default_ms(Cue c) {
    switch (c) {
        case Cue::Ok: return 2500;
        case Cue::NotOk: return 5000;
        case Cue::Warn: return 4000;
        case Cue::Alarm: return 8000;
        default: return 0;
    }
}

struct CueRequest {
    bool present = false;
    bool ok = true;
    std::string why;
    Cue cue = Cue::None;
    std::uint32_t duration_ms = 0;
};

/// Reads `cue=nok&cue_ms=3000`. A duration of 100 ms to ten minutes; without one the cue lasts as long as its kind says.
inline CueRequest parse_cue(const std::string& query) {
    CueRequest r;
    auto value = [&](const std::string& key) {
        std::size_t pos = 0;
        while (pos <= query.size()) {
            std::size_t end = query.find('&', pos);
            if (end == std::string::npos) end = query.size();
            std::string pair = query.substr(pos, end - pos);
            std::size_t eq = pair.find('=');
            if (eq != std::string::npos && pair.substr(0, eq) == key) return pair.substr(eq + 1);
            pos = end + 1;
        }
        return std::string();
    };
    const std::string cue = value("cue"), ms = value("cue_ms");
    if (cue.empty()) return r;
    r.present = true;
    if (!cue_from_text(cue, r.cue)) { r.ok = false; r.why = "cue"; return r; }
    r.duration_ms = cue_default_ms(r.cue);
    if (!ms.empty()) {
        std::uint32_t n;
        if (!detail::to_uint(ms, 100, 600000, n)) { r.ok = false; r.why = "cue_ms"; return r; }
        r.duration_ms = n;
    }
    return r;
}

/// The cue that is running.
struct CueRun {
    Cue cue = Cue::None;
    std::uint64_t started_ms = 0;
    std::uint32_t duration_ms = 0;
    bool active(std::uint64_t now_ms) const { return cue != Cue::None && (duration_ms == 0 || now_ms - started_ms < duration_ms); }
};

/// The ring while a cue runs. Ok is steady green; NotOk blinks red; Warn blinks amber slowly; Alarm blinks red fast; Busy is a blue spot that turns. A cue shines at the level of
/// the light (the level of the settings when the light is off), under the ceiling, so it never dazzles the camera more than the light does.
inline std::vector<std::uint8_t> render_cue(const LightSettings& s, const LightState& st, Cue cue, std::uint64_t elapsed_ms) {
    std::vector<std::uint8_t> out(static_cast<std::size_t>(s.leds) * 3, 0);
    if (cue == Cue::None || !s.enabled) return out;
    Rgb c;
    std::uint32_t period = 0;  // 0: steady
    switch (cue) {
        case Cue::Ok: c = {0, 255, 0}; break;
        case Cue::NotOk: c = {255, 0, 0}; period = 500; break;
        case Cue::Warn: c = {255, 140, 0}; period = 1000; break;
        case Cue::Alarm: c = {255, 0, 0}; period = 200; break;
        default: c = {0, 60, 255}; break;
    }
    if (period && (elapsed_ms % period) >= period / 2) return out;  // the dark half of a blink
    const std::uint32_t level = capped_level(s, st.on && st.level ? st.level : s.level);
    const double k = level_to_byte(level) / 255.0;
    // the busy spot is the LED at this step of the turn and the one behind it, dimmer
    const std::uint32_t spot = static_cast<std::uint32_t>((elapsed_ms / 90) % s.leds);
    const std::uint32_t tail = (spot + s.leds - 1) % s.leds;
    for (std::uint32_t i = 0; i < s.leds; ++i) {
        if (cue == Cue::Busy && i != spot && i != tail) continue;
        std::uint8_t* p = &out[static_cast<std::size_t>(i) * 3];
        const double f = k * ((cue == Cue::Busy && i == tail) ? 0.3 : 1.0);
        const std::uint8_t rr = static_cast<std::uint8_t>(std::lround(c.r * f));
        const std::uint8_t gg = static_cast<std::uint8_t>(std::lround(c.g * f));
        const std::uint8_t bb = static_cast<std::uint8_t>(std::lround(c.b * f));
        if (s.order == ColourOrder::Grb) { p[0] = gg; p[1] = rr; p[2] = bb; }
        else { p[0] = rr; p[1] = gg; p[2] = bb; }
    }
    return out;
}

/// The light a node starts with.
inline LightState initial_state(const LightSettings& s) {
    LightState st;
    st.level = capped_level(s, s.level);
    st.tone = s.tone;
    st.on = s.enabled && s.on_at_start;
    return st;
}

/// The timer that turns a forgotten light off. `arm` is called every time the light is turned on or changed.
class AutoOff {
public:
    explicit AutoOff(std::uint32_t seconds = 0) : seconds_(seconds) {}
    void arm(std::uint64_t now_ms) { armed_at_ = now_ms; armed_ = true; }
    void disarm() { armed_ = false; }
    bool expired(std::uint64_t now_ms) const { return armed_ && seconds_ > 0 && now_ms - armed_at_ >= static_cast<std::uint64_t>(seconds_) * 1000; }
    /// Seconds left, or -1 when the light does not turn itself off.
    long remaining_s(std::uint64_t now_ms) const {
        if (!armed_ || seconds_ == 0) return -1;
        const std::uint64_t spent = now_ms - armed_at_;
        const std::uint64_t total = static_cast<std::uint64_t>(seconds_) * 1000;
        return spent >= total ? 0 : static_cast<long>((total - spent + 999) / 1000);
    }

private:
    std::uint32_t seconds_;
    std::uint64_t armed_at_ = 0;
    bool armed_ = false;
};

/// The light as the programs read it (also inside the status of the node).
inline std::string json_light(const LightSettings& s, const LightState& st, long remaining_s, Cue cue = Cue::None) {
    char buf[300];
    std::snprintf(buf, sizeof buf, "{\"light\":%s,\"on\":%s,\"level\":%u,\"max_level\":%u,\"tone\":\"%s\",\"sectors\":%u,\"leds\":%u,\"auto_off_s\":%ld,\"color\":\"%02x%02x%02x\",\"cue\":\"%s\"}", s.enabled ? "true" : "false",
                  st.on ? "true" : "false", static_cast<unsigned>(st.level), static_cast<unsigned>(s.max_level), tone_text(st.tone), static_cast<unsigned>(st.sectors),
                  static_cast<unsigned>(s.leds), remaining_s, colour_of(st).r, colour_of(st).g, colour_of(st).b, cue_text(cue));
    return buf;
}

}  // namespace bungard_aux
