/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * By contributing to this project, you agree to license your contributions
 * under the GPLv3 (or any later version) or any future licenses chosen by
 * the project author(s). Contributions include any modifications,
 * enhancements, or additions to the project. These contributions become
 * part of the project and are adopted by the project author(s).
 */

#ifndef DEFINES_H
#define DEFINES_H

#define MKV_OUT
//#define SOFTWARE_ENCODE
//#define ENCODE_TIMING
//#define DISPLAY_TIMING
//#define OD_PROPER_WAIT
//#define FPS_STATS
//#define REFRESH_SYNC
//#define ORIGINAL_RATIO

/* This is per eye/display. */
/* === CAMERA RESOLUTION SELECTION === */
/* Uncomment ONE of the following resolution options */

//#define USE_720P_30FPS
#define USE_720P_60FPS
//#define USE_1080P_30FPS
//#define USE_1080P_60FPS
//#define USE_1440P_30FPS
//#define USE_1440P_60FPS
//#define USE_CUSTOM_RESOLUTION

/* === AUTOMATIC CONFIGURATION BASED ON SELECTION === */

#ifdef USE_720P_30FPS
   #define DEFAULT_CAM_INPUT_WIDTH  1280
   #define DEFAULT_CAM_INPUT_HEIGHT 720
   #define DEFAULT_CAM_INPUT_FPS    30
   #ifndef ORIGINAL_RATIO
      #define DEFAULT_CAM_CROP_WIDTH   720
      #define DEFAULT_CAM_CROP_X       280
   #else
      #define DEFAULT_CAM_CROP_X       0
   #endif

#elif defined(USE_720P_60FPS)
   #define DEFAULT_CAM_INPUT_WIDTH  1280
   #define DEFAULT_CAM_INPUT_HEIGHT 720
   #define DEFAULT_CAM_INPUT_FPS    60
   #ifndef ORIGINAL_RATIO
      #define DEFAULT_CAM_CROP_WIDTH   720
      #define DEFAULT_CAM_CROP_X       280
   #else
      #define DEFAULT_CAM_CROP_X       0
   #endif

#elif defined(USE_1080P_30FPS)
   #define DEFAULT_CAM_INPUT_WIDTH  1920
   #define DEFAULT_CAM_INPUT_HEIGHT 1080
   #define DEFAULT_CAM_INPUT_FPS    30
   #ifndef ORIGINAL_RATIO
      #define DEFAULT_CAM_CROP_WIDTH   1080
      #define DEFAULT_CAM_CROP_X       420
   #else
      #define DEFAULT_CAM_CROP_X       0
   #endif

#elif defined(USE_1080P_60FPS)
   #define DEFAULT_CAM_INPUT_WIDTH  1920
   #define DEFAULT_CAM_INPUT_HEIGHT 1080
   #define DEFAULT_CAM_INPUT_FPS    60
   #ifndef ORIGINAL_RATIO
      #define DEFAULT_CAM_CROP_WIDTH   1080
      #define DEFAULT_CAM_CROP_X       420
   #else
      #define DEFAULT_CAM_CROP_X       0
   #endif

#elif defined(USE_1440P_30FPS)
   #define DEFAULT_CAM_INPUT_WIDTH  2560
   #define DEFAULT_CAM_INPUT_HEIGHT 1440
   #define DEFAULT_CAM_INPUT_FPS    30
   #ifndef ORIGINAL_RATIO
      #define DEFAULT_CAM_CROP_WIDTH   1440
      #define DEFAULT_CAM_CROP_X       560
   #else
      #define DEFAULT_CAM_CROP_X       0
   #endif

#elif defined(USE_1440P_60FPS)
   #define DEFAULT_CAM_INPUT_WIDTH  2560
   #define DEFAULT_CAM_INPUT_HEIGHT 1440
   #define DEFAULT_CAM_INPUT_FPS    60
   #ifndef ORIGINAL_RATIO
      #define DEFAULT_CAM_CROP_WIDTH   1440
      #define DEFAULT_CAM_CROP_X       560
   #else
      #define DEFAULT_CAM_CROP_X       0
   #endif

#elif defined(USE_CUSTOM_RESOLUTION)
   /* Define your custom resolution here */
   #define DEFAULT_CAM_INPUT_WIDTH  1920
   #define DEFAULT_CAM_INPUT_HEIGHT 1080
   #define DEFAULT_CAM_INPUT_FPS    30
   #ifndef ORIGINAL_RATIO
      #define DEFAULT_CAM_CROP_WIDTH   1080
      #define DEFAULT_CAM_CROP_X       420
   #else
      #define DEFAULT_CAM_CROP_X       0
   #endif

