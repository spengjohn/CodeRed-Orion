#pragma once

#include <string>

struct Config {
    std::string tigerdata_url;
    int vitals_interval_sec = 60;
    bool timeseries_enabled = true;

    std::string stream_id = "ir_1";
    int target_fps = 20;
    bool uplink_enabled = true;
    bool tracking_enabled = true;

    static Config load_from_env();
};
