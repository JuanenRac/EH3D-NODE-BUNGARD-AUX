# Changelog

All notable changes to this project are documented here.

## [Unreleased]

## [0.0.1] - The rules of the node: the settings file, the measure of the laser, the watch of the spindle and the words of the network

- **The settings file:** `head.cfg` holds every tuned value (the network, the camera, the laser, the spindle, the temperature, the protect output); a value out of range or an unknown key is reported and the safe default stays, and limits that make no sense together fall back to the defaults.
- **The measure of the table:** several laser readings become one distance: the odd ones are thrown away (median and median absolute deviation), the range and the offset are applied, and a measure that spreads too much, lies out of the range or has too few readings is refused with the reason.
- **The spindle:** the speed from the pulses of a sensor (any number per turn, zero when the pulses stop), the temperature with a warning, a critical level and a hysteresis, and the protect output that opens when the spindle is too fast or too hot or when a sensor that is on does not answer.
- **The words of the network:** `GET /api/status`, `POST /api/measure` and `GET /stream`, the token in the header or the query compared in constant time, the answers as JSON and the line `Z x y height` that the desktop programs read as a point of the map of the table.
- **The firmware, not built:** the ESP-IDF project for an ESP32-S3 with a W5500 network chip (the network, the web server, the camera, the pulses of the spindle and the protect output); the drivers of the laser and of the temperature sensors are not written.
- **The video as an IP camera sends it:** RTSP (OPTIONS, DESCRIBE, SETUP, PLAY, PAUSE, GET_PARAMETER, TEARDOWN; JPEG over RTP over UDP or interleaved in the TCP connection; Digest and Basic login) and ONVIF (WS-Discovery, and the SOAP services of profile S: capabilities, services, profiles, video sources and encoders with the sizes the camera can do, the address of the stream and of the snapshot, the user token), with the user and the password of the settings; without a password nothing is served. The firmware that carries them is not built.
- **Host tests:** two programs with 175 checks on the rules above and on the video (the hashes of the logins against their published vectors, the RTSP session, the RTP payload of JPEG and ONVIF).
