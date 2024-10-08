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

#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

#include "SDL2/SDL.h"
#include "SDL2/SDL_ttf.h"

#include "defines.h"

/* Warning States */
typedef enum {
   WARN_NORMAL = 0x0,
   WARN_OVER_TEMP = 0x1,
   WARN_OVER_VOLT = 0x2
} armor_warning_t;

/* A single frame in an animaion. */
typedef struct _frame {
   int source_x;
   int source_y;
   int source_w;
   int source_h;

   int rotated;
   int trimmed;

   int dest_x;
   int dest_y;

   int source_size_w;
   int source_size_h;

   struct _frame *next;
} frame;

#define MAX_FRAMES 1024                /* Maximum number of animation frames per animation. */

/* Animation Object */
typedef struct _anim {
   frame *first_frame;
   frame *current_frame;
   int frame_count;

   unsigned int last_update;

   frame *frame_lookup[MAX_FRAMES];

   char image[MAX_FILENAME_LENGTH];
   char format[12];
} anim;

/* Types of UI Elements */
typedef enum {
   STATIC,
   ANIMATED,
   ANIMATED_DYNAMIC,
   TEXT,
   SPECIAL,
   ARMOR_COMPONENT
} element_t;

/* Parent data type for all UI elements. Not all fields are used for all types. */
typedef struct _element {
   element_t type;
   int enabled;

   char name[MAX_TEXT_LENGTH];
   char hotkey[2];   /* Hotkey to enable/disable element */

   /* Static and animated graphics */
   char filename[MAX_FILENAME_LENGTH * 2];      /* Regular filename to graphic. */
   char filename_r[MAX_FILENAME_LENGTH * 2];    /* Recording filename to graphic. */
   char filename_s[MAX_FILENAME_LENGTH * 2];    /* Streaming filename to graphic. */
   char filename_rs[MAX_FILENAME_LENGTH * 2];   /* Recording and streaming to graphic. */
   char filename_l[MAX_FILENAME_LENGTH * 2];    /* AI listening filename graphic. */
   char filename_w[MAX_FILENAME_LENGTH * 2];    /* AI wakework filename graphic. */
   char filename_p[MAX_FILENAME_LENGTH * 2];    /* AI processing filename graphic. */

   char filename_online[MAX_FILENAME_LENGTH * 2];    /* Filename of online armor graphic. */
   char filename_warning[MAX_FILENAME_LENGTH * 2];   /* Filename of warning armor graphic. */
   char filename_offline[MAX_FILENAME_LENGTH * 2];   /* Filename of offline armor graphic. */

   /* Dynamic Animation */
   SDL_Rect src_dst_rect, dst_dst_rect;         /* Where to start and end the animation. */
   int anim_dur;                                /* How long should this animation take (secs)? */
   int anim_frames_calc;                        /* We'll calculate how many total frames they'll
                                                 * based on current FPS. */
   int dyn_anim_frame;                          /* Which frame are we currenty on? */

   /* Text elements */
   char text[MAX_TEXT_LENGTH];
   char font[MAX_FILENAME_LENGTH * 2];
   SDL_Color font_color;
   TTF_Font *ttf_font;
   int font_size;
   char halign[7];

   /* Location information */
   int dest_x;
   int dest_y;
   double angle;
   int fixed;

   /* Layer */
   int layer;

   /* SDL components for display */
   SDL_Surface *surface;
   SDL_Texture *texture;
   SDL_Texture *texture_r;
   SDL_Texture *texture_s;
   SDL_Texture *texture_rs;
   SDL_Texture *texture_l;
   SDL_Texture *texture_w;
   SDL_Texture *texture_p;

   SDL_Texture *texture_base;
   SDL_Texture *texture_online;
   SDL_Texture *texture_warning;
   SDL_Texture *texture_offline;

   SDL_Rect dst_rect;

   char special_name[MAX_TEXT_LENGTH];
   char mqtt_device[MAX_TEXT_LENGTH];
   int mqtt_registered;
   time_t mqtt_last_time;

   /* The following are currently only used for map size. */
   int width;
   int height;

   int center_x_offset;
   int center_y_offset;

   int text_x_offset;
   int text_y_offset;

   /* Animation */
   anim this_anim;

   /* Armor Warnings */
   double warning_temp;
   double warning_voltage;

   double last_temp;
   double last_voltage;

   armor_warning_t warn_state;

   struct _element *prev;
   struct _element *next;
} element;

// Function prototypes for parsing functions
int parse_animated_json(element * curr_element);
int parse_color(char *string, unsigned char *r, unsigned char *g, unsigned char *b,
                unsigned char *a);
int parse_json_config(char *filename);

#endif // CONFIG_PARSER_H

