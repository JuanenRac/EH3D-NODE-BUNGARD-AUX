// EH3D-NODE-BUNGARD-AUX - main/head.hpp: what is only the head's (the camera, the laser, the spindle sensors, the protect output). GPL-3.0-or-later.
#pragma once
#include <string>

#include "esp_http_server.h"

#include "../core/aux_config.hpp"

namespace head {

/// Starts the sensors, the camera, the streams (RTSP and ONVIF) and the watch of the spindle, with the settings of the head.
void start(const bungard_aux::Settings& settings);

/// Adds the routes of the programs (`/api/status`, `/api/measure`, `/stream` and the ONVIF services) to a web server; call it before the wildcard routes of the panel.
void register_routes(httpd_handle_t server);

/// What the node says about itself, the way a program reads it, and one measure of the table (1 to 31 readings), as JSON.
std::string live_json();
std::string measure_json(int samples);

/// The calibration of the laser (kind "offset", "xtalk" or "reset"; `target_mm` is the distance of the flat target). Refused with "disabled" unless the settings allow it. `found` is true
/// when it worked, and the new values are then also stored in the settings of the node (`offset_mm` in mm, `xtalk_cps` in counts per second).
std::string calibrate_json(const std::string& kind, double target_mm, bool& found, double& offset_mm, double& xtalk_cps);

/// Stores the values a calibration found in the settings of the node (they are applied at once and kept over restarts).
void store_calibration(double offset_mm, double xtalk_cps);

}  // namespace head
