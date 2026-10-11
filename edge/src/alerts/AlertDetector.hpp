#pragma once

#include <vector>
#include <chrono>
#include <string>

struct AlertEvaluation {
    bool triggered{false};
    int emergency_code{0};
    std::string detail{""};
};

class AlertDetector {
public:
    explicit AlertDetector(std::chrono::milliseconds interval, int window_size = 30)
        : interval_(interval), window_size(window_size) {}

    virtual ~AlertDetector() = default;

    /**
     * @brief Periodic tick: verifies time interval, pops oldest on full window, pushes latest, and evaluates check().
     */
    AlertEvaluation tick(std::chrono::steady_clock::time_point now) {
        // 1. Check whether configured time interval has elapsed
        if (now - last_sample_time_ < interval_) {
            return {false, 0, ""};
        }
        last_sample_time_ = now;

        // 2. Sample current value from attached sensor
        float value = sample_current_value();

        // 3. Maintain sliding window: pop oldest when full, then push newest
        if (window_size > 0 && static_cast<int>(history_.size()) >= window_size) {
            history_.erase(history_.begin());
        }
        history_.push_back(value);

        // 4. Evaluate history vector
        return check();
    }

    size_t sample_count() const { return history_.size(); }
    bool is_window_full() const { return static_cast<int>(history_.size()) >= window_size; }
    virtual const char* name() const = 0;

    int window_size{30};

protected:
    // Pure virtual: pull latest reading from bound sensor
    virtual float sample_current_value() = 0;
    // Pure virtual: evaluate history_ vector against alert thresholds
    virtual AlertEvaluation check() = 0;

    std::chrono::milliseconds interval_;
    std::vector<float> history_;
    std::chrono::steady_clock::time_point last_sample_time_{};
};
