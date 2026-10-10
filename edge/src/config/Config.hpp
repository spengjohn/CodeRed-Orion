#pragma once

#include <string>

struct Config {
    std::string stream_id = "crib_1";
    std::string backend_base_url = "http://127.0.0.1:5000";
    std::string timeseries_url = "http://127.0.0.1:5000/ingest/vitals";

    int vitals_interval_sec = 60;
    bool timeseries_enabled = true;
    bool uplink_enabled = false;

    static Config load_from_env();

    std::string ingest_vitals_url() const;
    std::string ingest_frame_url() const;
};
