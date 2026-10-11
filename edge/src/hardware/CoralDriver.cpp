/************************************************************/
/*************** IMPORT / CREATE DEPENDENCIES ***************/
/************************************************************/


/*************** IMPORT DEPENDENCIES ***************/

/***** import custom modules *****/

#include "CoralDriver.hpp"

/***** import libraries *****/

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#if defined(__linux__) && defined(HAS_EDGETPU)

#include <edgetpu.h>
#include <iostream>
#include <memory>
#include <string>

#include "tensorflow/lite/interpreter.h"
#include "tensorflow/lite/kernels/register.h"
#include "tensorflow/lite/model.h"

#endif


/*************** CREATE DEPENDENCIES ***************/

/***** live Edge TPU state (only when built with HAS_EDGETPU) *****/

#if defined(__linux__) && defined(HAS_EDGETPU)

static std::shared_ptr<edgetpu::EdgeTpuContext> g_tpu_ctx;
static std::unique_ptr<tflite::FlatBufferModel> g_model;
static std::unique_ptr<tflite::Interpreter> g_interpreter;
static bool g_live_ready = false;
static int g_input_w = 0;
static int g_input_h = 0;
static int g_input_c = 0;
static TfLiteType g_input_type = kTfLiteNoType;
static std::vector<uint8_t> g_dummy_rgb;

static void coral_log(const char* msg) {

    std::cerr << "[CoralDriver] " << msg << std::endl;
}

#endif

/***** default model candidates (SSD MobileNet COCO Edge TPU) *****/

static const char* const k_model_candidates[] = {
    "models/tf2_ssd_mobilenet_v2_coco17_ptq_edgetpu.tflite",
    "models/ssd_mobilenet_v2_coco_quant_postprocess_edgetpu.tflite",
    "models/mobilenet_ssd_v2_coco_quant_postprocess_edgetpu.tflite",
    "/usr/share/pycoral/test_data/ssd_mobilenet_v2_coco_quant_postprocess_edgetpu.tflite",
};




/****************************************************/
/******************* CORAL DRIVER *******************/
/****************************************************/


/*************** COCO SSD LABEL (subset used for status) ***************/

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


/*************** CLEAR DETECTIONS ***************/

static void clear_detections(CoralDriver& self) {

    self.detection_count = 0;

    for (int i = 0; i < CoralDriver::max_detections; ++i) {

        self.detections[i] = CoralDetection{};
    }
}


/*************** PUSH ONE DETECTION ***************/

static void push_detection(
    CoralDriver& self,
    int class_id,
    float score,
    float ymin,
    float xmin,
    float ymax,
    float xmax
) {

    if (self.detection_count >= CoralDriver::max_detections) {

        return;
    }

    if (score < self.score_threshold) {

        return;
    }

    CoralDetection& d = self.detections[self.detection_count++];
    d.class_id = class_id;
    d.score = score;
    d.ymin = std::clamp(ymin, 0.0f, 1.0f);
    d.xmin = std::clamp(xmin, 0.0f, 1.0f);
    d.ymax = std::clamp(ymax, 0.0f, 1.0f);
    d.xmax = std::clamp(xmax, 0.0f, 1.0f);
    std::snprintf(d.label, sizeof(d.label), "%s", coco_label(class_id));
}


/*************** DEMO DETECTIONS (animated person box) ***************/

