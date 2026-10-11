#pragma once

#include <cstdint>

// Google Coral USB Accelerator (Edge TPU) via libedgetpu + TensorFlow Lite.
// Live path needs -DHAS_EDGETPU (ENABLE_CORAL=1) and scripts/setup_coral_cpp.sh.
// Demo path synthesizes person boxes so tracking can run offline.
//
// Models: quantized TFLite compiled for Edge TPU (*_edgetpu.tflite).
// Default: models/tf2_ssd_mobilenet_v2_coco17_ptq_edgetpu.tflite
// Override: CORAL_MODEL=/path/to/model_edgetpu.tflite
//
// Hub: OV5647 -> pack RGB888 -> detect() -> best_person() -> BabyTrackPipeline.

struct CoralDetection {
    int class_id = -1;
    float score = 0.0f;
    float xmin = 0.0f; // normalized [0, 1]
    float ymin = 0.0f;
    float xmax = 0.0f;
    float ymax = 0.0f;
    char label[32]{};
};

struct CoralDriver {
    static constexpr int max_detections = 10;
    static constexpr int person_class_id = 0; // COCO SSD
    static constexpr float default_score_threshold = 0.40f;

    CoralDetection detections[max_detections]{};
    int detection_count = 0;
    float score_threshold = default_score_threshold;
    float inference_ms = 0.0f;
    int frame_index = 0;
    int infer_ok = 0;
    int infer_fail = 0;
    bool ready = false;
    bool demo_mode = true;
    bool stick_present = false;
    char model_path[256]{};
    char last_error[128]{};

    bool init();
    bool refresh();
    bool detect(const uint8_t* rgb, int width, int height);
    const CoralDetection* best_person() const;
    void shutdown();
};
