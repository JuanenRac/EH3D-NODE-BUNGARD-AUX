// EH3D-NODE-BUNGARD-AUX host tests of the one-wire bus with several DS18B20: the address and its checksum, the search of every sensor on a simulated bus, the choice of the
// spindle's sensor, the resolution and the time of a conversion, and the settings that say it all.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "../core/aux_config.hpp"
#include "../core/onewire.hpp"

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

/// An address with its checksum filled in.
static Rom make_rom(std::uint8_t family, std::uint8_t serial) {
    Rom r{};
    r[0] = family;
    for (int i = 1; i < 7; ++i) r[i] = static_cast<std::uint8_t>(serial * 7 + i * 13);
    r[7] = onewire_crc8(r.data(), 7);
    return r;
}

/// A simulated bus: the devices answer the search the way the real ones do (the bit and its complement, wired AND), and leave the search when they disagree with the direction written.
struct SimBus {
    std::vector<Rom> devices;
    std::vector<bool> active;
    bool searching = false;
    int bit = 0;
    int phase = 0;  // 0: the bit is read next, 1: its complement, 2: the direction is written
    bool present = true;
    int resets = 0;

    bool reset() {
        ++resets;
        searching = false;
        active.assign(devices.size(), true);
        return present && !devices.empty();
    }
    void write_byte(std::uint8_t b) {
        if (b == 0xF0) {
            searching = true;
            bit = 0;
            phase = 0;
        }
    }
    bool bit_of(const Rom& r) const { return (r[static_cast<std::size_t>(bit / 8)] >> (bit % 8)) & 1; }
    bool read_bit() {
        bool value = true;  // nobody pulls the line down: 1
        for (std::size_t i = 0; i < devices.size(); ++i) {
            if (!active[i]) continue;
            const bool b = bit_of(devices[i]);
            if (phase == 0) value = value && b;
            else value = value && !b;
        }
        phase = (phase == 0) ? 1 : 2;
        return value;
    }
    void write_bit(bool direction) {
        for (std::size_t i = 0; i < devices.size(); ++i)
            if (active[i] && bit_of(devices[i]) != direction) active[i] = false;
        ++bit;
        phase = 0;
    }
};

static void address_and_checksum() {
    const Rom r = make_rom(kFamilyDs18b20, 3);
    CHECK(rom_valid(r) && rom_is_thermometer(r));
    Rom bad = r;
    bad[3] ^= 1;
    CHECK(!rom_valid(bad) && !rom_is_thermometer(bad));
    CHECK(!rom_is_thermometer(make_rom(0x01, 1)));  // a different family (a silicon serial number)
    CHECK(rom_is_thermometer(make_rom(0x10, 1)) && rom_is_thermometer(make_rom(0x22, 1)));
    const std::string t = rom_text(r);
    CHECK(t.size() == 16 && t.substr(0, 2) == "28");
    Rom back{};
    CHECK(rom_from_text(t, back) && back == r);
    std::string lower = t;
    for (char& c : lower) if (c >= 'A' && c <= 'F') c = static_cast<char>(c - 'A' + 'a');
    CHECK(rom_from_text(lower, back) && back == r);
    CHECK(!rom_from_text("28", back) && !rom_from_text("2800000000000000X", back) && !rom_from_text("28000000000000ZZ", back));
    CHECK(temp_rom_setting_valid("auto") && temp_rom_setting_valid(t) && !temp_rom_setting_valid("") && !temp_rom_setting_valid("autos") && !temp_rom_setting_valid("28FF"));
}

static void resolution_and_time() {
    CHECK(resolution_valid(9) && resolution_valid(12) && !resolution_valid(8) && !resolution_valid(13));
    CHECK(conversion_ms(9) < conversion_ms(10) && conversion_ms(10) < conversion_ms(11) && conversion_ms(11) < conversion_ms(12));
    CHECK(conversion_ms(12) >= 750 && conversion_ms(9) >= 94);  // never shorter than the data sheet
    CHECK(resolution_config(9) == 0x1F && resolution_config(10) == 0x3F && resolution_config(11) == 0x5F && resolution_config(12) == 0x7F);
    for (int bits = 9; bits <= 12; ++bits) {
        std::uint8_t pad[9] = {0x50, 0x05, 0x4B, 0x46, resolution_config(bits), 0xFF, 0x0C, 0x10, 0};
        CHECK(resolution_of(pad) == bits);
    }
}

