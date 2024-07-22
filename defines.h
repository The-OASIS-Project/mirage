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
#define REFRESH_SYNC
//#define ORIGINAL_RATIO

/* This is per eye/display. */
#if 1
#define DEFAULT_CAM_INPUT_WIDTH  1920
#define DEFAULT_CAM_INPUT_HEIGHT 1080
#define DEFAULT_CAM_INPUT_FPS    60
#ifndef ORIGINAL_RATIO
#define DEFAULT_CAM_CROP_WIDTH   1080
#define DEFAULT_CAM_CROP_X       420
#else
#define DEFAULT_CAM_CROP_X       0
#endif
#else
#define DEFAULT_CAM_INPUT_WIDTH  2560
#define DEFAULT_CAM_INPUT_HEIGHT 1440
#define DEFAULT_CAM_INPUT_FPS    30
#ifndef ORIGINAL_RATIO
#define DEFAULT_CAM_CROP_WIDTH   1440
#define DEFAULT_CAM_CROP_X       560
#else
#define DEFAULT_CAM_CROP_X       0
#endif
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
#define STREAM_BITRATE              8000000

#define DEFAULT_ARMOR_NOTICE_TIMEOUT      5
#define DEFAULT_ARMOR_DEREGISTER_TIMEOUT  5

#define TARGET_RECORDING_FPS                 30
#define TARGET_RECORDING_FRAME_DURATION_US   (1000000 / TARGET_RECORDING_FPS)

#define RECORD_AUDIO
#define RECORD_PULSE_AUDIO_DEVICE   "alsa_output.usb-KTMicro_TX_96Khz_USB_Audio_2022-08-08-0000-0000-0000--00.analog-stereo.monitor"

// New York City, NY
//#define DEFAULT_LATITUDE    40.7831
//#define DEFAULT_LONGITUDE  -73.9712

// Atlanta, GA
#define DEFAULT_LATITUDE    33.7488
#define DEFAULT_LONGITUDE  -84.3877

// San Jose, CA
//#define DEFAULT_LATITUDE    37.3292
//#define DEFAULT_LONGITUDE  -121.8890

#define SUCCESS 0
#define FAILURE 1

#define MAX_FILENAME_LENGTH   1024  /* Generic max filename supported. */
#define MAX_WIFI_DEV_LENGTH   10    /* Max length for a wifi device name. */

/* These setup local log buffering from USB input. */
#define LOG_ROWS              20
#define LOG_LINE_LENGTH       100
#define MAX_TEXT_LENGTH       (LOG_ROWS * LOG_LINE_LENGTH)

/* Default image and font paths. These are configurable in the config file. */
#define IMAGE_PATH_DEFAULT    "ui_assets/mk2/"
#define FONT_PATH_DEFAULT     "ui_assets/fonts/"
#define SOUND_PATH_DEFAULT    "sound_assets/"

#define DEFAULT_WIFI_DEV_NAME "wlan0"

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
                              "maptype=hybrid&markers=size:mid%%7Ccolor:red%%7C%f,%f&map_id=1f0d991c235e0c32&zoom=18&key=%s"
#define GOOGLE_APIKEY_FILE    "googleapi.key"      /* Where do we store our Google API key? */
#define MAP_UPDATE_SEC        30                   /* Fixed map update interval. */
                                                   /* TODO: Make these available in the config file. */

#define NUM_AUDIO_THREADS     8                    /* Number of simul audio threads. */
//#define STARTUP_SOUND         "jarvis_service.ogg"

#define USB_PORT              "/dev/ttyACM0"       /* Default USB port. */

//#define FAN_RPM_FILE          "/sys/devices/generic_pwm_tachometer/hwmon/hwmon1/rpm"       /* Old? */
//#define FAN_RPM_FILE          "/sys/devices/platform/39c0000.tachometer/hwmon/hwmon2/rpm"  /* Xavier NX */
#define FAN_RPM_FILE          "/sys/class/hwmon/hwmon0/rpm"
#define FAN_MAX_RPM           6000

