// EH3D-NODE-BUNGARD-AUX - host tests: every setting of the head survives the trip to flash and back, and the example file is the defaults.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
#include <cstdio>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "../core/node_config.hpp"

using namespace eh3d;
static int fails = 0;
static int checks = 0;
#define CHECK(x) do { ++checks; if (!(x)) { std::printf("FAIL line %d: %s\n", __LINE__, #x); ++fails; } } while (0)

// The text of a source file of the node, found from the folder the test runs in.
static std::string source(const std::string& rel) {
  for (const char* up : {"", "../", "../../", "../../../"}) {
    std::ifstream in(std::string(up) + rel, std::ios::binary);
    if (!in) continue;
    std::ostringstream all;
    all << in.rdbuf();
    return all.str();
  }
  return std::string();
}

// The keys the parser of the settings accepts: the `key == "name"` tests of core/aux_config.hpp.
static std::set<std::string> parser_keys() {
  std::set<std::string> keys;
  const std::string text = source("core/aux_config.hpp");
  const std::string needle = "key == \"";
  for (std::size_t at = text.find(needle); at != std::string::npos; at = text.find(needle, at + 1)) {
    const std::size_t from = at + needle.size();
    const std::size_t to = text.find('"', from);
    if (to != std::string::npos) keys.insert(text.substr(from, to - from));
  }
  return keys;
}

// The keys a `key = value` file names (comments and blank lines skipped).
static std::vector<std::string> file_keys(const std::string& text) {
  std::vector<std::string> keys;
  std::istringstream in(text);
  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    const std::size_t a = line.find_first_not_of(" \t");
    if (a == std::string::npos || line[a] == '#') continue;
    const std::size_t eq = line.find('=');
    if (eq == std::string::npos) continue;
    std::size_t b = eq;
    while (b > a && (line[b - 1] == ' ' || line[b - 1] == '\t')) --b;
    keys.push_back(line.substr(a, b - a));
  }
  return keys;
}

// The settings of the head with a value that is not the default for every key a person can change in the panel.
static const char* const kChanged =
    "camera_enabled = off\ncamera_model = ov5640\ncamera_width = 1280\ncamera_height = 720\ncamera_quality = 20\ncamera_fps_max = 15\n"
    "camera_hmirror = on\ncamera_vflip = on\ncamera_brightness = -1\ncamera_contrast = 2\ncamera_saturation = -2\ncamera_auto_exposure = off\ncamera_exposure = 640\ncamera_auto_whitebalance = off\n"
    "rtsp_enabled = off\nrtsp_port = 8554\nonvif_enabled = off\nstream_user = cam\nstream_password = longpassword\n"
    "laser_enabled = off\nlaser_samples = 9\nlaser_timeout_ms = 2500\nlaser_offset_mm = -1.5\nlaser_range_min_mm = 30\nlaser_range_max_mm = 300\nlaser_sigma_max_mm = 0.2\n"
    "laser_distance_mode = short\nlaser_timing_budget_ms = 50\nlaser_roi = 8x8\nlaser_roi_center = 199\nlaser_outlier_k = 2.5\nlaser_outlier_floor_mm = 0.1\n"
    "laser_calibration_enabled = on\nlaser_cal_offset_mm = 0.75\nlaser_cal_xtalk_cps = 1200\n"
    "rpm_enabled = off\nrpm_pulses_per_rev = 4\nrpm_input = analog\nrpm_edge = falling\nrpm_auto_threshold = off\nrpm_threshold_hi_mv = 2400\nrpm_threshold_lo_mv = 800\nrpm_max = 50000\nrpm_stale_ms = 2000\n"
    "temp_sensor = ds18b20\ntemp_resolution = 10\ntemp_rom = 28FF0102030405A6\nntc_series_ohm = 4700\nntc_r25_ohm = 4700\nntc_beta = 3380\ntemp_warn_c = 50\ntemp_critical_c = 65\ntemp_hysteresis_c = 2\n"
    "light_enabled = off\nlight_leds = 12\nlight_level = 40\nlight_max_level = 70\nlight_tone = warm\nlight_auto_off_s = 300\nlight_on_at_start = on\nlight_order = rgb\n"
    "protect_output = off\ntoken = abcdefgh12\n";

