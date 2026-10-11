#pragma once

#include "hardware/ADS1115Driver.hpp"
#include "hardware/MLX90640Driver.hpp"
#include "hardware/TfLunaDriver.hpp"
#include "vision/BreathMonitor.hpp"
#include "vision/ThermalRoiSampler.hpp"
#include "vitals/PressurePlate.hpp"
#include "vitals/VitalsReading.hpp"

struct VitalsHub {
    MLX90640Driver thermal;
    ADS1115Driver pressure;
    TfLunaDriver lidar;
    ThermalRoiSampler roi;
    PressurePlate plate;
    BreathMonitor breath;

    bool init();
    bool refresh();
    VitalsReading sample() const;
    void shutdown();
};
