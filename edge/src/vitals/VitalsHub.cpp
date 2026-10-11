#include "VitalsHub.hpp"

#include <chrono>
#include <cmath>

namespace {

float round2(float value) {
    if (!std::isfinite(value)) {
        return 0.0f;
    }
    return std::round(value * 100.0f) / 100.0f;
}

}

bool VitalsHub::init() {
    bool ok = true;
    ok = thermal.init() && ok;
    ok = pressure.init() && ok;
    ok = roi.init() && ok;
    ok = plate.init() && ok;
    return ok;
}

bool VitalsHub::refresh() {
    const bool thermal_ok = thermal.refresh();
    pressure.refresh();
    return thermal_ok;
}

VitalsReading VitalsHub::sample() const {
    const auto now = std::chrono::system_clock::now();
    const auto unix_time = std::chrono::duration_cast<std::chrono::seconds>(
                               now.time_since_epoch())
                               .count();

    const ThermalRoiSample temps = roi.sample(thermal, nullptr);
    const PressurePlateSample load = plate.sample(pressure);

    VitalsReading reading;
    reading.unix_time = unix_time;
    reading.weight = load.valid ? round2(load.weight_kg) : 0.0f;
    reading.ambient_temp = round2(temps.ambient_temp_c);
    reading.baby_temp = 0.0f;
    reading.breaths = 0;
    reading.moved = false;
    return reading;
}

void VitalsHub::shutdown() {
    plate.shutdown();
    roi.shutdown();
    pressure.shutdown();
    thermal.shutdown();
}
