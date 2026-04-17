cmake_minimum_required(VERSION 3.15)
project(mirage)

# Build options
option(USE_JETSON_INFERENCE "Enable Jetson Inference support" OFF)
option(USE_CUDA "Enable CUDA support" OFF)
set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose build type: Debug or Release")

# Platform selection
set(PLATFORM "AUTO" CACHE STRING "Choose platform: AUTO, JETSON, RPI")
set_property(CACHE PLATFORM PROPERTY STRINGS AUTO JETSON RPI)

# If the policy exists in your CMake version, set it to NEW
if(POLICY CMP0072)
    cmake_policy(SET CMP0072 NEW)
endif()

# Version and Git SHA
set(VERSION_NUMBER "1.0.0")
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_SHA
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE GIT_SHA_RESULT
)
if(NOT GIT_SHA_RESULT EQUAL 0)
    set(GIT_SHA "unknown")
endif()
add_definitions(-DGIT_SHA=\"${GIT_SHA}\")

# Platform detection
if(PLATFORM STREQUAL "AUTO")
    # Check if it's a Jetson board first
    if(EXISTS "/etc/nv_tegra_release")
        set(PLATFORM "JETSON")
        file(READ "/etc/nv_tegra_release" JETSON_MODEL)
        message(STATUS "Detected Jetson: ${JETSON_MODEL}")
    # Then check if it's a Raspberry Pi (both ARM and ARM64)
    elseif(EXISTS "/sys/firmware/devicetree/base/model")
        file(READ "/sys/firmware/devicetree/base/model" RPI_MODEL)
        if(RPI_MODEL MATCHES "Raspberry Pi")
            set(PLATFORM "RPI")
            message(STATUS "Detected RPi: ${RPI_MODEL}")
        endif()
    # Default platform detection based on processor architecture
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
        message(STATUS "Detected generic ARM64 platform")
        # Default platform for ARM64 if needed
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "armv")
        message(STATUS "Detected generic ARM platform")
        # Default platform for ARM if needed
    endif()
endif()

# Platform definitions
add_definitions(-DPLATFORM_${PLATFORM})
message(STATUS "Building for platform: ${PLATFORM}")

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Set source files
set(SOURCE_FILES
    # core
    src/core/mirage.c

    # config
    src/config/config_parser.c
    src/config/config_manager.c
    src/config/config_secrets.c

    # util
    src/util/curl_download.c
    src/util/data_sources.c
    src/util/image_utils.c
    src/util/logging.c
    src/util/sim_data.c
    src/util/utils.c

    # hardware
    src/hardware/armor.c
    src/hardware/detect.cpp
    src/hardware/devices.c
    src/hardware/frame_rate_tracker.c
    src/hardware/system_metrics.c

    # rendering
    src/rendering/element_renderer.c
    src/rendering/gauge_renderer.c

    # media
    src/media/recording.c
    src/media/screenshot.c

    # ui
    src/ui/hud_manager.c
    src/ui/notification.c

    # comm
    src/comm/command_processing.c
    src/comm/component_status.c
    src/comm/hud_discovery.c
    src/comm/mosquitto_comms.c
)

# Set C compiler and flags
set(CMAKE_C_COMPILER gcc)
set(CMAKE_CXX_COMPILER g++)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_C_FLAGS}")

# Set include directories

# Conditional Jetson includes
if(USE_JETSON_INFERENCE)
    include_directories(/usr/local/include/jetson-inference /usr/src/jetson_multimedia_api/include)
    add_definitions(-DUSE_JETSON_INFERENCE)
endif()

# Find required packages
find_package(PkgConfig REQUIRED)

# Core dependencies
pkg_check_modules(JSONC REQUIRED json-c)
include_directories(${JSONC_INCLUDE_DIRS})
pkg_check_modules(SDL2 REQUIRED sdl2)
include_directories(${SDL2_INCLUDE_DIRS})
pkg_check_modules(SDL2_GFX REQUIRED SDL2_gfx)
include_directories(${SDL2_GFX_INCLUDE_DIRS})
pkg_check_modules(SDL2_TTF REQUIRED SDL2_ttf)
include_directories(${SDL2_TTF_INCLUDE_DIRS})
pkg_check_modules(SDL2_IMAGE REQUIRED SDL2_image)
include_directories(${SDL2_IMAGE_INCLUDE_DIRS})
find_package(CURL REQUIRED)
include_directories(${CURL_INCLUDE_DIRS})

# Gstreamer
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0 gstreamer-app-1.0)
include_directories(${GSTREAMER_INCLUDE_DIRS})
pkg_check_modules(GLIB REQUIRED glib-2.0)
include_directories(${GLIB_INCLUDE_DIRS})

# Audio (PulseAudio for recording device validation)
pkg_check_modules(PULSE REQUIRED libpulse)
include_directories(${PULSE_INCLUDE_DIRS})

# Communications
pkg_check_modules(MOSQUITTO REQUIRED libmosquitto)
include_directories(${MOSQUITTO_INCLUDE_DIRS})

# Crypto (for OCP checksum validation)
find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

# OpenGL Support
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)

# Misc.
pkg_check_modules(GD REQUIRED gdlib)
include_directories(${GD_INCLUDE_DIRS})

# Optional CUDA support
if(USE_CUDA)
    pkg_check_modules(CUDA REQUIRED cuda-12.2 cudart-12.2)
    include_directories(${CUDA_INCLUDE_DIRS})
    link_directories(${CUDA_LIBRARY_DIRS})
endif()

# Set libraries
set(LIBRARIES
  # Audio
  ${PULSE_LIBRARIES}

  # Core
  ${JSONC_LIBRARIES}
  ${SDL2_LIBRARIES}
  ${CURL_LIBRARIES}
  ${SDL2_GFX_LIBRARIES}
  ${SDL2_TTF_LIBRARIES}
  ${SDL2_IMAGE_LIBRARIES}

  # Graphics and Video
  ${GSTREAMER_LIBRARIES}
  ${GLIB_LIBRARIES}
  ${OPENGL_LIBRARIES}
  ${GLEW_LIBRARIES}
  ${GD_LIBRARIES}
  X11
  gd

  # Communications
  ${MOSQUITTO_LIBRARIES}

  # Crypto
  OpenSSL::Crypto

  # System
  pthread
  rt
  m
  stdc++
)

# Optional libraries
if(USE_CUDA)
    list(APPEND LIBRARIES ${CUDA_LIBRARIES})
endif()

if(USE_JETSON_INFERENCE)
    list(APPEND LIBRARIES jetson-inference jetson-utils)
    link_directories(/usr/lib/aarch64-linux-gnu/tegra)
endif()

# Targets
add_executable(${PROJECT_NAME} ${SOURCE_FILES})
target_link_libraries(${PROJECT_NAME} ${LIBRARIES})


install(TARGETS ${PROJECT_NAME} DESTINATION bin)
