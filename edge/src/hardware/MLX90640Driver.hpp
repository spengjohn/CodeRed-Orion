#pragma once

struct MLX90640Driver {
    static constexpr int width = 32;
    static constexpr int height = 24;
    static constexpr int pixel_count = width * height;

    float pixels[pixel_count]{};
    float t_min = 0.0f;
    float t_max = 0.0f;
    bool ready = false;
    bool demo_mode = true;
    int frame_index = 0;

    bool init();
    bool refresh();
    void shutdown();
};
