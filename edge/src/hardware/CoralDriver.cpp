#include "CoralDriver.hpp"

bool CoralDriver::init() {
    ready = true;
    demo_mode = true;
    stick_present = false;
    detection_count = 0;
    return true;
}

bool CoralDriver::refresh() {
    if (!ready) {
        return false;
    }
    ++frame_index;
    return true;
}

bool CoralDriver::detect(const uint8_t* rgb, int width, int height) {
    (void)rgb;
    (void)width;
    (void)height;
    if (!ready) {
        ++infer_fail;
        return false;
    }
    detection_count = 0;
    ++frame_index;
    ++infer_ok;
    return true;
}

const CoralDetection* CoralDriver::best_person() const {
    const CoralDetection* best = nullptr;
    for (int i = 0; i < detection_count; ++i) {
        if (detections[i].class_id != person_class_id) {
            continue;
        }
        if (detections[i].score < score_threshold) {
            continue;
        }
        if (best == nullptr || detections[i].score > best->score) {
            best = &detections[i];
        }
    }
    return best;
}

void CoralDriver::shutdown() {
    ready = false;
    detection_count = 0;
}
