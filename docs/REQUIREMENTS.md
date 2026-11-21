# EH3D-NODE-BUNGARD-AUX: requirements

This register is the single source of the state of each capability: `STATUS.md` and the README files cite it and never state a state that it does not. A state is one of `planned`, `prototype`, `implemented` and `validated`. `validated` means that a person has run the capability on a real machine and the evidence of the manifest says `bench` or `field`; nothing here is validated, because the drivers of the laser, of the temperature sensors and of the camera have never run on a board, no measure has ever been taken, and nothing has been connected to a machine.

The register has 39 requirements: 18 implemented, 15 prototype, 6 planned and 0 validated. By progress: 18 done, 15 partial, 1 pending and 5 blocked. High coverage is not proof of correctness, so each row says how it is checked: the test that names it, and what it shows.

## How to read the register

* **ID:** `NBAX-NNN`, never reused and never renumbered; a new capability takes the next number.
* **State:** `implemented` is code that is complete and tested on a computer, `prototype` is part of the capability, `planned` is not written.
* **Progress:** `done`, `partial`, `pending` (can be written without hardware) or `blocked` (waits for hardware or for another program of the suite).
* **Evidence:** `simulator` when the test runs against a model of the sensor, the pulses or the head in memory, `host-test` when it checks a calculation, a reader or a screen-less piece, `none` when nothing is written yet.
* **Test:** the acceptance criterion: `file::name` is the test function `name` of `tests/file.cpp` (it must be called from its `main`); a name with a path and `.py`, such as `tools/file.py::name`, is a test of a Python file. The test must exist. For a capability that is not written the cell says `planned:` and the name of the test that will show it; the day that test exists, the row must change.
* **Limits:** what is not done or not proven; a row that is `partial` or `blocked` always says it.

The check is `tools/test_requirements.py`: it reads this table, finds each test and fails when a state, a test or a count does not agree.

## The laser measure

| ID | Requirement | State | Progress | Evidence | Test | Limits |
|---|---|---|---|---|---|---|
| NBAX-001 | The measure takes several readings, throws away the odd ones and answers the distance with the spread. | prototype | partial | simulator | `test_aux::laser_measure` | Against a model of the sensor; the driver of the VL53L1X has never run on a board. |
| NBAX-002 | Odd readings are thrown away. | prototype | partial | simulator | `test_laser::odd_readings` | Model of the sensor only. |
| NBAX-003 | The range status of a reading is read. | prototype | partial | simulator | `test_laser::range_status` | Model of the sensor only. |
| NBAX-004 | The calibration offset is applied to the measure. | prototype | partial | simulator | `test_laser::calibration_offset` | No calibration has been made on a real table. |
| NBAX-005 | The settings of the laser are read within their ranges. | implemented | done | host-test | `test_laser::settings_of_the_laser` | - |

## Spindle speed and temperature

| ID | Requirement | State | Progress | Evidence | Test | Limits |
|---|---|---|---|---|---|---|
| NBAX-006 | The speed is read from pulses between 400 Hz and 1 kHz. | prototype | partial | simulator | `test_rpm::speeds_from_400_hz_to_1_khz` | Model of the pulses; no sensor has been read on a board. |
| NBAX-007 | The spikes of the drive are refused. | prototype | partial | simulator | `test_rpm::spikes_of_the_variador_are_refused` | Model of the pulses only. |
| NBAX-008 | An overspeed is read as an overspeed. | prototype | partial | simulator | `test_rpm::an_overspeed_is_read_as_an_overspeed` | Model of the pulses only. |
| NBAX-009 | A missed pulse is mended. | prototype | partial | simulator | `test_rpm::a_missed_pulse_is_mended` | Model of the pulses only. |
| NBAX-010 | The ring of pulses survives the wrap of its index. | implemented | done | host-test | `test_rpm::the_ring_of_pulses_survives_the_wrap_of_its_index` | - |
| NBAX-011 | The temperature has a warning, a critical level and a hysteresis, and a silent sensor counts as hot. | prototype | partial | simulator | `test_aux::thermal_state` | Model of the sensors; no sensor has been read on a board. |
| NBAX-012 | The checksums of the one-wire bus and of the SMBus packet are checked. | prototype | partial | host-test | `test_more::the_crc_of_the_one_wire_bus_and_the_smbus_packet_check` | The drivers have never run on a board. |
| NBAX-013 | The sensor of the spindle is chosen among those on the one-wire bus. | prototype | partial | simulator | `test_onewire::the_spindles_sensor_is_chosen` | Model of the bus only. |
| NBAX-014 | The infrared thermometer has a range and an error flag. | prototype | partial | host-test | `test_more::the_infrared_thermometer_has_a_range_and_an_error_flag` | The driver has never run on a board. |

## The protect output

