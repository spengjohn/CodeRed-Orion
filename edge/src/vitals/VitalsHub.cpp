#include "VitalsHub.hpp"

#include <chrono>
#include <cmath>

namespace {

float round2(float value) {
    return std::round(value * 100.0f) / 100.0f;
}

}

bool VitalsHub::init() {
    bool ok = true;
    ok = thermal.init() && ok;
    ok = roi.init() && ok;
    return ok;
}

bool VitalsHub::refresh() {
    return thermal.refresh();
}

VitalsReading VitalsHub::sample() const {
    const auto now = std::chrono::system_clock::now();
    const auto unix_time = std::chrono::duration_cast<std::chrono::seconds>(
                               now.time_since_epoch())
                               .count();

    const ThermalRoiSample temps = roi.sample(thermal, nullptr);

    VitalsReading reading;
    reading.unix_time = unix_time;
    reading.weight = 0.0f;
    reading.ambient_temp = round2(temps.ambient_temp_c);
    reading.baby_temp = 0.0f;
    reading.breaths = 0;
    reading.moved = false;
    return reading;
}

void VitalsHub::shutdown() {
    roi.shutdown();
    thermal.shutdown();
}
