#pragma once

#include "hardware/CoralDriver.hpp"
#include "hardware/MLX90640Driver.hpp"
#include "vision/CameraFusion.hpp"

struct ThermalRoiSample {
    float ambient_temp_c = 0.0f;
    float baby_temp_c = 0.0f;
    bool baby_roi_valid = false;
    int baby_pixel_count = 0;
};

struct ThermalRoiSampler {
    float ambient_percentile = 0.20f;
    // Copied from CameraFusion so IR baby boxes use the same registration.
    float thermal_x_shift_px = -2.5f;
    float thermal_y_shift_px = 0.0f;

    bool init();
    void shutdown();
    void sync_registration(const CameraFusion& fusion);

    ThermalRoiSample sample(const MLX90640Driver& thermal) const;
    ThermalRoiSample sample(
        const MLX90640Driver& thermal,
        const CoralDetection* baby_box
    ) const;
};