#else
   /* Default fallback if nothing is selected */
   #error "Please uncomment one camera resolution option in defines.h"
#endif

/* === SANITY CHECK === */
#if defined(USE_720P_30FPS) + defined(USE_720P_60FPS) + defined(USE_1080P_30FPS) + \
    defined(USE_1080P_60FPS) + defined(USE_1440P_30FPS) + defined(USE_1440P_60FPS) + \
    defined(USE_CUSTOM_RESOLUTION) > 1
   #error "Only ONE camera resolution option should be uncommented"
#endif

//#define DEBUG_BUFFERS
//#define DEBUG_SHUTDOWN                  // Sometimes the shutdown process hangs.
                                          // This helps to figure out why.

/* These values should represent the dimensions of the output display
 * width divided by 2. */
#define DEFAULT_EYE_OUTPUT_WIDTH    1440
#define DEFAULT_EYE_OUTPUT_HEIGHT   1440

#define DEFAULT_STREAM_DEST_IP      "192.168.10.195"
#define STREAM_WIDTH                1920
#define STREAM_HEIGHT                960
#define STREAM_BITRATE              4500000

#define DEFAULT_ARMOR_NOTICE_TIMEOUT      5
#define DEFAULT_ARMOR_DEREGISTER_TIMEOUT  30

#define TARGET_RECORDING_FPS                 30
#define TARGET_RECORDING_FRAME_DURATION_US   (1000000 / TARGET_RECORDING_FPS)

#define RECORD_PULSE_AUDIO_DEVICE   "alsa_output.usb-KTMicro_TX_96Khz_USB_Audio_2022-08-08-0000-0000-0000--00.analog-stereo.monitor"

// New York City, NY
//#define DEFAULT_LATITUDE    40.7831
//#define DEFAULT_LONGITUDE  -73.9712

// Atlanta, GA
#define DEFAULT_LATITUDE    33.7615
#define DEFAULT_LONGITUDE  -84.3836

// San Jose, CA
//#define DEFAULT_LATITUDE    37.3292
//#define DEFAULT_LONGITUDE  -121.8890

// Default port for TCP socket helmet communications
#define HELMET_PORT  3000

#define SUCCESS 0
#define FAILURE 1

#define MAX_HUDS 16  /* Maximum number of HUDs supported */
#define MAX_DETECT 4 /* Max number of auto-detected objects on the screen. */

#define MAX_FILENAME_LENGTH      1024  /* Generic max filename supported. */
#define MAX_SERIAL_BUFFER_LENGTH 4096  /* Size of the serial buffer. */
#define MAX_WIFI_DEV_LENGTH      10    /* Max length for a wifi device name. */

/* These setup local log buffering from USB input. */
#define LOG_ROWS              20
#define LOG_LINE_LENGTH       100
#define MAX_TEXT_LENGTH       (LOG_ROWS * LOG_LINE_LENGTH)

/* Default image and font paths. These are configurable in the config file. */
#define IMAGE_PATH_DEFAULT    "ui_assets/mk2/"
#define FONT_PATH_DEFAULT     "ui_assets/fonts/"
#define SOUND_PATH_DEFAULT    "sound_assets/"

#define DEFAULT_WIFI_DEV_NAME "wlP1p1s0"

#define FIXED_DEFAULT         0     /* Are elements fixed in place or do they move
                                     * when adjusted by default?
                                     * 0 - Not fixed.
                                     * 1 - Fixed.
                                     */

#define SNAPSHOT_NOOVERLAY          /* When we capture a snapshot (for ML) do we include the overlay? */
#define SNAPSHOT_WIDTH        512   /* Width for ML snapshot */
#define SNAPSHOT_HEIGHT       512   /* Height for the ML snapshot */
#define SNAPSHOT_QUALITY       90   /* Quality setting 0-100 for the JPG snapshot */


enum { ANGLE_ROLL = 1000, ANGLE_OPPOSITE_ROLL = 1001 };  /* For the roll indicator, do we roll with
                                                            the angle measured or opposite it. */

#define GOOGLE_MAPS_API       "https://maps.googleapis.com/maps/api/staticmap?center=%f,%f&size=%dx%d&format=png32&" \
                              "maptype=%s&zoom=%d&markers=size:mid%%7Ccolor:red%%7C%f,%f&key=%s"
