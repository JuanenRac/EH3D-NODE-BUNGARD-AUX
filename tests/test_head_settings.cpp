// EH3D-NODE-BUNGARD-AUX - host tests of the settings document of the node: the head's section read, checked and written the way the web panel does.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#include "../core/bungard_config.hpp"

using namespace eh3d;
static int fails = 0;
#define CHECK(x) do { if (!(x)) { std::printf("FAIL line %d: %s\n", __LINE__, #x); ++fails; } } while (0)

static bool has_problem(const config::Problems& problems, const std::string& path) {
  for (const auto& p : problems) if (p.path == path) return true;
  return false;
}

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

int main() {
  // this node does not announce itself as the node of the machine: not by its default network name, nor in the places that name it by hand
  CHECK(config::default_settings("a1b2c3").ap.ssid == "EH3D-NODE-BUNGARD-AUX");
  for (const char* rel : {"core/node_settings.hpp", "main/improv_serial.cpp", "main/web_server.cpp"}) {
    const std::string text = source(rel);
    if (text.empty()) { std::printf("FAIL cannot find %s\n", rel); ++fails; continue; }
    CHECK(text.find("\"EH3D-NODE-BUNGARD\"") == std::string::npos);
    CHECK(text.find("\"EH3D-NODE-BUNGARD-\"") == std::string::npos);
  }
  config::Settings base = config::default_settings("a1b2c3");
  base.sta.enabled = true; base.sta.ssid = "home"; base.sta.password = "home-password";   // a node needs a network to be valid
  config::Settings out;
  config::Problems problems;

  // a document with no head section keeps the defaults of the head
  CHECK(config::load("{}", base, out, problems));
  for (const auto& p : problems) std::printf("  problem %s %s\n", p.path.c_str(), p.code.c_str());
  CHECK(out.head.camera_enabled == base.head.camera_enabled && out.head.laser_samples == base.head.laser_samples);

  // the panel sends the head's section: booleans, numbers and texts are read by the rules of the file head.cfg always had
  problems.clear();
  const std::string sent = R"({"head":{"camera_enabled":true,"camera_model":"ov5640","camera_width":1280,"camera_height":720,"camera_quality":10,"camera_fps_max":15,
    "rtsp_enabled":true,"rtsp_port":8554,"laser_enabled":true,"laser_samples":9,"laser_offset_mm":1.5,"rpm_enabled":true,"rpm_pulses_per_rev":2,"rpm_max":24000,
    "temp_sensor":"ntc","temp_warn_c":50,"temp_critical_c":65,"protect_output":true}})";
  CHECK(config::load(sent, base, out, problems));
  CHECK(out.head.camera_enabled && out.head.camera_width == 1280 && out.head.camera_height == 720 && out.head.camera_quality == 10 && out.head.camera_fps_max == 15);
  CHECK(out.head.camera_model == bungard_aux::CameraModel::Ov5640);
  CHECK(out.head.rtsp_enabled && out.head.rtsp_port == 8554 && out.head.laser_enabled && out.head.laser_samples == 9 && out.head.laser_offset_mm == 1.5);
  CHECK(out.head.rpm_enabled && out.head.rpm_pulses_per_rev == 2 && out.head.rpm_max == 24000);
  CHECK(out.head.temp_sensor == bungard_aux::TempSensor::Ntc && out.head.temp_warn_c == 50.0 && out.head.temp_critical_c == 65.0);

  // a value out of its range is refused with the path of the field, and nothing is applied
  problems.clear();
  CHECK(!config::load(R"({"head":{"camera_width":99999}})", base, out, problems));
  CHECK(has_problem(problems, "head.camera_width"));
  problems.clear();
  CHECK(!config::load(R"({"head":{"laser_samples":0}})", base, out, problems));
  CHECK(has_problem(problems, "head.laser_samples"));
  problems.clear();
  CHECK(!config::load(R"({"head":{"temp_sensor":"thermometer"}})", base, out, problems));
  CHECK(has_problem(problems, "head.temp_sensor"));
  problems.clear();
  CHECK(!config::load(R"({"head":{"stream_user":"line\nbreak"}})", base, out, problems));
  CHECK(has_problem(problems, "head.stream_user"));
  problems.clear();
  CHECK(!config::load(R"({"head":[1,2]})", base, out, problems));

  // the passwords and the token are write-only: the panel's copy has only a flag, an empty value keeps the stored one and `_clear` erases it
  config::Settings secret = base;
  secret.head.token = "token-12345";
  secret.head.stream_password = "stream-pw";
  const std::string panel_copy = config::to_json(secret, false);
  CHECK(panel_copy.find("token-12345") == std::string::npos && panel_copy.find("stream-pw") == std::string::npos);
  CHECK(panel_copy.find("\"token_set\":true") != std::string::npos && panel_copy.find("\"stream_password_set\":true") != std::string::npos);
  const std::string stored_copy = config::to_json(secret, true);
  CHECK(stored_copy.find("token-12345") != std::string::npos && stored_copy.find("stream-pw") != std::string::npos);
  problems.clear();
  CHECK(config::load(R"({"head":{"token":"","token_set":true,"camera_enabled":false}})", secret, out, problems));
  CHECK(out.head.token == "token-12345" && out.head.stream_password == "stream-pw" && !out.head.camera_enabled);
  problems.clear();
  CHECK(config::load(R"({"head":{"token":"new-token"}})", secret, out, problems));
  CHECK(out.head.token == "new-token" && out.head.stream_password == "stream-pw");
  problems.clear();
  CHECK(config::load(R"({"head":{"token_clear":true,"stream_password_clear":true}})", secret, out, problems));
  CHECK(out.head.token.empty() && out.head.stream_password.empty());

  // the document that is stored loads back to the same head
  problems.clear();
  config::Settings round;
  CHECK(config::load(config::to_json(secret, true), base, round, problems));
  for (const auto& p : problems) std::printf("  problem %s %s\n", p.path.c_str(), p.code.c_str());
  CHECK(round.head.token == secret.head.token && round.head.camera_width == secret.head.camera_width && round.head.laser_range_max_mm == secret.head.laser_range_max_mm);

  // the laser: mode, budget, area and calibration are read from the panel's document, checked together, and written back the same
  problems.clear();
  CHECK(config::load(R"({"head":{"laser_distance_mode":"short","laser_timing_budget_ms":15,"laser_roi":"8x6","laser_roi_center":120,"laser_outlier_k":4.5,"laser_calibration_enabled":true,
    "laser_cal_offset_mm":-1.25,"laser_cal_xtalk_cps":4200}})", base, out, problems));
  CHECK(out.head.laser_distance_mode == bungard_aux::DistanceMode::Short && out.head.laser_timing_budget_ms == 15 && out.head.laser_roi_w == 8 && out.head.laser_roi_h == 6);
  CHECK(out.head.laser_roi_center == 120 && out.head.laser_outlier_k == 4.5 && out.head.laser_calibration_enabled && out.head.laser_cal_offset_mm == -1.25 && out.head.laser_cal_xtalk_cps == 4200.0);
  const std::string laser_copy = config::to_json(out, false);
  CHECK(laser_copy.find("\"laser_roi\":\"8x6\"") != std::string::npos && laser_copy.find("\"laser_distance_mode\":\"short\"") != std::string::npos);
  config::Settings laser_round;
  problems.clear();
  CHECK(config::load(config::to_json(out, true), base, laser_round, problems));
  CHECK(laser_round.head.laser_roi_w == 8 && laser_round.head.laser_timing_budget_ms == 15 && laser_round.head.laser_cal_xtalk_cps == 4200.0);
  // a budget the mode does not have, a bad area and a calibration out of bounds are refused with the field's path
  problems.clear();
  CHECK(!config::load(R"({"head":{"laser_distance_mode":"long","laser_timing_budget_ms":15}})", base, out, problems));
  CHECK(has_problem(problems, "head.laser_timing_budget_ms"));
  problems.clear();
  CHECK(!config::load(R"({"head":{"laser_roi":"3x16"}})", base, out, problems));
  CHECK(has_problem(problems, "head.laser_roi"));
  problems.clear();
  CHECK(!config::load(R"({"head":{"laser_cal_offset_mm":90}})", base, out, problems));
  CHECK(has_problem(problems, "head.laser_cal_offset_mm"));

  // a restart is not needed for the name, the language or the time zone; every setting of the head needs one
  config::Settings a = base, b = base;
  b.node_name = "other";
  config::clear_live_settings(a);
  config::clear_live_settings(b);
  CHECK(config::to_json(a, true) == config::to_json(b, true));
  b.head.camera_width = 800;
  config::clear_live_settings(b);
  CHECK(config::to_json(a, true) != config::to_json(b, true));

  std::printf("%s\n", fails == 0 ? "head settings: all checks passed" : "head settings: FAILED");
  return fails == 0 ? 0 : 1;
}
