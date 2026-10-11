#include "ThermalRoiSampler.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

bool ThermalRoiSampler::init() {
    return true;
}

void ThermalRoiSampler::shutdown() {}

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

            const float nx = (static_cast<float>(x) + 0.5f)
                / static_cast<float>(MLX90640Driver::width);
            const float ny = (static_cast<float>(y) + 0.5f)
                / static_cast<float>(MLX90640Driver::height);

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
        return out;
    }

    out.ambient_temp_c = percentile_sorted(outside, ambient_percentile);
    if (!std::isfinite(out.ambient_temp_c)) {
        out.ambient_temp_c = 0.0f;
    }

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
