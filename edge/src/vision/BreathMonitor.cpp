#include "BreathMonitor.hpp"

bool BreathMonitor::init() {
    ready = true;
    breaths_per_min = 0;
    last_distance_mm = 0.0f;
    distance_delta_mm = 0.0f;
    return true;
}

bool BreathMonitor::update(uint16_t distance_mm, std::int64_t unix_time) {
    (void)unix_time;
    if (!ready) {
        return false;
    }
    const float current = static_cast<float>(distance_mm);
    distance_delta_mm = current - last_distance_mm;
    last_distance_mm = current;
    return true;
}

void BreathMonitor::shutdown() {
    ready = false;
}
