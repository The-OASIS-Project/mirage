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

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "config/defines.h"

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

#define MAX_FRAMES 1024 /* Maximum number of animation frames per animation. */

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

/* Map type enumeration */
typedef enum {
   MAP_TYPE_HYBRID = 0,
   MAP_TYPE_SATELLITE,
   MAP_TYPE_ROADMAP,
   MAP_TYPE_TERRAIN,
   MAP_TYPE_COUNT /* Always keep last to get count */
} map_type_t;

/* Map type string representations - declare as extern */
extern const char *MAP_TYPE_STRINGS[];

/* Dynamic text source IDs for O(1) dispatch in the render loop.
 * Resolved once at config parse time from the element's text string. */
typedef enum {
   TEXT_SOURCE_STATIC = 0, /* No dynamic source -- render text as-is */
   TEXT_SOURCE_FPS,
   TEXT_SOURCE_DATETIME,
   TEXT_SOURCE_GPSTIME,
   TEXT_SOURCE_SYSTIME,
   TEXT_SOURCE_AINAME,
   TEXT_SOURCE_CPU,
   TEXT_SOURCE_SYSTEM_TEMP,
   TEXT_SOURCE_SYSTEM_TEMP_F,
   TEXT_SOURCE_MEM,
   TEXT_SOURCE_HELMTEMP,
   TEXT_SOURCE_HELMTEMP_F,
   TEXT_SOURCE_HELMHUM,
   TEXT_SOURCE_AIRQUALITY,
   TEXT_SOURCE_AIRQUALITYDESC,
   TEXT_SOURCE_TVOC,
   TEXT_SOURCE_ECO2,
   TEXT_SOURCE_CO2,
   TEXT_SOURCE_CO2QUALITY,
   TEXT_SOURCE_CO2ECO2DIFF,
   TEXT_SOURCE_CO2SOURCEANALYSIS,
   TEXT_SOURCE_HEATINDEX_C,
   TEXT_SOURCE_DEWPOINT,
   TEXT_SOURCE_FAN,
   TEXT_SOURCE_BATTERY_LEVEL,
   TEXT_SOURCE_BATTERY_STATUS,
   TEXT_SOURCE_BATTERY_STATUS_REASON,
   TEXT_SOURCE_BATTERY_CELLS_CONFIG,
   TEXT_SOURCE_BATTERY_FAULT_COUNT,
   TEXT_SOURCE_BATTERY_CRITICAL_FAULTS,
   TEXT_SOURCE_BATTERY_WARNING_FAULTS,
   TEXT_SOURCE_BATTERY_INFO_FAULTS,
   TEXT_SOURCE_BATTERY_ALL_FAULTS,
   TEXT_SOURCE_BATTERY_TIME,
   TEXT_SOURCE_BATTERY_TIME_MIN,
   TEXT_SOURCE_BATTERY_VOLTAGE,
   TEXT_SOURCE_BATTERY_CURRENT,
   TEXT_SOURCE_BATTERY_POWER,
   TEXT_SOURCE_BATTERY_TEMP,
   TEXT_SOURCE_BATTERY_CHEMISTRY,
   TEXT_SOURCE_BATTERY_CAPACITY,
   TEXT_SOURCE_BATTERY_CELLS,
   TEXT_SOURCE_LATLON,
   TEXT_SOURCE_PITCH,
   TEXT_SOURCE_COMPASS,
   TEXT_SOURCE_LOG,
   TEXT_SOURCE_ALERT,
   /* Notification text sources */
   TEXT_SOURCE_CALLER_NAME,
   TEXT_SOURCE_CALLER_NUMBER,
   TEXT_SOURCE_CALL_STATUS,
   TEXT_SOURCE_SMS_PREVIEW,
   TEXT_SOURCE_NOTIFICATION_TITLE,
   TEXT_SOURCE_NOTIFICATION_SOURCE,
   TEXT_SOURCE_COUNT /* Always last */
} text_source_t;

/**
 * @brief Resolve a text element's dynamic source string to an enum ID.
 *
 * Called once at config parse time so the render loop can use a switch
 * instead of 47+ strcmp() calls per frame.
 *
 * @param text The element's text field (e.g., "*FPS*", "*BATTERY_LEVEL*")
 * @return The corresponding text_source_t enum value, or TEXT_SOURCE_STATIC
 */
text_source_t resolve_text_source(const char *text);

