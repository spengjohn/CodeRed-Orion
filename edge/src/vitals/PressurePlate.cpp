#include "PressurePlate.hpp"

#include <cmath>

namespace {

constexpr float k_leg_x_mm[ADS1115Driver::channel_count] = {
    PressurePlate::half_span_x_mm,
    -PressurePlate::half_span_x_mm,
    PressurePlate::half_span_x_mm,
    -PressurePlate::half_span_x_mm,
};

constexpr float k_leg_y_mm[ADS1115Driver::channel_count] = {
    PressurePlate::half_span_y_mm,
    PressurePlate::half_span_y_mm,
    -PressurePlate::half_span_y_mm,
    -PressurePlate::half_span_y_mm,
};

}

bool PressurePlate::init() {
    return true;
}

void PressurePlate::shutdown() {}

PressurePlateSample PressurePlate::sample(const ADS1115Driver& pressure) const {
    PressurePlateSample out;

    if (!pressure.ready || pressure.demo_mode) {
        return out;
    }

    float total = 0.0f;
    float moment_x = 0.0f;
    float moment_y = 0.0f;

    for (int i = 0; i < ADS1115Driver::channel_count; ++i) {
        float kg = pressure.kg[i];
        if (!std::isfinite(kg) || kg < 0.0f) {
            kg = 0.0f;
        }
        out.leg_kg[i] = kg;
        total += kg;
        moment_x += kg * k_leg_x_mm[i];
        moment_y += kg * k_leg_y_mm[i];
    }

    out.weight_kg = total;
    out.valid = true;

    if (total > 1.0e-3f) {
        out.centroid_x_mm = moment_x / total;
        out.centroid_y_mm = moment_y / total;
    }

    return out;
}
