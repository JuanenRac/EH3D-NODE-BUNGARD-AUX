// EH3D-NODE-BUNGARD-AUX - core/camera.hpp: the cameras the node takes (OV2640 and OV5640) and the picture size each one can give. GPL-3.0-or-later.
//
// The firmware asks the camera driver for one of its frame sizes; this table says which ones exist, which sensor can do each one, and which one is the nearest to the
// width and height of the settings. Nothing here touches the camera, so it is tested on a computer.
#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace bungard_aux {

enum class CameraModel { Auto, Ov2640, Ov5640 };

inline const char* model_text(CameraModel m) {
    switch (m) {
        case CameraModel::Ov2640: return "ov2640";
        case CameraModel::Ov5640: return "ov5640";
        default: return "auto";
    }
}

/// The model a camera says it is: the product id its sensor reports (OV2640 0x26, OV5640 0x5640); anything else is not one the node knows.
inline CameraModel model_from_pid(int pid) {
    if (pid == 0x26) return CameraModel::Ov2640;
    if (pid == 0x5640) return CameraModel::Ov5640;
    return CameraModel::Auto;
}

struct FrameSize {
    int width;
    int height;
    bool ov2640;  // the OV2640 can give this size too (the OV5640 gives all of them)
};

/// The sizes, from the smallest to the largest, in the order of the firmware's table of the driver's frame sizes (main/app_main.cpp keeps the same order).
inline const std::vector<FrameSize>& frame_sizes() {
    static const std::vector<FrameSize> table = {
        {160, 120, true},    // QQVGA
        {320, 240, true},    // QVGA
        {400, 296, true},    // CIF
        {640, 480, true},    // VGA
        {800, 600, true},    // SVGA
        {1024, 768, true},   // XGA
        {1280, 720, true},   // HD
        {1280, 1024, true},  // SXGA
        {1600, 1200, true},  // UXGA
        {1920, 1080, false},  // Full HD
        {2048, 1536, false},  // QXGA
        {2560, 1440, false},  // QHD
        {2560, 1920, false},  // QSXGA
        {2592, 1944, false},  // 5 MP
    };
    return table;
}

/// Can this model give this size. With the model not known yet (Auto) only what both sensors give is allowed.
inline bool model_gives(CameraModel m, const FrameSize& f) {
    return m == CameraModel::Ov5640 || f.ov2640;
}

/// The index in `frame_sizes()` of the largest size that is not larger than the wanted one in either side (the smallest when none fits), among the ones the model gives.
inline std::size_t frame_index_for(int width, int height, CameraModel model) {
    const auto& t = frame_sizes();
    std::size_t best = 0;
    for (std::size_t i = 0; i < t.size(); ++i) {
        if (!model_gives(model, t[i])) continue;
        if (t[i].width <= width && t[i].height <= height) best = i;
    }
    return best;
}

/// The sizes a model can give, as (width, height), for the ONVIF encoder options: the largest first.
inline std::vector<std::pair<int, int>> sizes_of(CameraModel model) {
    std::vector<std::pair<int, int>> out;
    const auto& t = frame_sizes();
    for (std::size_t i = t.size(); i-- > 0;) {
        if (model_gives(model, t[i])) out.emplace_back(t[i].width, t[i].height);
    }
    return out;
}

}  // namespace bungard_aux
