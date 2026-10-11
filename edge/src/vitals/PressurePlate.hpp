#pragma once

#include "hardware/ADS1115Driver.hpp"

struct PressurePlateSample {
    float weight_kg = 0.0f;
    float centroid_x_mm = 0.0f;
    float centroid_y_mm = 0.0f;
    float leg_kg[ADS1115Driver::channel_count]{};
    bool valid = false;
};

struct PressurePlate {
    static constexpr float plate_width_mm = 216.8f;
    static constexpr float plate_length_mm = 345.2f;
    static constexpr float sensor_diameter_mm = 18.2f;
    static constexpr float sensor_radius_mm = sensor_diameter_mm * 0.5f;
    static constexpr float half_span_x_mm =
        plate_width_mm * 0.5f - sensor_radius_mm;
    static constexpr float half_span_y_mm =
        plate_length_mm * 0.5f - sensor_radius_mm;

    bool init();
    void shutdown();
    PressurePlateSample sample(const ADS1115Driver& pressure) const;
};
