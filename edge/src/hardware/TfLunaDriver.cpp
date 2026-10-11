#include "TfLunaDriver.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

static int g_fd = -1;
static uint8_t g_rx[512]{};
static int g_rx_len = 0;
static bool g_live_ready = false;
static int g_empty_ticks = 0;

static constexpr int k_empty_ticks_limit = 40;
static constexpr int k_sync_frames_needed = 3;
static constexpr int k_sync_attempts = 30;
static constexpr useconds_t k_sync_sleep_us = 20 * 1000;

static const char* const k_port_candidates[] = {
    "/dev/ttyAMA1",
    "/dev/ttyAMA2",
    "/dev/ttyAMA3",
    "/dev/ttyUSB0",
    "/dev/ttyUSB1",
    "/dev/serial0",
    "/dev/ttyAMA0",
};

static uint8_t frame_checksum(const uint8_t* p) {
    unsigned sum = 0;
    for (int i = 0; i < TfLunaDriver::frame_bytes - 1; ++i) {
        sum += p[i];
    }
    return static_cast<uint8_t>(sum & 0xFF);
}

static bool parse_frame(TfLunaDriver& self, const uint8_t* pkt) {
    if (pkt[0] != TfLunaDriver::header0 || pkt[1] != TfLunaDriver::header1) {
        return false;
    }

    if (frame_checksum(pkt) != pkt[TfLunaDriver::frame_bytes - 1]) {
        ++self.frames_bad;
        return false;
    }

    self.distance_mm = static_cast<uint16_t>(pkt[2] | (pkt[3] << 8));
    self.strength = static_cast<uint16_t>(pkt[4] | (pkt[5] << 8));
    self.temp_c_x8 = static_cast<int16_t>(
        static_cast<uint16_t>(pkt[6] | (pkt[7] << 8))
    );
    ++self.frames_ok;
    return true;
}

static void consume_rx(TfLunaDriver& self) {
    if (g_fd < 0) {
        return;
    }

    while (true) {
        uint8_t chunk[128];
        const ssize_t n = ::read(g_fd, chunk, sizeof(chunk));

        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            break;
        }

        if (n == 0) {
            break;
        }

        if (g_rx_len + static_cast<int>(n) > static_cast<int>(sizeof(g_rx))) {
            const int keep = TfLunaDriver::frame_bytes - 1;
            if (g_rx_len > keep) {
                std::memmove(g_rx, g_rx + (g_rx_len - keep), static_cast<size_t>(keep));
                g_rx_len = keep;
            }
        }

        std::memcpy(g_rx + g_rx_len, chunk, static_cast<size_t>(n));
        g_rx_len += static_cast<int>(n);
    }

    int i = 0;
    while (i + TfLunaDriver::frame_bytes <= g_rx_len) {
        if (g_rx[i] != TfLunaDriver::header0
            || g_rx[i + 1] != TfLunaDriver::header1) {
            ++i;
            continue;
        }

        if (parse_frame(self, g_rx + i)) {
            i += TfLunaDriver::frame_bytes;
        } else {
            ++i;
        }
    }

    if (i > 0) {
        const int left = g_rx_len - i;
        std::memmove(g_rx, g_rx + i, static_cast<size_t>(left));
        g_rx_len = left;
    }
}

static bool configure_port(int fd) {
    termios tty{};

    if (tcgetattr(fd, &tty) != 0) {
        return false;
    }

    cfmakeraw(&tty);
    cfsetispeed(&tty, B115200);
    cfsetospeed(&tty, B115200);
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 0;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        return false;
    }

    tcflush(fd, TCIOFLUSH);
    return true;
}

static bool open_path(TfLunaDriver& self, const char* path) {
    const int fd = ::open(path, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        return false;
    }

    if (!configure_port(fd)) {
        ::close(fd);
        return false;
    }

    tcflush(fd, TCIOFLUSH);
    g_fd = fd;
    g_rx_len = 0;
    std::snprintf(self.port_path, sizeof(self.port_path), "%s", path);
    return true;
}