#define FAN_PWM_FILE          "/sys/devices/platform/pwm-fan/hwmon/hwmon3/pwm1"

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
#define GST_CAM_PIPELINE   "nvarguscamerasrc exposurecompensation=-1 tnr-mode=2 sensor_id=0 ! " \
                           "video/x-raw(memory:NVMM), width=%d, height=%d, format=(string)NV12, framerate=(fraction)%d/1 ! " \
                           "nvvidconv flip-method=0 ! " \
                           "video/x-raw, format=(string)RGBA ! queue max-size-time=%lu leaky=2 ! appsink processing-deadline=0 name=sinkL " \
                              "caps=\"video/x-raw,format=RGBA,pixel-aspect-ratio=1/1\" " \
                           "nvarguscamerasrc exposurecompensation=-1 tnr-mode=2 sensor_id=1 ! " \
                           "video/x-raw(memory:NVMM), width=%d, height=%d, format=(string)NV12, framerate=(fraction)%d/1 ! " \
                           "nvvidconv flip-method=0 ! " \
                           "video/x-raw, format=(string)RGBA ! queue max-size-time=%lu leaky=2 ! appsink processing-deadline=0 name=sinkR " \
                              "caps=\"video/x-raw,format=RGBA,pixel-aspect-ratio=1/1\""

#if defined(MKV_OUT) && defined(SOFTWARE_ENCODE)

#ifndef RECORD_AUDIO
#define GST_ENC_PIPELINE   "appsrc name=srcEncode ! " \
                           "video/x-raw, width=(int)%d, height=(int)%d, format=(string)RGBA, framerate=(fraction)%d/1 ! " \
                           "nvvidconv ! video/x-raw, format=I420 ! x264enc bitrate=16000 speed-preset=1 ! " \
                           "h264parse ! matroskamux ! filesink location=%s"
#else
// Untested
#define GST_ENC_PIPELINE "appsrc name=srcEncode ! " \
                         "video/x-raw, width=(int)%d, height=(int)%d, format=(string)RGBA, framerate=(fraction)%d/1 ! " \
                         "nvvidconv ! video/x-raw, format=I420 ! x264enc bitrate=16000 speed-preset=1 ! " \
                         "h264parse ! queue ! mux. " \
                         "pulsesrc device=%s do-timestamp=true provide-clock=true ! " \
                         "audio/x-raw, format=(string)S16LE, rate=(int)44100, channels=(int)2 ! " \
                         "audioconvert ! voaacenc bitrate=128000 ! queue ! mux. " \
                         "matroskamux name=mux ! filesink location=%s"
#endif

#define GST_ENCSTR_PIPELINE   "appsrc name=srcEncode ! " \
                              "video/x-raw, width=(int)2880, height=(int)1440, format=(string)RGBA, framerate=(fraction)30/1 ! " \
                              "tee name=split ! nvvidconv ! video/x-raw, format=I420 ! " \
                              "x264enc bitrate=16000 profile=4 preset-level=4 speed-preset=1 ! " \
                              "h264parse ! matroskamux ! filesink location=%s " \
                              "split. ! nvvidconv ! video/x-raw, format=I420, width=(int)%d, height=(int)%d ! " \
                              "x264enc bitrate=8000 speed-preset=1 ! " \
                              "h264parse config-interval=1 ! rtph264pay ! udpsink host=%s port=5000 sync=false"

#elif defined(MKV_OUT) && !defined(SOFTWARE_ENCODE)
#ifndef RECORD_AUDIO
#define GST_ENC_PIPELINE   "appsrc name=srcEncode ! " \
                           "video/x-raw, width=(int)%d, height=(int)%d, format=(string)RGBA, framerate=(fraction)%d/1 ! " \
                           "nvvidconv ! video/x-raw(memory:NVMM), format=NV12 ! " \
                           "nvv4l2h264enc bitrate=16000000 profile=4 preset-level=4 ! " \
                           "h264parse ! matroskamux ! filesink location=%s"