| ID | Requirement | State | Progress | Evidence | Test | Limits |
|---|---|---|---|---|---|---|
| NBAX-015 | The protect output is low whenever the spindle must stop and also while the node is not running. | prototype | partial | simulator | `test_aux::protect_output` | Model of the output; it has never been wired to a control unit. |
| NBAX-016 | Each reason to stop (too fast, too hot, a silent sensor) is reported as its own reason. | prototype | partial | simulator | `test_aux::protect_reasons` | Model only. |
| NBAX-017 | The task that writes the output is expected by the node guard, and a hang restarts the chip. | prototype | partial | host-test | `test_base::test_node_guard_rules` | A model of the watchdog; never on the chip. |

## Video and light

| ID | Requirement | State | Progress | Evidence | Test | Limits |
|---|---|---|---|---|---|---|
| NBAX-018 | The RTSP session answers its requests in order. | implemented | done | host-test | `test_stream::rtsp_flow` | Against a model of the client, not against a player. |
| NBAX-019 | The RTP payload of a JPEG carries every byte once in order. | implemented | done | host-test | `test_more::the_packets_carry_every_byte_once_in_order_and_say_where_the_picture_ends` | - |
| NBAX-020 | A cut or padded JPEG is read up to the last end marker. | implemented | done | host-test | `test_more::a_cut_or_padded_jpeg_is_read_up_to_the_last_end_marker` | - |
| NBAX-021 | The ONVIF services answer with profiles, encoders and addresses. | implemented | done | host-test | `test_stream::onvif` | Against a model of the client, not against an ONVIF client. |
| NBAX-022 | MD5, SHA-1 and Base64 give the known answers of the standard. | implemented | done | host-test | `test_more::md5_known_answers_of_the_standard` | - |
| NBAX-023 | The ceiling of the light holds. | implemented | done | host-test | `test_light::the_ceiling_holds` | - |
| NBAX-024 | The timer of the light ends it. | implemented | done | host-test | `test_light::the_timer` | - |

## Settings, contract and the shared base

| ID | Requirement | State | Progress | Evidence | Test | Limits |
|---|---|---|---|---|---|---|
| NBAX-025 | The settings file keeps its safe defaults. | implemented | done | host-test | `test_aux::settings_file` | - |
| NBAX-026 | The words the node says on the network are the documented ones. | implemented | done | host-test | `test_aux::words_of_the_network` | - |
| NBAX-027 | The firmware serves the routes that the node decides, and the contract page says every route. | implemented | done | host-test | `tools/test_server_contract.py::test_the_firmware_serves_the_routes_the_node_decides` | - |
| NBAX-028 | The token goes the way the node reads it. | implemented | done | host-test | `tools/test_server_contract.py::test_the_token_goes_the_way_the_node_reads_it` | - |
| NBAX-029 | The range of the readings of a measure is the same on both sides. | implemented | done | host-test | `tools/test_server_contract.py::test_the_range_of_the_readings_of_a_measure_is_the_same` | - |
| NBAX-030 | A write without the active mode is refused by the check of the node. | implemented | done | host-test | `tools/test_selftest.py::test_write_without_active_is_refused` | Against a mock node. |
| NBAX-031 | An active check that fails still puts the light back. | implemented | done | host-test | `tools/test_selftest.py::test_active_failure_still_puts_the_light_back` | Against a mock node. |
| NBAX-032 | The login throttle follows the rules of the shared base. | implemented | done | host-test | `test_shared::test_login_throttle` | - |
| NBAX-033 | The image picked for an update is the one with a version for this board. | implemented | done | host-test | `test_release::picking_the_image` | - |

## Not written yet

| ID | Requirement | State | Progress | Evidence | Test | Limits |
|---|---|---|---|---|---|---|
| NBAX-034 | The firmware is flashed on a board and the web panel answers. | planned | blocked | none | `planned:on_board::the_panel_answers_on_a_flashed_board` | Needs a board. |
| NBAX-035 | The laser takes a measure of a real table. | planned | blocked | none | `planned:real_laser::a_real_table_is_measured` | Needs the laser on a board and a table. |
| NBAX-036 | The speed and the temperature of a real spindle are read. | planned | blocked | none | `planned:real_spindle::the_speed_and_the_temperature_of_a_spindle_are_read` | Needs the sensors, a board and a spindle. |
| NBAX-037 | The protect output opens the stop line of a real control unit when the spindle is too hot. | planned | blocked | none | `planned:real_protect::the_output_opens_the_stop_line` | Needs the wiring, a control unit and a person ready to stop the machine. |
| NBAX-038 | The camera streams to a player and to an ONVIF client. | planned | blocked | none | `planned:real_camera::the_camera_streams_to_a_player` | Needs the camera, a board and a player. |
| NBAX-039 | The update is signed and the node verifies the signature. | planned | pending | none | `planned:signed_update::the_node_refuses_an_update_with_a_wrong_signature` | Not written; the signing key is missing. |
