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
 * All contributions to this project are agreed to be licensed under the
 * GPLv3 or any later version. Contributions are understood to be
 * any modifications, enhancements, or additions to the project
 * and become the property of the original author Kris Kersey.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "defines.h"
#include "config_manager.h"

/* Default image and font paths. These are configurable in the config file. */
static char IMAGE_PATH[MAX_FILENAME_LENGTH] = IMAGE_PATH_DEFAULT;
static char SOUND_PATH[MAX_FILENAME_LENGTH] = SOUND_PATH_DEFAULT;
static char FONT_PATH[MAX_FILENAME_LENGTH] = FONT_PATH_DEFAULT;

static char WIFI_DEV_NAME[MAX_WIFI_DEV_LENGTH] = DEFAULT_WIFI_DEV_NAME; /* Buffer for wifi device name. */
                                                                        /* As found in /proc/net/wireless */

static int inv_compass = 0;                  /* Sometimes compass data comes in "upside down".
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

static stream_settings this_ss = {
   .stream_dest_ip = DEFAULT_STREAM_DEST_IP,
   .stream_width = DEFAULT_EYE_OUTPUT_WIDTH * 2,
   .stream_height = DEFAULT_EYE_OUTPUT_HEIGHT
};

static armor_settings this_as = {
   .armor_elements = NULL,

   .armor_notice_timeout = DEFAULT_ARMOR_NOTICE_TIMEOUT,
   .armor_deregister = DEFAULT_ARMOR_DEREGISTER_TIMEOUT
};

hud_display_settings *get_hud_display_settings(void)
{
   return &this_hds;
}

stream_settings *get_stream_settings(void)
{
   return &this_ss;
}

armor_settings *get_armor_settings(void)
{
   return &this_as;
}

/* Image Paths */
const char *get_image_path(void)
{
   return IMAGE_PATH;
}

char *set_image_path(const char *path, int length)
{
   if ((path != NULL) && (length > 0) && (length < MAX_FILENAME_LENGTH))
   {
      return strncpy(IMAGE_PATH, path, length);
   }

   if (path == NULL)
   {
      printf("%s: path is NULL\n", __func__);
   }

   if (length <= 0)
   {
      printf("%s: length is too short\n", __func__);
   }

   if (length >= MAX_FILENAME_LENGTH)
   {
      printf("%s: length is too long\n", __func__);
   }

   return NULL;
}

/* Sound Paths */
const char *get_sound_path(void)
{
   return SOUND_PATH;
}

char *set_sound_path(const char *path, int length)
{
   if ((path != NULL) && (length > 0) && (length < MAX_FILENAME_LENGTH))
   {
      return strncpy(SOUND_PATH, path, length);
   }

   if (path == NULL)
   {
      printf("%s: path is NULL\n", __func__);
   }

   if (length <= 0)
   {
      printf("%s: length is too short\n", __func__);
   }

   if (length >= MAX_FILENAME_LENGTH)
   {
      printf("%s: length is too long\n", __func__);
   }

   return NULL;
}

/* Font Paths */
const char *get_font_path(void)
{
   return FONT_PATH;
}

char *set_font_path(const char *path, int length)
{
   if ((path != NULL) && (length > 0) && (length < MAX_FILENAME_LENGTH))
   {
      return strncpy(FONT_PATH, path, length);
   }

   if (path == NULL)
   {
      printf("%s: path is NULL\n", __func__);
   }

   if (length <= 0)
   {
      printf("%s: length is too short\n", __func__);
   }

   if (length >= MAX_FILENAME_LENGTH)
   {
      printf("%s: length is too long\n", __func__);
   }

   return NULL;
}

/* Wifi Device Name */
const char *get_wifi_dev_name(void)
{
   return WIFI_DEV_NAME;
}

const char *set_wifi_dev_name(const char *name, int length)
{
   if ((name != NULL) && (length > 0) && (length < MAX_WIFI_DEV_LENGTH))
   {
      return strncpy(WIFI_DEV_NAME, name, length);
   }

   if (name == NULL)
   {
      printf("%s: name is NULL\n", __func__);
   }

   if (length <= 0)
   {
      printf("%s: length is too short\n", __func__);
   }

   if (length >= MAX_WIFI_DEV_LENGTH)
   {
      printf("%s: length is too long\n", __func__);
   }

   return NULL;
}

/* Compass Settings */
int get_inv_compass(void)
{
   return inv_compass;
}

int set_inv_compass(int inv)
{
   inv_compass = inv;

   return inv;
}