#else
#define GST_ENC_PIPELINE "appsrc name=srcEncode ! " \
                         "video/x-raw, width=(int)%d, height=(int)%d, format=(string)RGBA, framerate=(fraction)%d/1 ! " \
                         "nvvidconv ! video/x-raw(memory:NVMM), format=NV12 ! " \
                         "nvv4l2h264enc bitrate=16000000 profile=4 preset-level=4 ! " \
                         "h264parse ! queue ! mux. " \
                         "pulsesrc device=%s do-timestamp=true provide-clock=true ! " \
                         "audio/x-raw, format=(string)S16LE, rate=(int)44100, channels=(int)2 ! " \
                         "audioconvert ! voaacenc bitrate=128000 ! queue ! mux. " \
                         "matroskamux name=mux ! filesink location=%s"
#endif

#define GST_ENCSTR_PIPELINE   "appsrc name=srcEncode ! " \
                              "video/x-raw, width=(int)2880, height=(int)1440, format=(string)RGBA, framerate=(fraction)30/1 ! " \
                              "tee name=split ! nvvidconv ! video/x-raw(memory:NVMM), format=NV12 ! " \
                              "nvv4l2h264enc bitrate=16000000 profile=4 preset-level=4 ! " \
                              "h264parse ! matroskamux ! filesink location=%s " \
                              "split. ! nvvidconv ! video/x-raw(memory:NVMM), format=NV12, width=(int)%d, height=(int)%d ! " \
                              "nvv4l2h264enc bitrate=8000000 profile=4 preset-level=4 ! " \
                              "h264parse config-interval=1 ! rtph264pay ! udpsink host=%s port=5000 sync=false"

#elif !defined(MKV_OUT) && defined(SOFTWARE_ENCODE)

#ifndef RECORD_AUDIO
#define GST_ENC_PIPELINE   "appsrc name=srcEncode ! " \
                           "video/x-raw, width=(int)%d, height=(int)%d, format=(string)RGBA, framerate=(fraction)%d/1 ! " \
                           "nvvidconv ! video/x-raw, format=I420 ! x264enc bitrate=16000 ! " \
                           "h264parse ! qtmux ! filesink location=%s"
#else
// untested
#define GST_ENC_PIPELINE "appsrc name=srcEncode ! " \
                         "video/x-raw, width=(int)%d, height=(int)%d, format=(string)RGBA, framerate=(fraction)%d/1 ! " \
                         "nvvidconv ! video/x-raw, format=I420 ! x264enc bitrate=16000 ! " \
                         "h264parse ! queue ! mux. " \
                         "pulsesrc device=%s do-timestamp=true provide-clock=true ! " \
                         "audio/x-raw, format=(string)S16LE, rate=(int)44100, channels=(int)2 ! " \
                         "audioconvert ! voaacenc bitrate=128000 ! queue ! mux. " \
                         "qtmux name=mux ! filesink location=%s"
#endif

#define GST_ENCSTR_PIPELINE   "appsrc name=srcEncode ! " \
                              "video/x-raw, width=(int)2880, height=(int)1440, format=(string)RGBA, framerate=(fraction)30/1 ! " \
                              "tee name=split ! nvvidconv ! video/x-raw, format=I420 ! " \
                              "x264enc bitrate=16000 speed-preset=1 ! " \
                              "h264parse ! qtmux ! filesink location=%s " \
                              "split. ! nvvidconv ! video/x-raw, format=I420, width=(int)%d, height=(int)%d ! " \
                              "x264enc bitrate=8000 speed-preset=1 ! " \
                              "h264parse config-interval=1 ! rtph264pay ! udpsink host=%s port=5000 sync=false"

#elif !defined(MKV_OUT) && !defined(SOFTWARE_ENCODE)

