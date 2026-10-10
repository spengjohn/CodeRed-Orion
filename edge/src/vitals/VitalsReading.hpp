#pragma once

#include <cstdint>

struct VitalsReading {
    std::int64_t ts = 0;
    float baby_weight_kg = 0.0f;
    float ambient_temp_c = 0.0f;
    float baby_temp_c = 0.0f;
    int breaths_per_min = 0;
    int is_moving = 0;
};
