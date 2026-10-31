<p align="center">
  <img src="images/EH3D_BANNER.svg" alt="EH3D-NODE-BUNGARD-AUX banner" width="100%">
</p>

# 🎛️ EH3D-NODE-BUNGARD-AUX

<p align="center">🇺🇸 <b>English</b> | <a href="README_spa.md">🇪🇸 Español</a> | <a href="README_fra.md">🇫🇷 Français</a> | <a href="README_ita.md">🇮🇹 Italiano</a> | <a href="README_deu.md">🇩🇪 Deutsch</a> | <a href="README_zho.md">🇨🇳 中文</a> | <a href="README_jpn.md">🇯🇵 日本語</a> | <a href="README_por.md">🇵🇹 Português</a></p>

### A small node on the drill head of a Bungard CCD machine (ESP32-S3 with a wired network port): it sends the video of the camera, measures the height of the table with a laser, reads the speed and the temperature of the spindle and opens a protect output when the spindle is too fast or too hot.

<p align="center">
  <img src="https://img.shields.io/badge/License-GPL%203.0--or--later-blue.svg" alt="GPL 3.0 or later">
  <img src="https://img.shields.io/badge/Stack-C%2B%2B17%20%2B%20ESP--IDF-00599C.svg" alt="C++17 and ESP-IDF">
  <img src="https://img.shields.io/badge/Languages-8-00E5FF.svg" alt="Languages">
  <img src="https://img.shields.io/badge/Version-0.0.2-0f6cbd.svg" alt="Version">
  <img src="https://img.shields.io/badge/Maturity-scaffolding-FFB020.svg" alt="Maturity">
</p>

---

**Honesty check - what actually runs today:** the rules of the node are written in C++17 and tested on a computer: the settings file with its safe defaults, the measure of the height of the table from several laser readings (the odd ones are thrown away and a measure that spreads too much is refused), the speed of the spindle from the pulses of a sensor, the temperature with its warning, its critical level and a hysteresis, the protect output and the words the node says on the network (the status, the measure, the token). The ESP-IDF firmware (wired network, web server, camera, pulses of the spindle, protect output) is written and builds with ESP-IDF 5.5.5, but has never been flashed or run on a board; **the drivers of the laser (VL53L1X), of the temperature sensors (DS18B20, NTC, MLX90614) and of the camera (OV2640 or OV5640) are written but have never run on a board**, so no measure has ever been taken, and nothing has been connected to a machine.

---

> **Safety and security status:** this is not a safety system. The protect output is a signal for the stop line of the control unit, not a replacement for it: the stop that protects people and the machine is a hardware button that cuts the drive by itself. The output opens when the spindle is too fast or too hot, when a temperature sensor that is switched on does not answer, and when this node is not running; a node without a token refuses every request that measures or shows the camera. Do not rely on it until the whole chain has been tried on a real machine. The project is independent: it is not made, endorsed or backed by Bungard Elektronik, and product names appear only to say which machines it is meant to work with.

## 📋 At a glance

| | |
|---|---|
| Version | `0.0.2` |
| Maturity | Scaffolding |
| Tests | 11 host test programs |
| Stack | C++17, ESP-IDF 5.5, ESP32-S3, W5500 network |
| On real hardware | No: the firmware builds but has not been flashed, and nothing has been connected to a machine |

