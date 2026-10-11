/************************************************************/
/*************** IMPORT / CREATE DEPENDENCIES ***************/
/************************************************************/


/*************** IMPORT DEPENDENCIES ***************/

/***** import custom modules *****/

#include "MaestroDriver.hpp"

/***** import libraries *****/

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>


/*************** CREATE DEPENDENCIES ***************/

/***** live serial state *****/

static int g_fd = -1;
static bool g_live_ready = false;

/***** primary UART (pins 8/10) first, then USB fallbacks *****/

static const char* const k_port_candidates[] = {
    "/dev/serial0",
    "/dev/ttyAMA0",
    "/dev/ttyUSB0",
    "/dev/ttyUSB1",
    "/dev/ttyACM0",
};

static constexpr int k_read_timeout_ms = 80;




/****************************************************/
/****************** MAESTRO DRIVER ******************/
/****************************************************/


/*************** CHANNEL INDEX HELPERS ***************/

static int channel_index(uint8_t channel) {

    if (channel == MaestroDriver::channel_pan) {

        return 0;
    }

    if (channel == MaestroDriver::channel_tilt) {

        return 1;
    }

    return -1;
}


static int clamp_pulse_us(int pulse_us) {

    return std::max(

        MaestroDriver::pulse_min_us,
        std::min(MaestroDriver::pulse_max_us, pulse_us)
    );
}


/*************** CONFIGURE TERMIOS @ 9600 8N1 ***************/

