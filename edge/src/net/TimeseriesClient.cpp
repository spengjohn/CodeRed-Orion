#include "TimeseriesClient.hpp"

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>

namespace {

struct ParsedUrl {
    std::string host;
    int port = 80;
    std::string path = "/";
};

bool parse_http_url(const std::string& url, ParsedUrl& out) {
    const std::string prefix = "http://";
    if (url.rfind(prefix, 0) != 0) {
        return false;
    }
    std::string rest = url.substr(prefix.size());
    const auto slash = rest.find('/');
    std::string hostport = (slash == std::string::npos) ? rest : rest.substr(0, slash);
    out.path = (slash == std::string::npos) ? "/" : rest.substr(slash);

    const auto colon = hostport.find(':');
    if (colon == std::string::npos) {
        out.host = hostport;
        out.port = 80;
    } else {
        out.host = hostport.substr(0, colon);
        out.port = std::atoi(hostport.substr(colon + 1).c_str());
    }
    return !out.host.empty() && out.port > 0;
}

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

bool send_all(int fd, const char* data, std::size_t len) {
    std::size_t sent = 0;
    while (sent < len) {
        const ssize_t n = ::send(fd, data + sent, len - sent, 0);
        if (n <= 0) {
            return false;
        }
        sent += static_cast<std::size_t>(n);
    }
    return true;
}

std::string to_json(const VitalsReading& reading) {
    std::ostringstream body;
    body << "{"
         << "\"ts\":" << reading.ts << ","
         << "\"babyWeightKg\":" << reading.baby_weight_kg << ","
         << "\"ambientTempC\":" << reading.ambient_temp_c << ","
         << "\"babyTempC\":" << reading.baby_temp_c << ","
         << "\"breathsPerMin\":" << reading.breaths_per_min << ","
         << "\"isMoving\":" << (reading.is_moving ? "true" : "false")
         << "}";
    return body.str();
}

}

bool TimeseriesClient::init(const Config& cfg) {
    cfg_ = cfg;
    last_ok_ = true;
    last_error_.clear();
    return true;
}

void TimeseriesClient::shutdown() {
    last_ok_ = false;
}

bool TimeseriesClient::post_vitals(const VitalsReading& reading) {
    const std::string url = cfg_.ingest_vitals_url();
    const std::string json = to_json(reading);

    ParsedUrl parsed;
    if (!parse_http_url(url, parsed)) {
        last_ok_ = false;
        last_error_ = "invalid timeseries url";
        return false;
    }

    int fd = -1;
    if (!connect_tcp(parsed.host, parsed.port, fd)) {
        last_ok_ = false;
        last_error_ = "connect failed";
        return false;
    }

    std::ostringstream req;
    req << "POST " << parsed.path << " HTTP/1.1\r\n"
        << "Host: " << parsed.host << "\r\n"
        << "Content-Type: application/json\r\n"
        << "Content-Length: " << json.size() << "\r\n"
        << "Connection: close\r\n"
        << "\r\n"
        << json;

    const std::string payload = req.str();
    const bool sent = send_all(fd, payload.data(), payload.size());

    char buf[1024];
    std::string response;
    if (sent) {
        while (true) {
            const ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
            if (n <= 0) {
                break;
            }
            response.append(buf, static_cast<std::size_t>(n));
        }
    }
    ::close(fd);

    if (!sent) {
        last_ok_ = false;
        last_error_ = "send failed";
        return false;
    }

    int status = 0;
    if (response.rfind("HTTP/", 0) == 0) {
        const auto sp = response.find(' ');
        if (sp != std::string::npos) {
            status = std::atoi(response.c_str() + sp + 1);
        }
    }

    if (status < 200 || status >= 300) {
        last_ok_ = false;
        last_error_ = "http status " + std::to_string(status);
        return false;
    }

    last_ok_ = true;
    last_error_.clear();
    return true;
}

bool TimeseriesClient::last_ok() const {
    return last_ok_;
}

const std::string& TimeseriesClient::last_error() const {
    return last_error_;
}
