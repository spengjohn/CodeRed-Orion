cmake_minimum_required(VERSION 3.16)
project(codered_orion_edge LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Optional: point at a local config directory at runtime
add_compile_definitions(EDGE_CONFIG_DIR="${CMAKE_SOURCE_DIR}/config")
add_compile_definitions(EDGE_MODELS_DIR="${CMAKE_SOURCE_DIR}/models")

set(EDGE_SOURCES
    src/main.cpp
    src/hardware/sensors.cpp
    src/vision/inference.cpp
)

add_executable(edge ${EDGE_SOURCES})

target_include_directories(edge PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/hardware
    ${CMAKE_SOURCE_DIR}/src/vision
)

# Uncomment / extend as hardware libs are wired in:
# find_package(PkgConfig REQUIRED)
# pkg_check_modules(LIBIIO REQUIRED libiio)
# target_link_libraries(edge PRIVATE ${LIBIIO_LIBRARIES})
