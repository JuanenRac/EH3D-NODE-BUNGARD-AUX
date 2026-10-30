# Hardware notes

EH3D-NODE-BUNGARD-AUX is written for an ESP32-S3 board with a wired network port (a W5500 over SPI) and a camera connector, mounted on the drill head. Nothing here has been wired or run; every pin is a setting of `idf.py menuconfig` (`EH3D Bungard AUX`) and must be checked against the schematic of the board you use.

| Part | How it connects | Notes |
|---|---|---|
| Wired network | W5500 over SPI (MISO, MOSI, SCLK, CS, interrupt, reset) | the defaults in `main/Kconfig.projbuild` are placeholders |
| Camera | the DVP camera connector of the board (the pins are set in menuconfig); the model of the camera that goes on the board is not decided yet | MJPEG on `/stream`, RTSP (JPEG over RTP) and ONVIF at the size and quality of `head.cfg`; the sizes the camera can do are listed in `core/onvif.hpp` and must follow the real sensor |
| Laser distance sensor | I2C (SDA, SCL), a VL53L1X or similar | the driver is not written; the sensor looks down at the table beside the tool; `laser_offset_mm` tells where it sits above the tool tip |
| Pulses of the spindle | one GPIO, rising edges | a hall sensor and a magnet on the spindle, or a slotted disc; `rpm_pulses_per_rev` says how many pulses a turn gives |
| Temperature of the spindle | one-wire (DS18B20), an NTC on an ADC pin or an I2C infrared sensor (MLX90614) | the drivers are not written; `temp_sensor` picks the kind |
| Protect output | one GPIO, high while the spindle may run | wire it into the stop line of the control unit through a relay or an opto-coupler that opens by itself without power |

## The protect output

The output is low at start-up and stays low whenever the node is not running, so a loss of power or a crash opens the stop line. It goes high only after the node has seen the spindle inside its limits. It is a help, not a safety function: the hardware stop button of the machine stays as it is.

## The laser and the table

The sensor measures the distance from the head to the table. The node returns that distance (plus `laser_offset_mm`) and the spread of the readings; the desktop programs take minus the distance as the height of the table and build the map (`eh3d-levelling`). Keep the sensor clean and away from chips and coolant: a measure that is refused is better than a wrong one.
