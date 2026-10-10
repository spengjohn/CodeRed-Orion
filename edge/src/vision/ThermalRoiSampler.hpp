#pragma once

#include "hardware/CoralDriver.hpp"
#include "hardware/MLX90640Driver.hpp"

struct ThermalRoiSample {
    float ambient_temp_c = 0.0f;
    float baby_temp_c = 0.0f;
    bool baby_roi_valid = false;
};

struct ThermalRoiSampler {
    float ambient_percentile = 0.20f;

    bool init();
    void shutdown();
    ThermalRoiSample sample(const MLX90640Driver& thermal) const;
    ThermalRoiSample sample(
        const MLX90640Driver& thermal,
        const CoralDetection* baby_box
    ) const;
};
