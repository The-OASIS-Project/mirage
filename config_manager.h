#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include "SDL2/SDL.h"
#include "config_parser.h"

/* Variables for display settings */
typedef struct _hud_display_settings {
   int cam_input_width;
   int cam_input_height;
   int cam_input_fps;
   long cam_frame_duration;   /* The duration of a single frame. */

   int cam_crop_width;
   int cam_crop_x;

   int eye_output_width;
   int eye_output_height;

   int stereo_offset;         /* Offset to adjust stereo perception of UI elements.
                               * This offsets to the left or right accordingly on each eye. */
} hud_display_settings;

hud_display_settings *get_hud_display_settings(void);

/* Variables for streaming output */
typedef struct _stream_settings {
   char stream_dest_ip[16];
   int stream_width;
   int stream_height;
} stream_settings;

stream_settings *get_stream_settings(void);

/* Variables for the armor components */
typedef struct _armor_settings {
   element *armor_elements;      /* A special element list for armor components. */

   SDL_Rect armor_dest;          /* Where do we draw the armor on the screen */
   SDL_Rect armor_notice_dest;   /* Where do we draw the armor when we get a notification */
   int armor_notice_timeout;     /* This is the configurable value, how long does the notice stay up. */
   time_t armor_deregister;      /* How long can we go without seeing an MQTT message from an armor
                                  * element before we say it's missing? */
} armor_settings;

armor_settings *get_armor_settings(void);

const char *get_image_path(void);
char *set_image_path(const char *path, int length);

const char *get_sound_path(void);
char *set_sound_path(const char *path, int length);

const char *get_font_path(void);
char *set_font_path(const char *path, int length);

const char *get_wifi_dev_name(void);
const char *set_wifi_dev_name(const char *name, int length);

int get_inv_compass(void);
int set_inv_compass(int inv);


#endif // CONFIG_MANAGER_H
