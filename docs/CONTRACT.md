# What the node says on the network

The programs of the machine (the server, the desktop programs, the phones) read the head through three requests. The words below are built in `core/protocol.hpp`, the judgement of the node on itself in `core/self_check.hpp`, and `core/head_sim.hpp` builds the very same answers for a head that is not there. A host test (`tests/test_head_sim.cpp`) reads this page and fails when a key or a code is in the page and not in the answer, or the other way round, so the page cannot go stale.

Nothing in this page has been checked against a head on a machine: it is checked against the simulator.

## `GET /api/status`

Always answered, with or without the token. One line of JSON; a program must ignore a key it does not know.

| Key | Type | Meaning |
|---|---|---|
| `node` | text | the name of the node |
| `version` | text | the version of the firmware |
| `camera` | bool | the camera is on and started |
| `camera_model` | text | `ov2640`, `ov5640` or empty |
| `laser` | bool | the laser is switched on in the settings |
| `rpm_sensor` | bool | the speed sensor is switched on in the settings |
| `rpm` | number | revolutions per minute |
| `temp_c` | number or null | the temperature of the spindle; null without a reading |
| `heat` | text | `unknown`, `ok`, `warm` or `hot` |
| `protect_open` | bool | the protect output is open: the spindle must not run |
| `uptime_s` | number | seconds since the start |
| `light` | bool | the ring of LEDs was found |
| `light_on` | bool | the ring is shining |
| `light_level` | number | 0 to 100 |
| `laser_mode` | text | `short` or `long` |
| `laser_budget_ms` | number | timing budget of the sensor |
| `laser_roi` | text | the receiving area, `WxH` |
| `laser_cal_offset_mm` | number | the offset found by the calibration |
| `laser_cal_xtalk_cps` | number | the crosstalk found by the calibration |
| `laser_cal_allowed` | bool | the calibration is allowed in the settings |
| `laser_last_mm` | number | the last reading of the laser, with the offsets |
| `laser_last_status` | text | `valid`, `sigma_high`, `signal_low`, `out_of_range`, `sensor_fault`, `other` or `none` |
| `laser_age_ms` | number | how old the last reading is; -1 when there is none |
| `temp_roms` | text | the DS18B20 found on the wire, separated by commas |
| `temp_rom_used` | text | the one that is read |
| `temp_resolution` | number | bits of the conversion |
| `temp_guess` | bool | the sensor was a guess among several |
| `rpm_input` | text | `digital` or `analog` |
| `rpm_quality` | text | `none`, `good` or `noisy` |
| `rpm_rejected` | number | pulses refused as noise |
| `rpm_level_mv` | number | analogue input: the level |
| `rpm_th_hi_mv` | number | analogue input: the high threshold |
| `rpm_th_lo_mv` | number | analogue input: the low threshold |
| `selfcheck` | text | `ok`, `warn`, `fail` or `off` (see below) |
| `selfcheck_codes` | text | the codes behind the verdict, separated by commas; empty when there are none |

The server needs `node`, `camera`, `laser`, `rpm_sensor` and `protect_open` (it refuses a status without them); every other key may be missing.

## `POST /api/measure[?samples=N]`

Needs the token. One measure of the height of the table under the head.

| Key | Type | Meaning |
|---|---|---|
| `ok` | bool | the measure can be used |
| `why` | text | `ok`, `no_readings`, `out_of_range`, `too_few` or `too_noisy` |
| `distance_mm` | number | the median of the readings kept, with the offsets |
| `sigma_mm` | number | their spread |
| `asked` | number | readings asked for |
| `used` | number | readings kept |
| `rejected` | number | odd readings thrown away |
| `refused` | number | readings the sensor itself refused |
| `status` | text | the commonest reason the sensor gave for refusing |
| `status_code` | number | and its number |

## The self-check

The node judges its own status against its settings and says it in `selfcheck` and `selfcheck_codes`. It never touches the hardware to do so. The limits are settings of `head.cfg`: `selfcheck_enabled`, `selfcheck_laser_age_max_s` and `selfcheck_grace_s` (for that many seconds after the start a reading that is not there yet is not a fault).

The verdict is `fail` when any code of severity `fail` is raised, `warn` when the worst is `warn`, `ok` when nothing above `info` is raised, and `off` when the check is switched off. A code is never reused for another meaning.

| Code | Severity | Raised when | Setting to look at |
|---|---|---|---|
| `AUX-C01` | fail | the camera is switched on but did not start | `camera_enabled` |
| `AUX-L01` | info | the laser has not given a reading since the start | `laser_enabled` |
| `AUX-L02` | fail | the laser sensor reports a fault | `laser_enabled` |
| `AUX-L03` | warn | the last laser reading is older than the limit | `selfcheck_laser_age_max_s` |
| `AUX-L04` | warn | the last laser reading had a weak signal or a high spread | `laser_sigma_max_mm` |
| `AUX-L05` | info | the calibration of the laser is allowed | `laser_calibration_enabled` |
| `AUX-R01` | warn | the pulses of the speed sensor are noisy | `rpm_input` |
| `AUX-R02` | fail | the spindle turns faster than `rpm_max` | `rpm_max` |
| `AUX-T01` | fail | the temperature sensor is on and gives no reading | `temp_sensor` |
| `AUX-T02` | warn | the spindle is warm | `temp_warn_c` |
| `AUX-T03` | fail | the spindle is hot | `temp_critical_c` |
| `AUX-T04` | warn | several temperature sensors on the wire and none chosen | `temp_rom` |
| `AUX-P01` | warn | the protect output is switched off in the settings | `protect_output` |
| `AUX-P02` | warn | the protect output is open: the spindle must not run | |
| `AUX-S01` | warn | the ring of LEDs is switched on in the settings but was not found | `light_enabled` |
| `AUX-S02` | warn | the node has no token: it refuses every command that changes something | `token` |
| `AUX-S03` | info | RTSP or ONVIF is on and has no password: nobody is served | `stream_password` |

## The simulator

`core/head_sim.hpp` is a head on a computer: a table that is not flat under a laser, a spindle that speeds up and heats, a ring of LEDs and a camera, with faults that can be switched on (camera down, laser fault or weak, noisy pulses, missing temperature reading, ring not found). `answer("GET /api/status")` and `answer("POST /api/measure?samples=5")` give the bodies the firmware would send. The numbers of the table, of the noise and of the heating are made up for the simulation. `tests/head_sim_cli.cpp` is a small program that reads request lines and prints the answers, for a program that wants to read a head that is not there.
