#pragma once

#include <cstdint>
#include <vector>

#include "hardware/CoralDriver.hpp"
#include "hardware/MLX90640Driver.hpp"
#include "hardware/MaestroDriver.hpp"
#include "hardware/OV5647.hpp"
#include "vision/ThermalRoiSampler.hpp"

// Per-frame loop when TRACKING_ENABLED=1:
//   pack RGB -> Coral detect/best_person -> thermal ROI -> pan/tilt error
//   -> MaestroDriver::center_from_error so the baby stays framed center.

struct BabyTrackPipeline {
    OV5647* camera = nullptr;
    CoralDriver* coral = nullptr;
    MLX90640Driver* thermal = nullptr;
    MaestroDriver* maestro = nullptr;
    ThermalRoiSampler* roi = nullptr;
    bool ready = false;

    std::vector<std::uint8_t> rgb{};
    const CoralDetection* baby = nullptr;
    float baby_temp_c = 0.0f;
    bool baby_temp_valid = false;
    float pan_error = 0.0f;
    float tilt_error = 0.0f;

    bool init(
        OV5647& cam,
        CoralDriver& tpu,
        MLX90640Driver& therm,
        MaestroDriver& servos,
        ThermalRoiSampler& sampler
    );
    bool process_frame();
    void shutdown();

    bool pack_camera_rgb();
    bool detect_baby();
    bool map_detection_to_thermal();
    bool sample_baby_temperature();
    bool compute_servo_error();
    bool control_servos();
};
