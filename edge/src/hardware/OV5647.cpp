#include "OV5647.hpp"

bool OV5647::init() {
    ready = true;
    demo_mode = true;
    return true;
}

bool OV5647::refresh() {
    if (!ready) {
        return false;
    }
    ++frame_index;
    return true;
}

void OV5647::shutdown() {
    ready = false;
}

bool OV5647::pack_rgb888(uint8_t* rgb_out) const {
    if (!rgb_out) {
        return false;
    }
    for (int i = 0; i < pixel_count; ++i) {
        rgb_out[i * 3 + 0] = r[i];
        rgb_out[i * 3 + 1] = g[i];
        rgb_out[i * 3 + 2] = b[i];
    }
    return true;
}
