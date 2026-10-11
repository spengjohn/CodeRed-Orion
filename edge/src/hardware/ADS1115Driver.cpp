#include "ADS1115Driver.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>

#include <errno.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>
#include <unistd.h>

static int g_fd = -1;
static const char* k_i2c_device = "/dev/i2c-1";
static char g_io_error[96]{};
static uint8_t g_addr = 0;

using SteadyClock = std::chrono::steady_clock;
using SteadyTime = SteadyClock::time_point;
static constexpr auto k_poll_period = std::chrono::milliseconds(500);
static SteadyTime g_next_poll{};

static constexpr uint8_t k_reg_conversion = 0x00;
static constexpr uint8_t k_reg_config = 0x01;

static constexpr uint8_t k_addr_try[] = {0x48, 0x4B};
static constexpr int k_addr_try_count = 2;

static constexpr uint16_t k_config_base = 0x0383;
static constexpr uint16_t k_mux_ain[ADS1115Driver::channel_count] = {
    0x4000,
    0x5000,
    0x6000,
    0x7000,
};
static constexpr useconds_t k_conv_wait_us = 10000;
static constexpr useconds_t k_gap_us = 1500;

static void close_fd() {
    if (g_fd >= 0) {
        ::close(g_fd);
        g_fd = -1;
    }
    g_addr = 0;
}

static bool ensure_open(ADS1115Driver& self) {
    if (g_fd >= 0) {
        return true;
    }

    g_fd = ::open(k_i2c_device, O_RDWR);
    if (g_fd < 0) {
        std::snprintf(
            self.last_error,
            sizeof(self.last_error),
            "open %s failed (%s)",
            k_i2c_device,
            std::strerror(errno)
        );
        return false;
    }

    return true;
}

static bool select_addr(uint8_t addr) {
    g_addr = addr;

    if (g_fd < 0) {
        return false;
    }

    if (ioctl(g_fd, I2C_SLAVE, static_cast<int>(addr)) < 0) {
        std::snprintf(
            g_io_error,
            sizeof(g_io_error),
            "I2C_SLAVE 0x%02X (%s)",
            addr,
            std::strerror(errno)
        );
        return false;
    }

    return true;
}

static bool i2c_write_bytes(const uint8_t* data, int len) {
    int written = 0;

    while (written < len) {
        const ssize_t n = ::write(g_fd, data + written, static_cast<size_t>(len - written));

        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }

            std::snprintf(
                g_io_error,
                sizeof(g_io_error),
                "write (%s)",
                std::strerror(errno)
            );
            return false;
        }

        if (n == 0) {
            std::snprintf(g_io_error, sizeof(g_io_error), "write returned 0");
            return false;
        }

        written += static_cast<int>(n);
    }

    return true;
}

static bool i2c_write_read(
    const uint8_t* wbuf,
    int wlen,
    uint8_t* rbuf,
    int rlen
) {
    if (rlen <= 0) {
        return i2c_write_bytes(wbuf, wlen);
    }

    struct i2c_msg msgs[2]{};
    msgs[0].addr = g_addr;
    msgs[0].flags = 0;
    msgs[0].len = static_cast<__u16>(wlen);
    msgs[0].buf = const_cast<__u8*>(wbuf);

    msgs[1].addr = g_addr;
    msgs[1].flags = I2C_M_RD;
    msgs[1].len = static_cast<__u16>(rlen);
    msgs[1].buf = rbuf;

    struct i2c_rdwr_ioctl_data pkt{};
    pkt.msgs = msgs;
    pkt.nmsgs = 2;

    for (;;) {
        const int rc = ioctl(g_fd, I2C_RDWR, &pkt);

        if (rc >= 0) {
            return true;
        }

        if (errno == EINTR) {
            continue;
        }

        std::snprintf(
            g_io_error,
            sizeof(g_io_error),
            "I2C_RDWR (%s)",
            std::strerror(errno)
        );
        return false;
    }
}

static bool write_reg16(uint8_t reg, uint16_t value) {
    const uint8_t buf[3] = {
        reg,
        static_cast<uint8_t>((value >> 8) & 0xFF),
        static_cast<uint8_t>(value & 0xFF),
    };
    return i2c_write_bytes(buf, 3);
}

static bool read_reg16(uint8_t reg, uint16_t* out) {
    uint8_t buf[2]{};

    if (!i2c_write_read(&reg, 1, buf, 2)) {
        return false;
    }

    *out = static_cast<uint16_t>((buf[0] << 8) | buf[1]);
    return true;
}

static bool read_channel(int channel, int16_t* raw_out) {
    if (channel < 0 || channel >= ADS1115Driver::channel_count) {
        return false;
    }

    if (!select_addr(g_addr)) {
        return false;
    }

    const uint16_t config = static_cast<uint16_t>(
        0x8000 | k_mux_ain[channel] | k_config_base
    );

    for (int attempt = 0; attempt < 2; ++attempt) {
        if (!write_reg16(k_reg_config, config)) {
            usleep(k_gap_us);
            continue;
        }

        usleep(k_conv_wait_us);

        uint16_t conv = 0;
        if (!read_reg16(k_reg_conversion, &conv)) {
            usleep(k_gap_us);
            continue;
        }

        *raw_out = static_cast<int16_t>(conv);
        g_io_error[0] = '\0';
        return true;
    }

    return false;
}

static float raw_to_volts(int16_t raw) {
    return static_cast<float>(raw)
        * (ADS1115Driver::full_scale_v / 32768.0f);
}

