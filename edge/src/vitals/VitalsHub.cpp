#include "VitalsHub.hpp"

#include <chrono>
#include <vector>

bool VitalsHub::init() {
    bool ok = true;
    ok = thermal.init() && ok;
    ok = camera.init() && ok;
    ok = coral.init() && ok;
    ok = weight.init() && ok;
    ok = lidar.init() && ok;
    ok = breath.init() && ok;
    ok = roi.init() && ok;
    ok = fusion.init() && ok;
    return ok;
}

bool VitalsHub::refresh() {
    bool ok = true;
    ok = thermal.refresh() && ok;
    ok = camera.refresh() && ok;
    ok = weight.refresh() && ok;
    ok = lidar.refresh() && ok;

    const auto now = std::chrono::system_clock::now();
    const auto unix_time = std::chrono::duration_cast<std::chrono::seconds>(
                               now.time_since_epoch())
                               .count();
    ok = breath.update(lidar.distance_mm, unix_time) && ok;

    std::vector<uint8_t> rgb(static_cast<std::size_t>(OV5647::pixel_count * 3));
    if (camera.pack_rgb888(rgb.data())) {
        ok = coral.detect(rgb.data(), OV5647::width, OV5647::height) && ok;
    }
    return ok;
}

VitalsReading VitalsHub::sample() const {
    const auto now = std::chrono::system_clock::now();
    const auto unix_time = std::chrono::duration_cast<std::chrono::seconds>(
                               now.time_since_epoch())
                               .count();

    const CoralDetection* baby = coral.best_person();
    const ThermalRoiSample temps = roi.sample(thermal, baby);

    VitalsReading reading;
    reading.ts = unix_time;
    reading.baby_weight_kg = weight.total_kg;
    reading.ambient_temp_c = temps.ambient_temp_c;
    reading.baby_temp_c = temps.baby_temp_c;
    reading.breaths_per_min = breath.breaths_per_min;
    reading.is_moving = 0;
    return reading;
}

bool VitalsHub::render_ir_jpeg(std::vector<std::uint8_t>& jpeg_out) {
    RgbFrame frame;
    if (!fusion.render_ir(camera, frame)) {
        return false;
    }
    return fusion.encode_jpeg(frame, jpeg_out);
}

void VitalsHub::shutdown() {
    fusion.shutdown();
    roi.shutdown();
    breath.shutdown();
    lidar.shutdown();
    weight.shutdown();
    coral.shutdown();
    camera.shutdown();
    thermal.shutdown();
}