static bool configure_port(int fd) {

    termios tty{};

    if (tcgetattr(fd, &tty) != 0) {

        return false;
    }

    cfmakeraw(&tty);
    cfsetispeed(&tty, B9600);
    cfsetospeed(&tty, B9600);
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


/*************** OPEN ONE PATH ***************/

static bool open_path(MaestroDriver& self, const char* path) {

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
    std::snprintf(self.port_path, sizeof(self.port_path), "%s", path);
    return true;
}


/*************** TRY OPEN PRIMARY UART / FALLBACKS ***************/

static bool try_open_port(MaestroDriver& self) {

    if (const char* env = std::getenv("MAESTRO_PORT")) {

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
        "no UART (pins 8/10 serial0? tried serial0/AMA0/USB)"
    );

    return false;
}


/*************** WRITE BYTES TO MAESTRO ***************/

static bool write_bytes(const uint8_t* data, size_t len) {

    if (g_fd < 0 || data == nullptr || len == 0) {

        return false;
    }

    size_t sent = 0;

    while (sent < len) {

        const ssize_t n = ::write(g_fd, data + sent, len - sent);

        if (n < 0) {

            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {

                usleep(1000);
                continue;
            }

            return false;
        }

        if (n == 0) {

            return false;
        }

        sent += static_cast<size_t>(n);
    }

    // Drain TX so Compact Protocol commands aren't coalesced oddly.
    tcdrain(g_fd);
    return true;
}


/*************** READ EXACT BYTES (poll + short timeout) ***************/

static bool read_exact(uint8_t* data, size_t len, int timeout_ms) {

    if (g_fd < 0 || data == nullptr || len == 0) {

        return false;
    }

    size_t got = 0;

    while (got < len) {

        pollfd pfd{};
        pfd.fd = g_fd;
        pfd.events = POLLIN;

        const int pr = ::poll(&pfd, 1, timeout_ms);

        if (pr < 0) {

            if (errno == EINTR) {

                continue;
            }

            return false;
        }

        if (pr == 0) {

            return false; // timeout
        }

        const ssize_t n = ::read(g_fd, data + got, len - got);

        if (n < 0) {

            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {

                continue;
            }

            return false;
        }

        if (n == 0) {

            return false;
        }

        got += static_cast<size_t>(n);
        timeout_ms = 20; // remaining bytes should arrive quickly
    }

    return true;
}


/*************** COMPACT PROTOCOL: BAUD INDICATOR ***************/

static bool send_baud_indication() {

    // First byte after open tells the Maestro the bit rate (0xAA).
    const uint8_t aa = 0xAA;

    if (!write_bytes(&aa, 1)) {

        return false;
    }

    usleep(100 * 1000);
    return true;
}


/*************** COMPACT PROTOCOL: SET SPEED ***************/

static bool write_speed(uint8_t channel, int speed_units) {

    speed_units = std::max(0, std::min(16383, speed_units));
    const uint8_t cmd[4] = {
        0x87,
        channel,
        static_cast<uint8_t>(speed_units & 0x7F),
        static_cast<uint8_t>((speed_units >> 7) & 0x7F),
    };
    return write_bytes(cmd, sizeof(cmd));
}


/*************** COMPACT PROTOCOL: SET ACCELERATION ***************/

static bool write_acceleration(uint8_t channel, int accel_units) {

    accel_units = std::max(0, std::min(255, accel_units));
    const uint8_t cmd[4] = {
        0x89,
        channel,
        static_cast<uint8_t>(accel_units & 0x7F),
        static_cast<uint8_t>((accel_units >> 7) & 0x7F),
    };
    return write_bytes(cmd, sizeof(cmd));
}


/*************** COMPACT PROTOCOL: SET TARGET (¼ µs) ***************/

static bool write_target_quarter_us(uint8_t channel, int quarter_us) {

    quarter_us = std::max(0, std::min(16383, quarter_us));
    const uint8_t cmd[4] = {
        0x84,
        channel,
        static_cast<uint8_t>(quarter_us & 0x7F),
        static_cast<uint8_t>((quarter_us >> 7) & 0x7F),
    };
    return write_bytes(cmd, sizeof(cmd));
}


/*************** COMPACT PROTOCOL: GET POSITION ***************/

static bool read_position_quarter_us(uint8_t channel, int* quarter_us_out) {

    const uint8_t cmd[2] = {0x90, channel};

    if (!write_bytes(cmd, sizeof(cmd))) {

        return false;
    }

    uint8_t resp[2]{};

    if (!read_exact(resp, sizeof(resp), k_read_timeout_ms)) {

        return false;
    }

    // Little-endian 16-bit (unlike the 7-bit packing used for Set Target).
    *quarter_us_out = static_cast<int>(resp[0] | (resp[1] << 8));
    return true;
}


/*************** COMPACT PROTOCOL: GET MOVING STATE ***************/

static bool read_moving_state(bool* moving_out) {

    const uint8_t cmd = 0x93;

    if (!write_bytes(&cmd, 1)) {

        return false;
    }

    uint8_t resp = 0;

    if (!read_exact(&resp, 1, k_read_timeout_ms)) {

        return false;
    }

    *moving_out = (resp != 0);
    return true;
}


/*************** COMPACT PROTOCOL: GET ERRORS ***************/

static bool read_errors(uint16_t* flags_out) {

    const uint8_t cmd = 0xA1;

    if (!write_bytes(&cmd, 1)) {

        return false;
    }

    uint8_t resp[2]{};

    if (!read_exact(resp, sizeof(resp), k_read_timeout_ms)) {

        return false;
    }

    *flags_out = static_cast<uint16_t>(resp[0] | (resp[1] << 8));
    return true;
}


/*************** COMPACT PROTOCOL: GO HOME ***************/

static bool write_go_home() {

    const uint8_t cmd = 0xA2;
    return write_bytes(&cmd, 1);
}


/*************** FALL BACK TO DEMO ***************/

static void enter_demo(MaestroDriver& self, const char* why) {

    if (why != nullptr && why[0] != '\0') {

        std::snprintf(self.last_error, sizeof(self.last_error), "%s", why);
    }

    self.demo_mode = true;
    self.position_ok = false;
    self.moving = false;
    g_live_ready = false;

    if (g_fd >= 0) {

        ::close(g_fd);
        g_fd = -1;
    }
}


/*************** RESET LOCAL SERVO STATE ***************/

static void reset_servo_state(MaestroDriver& self) {

    for (int i = 0; i < MaestroDriver::servo_count; ++i) {

        self.target_us[i] = MaestroDriver::pulse_neutral_us;
        self.position_us[i] = MaestroDriver::pulse_neutral_us;
        self.speed[i] = MaestroDriver::default_speed;
        self.accel[i] = MaestroDriver::default_accel;
        self.enabled[i] = false;
    }

    self.moving = false;
    self.position_ok = false;
    self.error_flags = 0;
}


/*************** APPLY SPEED + ACCEL + TARGET ***************/

static bool write_move(

    uint8_t channel,
    int pulse_us,
    int speed_units,
    int accel_units
) {

    if (!write_speed(channel, speed_units)) {

        return false;
    }

    if (!write_acceleration(channel, accel_units)) {

        return false;
    }

    return write_target_quarter_us(channel, pulse_us * 4);
}


/*************** POLL FEEDBACK FROM MAESTRO ***************/

static bool poll_feedback(MaestroDriver& self) {

    if (g_fd < 0) {

        return false;
    }

    bool any_pos = false;

    for (int i = 0; i < MaestroDriver::servo_count; ++i) {

        const uint8_t ch = (i == 0)
            ? MaestroDriver::channel_pan
            : MaestroDriver::channel_tilt;

        int quarter = 0;

        if (read_position_quarter_us(ch, &quarter)) {

            self.position_us[i] = quarter / 4;
            any_pos = true;
        }
    }

    self.position_ok = any_pos;

    bool moving = false;

    if (read_moving_state(&moving)) {

        self.moving = moving;
    }

    uint16_t flags = 0;

    if (read_errors(&flags)) {

        self.error_flags = flags;
    }

    return any_pos;
}


/*************** INITIALIZE MAESTRO DRIVER ***************/

bool MaestroDriver::init() {

    frame_index = 0;
    command_count = 0;
    port_path[0] = '\0';
    last_error[0] = '\0';
    g_live_ready = false;
    reset_servo_state(*this);

    if (g_fd >= 0) {

        ::close(g_fd);
        g_fd = -1;
    }

    if (try_open_port(*this)) {

        if (send_baud_indication()) {

            // Clear leftover errors, apply motion limits, park at neutral.
            uint16_t flags = 0;
            read_errors(&flags);
            error_flags = flags;

            write_speed(channel_pan, default_speed);
            write_speed(channel_tilt, default_speed);
            write_acceleration(channel_pan, default_accel);
            write_acceleration(channel_tilt, default_accel);

            demo_mode = false;
            ready = true;
            g_live_ready = true;
            last_error[0] = '\0';

            // Enable both channels at neutral so the camera mount is ready.
            move_to(channel_pan, pulse_neutral_us);
            move_to(channel_tilt, pulse_neutral_us);
            poll_feedback(*this);
            return true;
        }

        char why[128];
        std::snprintf(

            why,
            sizeof(why),
            "open %s but baud 0xAA failed",
            port_path[0] ? port_path : "?"
        );
        enter_demo(*this, why);
    }

    demo_mode = true;
    ready = true;
    return true;
}


/*************** REFRESH MAESTRO DRIVER ***************/

bool MaestroDriver::refresh() {

    if (!ready) {

        return false;
    }

    ++frame_index;

    if (!demo_mode && g_live_ready && g_fd >= 0) {

        if (!poll_feedback(*this)) {

            // TX-only wiring still allows control; don't force demo on miss.
            position_ok = false;
        }
    } else if (demo_mode) {

        // Soft-follow targets in demo so the UI looks alive.
        for (int i = 0; i < servo_count; ++i) {

            const int delta = target_us[i] - position_us[i];
            const int step = std::max(-20, std::min(20, delta));
            position_us[i] += step;
        }

        moving = (position_us[0] != target_us[0]) || (position_us[1] != target_us[1]);
        position_ok = true;
    }

    return true;
}


/*************** MOVE TO (speed + accel + target) ***************/

bool MaestroDriver::move_to(uint8_t channel, int pulse_us) {

    const int idx = channel_index(channel);

    if (idx < 0 || !ready) {

        return false;
    }

    return move_to(channel, pulse_us, speed[idx], accel[idx]);
}


bool MaestroDriver::move_to(

    uint8_t channel,
    int pulse_us,
    int speed_units,
    int accel_units
) {

    const int idx = channel_index(channel);

    if (idx < 0 || !ready) {

        return false;
    }

    pulse_us = clamp_pulse_us(pulse_us);
    speed_units = std::max(0, std::min(16383, speed_units));
    accel_units = std::max(0, std::min(255, accel_units));

    target_us[idx] = pulse_us;
    speed[idx] = speed_units;
    accel[idx] = accel_units;
    enabled[idx] = true;

    if (demo_mode || !g_live_ready || g_fd < 0) {

        ++command_count;
        return true;
    }

    if (!write_move(channel, pulse_us, speed_units, accel_units)) {

        enter_demo(*this, "move_to write failed");
        return false;
    }

    ++command_count;
    return true;
}


/*************** SET SPEED ***************/

bool MaestroDriver::set_speed(uint8_t channel, int speed_units) {

    const int idx = channel_index(channel);

    if (idx < 0 || !ready) {

        return false;
    }

    speed_units = std::max(0, std::min(16383, speed_units));
    speed[idx] = speed_units;

    if (demo_mode || !g_live_ready || g_fd < 0) {

        ++command_count;
        return true;
    }

    if (!write_speed(channel, speed_units)) {

        enter_demo(*this, "speed write failed");
        return false;
    }

    ++command_count;
    return true;
}


/*************** SET ACCELERATION ***************/

bool MaestroDriver::set_acceleration(uint8_t channel, int accel_units) {

    const int idx = channel_index(channel);

    if (idx < 0 || !ready) {

        return false;
    }

    accel_units = std::max(0, std::min(255, accel_units));
    accel[idx] = accel_units;

    if (demo_mode || !g_live_ready || g_fd < 0) {

        ++command_count;
        return true;
    }

    if (!write_acceleration(channel, accel_units)) {

        enter_demo(*this, "accel write failed");
        return false;
    }

    ++command_count;
    return true;
}


/*************** SET TARGET (µs) ***************/

bool MaestroDriver::set_target_us(uint8_t channel, int pulse) {

    // Keep the Neuralink ordering: always send speed + accel with the target.
    return move_to(channel, pulse);
}


/*************** DISABLE CHANNEL (target 0) ***************/

bool MaestroDriver::disable_channel(uint8_t channel) {

    const int idx = channel_index(channel);

    if (idx < 0 || !ready) {

        return false;
    }

    enabled[idx] = false;

    if (demo_mode || !g_live_ready || g_fd < 0) {

        ++command_count;
        return true;
    }

    if (!write_target_quarter_us(channel, 0)) {

        enter_demo(*this, "disable write failed");
        return false;
    }

    ++command_count;
    return true;
}


/*************** BOTH SERVOS TO NEUTRAL ***************/

bool MaestroDriver::go_neutral() {

    bool ok = true;
    ok = move_to(channel_pan, pulse_neutral_us) && ok;
    ok = move_to(channel_tilt, pulse_neutral_us) && ok;
    return ok;
}


/*************** GO HOME (Maestro home positions) ***************/

bool MaestroDriver::go_home() {

    if (!ready) {

        return false;
    }

    if (demo_mode || !g_live_ready || g_fd < 0) {

        return go_neutral();
    }

    if (!write_go_home()) {

        enter_demo(*this, "go_home write failed");
        return false;
    }

    for (int i = 0; i < servo_count; ++i) {

        target_us[i] = pulse_neutral_us;
        enabled[i] = true;
    }

    ++command_count;
    return true;
}


/*************** NUDGE BY DELTA µs ***************/

bool MaestroDriver::nudge(uint8_t channel, int delta_us) {

    const int idx = channel_index(channel);

    if (idx < 0 || !ready) {

        return false;
    }

    const int base = enabled[idx] ? target_us[idx] : pulse_neutral_us;
    return move_to(channel, base + delta_us);
}


/*************** CENTER FROM NORMALIZED IMAGE ERROR ***************/

bool MaestroDriver::center_from_error(

    float pan_error,
    float tilt_error,
    float gain_us
) {

    if (!ready) {

        return false;
    }

    pan_error = std::max(-1.0f, std::min(1.0f, pan_error));
    tilt_error = std::max(-1.0f, std::min(1.0f, tilt_error));

    if (gain_us < 0.0f) {

        gain_us = 0.0f;
    }

    // Deadband so tiny detection jitter doesn't chatter the mount.
    constexpr float k_deadband = 0.05f;
    const int pan_delta =
        (std::fabs(pan_error) < k_deadband)
            ? 0
            : static_cast<int>(std::lround(static_cast<double>(pan_error * gain_us)));
    const int tilt_delta =
        (std::fabs(tilt_error) < k_deadband)
            ? 0
            : static_cast<int>(std::lround(static_cast<double>(tilt_error * gain_us)));

    bool ok = true;

    if (pan_delta != 0) {

        ok = nudge(channel_pan, pan_delta) && ok;
    }

    if (tilt_delta != 0) {

        ok = nudge(channel_tilt, tilt_delta) && ok;
    }

    return ok;
}


/*************** SHUT DOWN MAESTRO DRIVER ***************/

void MaestroDriver::shutdown() {

    if (ready && !demo_mode && g_fd >= 0) {

        write_target_quarter_us(channel_pan, 0);
        write_target_quarter_us(channel_tilt, 0);
    }

    ready = false;
    demo_mode = true;
    frame_index = 0;
    command_count = 0;
    g_live_ready = false;
    port_path[0] = '\0';
    last_error[0] = '\0';
    reset_servo_state(*this);

    if (g_fd >= 0) {

        ::close(g_fd);
        g_fd = -1;
    }
}
