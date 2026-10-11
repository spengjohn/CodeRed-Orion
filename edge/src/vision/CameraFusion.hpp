#pragma once

#include <cstdint>
#include <vector>

#include "hardware/MLX90640Driver.hpp"
#include "hardware/OV5647.hpp"

// Thermal / IR fusion. Registration matches RowdyHacks / FusionScreen:
// thermal is mirrored in X, then shifted by thermal_*_shift_px (thermal pixels).

struct RgbFrame {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgb;
};

struct CameraFusion {
    static constexpr int out_width = 320;
    static constexpr int out_height = 240;

    float ema_alpha = 0.7f;
    float ambient_ema_alpha = 0.08f;
    float ambient_percentile = 0.20f;
    float dead_c = 0.40f;
    float cold_span_c = 3.0f;
    float warm_span_c = 4.0f;
    float thermal_alpha = 0.225f;
    // Registration in thermal pixels. Positive x = overlay right; positive y = down.
    float thermal_x_shift_px = -2.5f;
    float thermal_y_shift_px = 0.0f;

    bool init();
    void shutdown();
    bool render_ir(const OV5647& camera, RgbFrame& out);
    bool render_fusion(const OV5647& camera, const MLX90640Driver& thermal, RgbFrame& out);
    bool encode_jpeg(const RgbFrame& frame, std::vector<std::uint8_t>& jpeg_out, int quality = 80);

    // Inverse of render_fusion registration: thermal pixel -> IR normalized [0,1].
    // Used to test whether an MLX90640 pixel falls inside a Coral IR baby box.
    void thermal_pixel_to_ir_norm(int tx, int ty, float* u, float* v) const;
};
