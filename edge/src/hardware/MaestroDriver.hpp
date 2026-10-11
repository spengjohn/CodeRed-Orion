#pragma once

#include <cstdint>

struct MaestroDriver {
    static constexpr int baud = 9600;
    static constexpr int servo_count = 2;
    static constexpr uint8_t channel_pan = 0;
    static constexpr uint8_t channel_tilt = 1;

    static constexpr int pulse_min_us = 1000;
    static constexpr int pulse_max_us = 2000;
    static constexpr int pulse_neutral_us = 1500;
    static constexpr int default_speed = 32;
    static constexpr int default_accel = 16;

    int target_us[servo_count]{};
    int position_us[servo_count]{};
    int speed[servo_count]{};
    int accel[servo_count]{};
    bool enabled[servo_count]{};
    bool moving = false;
    bool position_ok = false;
    int command_count = 0;
    int frame_index = 0;
    uint16_t error_flags = 0;
    bool ready = false;
    bool demo_mode = true;
    char port_path[64]{};
    char last_error[128]{};

    bool init();
    bool refresh();
    void shutdown();

    bool move_to(uint8_t channel, int pulse_us);
    bool move_to(uint8_t channel, int pulse_us, int speed_units, int accel_units);
    bool set_speed(uint8_t channel, int speed_units);
    bool set_acceleration(uint8_t channel, int accel_units);
    bool set_target_us(uint8_t channel, int pulse_us);
    bool disable_channel(uint8_t channel);
    bool go_neutral();
    bool go_home();
    bool nudge(uint8_t channel, int delta_us);
    bool center_from_error(float pan_error, float tilt_error, float gain_us = 40.0f);
};
