#pragma once

#include <cstdint>

namespace edge::hardware {

struct SensorSample {
    float ambient_temp;      // °C
    float baby_temp;         // °C
    int breaths_per_min;     // from lidar delta distance
    float weight;            // from ADS1115 + 4 pressure sensors
    std::int64_t unix_time;  // linux time in seconds
};

class SensorHub {
public:
    bool init();
    SensorSample read();
};

}  // namespace edge::hardware
