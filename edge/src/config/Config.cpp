#include "Config.hpp"

#include <cstdlib>

Config Config::load_from_env() {
    Config cfg;
    if (const char* value = std::getenv("STREAM_ID")) {
        cfg.stream_id = value;
    }
    if (const char* value = std::getenv("BACKEND_BASE_URL")) {
        cfg.backend_base_url = value;
    }
    if (const char* value = std::getenv("TIMESERIES_URL")) {
        cfg.timeseries_url = value;
    }
    if (const char* value = std::getenv("VITALS_INTERVAL_SEC")) {
        cfg.vitals_interval_sec = std::atoi(value);
    }
    if (const char* value = std::getenv("TIMESERIES_ENABLED")) {
        cfg.timeseries_enabled = std::string(value) != "0";
    }
    if (const char* value = std::getenv("UPLINK_ENABLED")) {
        cfg.uplink_enabled = std::string(value) != "0";
    }
    return cfg;
}

std::string Config::ingest_vitals_url() const {
    if (!timeseries_url.empty()) {
        return timeseries_url;
    }
    return backend_base_url + "/ingest/vitals";
}

std::string Config::ingest_frame_url() const {
    return backend_base_url + "/ingest/frame/jpeg?stream_id=" + stream_id;
}