#define GOOGLE_APIKEY_FILE    "googleapi.key"      /* Where do we store our Google API key? */
#define MAP_UPDATE_SEC        30                   /* Fixed map update interval. */
                                                   /* TODO: Make these available in the config file. */

#define NUM_AUDIO_THREADS     8                    /* Number of simul audio threads. */
//#define STARTUP_SOUND         "jarvis_service.ogg"

#define USB_PORT              "/dev/ttyACM0"       /* Default USB port. */

#define FAN_RPM_FILE          "/sys/class/hwmon/hwmon3/rpm"
#define FAN_MAX_RPM           6000

/* Defines for sound server. */
#define SERVER_QUEUE_NAME   "/stark-sound-server"
#define QUEUE_PERMISSIONS 0660
#define MAX_MESSAGES 10
#define MAX_MSG_SIZE sizeof(audio_msg)
#define MSG_BUFFER_SIZE MAX_MSG_SIZE + 10

/* ALSA */
#define PCM_DEVICE "default"

/* The colorspace of the output display. */
#define RGB_OUT_SIZE 4
#define PIXEL_FORMAT_OUT SDL_PIXELFORMAT_RGBA32

/* All of the Gstreamer pipelines. These should be defined per platform.
 *
 * Right now only NVIDIA is supported.
 *
 * FIXME: These are getting a bit out of hand, so I think I need to break these up into their components.
 *        After my latest work getting YouTube streaming working... it's worse. Sorry.
 */
#define GSTREAMER_PIPELINE_LENGTH   2048
#define DEFAULT_CSI_CAM1            0
#define DEFAULT_CSI_CAM2            1
#define DEFAULT_USB_CAM1            0
#define DEFAULT_USB_CAM2            2

/*
 * GStreamer Pipeline Components
 * Organized by function for easier maintenance
 */

/* === INPUT COMPONENTS === */
// Common output pipeline portion
#define GST_CAM_PIPELINE_OUTPUT \
    "video/x-raw, format=(string)RGBA ! " \
    "queue max-size-time=%lu leaky=2 ! " \
    "appsink processing-deadline=0 name=sink%s " \
    "caps=\"video/x-raw,format=RGBA,pixel-aspect-ratio=1/1\""

#ifdef PLATFORM_JETSON
// Input pipeline portions for CSI cameras
#define GST_CAM_PIPELINE_CSI_INPUT \
    "nvarguscamerasrc exposurecompensation=0 tnr-mode=0 sensor_id=%d ! " \
    "video/x-raw(memory:NVMM), width=%d, height=%d, format=(string)NV12, framerate=(fraction)%d/1 ! " \
    "nvvidconv flip-method=0 ! "

// Input pipeline portions for USB cameras
#define GST_CAM_PIPELINE_USB_INPUT \
    "v4l2src device=/dev/video%d ! " \
    "image/jpeg, width=%d, height=%d, framerate=%d/1, format=MJPG ! " \
    "jpegdec ! nvvidconv ! "
#else
// Input pipeline for Raspberry Pi Camera Module (CSI)
#define GST_CAM_PIPELINE_CSI_INPUT \
    "libcamerasrc name=cam%d ! " \
    "video/x-raw, width=%d, height=%d, framerate=(fraction)%d/1 ! " \
    "videoconvert ! "

// Input pipeline for USB cameras
#define GST_CAM_PIPELINE_USB_INPUT \
    "v4l2src device=/dev/video%d ! " \
    "image/jpeg, width=%d, height=%d, framerate=%d/1, format=MJPG ! " \
    "jpegdec ! videoconvert ! "
#endif

#define GST_PIPE_INPUT      "appsrc name=srcEncode ! " \
                           "video/x-raw, width=(int)%d, height=(int)%d, format=(string)RGBA, framerate=(fraction)%d/1 ! queue max-size-buffers=30 ! clocksync ! "

