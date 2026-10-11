#pragma once

#include <string>

struct Config {
    std::string tigerdata_url;
    int vitals_interval_sec = 60;
    bool timeseries_enabled = true;

    std::string stream_id = "ir_1";
    std::string video_host = "127.0.0.1";
    int video_port = 8765;
    std::string edge_secret = "hackathon-secret";
    int target_fps = 12;
    int jpeg_quality = 70;
    bool uplink_enabled = true;
    bool tracking_enabled = false;

    static Config load_from_env();
};
