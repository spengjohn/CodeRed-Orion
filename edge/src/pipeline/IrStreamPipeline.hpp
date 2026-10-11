#pragma once

#include <cstdint>
#include <vector>

#include "config/Config.hpp"
#include "hardware/OV5647.hpp"
#include "net/UplinkClient.hpp"
#include "vision/CameraFusion.hpp"

struct IrStreamPipeline {
    OV5647* camera = nullptr;
    CameraFusion* fusion = nullptr;
    UplinkClient* uplink = nullptr;
    Config cfg{};
    bool ready = false;

    RgbFrame frame{};
    std::vector<std::uint8_t> jpeg{};
    int frames_posted = 0;
    int frames_failed = 0;

    bool init(const Config& config, OV5647& cam, CameraFusion& fus, UplinkClient& client);
    bool process_frame();
    void shutdown();

    bool render_frame();
    bool encode_frame();
    bool upload_frame();
};
