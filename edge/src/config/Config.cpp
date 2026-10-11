#include "Config.hpp"

#include <cstdlib>
#include <fstream>
#include <string>

namespace {

void trim_inplace(std::string& value) {
    const auto start = value.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        value.clear();
        return;
    }
    const auto end = value.find_last_not_of(" \t\r\n");
    value = value.substr(start, end - start + 1);
}

void strip_quotes(std::string& value) {
    if (value.size() >= 2) {
        const char first = value.front();
        const char last = value.back();
        if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
            value = value.substr(1, value.size() - 2);
        }
    }
}

void load_dotenv_file(const char* path) {
    std::ifstream input(path);
    if (!input.is_open()) {
        return;
    }

    std::string line;
    while (std::getline(input, line)) {
        trim_inplace(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        if (line.rfind("export ", 0) == 0) {
            line = line.substr(7);
            trim_inplace(line);
        }

        const auto eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }

        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);
        trim_inplace(key);
        trim_inplace(value);
        strip_quotes(value);
        if (key.empty()) {
            continue;
        }
        if (std::getenv(key.c_str()) != nullptr) {
            continue;
        }
        setenv(key.c_str(), value.c_str(), 0);
    }
}

void load_dotenv() {
    if (const char* path = std::getenv("EDGE_ENV_FILE")) {
        load_dotenv_file(path);
        return;
    }
    load_dotenv_file(".env");
}

}

Config Config::load_from_env() {
    load_dotenv();

    Config cfg;
    if (const char* value = std::getenv("TIGERDATA_URL")) {
        cfg.tigerdata_url = value;
    }
    if (const char* value = std::getenv("VITALS_INTERVAL_SEC")) {
        cfg.vitals_interval_sec = std::atoi(value);
    }
    if (const char* value = std::getenv("TIMESERIES_ENABLED")) {
        cfg.timeseries_enabled = std::string(value) != "0";
    }
    if (const char* value = std::getenv("STREAM_ID")) {
        cfg.stream_id = value;
    }
    if (const char* value = std::getenv("VIDEO_HOST")) {
        cfg.video_host = value;
    }
    if (const char* value = std::getenv("VIDEO_PORT")) {
        cfg.video_port = std::atoi(value);
    }
    if (const char* value = std::getenv("EDGE_SECRET")) {
        cfg.edge_secret = value;
    }
    if (const char* value = std::getenv("TARGET_FPS")) {
        cfg.target_fps = std::atoi(value);
    }
    if (const char* value = std::getenv("JPEG_QUALITY")) {
        cfg.jpeg_quality = std::atoi(value);
    }
    if (const char* value = std::getenv("UPLINK_ENABLED")) {
        cfg.uplink_enabled = std::string(value) != "0";
    }
    if (const char* value = std::getenv("TRACKING_ENABLED")) {
        cfg.tracking_enabled = std::string(value) != "0";
    }
    return cfg;
}