static void search_finds_every_sensor() {
    for (int n = 1; n <= kMaxSensors; ++n) {
        SimBus bus;
        for (int i = 0; i < n; ++i) bus.devices.push_back(make_rom(kFamilyDs18b20, static_cast<std::uint8_t>(i + 1)));
        std::vector<Rom> found;
        const int answered = search_roms(bus, found);
        CHECK(answered == n);
        CHECK(static_cast<int>(found.size()) == n);
        for (const Rom& d : bus.devices) {
            bool seen = false;
            for (const Rom& f : found) seen = seen || (f == d);
            CHECK(seen);
        }
    }
    // nothing on the wire
    SimBus empty;
    std::vector<Rom> found;
    CHECK(search_roms(empty, found) == 0 && found.empty());
    // the bus is there but nobody answers the presence pulse
    SimBus dead;
    dead.devices.push_back(make_rom(kFamilyDs18b20, 1));
    dead.present = false;
    CHECK(search_roms(dead, found) == 0 && found.empty());
    // devices of other families are on the bus but are not thermometers: counted as answering, not listed
    SimBus mixed;
    mixed.devices.push_back(make_rom(kFamilyDs18b20, 4));
    mixed.devices.push_back(make_rom(0x01, 5));
    mixed.devices.push_back(make_rom(kFamilyDs18b20, 6));
    CHECK(search_roms(mixed, found) == 3 && found.size() == 2);
    // the limit
    SimBus many;
    for (int i = 0; i < 12; ++i) many.devices.push_back(make_rom(kFamilyDs18b20, static_cast<std::uint8_t>(i + 20)));
    CHECK(search_roms(many, found, 4) == 12 && found.size() == 4);
}

static void the_spindles_sensor_is_chosen() {
    std::vector<Rom> found = {make_rom(kFamilyDs18b20, 1), make_rom(kFamilyDs18b20, 2)};
    Rom chosen{};
    bool ambiguous = false;
    CHECK(choose_sensor(found, "auto", chosen, ambiguous) && chosen == found[0] && ambiguous);
    CHECK(choose_sensor({found[0]}, "auto", chosen, ambiguous) && chosen == found[0] && !ambiguous);
    CHECK(choose_sensor(found, rom_text(found[1]), chosen, ambiguous) && chosen == found[1] && !ambiguous);
    // an address that is not on the wire: no reading is taken from another sensor
    CHECK(!choose_sensor(found, rom_text(make_rom(kFamilyDs18b20, 9)), chosen, ambiguous));
    CHECK(!choose_sensor({}, "auto", chosen, ambiguous));
    CHECK(!choose_sensor(found, "not an address", chosen, ambiguous));
}

static void scratchpads_of_the_families() {
    // a DS18B20 at 12 bits: 25.0625 C = 0x0191
    std::uint8_t pad[9] = {0x91, 0x01, 0x4B, 0x46, 0x7F, 0xFF, 0x0C, 0x10, 0};
    pad[8] = onewire_crc8(pad, 8);
    double c = 0;
    CHECK(thermometer_celsius(make_rom(kFamilyDs18b20, 1), pad, c) && c > 25.06 && c < 25.07);
    // the power-on value (85 C with its reserved bytes) is not a reading
    std::uint8_t boot[9] = {0x50, 0x05, 0x4B, 0x46, 0x7F, 0xFF, 0x0C, 0x10, 0};
    boot[8] = onewire_crc8(boot, 8);
    CHECK(!thermometer_celsius(make_rom(kFamilyDs18b20, 1), boot, c));
    // a wrong checksum
    std::uint8_t broken[9];
    for (int i = 0; i < 9; ++i) broken[i] = pad[i];
    broken[0] ^= 4;
    CHECK(!thermometer_celsius(make_rom(kFamilyDs18b20, 1), broken, c));
    // a DS18S20 (half degrees): 0x002D is 22 whole degrees (the half bit is dropped); with 8 counts remaining of 16 the extended resolution gives 22 - 0.25 + 8/16 = 22.25
    std::uint8_t old[9] = {0x2D, 0x00, 0x4B, 0x46, 0xFF, 0xFF, 0x08, 0x10, 0};
    old[8] = onewire_crc8(old, 8);
    CHECK(thermometer_celsius(make_rom(0x10, 1), old, c) && c > 22.24 && c < 22.26);
}

static void the_settings() {
    Settings s;
    CHECK(s.temp_resolution == 12 && s.temp_rom == "auto");
    std::vector<Problem> problems;
    s = parse_settings("temp_resolution = 10\ntemp_rom = 28FF641E8F160423\n", problems);
    CHECK(problems.empty() || problems[0].key != "temp_resolution");
    CHECK(s.temp_resolution == 10);
    s = parse_settings("temp_resolution = 8\n", problems);
    CHECK(s.temp_resolution == 12);
    s = parse_settings("temp_resolution = 13\n", problems);
    CHECK(s.temp_resolution == 12);
    s = parse_settings("temp_rom = not-an-address\n", problems);
    CHECK(s.temp_rom == "auto");
    s = parse_settings("temp_rom = auto\n", problems);
    CHECK(s.temp_rom == "auto");
}

int main() {
    address_and_checksum();
    resolution_and_time();
    search_finds_every_sensor();
    the_spindles_sensor_is_chosen();
    scratchpads_of_the_families();
    the_settings();
    std::printf("%d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
