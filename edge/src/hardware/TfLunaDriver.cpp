#include "TfLunaDriver.hpp"

#include <cmath>

bool TfLunaDriver::init() {
    ready = true;
    demo_mode = true;
    distance_mm = 0;
    strength = 0;
    frame_index = 0;
    last_error[0] = '\0';
    return true;
}

bool TfLunaDriver::refresh() {
    if (!ready) {
        return false;
    }

    ++frame_index;

    if (demo_mode) {
        constexpr float k_center_mm = 300.0f;
        constexpr float k_amp_mm = 5.0f;
        constexpr float k_breaths_per_sec = 25.0f / 60.0f;
        constexpr float k_fps = 20.0f;
        const float t = static_cast<float>(frame_index) / k_fps;
        const float phase = 2.0f * 3.14159265f * k_breaths_per_sec * t;
        const float d = k_center_mm + k_amp_mm * std::sin(phase);
        distance_mm = static_cast<uint16_t>(d + 0.5f);
        strength = 2000;
    }

    return true;
}

void TfLunaDriver::shutdown() {
    ready = false;
    distance_mm = 0;
    strength = 0;
}
