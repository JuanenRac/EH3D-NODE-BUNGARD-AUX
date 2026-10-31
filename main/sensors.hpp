// EH3D-NODE-BUNGARD-AUX - main/sensors.hpp: what the firmware reads from the head. GPL-3.0-or-later.
#pragma once

#include <cstdint>

#include <vector>

#include "../core/aux_config.hpp"
#include "../core/laser.hpp"

namespace sensors {

/// Sets up the pins that are wired (a GPIO set to -1 in the menu is left alone).
void init(const bungard_aux::Settings& s);

/// One reading of the distance to the table, with the sensor's own verdict (its range status, the spread it estimates, the strength of the light that came back).
/// False when the sensor gave nothing. Through the VL53L1X on the I2C bus (the component `grrtzm/vl53l1x_library`); with no laser answering it is false and every measure is refused with "no_readings".
bool laser_read(bungard_aux::LaserReading& out);

/// The last reading any caller got from the sensor and how long ago (ms); false when it has given none yet.
bool laser_last(bungard_aux::LaserReading& out, long& age_ms);

/// Up to `count` readings (with the number of detectors and the corrected signal, for calibrating) in at most `timeout_ms`, the sensor held for the whole run. False when it gave none.
bool laser_collect(int count, std::uint32_t timeout_ms, std::vector<bungard_aux::LaserReading>& out);

/// Writes the crosstalk found by the calibration (counts per second; 0 clears it) to the sensor now.
bool laser_set_xtalk(double cps);

/// The turns per minute of the spindle, from the pulses counted in the interrupt.
double rpm_now();

/// The temperature of the spindle in degrees. False when the sensor does not answer.
/// The DS18B20, the NTC and the MLX90614 are read as head.cfg says; a sensor that is on in the settings and does not answer counts as hot.
bool temperature_read(double& celsius);

/// Opens (true) or closes the protect output: low means the spindle must stop.
void protect_set(bool open);

}  // namespace sensors
