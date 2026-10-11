#include "TFLiteDriver.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#if defined(__linux__) && defined(HAS_EDGETPU)

#include <edgetpu.h>
#include <memory>
#include <string>
#include <vector>

#include "tensorflow/lite/interpreter.h"
#include "tensorflow/lite/kernels/register.h"
#include "tensorflow/lite/model.h"

#endif

#if defined(__linux__) && defined(HAS_EDGETPU)

static std::shared_ptr<edgetpu::EdgeTpuContext> g_tpu_ctx;
static std::unique_ptr<tflite::FlatBufferModel> g_model;
static std::unique_ptr<tflite::Interpreter> g_interpreter;
static bool g_live_ready = false;
static int g_input_w = 0;
static int g_input_h = 0;
static int g_input_c = 0;
static TfLiteType g_input_type = kTfLiteNoType;

#endif

static const char* const k_model_candidates[] = {
    "models/tf2_ssd_mobilenet_v2_coco17_ptq_edgetpu.tflite",
    "models/ssd_mobilenet_v2_coco_quant_postprocess_edgetpu.tflite",
    "models/mobilenet_ssd_v2_coco_quant_postprocess_edgetpu.tflite",
    "/usr/share/pycoral/test_data/ssd_mobilenet_v2_coco_quant_postprocess_edgetpu.tflite",
};

static const char* coco_label(int class_id) {
    switch (class_id) {
        case 0: return "person";
        case 1: return "bicycle";
        case 2: return "car";
        case 3: return "motorcycle";
        case 15: return "cat";
        case 16: return "dog";
        default: return "object";
    }
}

static void clear_detections(TFLiteDriver& self) {
    self.detection_count = 0;
    for (int i = 0; i < TFLiteDriver::max_detections; ++i) {
        self.detections[i] = TFLiteDetection{};
    }
}

static void push_detection(
    TFLiteDriver& self,
    int class_id,
    float score,
    float ymin,
    float xmin,
    float ymax,
    float xmax
) {
    if (self.detection_count >= TFLiteDriver::max_detections) {
        return;
    }
    if (score < self.score_threshold) {
        return;
    }

    TFLiteDetection& d = self.detections[self.detection_count++];
    d.class_id = class_id;
    d.score = score;
    d.ymin = std::clamp(ymin, 0.0f, 1.0f);
    d.xmin = std::clamp(xmin, 0.0f, 1.0f);
    d.ymax = std::clamp(ymax, 0.0f, 1.0f);
    d.xmax = std::clamp(xmax, 0.0f, 1.0f);
    std::snprintf(d.label, sizeof(d.label), "%s", coco_label(class_id));
}

static void refresh_demo(TFLiteDriver& self) {
    clear_detections(self);

    const float t = self.frame_index * 0.06f;
    const float cx = 0.50f + 0.22f * std::sin(t);
    const float cy = 0.48f + 0.12f * std::cos(t * 0.85f);
    const float half_w = 0.12f;
    const float half_h = 0.22f;

    push_detection(
        self,
        TFLiteDriver::person_class_id,
        0.92f,
        cy - half_h,
        cx - half_w,
        cy + half_h,
        cx + half_w
    );

    if ((self.frame_index / 40) % 2 == 0) {
        push_detection(
            self,
            TFLiteDriver::person_class_id,
            0.55f,
            0.55f,
            0.70f,
            0.95f,
            0.95f
        );
    }

    self.inference_ms = 2.0f + 0.4f * std::sin(t);
    ++self.frame_index;
}

static void enter_demo(TFLiteDriver& self, const char* why) {
    if (why != nullptr && why[0] != '\0') {
        std::snprintf(self.last_error, sizeof(self.last_error), "%s", why);
    }

    self.demo_mode = true;
    self.stick_present = false;

#if defined(__linux__) && defined(HAS_EDGETPU)
    g_live_ready = false;
    g_interpreter.reset();
    g_model.reset();
    g_tpu_ctx.reset();
#endif
}

#if defined(__linux__) && defined(HAS_EDGETPU)

