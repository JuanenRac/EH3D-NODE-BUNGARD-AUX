// EH3D-NODE-BUNGARD-AUX - main/stream.hpp: the video as an IP camera sends it (RTSP and ONVIF). GPL-3.0-or-later.
#pragma once

#include "esp_http_server.h"

#include "../core/aux_config.hpp"

namespace stream {

/// Starts the RTSP server and the ONVIF discovery, as the settings say. Call it after the network and the camera are up.
void start(const bungard_aux::Settings& s);

/// Adds the SOAP services of ONVIF and the snapshot to the web server (call it after `start`).
void register_http(httpd_handle_t h);

}  // namespace stream
