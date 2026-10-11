#pragma once

#include "AlertDetector.hpp"
#include "pipeline/BabyTrackPipeline.hpp"

class BabyThermalAlertDetector : public AlertDetector {
public:
    // Directly binds to an existing BabyTrackPipeline instance
    explicit BabyThermalAlertDetector(const BabyTrackPipeline& tracker, int window_size = 15)
        : AlertDetector(std::chrono::milliseconds(2000), window_size), tracker_(tracker) {}

    const char* name() const override { return "BabyThermalAlertDetector"; }

protected:
    // Sample computed baby temperature from tracked thermal ROI
    float sample_current_value() override {
        return tracker_.baby_temp_valid ? tracker_.baby_temp_c : 0.0f;
    }

    AlertEvaluation check() override {
        // Require at least 5 samples in history
        if (history_.size() < 5) {
            return {false, 0, ""};
        }

        float sum = 0.0f;
        int valid_cnt = 0;
        for (float t : history_) {
            if (t > 20.0f) { // Filter out uninitialized / invalid samples
                sum += t;
                valid_cnt++;
            }
        }

        if (valid_cnt < 3) {
            return {false, 0, ""};
        }

        const float avg_temp = sum / static_cast<float>(valid_cnt);
        if (avg_temp > 38.0f) {
            return {true, 102, "Baby fever detected: avg " + std::to_string(avg_temp) + "C"};
        }
        if (avg_temp < 35.5f) {
            return {true, 102, "Baby hypothermia detected: avg " + std::to_string(avg_temp) + "C"};
        }

        return {false, 0, ""};
    }

private:
    const BabyTrackPipeline& tracker_;
};
