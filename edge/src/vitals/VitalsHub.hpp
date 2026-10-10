#pragma once

#include "hardware/MLX90640Driver.hpp"
#include "vision/ThermalRoiSampler.hpp"
#include "vitals/VitalsReading.hpp"

struct VitalsHub {
    MLX90640Driver thermal;
    ThermalRoiSampler roi;

    bool init();
    bool refresh();
    VitalsReading sample() const;
    void shutdown();
};