/* === VIDEO PROCESSING COMPONENTS === */
#ifdef PLATFORM_JETSON
    #ifdef SOFTWARE_ENCODE
        /* Jetson software encoding paths */
        #define GST_PIPE_VIDEO_MAIN    "nvvidconv ! video/x-raw, format=I420 ! " \
                                      "x264enc bitrate=16000 speed-preset=1 ! "

        #define GST_PIPE_VIDEO_STREAM  "nvvidconv ! video/x-raw, format=I420, width=(int)%d, height=(int)%d ! " \
                                      "x264enc bitrate=8000 speed-preset=1 ! "

        #define GST_PIPE_VIDEO_HLS     "nvvidconv ! video/x-raw, format=I420 ! " \
                                      "x264enc bitrate=8000 tune=zerolatency ! "

        #define GST_PIPE_VIDEO_YOUTUBE "videoconvert ! video/x-raw, width=(int)%d, height=(int)%d, format=I420 ! " \
                                      "x264enc bitrate=%d tune=zerolatency speed-preset=veryfast key-int-max=60 ! "
    #else
        /* Jetson hardware encoding paths */
        #define GST_PIPE_VIDEO_MAIN    "nvvidconv ! video/x-raw(memory:NVMM), format=NV12 ! " \
                                      "nvv4l2h264enc bitrate=16000000 profile=4 preset-level=4 ! "

        #define GST_PIPE_VIDEO_STREAM  "nvvidconv ! video/x-raw(memory:NVMM), format=NV12, width=(int)%d, height=(int)%d ! " \
                                      "nvv4l2h264enc bitrate=8000000 profile=4 preset-level=4 ! "

        #define GST_PIPE_VIDEO_HLS     "nvvidconv ! video/x-raw(memory:NVMM), format=NV12 ! " \
                                      "nvv4l2h264enc bitrate=8000000 profile=2 preset-level=3 ! "

        #define GST_PIPE_VIDEO_YOUTUBE "nvvidconv ! " \
                                      "video/x-raw(memory:NVMM), width=(int)%d, height=(int)%d, format=NV12 ! " \
                                      "nvv4l2h264enc bitrate=%d " \
                                      "control-rate=1 " \
                                      "preset-level=4 " \
                                      "profile=4 " \
                                      "maxperf-enable=1 " \
                                      "EnableTwopassCBR=1 " \
                                      "disable-cabac=0 " \
                                      "insert-sps-pps=1 " \
                                      "insert-vui=1 " \
                                      "iframeinterval=60 " \
                                      "idrinterval=60 " \
                                      "vbv-size=8000000 ! "
#endif
#elif defined(PLATFORM_RPI)
    #ifdef SOFTWARE_ENCODE
        /* Software encoding not supported on Raspberry Pi */
        #error "Software encoding is not supported on Raspberry Pi platform. Please undefine SOFTWARE_ENCODE."
    #else
        /* Pi hardware encoding paths */
        #define GST_PIPE_VIDEO_MAIN    "videoconvert ! video/x-raw, format=I420 ! " \
                                      "avenc_h264_omx bitrate=16000000 profile=100 ! "
        #define GST_PIPE_VIDEO_STREAM  "videoconvert ! video/x-raw, format=I420, width=(int)%d, height=(int)%d ! " \
                                      "avenc_h264_omx bitrate=8000000 profile=100 ! "
        #define GST_PIPE_VIDEO_HLS     "videoconvert ! video/x-raw, format=I420 ! " \
                                      "avenc_h264_omx bitrate=8000000 profile=100 ! "
        #define GST_PIPE_VIDEO_YOUTUBE "videoconvert ! video/x-raw, width=(int)%d, height=(int)%d, format=I420 ! " \
                                      "avenc_h264_omx bitrate=%d profile=100 ! "
    #endif
#else
    /* Default to software encoding for other platforms */
    #define GST_PIPE_VIDEO_MAIN    "videoconvert ! video/x-raw, format=I420 ! " \
                                  "x264enc bitrate=16000 speed-preset=1 ! "

    #define GST_PIPE_VIDEO_STREAM  "videoconvert ! video/x-raw, format=I420, width=(int)%d, height=(int)%d ! " \
                                  "x264enc bitrate=8000 speed-preset=1 ! "

    #define GST_PIPE_VIDEO_HLS     "videoconvert ! video/x-raw, format=I420 ! " \
                                  "x264enc bitrate=8000 tune=zerolatency ! "

    #define GST_PIPE_VIDEO_YOUTUBE "videoconvert ! video/x-raw, width=(int)%d, height=(int)%d, format=I420 ! " \
                                  "x264enc bitrate=%d tune=zerolatency ! "
#endif

/* === PARSER COMPONENTS === */
#define GST_PIPE_PARSE          "h264parse ! "
#define GST_PIPE_PARSE_CONFIG   "h264parse config-interval=1 ! "

/* === AUDIO COMPONENTS === */
#define GST_PIPE_AUDIO         "pulsesrc device=%s do-timestamp=true provide-clock=false ! " \
                               "audio/x-raw, format=(string)S16LE, rate=(int)44100, channels=(int)2 ! " \
                               "audioconvert ! voaacenc bitrate=128000 ! queue ! mux. "

