// EH3D-NODE-BUNGARD-AUX - host tests of the simulated head, of the self-check and of the contract page (docs/CONTRACT.md) against the real answers.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "../core/head_sim.hpp"
#include "../core/json.hpp"

using namespace bungard_aux;
static int fails = 0;
static int checks = 0;
#define CHECK(x) do { ++checks; if (!(x)) { std::printf("FAIL line %d: %s\n", __LINE__, #x); ++fails; } } while (0)

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

// The rows `| `key` | ...` of a section of docs/CONTRACT.md: key -> second column.
static std::map<std::string, std::string> doc_rows(const std::string& section) {
  std::map<std::string, std::string> rows;
  std::istringstream in(source("docs/CONTRACT.md"));
  std::string line;
  bool inside = false;
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.rfind("## ", 0) == 0) { inside = line.find(section) != std::string::npos; continue; }
    if (!inside || line.rfind("| `", 0) != 0) continue;
    const std::size_t a = 3, b = line.find('`', a);
    if (b == std::string::npos) continue;
    const std::size_t c = line.find('|', b);
    const std::size_t d = c == std::string::npos ? c : line.find('|', c + 1);
    rows[line.substr(a, b - a)] = d == std::string::npos ? "" : line.substr(c + 2, d - c - 3);
  }
  return rows;
}

static std::set<std::string> keys_of(const std::string& text) {
  eh3d::json::Value v;
  std::set<std::string> keys;
  if (!eh3d::json::parse(text, v) || !v.is_object()) return keys;
  for (const std::string& n : v.names) keys.insert(n);
  return keys;
}

static eh3d::json::Value parsed(const std::string& text) {
  eh3d::json::Value v;
  CHECK(eh3d::json::parse(text, v));
  return v;
}

static Settings secure() {  // a node that has its token and its stream password, so the only things to say are the real ones
  Settings s;
  s.token = "abcdefgh12";
  s.stream_password = "longpassword";
  return s;
}

static std::string codes_of(const HeadSim& sim) { return parsed(json_status(sim.status())).string_or("selfcheck_codes", "?"); }
static std::string overall_of(const HeadSim& sim) { return parsed(json_status(sim.status())).string_or("selfcheck", "?"); }
static bool has_code(const HeadSim& sim, const char* code) { return self_check(sim.status(), sim.settings()).has(code); }

