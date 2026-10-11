cmake_minimum_required(VERSION 3.16)
project(codered_orion_edge LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(EDGE_SOURCES
    src/main.cpp
    src/config/Config.cpp
    src/hardware/ADS1115Driver.cpp
    src/hardware/MLX90640Driver.cpp
    src/hardware/mlx90640/MLX90640_API.cpp
    src/hardware/mlx90640/MLX90640_LINUX_I2C_Driver.cpp
    src/net/TimeseriesClient.cpp
    src/vision/ThermalRoiSampler.cpp
    src/vitals/PressurePlate.cpp
    src/vitals/VitalsHub.cpp
)

add_executable(edge ${EDGE_SOURCES})

target_include_directories(edge PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/hardware
    ${CMAKE_SOURCE_DIR}/src/hardware/mlx90640
)

find_package(Threads REQUIRED)
find_package(PostgreSQL REQUIRED)
target_link_libraries(edge PRIVATE Threads::Threads m PostgreSQL::PostgreSQL)
