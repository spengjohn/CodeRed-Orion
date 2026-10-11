#include "MaestroDriver.hpp"

bool MaestroDriver::init() {
    ready = true;
    demo_mode = true;
    return true;
}

bool MaestroDriver::refresh() {
    if (!ready) {
        return false;
    }
    ++frame_index;
    return true;
}

void MaestroDriver::shutdown() {
    ready = false;
}

bool MaestroDriver::move_to(uint8_t channel, int pulse_us) {
    (void)channel;
    (void)pulse_us;
    return true;
}

bool MaestroDriver::move_to(
    uint8_t channel,
    int pulse_us,
    int speed_units,
    int accel_units
) {
    (void)channel;
    (void)pulse_us;
    (void)speed_units;
    (void)accel_units;
    return true;
}

bool MaestroDriver::set_speed(uint8_t channel, int speed_units) {
    (void)channel;
    (void)speed_units;
    return true;
}

bool MaestroDriver::set_acceleration(uint8_t channel, int accel_units) {
    (void)channel;
    (void)accel_units;
    return true;
}

bool MaestroDriver::set_target_us(uint8_t channel, int pulse_us) {
    (void)channel;
    (void)pulse_us;
    return true;
}

bool MaestroDriver::disable_channel(uint8_t channel) {
    (void)channel;
    return true;
}

bool MaestroDriver::go_neutral() {
    return true;
}

bool MaestroDriver::go_home() {
    return true;
}

bool MaestroDriver::nudge(uint8_t channel, int delta_us) {
    (void)channel;
    (void)delta_us;
    return true;
}

bool MaestroDriver::center_from_error(
    float pan_error,
    float tilt_error,
    float gain_us
) {
    (void)pan_error;
    (void)tilt_error;
    (void)gain_us;
    return true;
}
