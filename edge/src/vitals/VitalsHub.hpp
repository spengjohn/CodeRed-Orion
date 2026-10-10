#pragma once

#include <cstdint>
#include <vector>

#include "hardware/ADS1115Driver.hpp"
#include "hardware/CoralDriver.hpp"
#include "hardware/MLX90640Driver.hpp"
#include "hardware/OV5647.hpp"
#include "hardware/TfLunaDriver.hpp"
#include "vision/BreathMonitor.hpp"
#include "vision/CameraFusion.hpp"
#include "vision/ThermalRoiSampler.hpp"
#include "vitals/VitalsReading.hpp"

struct VitalsHub {
    MLX90640Driver thermal;
    OV5647 camera;
    CoralDriver coral;
    ADS1115Driver weight;
    TfLunaDriver lidar;
    BreathMonitor breath;
    ThermalRoiSampler roi;
    CameraFusion fusion;

    bool init();
    bool refresh();
    VitalsReading sample() const;
    bool render_ir_jpeg(std::vector<std::uint8_t>& jpeg_out);
    void shutdown();
};
