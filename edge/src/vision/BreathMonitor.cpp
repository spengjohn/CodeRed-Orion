#include "BreathMonitor.hpp"

#include <cmath>

bool BreathMonitor::init() {
    breaths_this_minute = 0;
    breaths_per_min = 0;
    last_distance_mm = 0.0f;
    smooth_distance_mm = 0.0f;
    baseline_mm = 0.0f;
    distance_delta_mm = 0.0f;
    phase = Phase::Baseline;
    baseline_count = 0;
    last_breath_at = {};
    have_last_breath = false;
    have_smooth = false;
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
        have_smooth = true;
    } else {
        smooth_distance_mm =
            smooth_alpha * current + (1.0f - smooth_alpha) * smooth_distance_mm;
    }

    distance_delta_mm = smooth_distance_mm - last_distance_mm;
    last_distance_mm = smooth_distance_mm;

    if (phase == Phase::Baseline) {
        if (baseline_count == 0) {
            baseline_mm = smooth_distance_mm;
        } else {
            baseline_mm =
                baseline_alpha * smooth_distance_mm
                + (1.0f - baseline_alpha) * baseline_mm;
        }
        ++baseline_count;
        if (baseline_count >= baseline_samples) {
            phase = Phase::AtRest;
        }
        return true;
    }

    const float abs_delta = std::fabs(smooth_distance_mm - baseline_mm);

    if (phase == Phase::AtRest) {
        if (abs_delta <= return_threshold_mm) {
            baseline_mm =
                baseline_alpha * smooth_distance_mm
                + (1.0f - baseline_alpha) * baseline_mm;
        } else if (abs_delta >= depart_threshold_mm) {
            phase = Phase::Departed;
        }
        return true;
    }

    if (abs_delta <= return_threshold_mm) {
        const auto now = std::chrono::steady_clock::now();
        const bool gap_ok = !have_last_breath
            || (now - last_breath_at) >= min_breath_gap;
        if (gap_ok) {
            ++breaths_this_minute;
            breaths_per_min = breaths_this_minute;
            last_breath_at = now;
            have_last_breath = true;
        }
        baseline_mm =
            baseline_alpha * smooth_distance_mm
            + (1.0f - baseline_alpha) * baseline_mm;
        phase = Phase::AtRest;
    }

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
    phase = Phase::Baseline;
    baseline_count = 0;
    have_smooth = false;
    have_last_breath = false;
    breaths_this_minute = 0;
    breaths_per_min = 0;
}
