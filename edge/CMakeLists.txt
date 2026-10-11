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
    # Prefer linking a finished TFLite tree (e.g. from C_Hardware_Testing).
    # add_subdirectory() re-downloads deps into build-coral and can take hours.
    option(CORAL_USE_PREBUILT_TFLITE
        "Link existing libtensorflow-lite.a instead of add_subdirectory" ON)

    if(NOT TENSORFLOW_SOURCE_DIR)
        if(DEFINED ENV{TENSORFLOW_SOURCE_DIR})
            set(TENSORFLOW_SOURCE_DIR "$ENV{TENSORFLOW_SOURCE_DIR}")
        elseif(EXISTS "${CMAKE_SOURCE_DIR}/third_party/tensorflow/tensorflow/lite")
            set(TENSORFLOW_SOURCE_DIR "${CMAKE_SOURCE_DIR}/third_party/tensorflow")
        else()
            message(FATAL_ERROR "ENABLE_CORAL=ON needs TENSORFLOW_SOURCE_DIR.")
        endif()
    endif()

    set(EDGETPU_INCLUDE "${CMAKE_SOURCE_DIR}/third_party/coral/include")
    if(DEFINED ENV{EDGETPU_INCLUDE})
        set(EDGETPU_INCLUDE "$ENV{EDGETPU_INCLUDE}")
    endif()

    if(NOT EXISTS "${EDGETPU_INCLUDE}/edgetpu.h")
        message(FATAL_ERROR "edgetpu.h not found at ${EDGETPU_INCLUDE}")
    endif()

    set(TFLITE_BINARY_DIR "${CMAKE_SOURCE_DIR}/third_party/tflite_build")
    if(DEFINED ENV{TFLITE_BINARY_DIR})
        set(TFLITE_BINARY_DIR "$ENV{TFLITE_BINARY_DIR}")
    endif()

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
    target_compile_options(edge PRIVATE -Wno-deprecated-declarations)

    set(_tflite_a "${TFLITE_BINARY_DIR}/libtensorflow-lite.a")
    if(CORAL_USE_PREBUILT_TFLITE AND EXISTS "${_tflite_a}")
        message(STATUS "Using PREBUILT TFLite (no FetchContent): ${_tflite_a}")

        # Pull every static lib from the finished tree; --start-group resolves cycles.
        file(GLOB_RECURSE _tflite_static_libs "${TFLITE_BINARY_DIR}/*.a")
        # Also pick up deps that Hardware Testing may have left under its build-coral.
        if(DEFINED ENV{TFLITE_EXTRA_LIB_DIR} AND EXISTS "$ENV{TFLITE_EXTRA_LIB_DIR}")
            file(GLOB_RECURSE _extra_static "$ENV{TFLITE_EXTRA_LIB_DIR}/*.a")
            list(APPEND _tflite_static_libs ${_extra_static})
        endif()
        list(REMOVE_DUPLICATES _tflite_static_libs)

        # Flatbuffers / other generated headers often live next to the build.
        if(EXISTS "${TFLITE_BINARY_DIR}/flatbuffers/include")
            target_include_directories(edge PRIVATE "${TFLITE_BINARY_DIR}/flatbuffers/include")
        endif()
        file(GLOB _fb_inc_dirs "${TFLITE_BINARY_DIR}/**/flatbuffers/include")
        foreach(_d ${_fb_inc_dirs})
            if(IS_DIRECTORY "${_d}")
                target_include_directories(edge PRIVATE "${_d}")
            endif()
        endforeach()

        target_link_libraries(edge PRIVATE
            -Wl,--start-group
            ${_tflite_static_libs}
            -Wl,--end-group
            "${EDGETPU_LIB}"
            dl
        )
        if(LIBUSB_LIB)
            target_link_libraries(edge PRIVATE "${LIBUSB_LIB}")
        endif()
    else()
        message(WARNING
            "Prebuilt libtensorflow-lite.a not found at ${_tflite_a}; "
            "falling back to add_subdirectory (slow). Set TFLITE_BINARY_DIR.")
        add_compile_options(-Wno-deprecated-declarations)
        add_subdirectory(
            "${TENSORFLOW_SOURCE_DIR}/tensorflow/lite"
            "${TFLITE_BINARY_DIR}"
            EXCLUDE_FROM_ALL
        )
        target_link_libraries(edge PRIVATE tensorflow-lite "${EDGETPU_LIB}")
        if(LIBUSB_LIB)
            target_link_libraries(edge PRIVATE "${LIBUSB_LIB}")
        endif()
    endif()
endif()
