#pragma once

#include <cstdint>

struct VitalsReading {
    std::int64_t unix_time = 0;
    float weight = 0.0f;
    float ambient_temp = 0.0f;
    float baby_temp = 0.0f;
    int breaths = 0;
    bool moved = false;
};