static bool try_open_port(TfLunaDriver& self) {
    if (const char* env = std::getenv("TFLUNA_PORT")) {
        if (env[0] != '\0' && open_path(self, env)) {
            return true;
        }
    }

    for (const char* path : k_port_candidates) {
        if (open_path(self, path)) {
            return true;
        }
    }

    std::snprintf(
        self.last_error,
        sizeof(self.last_error),
        "no UART (enable dtoverlay=uart3? tried ttyAMA1+)"
    );
    return false;
}

static bool sync_live(TfLunaDriver& self) {
    const int ok_before = self.frames_ok;

    for (int attempt = 0; attempt < k_sync_attempts; ++attempt) {
        consume_rx(self);
        if (self.frames_ok - ok_before >= k_sync_frames_needed) {
            return true;
        }
        usleep(k_sync_sleep_us);
    }

    return false;
}

static void refresh_demo(TfLunaDriver& self) {
    const float t = static_cast<float>(self.frame_index) * 0.08f;
    const float m = 1.4f + 1.1f * std::sin(t);
    self.distance_mm = static_cast<uint16_t>(m * 1000.0f);
    self.strength = static_cast<uint16_t>(180 + 40 * std::sin(t * 1.7f));
    self.temp_c_x8 = static_cast<int16_t>(25 * 8);
    ++self.frame_index;
}

static void enter_demo(TfLunaDriver& self, const char* why) {
    if (why != nullptr && why[0] != '\0') {
        std::snprintf(self.last_error, sizeof(self.last_error), "%s", why);
    }

    self.demo_mode = true;
    g_live_ready = false;
    g_empty_ticks = 0;

    if (g_fd >= 0) {
        ::close(g_fd);
        g_fd = -1;
    }
}

bool TfLunaDriver::init() {
    frame_index = 0;
    frames_ok = 0;
    frames_bad = 0;
    distance_mm = 0;
    strength = 0;
    temp_c_x8 = 0;
    port_path[0] = '\0';
    last_error[0] = '\0';
    g_live_ready = false;
    g_rx_len = 0;
    g_empty_ticks = 0;

    if (g_fd >= 0) {
        ::close(g_fd);
        g_fd = -1;
    }

    if (try_open_port(*this)) {
        if (sync_live(*this)) {
            demo_mode = false;
            ready = true;
            g_live_ready = true;
            last_error[0] = '\0';
            return true;
        }

        char why[128];
        std::snprintf(
            why,
            sizeof(why),
            "open %s but no 0x59 frames (wiring / uart3?)",
            port_path[0] ? port_path : "?"
        );
        enter_demo(*this, why);
    }

    demo_mode = true;
    ready = true;
    refresh_demo(*this);
    return true;
}

bool TfLunaDriver::refresh() {
    if (!ready) {
        return false;
    }

    if (!demo_mode && g_live_ready && g_fd >= 0) {
        const int ok_before = frames_ok;
        consume_rx(*this);
        ++frame_index;

        if (frames_ok > ok_before) {
            g_empty_ticks = 0;
            last_error[0] = '\0';
            return true;
        }

        ++g_empty_ticks;

        if (g_empty_ticks > k_empty_ticks_limit) {
            char why[128];
            std::snprintf(
                why,
                sizeof(why),
                "no TF-Luna frames on %s",
                port_path[0] ? port_path : "?"
            );
            enter_demo(*this, why);
        } else {
            return true;
        }
    }

    refresh_demo(*this);
    return true;
}

void TfLunaDriver::shutdown() {
    ready = false;
    demo_mode = true;
    frame_index = 0;
    frames_ok = 0;
    frames_bad = 0;
    distance_mm = 0;
    strength = 0;
    temp_c_x8 = 0;
    g_live_ready = false;
    g_rx_len = 0;
    g_empty_ticks = 0;
    port_path[0] = '\0';
    last_error[0] = '\0';

    if (g_fd >= 0) {
        ::close(g_fd);
        g_fd = -1;
    }
}
