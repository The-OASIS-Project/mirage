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

#include "config/config_manager.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "config/defines.h"
#include "logging.h"
#include "util/string_utils.h"

/* Default image and font paths. These are configurable in the config file. */
static char IMAGE_PATH[MAX_FILENAME_LENGTH] = IMAGE_PATH_DEFAULT;
static char SOUND_PATH[MAX_FILENAME_LENGTH] = SOUND_PATH_DEFAULT;
static char FONT_PATH[MAX_FILENAME_LENGTH] = FONT_PATH_DEFAULT;

static char WIFI_DEV_NAME[MAX_WIFI_DEV_LENGTH] =
    DEFAULT_WIFI_DEV_NAME; /* Buffer for wifi device name. */
                           /* As found in /proc/net/wireless */

static int inv_compass = 0; /* Sometimes compass data comes in "upside down".
                               We can fix that. */
static hud_display_settings this_hds = {
   .cam_input_width = DEFAULT_CAM_INPUT_WIDTH,
   .cam_input_height = DEFAULT_CAM_INPUT_HEIGHT,
   .cam_input_fps = DEFAULT_CAM_INPUT_FPS,
   //.cam_frame_duration = (long) ceil((long)1000000000 / DEFAULT_CAM_INPUT_FPS),
   .cam_frame_duration = (long)1000000000 / DEFAULT_CAM_INPUT_FPS + 1,

   .cam_crop_width = DEFAULT_CAM_CROP_WIDTH,
   .cam_crop_x = DEFAULT_CAM_CROP_X,

   .eye_output_width = DEFAULT_EYE_OUTPUT_WIDTH,
   .eye_output_height = DEFAULT_EYE_OUTPUT_HEIGHT,

   .stereo_offset = 0
};

static stream_settings this_ss = { .stream_dest_ip = DEFAULT_STREAM_DEST_IP,
                                   .stream_width = DEFAULT_EYE_OUTPUT_WIDTH * 2,
                                   .stream_height = DEFAULT_EYE_OUTPUT_HEIGHT };

static armor_settings this_as = { .armor_elements = NULL,
                                  .armor_deregister = DEFAULT_ARMOR_DEREGISTER_TIMEOUT };

hud_display_settings *get_hud_display_settings(void) {
   return &this_hds;
}

stream_settings *get_stream_settings(void) {
   return &this_ss;
}

armor_settings *get_armor_settings(void) {
   return &this_as;
}

/* Image Paths */
const char *get_image_path(void) {
   return IMAGE_PATH;
}

char *set_image_path(const char *path, int length) {
   if ((path != NULL) && (length > 0) && (length < MAX_FILENAME_LENGTH)) {
      return strncpy(IMAGE_PATH, path, length);
   }

   if (path == NULL) {
      OLOG_ERROR("%s: path is NULL", __func__);
   }

   if (length <= 0) {
      OLOG_ERROR("%s: length is too short", __func__);
   }

   if (length >= MAX_FILENAME_LENGTH) {
      OLOG_ERROR("%s: length is too long", __func__);
   }

   return NULL;
}

/* Sound Paths */
const char *get_sound_path(void) {
   return SOUND_PATH;
}

char *set_sound_path(const char *path, int length) {
   if ((path != NULL) && (length > 0) && (length < MAX_FILENAME_LENGTH)) {
      return strncpy(SOUND_PATH, path, length);
   }

   if (path == NULL) {
      OLOG_ERROR("%s: path is NULL", __func__);
   }

   if (length <= 0) {
      OLOG_ERROR("%s: length is too short", __func__);
   }

   if (length >= MAX_FILENAME_LENGTH) {
      OLOG_ERROR("%s: length is too long", __func__);
   }

   return NULL;
}

/* Font Paths */
const char *get_font_path(void) {
   return FONT_PATH;
}

