#pragma once

#include <cstdint>

struct ADS1115Driver {
    static constexpr int channel_count = 4;

    float kg[channel_count]{};
    float total_kg = 0.0f;
    bool ready = false;
    bool demo_mode = true;
    int frame_index = 0;
    char last_error[128]{};

    bool init();
    bool refresh();
    void shutdown();
};
