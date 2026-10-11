cmake_minimum_required(VERSION 3.16)
project(codered_orion_edge LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(EDGE_SOURCES
    src/main.cpp
    src/config/Config.cpp
    src/hardware/ADS1115Driver.cpp
    src/hardware/CoralDriver.cpp
    src/hardware/MaestroDriver.cpp
    src/hardware/MLX90640Driver.cpp
    src/hardware/OV5647.cpp
    src/hardware/TfLunaDriver.cpp
    src/hardware/mlx90640/MLX90640_API.cpp
    src/hardware/mlx90640/MLX90640_LINUX_I2C_Driver.cpp
    src/net/TimeseriesClient.cpp
    src/net/UplinkClient.cpp
    src/pipeline/BabyTrackPipeline.cpp
    src/pipeline/IrStreamPipeline.cpp
    src/util/Base64.cpp
    src/vision/BreathMonitor.cpp
    src/vision/CameraFusion.cpp
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

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(LIBCAMERA QUIET libcamera)
endif()

if(LIBCAMERA_FOUND)
    message(STATUS "libcamera found: enabling HAS_LIBCAMERA")
    target_compile_definitions(edge PRIVATE HAS_LIBCAMERA)
    target_include_directories(edge PRIVATE ${LIBCAMERA_INCLUDE_DIRS})
    target_link_directories(edge PRIVATE ${LIBCAMERA_LIBRARY_DIRS})
    target_link_libraries(edge PRIVATE ${LIBCAMERA_LIBRARIES})
    target_compile_options(edge PRIVATE ${LIBCAMERA_CFLAGS_OTHER})
else()
    message(STATUS "libcamera not found: OV5647 will use demo mode")
endif()
