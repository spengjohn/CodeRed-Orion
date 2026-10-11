#include "UplinkClient.hpp"

#include "util/Base64.hpp"

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <sstream>

namespace {

bool connect_tcp(const std::string& host, int port, int& fd_out) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    const std::string port_str = std::to_string(port);
    if (getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res) != 0 || res == nullptr) {
        return false;
    }

    int fd = -1;
    for (addrinfo* p = res; p != nullptr; p = p->ai_next) {
        fd = static_cast<int>(::socket(p->ai_family, p->ai_socktype, p->ai_protocol));
        if (fd < 0) {
            continue;
        }
        if (::connect(fd, p->ai_addr, p->ai_addrlen) == 0) {
            break;
        }
        ::close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    if (fd < 0) {
        return false;
    }
    fd_out = fd;
    return true;
}

}

void UplinkClient::close_locked() {
    if (sock_fd_ >= 0) {
        ::close(sock_fd_);
        sock_fd_ = -1;
    }
    authenticated_ = false;
}

bool UplinkClient::send_all_locked(const char* data, std::size_t len) {
    std::size_t sent = 0;
    while (sent < len) {
        const ssize_t n = ::send(sock_fd_, data + sent, len - sent, 0);
        if (n <= 0) {
            return false;
        }
        sent += static_cast<std::size_t>(n);
    }
    return true;
}

bool UplinkClient::recv_line_locked(std::string& line_out) {
    line_out.clear();
    char ch = 0;
    while (true) {
        const ssize_t n = ::recv(sock_fd_, &ch, 1, 0);
        if (n <= 0) {
            return false;
        }
        if (ch == '\n') {
            break;
        }
        if (ch != '\r') {
            line_out.push_back(ch);
        }
        if (line_out.size() > 256) {
            return false;
        }
    }
    return true;
}

bool UplinkClient::connect_locked() {
    close_locked();

    int fd = -1;
    if (!connect_tcp(cfg_.video_host, cfg_.video_port, fd)) {
        last_error_ = "tcp connect failed";
        return false;
    }
    sock_fd_ = fd;

    const std::string auth = "AUTH " + cfg_.edge_secret + "\n";
    if (!send_all_locked(auth.data(), auth.size())) {
        close_locked();
        last_error_ = "auth send failed";
        return false;
    }

    std::string reply;
    if (!recv_line_locked(reply)) {
        close_locked();
        last_error_ = "auth reply failed";
        return false;
    }
    if (reply != "AUTH_OK") {
        close_locked();
        last_error_ = "auth rejected: " + reply;
        return false;
    }

    authenticated_ = true;
    last_error_.clear();
    return true;
}

bool UplinkClient::init(const Config& cfg) {
    std::lock_guard<std::mutex> lock(mu_);
    cfg_ = cfg;
    last_ok_ = false;
    last_error_.clear();
    frame_id_ = 0;
    last_ok_ = connect_locked();
    return true;
}

void UplinkClient::shutdown() {
    std::lock_guard<std::mutex> lock(mu_);
    close_locked();
    last_ok_ = false;
}

bool UplinkClient::connect() {
    std::lock_guard<std::mutex> lock(mu_);
    const bool ok = connect_locked();
    last_ok_ = ok;
    return ok;
}

bool UplinkClient::post_ir_frame(
    const std::string& stream_id,
    const std::vector<std::uint8_t>& jpeg_bytes
) {
    (void)stream_id;
    std::lock_guard<std::mutex> lock(mu_);

    if (jpeg_bytes.empty()) {
        last_ok_ = false;
        last_error_ = "empty jpeg";
        return false;
    }

    if (!authenticated_ && !connect_locked()) {
        last_ok_ = false;
        return false;
    }

    ++frame_id_;
    const auto now = std::chrono::system_clock::now();
    const auto unix_time = std::chrono::duration_cast<std::chrono::seconds>(
                               now.time_since_epoch())
                               .count();
    const std::string b64 = base64::encode(jpeg_bytes);

    std::ostringstream body;
    body << "{\"ts\":" << unix_time
         << ",\"frame\":\"" << b64
         << "\",\"frame_id\":" << frame_id_
         << "}\n";
    const std::string payload = body.str();

    if (!send_all_locked(payload.data(), payload.size())) {
        close_locked();
        if (!connect_locked() || !send_all_locked(payload.data(), payload.size())) {
            last_ok_ = false;
            last_error_ = "frame send failed";
            return false;
        }
    }

    last_ok_ = true;
    last_error_.clear();
    return true;
}

bool UplinkClient::last_ok() const {
    std::lock_guard<std::mutex> lock(mu_);
    return last_ok_;
}

std::string UplinkClient::last_error() const {
    std::lock_guard<std::mutex> lock(mu_);
    return last_error_;
}
