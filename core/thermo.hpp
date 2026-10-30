// EH3D-NODE-BUNGARD-AUX - core/thermo.hpp: how the temperature of the spindle is read from each kind of sensor. GPL-3.0-or-later.
//
// The firmware moves the bytes and the voltages; what they mean is here, so it is tested on a computer: the checksum and the scratchpad of a DS18B20, the divider of an
// NTC thermistor and the raw word of an MLX90614 infrared sensor.
#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace bungard_aux {

/// The checksum of the one-wire bus (Dallas/Maxim CRC-8, polynomial 0x31 reflected). The CRC of a scratchpad of 9 bytes, with its own CRC byte included, is 0.
inline std::uint8_t onewire_crc8(const std::uint8_t* data, std::size_t n) {
    std::uint8_t crc = 0;
    for (std::size_t i = 0; i < n; ++i) {
        std::uint8_t b = data[i];
        for (int k = 0; k < 8; ++k) {
            const std::uint8_t mix = (crc ^ b) & 1;
            crc >>= 1;
            if (mix) crc ^= 0x8C;
            b >>= 1;
        }
    }
    return crc;
}

/// The temperature from the 9 bytes of a DS18B20 scratchpad. False when the checksum is wrong, or when the sensor still says 85 degrees with the reserved bytes of
/// its power-on state (it has not converted yet), or when the value is outside what the chip measures (-55 to 125).
inline bool ds18b20_celsius(const std::uint8_t scratchpad[9], double& celsius) {
    if (onewire_crc8(scratchpad, 9) != 0) return false;
    const std::int16_t raw = static_cast<std::int16_t>((static_cast<std::uint16_t>(scratchpad[1]) << 8) | scratchpad[0]);
    const double c = raw / 16.0;  // 12 bits: 0.0625 degrees; fewer bits leave the low ones at 0
    if (raw == 0x0550 && scratchpad[5] == 0xFF && scratchpad[7] == 0x10) return false;  // 85.0 and the power-on bytes: not converted yet
    if (c < -55.0 || c > 125.0) return false;
    celsius = c;
    return true;
}

/// The temperature of an NTC thermistor wired as a divider: the thermistor to ground and a fixed resistor to the supply, the voltage read across the thermistor.
/// `beta` and `r25` are the ones of the data sheet (the resistance at 25 degrees). False when the voltage is not between 0 and the supply (a wire off).
inline bool ntc_celsius(double volts, double supply_volts, double series_ohm, double r25_ohm, double beta, double& celsius) {
    if (supply_volts <= 0.0 || volts <= 0.0 || volts >= supply_volts || series_ohm <= 0.0 || r25_ohm <= 0.0 || beta <= 0.0) return false;
    const double r = series_ohm * volts / (supply_volts - volts);
    const double kelvin = 1.0 / (1.0 / 298.15 + std::log(r / r25_ohm) / beta);
    celsius = kelvin - 273.15;
    return celsius > -55.0 && celsius < 250.0;
}

/// The object temperature from the raw word of an MLX90614 (register 0x07): units of 0.02 kelvin. The top bit is the error flag of the chip.
inline bool mlx90614_celsius(std::uint16_t raw, double& celsius) {
    if (raw & 0x8000) return false;
    const double c = raw * 0.02 - 273.15;
    if (c < -70.0 || c > 380.0) return false;
    celsius = c;
    return true;
}

/// The SMBus packet error code (CRC-8, polynomial 0x07) that the MLX90614 sends after a word, over the bytes of the whole exchange.
inline std::uint8_t smbus_pec(const std::uint8_t* data, std::size_t n) {
    std::uint8_t crc = 0;
    for (std::size_t i = 0; i < n; ++i) {
        crc ^= data[i];
        for (int k = 0; k < 8; ++k) crc = (crc & 0x80) ? static_cast<std::uint8_t>((crc << 1) ^ 0x07) : static_cast<std::uint8_t>(crc << 1);
    }
    return crc;
}

}  // namespace bungard_aux
