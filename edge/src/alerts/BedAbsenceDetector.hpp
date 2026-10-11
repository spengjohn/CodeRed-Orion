#pragma once

#include "AlertDetector.hpp"
#include <functional>

class BedAbsenceDetector : public AlertDetector {
public:
    // Directly binds to a weight provider lambda
    explicit BedAbsenceDetector(std::function<float()> weight_fn, int window_size = 10)
        : AlertDetector(std::chrono::milliseconds(500), window_size), weight_fn_(weight_fn) {}

    const char* name() const override { return "BedAbsenceDetector"; }

protected:
    float sample_current_value() override {
        return weight_fn_ ? weight_fn_() : 0.0f;
    }

    AlertEvaluation check() override {
        // Wait until window is full before evaluating
        if (!is_window_full()) {
            return {false, 0, ""};
        }

        // Trigger if weight remains below 0.4 kg throughout the window (baby absent or fall)
        for (float w : history_) {
            if (w > 0.4f) {
                return {false, 0, ""}; // Still on bed
            }
        }

        return {true, 104, "Baby absence / fall from bed detected (weight near 0)"};
    }

private:
    std::function<float()> weight_fn_;
};
