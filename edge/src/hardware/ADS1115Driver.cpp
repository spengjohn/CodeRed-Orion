#include "ADS1115Driver.hpp"

bool ADS1115Driver::init() {
    ready = true;
    demo_mode = true;
    total_kg = 0.0f;
    return true;
}

bool ADS1115Driver::refresh() {
    if (!ready) {
        return false;
    }
    ++frame_index;
    return true;
}

void ADS1115Driver::shutdown() {
    ready = false;
}
