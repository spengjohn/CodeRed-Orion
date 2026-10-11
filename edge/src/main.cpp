#include "config/Config.hpp"
#include "hardware/CoralDriver.hpp"
#include "hardware/MaestroDriver.hpp"
#include "hardware/OV5647.hpp"
#include "net/TimeseriesClient.hpp"
#include "net/UplinkClient.hpp"
#include "pipeline/BabyTrackPipeline.hpp"
#include "pipeline/IrStreamPipeline.hpp"
#include "vision/CameraFusion.hpp"
#include "vitals/VitalsHub.hpp"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <thread>

namespace {

float round2(float value) {
    if (!std::isfinite(value)) {
        return 0.0f;
    }
    return std::round(value * 100.0f) / 100.0f;
}

}

int main() {
    const Config cfg = Config::load_from_env();

    VitalsHub hub;
    if (!hub.init()) {
        return EXIT_FAILURE;
    }

    OV5647 camera;
    CameraFusion fusion;
    CoralDriver coral;
    MaestroDriver maestro;
    UplinkClient uplink;
    IrStreamPipeline ir_stream;
    BabyTrackPipeline baby_track;

    if (!camera.init()) {
        hub.shutdown();
        return EXIT_FAILURE;
    }
    fusion.init();
    coral.init();
    maestro.init();
    if (cfg.uplink_enabled) {
        uplink.init(cfg);
        if (!uplink.last_ok()) {
            std::cerr << "video uplink not connected yet: " << uplink.last_error() << '\n';
        }
        ir_stream.init(cfg, camera, fusion, uplink);
    }
    if (cfg.tracking_enabled) {
        baby_track.init(camera, coral, hub.thermal, maestro, hub.roi);
    }

    TimeseriesClient timeseries;
    if (cfg.timeseries_enabled) {
        if (!timeseries.init(cfg)) {
            std::cerr << "timeseries init failed: " << timeseries.last_error() << '\n';
            baby_track.shutdown();
            ir_stream.shutdown();
            uplink.shutdown();
            maestro.shutdown();
            coral.shutdown();
            fusion.shutdown();
            camera.shutdown();
            hub.shutdown();
            return EXIT_FAILURE;
        }
    }

    const int interval_sec = cfg.vitals_interval_sec > 0 ? cfg.vitals_interval_sec : 60;
    auto next_vitals_at = std::chrono::steady_clock::now()
        + std::chrono::seconds(interval_sec);

    double ambient_sum = 0.0;
    int ambient_count = 0;
    float last_good_ambient = 0.0f;
    bool have_last_good_ambient = false;

    double weight_sum = 0.0;
    int weight_count = 0;
    float last_good_weight = 0.0f;
    bool have_last_good_weight = false;

    while (hub.thermal.ready && camera.ready) {
        if (!hub.refresh()) {
            break;
        }
        if (!camera.refresh()) {
            break;
        }

        if (cfg.uplink_enabled) {
            ir_stream.process_frame();
        }
        if (cfg.tracking_enabled) {
            baby_track.process_frame();
            maestro.refresh();
        }

        const CoralDetection* baby_box = baby_track.baby;
        const ThermalRoiSample temps = hub.roi.sample(hub.thermal, baby_box);
        if (std::isfinite(temps.ambient_temp_c)) {
            ambient_sum += static_cast<double>(temps.ambient_temp_c);
            ++ambient_count;
            last_good_ambient = temps.ambient_temp_c;
            have_last_good_ambient = true;
        }

        const PressurePlateSample load = hub.plate.sample(hub.pressure);
        if (load.valid && std::isfinite(load.weight_kg)) {
            weight_sum += static_cast<double>(load.weight_kg);
            ++weight_count;
            last_good_weight = load.weight_kg;
            have_last_good_weight = true;
        }

        const auto now = std::chrono::steady_clock::now();
        if (cfg.timeseries_enabled && now >= next_vitals_at) {
            VitalsReading reading = hub.sample();

            if (ambient_count > 0) {
                reading.ambient_temp = round2(static_cast<float>(
                    ambient_sum / static_cast<double>(ambient_count)
                ));
            } else if (have_last_good_ambient) {
                reading.ambient_temp = round2(last_good_ambient);
            } else {
                reading.ambient_temp = 0.0f;
            }

            if (!std::isfinite(reading.ambient_temp)) {
                reading.ambient_temp = have_last_good_ambient
                    ? round2(last_good_ambient)
                    : 0.0f;
            }

            if (weight_count > 0) {
                reading.weight = round2(static_cast<float>(
                    weight_sum / static_cast<double>(weight_count)
                ));
            } else if (have_last_good_weight) {
                reading.weight = round2(last_good_weight);
            } else {
                reading.weight = 0.0f;
            }

            if (!std::isfinite(reading.weight)) {
                reading.weight = have_last_good_weight
                    ? round2(last_good_weight)
                    : 0.0f;
            }

            if (baby_track.baby_temp_valid) {
                reading.baby_temp = round2(baby_track.baby_temp_c);
            } else {
                reading.baby_temp = 0.0f;
            }
            reading.breaths = hub.breath.consume_minute_count();
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
            weight_sum = 0.0;
            weight_count = 0;
            next_vitals_at = now + std::chrono::seconds(interval_sec);
        }

        const int fps = cfg.target_fps > 0 ? cfg.target_fps : 20;
        std::this_thread::sleep_for(std::chrono::milliseconds(1000 / fps));
    }

    baby_track.shutdown();
    ir_stream.shutdown();
    uplink.shutdown();
    maestro.shutdown();
    coral.shutdown();
    fusion.shutdown();
    camera.shutdown();
    hub.shutdown();
    timeseries.shutdown();
    return EXIT_SUCCESS;
}
