#pragma once

#include "hardware/ADS1115Driver.hpp"
#include "hardware/MLX90640Driver.hpp"
#include "vision/ThermalRoiSampler.hpp"
#include "vitals/PressurePlate.hpp"
#include "vitals/VitalsReading.hpp"

struct VitalsHub {
    MLX90640Driver thermal;
    ADS1115Driver pressure;
    ThermalRoiSampler roi;
    PressurePlate plate;

    bool init();
    bool refresh();
    VitalsReading sample() const;
    void shutdown();
};
