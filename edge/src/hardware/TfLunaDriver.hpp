#pragma once

#include <cstdint>

struct TfLunaDriver {
    static constexpr int frame_bytes = 9;
    static constexpr int baud = 115200;
    static constexpr uint8_t header0 = 0x59;
    static constexpr uint8_t header1 = 0x59;

    uint16_t distance_mm = 0;
    uint16_t strength = 0;
    int16_t temp_c_x8 = 0;
    int frames_ok = 0;
    int frames_bad = 0;
    int frame_index = 0;
    bool ready = false;
    bool demo_mode = true;
    char port_path[64]{};
    char last_error[128]{};

    bool init();
    bool refresh();
    void shutdown();
};
