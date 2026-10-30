// EH3D-NODE-BUNGARD-AUX - main/sensors.hpp: what the firmware reads from the head. GPL-3.0-or-later.
#pragma once

#include <cstdint>

#include "../core/aux_config.hpp"

namespace sensors {

/// Sets up the pins that are wired (a GPIO set to -1 in the menu is left alone).
void init(const bungard_aux::Settings& s);

/// One reading of the distance to the table, in mm. False when the sensor gave nothing or says the reading is bad.
/// Through the VL53L1X on the I2C bus (the component `grrtzm/vl53l1x_library`); with no laser answering it is false and every measure is refused with "no_readings".
bool laser_read(double& distance_mm, bool& valid);

/// The turns per minute of the spindle, from the pulses counted in the interrupt.
double rpm_now();

/// The temperature of the spindle in degrees. False when the sensor does not answer.
/// The DS18B20, the NTC and the MLX90614 are read as head.cfg says; a sensor that is on in the settings and does not answer counts as hot.
bool temperature_read(double& celsius);

/// Opens (true) or closes the protect output: low means the spindle must stop.
void protect_set(bool open);

}  // namespace sensors
