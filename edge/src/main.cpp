#include "hardware/sensors.hpp"
#include "vision/inference.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>

int main() {
    edge::hardware::SensorHub sensors;
    edge::vision::InferenceEngine vision;

    if (!sensors.init()) {
        std::cerr << "Failed to initialize sensors\n";
        return 1;
    }

    if (!vision.init()) {
        std::cerr << "Failed to initialize vision\n";
        return 1;
    }

    std::cout << "CodeRED Orion edge node running\n";

    while (true) {
        const auto sample = sensors.read();
        const auto moving = vision.is_moving();

        // Fields destined for TigerData (TimescaleDB) time series
        // ambient_temp, baby_temp, breaths_per_min, weight, is_moving, time
        std::cout
            << "t=" << sample.unix_time
            << " ambient=" << sample.ambient_temp
            << " baby=" << sample.baby_temp
            << " bpm=" << sample.breaths_per_min
            << " weight=" << sample.weight
            << " moving=" << (moving ? 1 : 0)
            << '\n';

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