char *set_font_path(const char *path, int length) {
   if ((path != NULL) && (length > 0) && (length < MAX_FILENAME_LENGTH)) {
      return strncpy(FONT_PATH, path, length);
   }

   if (path == NULL) {
      OLOG_ERROR("%s: path is NULL", __func__);
   }

   if (length <= 0) {
      OLOG_ERROR("%s: length is too short", __func__);
   }

   if (length >= MAX_FILENAME_LENGTH) {
      OLOG_ERROR("%s: length is too long", __func__);
   }

   return NULL;
}

/* Wifi Device Name */
const char *get_wifi_dev_name(void) {
   return WIFI_DEV_NAME;
}

const char *set_wifi_dev_name(const char *name, int length) {
   if ((name != NULL) && (length > 0) && (length < MAX_WIFI_DEV_LENGTH)) {
      return strncpy(WIFI_DEV_NAME, name, length);
   }

   if (name == NULL) {
      OLOG_ERROR("%s: name is NULL", __func__);
   }

   if (length <= 0) {
      OLOG_ERROR("%s: length is too short", __func__);
   }

   if (length >= MAX_WIFI_DEV_LENGTH) {
      OLOG_ERROR("%s: length is too long", __func__);
   }

   return NULL;
}

/* Compass Settings */
int get_inv_compass(void) {
   return inv_compass;
}

int set_inv_compass(int inv) {
   inv_compass = inv;

   return inv;
}

int get_snapshot_overlay(void) {
   return this_hds.snapshot_overlay;
}

int get_vision_inline_data(void) {
   return this_hds.vision_inline_data;
}

/* MQTT connection settings */
static char mqtt_host[64] = "127.0.0.1";
static int mqtt_port = 1883;
static int mqtt_tls = 0;
static char mqtt_tls_ca_cert[1024] = "";
static char mqtt_tls_cert_path[1024] = "";
static char mqtt_tls_key_path[1024] = "";

const char *get_mqtt_host(void) {
   return mqtt_host;
}

int get_mqtt_port(void) {
   return mqtt_port;
}

void set_mqtt_host(const char *host) {
   safe_strncpy(mqtt_host, host, sizeof(mqtt_host));
}

void set_mqtt_port(int port) {
   mqtt_port = port;
}

/* Serial (helmet) connection settings.  serial_port empty => fall back to the
 * compile-time USB_PORT default; serial_enable -1 => unset in config (CLI/default
 * decides).  Prefer a stable /dev/serial/by-id/... path so a helmet re-enumeration
 * can't shift the ttyACM number out from under the reconnect loop. */
static char serial_port_cfg[MAX_SERIAL_PORT_LENGTH] = "";
static int serial_enable_cfg = -1;

const char *get_serial_port(void) {
   return serial_port_cfg;
}

int get_serial_enable(void) {
   return serial_enable_cfg;
}

void set_serial_port(const char *port) {
   if (port != NULL) {
      safe_strncpy(serial_port_cfg, port, sizeof(serial_port_cfg));
   }
}

void set_serial_enable(int enable) {
   serial_enable_cfg = enable ? 1 : 0;
}

int get_mqtt_tls(void) {
   return mqtt_tls;
}

void set_mqtt_tls(int enabled) {
   mqtt_tls = enabled;
}

const char *get_mqtt_tls_ca_cert(void) {
   return mqtt_tls_ca_cert;
}

void set_mqtt_tls_ca_cert(const char *path) {
   safe_strncpy(mqtt_tls_ca_cert, path, sizeof(mqtt_tls_ca_cert));
}

const char *get_mqtt_tls_cert_path(void) {
   return mqtt_tls_cert_path;
}

void set_mqtt_tls_cert_path(const char *path) {
   safe_strncpy(mqtt_tls_cert_path, path, sizeof(mqtt_tls_cert_path));
}

const char *get_mqtt_tls_key_path(void) {
   return mqtt_tls_key_path;
}

void set_mqtt_tls_key_path(const char *path) {
   safe_strncpy(mqtt_tls_key_path, path, sizeof(mqtt_tls_key_path));
}
