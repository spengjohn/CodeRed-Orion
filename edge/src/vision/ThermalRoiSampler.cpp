#include "ThermalRoiSampler.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

bool ThermalRoiSampler::init() {
    return true;
}

void ThermalRoiSampler::shutdown() {}

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
    if (values.empty()) {
        return 0.0f;
    }
    float sum = 0.0f;
    for (float value : values) {
        sum += value;
    }
    return sum / static_cast<float>(values.size());
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
        outside.assign(
            thermal.pixels,
            thermal.pixels + MLX90640Driver::pixel_count
        );
    }

    out.ambient_temp_c = percentile_sorted(outside, ambient_percentile);
    if (!inside.empty()) {
        out.baby_temp_c = mean_or_zero(inside);
        out.baby_roi_valid = true;
    } else {
        out.baby_temp_c = 0.0f;
        out.baby_roi_valid = false;
    }
    return out;
}
