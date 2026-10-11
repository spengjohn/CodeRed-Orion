#include "BreathMonitor.hpp"

#include <cmath>

bool BreathMonitor::init() {
    breaths_this_minute = 0;
    breaths_per_min = 0;
    half_cycles = 0;
    last_distance_mm = 0.0f;
    smooth_distance_mm = 0.0f;
    prev_smooth_mm = 0.0f;
    extremum_mm = 0.0f;
    distance_delta_mm = 0.0f;
    direction = 0;
    last_breath_at = {};
    have_last_breath = false;
    have_smooth = false;
    have_extremum = false;
    ready = true;
    return true;
}

bool BreathMonitor::update(uint16_t distance_mm, std::int64_t unix_time) {
    (void)unix_time;

    if (!ready) {
        return false;
    }

    if (distance_mm == 0) {
        return true;
    }

    const float current = static_cast<float>(distance_mm);
    if (!std::isfinite(current) || current <= 0.0f) {
        return true;
    }

    if (!have_smooth) {
        smooth_distance_mm = current;
        prev_smooth_mm = current;
        extremum_mm = current;
        have_smooth = true;
        have_extremum = true;
        last_distance_mm = current;
        return true;
    }

    smooth_distance_mm =
        smooth_alpha * current + (1.0f - smooth_alpha) * smooth_distance_mm;
    distance_delta_mm = smooth_distance_mm - prev_smooth_mm;

    if (std::fabs(distance_delta_mm) < noise_mm) {
        last_distance_mm = smooth_distance_mm;
        return true;
    }

    const int step_dir = (distance_delta_mm > 0.0f) ? 1 : -1;

    if (direction == 0) {
        direction = step_dir;
        extremum_mm = smooth_distance_mm;
        have_extremum = true;
        prev_smooth_mm = smooth_distance_mm;
        last_distance_mm = smooth_distance_mm;
        return true;
    }

    if (step_dir == direction) {
        if (direction > 0) {
            if (smooth_distance_mm > extremum_mm) {
                extremum_mm = smooth_distance_mm;
            }
        } else if (smooth_distance_mm < extremum_mm) {
            extremum_mm = smooth_distance_mm;
        }
        prev_smooth_mm = smooth_distance_mm;
        last_distance_mm = smooth_distance_mm;
        return true;
    }

    const float amplitude = std::fabs(smooth_distance_mm - extremum_mm);
    if (amplitude >= min_amplitude_mm) {
        ++half_cycles;
        if (half_cycles >= 2) {
            const auto now = std::chrono::steady_clock::now();
            const bool gap_ok = !have_last_breath
                || (now - last_breath_at) >= min_breath_gap;
            if (gap_ok) {
                ++breaths_this_minute;
                breaths_per_min = breaths_this_minute;
                last_breath_at = now;
                have_last_breath = true;
            }
            half_cycles = 0;
        }
        extremum_mm = smooth_distance_mm;
        have_extremum = true;
        direction = step_dir;
    }

    prev_smooth_mm = smooth_distance_mm;
    last_distance_mm = smooth_distance_mm;
    return true;
}

int BreathMonitor::consume_minute_count() {
    const int count = breaths_this_minute;
    breaths_this_minute = 0;
    breaths_per_min = 0;
    return count;
}

void BreathMonitor::shutdown() {
    ready = false;
    have_smooth = false;
    have_extremum = false;
    have_last_breath = false;
    direction = 0;
    half_cycles = 0;
    breaths_this_minute = 0;
    breaths_per_min = 0;
}