/* Parent data type for all UI elements. Not all fields are used for all types. */
typedef struct _element {
   element_t type;
   int enabled;

   char name[MAX_TEXT_LENGTH];
   /**
    * Bitmap of HUD memberships.
    * Each bit in this array represents membership in a specific HUD.
    * - A value of 1 indicates that the element belongs to the corresponding HUD.
    * - A value of 0 indicates that the element does not belong to the corresponding HUD.
    * The maximum number of HUDs is defined by the MAX_HUDS constant from defines.h.
    */
   char hud_flags[MAX_HUDS];
   char hotkey[2]; /* Hotkey to enable/disable element */

   /* Static and animated graphics */
   char filename[MAX_FILENAME_LENGTH * 2];    /* Regular filename to graphic. */
   char filename_r[MAX_FILENAME_LENGTH * 2];  /* Recording filename to graphic. */
   char filename_s[MAX_FILENAME_LENGTH * 2];  /* Streaming filename to graphic. */
   char filename_rs[MAX_FILENAME_LENGTH * 2]; /* Recording and streaming to graphic. */
   char filename_l[MAX_FILENAME_LENGTH * 2];  /* AI listening filename graphic. */
   char filename_w[MAX_FILENAME_LENGTH * 2];  /* AI wakework filename graphic. */
   char filename_p[MAX_FILENAME_LENGTH * 2];  /* AI processing filename graphic. */

   char filename_base[MAX_FILENAME_LENGTH * 2];    /* Filename of base armor graphic. */
   char filename_online[MAX_FILENAME_LENGTH * 2];  /* Filename of online armor graphic. */
   char filename_warning[MAX_FILENAME_LENGTH * 2]; /* Filename of warning armor graphic. */
   char filename_offline[MAX_FILENAME_LENGTH * 2]; /* Filename of offline armor graphic. */

   /* Text elements */
   char text[MAX_TEXT_LENGTH];
   char last_rendered_text[MAX_TEXT_LENGTH];
   text_source_t text_source_id; /* Resolved at parse time for O(1) render dispatch */
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

   int width;
   int height;

   /* Map-specific settings */
   int download_count;
   map_type_t map_type;
   int map_zoom;
   int update_interval_sec;
   int force_refresh;

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

   /* Armor metrics positioning */
   float metrics_x_offset; /* Horizontal position within component (0.0 - 1.0) */
   float metrics_y_offset; /* Vertical position within component (0.0 - 1.0) */

   /* Armor display properties */
   int notice_x;
   int notice_y;
   int notice_width;
   int notice_height;
   int notice_timeout;
   int show_metrics;
   char metrics_font[MAX_FILENAME_LENGTH * 2];
   int metrics_font_size;

   /* Metrics texture caching */
   SDL_Texture **metrics_textures; /* Array of metrics textures for each component */
   char **last_metrics_text;       /* Array of last metrics text strings */
   int metrics_texture_count;      /* Number of components with metrics */

   armor_warning_t warn_state;

   /* Gauge-specific properties */
   char gauge_type[MAX_TEXT_LENGTH];         /* "linear", "arc", "ring" */
   float gauge_min_value;                    /* Minimum value on scale */
   float gauge_max_value;                    /* Maximum value on scale */
   char gauge_value_source[MAX_TEXT_LENGTH]; /* "*BATTERY*", "*SPEED*", etc. or static value */
   float gauge_current_value;                /* Current numeric value (computed) */
   float gauge_warning_threshold;            /* Value at which to change color */
   SDL_Color gauge_primary_color;            /* Normal operation color */
   SDL_Color gauge_warning_color;            /* Warning/critical color */
   int gauge_orientation;                    /* 0=horizontal, 1=vertical (linear only) */
   float gauge_arc_start;                    /* Start angle in degrees (arc/ring) */
   float gauge_arc_sweep;                    /* Sweep angle in degrees (arc/ring) */
   int gauge_thickness;                      /* Line thickness for ring gauges */
   int gauge_ticks;                          /* Number of tick marks (arc gauges) */
   int gauge_smooth;                         /* Enable smooth interpolation */
   float gauge_smooth_factor;                /* Smoothing speed (0.0-1.0, default 0.2) */
   unsigned int gauge_last_update_ms;        /* Last frame time for delta-time smoothing */
   int gauge_glow;                           /* Enable glow effect */
   float gauge_display_value;                /* Smoothed value for rendering */
   int gauge_show_value;                     /* Show numeric value label (0/1) */
   char gauge_value_format[32];              /* Format string for value label (e.g., "%.1f%%") */
   SDL_Color gauge_value_color;              /* Color for value label text */
   int gauge_value_size;                     /* Font size for value label */

   /* Gauge cache for performance optimization */
   SDL_Texture *gauge_cache_texture;       /* Pre-rendered static background */
   int gauge_cache_dirty;                  /* 1 = needs regeneration, 0 = valid */
   float gauge_last_rendered_value;        /* Last value we rendered label for */
   SDL_Texture *gauge_value_label_texture; /* Cached value label texture */
   int gauge_value_label_width;            /* Cached label dimensions */
   int gauge_value_label_height;

   /* Notification group — links element to a notification slot (resolved at parse time) */
   int notification_group; /* notif_group_t enum from notification.h */

   /* Transition state - used for fade/zoom effects */
   float transition_alpha;
   int in_transition;
   float scale;

   struct _element *prev;
   struct _element *next;
} element;

/**
 * @brief Checks if the configuration file has been modified and reloads it if necessary.
 *
 * This function monitors the configuration file's modification time and automatically
 * reloads the UI layout when changes are detected. The check is performed at regular
 * intervals defined by the config_check_interval. If the file has been modified since
 * the last check, it will attempt to reload the configuration while preserving the
 * current HUD state and providing appropriate user feedback via the alert system.
 *
 * @param config_filename Path to the configuration file to monitor
 * @return SUCCESS if no reload was needed or reload was successful, FAILURE on error
 */
int check_and_reload_config(const char *config_filename);

/**
 * @brief Safely reloads the configuration file and updates the UI layout.
 *
 * This function performs a complete reload of the configuration file, including
 * validation of the new configuration before applying changes. It preserves the
 * current HUD state when possible and properly cleans up old resources. The reload
 * process is atomic - if the new configuration is invalid, the current configuration
 * remains unchanged and an error is logged.
 *
 * @param config_filename Path to the configuration file to reload
 * @return SUCCESS if reload was successful, FAILURE if new config was invalid
 */
int reload_config(const char *config_filename);

// Function prototypes for parsing functions
int parse_animated_json(element *curr_element);
int parse_color(char *string,
                unsigned char *r,
                unsigned char *g,
                unsigned char *b,
                unsigned char *a);
int parse_json_config(const char *filename);

#endif  // CONFIG_PARSER_H
