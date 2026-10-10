#pragma once

#include <string>

#include "config/Config.hpp"
#include "vitals/VitalsReading.hpp"

struct TimeseriesClient {
    bool init(const Config& cfg);
    void shutdown();
    bool post_vitals(const VitalsReading& reading);
    bool last_ok() const;
    const std::string& last_error() const;

private:
    void* conn_ = nullptr;
    bool last_ok_ = false;
    std::string last_error_;
};
