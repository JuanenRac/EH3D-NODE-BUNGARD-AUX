// EH3D-NODE-BUNGARD-AUX - core/onewire.hpp: the DS18B20 on a one-wire bus that has more than one sensor. GPL-3.0-or-later.
//
// The firmware moves the bits; what they mean is here, so it is tested on a computer with a simulated bus: the address (ROM) of a sensor with its checksum, the search of
// every sensor on the wire (the algorithm of Maxim application note 187), the choice of the one that belongs to the spindle, the resolution and the time a conversion takes.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "thermo.hpp"

namespace bungard_aux {

/// The address of a one-wire device: the family code, six bytes of serial number and the checksum of the seven.
using Rom = std::array<std::uint8_t, 8>;

constexpr std::uint8_t kFamilyDs18b20 = 0x28;
constexpr int kMaxSensors = 8;

/// A real address: its checksum is right.
inline bool rom_valid(const Rom& r) { return onewire_crc8(r.data(), 8) == 0; }

/// A DS18B20 (family 0x28, or the older DS18S20 0x10 and DS1822 0x22 that the same commands read) with a right checksum.
inline bool rom_is_thermometer(const Rom& r) { return rom_valid(r) && (r[0] == kFamilyDs18b20 || r[0] == 0x22 || r[0] == 0x10); }

/// The address as 16 hex digits, the family code first (the form of the setting `temp_rom` and of the panel).
inline std::string rom_text(const Rom& r) {
    static const char* digits = "0123456789ABCDEF";
    std::string s;
    for (std::uint8_t b : r) {
        s += digits[b >> 4];
        s += digits[b & 15];
    }
    return s;
}

/// 16 hex digits (upper or lower case) as an address; false for anything else. The checksum is not checked here.
inline bool rom_from_text(const std::string& text, Rom& out) {
    if (text.size() != 16) return false;
    Rom r{};
    for (std::size_t i = 0; i < 16; ++i) {
        const char c = text[i];
        int v;
        if (c >= '0' && c <= '9') v = c - '0';
        else if (c >= 'a' && c <= 'f') v = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') v = c - 'A' + 10;
        else return false;
        r[i / 2] = static_cast<std::uint8_t>((i % 2 == 0) ? (v << 4) : (r[i / 2] | v));
    }
    out = r;
    return true;
}

/// The setting `temp_rom`: "auto" (the only sensor on the wire, or the first one found when there are several) or the 16 hex digits of one address.
inline bool temp_rom_setting_valid(const std::string& v) {
    Rom r;
    return v == "auto" || rom_from_text(v, r);
}

/// How long a conversion takes at a resolution of 9, 10, 11 or 12 bits, in ms (the maximum of the data sheet), with a small margin.
inline int conversion_ms(int bits) {
    switch (bits) {
        case 9: return 100;
        case 10: return 200;
        case 11: return 400;
        default: return 800;
    }
}

inline bool resolution_valid(int bits) { return bits >= 9 && bits <= 12; }

/// The configuration byte of the scratchpad for a resolution (the two bits R1 R0 above the five fixed ones).
inline std::uint8_t resolution_config(int bits) { return static_cast<std::uint8_t>(((bits - 9) << 5) | 0x1F); }

/// The resolution a scratchpad says it has (byte 4), 9 to 12.
inline int resolution_of(const std::uint8_t scratchpad[9]) { return 9 + ((scratchpad[4] >> 5) & 3); }

/// The search of the sensors. `Bus` is the one-wire bus: reset() (true when something answers), write_byte(b), read_bit() and write_bit(b). Finds up to `max` addresses with a right
/// checksum (the others are dropped) and says how many devices answered in all; a bus with nothing on it finds none.
template <class Bus>
int search_roms(Bus& bus, std::vector<Rom>& found, int max = kMaxSensors) {
    found.clear();
    int last_discrepancy = 0;
    bool done = false;
    Rom rom{};
    int answered = 0;
    while (!done && answered < 64) {
        if (!bus.reset()) return answered;
        bus.write_byte(0xF0);  // SEARCH ROM
        int id_bit_number = 1;
        int last_zero = 0;
        std::array<std::uint8_t, 8> next = rom;
        for (int i = 0; i < 8; ++i) next[i] = (last_discrepancy == 0) ? 0 : rom[i];
        bool ok = true;
        for (id_bit_number = 1; id_bit_number <= 64; ++id_bit_number) {
            const bool id_bit = bus.read_bit();
            const bool cmp_id_bit = bus.read_bit();
            bool direction;
            if (id_bit && cmp_id_bit) {  // nobody answers any more: the bus was disturbed
                ok = false;
                break;
            }
            const int byte = (id_bit_number - 1) / 8;
            const std::uint8_t mask = static_cast<std::uint8_t>(1u << ((id_bit_number - 1) % 8));
            if (id_bit != cmp_id_bit) {
                direction = id_bit;  // all the devices still in the search have the same bit
            } else {
                if (id_bit_number < last_discrepancy) direction = (next[byte] & mask) != 0;
                else direction = (id_bit_number == last_discrepancy);
                if (!direction) last_zero = id_bit_number;
            }
            if (direction) next[byte] = static_cast<std::uint8_t>(next[byte] | mask);
            else next[byte] = static_cast<std::uint8_t>(next[byte] & ~mask);
            bus.write_bit(direction);
        }
        if (!ok) return answered;
        rom = next;
        last_discrepancy = last_zero;
        ++answered;
        if (rom_is_thermometer(rom) && static_cast<int>(found.size()) < max) found.push_back(rom);
        if (last_discrepancy == 0) done = true;
    }
    return answered;
}

/// Which sensor is the spindle's. `want` is the setting `temp_rom`. With "auto" the only sensor is taken, and when there are several the first one found (a warning for the panel:
/// `ambiguous`); with an address the one with that address, and none at all when it is not on the wire (the reading is refused rather than taken from another sensor).
inline bool choose_sensor(const std::vector<Rom>& found, const std::string& want, Rom& chosen, bool& ambiguous) {
    ambiguous = false;
    if (found.empty()) return false;
    if (want == "auto") {
        chosen = found.front();
        ambiguous = found.size() > 1;
        return true;
    }
    Rom wanted;
    if (!rom_from_text(want, wanted)) return false;
    for (const Rom& r : found) {
        if (r == wanted) {
            chosen = r;
            return true;
        }
    }
    return false;
}

/// The reading of a sensor that has answered: the value only when the checksum is right and the sensor has really converted (not the 85 of power-on). A DS18S20 (family 0x10)
/// has its own format (half degrees); it is turned into the 12-bit form first.
inline bool thermometer_celsius(const Rom& rom, const std::uint8_t scratchpad[9], double& celsius) {
    if (rom[0] != 0x10) return ds18b20_celsius(scratchpad, celsius);
    if (onewire_crc8(scratchpad, 9) != 0) return false;
    const std::int16_t raw = static_cast<std::int16_t>((static_cast<std::uint16_t>(scratchpad[1]) << 8) | scratchpad[0]);
    double c = (raw >> 1);
    const int remain = scratchpad[6], per_c = scratchpad[7];
    if (per_c != 0) c = c - 0.25 + static_cast<double>(per_c - remain) / per_c;  // the extended resolution of the data sheet
    if (c < -55.0 || c > 125.0) return false;
    celsius = c;
    return true;
}

}  // namespace bungard_aux
