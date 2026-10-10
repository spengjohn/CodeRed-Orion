#include "sensors.hpp"

#include <chrono>

namespace edge::hardware {

bool SensorHub::init() {
    // TODO: init thermal sensor, single-point lidar, ADS1115, Pololu Maestro
    return true;
}

SensorSample SensorHub::read() {
    const auto now = std::chrono::system_clock::now();
    const auto unix_time = std::chrono::duration_cast<std::chrono::seconds>(
                               now.time_since_epoch())
                               .count();

    // Placeholder sample until hardware drivers are wired
    return SensorSample{
        .ambient_temp = 0.0f,
        .baby_temp = 0.0f,
        .breaths_per_min = 0,
        .weight = 0.0f,
        .unix_time = unix_time,
    };
}

}  // namespace edge::hardware