#define GST_PIPE_AUDIO_YOUTUBE "pulsesrc device=%s do-timestamp=true provide-clock=true ! " \
                               "audio/x-raw, format=(string)S16LE, rate=(int)44100, channels=(int)2 ! " \
                               "audioconvert ! voaacenc bitrate=128000 ! aacparse ! queue ! mux. "

/* === MUXER COMPONENTS === */
#ifdef MKV_OUT
    #define GST_PIPE_MUXER      "matroskamux name=mux"
#else
    #define GST_PIPE_MUXER      "qtmux name=mux"
#endif

/* === OUTPUT COMPONENTS === */
#define GST_PIPE_FILE_OUT       " ! filesink location=%s"
#define GST_PIPE_UDP_OUT        " ! rtph264pay ! udpsink host=%s port=5000 sync=false"
#define GST_PIPE_HLS_OUT        "mux. hlssink2 name=mux playlist-root=http://%s/hls/ " \
                               "location=/var/www/html/hls/segment%%d.ts " \
                               "playlist-location=/var/www/html/hls/playlist.m3u8"
#define GST_PIPE_RTMP_OUT       "flvmux name=mux streamable=true latency=100000000 ! " \
                               "queue name=mux_queue max-size-buffers=50 max-size-time=0 max-size-bytes=0 ! " \
                               "rtmpsink location='rtmp://a.rtmp.youtube.com/live2/%s live=1' sync=false async=false"

/* === UTILITY COMPONENTS === */
#define GST_PIPE_QUEUE          "queue ! mux."
#define GST_PIPE_QUEUE_VIDEO    "queue name=video_queue max-size-buffers=30 max-size-time=0 max-size-bytes=0 ! mux. "
#define GST_PIPE_INPUT_QUEUE_LEAKY "queue name=input_queue max-size-buffers=10 max-size-time=0 max-size-bytes=0 leaky=downstream ! "

/* === COMPLETE PIPELINE DEFINITIONS === */

/* Recording pipeline */
#define GST_ENC_PIPELINE        GST_PIPE_INPUT \
                               GST_PIPE_VIDEO_MAIN \
                               GST_PIPE_PARSE \
                               GST_PIPE_QUEUE " " \
                               GST_PIPE_AUDIO " " \
                               GST_PIPE_MUXER \
                               GST_PIPE_FILE_OUT

/* Recording + Streaming pipeline */
#define GST_PIPE_AUDIO_TEE      "pulsesrc device=%s do-timestamp=true ! " \
                               "audio/x-raw, format=(string)S16LE, rate=(int)44100, channels=(int)2 ! " \
                               "audioconvert ! voaacenc bitrate=128000 ! " \
                               "tee name=audio_tee ! " \
                               "queue ! filemux. " \
                               "audio_tee. ! " \
                               "queue ! aacparse ! streammux. "

#define GST_ENCSTR_PIPELINE     GST_PIPE_INPUT \
                               "tee name=raw_split ! " \
                               "queue name=record_queue max-size-buffers=10 max-size-time=0 max-size-bytes=0 ! " \
                               GST_PIPE_VIDEO_MAIN \
                               GST_PIPE_PARSE \
                               "queue ! filemux. " \
                               "raw_split. ! " \
                               "queue name=stream_queue max-size-buffers=10 max-size-time=0 max-size-bytes=0 leaky=downstream ! " \
                               GST_PIPE_VIDEO_YOUTUBE \
                               "h264parse config-interval=1 ! " \
                               "queue ! streammux. " \
                               GST_PIPE_AUDIO_TEE \
                               "matroskamux name=filemux ! " \
                               "filesink location=%s " \
                               "flvmux name=streammux streamable=true latency=100000000 ! " \
                               "queue name=rtmp_queue max-size-buffers=50 max-size-time=0 max-size-bytes=0 ! " \
                               "rtmpsink location='rtmp://a.rtmp.youtube.com/live2/%s live=1' sync=false async=false"

/* Streaming-only pipeline */
#define GST_STR_PIPELINE        GST_PIPE_INPUT \
                               GST_PIPE_INPUT_QUEUE_LEAKY \
                               GST_PIPE_VIDEO_YOUTUBE \
                               GST_PIPE_PARSE \
                               GST_PIPE_QUEUE_VIDEO \
                               GST_PIPE_AUDIO_YOUTUBE \
                               GST_PIPE_RTMP_OUT

#endif // DEFINES_H