static bool resolve_model_path(TFLiteDriver& self) {
    if (const char* env = std::getenv("CORAL_MODEL")) {
        if (env[0] != '\0') {
            std::snprintf(self.model_path, sizeof(self.model_path), "%s", env);
            return true;
        }
    }

    for (const char* cand : k_model_candidates) {
        if (FILE* f = std::fopen(cand, "rb")) {
            std::fclose(f);
            std::snprintf(self.model_path, sizeof(self.model_path), "%s", cand);
            return true;
        }
    }

    self.model_path[0] = '\0';
    return false;
}

static bool fill_input_rgb(const uint8_t* rgb, int width, int height) {
    if (g_interpreter == nullptr || rgb == nullptr || width <= 0 || height <= 0) {
        return false;
    }

    TfLiteTensor* input = g_interpreter->input_tensor(0);
    if (input == nullptr) {
        return false;
    }

    auto sample = [&](int dx, int dy, int c) -> uint8_t {
        const int sx = dx * width / g_input_w;
        const int sy = dy * height / g_input_h;
        const int x = std::clamp(sx, 0, width - 1);
        const int y = std::clamp(sy, 0, height - 1);
        return rgb[(y * width + x) * 3 + c];
    };

    if (g_input_type == kTfLiteUInt8) {
        uint8_t* dst = g_interpreter->typed_input_tensor<uint8_t>(0);
        if (dst == nullptr) {
            return false;
        }

        for (int y = 0; y < g_input_h; ++y) {
            for (int x = 0; x < g_input_w; ++x) {
                const int i = (y * g_input_w + x) * g_input_c;
                dst[i + 0] = sample(x, y, 0);
                dst[i + 1] = sample(x, y, 1);
                dst[i + 2] = sample(x, y, 2);
            }
        }
        return true;
    }

    if (g_input_type == kTfLiteFloat32) {
        float* dst = g_interpreter->typed_input_tensor<float>(0);
        if (dst == nullptr) {
            return false;
        }

        for (int y = 0; y < g_input_h; ++y) {
            for (int x = 0; x < g_input_w; ++x) {
                const int i = (y * g_input_w + x) * g_input_c;
                dst[i + 0] = sample(x, y, 0) / 255.0f;
                dst[i + 1] = sample(x, y, 1) / 255.0f;
                dst[i + 2] = sample(x, y, 2) / 255.0f;
            }
        }
        return true;
    }

    return false;
}

static void parse_ssd_outputs(TFLiteDriver& self) {
    clear_detections(self);

    if (g_interpreter->outputs().size() < 3) {
        return;
    }

    const float* boxes = g_interpreter->typed_output_tensor<float>(0);
    const float* classes = g_interpreter->typed_output_tensor<float>(1);
    const float* scores = g_interpreter->typed_output_tensor<float>(2);
    if (boxes == nullptr || classes == nullptr || scores == nullptr) {
        return;
    }

    int count = 0;
    if (g_interpreter->outputs().size() >= 4) {
        if (const float* n = g_interpreter->typed_output_tensor<float>(3)) {
            count = static_cast<int>(n[0] + 0.5f);
        }
    }

    if (count <= 0) {
        count = TFLiteDriver::max_detections;
    }
    count = std::min(count, TFLiteDriver::max_detections);

    for (int i = 0; i < count; ++i) {
        const float score = scores[i];
        const int class_id = static_cast<int>(classes[i]);
        push_detection(
            self,
            class_id,
            score,
            boxes[i * 4 + 0],
            boxes[i * 4 + 1],
            boxes[i * 4 + 2],
            boxes[i * 4 + 3]
        );
    }
}

