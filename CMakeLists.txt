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
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
        if(EXISTS "/etc/nv_tegra_release")
            set(PLATFORM "JETSON")
            file(READ "/etc/nv_tegra_release" JETSON_MODEL)
            message(STATUS "Detected Jetson: ${JETSON_MODEL}")
        endif()
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "armv")
        if(EXISTS "/sys/firmware/devicetree/base/model")
            file(READ "/sys/firmware/devicetree/base/model" RPI_MODEL)
            if(RPI_MODEL MATCHES "Raspberry Pi")
                set(PLATFORM "RPI")
                message(STATUS "Detected RPi: ${RPI_MODEL}")
            endif()
        endif()
    endif()
endif()

# Platform definitions
add_definitions(-DPLATFORM_${PLATFORM})
message(STATUS "Building for platform: ${PLATFORM}")

# Set source files
set(SOURCE_FILES
    armor.c
    audio.c
    command_processing.c
    config_parser.c
    config_manager.c
    curl_download.c
    detect.cpp
    devices.c
    frame_rate_tracker.c
    image_utils.c
    logging.c
    mirage.c
    mosquitto_comms.c
    utils.c)

# Set C compiler and flags
set(CMAKE_C_COMPILER gcc)
set(CMAKE_CXX_COMPILER g++)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wall -Wno-error=unused-but-set-variable -Wno-error=unused-function -Wno-error=unused-variable")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_C_FLAGS}")

# Set include directories
include_directories(/usr/include/vorbis /usr/local/include/vorbis)

# Conditional Jetson includes
if(USE_JETSON_INFERENCE)
    include_directories(/usr/local/include/jetson-inference /usr/src/jetson_multimedia_api/include)
    add_definitions(-DUSE_JETSON_INFERENCE)
endif()

# Find required packages
find_package(PkgConfig REQUIRED)

# Audio dependencies
pkg_check_modules(VORBISFILE REQUIRED vorbisfile)
include_directories(${VORBISFILE_INCLUDE_DIRS})
pkg_check_modules(ALSA REQUIRED alsa)
include_directories(${ALSA_INCLUDE_DIRS})

# Core dependencies
pkg_check_modules(JSONC REQUIRED json-c)
include_directories(${JSONC_INCLUDE_DIRS})
pkg_check_modules(SDL2 REQUIRED sdl2)
include_directories(${SDL2_INCLUDE_DIRS})
find_package(CURL REQUIRED)
include_directories(${CURL_INCLUDE_DIRS})

# Gstreamer
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0 gstreamer-app-1.0)
include_directories(${GSTREAMER_INCLUDE_DIRS})
pkg_check_modules(GLIB REQUIRED glib-2.0)
include_directories(${GLIB_INCLUDE_DIRS})

# Communications
pkg_check_modules(MOSQUITTO REQUIRED libmosquitto)
include_directories(${MOSQUITTO_INCLUDE_DIRS})

# OpenGL Support
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)

# Optional CUDA support
if(USE_CUDA)
    pkg_check_modules(CUDA REQUIRED cuda-12.2 cudart-12.2)
    include_directories(${CUDA_INCLUDE_DIRS})
    link_directories(${CUDA_LIBRARY_DIRS})
endif()

# Set libraries
set(LIBRARIES
  # Audio
  ${VORBISFILE_LIBRARIES}
  ${ALSA_LIBRARIES}

  # Core
  ${JSONC_LIBRARIES}
  ${SDL2_LIBRARIES}
  ${CURL_LIBRARIES}
  SDL2_image
  SDL2_ttf

  # Graphics and Video
  ${GSTREAMER_LIBRARIES}
  ${GLIB_LIBRARIES}
  ${OPENGL_LIBRARIES}
  ${GLEW_LIBRARIES}
  X11
  gd

  # Communications
  ${MOSQUITTO_LIBRARIES}

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

# Add a custom target for indent
add_custom_target(
  indent 
  COMMAND indent -linux -nut -i3 -l100 ${SOURCE_FILES}
)

install(TARGETS ${PROJECT_NAME} DESTINATION bin)
