# EH3D-NODE-BUNGARD-AUX: status

## Current state

EH3D-NODE-BUNGARD-AUX has its rules written and tested on a computer: the settings file with its safe defaults, the measure of the table from several readings, the speed from the pulses, the temperature with its levels, the protect output and its reasons, the video (RTSP, RTP of JPEG and ONVIF), the light ring, the settings document of the head, the update from GitHub, the clock and what every node shares. The ESP-IDF firmware is written and builds, but it has never been flashed or run on a board; the drivers of the laser, the temperature sensors and the camera are written and have never run, so no measure has ever been taken.

## Capabilities

The official state of each capability is in `REQUIREMENTS.md`. The register has 39 requirements: 18 implemented, 15 prototype, 6 planned and 0 validated. By progress: 18 done, 15 partial, 1 pending and 5 blocked. This page never states a state that `REQUIREMENTS.md` does not.

## Open questions

* Which pins of the camera connector the board uses, which the firmware takes from menuconfig.
* Whether the protect output should also be able to be inverted for a control unit whose stop line is active high.

## What it needs from other repositories

* A board with the laser, the temperature sensors, the pulse sensor and the camera, for anything that is not tried on a computer.
* A Bungard CCD machine with its control unit, and a person at it, to wire and try the protect output.
* `EH3D-SERVER`, which reads the contract and the measures of this node.

## Known technical debt and omitted tests

* The firmware has never run on a board: the timing of the laser, of the pulses and of the one-wire bus is untried.
* The camera needs the pins of the board in menuconfig and has not run.
* The panel has been exercised in a browser against a mock node, not against a real one.