int main() {
  const std::set<std::string> keys = parser_keys();
  CHECK(keys.size() > 60);  // the scan found the table

  // every key the parser takes (but the three of the node itself) is a member of the head section written to flash and to the panel
  const std::string written = config::head_to_json(bungard_aux::Settings{}, true);
  json::Value doc;
  CHECK(json::parse(written, doc));
  for (const std::string& key : keys) {
    if (key == "name" || key == "address" || key == "port") continue;  // the node's own name, address and port live in the node section
    if (doc.get(key) == nullptr) { std::printf("  not written to flash: %s\n", key.c_str()); }
    CHECK(doc.get(key) != nullptr);
  }

  // the example file names every key once, with no key twice and none the parser does not know, and reads with no problem
  const std::string example = source("head.cfg.example");
  CHECK(!example.empty());
  std::set<std::string> seen;
  for (const std::string& key : file_keys(example)) {
    CHECK(keys.count(key) == 1);
    CHECK(seen.insert(key).second);
  }
  for (const std::string& key : keys) {
    if (seen.count(key) == 0) std::printf("  not in head.cfg.example: %s\n", key.c_str());
    CHECK(seen.count(key) == 1);
  }
  std::vector<bungard_aux::Problem> found;
  const bungard_aux::Settings from_example = bungard_aux::parse_settings(example, found);
  CHECK(found.empty());
  // the example is the defaults: the file written from it is the file written from the defaults (the secrets and the node's own keys are empty in both)
  CHECK(config::head_to_json(from_example, true) == written);
  CHECK(from_example.name == bungard_aux::Settings{}.name && from_example.port == bungard_aux::Settings{}.port && from_example.address.empty());

  // a trip through flash: the changed settings are written, read back on top of the defaults and written again, the same
  found.clear();
  const bungard_aux::Settings changed = bungard_aux::parse_settings(kChanged, found);
  CHECK(found.empty());
  CHECK(config::head_to_json(changed, true) != written);
  const std::string once = config::head_to_json(changed, true);
  json::Value back_doc;
  CHECK(json::parse(once, back_doc));
  bungard_aux::Settings back;
  config::Problems problems;
  config::head_read(back_doc, back, problems);
  CHECK(problems.empty());
  CHECK(config::head_to_json(back, true) == once);
  // and the values that used to be lost on the way are there
  CHECK(back.camera_hmirror && back.camera_vflip && back.camera_brightness == -1 && back.camera_contrast == 2 && back.camera_saturation == -2);
  CHECK(!back.camera_auto_exposure && back.camera_exposure == 640 && !back.camera_auto_whitebalance);
  CHECK(back.rpm_input == bungard_aux::RpmInput::Analog && back.rpm_edge == bungard_aux::RpmEdge::Falling && !back.rpm_auto_threshold);
  CHECK(back.rpm_threshold_hi_mv == 2400 && back.rpm_threshold_lo_mv == 800);
  CHECK(back.temp_resolution == 10 && back.temp_rom == "28FF0102030405A6");
  CHECK(back.token == "abcdefgh12" && back.stream_password == "longpassword");

  // the document for the panel never carries the secrets, whatever else it carries
  const std::string for_panel = config::head_to_json(changed, false);
  CHECK(for_panel.find("abcdefgh12") == std::string::npos && for_panel.find("longpassword") == std::string::npos);
  CHECK(for_panel.find("camera_hmirror") != std::string::npos && for_panel.find("rpm_input") != std::string::npos);

  std::printf("%d checks, %d failed\n", checks, fails);
  return fails == 0 ? 0 : 1;
}
