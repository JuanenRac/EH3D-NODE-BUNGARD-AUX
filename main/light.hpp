// EH3D-NODE-BUNGARD-AUX - main/light.hpp: the ring of LEDs around the camera (WS2812, one wire). GPL-3.0-or-later.
#pragma once
#include <string>

#include "../core/aux_config.hpp"

namespace light {

/// Starts the driver on the GPIO of the menu (-1: no ring wired) and the task that shows the light and the cues. Without a wired ring nothing is started and every request is refused.
void start(const bungard_aux::Settings& s);

/// A ring is wired and the settings switch it on.
bool present();

/// The ring is shining now (without counting a cue).
bool is_on();
unsigned level();

/// Applies a request like `on=1&level=70&color=ff8800&cue=ok` and says what came of it: the HTTP status and the JSON of the answer.
int apply(const std::string& query, std::string& body);

/// The light as the programs read it.
std::string json();

}  // namespace light
