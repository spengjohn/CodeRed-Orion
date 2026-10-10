#include "TfLunaDriver.hpp"

bool TfLunaDriver::init() {
    ready = true;
    demo_mode = true;
    return true;
}

bool TfLunaDriver::refresh() {
    if (!ready) {
        return false;
    }
    ++frame_index;
    return true;
}

void TfLunaDriver::shutdown() {
    ready = false;
}
