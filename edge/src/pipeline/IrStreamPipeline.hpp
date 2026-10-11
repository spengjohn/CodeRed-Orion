#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

#include "config/Config.hpp"
#include "hardware/MLX90640Driver.hpp"
#include "hardware/OV5647.hpp"
#include "net/UplinkClient.hpp"
#include "vision/CameraFusion.hpp"

struct IrStreamPipeline {
    OV5647* camera = nullptr;
    CameraFusion* fusion = nullptr;
    MLX90640Driver* thermal = nullptr;
    UplinkClient* uplink = nullptr;
    Config cfg{};
    bool ready = false;

    RgbFrame frame{};
    std::vector<std::uint8_t> jpeg{};
    int frames_posted = 0;
    int frames_failed = 0;

    bool init(
        const Config& config,
        OV5647& cam,
        CameraFusion& fus,
        UplinkClient& client,
        MLX90640Driver* therm = nullptr
    );
    bool process_frame();
    void shutdown();

    bool render_frame();
    bool encode_frame();
    bool upload_frame();

private:
    std::mutex uplink_mu_;
    std::condition_variable uplink_cv_;
    std::vector<std::uint8_t> uplink_jpeg_;
    bool uplink_has_job_ = false;
    std::atomic<bool> uplink_stop_{false};
    std::thread uplink_worker_;

    void start_worker();
    void stop_worker();
};
