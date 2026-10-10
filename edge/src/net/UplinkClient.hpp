#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "config/Config.hpp"

struct UplinkClient {
    bool init(const Config& cfg);
    void shutdown();
    bool post_ir_frame(const std::string& stream_id, const std::vector<std::uint8_t>& jpeg_bytes);
    bool last_ok() const;
    const std::string& last_error() const;

private:
    Config cfg_{};
    bool last_ok_ = false;
    std::string last_error_;
};