static void refresh_demo(CoralDriver& self) {

    clear_detections(self);

    const float t = self.frame_index * 0.06f;
    const float cx = 0.50f + 0.22f * std::sin(t);
    const float cy = 0.48f + 0.12f * std::cos(t * 0.85f);
    const float half_w = 0.12f;
    const float half_h = 0.22f;

    push_detection(
        self,
        CoralDriver::person_class_id,
        0.92f,
        cy - half_h,
        cx - half_w,
        cy + half_h,
        cx + half_w
    );

    // Occasional second "person" so the list UI isn't always one row.
    if ((self.frame_index / 40) % 2 == 0) {

        push_detection(
            self,
            CoralDriver::person_class_id,
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


/*************** FALL BACK TO DEMO ***************/

static void enter_demo(CoralDriver& self, const char* why) {

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

/*************** RESOLVE MODEL PATH ***************/

static bool resolve_model_path(CoralDriver& self) {

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


/*************** READ INPUT SHAPE (NHWC with or without batch) ***************/

static bool read_input_shape(const TfLiteTensor* input) {

    if (input == nullptr || input->dims == nullptr) {

        return false;
    }

    g_input_type = input->type;

    // [1, H, W, 3]
    if (input->dims->size == 4) {

        g_input_h = input->dims->data[1];
        g_input_w = input->dims->data[2];
        g_input_c = input->dims->data[3];
        return g_input_h > 0 && g_input_w > 0 && g_input_c == 3;
    }

    // [H, W, 3]
    if (input->dims->size == 3) {

        g_input_h = input->dims->data[0];
        g_input_w = input->dims->data[1];
        g_input_c = input->dims->data[2];
        return g_input_h > 0 && g_input_w > 0 && g_input_c == 3;
    }

    return false;
}


/*************** FILL INPUT TENSOR FROM RGB888 ***************/

static bool fill_input_rgb(const uint8_t* rgb, int width, int height) {

    if (g_interpreter == nullptr || rgb == nullptr || width <= 0 || height <= 0) {

        return false;
    }

    if (g_input_w <= 0 || g_input_h <= 0 || g_input_c != 3) {

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

                const int i = (y * g_input_w + x) * 3;
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

                const int i = (y * g_input_w + x) * 3;
                dst[i + 0] = sample(x, y, 0) / 255.0f;
                dst[i + 1] = sample(x, y, 1) / 255.0f;
                dst[i + 2] = sample(x, y, 2) / 255.0f;
            }
        }

        return true;
    }

    return false;
}


/*************** FIND SSD OUTPUT TENSORS ***************/

// TF1 / docs order:  boxes, classes, scores, count
// TF2 export order (common): scores, boxes, count, classes
// Prefer tensor names when present; otherwise pick by shape.
static bool resolve_ssd_outputs(
    const TfLiteTensor** out_boxes,
    const TfLiteTensor** out_classes,
    const TfLiteTensor** out_scores,
    const TfLiteTensor** out_count
) {

    *out_boxes = nullptr;
    *out_classes = nullptr;
    *out_scores = nullptr;
    *out_count = nullptr;

    if (g_interpreter == nullptr) {

        return false;
    }

    const size_t n_out = g_interpreter->outputs().size();

    if (n_out < 3) {

        return false;
    }

    auto rank = [](const TfLiteTensor* t) -> int {

        return (t && t->dims) ? t->dims->size : -1;
    };

    auto last_dim = [](const TfLiteTensor* t) -> int {

        if (!t || !t->dims || t->dims->size < 1) {

            return -1;
        }

        return t->dims->data[t->dims->size - 1];
    };

    auto name_has = [](const TfLiteTensor* t, const char* key) -> bool {

        return t && t->name && std::strstr(t->name, key) != nullptr;
    };

    // Pass 1: names (most reliable).
    for (size_t i = 0; i < n_out; ++i) {

        const TfLiteTensor* t = g_interpreter->output_tensor(static_cast<int>(i));

        if (t == nullptr || t->type != kTfLiteFloat32) {

            continue;
        }

        if (name_has(t, "box")) {

            *out_boxes = t;

        } else if (name_has(t, "class")) {

            *out_classes = t;

        } else if (name_has(t, "score")) {

            *out_scores = t;

        } else if (name_has(t, "num") || name_has(t, "count") || name_has(t, "detection_count")) {

            *out_count = t;
        }
    }

    // Pass 2: shape heuristics for unnamed TF2 exports.
    if (*out_boxes == nullptr || *out_classes == nullptr || *out_scores == nullptr) {

        const TfLiteTensor* by_shape_boxes = nullptr;
        const TfLiteTensor* by_shape_count = nullptr;
        std::vector<const TfLiteTensor*> vectors;

        for (size_t i = 0; i < n_out; ++i) {

            const TfLiteTensor* t = g_interpreter->output_tensor(static_cast<int>(i));

            if (t == nullptr || t->type != kTfLiteFloat32 || t->data.f == nullptr) {

                continue;
            }

            // boxes: [1,N,4] or [N,4]
            if ((rank(t) == 3 && last_dim(t) == 4)
                || (rank(t) == 2 && last_dim(t) == 4 && t->dims->data[0] > 4)) {

                by_shape_boxes = t;
                continue;
            }

            // count: scalar-ish [1] or [1,1]
            if (t->bytes <= 4 * sizeof(float)
                && (rank(t) == 1 || rank(t) == 0
                    || (rank(t) == 2 && t->dims->data[0] == 1 && t->dims->data[1] == 1))) {

                by_shape_count = t;
                continue;
            }

            // classes / scores: [1,N] or [N]
            if (rank(t) == 1 || rank(t) == 2) {

                vectors.push_back(t);
            }
        }

        if (*out_boxes == nullptr) {

            *out_boxes = by_shape_boxes;
        }

        if (*out_count == nullptr) {

            *out_count = by_shape_count;
        }

        // Assign remaining vectors: TF2 often scores then classes by index order.
        if (vectors.size() >= 2) {

            if (*out_scores == nullptr) {

                *out_scores = vectors[0];
            }

            if (*out_classes == nullptr) {

                *out_classes = vectors[1];
            }

        } else if (vectors.size() == 1) {

            if (*out_scores == nullptr) {

                *out_scores = vectors[0];
            }
        }
    }

    // Pass 3: classic TF1 index fallback.
    if (*out_boxes == nullptr || *out_classes == nullptr || *out_scores == nullptr) {

        if (n_out >= 4) {

            // Try TF2 index order: scores, boxes, count, classes
            const TfLiteTensor* a = g_interpreter->output_tensor(0);
            const TfLiteTensor* b = g_interpreter->output_tensor(1);
            const TfLiteTensor* c = g_interpreter->output_tensor(2);
            const TfLiteTensor* d = g_interpreter->output_tensor(3);

            if (b && rank(b) == 3 && last_dim(b) == 4) {

                *out_scores = a;
                *out_boxes = b;
                *out_count = c;
                *out_classes = d;

            } else {
                // TF1: boxes, classes, scores, count
                *out_boxes = a;
                *out_classes = b;
                *out_scores = c;
                *out_count = d;
            }

        } else {

            *out_boxes = g_interpreter->output_tensor(0);
            *out_classes = g_interpreter->output_tensor(1);
            *out_scores = g_interpreter->output_tensor(2);
        }
    }

    return *out_boxes && *out_classes && *out_scores
        && (*out_boxes)->data.f && (*out_classes)->data.f && (*out_scores)->data.f;
}


/*************** PARSE SSD OUTPUTS ***************/

static void parse_ssd_outputs(CoralDriver& self) {

    clear_detections(self);

    const TfLiteTensor* t_boxes = nullptr;
    const TfLiteTensor* t_classes = nullptr;
    const TfLiteTensor* t_scores = nullptr;
    const TfLiteTensor* t_count = nullptr;

    if (!resolve_ssd_outputs(&t_boxes, &t_classes, &t_scores, &t_count)) {

        coral_log("could not resolve SSD output tensors");
        return;
    }

    static bool logged_layout = false;

    if (!logged_layout) {

        char msg[256];
        std::snprintf(
            msg,
            sizeof(msg),
            "SSD outs boxes='%s' classes='%s' scores='%s' count='%s'",
            t_boxes->name ? t_boxes->name : "?",
            t_classes->name ? t_classes->name : "?",
            t_scores->name ? t_scores->name : "?",
            (t_count && t_count->name) ? t_count->name : "-"
        );
        coral_log(msg);
        logged_layout = true;
    }

    const float* boxes = t_boxes->data.f;
    const float* classes = t_classes->data.f;
    const float* scores = t_scores->data.f;

    int max_from_shape = CoralDriver::max_detections;

    if (t_boxes->dims != nullptr && t_boxes->dims->size >= 2) {

        if (t_boxes->dims->size == 3) {

            max_from_shape = t_boxes->dims->data[1];

        } else if (t_boxes->dims->size == 2) {

            max_from_shape = t_boxes->dims->data[0];
        }
    }

    int count = 0;

    if (t_count != nullptr && t_count->data.f != nullptr) {

        count = static_cast<int>(t_count->data.f[0] + 0.5f);
    }

    if (count <= 0) {

        count = std::min(CoralDriver::max_detections, max_from_shape);
    }

    count = std::min(count, CoralDriver::max_detections);
    count = std::min(count, max_from_shape);

    for (int i = 0; i < count; ++i) {

        const float score = scores[i];

        // Reject garbage from wrong tensor wiring (e.g. num_detections as score).
        if (!(score >= 0.0f && score <= 1.0f)) {

            continue;
        }

        const float ymin = boxes[i * 4 + 0];
        const float xmin = boxes[i * 4 + 1];
        const float ymax = boxes[i * 4 + 2];
        const float xmax = boxes[i * 4 + 3];

        if (!(ymax >= ymin && xmax >= xmin)) {

            continue;
        }

        // TF2 COCO often uses 0-based class ids already; keep as int.
        const int class_id = static_cast<int>(classes[i] + 0.5f);
        push_detection(self, class_id, score, ymin, xmin, ymax, xmax);
    }
}


/*************** TRY INIT LIVE EDGE TPU ***************/

static bool try_init_live(CoralDriver& self) {

    g_live_ready = false;
    g_dummy_rgb.clear();
    // Destroy interpreter before releasing the Edge TPU context.
    g_interpreter.reset();
    g_model.reset();
    g_tpu_ctx.reset();

    coral_log("GetSingleton");
    auto* manager = edgetpu::EdgeTpuManager::GetSingleton();

    if (manager == nullptr) {

        enter_demo(self, "EdgeTpuManager unavailable");
        return false;
    }

    coral_log("EnumerateEdgeTpu");
    const auto devices = manager->EnumerateEdgeTpu();

    if (devices.empty()) {

        enter_demo(self, "no Edge TPU found (plug Coral into USB 3.0)");
        return false;
    }

    char found[96];
    std::snprintf(found, sizeof(found), "found %zu Edge TPU device(s)", devices.size());
    coral_log(found);

    coral_log("OpenDevice");
    g_tpu_ctx = manager->OpenDevice(devices[0].type, devices[0].path);

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

    char model_msg[320];
    std::snprintf(model_msg, sizeof(model_msg), "BuildFromFile %s", self.model_path);
    coral_log(model_msg);

    g_model = tflite::FlatBufferModel::BuildFromFile(self.model_path);

    if (!g_model) {

        enter_demo(self, "failed to load TFLite model");
        self.stick_present = true;
        return false;
    }

    coral_log("InterpreterBuilder + RegisterCustomOp");
    tflite::ops::builtin::BuiltinOpResolver resolver;
    resolver.AddCustom(edgetpu::kCustomOp, edgetpu::RegisterCustomOp());

    if (tflite::InterpreterBuilder(*g_model, resolver)(&g_interpreter) != kTfLiteOk
        || g_interpreter == nullptr) {

        enter_demo(self, "InterpreterBuilder failed");
        self.stick_present = true;
        return false;
    }

    coral_log("SetExternalContext");
    g_interpreter->SetExternalContext(kTfLiteEdgeTpuContext, g_tpu_ctx.get());
    g_interpreter->SetNumThreads(1);

    coral_log("AllocateTensors");
    if (g_interpreter->AllocateTensors() != kTfLiteOk) {

        enter_demo(self, "AllocateTensors failed (libedgetpu/TFLite mismatch?)");
        self.stick_present = true;
        return false;
    }

    const TfLiteTensor* input = g_interpreter->input_tensor(0);

    if (!read_input_shape(input)) {

        enter_demo(self, "unexpected model input shape");
        self.stick_present = true;
        return false;
    }

    char shape_msg[96];
    std::snprintf(
        shape_msg,
        sizeof(shape_msg),
        "input %dx%dx%d type=%d",
        g_input_w,
        g_input_h,
        g_input_c,
        static_cast<int>(g_input_type)
    );
    coral_log(shape_msg);

    g_dummy_rgb.assign(
        static_cast<size_t>(g_input_w) * static_cast<size_t>(g_input_h) * 3u,
        0
    );

    // One blank Invoke to prove the stick path before the TUI loop.
    coral_log("smoke Invoke (blank frame)");
    if (!fill_input_rgb(g_dummy_rgb.data(), g_input_w, g_input_h)) {

        enter_demo(self, "failed to fill blank input tensor");
        self.stick_present = true;
        return false;
    }

    if (g_interpreter->Invoke() != kTfLiteOk) {

        enter_demo(self, "smoke Invoke failed");
        self.stick_present = true;
        return false;
    }

    coral_log("live path ready");
    g_live_ready = true;
    self.last_error[0] = '\0';
    return true;
}


/*************** RUN LIVE INFERENCE ***************/

static bool detect_live(
    CoralDriver& self,
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

#endif // HAS_EDGETPU


/*************** INITIALIZE CORAL DRIVER ***************/

bool CoralDriver::init() {

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

    // try_init_live already called enter_demo with a reason.
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


/*************** REFRESH CORAL DRIVER ***************/

bool CoralDriver::refresh() {

    if (!ready) {

        return false;
    }

    if (demo_mode) {

        refresh_demo(*this);
        return true;
    }

#if defined(__linux__) && defined(HAS_EDGETPU)

    // No camera wired on this screen yet — run a blank frame so LIVE still
    // exercises the stick. Prefer detect(rgb,w,h) when you have real frames.
    if (!g_dummy_rgb.empty()) {

        if (detect_live(*this, g_dummy_rgb.data(), g_input_w, g_input_h)) {

            return true;
        }

        enter_demo(*this, "live Invoke failed; falling back to demo");
        refresh_demo(*this);
        return true;
    }

#endif

    return true;
}


/*************** DETECT ON RGB FRAME ***************/

bool CoralDriver::detect(const uint8_t* rgb, int width, int height) {

    if (!ready) {

        return false;
    }

#if defined(__linux__) && defined(HAS_EDGETPU)

    if (!demo_mode) {

        if (detect_live(*this, rgb, width, height)) {

            return true;
        }

        // Live invoke failed mid-run — keep UI alive with demo boxes.
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


/*************** BEST PERSON DETECTION ***************/

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


/*************** SHUT DOWN CORAL DRIVER ***************/

void CoralDriver::shutdown() {

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
