#pragma once

#include "AlertDetector.hpp"
#include <vector>
#include <memory>
#include <functional>
#include <chrono>
#include <iostream>

enum class AlertState {
    Normal,
    Alert
};

class AlertManager {
public:
    using TcpSendCallback = std::function<bool(
        int emergency_code,
        const std::string& detail,
        const std::string& snapshot_b64,
        int64_t timestamp
    )>;

    AlertManager() = default;

    void register_detector(std::shared_ptr<AlertDetector> detector) {
        detectors_.push_back(detector);
    }

    // Called on every main loop iteration
    void tick(const std::string& current_snapshot_b64, TcpSendCallback send_fn) {
        const auto now = std::chrono::steady_clock::now();
        const auto unix_ts = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        // 1. Advance detector ticks (sampling on interval, sliding window, checking thresholds)
        AlertEvaluation triggered_alert;
        for (auto& detector : detectors_) {
            AlertEvaluation res = detector->tick(now);
            if (res.triggered) {
                triggered_alert = res;
            }
        }

        // 2. State transition & actions
        if (state_ == AlertState::Normal) {
            if (triggered_alert.triggered) {
                state_ = AlertState::Alert;
                active_alert_ = triggered_alert;
                cached_snapshot_ = current_snapshot_b64;
                last_sent_time_ = now;

                set_buzzer(true);
                std::cout << "[AlertManager] 🚨 TRIGGERED: code=" << active_alert_.emergency_code
                          << " detail=" << active_alert_.detail << std::endl;
                send_fn(active_alert_.emergency_code, active_alert_.detail, cached_snapshot_, unix_ts);
            }
        } else if (state_ == AlertState::Alert) {
            // Re-send package every 5 minutes (300s) while in Alert state
            auto elapsed_sec = std::chrono::duration_cast<std::chrono::seconds>(now - last_sent_time_).count();
            if (elapsed_sec >= 300) {
                last_sent_time_ = now;
                std::cout << "[AlertManager] 🔁 Resending 5-min alert package..." << std::endl;
                send_fn(active_alert_.emergency_code, active_alert_.detail, current_snapshot_b64, unix_ts);
            }
        }
    }

    // Dismiss alarm (resets state back to Normal)
    void silence() {
        if (state_ == AlertState::Alert) {
            std::cout << "[AlertManager] 🔕 Alert silenced by user." << std::endl;
            state_ = AlertState::Normal;
            set_buzzer(false);
        }
    }

    bool is_alerting() const { return state_ == AlertState::Alert; }

private:
    void set_buzzer(bool active) {
        if (active) {
            std::cout << "\a[Buzzer] >>> ALARM ACTIVE <<< \a" << std::endl;
        } else {
            std::cout << "[Buzzer] Alarm silenced." << std::endl;
        }
    }

    AlertState state_{AlertState::Normal};
    std::vector<std::shared_ptr<AlertDetector>> detectors_;
    AlertEvaluation active_alert_{};
    std::string cached_snapshot_{""};
    std::chrono::steady_clock::time_point last_sent_time_{};
};