#ifndef RECORD_AUDIO
#define GST_ENC_PIPELINE   "appsrc name=srcEncode ! " \
                           "video/x-raw, width=(int)%d, height=(int)%d, format=(string)RGBA, framerate=(fraction)%d/1 ! " \
                           "nvvidconv ! video/x-raw(memory:NVMM), format=NV12 ! " \
                           "nvv4l2h264enc bitrate=16000000 profile=4 preset-level=4 ! " \
                           "h264parse ! qtmux ! filesink location=%s"
#else
// untested
#define GST_ENC_PIPELINE "appsrc name=srcEncode ! " \
                         "video/x-raw, width=(int)%d, height=(int)%d, format=(string)RGBA, framerate=(fraction)%d/1 ! " \
                         "nvvidconv ! video/x-raw(memory:NVMM), format=NV12 ! " \
                         "nvv4l2h264enc bitrate=16000000 profile=4 preset-level=4 ! " \
                         "h264parse ! queue ! mux. " \
                         "pulsesrc device=%s do-timestamp=true provide-clock=true ! " \
                         "audio/x-raw, format=(string)S16LE, rate=(int)44100, channels=(int)2 ! " \
                         "audioconvert ! voaacenc bitrate=128000 ! queue ! mux. " \
                         "qtmux name=mux ! filesink location=%s"
#endif

#define GST_ENCSTR_PIPELINE   "appsrc name=srcEncode ! " \
                              "video/x-raw, width=(int)2880, height=(int)1440, format=(string)RGBA, framerate=(fraction)30/1 ! " \
                              "tee name=split ! nvvidconv ! video/x-raw(memory:NVMM), format=NV12 ! " \
                              "nvv4l2h264enc bitrate=16000000 profile=4 preset-level=4 ! " \
                              "h264parse ! qtmux ! filesink location=%s " \
                              "split. ! nvvidconv ! video/x-raw(memory:NVMM), format=NV12, width=(int)%d, height=(int)%d ! " \
                              "nvv4l2h264enc bitrate=8000000 profile=4 preset-level=4 ! " \
                              "h264parse config-interval=1 ! rtph264pay ! udpsink host=%s port=5000 sync=false"

#endif

#ifndef RECORD_AUDIO
#define GST_STR_PIPELINE "appsrc name=srcEncode ! " \
                         "video/x-raw, width=(int)%d, height=(int)%d, format=(string)RGBA, framerate=(fraction)%d/1 ! " \
                         "nvvidconv ! video/x-raw(memory:NVMM), format=NV12 ! " \
                         "nvv4l2h264enc bitrate=8000000 profile=2 preset-level=3 ! " \
                         "h264parse ! mux. " \
                         "hlssink2 name=mux playlist-root=http://%s/hls/ location=/var/www/html/hls/segment%%d.ts playlist-location=/var/www/html/hls/playlist.m3u8"
#else
#define GST_STR_PIPELINE "appsrc name=srcEncode ! " \
                         "video/x-raw, width=(int)%d, height=(int)%d, format=(string)RGBA, framerate=(fraction)%d/1 ! " \
                         "nvvidconv ! video/x-raw(memory:NVMM), width=(int)%d, height=(int)%d, format=NV12 ! " \
                         "nvv4l2h264enc bitrate=%d control-rate=0 iframeinterval=120 profile=2 preset-level=3 ! " \
                         "h264parse ! queue leaky=2 ! mux. " \
                         "pulsesrc device=%s do-timestamp=true ! " \
                         "audio/x-raw, format=(string)S16LE, rate=(int)44100, channels=(int)2 ! " \
                         "audioconvert ! voaacenc bitrate=128000 ! " \
                         "aacparse ! queue leaky=2 ! mux. " \
                         "flvmux name=mux streamable=true latency=100000000 ! " \
                         "rtmpsink location='rtmp://a.rtmp.youtube.com/live2/%s live=1'"
#endif

#endif // DEFINES_H