static float volts_to_kg(float volts) {
    constexpr float k_noise_floor_v = 0.05f;

    if (volts < k_noise_floor_v) {
        return 0.0f;
    }

    float v = volts;
    if (v > ADS1115Driver::vcc_v * 0.98f) {
        v = ADS1115Driver::vcc_v * 0.98f;
    }

    const float r_fsr =
        ADS1115Driver::pull_down_ohm
        * (ADS1115Driver::vcc_v / v - 1.0f);

    if (r_fsr < 1.0f) {
        return ADS1115Driver::kg_per_channel_max;
    }

    const float g = 1.0f / r_fsr;
    const float g_at_10kg = 1.0f / ADS1115Driver::r_fsr_at_10kg_ohm;
    float kg = ADS1115Driver::kg_per_channel_max * (g / g_at_10kg);

    if (kg < 0.0f) {
        kg = 0.0f;
    }

    if (kg > ADS1115Driver::kg_per_channel_max) {
        kg = ADS1115Driver::kg_per_channel_max;
    }

    return kg;
}

static void update_weights(ADS1115Driver& self) {
    self.total_kg = 0.0f;
    self.v_min = self.volts[0];
    self.v_max = self.volts[0];

    for (int i = 0; i < ADS1115Driver::channel_count; ++i) {
        self.kg[i] = volts_to_kg(self.volts[i]);
        self.total_kg += self.kg[i];
        self.v_min = std::min(self.v_min, self.volts[i]);
        self.v_max = std::max(self.v_max, self.volts[i]);
    }
}

static void refresh_demo(ADS1115Driver& self) {
    const float t = static_cast<float>(self.frame_index) * 0.12f;

    for (int i = 0; i < ADS1115Driver::channel_count; ++i) {
        const float phase = t + static_cast<float>(i) * 1.1f;
        const float press = 0.5f + 0.5f * std::sin(phase);
        const float v = 0.15f + press * 1.8f;
        self.volts[i] = v;
        self.raw[i] = static_cast<int16_t>(
            (v / ADS1115Driver::full_scale_v) * 32768.0f
        );
    }

    update_weights(self);
    self.holding = false;
    self.hold_frames = 0;
    ++self.frame_index;
}

static bool read_all_at(uint8_t addr, ADS1115Driver& self) {
    if (!select_addr(addr)) {
        return false;
    }

    int16_t local_raw[ADS1115Driver::channel_count]{};

    for (int ch = 0; ch < ADS1115Driver::channel_count; ++ch) {
        if (!read_channel(ch, &local_raw[ch])) {
            return false;
        }
        usleep(k_gap_us);
    }

    for (int ch = 0; ch < ADS1115Driver::channel_count; ++ch) {
        self.raw[ch] = local_raw[ch];
        self.volts[ch] = raw_to_volts(local_raw[ch]);
    }

    self.i2c_addr = addr;
    update_weights(self);
    return true;
}

static bool try_any_address(ADS1115Driver& self) {
    if (!ensure_open(self)) {
        return false;
    }

    if (self.i2c_addr == 0x48 || self.i2c_addr == 0x4B) {
        if (read_all_at(self.i2c_addr, self)) {
            return true;
        }
    }

    for (int i = 0; i < k_addr_try_count; ++i) {
        const uint8_t addr = k_addr_try[i];
        if (addr == self.i2c_addr) {
            continue;
        }
        if (read_all_at(addr, self)) {
            return true;
        }
    }

    return false;
}

bool ADS1115Driver::init() {
    frame_index = 0;
    hold_frames = 0;
    holding = false;
    ever_live = false;
    i2c_addr = 0;
    last_error[0] = '\0';
    close_fd();
    g_next_poll = SteadyTime{};

    ready = true;

    if (try_any_address(*this)) {
        demo_mode = false;
        ever_live = true;
        holding = false;
        g_next_poll = SteadyClock::now() + k_poll_period;
        ++frame_index;
        return true;
    }

    demo_mode = true;
    g_next_poll = SteadyClock::now() + k_poll_period;
    std::snprintf(
        last_error,
        sizeof(last_error),
        "no ADS @ 0x48/0x4B yet (%s) — will keep trying",
        g_io_error[0] ? g_io_error : "no ACK"
    );
    refresh_demo(*this);
    return true;
}

bool ADS1115Driver::refresh() {
    if (!ready) {
        return false;
    }

    const SteadyTime now = SteadyClock::now();

    if (g_next_poll.time_since_epoch().count() != 0 && now < g_next_poll) {
        ++frame_index;
        return true;
    }

    g_next_poll = now + k_poll_period;

    if (try_any_address(*this)) {
        demo_mode = false;
        ever_live = true;
        holding = false;
        hold_frames = 0;
        last_error[0] = '\0';
        ++frame_index;
        return true;
    }

    if (ever_live) {
        demo_mode = false;
        holding = true;
        ++hold_frames;
        std::snprintf(
            last_error,
            sizeof(last_error),
            "no ACK @ 0x48/0x4B — holding last reading (%d)",
            hold_frames
        );
        ++frame_index;
        return true;
    }

    demo_mode = true;
    holding = false;
    refresh_demo(*this);
    return true;
}

void ADS1115Driver::shutdown() {
    close_fd();
    g_next_poll = SteadyTime{};
    ready = false;
    demo_mode = true;
    holding = false;
    ever_live = false;
    hold_frames = 0;
    frame_index = 0;
    i2c_addr = 0;
    last_error[0] = '\0';
}
