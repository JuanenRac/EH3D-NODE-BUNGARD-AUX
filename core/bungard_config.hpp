// EH3D-NODE-BUNGARD-AUX - the settings of the node of the drill head: what the web panel edits, what is stored in flash and what the firmware obeys.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// The file has the name that the node of the machine (EH3D-NODE-BUNGARD) gives to its own, so the files of the firmware base that both nodes share (the store, the network, the panel server)
// include it unchanged. Everything every node has (identity, Wi-Fi with its backup networks, address, users' language, clock) is `NodeSettings` of core/node_settings.hpp; what is only
// the head's - the camera, the stream, the laser, the spindle sensors, the temperature and the protect output - is the `head` section, which is the settings of core/aux_config.hpp (the
// ranges and the words of each value are checked there, the same ones that the file head.cfg always had) carried as JSON. A password or a token is never written back to the panel:
// a value sent empty keeps the stored one, `<name>_clear: true` erases it, and `<name>_set` tells the panel that there is one.
#pragma once
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

#include "aux_config.hpp"
#include "board_s3.hpp"
#include "config_common.hpp"
#include "json.hpp"
#include "node_settings.hpp"
#include "stored_load.hpp"

namespace eh3d::config {

struct Settings : NodeSettings {   // what every node has and the head's own
  bungard_aux::Settings head;
};

inline Settings default_settings(std::string_view mac_tail) {
  Settings s;
  node_defaults(s, mac_tail);
  return s;
}

// ---- reading -------------------------------------------------------------------------------------------------------------------

namespace head_detail {
// A JSON value as the text the file of the head would have after `=`: on/off, a number as it is, a text as it is.
inline bool as_text(const json::Value& v, std::string& out) {
  char buffer[48];
  if (v.is_bool()) { out = v.boolean ? "on" : "off"; return true; }
  if (v.is_number()) {
    const double d = v.number;
    if (d == static_cast<double>(static_cast<long long>(d))) std::snprintf(buffer, sizeof buffer, "%lld", static_cast<long long>(d)); else std::snprintf(buffer, sizeof buffer, "%.6g", d);
    out = buffer;
    return true;
  }
  if (v.is_string()) { out = v.text; return true; }
  return false;
}
inline bool is_secret(std::string_view key) { return key == "token" || key == "stream_password"; }
}  // namespace head_detail

// Applies the members of a `head` object on top of `h`; whatever the object omits stays as it was. Each member is checked by the same rules as a line of head.cfg.
inline void head_read(const json::Value& head, bungard_aux::Settings& h, Problems& problems, const std::string& prefix = "head.") {
  if (!head.is_object()) { detail::bad(problems, prefix.empty() ? "" : prefix.substr(0, prefix.size() - 1), "invalid"); return; }
  std::string text;
  for (std::size_t i = 0; i < head.names.size(); ++i) {
    const std::string& key = head.names[i];
    const json::Value& value = head.items[i];
    if (key.size() > 6 && key.compare(key.size() - 6, 6, "_clear") == 0) continue;   // read with its secret below
    if (key.size() > 4 && key.compare(key.size() - 4, 4, "_set") == 0) continue;     // what the panel was told, not a setting
    std::string v;
    if (!head_detail::as_text(value, v)) { detail::bad(problems, prefix + key, "invalid"); continue; }
    if (head_detail::is_secret(key)) {
      const json::Value* clear = head.get(key + "_clear");
      if (clear != nullptr && clear->is_bool() && clear->boolean) v.clear();   // erased
      else if (v.empty()) continue;                                              // empty: the stored one stays
    }
    if (v.find('\n') != std::string::npos || v.find('\r') != std::string::npos) { detail::bad(problems, prefix + key, "invalid"); continue; }
    text += key + " = " + v + "\n";
  }
  // a secret that is only to be cleared has no value member of its own
  for (const char* secret : {"token", "stream_password"}) {
    const json::Value* clear = head.get(std::string(secret) + "_clear");
    if (clear != nullptr && clear->is_bool() && clear->boolean && head.get(secret) == nullptr) text += std::string(secret) + " = \n";
  }
  std::vector<bungard_aux::Problem> found;
  h = bungard_aux::parse_settings_over(h, text, found);
  for (const bungard_aux::Problem& p : found) detail::bad(problems, prefix + (p.key.empty() ? "settings" : p.key), "invalid");
}

inline void read_settings(const json::Value& document, Settings& s, Problems& problems) {
  if (!document.is_object()) { detail::bad(problems, "", "invalid"); return; }
  node_read(document, s, problems);
  if (const json::Value* head = document.get("head"); head != nullptr) head_read(*head, s.head, problems);
}

// ---- checking ------------------------------------------------------------------------------------------------------------------

inline Problems validate(const Settings& s) {
  Problems problems;
  node_validate(s, problems);
  return problems;
}

// ---- writing -------------------------------------------------------------------------------------------------------------------

// The members of the `head` object (the caller opens the object). `secrets`: true writes the token and the password of the stream (for flash storage); false replaces them with `_set` flags.
inline void head_write(json::Writer& w, const bungard_aux::Settings& h, bool secrets) {
  w.field("camera_enabled", h.camera_enabled).field("camera_model", bungard_aux::model_text(h.camera_model)).field("camera_width", static_cast<int>(h.camera_width))
      .field("camera_height", static_cast<int>(h.camera_height)).field("camera_quality", static_cast<int>(h.camera_quality)).field("camera_fps_max", static_cast<int>(h.camera_fps_max));
  w.field("rtsp_enabled", h.rtsp_enabled).field("rtsp_port", static_cast<int>(h.rtsp_port)).field("onvif_enabled", h.onvif_enabled).field("stream_user", h.stream_user);
  if (secrets) w.field("stream_password", h.stream_password); else w.field("stream_password_set", !h.stream_password.empty());
  w.field("laser_enabled", h.laser_enabled).field("laser_samples", static_cast<int>(h.laser_samples)).field("laser_timeout_ms", static_cast<long long>(h.laser_timeout_ms))
      .field("laser_offset_mm", h.laser_offset_mm).field("laser_range_min_mm", h.laser_range_min_mm).field("laser_range_max_mm", h.laser_range_max_mm).field("laser_sigma_max_mm", h.laser_sigma_max_mm);
  w.field("laser_distance_mode", bungard_aux::distance_mode_text(h.laser_distance_mode)).field("laser_timing_budget_ms", static_cast<long long>(h.laser_timing_budget_ms))
      .field("laser_roi", bungard_aux::roi_text(h.laser_roi_w, h.laser_roi_h)).field("laser_roi_center", static_cast<long long>(h.laser_roi_center))
      .field("laser_outlier_k", h.laser_outlier_k).field("laser_outlier_floor_mm", h.laser_outlier_floor_mm).field("laser_calibration_enabled", h.laser_calibration_enabled)
      .field("laser_cal_offset_mm", h.laser_cal_offset_mm).field("laser_cal_xtalk_cps", h.laser_cal_xtalk_cps);
  w.field("light_enabled", h.light_enabled).field("light_leds", static_cast<long long>(h.light_leds)).field("light_level", static_cast<long long>(h.light_level))
      .field("light_max_level", static_cast<long long>(h.light_max_level)).field("light_tone", bungard_aux::tone_text(h.light_tone)).field("light_auto_off_s", static_cast<long long>(h.light_auto_off_s))
      .field("light_on_at_start", h.light_on_at_start).field("light_order", h.light_order == bungard_aux::ColourOrder::Rgb ? "rgb" : "grb");
  w.field("rpm_enabled", h.rpm_enabled).field("rpm_pulses_per_rev", static_cast<long long>(h.rpm_pulses_per_rev)).field("rpm_max", static_cast<long long>(h.rpm_max))
      .field("rpm_stale_ms", static_cast<long long>(h.rpm_stale_ms));
  w.field("temp_sensor", bungard_aux::temp_sensor_text(h.temp_sensor)).field("ntc_series_ohm", h.ntc_series_ohm).field("ntc_r25_ohm", h.ntc_r25_ohm).field("ntc_beta", h.ntc_beta)
      .field("temp_warn_c", h.temp_warn_c).field("temp_critical_c", h.temp_critical_c).field("temp_hysteresis_c", h.temp_hysteresis_c).field("protect_output", h.protect_output);
  if (secrets) w.field("token", h.token); else w.field("token_set", !h.token.empty());
}

inline std::string head_to_json(const bungard_aux::Settings& h, bool secrets) {
  json::Writer w;
  w.begin_object();
  head_write(w, h, secrets);
  w.end_object();
  return w.str();
}

// `secrets`: true writes the passwords and the tokens (for flash storage); false replaces them with `_set` flags (for the panel).
inline std::string to_json(const Settings& s, bool secrets) {
  json::Writer w;
  w.begin_object();
  node_write_head(w, s, secrets);
  w.key("head").begin_object();
  head_write(w, s.head, secrets);
  w.end_object();
  node_write_tail(w, s);
  w.end_object();
  return w.str();
}

// Reads a stored or received document on top of `base` and checks the result. Nothing is applied unless `problems` stays empty.
inline bool load(std::string_view text, const Settings& base, Settings& out, Problems& problems) {
  json::Value document;
  if (!json::parse(text, document)) { problems.push_back({"", "not_json"}); return false; }
  out = base;
  read_settings(document, out, problems);
  if (problems.empty()) problems = validate(out);
  return problems.empty();
}

// ---- what a change needs ---------------------------------------------------------------------------------------------------------------

// Blanks the settings that are applied as soon as they are saved, so that two sets of settings that differ only in those need no restart: the display name, the language and the time zone.
// Everything of the head is taken when the node starts (the camera, the sensors and the streams are started once).
inline void clear_live_settings(Settings& s) {
  s.node_name.clear();
  s.language = "en";
  s.time.zone.clear();
}

}  // namespace eh3d::config
