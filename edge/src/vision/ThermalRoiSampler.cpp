#include "ThermalRoiSampler.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

bool ThermalRoiSampler::init() {
    return true;
}

void ThermalRoiSampler::shutdown() {}

void ThermalRoiSampler::sync_registration(const CameraFusion& fusion) {
    thermal_x_shift_px = fusion.thermal_x_shift_px;
    thermal_y_shift_px = fusion.thermal_y_shift_px;
}

static bool finite_temp(float value) {
    return std::isfinite(value);
}

static float percentile_sorted(std::vector<float>& values, float percentile) {
    if (values.empty()) {
        return 0.0f;
    }
    std::sort(values.begin(), values.end());
    float clamped = percentile;
    if (clamped < 0.0f) {
        clamped = 0.0f;
    }
    if (clamped > 1.0f) {
        clamped = 1.0f;
    }
    const std::size_t index = static_cast<std::size_t>(
        clamped * static_cast<float>(values.size() - 1)
    );
    return values[index];
}

static float mean_or_zero(const std::vector<float>& values) {
    double sum = 0.0;
    int count = 0;
    for (float value : values) {
        if (!finite_temp(value)) {
            continue;
        }
        sum += static_cast<double>(value);
        ++count;
    }
    if (count == 0) {
        return 0.0f;
    }
    return static_cast<float>(sum / static_cast<double>(count));
}

static bool point_in_box(
    float nx,
    float ny,
    const CoralDetection& box
) {
    return nx >= box.xmin && nx <= box.xmax && ny >= box.ymin && ny <= box.ymax;
}

// Same inverse registration as CameraFusion::thermal_pixel_to_ir_norm.
static void thermal_pixel_to_ir_norm(
    int tx,
    int ty,
    float x_shift_px,
    float y_shift_px,
    float* u,
    float* v
) {
    const float th_w = static_cast<float>(MLX90640Driver::width);
    const float th_h = static_cast<float>(MLX90640Driver::height);
    const float u_th = 1.0f - (static_cast<float>(tx) + 0.5f) / th_w;
    const float v_th = (static_cast<float>(ty) + 0.5f) / th_h;
    *u = u_th + (x_shift_px / th_w);
    *v = v_th + (y_shift_px / th_h);
}

ThermalRoiSample ThermalRoiSampler::sample(const MLX90640Driver& thermal) const {
    return sample(thermal, nullptr);
}

ThermalRoiSample ThermalRoiSampler::sample(
    const MLX90640Driver& thermal,
    const CoralDetection* baby_box
) const {
    ThermalRoiSample out;
    std::vector<float> outside;
    std::vector<float> inside;
    outside.reserve(MLX90640Driver::pixel_count);
    inside.reserve(MLX90640Driver::pixel_count);

    for (int y = 0; y < MLX90640Driver::height; ++y) {
        for (int x = 0; x < MLX90640Driver::width; ++x) {
            const float temp = thermal.pixels[y * MLX90640Driver::width + x];
            if (!finite_temp(temp)) {
                continue;
            }

            float nx = 0.0f;
            float ny = 0.0f;
            thermal_pixel_to_ir_norm(
                x, y, thermal_x_shift_px, thermal_y_shift_px, &nx, &ny
            );

            if (baby_box != nullptr && point_in_box(nx, ny, *baby_box)) {
                inside.push_back(temp);
            } else {
                outside.push_back(temp);
            }
        }
    }

    if (outside.empty()) {
        for (int i = 0; i < MLX90640Driver::pixel_count; ++i) {
            if (finite_temp(thermal.pixels[i])) {
                outside.push_back(thermal.pixels[i]);
            }
        }
    }

    if (outside.empty()) {
        out.ambient_temp_c = 0.0f;
        out.baby_temp_c = 0.0f;
        out.baby_roi_valid = false;
        out.baby_pixel_count = 0;
        return out;
    }

    out.ambient_temp_c = percentile_sorted(outside, ambient_percentile);
    if (!std::isfinite(out.ambient_temp_c)) {
        out.ambient_temp_c = 0.0f;
    }

    out.baby_pixel_count = static_cast<int>(inside.size());
    if (!inside.empty()) {
        out.baby_temp_c = mean_or_zero(inside);
        out.baby_roi_valid = std::isfinite(out.baby_temp_c);
        if (!out.baby_roi_valid) {
            out.baby_temp_c = 0.0f;
        }
    } else {
        out.baby_temp_c = 0.0f;
        out.baby_roi_valid = false;
    }
    return out;
}
