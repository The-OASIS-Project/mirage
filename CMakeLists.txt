cmake_minimum_required(VERSION 3.10)
project(horizon)

# Set source files
set(SOURCE_FILES main.c utils.c detect.cpp frame_rate_tracker.c config_parser.c config_manager.c audio.c armor.c devices.c command_processing.c mosquitto_comms.c curl_download.c image_utils.c)

# Set C compiler and flags
set(CMAKE_C_COMPILER gcc)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wall -Werror -Wno-error=unused-but-set-variable -Wno-error=unused-function -Wno-error=unused-variable")

# Set include directories
include_directories(/usr/include/vorbis /usr/local/include/vorbis /usr/local/include/jetson-inference /usr/src/jetson_multimedia_api/include)

# Find required packages
find_package(PkgConfig REQUIRED)

pkg_check_modules(VORBISFILE REQUIRED vorbisfile)
include_directories(${VORBISFILE_INCLUDE_DIRS})

pkg_check_modules(ALSA REQUIRED alsa)
include_directories(${ALSA_INCLUDE_DIRS})

pkg_check_modules(JSONC REQUIRED json-c)
include_directories(${JSONC_INCLUDE_DIRS})

pkg_check_modules(SDL2 REQUIRED sdl2)
include_directories(${SDL2_INCLUDE_DIRS})

find_package(CURL REQUIRED)
include_directories(${CURL_INCLUDE_DIRS})

pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0 gstreamer-app-1.0)
include_directories(${GSTREAMER_INCLUDE_DIRS})

pkg_check_modules(GLIB REQUIRED glib-2.0)
include_directories(${GLIB_INCLUDE_DIRS})

pkg_check_modules(MOSQUITTO REQUIRED libmosquitto)
include_directories(${MOSQUITTO_INCLUDE_DIRS})

#pkg_check_modules(CUDA REQUIRED cuda-11.4 cudart-11.4)
#include_directories(${CUDA_INCLUDE_DIRS})
find_package(PkgConfig REQUIRED)
pkg_check_modules(CUDA REQUIRED cuda-11.4 cudart-11.4)
include_directories(${CUDA_INCLUDE_DIRS})
link_directories(${CUDA_LIBRARY_DIRS})

# Set libraries
set(LIBRARIES 
  ${VORBISFILE_LIBRARIES}
  ${ALSA_LIBRARIES}
  ${JSONC_LIBRARIES}
  ${SDL2_LIBRARIES}
  ${CURL_LIBRARIES}
  ${GSTREAMER_LIBRARIES}
  ${GLIB_LIBRARIES}
  ${MOSQUITTO_LIBRARIES}
  gd
  ${CUDA_LIBRARIES}
  SDL2_image
  SDL2_ttf
  jetson-inference
  jetson-utils
  X11
  pthread
  rt
  m
  stdc++
)

link_directories(/usr/lib/aarch64-linux-gnu/tegra)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCE_FILES})
target_link_libraries(${PROJECT_NAME} ${LIBRARIES})

# Add a custom target for indent
add_custom_target(
  indent 
  COMMAND indent -linux -nut -i3 -l100 ${SOURCE_FILES}
)

