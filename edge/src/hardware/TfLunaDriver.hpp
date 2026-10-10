#pragma once

#include <cstdint>

struct TfLunaDriver {
    uint16_t distance_mm = 0;
    uint16_t strength = 0;
    bool ready = false;
    bool demo_mode = true;
    int frame_index = 0;
    char last_error[128]{};

    bool init();
    bool refresh();
    void shutdown();
};
