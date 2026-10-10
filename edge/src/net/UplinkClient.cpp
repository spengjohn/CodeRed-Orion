#include "UplinkClient.hpp"

bool UplinkClient::init(const Config& cfg) {
    cfg_ = cfg;
    last_ok_ = true;
    last_error_.clear();
    return true;
}

void UplinkClient::shutdown() {
    last_ok_ = false;
}

bool UplinkClient::post_ir_frame(
    const std::string& stream_id,
    const std::vector<std::uint8_t>& jpeg_bytes
) {
    (void)stream_id;
    (void)jpeg_bytes;
    last_ok_ = true;
    last_error_.clear();
    return true;
}

bool UplinkClient::last_ok() const {
    return last_ok_;
}

const std::string& UplinkClient::last_error() const {
    return last_error_;
}
