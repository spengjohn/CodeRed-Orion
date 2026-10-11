#pragma once

#include <cstdint>

struct ADS1115Driver {
    static constexpr int channel_count = 4;
    static constexpr float full_scale_v = 4.096f;
    static constexpr float vcc_v = 3.3f;
    static constexpr float pull_down_ohm = 10000.0f;
    static constexpr float kg_per_channel_max = 10.0f;
    static constexpr float r_fsr_at_10kg_ohm = 400.0f;

    uint8_t i2c_addr = 0;
    int16_t raw[channel_count]{};
    float volts[channel_count]{};
    float kg[channel_count]{};
    float total_kg = 0.0f;
    float v_min = 0.0f;
    float v_max = 0.0f;
    int frame_index = 0;
    int hold_frames = 0;
    bool ready = false;
    bool demo_mode = true;
    bool holding = false;
    bool ever_live = false;
    char last_error[128]{};

    bool init();
    bool refresh();
    void shutdown();
};