int main() {
  // ---- the contract page and the answers say the same ---------------------------------------------------------------------
  {
    HeadSim sim(secure());
    sim.advance(1000);
    const std::string status = json_status(sim.status());
    const auto want = doc_rows("GET /api/status");
    std::set<std::string> doc_keys;
    for (const auto& r : want) doc_keys.insert(r.first);
    CHECK(keys_of(status) == doc_keys);
    CHECK(doc_keys.size() > 30);
    for (const std::string& k : keys_of(status)) if (doc_keys.count(k) == 0) std::printf("  answer key not in the page: %s\n", k.c_str());
    for (const std::string& k : doc_keys) if (keys_of(status).count(k) == 0) std::printf("  page key not in the answer: %s\n", k.c_str());

    HeadSim m(secure());
    std::set<std::string> measure_doc;
    for (const auto& r : doc_rows("POST /api/measure")) measure_doc.insert(r.first);
    CHECK(keys_of(m.answer("POST /api/measure?samples=5")) == measure_doc);

    // the keys the server cannot do without are in the answer with the right type
    const eh3d::json::Value v = parsed(status);
    CHECK(v.get("node") != nullptr && v.get("node")->is_string());
    for (const char* k : {"camera", "laser", "rpm_sensor", "protect_open"}) CHECK(v.get(k) != nullptr && v.get(k)->is_bool());

    // the codes of the page are the codes of the firmware, with the same severity
    const auto codes = doc_rows("The self-check");
    CHECK(codes.size() == self_check_catalog().size());
    for (const Finding& f : self_check_catalog()) {
      CHECK(codes.count(f.code) == 1);
      CHECK(codes.count(f.code) == 0 || codes.at(f.code) == severity_text(f.severity));
    }
    std::set<std::string> unique;
    for (const Finding& f : self_check_catalog()) CHECK(unique.insert(f.code).second);
  }

  // ---- the answers are the same for the same seed, and a request the simulator does not play is not answered ------------------
  {
    HeadSim a(secure(), SimWorld{}, 7), b(secure(), SimWorld{}, 7);
    CHECK(a.answer("POST /api/measure?samples=9") == b.answer("POST /api/measure?samples=9"));
    CHECK(a.answer("GET /nowhere").empty() && a.answer("garbage").empty() && a.answer("GET /api/light").empty());
    CHECK(a.answer("POST /api/measure?samples=5").find("\"asked\":5") != std::string::npos);
    CHECK(a.answer("POST /api/measure?samples=99").find("\"asked\":7") != std::string::npos);  // out of range: the setting is used
  }

  // ---- the table is not flat and the measure sees it ---------------------------------------------------------------------------
  {
    HeadSim sim(secure());
    sim.advance(1000);
    sim.move_to(0, 0);
    const Measure at0 = sim.measure(9);
    sim.move_to(500, 0);
    const Measure at500 = sim.measure(9);
    CHECK(at0.ok && at500.ok);
    CHECK(std::fabs(at0.distance_mm - 120.0) < 0.05);
    CHECK(std::fabs((at500.distance_mm - at0.distance_mm) - 0.4) < 0.05);  // 0.8 mm per metre over half a metre
    CHECK(at0.sigma_mm > 0.0 && at0.sigma_mm < 0.1);
    CHECK(std::fabs(parsed(sim.answer("POST /api/measure")).number_or("distance_mm", -1) - at500.distance_mm) < 0.05);
  }

  // ---- a node with its token and its password, healthy ---------------------------------------------------------------------------
  {
    HeadSim sim(secure());
    sim.advance(1000);
    CHECK(overall_of(sim) == "ok");  // inside the grace time nothing that has not arrived yet counts
    CHECK(codes_of(sim).empty());
    sim.advance(40000);
    CHECK(has_code(sim, "AUX-L01") && overall_of(sim) == "ok");  // no reading yet: said, but only as information
    sim.move_to(100, 100);
    sim.measure(7);
    CHECK(!has_code(sim, "AUX-L01"));
    sim.advance(601000);
    CHECK(has_code(sim, "AUX-L03") && overall_of(sim) == "warn");
    sim.measure(7);
    CHECK(!has_code(sim, "AUX-L03") && overall_of(sim) == "ok");
  }

  // ---- the defaults of a fresh node: no token and no password ---------------------------------------------------------------------
  {
    HeadSim sim{Settings{}};
    sim.advance(1000);
    CHECK(has_code(sim, "AUX-S02") && has_code(sim, "AUX-S03") && overall_of(sim) == "warn");
    CHECK(codes_of(sim) == "AUX-S02,AUX-S03");
  }

  // ---- the faults are said, once the grace time is over --------------------------------------------------------------------------
  {
    HeadSim sim(secure());
    sim.faults().camera_down = true;
    sim.faults().light_missing = true;
    sim.advance(5000);
    CHECK(!has_code(sim, "AUX-C01") && !has_code(sim, "AUX-S01"));  // the start is not a fault
    sim.advance(30000);
    CHECK(has_code(sim, "AUX-C01") && has_code(sim, "AUX-S01") && overall_of(sim) == "fail");
    sim.faults().camera_down = false;
    sim.faults().light_missing = false;
    CHECK(!has_code(sim, "AUX-C01") && !has_code(sim, "AUX-S01") && overall_of(sim) == "ok");

    sim.faults().laser_fault = true;
    CHECK(!sim.measure(7).ok);
    CHECK(has_code(sim, "AUX-L02") && overall_of(sim) == "fail");
    CHECK(sim.status().laser_last_status == "sensor_fault");
    sim.faults().laser_fault = false;
    sim.faults().laser_weak = true;
    CHECK(!sim.measure(7).ok);
    CHECK(has_code(sim, "AUX-L04") && !has_code(sim, "AUX-L02") && overall_of(sim) == "warn");
    sim.faults().laser_weak = false;
    CHECK(sim.measure(7).ok && overall_of(sim) == "ok");
  }

  // ---- the spindle: noisy pulses, too fast ---------------------------------------------------------------------------------------
  {
    HeadSim sim(secure());
    sim.advance(31000);
    sim.set_spindle_target(24000);
    sim.advance(4000);
    CHECK(std::fabs(sim.status().rpm - 24000.0) < 1.0 && sim.status().rpm_quality == "good" && overall_of(sim) == "ok");
    sim.faults().rpm_noise = true;
    CHECK(has_code(sim, "AUX-R01") && overall_of(sim) == "warn");
    sim.faults().rpm_noise = false;
    sim.set_spindle_target(70000);  // above rpm_max
    sim.advance(10000);
    CHECK(has_code(sim, "AUX-R02") && overall_of(sim) == "fail");
    sim.set_spindle_target(0);
    sim.advance(20000);
    CHECK(!has_code(sim, "AUX-R02"));
    // with the speed sensor off in the settings the speed is never judged
    sim.settings().rpm_enabled = false;
    sim.set_spindle_target(70000);
    sim.advance(20000);
    CHECK(!has_code(sim, "AUX-R02"));
  }

  // ---- the temperature: warm, hot (the protect output opens), a sensor that goes silent ---------------------------------------------------
  {
    Settings cfg = secure();
    cfg.temp_sensor = TempSensor::Ds18b20;
    SimWorld world;
    world.heat_c_per_s_at_max = 0.2;  // a spindle that heats fast, so the test is short
    HeadSim sim(cfg, world);
    sim.advance(31000);
    sim.set_spindle_target(65000);
    bool warm = false, hot = false;
    for (int i = 0; i < 4000 && !hot; ++i) {
      sim.advance(1000);
      const Heat h = sim.status().heat;
      if (h == Heat::Warm && !warm) {
        warm = true;
        CHECK(has_code(sim, "AUX-T02") && !has_code(sim, "AUX-T03") && overall_of(sim) == "warn");
        CHECK(!sim.status().protect_open);
      }
      if (h == Heat::Hot) hot = true;
    }
    CHECK(warm && hot);
    CHECK(has_code(sim, "AUX-T03") && overall_of(sim) == "fail" && sim.status().protect_open && has_code(sim, "AUX-P02"));
    sim.set_spindle_target(0);
    bool cool = false;
    for (int i = 0; i < 4000 && !cool; ++i) { sim.advance(1000); cool = sim.status().heat == Heat::Ok; }
    CHECK(cool && !has_code(sim, "AUX-T03") && !has_code(sim, "AUX-T02") && !sim.status().protect_open);
    sim.faults().temp_missing = true;
    sim.advance(1000);
    CHECK(has_code(sim, "AUX-T01") && overall_of(sim) == "fail" && sim.status().protect_open);  // a sensor that is on and silent counts as hot
    CHECK(parsed(json_status(sim.status())).get("temp_c")->type == eh3d::json::Type::kNull);
  }

  // ---- the settings change what is said, and they are never in the code ------------------------------------------------------------------
  {
    Settings cfg = secure();
    cfg.protect_output = false;
    cfg.laser_calibration_enabled = true;
    HeadSim sim(cfg);
    sim.advance(1000);
    CHECK(has_code(sim, "AUX-P01") && has_code(sim, "AUX-L05") && overall_of(sim) == "warn");
    cfg.selfcheck_laser_age_max_s = 10;
    cfg.selfcheck_grace_s = 0;
    HeadSim quick(cfg);
    quick.advance(500);
    quick.measure(5);
    quick.advance(11000);
    CHECK(has_code(quick, "AUX-L03"));
    cfg.selfcheck_enabled = false;
    HeadSim off(cfg);
    off.advance(1000);
    CHECK(overall_of(off) == "off" && codes_of(off).empty());
    // the limits are settings of the file, with a range
    std::vector<Problem> problems;
    Settings read = parse_settings("selfcheck_laser_age_max_s = 5\nselfcheck_grace_s = 601\nselfcheck_enabled = maybe\n", problems);
    CHECK(problems.size() == 3 && read.selfcheck_laser_age_max_s == 600 && read.selfcheck_grace_s == 30 && read.selfcheck_enabled);
    read = parse_settings("selfcheck_laser_age_max_s = 60\nselfcheck_grace_s = 0\nselfcheck_enabled = off\n", problems);
    CHECK(read.selfcheck_laser_age_max_s == 60 && read.selfcheck_grace_s == 0 && !read.selfcheck_enabled);
  }

  std::printf("%d checks, %d failed\n", checks, fails);
  return fails == 0 ? 0 : 1;
}
