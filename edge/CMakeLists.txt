cmake_minimum_required(VERSION 3.16)
project(codered_orion_edge LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ENABLE_CORAL "Build Coral Edge TPU live path (needs TensorFlow source)" OFF)

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

if(ENABLE_CORAL)
    if(NOT TENSORFLOW_SOURCE_DIR)
        if(DEFINED ENV{TENSORFLOW_SOURCE_DIR})
            set(TENSORFLOW_SOURCE_DIR "$ENV{TENSORFLOW_SOURCE_DIR}")
        elseif(EXISTS "${CMAKE_SOURCE_DIR}/third_party/tensorflow/tensorflow/lite")
            set(TENSORFLOW_SOURCE_DIR "${CMAKE_SOURCE_DIR}/third_party/tensorflow")
        else()
            message(FATAL_ERROR "ENABLE_CORAL=ON needs TENSORFLOW_SOURCE_DIR. Run scripts/setup_coral_cpp.sh first.")
        endif()
    endif()

    set(EDGETPU_INCLUDE "${CMAKE_SOURCE_DIR}/third_party/coral/include")
    if(DEFINED ENV{EDGETPU_INCLUDE})
        set(EDGETPU_INCLUDE "$ENV{EDGETPU_INCLUDE}")
    endif()

    if(NOT EXISTS "${EDGETPU_INCLUDE}/edgetpu.h")
        message(FATAL_ERROR "edgetpu.h not found at ${EDGETPU_INCLUDE}. Run scripts/setup_coral_cpp.sh")
    endif()

    set(TFLITE_BINARY_DIR "${CMAKE_SOURCE_DIR}/third_party/tflite_build")
    if(DEFINED ENV{TFLITE_BINARY_DIR})
        set(TFLITE_BINARY_DIR "$ENV{TFLITE_BINARY_DIR}")
    endif()
    add_compile_options(-Wno-deprecated-declarations)

    add_subdirectory(
        "${TENSORFLOW_SOURCE_DIR}/tensorflow/lite"
        "${TFLITE_BINARY_DIR}"
        EXCLUDE_FROM_ALL
    )

    if(DEFINED ENV{EDGETPU_LIB} AND EXISTS "$ENV{EDGETPU_LIB}")
        set(EDGETPU_LIB "$ENV{EDGETPU_LIB}")
    else()
        find_library(EDGETPU_LIB
            NAMES edgetpu edgetpu.so.1 libedgetpu.so.1
            PATHS /usr/lib /usr/lib/aarch64-linux-gnu /usr/lib/arm-linux-gnueabihf /usr/local/lib
            REQUIRED
        )
    endif()
    find_library(LIBUSB_LIB NAMES usb-1.0)
    message(STATUS "Linking Edge TPU library: ${EDGETPU_LIB}")

    target_compile_definitions(edge PRIVATE HAS_EDGETPU)
    target_include_directories(edge PRIVATE
        "${TENSORFLOW_SOURCE_DIR}"
        "${EDGETPU_INCLUDE}"
    )
    target_link_libraries(edge PRIVATE
        tensorflow-lite
        "${EDGETPU_LIB}"
    )
    if(LIBUSB_LIB)
        target_link_libraries(edge PRIVATE "${LIBUSB_LIB}")
    endif()
endif()
