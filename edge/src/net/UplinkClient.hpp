#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "config/Config.hpp"

struct UplinkClient {
    bool init(const Config& cfg);
    void shutdown();
    bool connect();
    bool post_ir_frame(
        const std::string& stream_id,
        const std::vector<std::uint8_t>& jpeg_bytes
    );
    bool last_ok() const;
    std::string last_error() const;

private:
    Config cfg_{};
    bool last_ok_ = false;
    std::string last_error_;
    int sock_fd_ = -1;
    bool authenticated_ = false;
    int frame_id_ = 0;
    mutable std::mutex mu_;

    void close_locked();
    bool connect_locked();
    bool send_all_locked(const char* data, std::size_t len);
    bool recv_line_locked(std::string& line_out);
};
