#include "hardware/MLX90640Driver.hpp"

#include <cstdlib>
#include <iostream>

int main() {
    MLX90640Driver thermal;

    if (!thermal.init()) {
        return EXIT_FAILURE;
    }

    while (thermal.ready) {
        if (!thermal.refresh()) {
            break;
        }

        std::cout
            << "frame=" << thermal.frame_index
            << " demo=" << (thermal.demo_mode ? 1 : 0)
            << " t_min=" << thermal.t_min
            << " t_max=" << thermal.t_max
            << '\n';
    }

    thermal.shutdown();
    return EXIT_SUCCESS;
}
