#pragma once

#include "AlertDetector.hpp"
#include <functional>

class AmbientThermalAlertDetector : public AlertDetector {
public:
    // Directly binds to an ambient temperature provider lambda
    explicit AmbientThermalAlertDetector(std::function<float()> ambient_fn, int window_size = 15)
        : AlertDetector(std::chrono::milliseconds(2000), window_size), ambient_fn_(ambient_fn) {}

    const char* name() const override { return "AmbientThermalAlertDetector"; }

protected:
    float sample_current_value() override {
        return ambient_fn_ ? ambient_fn_() : 0.0f;
    }

    AlertEvaluation check() override {
        if (history_.size() < 5) {
            return {false, 0, ""};
        }

        float sum = 0.0f;
        int valid_cnt = 0;
        for (float t : history_) {
            if (t > 0.0f) {
                sum += t;
                valid_cnt++;
            }
        }

        if (valid_cnt < 3) {
            return {false, 0, ""};
        }

        const float avg_ambient = sum / static_cast<float>(valid_cnt);
        if (avg_ambient > 32.0f) {
            return {true, 103, "Room temperature too hot: avg " + std::to_string(avg_ambient) + "C"};
        }
        if (avg_ambient < 16.0f) {
            return {true, 103, "Room temperature too cold: avg " + std::to_string(avg_ambient) + "C"};
        }

        return {false, 0, ""};
    }

private:
    std::function<float()> ambient_fn_;
};
