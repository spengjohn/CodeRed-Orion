#include "config/Config.hpp"
#include "net/TimeseriesClient.hpp"
#include "net/UplinkClient.hpp"
#include "vitals/VitalsHub.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

int main() {
    const Config cfg = Config::load_from_env();

    VitalsHub hub;
    if (!hub.init()) {
        return EXIT_FAILURE;
    }

    TimeseriesClient timeseries;
    if (cfg.timeseries_enabled) {
        timeseries.init(cfg);
    }

    UplinkClient uplink;
    if (cfg.uplink_enabled) {
        uplink.init(cfg);
    }

    auto next_vitals_at = std::chrono::steady_clock::now();

    while (hub.thermal.ready) {
        if (!hub.refresh()) {
            break;
        }

        const auto now = std::chrono::steady_clock::now();
        if (cfg.timeseries_enabled && now >= next_vitals_at) {
            const VitalsReading reading = hub.sample();
            const bool ok = timeseries.post_vitals(reading);
            std::cout
                << "vitals ts=" << reading.ts
                << " ambient=" << reading.ambient_temp_c
                << " baby=" << reading.baby_temp_c
                << " weight=" << reading.baby_weight_kg
                << " bpm=" << reading.breaths_per_min
                << " ok=" << (ok ? 1 : 0);
            if (!ok) {
                std::cout << " err=" << timeseries.last_error();
            }
            std::cout << '\n';

            next_vitals_at = now + std::chrono::seconds(
                cfg.vitals_interval_sec > 0 ? cfg.vitals_interval_sec : 60
            );
        }

        if (cfg.uplink_enabled) {
            std::vector<std::uint8_t> jpeg;
            if (hub.render_ir_jpeg(jpeg)) {
                uplink.post_ir_frame(cfg.stream_id, jpeg);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    hub.shutdown();
    timeseries.shutdown();
    uplink.shutdown();
    return EXIT_SUCCESS;
}
