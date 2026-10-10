#include "config/Config.hpp"
#include "net/TimeseriesClient.hpp"
#include "vitals/VitalsHub.hpp"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <thread>

namespace {

float round2(float value) {
    return std::round(value * 100.0f) / 100.0f;
}

}

int main() {
    const Config cfg = Config::load_from_env();

    VitalsHub hub;
    if (!hub.init()) {
        return EXIT_FAILURE;
    }

    TimeseriesClient timeseries;
    if (cfg.timeseries_enabled) {
        if (!timeseries.init(cfg)) {
            std::cerr << "timeseries init failed: " << timeseries.last_error() << '\n';
            hub.shutdown();
            return EXIT_FAILURE;
        }
    }

    const int interval_sec = cfg.vitals_interval_sec > 0 ? cfg.vitals_interval_sec : 60;
    auto next_vitals_at = std::chrono::steady_clock::now()
        + std::chrono::seconds(interval_sec);

    double ambient_sum = 0.0;
    int ambient_count = 0;

    while (hub.thermal.ready) {
        if (!hub.refresh()) {
            break;
        }

        const ThermalRoiSample temps = hub.roi.sample(hub.thermal, nullptr);
        ambient_sum += static_cast<double>(temps.ambient_temp_c);
        ++ambient_count;

        const auto now = std::chrono::steady_clock::now();
        if (cfg.timeseries_enabled && now >= next_vitals_at) {
            VitalsReading reading = hub.sample();
            if (ambient_count > 0) {
                reading.ambient_temp = round2(static_cast<float>(
                    ambient_sum / static_cast<double>(ambient_count)
                ));
            }
            reading.weight = 0.0f;
            reading.baby_temp = 0.0f;
            reading.breaths = 0;
            reading.moved = false;

            const bool ok = timeseries.post_vitals(reading);
            std::cout
                << "vitals unix_time=" << reading.unix_time
                << " weight=" << reading.weight
                << " ambient_temp=" << reading.ambient_temp
                << " baby_temp=" << reading.baby_temp
                << " breaths=" << reading.breaths
                << " moved=" << (reading.moved ? 1 : 0)
                << " ok=" << (ok ? 1 : 0);
            if (!ok) {
                std::cout << " err=" << timeseries.last_error();
            }
            std::cout << '\n';

            ambient_sum = 0.0;
            ambient_count = 0;
            next_vitals_at = now + std::chrono::seconds(interval_sec);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    hub.shutdown();
    timeseries.shutdown();
    return EXIT_SUCCESS;
}
