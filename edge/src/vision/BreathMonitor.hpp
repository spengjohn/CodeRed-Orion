#pragma once

#include <cstdint>

struct BreathMonitor {
    int breaths_per_min = 0;
    float last_distance_mm = 0.0f;
    float distance_delta_mm = 0.0f;
    bool ready = false;

    bool init();
    bool update(uint16_t distance_mm, std::int64_t unix_time);
    void shutdown();
};
