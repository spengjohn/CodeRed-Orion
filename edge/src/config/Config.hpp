#pragma once

#include <string>

struct Config {
    std::string tigerdata_url;
    int vitals_interval_sec = 60;
    bool timeseries_enabled = true;

    static Config load_from_env();
};
