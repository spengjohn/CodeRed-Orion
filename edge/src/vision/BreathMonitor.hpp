#pragma once

#include <chrono>
#include <cstdint>

struct BreathMonitor {
    static constexpr float depart_threshold_mm = 3.0f;
    static constexpr float return_threshold_mm = 1.5f;
    static constexpr float smooth_alpha = 0.35f;
    static constexpr float baseline_alpha = 0.08f;
    static constexpr int baseline_samples = 20;
    static constexpr auto min_breath_gap = std::chrono::milliseconds(400);

    enum class Phase : std::uint8_t {
        Baseline = 0,
        AtRest = 1,
        Departed = 2,
    };

    int breaths_this_minute = 0;
    int breaths_per_min = 0;
    float last_distance_mm = 0.0f;
    float smooth_distance_mm = 0.0f;
    float baseline_mm = 0.0f;
    float distance_delta_mm = 0.0f;
    Phase phase = Phase::Baseline;
    int baseline_count = 0;
    std::chrono::steady_clock::time_point last_breath_at{};
    bool have_last_breath = false;
    bool have_smooth = false;
    bool ready = false;

    bool init();
    bool update(uint16_t distance_mm, std::int64_t unix_time);
    int consume_minute_count();
    void shutdown();
};
