#pragma once

#include <chrono>
#include <cstdint>

struct BreathMonitor {
    static constexpr float min_amplitude_mm = 3.0f;
    static constexpr float noise_mm = 0.75f;
    static constexpr float smooth_alpha = 0.40f;
    static constexpr auto min_breath_gap = std::chrono::milliseconds(400);

    int breaths_this_minute = 0;
    int breaths_per_min = 0;
    int half_cycles = 0;
    float last_distance_mm = 0.0f;
    float smooth_distance_mm = 0.0f;
    float prev_smooth_mm = 0.0f;
    float extremum_mm = 0.0f;
    float distance_delta_mm = 0.0f;
    int direction = 0;
    std::chrono::steady_clock::time_point last_breath_at{};
    bool have_last_breath = false;
    bool have_smooth = false;
    bool have_extremum = false;
    bool ready = false;

    bool init();
    bool update(uint16_t distance_mm, std::int64_t unix_time);
    int consume_minute_count();
    void shutdown();
};
