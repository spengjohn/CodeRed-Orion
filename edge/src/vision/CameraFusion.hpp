#pragma once

#include <cstdint>
#include <vector>

#include "hardware/MLX90640Driver.hpp"
#include "hardware/OV5647.hpp"

struct RgbFrame {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgb;
};

struct CameraFusion {
    static constexpr int out_width = 320;
    static constexpr int out_height = 240;

    float thermal_alpha = 0.225f;
    float thermal_x_shift_px = -2.5f;
    float thermal_y_shift_px = 0.0f;

    bool init();
    void shutdown();
    bool render_ir(const OV5647& camera, RgbFrame& out);
    bool render_fusion(const OV5647& camera, const MLX90640Driver& thermal, RgbFrame& out);
    bool encode_jpeg(const RgbFrame& frame, std::vector<std::uint8_t>& jpeg_out, int quality = 80);
};
