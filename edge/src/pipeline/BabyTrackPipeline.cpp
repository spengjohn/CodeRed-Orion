#include "BabyTrackPipeline.hpp"

#include <algorithm>

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
    baby_thermal_pixels = 0;
    pan_error = 0.0f;
    tilt_error = 0.0f;
    rgb.assign(static_cast<std::size_t>(OV5647::pixel_count) * 3u, 0);
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
    baby_temp_c = 0.0f;
    baby_temp_valid = false;
    baby_thermal_pixels = 0;
    pan_error = 0.0f;
    tilt_error = 0.0f;
    rgb.clear();
}

bool BabyTrackPipeline::pack_camera_rgb() {
    if (camera == nullptr) {
        return false;
    }
    rgb.resize(static_cast<std::size_t>(OV5647::pixel_count) * 3u);
    return camera->pack_rgb888(rgb.data());
}

bool BabyTrackPipeline::detect_baby() {
    if (coral == nullptr || !coral->ready) {
        baby = nullptr;
        return false;
    }
    if (!coral->detect(rgb.data(), OV5647::width, OV5647::height)) {
        baby = nullptr;
        return true;
    }
    baby = coral->best_person();
    return true;
}

bool BabyTrackPipeline::map_detection_to_thermal() {
    // Coral IR box stays in IR-normalized coords. ThermalRoiSampler projects
    // each MLX90640 pixel into IR space with CameraFusion registration
    // (X mirror + thermal_*_shift_px) before the inside-box test.
    return true;
}

bool BabyTrackPipeline::sample_baby_temperature() {
    if (thermal == nullptr || roi == nullptr) {
        baby_temp_c = 0.0f;
        baby_temp_valid = false;
        baby_thermal_pixels = 0;
        return false;
    }
    const ThermalRoiSample sample = roi->sample(*thermal, baby);
    baby_temp_c = sample.baby_temp_c;
    baby_temp_valid = sample.baby_roi_valid;
    baby_thermal_pixels = sample.baby_pixel_count;
    return true;
}

bool BabyTrackPipeline::compute_servo_error() {
    if (baby == nullptr) {
        pan_error = 0.0f;
        tilt_error = 0.0f;
        return true;
    }

    const float cx = (baby->xmin + baby->xmax) * 0.5f;
    const float cy = (baby->ymin + baby->ymax) * 0.5f;
    pan_error = std::clamp((cx - 0.5f) * 2.0f, -1.0f, 1.0f);
    tilt_error = std::clamp((cy - 0.5f) * 2.0f, -1.0f, 1.0f);
    return true;
}

bool BabyTrackPipeline::control_servos() {
    if (maestro == nullptr || !maestro->ready) {
        return false;
    }
    if (baby == nullptr) {
        return true;
    }
    // Mount wiring: positive image error needs a negative Maestro nudge on both
    // axes. Logged pan/tilt_error stay image-space; only actuation flips.
    return maestro->center_from_error(-pan_error, -tilt_error);
}
