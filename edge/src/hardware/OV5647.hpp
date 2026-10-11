#pragma once

#include <cstdint>

struct OV5647 {
    static constexpr int width = 320;
    static constexpr int height = 240;
    static constexpr int pixel_count = width * height;

    uint8_t r[pixel_count]{};
    uint8_t g[pixel_count]{};
    uint8_t b[pixel_count]{};
    uint8_t v_min = 0;
    uint8_t v_max = 255;
    int frame_index = 0;
    bool ready = false;
    bool demo_mode = true;
    char last_error[128]{};

    bool init();
    bool refresh();
    void shutdown();
    bool pack_rgb888(uint8_t* rgb_out) const;
};
