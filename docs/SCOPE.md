# EH3D-NODE-BUNGARD-AUX: scope

Status: **host-test**. EH3D-NODE-BUNGARD-AUX is the firmware, in C++17 for the ESP32-S3, of the auxiliary node of the Bungard drill: it measures the height of the table with a laser, the speed and the temperature of the spindle, offers a protect output, streams the camera and lights the ring. Its rules are tested on a computer; the firmware builds, and it has never been flashed or run on a board. This page says what it is for, what it will not do and where it sits among the other programs of the suite.

## Purpose

The node that sits on the head of the drill beside the control node and gives the suite what the control unit does not: the distance to the table from several laser readings, the speed of the spindle from the pulses of a sensor, its temperature with a warning and a critical level, a protect output that is low whenever the spindle must stop, a video stream (MJPEG, RTSP and ONVIF) and the light ring, all over a wired network with a web panel.

## What it does

* Measures the table: asks the laser for several readings, throws the odd ones away, refuses a measure that spreads more than its limit or lies outside the range, and answers the distance with the spread.
* Gives the speed of the spindle from the pulses of a sensor (any number per turn), zero when the pulses stop, refusing the spikes of the drive and mending a missed pulse.
* Reads the temperature (DS18B20, NTC or infrared) with a warning, a critical level and a hysteresis, and counts a sensor that is switched on and does not answer as hot.
* Drives the protect output: low whenever the spindle must stop (too fast, too hot, a silent sensor) and while the node is not running, to be wired into the stop line of the control unit; the task that writes it is watched.
* Streams the camera (MJPEG, RTSP over UDP or inside the TCP connection with Digest and Basic login, and ONVIF discovery and services) and lights the ring with its curve, its ceiling and its timer.
* Keeps the settings file in its own partition with safe defaults, updates from the release files of its repository after the SHA-256 agrees, and publishes a contract that the server is tested against.

## What it will not do

* No stop of the machine by itself: the protect output opens the stop line of the control unit through a relay or an opto-coupler, and the control unit and the person stop the machine (see below).
* No signed update: the update checks the SHA-256 that the release publishes.
* No measure that was not taken: the drivers of the laser, the temperature sensors and the camera have never run on a board.
* No change of the safe defaults without an administrator.

## Where it sits

* **It reads:** the laser (VL53L1X), the pulses of the speed sensor, the temperature sensors (DS18B20, NTC, MLX90614), the camera, its settings in flash and the requests of the network.
* **It writes:** the protect output, the light ring, the video streams and the answers of the web panel and of the contract; its settings in flash.
* **It is read by:** EH3D-SERVER and the programs of the Bungard workflow, which turn a measure into a point of the height map and show the head.
* **It depends on:** ESP-IDF 5.5.5 for the firmware, a C++17 compiler and CMake for the host tests, and the shared base of the nodes of the suite.

## What it never decides

The limits of a machine, its interlocks and its emergency stop belong to the machine and to its control unit, not to this node. The protect output is a request to stop that has to be wired into the stop line, and a measure of the table or of the speed is a reading, not a verified fact: nothing here has run on a board, so a person must check what it reports before anything depends on it.

The requirements of the program, with the state of each and the test that shows it, are in `REQUIREMENTS.md`; the state in a paragraph is in `STATUS.md`.