static bool try_init_live(TFLiteDriver& self) {
    g_live_ready = false;
    g_interpreter.reset();
    g_model.reset();
    g_tpu_ctx.reset();

    auto* manager = edgetpu::EdgeTpuManager::GetSingleton();
    if (manager == nullptr) {
        enter_demo(self, "EdgeTpuManager unavailable");
        return false;
    }

    const auto devices = manager->EnumerateEdgeTpu();
    if (devices.empty()) {
        enter_demo(self, "no Edge TPU found (plug Coral into USB 3.0)");
        return false;
    }

    g_tpu_ctx = manager->OpenDevice();
    if (!g_tpu_ctx) {
        enter_demo(self, "failed to open Edge TPU");
        return false;
    }

    self.stick_present = true;

    if (!resolve_model_path(self)) {
        enter_demo(
            self,
            "Edge TPU open; set CORAL_MODEL to an *_edgetpu.tflite"
        );
        self.stick_present = true;
        return false;
    }

    g_model = tflite::FlatBufferModel::BuildFromFile(self.model_path);
    if (!g_model) {
        enter_demo(self, "failed to load TFLite model");
        self.stick_present = true;
        return false;
    }

    tflite::ops::builtin::BuiltinOpResolver resolver;
    resolver.AddCustom(edgetpu::kCustomOp, edgetpu::RegisterCustomOp());

    if (tflite::InterpreterBuilder(*g_model, resolver)(&g_interpreter) != kTfLiteOk
        || g_interpreter == nullptr) {
        enter_demo(self, "InterpreterBuilder failed");
        self.stick_present = true;
        return false;
    }

    g_interpreter->SetExternalContext(kTfLiteEdgeTpuContext, g_tpu_ctx.get());
    g_interpreter->SetNumThreads(1);

    if (g_interpreter->AllocateTensors() != kTfLiteOk) {
        enter_demo(self, "AllocateTensors failed");
        self.stick_present = true;
        return false;
    }

    const TfLiteTensor* input = g_interpreter->input_tensor(0);
    if (input == nullptr || input->dims->size < 3) {
        enter_demo(self, "unexpected model input shape");
        self.stick_present = true;
        return false;
    }

    g_input_h = input->dims->data[1];
    g_input_w = input->dims->data[2];
    g_input_c = input->dims->data[3];
    g_input_type = input->type;

    if (g_input_c != 3) {
        enter_demo(self, "model input is not RGB");
        self.stick_present = true;
        return false;
    }

    g_live_ready = true;
    self.last_error[0] = '\0';
    return true;
}

static bool detect_live(
    TFLiteDriver& self,
    const uint8_t* rgb,
    int width,
    int height
) {
    if (!g_live_ready || g_interpreter == nullptr) {
        return false;
    }

    if (!fill_input_rgb(rgb, width, height)) {
        ++self.infer_fail;
        return false;
    }

    const auto t0 = std::chrono::steady_clock::now();
    const TfLiteStatus status = g_interpreter->Invoke();
    const auto t1 = std::chrono::steady_clock::now();

    self.inference_ms = std::chrono::duration<float, std::milli>(t1 - t0).count();

    if (status != kTfLiteOk) {
        ++self.infer_fail;
        return false;
    }

    parse_ssd_outputs(self);
    ++self.infer_ok;
    ++self.frame_index;
    return true;
}

#endif

bool TFLiteDriver::init() {
    frame_index = 0;
    infer_ok = 0;
    infer_fail = 0;
    inference_ms = 0.0f;
    score_threshold = default_score_threshold;
    stick_present = false;
    model_path[0] = '\0';
    last_error[0] = '\0';
    clear_detections(*this);

#if defined(__linux__) && defined(HAS_EDGETPU)
    if (try_init_live(*this)) {
        demo_mode = false;
        ready = true;
        return refresh();
    }

    demo_mode = true;
    ready = true;
    refresh_demo(*this);
    return true;
#else
    enter_demo(
        *this,
        "built without HAS_EDGETPU (demo only; see README)"
    );
    ready = true;
    refresh_demo(*this);
    return true;
#endif
}

bool TFLiteDriver::refresh() {
    if (!ready) {
        return false;
    }

    if (demo_mode) {
        refresh_demo(*this);
        return true;
    }

    return true;
}

bool TFLiteDriver::detect(const uint8_t* rgb, int width, int height) {
    if (!ready) {
        return false;
    }

#if defined(__linux__) && defined(HAS_EDGETPU)
    if (!demo_mode) {
        if (detect_live(*this, rgb, width, height)) {
            return true;
        }
        enter_demo(*this, "Edge TPU invoke failed; falling back to demo");
    }
#else
    (void)rgb;
    (void)width;
    (void)height;
#endif

    refresh_demo(*this);
    return true;
}

void TFLiteDriver::shutdown() {
    ready = false;
    demo_mode = true;
    stick_present = false;
    frame_index = 0;
    inference_ms = 0.0f;
    clear_detections(*this);

#if defined(__linux__) && defined(HAS_EDGETPU)
    g_live_ready = false;
    g_interpreter.reset();
    g_model.reset();
    g_tpu_ctx.reset();
#endif
}
