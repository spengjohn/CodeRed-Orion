#include "BabyTrackPipeline.hpp"

bool BabyTrackPipeline::init(
    OV5647& cam,
    CoralDriver& tpu,
    MLX90640Driver& therm,
    MaestroDriver& servos,
    ThermalRoiSampler& sampler
) {
    camera = &cam;
    coral = &tpu;
    thermal = &therm;
    maestro = &servos;
    roi = &sampler;
    baby = nullptr;
    baby_temp_c = 0.0f;
    baby_temp_valid = false;
    pan_error = 0.0f;
    tilt_error = 0.0f;
    ready = camera != nullptr
        && coral != nullptr
        && thermal != nullptr
        && maestro != nullptr
        && roi != nullptr;
    return ready;
}

bool BabyTrackPipeline::process_frame() {
    if (!ready || camera == nullptr || !camera->ready) {
        return false;
    }
    if (!pack_camera_rgb()) {
        return false;
    }
    if (!detect_baby()) {
        return false;
    }
    if (!map_detection_to_thermal()) {
        return false;
    }
    if (!sample_baby_temperature()) {
        return false;
    }
    if (!compute_servo_error()) {
        return false;
    }
    return control_servos();
}

void BabyTrackPipeline::shutdown() {
    ready = false;
    camera = nullptr;
    coral = nullptr;
    thermal = nullptr;
    maestro = nullptr;
    roi = nullptr;
    baby = nullptr;
    rgb.clear();
}

bool BabyTrackPipeline::pack_camera_rgb() {
    return true;
}

bool BabyTrackPipeline::detect_baby() {
    return true;
}

bool BabyTrackPipeline::map_detection_to_thermal() {
    return true;
}

bool BabyTrackPipeline::sample_baby_temperature() {
    return true;
}

bool BabyTrackPipeline::compute_servo_error() {
    return true;
}

bool BabyTrackPipeline::control_servos() {
    return true;
}
