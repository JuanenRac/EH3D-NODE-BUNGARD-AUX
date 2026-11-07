// EH3D-NODE-BUNGARD-AUX - a head that is not there: reads request lines from the standard input and prints the answers the firmware would send.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
// Lines: `GET /api/status`, `POST /api/measure?samples=5`, and the commands `move X Y`, `spindle RPM`, `advance MS`, `fault NAME on|off` (camera_down, laser_fault,
// laser_weak, rpm_noise, temp_missing, light_missing). A `head.cfg` file given as the first argument sets the settings. The numbers of the table are made up.
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "../core/head_sim.hpp"

using namespace bungard_aux;

int main(int argc, char** argv) {
    Settings cfg;
    if (argc > 1) {
        std::ifstream in(argv[1], std::ios::binary);
        if (!in) { std::fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }
        std::ostringstream all;
        all << in.rdbuf();
        std::vector<Problem> problems;
        cfg = parse_settings(all.str(), problems);
        for (const Problem& p : problems) std::fprintf(stderr, "line %d %s: %s\n", p.line, p.key.c_str(), p.what.c_str());
    }
    HeadSim sim(cfg);
    std::string line;
    while (std::getline(std::cin, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream words(line);
        std::string first;
        words >> first;
        if (first == "GET" || first == "POST") {
            const std::string body = sim.answer(line);
            std::printf("%s\n", body.empty() ? "{\"error\":\"not_found\"}" : body.c_str());
        } else if (first == "move") {
            double x = 0, y = 0;
            words >> x >> y;
            sim.move_to(x, y);
        } else if (first == "spindle") {
            double rpm = 0;
            words >> rpm;
            sim.set_spindle_target(rpm);
        } else if (first == "advance") {
            unsigned ms = 0;
            words >> ms;
            sim.advance(ms);
        } else if (first == "fault") {
            std::string name, state;
            words >> name >> state;
            const bool on = state != "off";
            SimFaults& f = sim.faults();
            if (name == "camera_down") f.camera_down = on;
            else if (name == "laser_fault") f.laser_fault = on;
            else if (name == "laser_weak") f.laser_weak = on;
            else if (name == "rpm_noise") f.rpm_noise = on;
            else if (name == "temp_missing") f.temp_missing = on;
            else if (name == "light_missing") f.light_missing = on;
            else std::fprintf(stderr, "unknown fault %s\n", name.c_str());
        } else if (!first.empty()) {
            std::fprintf(stderr, "unknown command %s\n", first.c_str());
        }
    }
    return 0;
}
