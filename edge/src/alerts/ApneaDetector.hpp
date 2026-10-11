#pragma once

#include "AlertDetector.hpp"
#include "vision/BreathMonitor.hpp"

class ApneaDetector : public AlertDetector {
public:
    // Directly binds to an existing BreathMonitor instance
    explicit ApneaDetector(const BreathMonitor& breath, int window_size = 20)
        : AlertDetector(std::chrono::milliseconds(1000), window_size), breath_(breath) {}

    const char* name() const override { return "ApneaDetector"; }

protected:
    // Sample computed breaths per minute from BreathMonitor
    float sample_current_value() override {
        return static_cast<float>(breath_.breaths_this_minute);
    }

    AlertEvaluation check() override {
        // Wait until window is full before evaluating
        if (!is_window_full()) {
            return {false, 0, ""};
        }

        // Trigger if breath count remains zero throughout the window
        for (float bpm : history_) {
            if (bpm > 2.0f) {
                return {false, 0, ""}; // Normal breathing detected
            }
        }

        return {true, 101, "Apnea: no breathing activity for " + std::to_string(window_size) + "s"};
    }

private:
    const BreathMonitor& breath_;
};
