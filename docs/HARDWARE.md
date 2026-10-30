# Hardware notes

EH3D-NODE-BUNGARD-AUX is written for an ESP32-S3 board with a wired network port (a W5500 over SPI) and a camera connector, mounted on the drill head. Nothing here has been wired or run; every pin is a setting of `idf.py menuconfig` (`EH3D Bungard AUX`) and must be checked against the schematic of the board you use.

| Part | How it connects | Notes |
|---|---|---|
| Wired network | W5500 over SPI (MISO, MOSI, SCLK, CS, interrupt, reset) | the defaults in `main/Kconfig.projbuild` are placeholders |
| Camera | the DVP camera connector of the board, an OV2640 (2 MP) or an OV5640 (5 MP); the defaults of `menuconfig` are the pins of the camera connector of the Waveshare ESP32-S3-ETH (XCLK 3, SDA 48, SCL 47, VSYNC 1, HREF 2, PCLK 39, D0 to D7 41, 45, 46, 42, 40, 38, 15, 18; PWDN and RESET not wired) | MJPEG on `/stream`, RTSP (JPEG over RTP) and ONVIF at the size and quality of `head.cfg`; the sensor says which one it is and the node gives the largest size that `camera_width` and `camera_height` allow and that sensor can do (`core/camera.hpp`); the OV5640 needs the PSRAM for the large sizes |
| Laser distance sensor | I2C (SDA, SCL), a VL53L1X or similar | the driver is written (the component `grrtzm/vl53l1x_library`) and has never run; on the Waveshare ESP32-S3-ETH the camera takes GPIO 41 and 42, so the I2C pins are not set by default; the sensor looks down at the table beside the tool; `laser_offset_mm` tells where it sits above the tool tip |
| Pulses of the spindle | one GPIO, rising edges | a hall sensor and a magnet on the spindle, or a slotted disc; `rpm_pulses_per_rev` says how many pulses a turn gives |
| Temperature of the spindle | one-wire (DS18B20), an NTC on an ADC pin or an I2C infrared sensor (MLX90614) | the drivers are written and have never run; `temp_sensor` picks the kind; the NTC has its divider in `ntc_series_ohm`, `ntc_r25_ohm` and `ntc_beta`, and the MLX90614 shares the I2C bus of the laser |
| Protect output | one GPIO, high while the spindle may run | wire it into the stop line of the control unit through a relay or an opto-coupler that opens by itself without power |

## The protect output

The output is low at start-up and stays low whenever the node is not running, so a loss of power or a crash opens the stop line. It goes high only after the node has seen the spindle inside its limits. It is a help, not a safety function: the hardware stop button of the machine stays as it is.

## The laser and the table

The sensor measures the distance from the head to the table. The node returns that distance (plus `laser_offset_mm`) and the spread of the readings; the desktop programs take minus the distance as the height of the table and build the map (`eh3d-levelling`). Keep the sensor clean and away from chips and coolant: a measure that is refused is better than a wrong one.