**Jump to:** [Overview](#-overview) · [Architecture](#-architecture) · [Quick start](#-quick-start) · [Testing](#-testing) · [Structure](#-repository-structure) · [Related](#-related-projects)

## 🚦 Project status
Each line is one part of this repository and the light says how far it is today. It is written from what the repository can show (tests, builds, runs), not from what is planned.

🟢 Done and verified · 🟡 Works, with known limits · 🟠 Partly done or not verified yet · 🔴 Not done or not working

| Area | State | What it means here |
|---|---|---|
| Software | 🟠 | The ESP-IDF project (wired network on a W5500, web server, camera, pulses of the spindle, protect output, the settings file in its own partition) is written and builds; it has not been flashed. |
| Tests | 🟢 | 11 host test programs pass (`ctest`): the settings, the measure of the laser, the speed and the temperature of the spindle, the protect output, the words of the network, the video (RTSP, ONVIF), the settings document of the head, the update from GitHub, the clock, what every node shares, the texts of the panel in eight languages and the stacks of the tasks. |
| Laser (table height) | 🟡 | The measure from several readings (median, odd readings thrown away, spread checked, range and offset) is tested; the driver of the VL53L1X is written but has never run, so no measure has been taken yet. |
| Spindle | 🟡 | Speed from the pulses of a sensor (any number per turn, stopped when the pulses stop), temperature with warning, critical level and hysteresis, and the protect output are tested; the drivers of the DS18B20, the NTC and the MLX90614 are written but have never run. |
| Camera | 🟠 | The MJPEG stream is written for the camera connector of the board and needs the pins of the board in menuconfig; it has not run. |
| Video (RTSP, ONVIF) | 🟡 | The RTSP session (OPTIONS, DESCRIBE, SETUP, PLAY, PAUSE, TEARDOWN, over UDP or inside the TCP connection, Digest and Basic login), the RTP payload of JPEG, the discovery and the SOAP services of ONVIF (profiles, encoders with their sizes, stream and snapshot addresses, the user token) are tested on a computer; no player or recorder has taken the video from a real board. |
| Real hardware | 🔴 | No board has run this firmware and nothing has been connected to a machine. |
| Documentation | 🟢 | README in eight languages, the settings reference and the hardware notes. |

## 🎯 Overview

* The **laser measure**: the node asks the sensor for several readings, throws away the odd ones (median and median absolute deviation), refuses a measure that spreads more than `laser_sigma_max_mm` or lies outside the range, and answers the distance to the table with the spread; the desktop programs turn it into a point of the map of the table (`Z x y height`, read by `eh3d-levelling`).
* The **spindle**: the speed comes from the pulses of a sensor (a hall sensor or a slotted disc, any number of pulses per turn) and is zero when the pulses stop; the temperature has a warning, a critical level and a hysteresis so the state does not flicker; a sensor that is switched on and does not answer counts as hot.
* The **protect output**: a GPIO that is low whenever the spindle must stop (too fast, too hot, a silent sensor) and also while the node is not running, to be wired into the stop line of the control unit through a relay or an opto-coupler; it can be switched off in the settings.
* The **camera and the network**: the video of the camera as MJPEG on `/stream`, and the readings on `/api/status`, over a wired network port (W5500 over SPI); the token protects the measure and the camera.
* The **settings**: every tuned value (the network, the camera, the laser, the spindle, the temperature, the protect output) is a setting of the node, kept in flash and edited on the web panel, never a number in the code; a value out of range is refused with the reason.
* The **video as an IP camera sends it**: RTSP (`rtsp://ADDRESS/stream1`, JPEG over RTP, over UDP or inside the TCP connection, with the Digest or Basic login) and ONVIF (WS-Discovery, so that recorders and managers find the node, and the services of profile S: profiles, encoders with the sizes the camera can do, the address of the stream and of the snapshot, the user token); both use the user and the password of the settings, and without a password nothing is served.
* **The panel and the update, as the other nodes have them:** the node has the web panel of the nodes of A.R.M.O.R. and of EH3D-NODE-BUNGARD (the menu in groups with the language and the sign-out at its foot; a Summary; a Wi-Fi page with up to three backup networks for when the main one is lost; a Head page that edits the settings of the camera, the streams, the laser and the spindle; Help; About; users; the log), the copy of one firmware slot over the other in both directions and the update from the newest GitHub release of this project (the SHA-256 that the release publishes is checked, the address must be the project's own and only an administrator asks for it; the releases are not signed). It shares its firmware base with EH3D-NODE-BUNGARD.

## 🧩 Role in EH3D

EH3D-NODE-BUNGARD-AUX is the node of the drill head of a Bungard CCD machine, beside EH3D-NODE-BUNGARD (the node that drives the machine) and EH3D-NODE-BUNGARD-HMI (the screen). It sits on the head, sees the board with its camera, measures the table under the head and watches the spindle. The desktop programs (EH3D-BUNGARD-GUI, DRILLPRO, ROUTEPRO) use its measures to build the map of the table (`eh3d-levelling` in EH3D-COMMON) and correct the Z of the jobs; EH3D-BUNGARD-GUI asks the node directly or through EH3D-SERVER (the window of the head node).

## 🏛️ Architecture

<p align="center">
  <img src="images/architecture.svg" alt="EH3D-NODE-BUNGARD-AUX architecture: who uses it, what it holds and what it depends on" width="100%">
</p>

How the parts of this repository fit together: who talks to it, what it holds and what it depends on. The picture is drawn from `docs/architecture.json` and checked against the code.

## 🖼️ Screenshots

<p align="center">
  <img src="images/screenshots/answers.png" alt="what the node answers" width="100%">
</p>

<p align="center"><em>What the node answers on the network: the status (`GET /api/status`), a measure of the table (`POST /api/measure`) and the line that the desktop programs read as a point of the map. A drawing of the formats that the tests check, not a capture from a board.</em></p>

<p align="center">
  <img src="images/screenshots/panel-overview.png" alt="panel-overview" width="100%">
</p>

<p align="center"><em>The Summary page: the node, its hardware, its clock, its flash with the two firmware slots and its network.</em></p>

<p align="center">
  <img src="images/screenshots/panel-status.png" alt="panel-status" width="100%">
</p>

<p align="center"><em>The Status page, in the Forge theme of the other EH3D programs.</em></p>

<p align="center">
  <img src="images/screenshots/panel-head.png" alt="panel-head" width="100%">
</p>

<p align="center"><em>The Head page: the camera, the streams, the laser, the spindle sensors and the protect output (the passwords and the token are write-only).</em></p>

<p align="center">
  <img src="images/screenshots/panel-wifi.png" alt="panel-wifi" width="100%">
</p>

<p align="center"><em>The Wi-Fi page: the network the node joins and up to three backup networks for when it is lost.</em></p>

<p align="center">
  <img src="images/screenshots/panel-update.png" alt="panel-update" width="100%">
</p>

<p align="center"><em>The Firmware page: the update from GitHub, the upload of a file and the two slots, with the copy of one over the other.</em></p>

<p align="center">
  <img src="images/screenshots/panel-help.png" alt="panel-help" width="100%">
</p>

<p align="center"><em>The Help page: a topic for each page of the panel.</em></p>

## ✨ Features

* **A measure you can trust or no measure:** several readings, the odd ones thrown away, and a refusal (with the reason) when the readings spread too much, are out of the range or are too few.
* **A spindle that is watched all the time:** speed from the pulses, temperature with a warning, a critical level and a hysteresis, and a protect output that opens by itself when something is wrong or the node is not running.
* **Video like an IP camera:** RTSP and ONVIF with a login, so any player, recorder or ONVIF manager can take it; and the camera as MJPEG on `/stream` behind the token.
* **Safe by default:** no token means no measure and no camera; every limit has a switch in the settings and starts in its conservative position.
* **Two cameras, the OV2640 and the OV5640:** the sensor says which one it is and the node gives the largest size that the settings allow and that sensor can do (the OV2640 up to 1600x1200, the OV5640 up to 2592x1944), reports it on `/api/status` (`camera_model`) and in the encoder options of ONVIF; `camera_model = auto | ov2640 | ov5640` in the Head page. The pins of the camera connector of the Waveshare ESP32-S3-ETH are the defaults of `menuconfig`.
* **Three temperature sensors:** a DS18B20 on one wire, an NTC thermistor on an analogue input (`ntc_series_ohm`, `ntc_r25_ohm`, `ntc_beta` in the Head page) or an MLX90614 infrared thermometer that looks at the spindle without touching it (on the I2C bus of the laser); the checksums and the conversions are tested on a computer.
* **A ring of LEDs around the camera:** eight RGB LEDs of the WS2812 kind on one wire light the area under the head so the camera sees well. White is the light for the camera; the levels follow the eye and never go over a ceiling; one side only gives a raking light; any colour can be asked, and the ring says how a check went (green, red blinking, a blue spot turning, amber, fast red). It turns itself off after a while. `GET` and `POST /api/light` and `GET /api/snapshot` serve the programs; the settings are in the panel and in `head.cfg` (`light_*`), and the pin in the menu of the firmware (`AUX_LIGHT_GPIO`).

## 🔒 Security & Safety

| Threat | Defense |
|---|---|
| A spindle that runs too fast or too hot | the protect output opens above `rpm_max` and at the critical temperature, with a hysteresis so it does not flicker; a temperature sensor that is switched on and silent counts as hot |
| The node loses power or crashes | the output is low until the node has checked the spindle and stays low while it is not running, so it must be wired as a stop that opens by itself |
| Somebody on the network measures or watches the workshop | without a token the node refuses the measure and the camera; the token is compared in constant time and is never shown or logged |
| A bad measure taken for a good one | odd readings are thrown away, a spread above `laser_sigma_max_mm`, a range or too few readings refuse the measure with the reason |
| A typing mistake in the settings | a value out of range or an unknown key is reported and the safe default stays; a critical temperature under the warning, or an upside-down range, falls back to the defaults |
| A secret in the firmware or in the repository | nothing secret is compiled in; the token is a setting in its partition |

**What is not defended:** the node is not a safety device. The protect output is only as good as the wiring that carries it to the stop line, and nothing here has been tried on a machine. The network port has no encryption (a plain HTTP port on the workshop network): keep it on a network of its own.

## 🔌 Interfaces

| Interface | What it is | Where |
|---|---|---|
| Settings | one document in flash, edited on the Head page of the web panel; the keys are the ones of `head.cfg.example` | `core/bungard_config.hpp`, `core/aux_config.hpp` |
| Status | `GET /api/status`: the node, the camera, the laser, the speed, the temperature, the protect output and the uptime as one line of JSON | `core/protocol.hpp` |
| Measure | `POST /api/measure[?samples=N]` with the token: the distance to the table, the spread and why a measure was refused | `core/laser.hpp`, `main/app_main.cpp` |
| Camera | `GET /stream` with the token: MJPEG | `main/app_main.cpp` |
| Pulses of the spindle | one GPIO (`AUX_RPM_GPIO` in menuconfig), rising edges | `main/sensors.cpp` |
| Protect output | one GPIO (`AUX_PROTECT_GPIO`), low when the spindle must stop | `main/sensors.cpp` |
| Laser and temperature | I2C for the laser and one-wire for the temperature sensor; the drivers are written and have never run (the thermometer MLX90614 shares the I2C bus, the DS18B20 uses one pin, the NTC an analogue input) | `main/sensors.cpp` |
| RTSP | `rtsp://ADDRESS:PORT/stream1`: OPTIONS, DESCRIBE, SETUP, PLAY, PAUSE, GET_PARAMETER, TEARDOWN; JPEG over RTP over UDP or interleaved in TCP; Digest or Basic login | `core/rtsp.hpp`, `core/rtsp_session.hpp`, `core/rtp_jpeg.hpp`, `main/stream.cpp` |
| ONVIF | WS-Discovery on the multicast address `239.255.255.250:3702`; SOAP on `/onvif/device_service` and `/onvif/media_service` (profile S: capabilities, services, profiles, encoders, stream and snapshot addresses); the snapshot on `/snapshot.jpg` | `core/onvif.hpp`, `main/stream.cpp` |

## ⚙️ Configuration

The settings are kept in flash as one document and are edited on the Head page of the web panel (the passwords and the token are write-only: the panel never shows them). The keys and their ranges are the ones listed in `head.cfg.example`; the file the Head page of earlier versions is no longer read.

| Setting | Default | Range | What it does |
|---|---|---|---|
| `token` | none | 8 - 64 | the access token; without one the node refuses the measure and the camera |
| `laser_samples` | `7` | 1 - 31 | readings of one measure (the median of the good ones) |
| `laser_offset_mm` | `0` | -50 - 50 | added to every distance: where the sensor sits above the tool tip |
| `laser_sigma_max_mm` | `0.15` | 0.01 - 5 | a measure whose readings spread more than this is refused |
| `rpm_pulses_per_rev` | `1` | 1 - 64 | pulses of the sensor for each turn of the spindle |
| `rpm_max` | `65000` | 1000 - 200000 | above this speed the protect output opens |
| `temp_sensor` | none | none ds18b20 ntc mlx90614 | the sensor of the temperature of the spindle |
| `temp_warn_c / temp_critical_c` | `55 / 70` | 20 - 200 | the warning and the level at which the protect output opens |
| `protect_output` | `on` | on / off | the protect output works; off only on the bench |
| `stream_user / stream_password` | `viewer / none` | 8 - 64 | the login of RTSP and ONVIF; without a password they serve nobody |
| `rtsp_port` | `554` | 1 - 65535 | the port of the RTSP server (the port of the web server is `port`) |

## 🚀 Quick start

You need a C++17 compiler and CMake for the host tests, and ESP-IDF 5.5.5 for the firmware (the ESP32-S3 target).

```bash
build.bat            # the host tests (Windows)  -  ./build.sh on Linux
python -I tools/ci_validate.py
```

The firmware builds with ESP-IDF 5.5.5: `idf.py set-target esp32s3 && idf.py menuconfig && idf.py build` makes the image (set the pins of your board under `EH3D Bungard AUX`), and the settings are made on the web panel once the node runs.

## 🧪 Testing

The host tests (`build.bat` or `./build.sh`, then `ctest`) are two programs and all of it passes: `test_aux` (103 checks) on the settings file (defaults, values, wrong values, limits that make no sense together), the measure of the laser (the median, the odd reading thrown away, the offset, readings marked bad, the range, no readings, a spread that is too large), the speed of the spindle (pulses per turn, a slowing spindle, stopped when the pulses stop), the temperature (the hysteresis, a sensor that fails), the protect output, and the words of the network (the request line, the routes, the token in the header or the query, the number of readings, the JSON of the status and of the measure, the line of a point of the map). The firmware is not covered: it builds, but the tests run on a computer. The second program, `test_stream` (100 checks), covers the video: the hashes of the logins against their published vectors (MD5, SHA-1, base64 and the Digest example of RFC 2617), the RTSP requests and the whole session (login, SDP, UDP and interleaved transport, play, pause, teardown, a wrong session, multicast refused), the RTP payload of JPEG (the tables in the first packet, the order, the marker, the sequence wrapping) and ONVIF (discovery, the user token with the example of the standard, the open and the protected calls, the services and a call that is not done). Besides those two programs, `test_head_settings` reads, checks and writes the head section of the settings document the way the panel does (the passwords and the token stay write-only), and the shared tests of the firmware base cover the update from GitHub, the clock and the users.

## 🏗️ Build & Release

The version lives in `eh3d.project.json`, and each version has its entry in [CHANGELOG.md](CHANGELOG.md). `build.sh` / `build.bat` build and run the host tests and `test.sh` / `test.bat` run the repository checks. The firmware is built with ESP-IDF 5.5.5 (`idf.py build`), which has not been done yet; no release has been published.

## 🩺 Troubleshooting

| Symptom | What to do |
|---|---|
| A measure is refused with `no_readings` | No VL53L1X answers on the I2C bus: check the pins SDA and SCL in `menuconfig` (the camera takes GPIO 41 and 42 on the Waveshare ESP32-S3-ETH) and the wiring; the rest of the answer is tested. |
| A measure is refused with `too_noisy` | The readings spread more than `laser_sigma_max_mm`: let the head and the table settle, or raise the limit a little; check that nothing shakes. |
| The node answers 401 to a measure or to the camera | Set `token` in the Head page (8 to 64 characters) and send it in the header `Authorization: Bearer ...` or in `?token=`. |
| The protect output is always open | A temperature sensor that is on and does not answer counts as hot: check its wire and its pin in `menuconfig` and `temp_sensor` in the Head page, or set `temp_sensor = none` on the bench. |

## 📂 Repository Structure

<p align="center">
  <img src="images/repository-structure.svg" alt="Repository structure of EH3D-NODE-BUNGARD-AUX" width="100%">
</p>

<details>
<summary>Text version of the tree</summary>

```text
EH3D-NODE-BUNGARD-AUX/
├── core/
│   ├── aux_config.hpp                           the settings file and its safe defaults
│   ├── bungard_config.hpp                       the settings of the node and the head section
│   ├── laser.hpp                                the measure of the height of the table
│   ├── light.hpp                                the ring of LEDs: levels, colours, signals
│   ├── camera.hpp                               the cameras OV2640 and OV5640 and their sizes
│   ├── spindle.hpp                              speed, heat and the protect output
│   ├── thermo.hpp                               the temperature: DS18B20, NTC, MLX90614
│   ├── hash.hpp                                 the logins: MD5, SHA-1, base64
│   ├── rtsp.hpp rtsp_session.hpp rtp_jpeg.hpp   RTSP and the RTP payload of JPEG
│   ├── onvif.hpp                                ONVIF: discovery and the SOAP services
│   ├── protocol.hpp                             the words of the network: routes, token, JSON
│   ├── node_settings.hpp auth.hpp               settings and users (shared base)
│   ├── netplan.hpp json.hpp                     network plan and JSON (shared base)
│   └── release_assets.hpp semver.hpp            which file of a release is this board's image
├── main/
│   ├── app_main.cpp                             start, settings, network, panel
│   ├── head.cpp head.hpp                        camera, laser, spindle, routes
│   ├── light.cpp light.hpp                      the driver of the ring of LEDs
│   ├── sensors.cpp sensors.hpp                  the pins: pulses, protect output, sensors
│   ├── stream.cpp stream.hpp                    RTSP server, ONVIF services and discovery
│   ├── idf_component.yml Kconfig.projbuild      component list and menuconfig pins
│   ├── web_server.cpp network.cpp               panel server, network (base)
│   ├── node_store.cpp node_guard.cpp            settings store, guard (base)
│   └── github_update.cpp entropy.cpp            the update from GitHub and the copy of a slot
├── panel/
│   ├── index.html style.css                     the page and the side menu layout
│   └── app.js text.js                           the pages and the texts in eight languages
├── tests/                                       host tests of the settings, the gate, the voice and the screen texts
├── docs/                                        the hardware notes and the two pictures' sources
├── tools/                                       CI check, README check and the picture tools
├── CMakeLists.txt                               the ESP-IDF project
├── partitions.csv                               the flash layout with two slots
├── head.cfg.example                             every setting with its default and range
├── sdkconfig.defaults                           ESP-IDF settings shared by both boards
├── sdkconfig.board.s3-eth                       settings of the wired board
├── sdkconfig.board.s3-wifi                      settings of the Wi-Fi board
├── dependencies.lock                            the exact versions of the components
├── images/                                      banner and the Architecture and Repository Structure pictures
├── .github/                                     CI and release workflows, issue and pull-request templates
├── build.sh build.bat                           build and run the host tests in a live panel
├── eh3d.project.json                            manifest: version, role, maturity and build command
├── CHANGELOG.md                                 what changed in each version
├── SECURITY.md SUPPORT.md                       how to report a vulnerability and where to ask
├── CONTRIBUTING.md CODE_OF_CONDUCT.md LICENSE   how to contribute, the code of conduct and the licence
└── bump_manifest_version.py                     version utility of the manifest
```

</details>

## 🗺️ Roadmap & Known limits

**Known limits today**

* Nothing has run on a board: the firmware builds but has not been flashed or tried, and the pins of the camera and of the network chip must be checked against the schematic of the board.
* The drivers of the laser (VL53L1X over I2C, through the component `grrtzm/vl53l1x_library`, little used), of the temperature sensors and of the camera are written and have never run: the first sensor that answers on a board may show a mistake.
* EH3D-BUNGARD-GUI (the window of the head node) and EH3D-SERVER ask the node, directly or through the server; neither has been tried against a real node.
* The network port is plain HTTP: keep it on a network of its own.
* RTSP and ONVIF have not been tried with a real player, recorder or ONVIF manager; the model of the camera on the board will decide the sizes and the pins.

**Next**

* Build the firmware and try the drivers on a board.
* Add the link to EH3D-BUNGARD-GUI, to EH3D-SERVER and to the screen, and the camera view.
* Try the protect output wired into a stop line on a machine.

## 🌍 Languages

README and texts exist in English, Spanish, French, Italian, German, Chinese, Japanese and European Portuguese.

## 🔗 Related Projects

* [EH3D-BUNGARD-GUI](https://github.com/JuanenRac/EH3D-BUNGARD-GUI): the modern interface that uses the measures to build the map of the table
* [EH3D-NODE-BUNGARD](https://github.com/JuanenRac/EH3D-NODE-BUNGARD): the node on the serial port of the control unit that drives the machine
* [EH3D-NODE-BUNGARD-HMI](https://github.com/JuanenRac/EH3D-NODE-BUNGARD-HMI): the touch screen of the operator
* [EH3D-SERVER](https://github.com/JuanenRac/EH3D-SERVER): the center of the family: it will carry the node's answers to every client
* [EH3D-COMMON](https://github.com/JuanenRac/EH3D-COMMON): the common parts of the family, among them `eh3d-levelling` (the height map of the table)

## 📚 Documentation

* The settings: `head.cfg.example` lists every key with its default and its range.
* The hardware: the pins, the wiring of the protect output and the parts, in `docs/HARDWARE.md`.
* [Changelog](CHANGELOG.md)

## 🤝 Contributing & Community

* Before a pull request run the host tests (`./build.sh` or `build.bat`) and `python -I tools/ci_validate.py`: all must pass. See [CONTRIBUTING.md](CONTRIBUTING.md) and the [code of conduct](CODE_OF_CONDUCT.md).
* The README exists in eight languages: a change to the text is made in all eight files, and the two pictures are drawn from `docs/architecture.json` and `docs/structure.json` by `tools/make_architecture.py` and `tools/make_structure.py`.
* Nothing that lets the node report a measure or open the protect output differently changes without a test that covers it and a note of what was seen on hardware.
* Report a vulnerability privately: [SECURITY.md](SECURITY.md). Questions, ideas and reports: electrohobby3d@gmail.com ([SUPPORT.md](SUPPORT.md)).

## 👤 AUTHOR

**JuanenRac (Electro Hobby 3D)** · electrohobby3d@gmail.com

## 📜 LICENSE

Software: GPL-3.0-or-later. See [LICENSE](LICENSE). Hardware designs of the suite use CERN OHL-S.
