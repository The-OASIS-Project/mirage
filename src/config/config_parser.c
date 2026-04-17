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

#include "config/config_parser.h"

#include <json-c/json.h>
#include <stdatomic.h>
#include <stdio.h>
#include <sys/stat.h>

#include "SDL2/SDL_image.h"
#include "config/config_manager.h"
#include "core/mirage.h"
#include "ui/hud_manager.h"
#include "ui/notification.h"
#include "util/logging.h"

/* Map type string representations */
const char *MAP_TYPE_STRINGS[] = { "hybrid", "satellite", "roadmap", "terrain" };

static time_t config_last_modified = 0; /* When was the config file last checked? */

extern _Atomic alert_t active_alerts;

/* Lookup table for resolving text source strings to enum IDs at parse time. */
static const struct {
   const char *key;
   text_source_t id;
} text_source_map[] = {
   { "*FPS*", TEXT_SOURCE_FPS },
   { "*DATETIME*", TEXT_SOURCE_DATETIME },
   { "*GPSTIME*", TEXT_SOURCE_GPSTIME },
   { "*SYSTIME*", TEXT_SOURCE_SYSTIME },
   { "*AINAME*", TEXT_SOURCE_AINAME },
   { "*CPU*", TEXT_SOURCE_CPU },
   { "*SYSTEM_TEMP*", TEXT_SOURCE_SYSTEM_TEMP },
   { "*SYSTEM_TEMP_F*", TEXT_SOURCE_SYSTEM_TEMP_F },
   { "*MEM*", TEXT_SOURCE_MEM },
   { "*HELMTEMP*", TEXT_SOURCE_HELMTEMP },
   { "*HELMTEMP_F*", TEXT_SOURCE_HELMTEMP_F },
   { "*HELMHUM*", TEXT_SOURCE_HELMHUM },
   { "*AIRQUALITY*", TEXT_SOURCE_AIRQUALITY },
   { "*AIRQUALITYDESC*", TEXT_SOURCE_AIRQUALITYDESC },
   { "*TVOC*", TEXT_SOURCE_TVOC },
   { "*ECO2*", TEXT_SOURCE_ECO2 },
   { "*CO2*", TEXT_SOURCE_CO2 },
   { "*CO2QUALITY*", TEXT_SOURCE_CO2QUALITY },
   { "*CO2ECO2DIFF*", TEXT_SOURCE_CO2ECO2DIFF },
   { "*CO2SOURCEANALYSIS*", TEXT_SOURCE_CO2SOURCEANALYSIS },
   { "*HEATINDEX_C*", TEXT_SOURCE_HEATINDEX_C },
   { "*DEWPOINT*", TEXT_SOURCE_DEWPOINT },
   { "*FAN*", TEXT_SOURCE_FAN },
   { "*BATTERY_LEVEL*", TEXT_SOURCE_BATTERY_LEVEL },
   { "*BATTERY_STATUS*", TEXT_SOURCE_BATTERY_STATUS },
   { "*BATTERY_STATUS_REASON*", TEXT_SOURCE_BATTERY_STATUS_REASON },
   { "*BATTERY_CELLS_CONFIG*", TEXT_SOURCE_BATTERY_CELLS_CONFIG },
   { "*BATTERY_FAULT_COUNT*", TEXT_SOURCE_BATTERY_FAULT_COUNT },
   { "*BATTERY_CRITICAL_FAULTS*", TEXT_SOURCE_BATTERY_CRITICAL_FAULTS },
   { "*BATTERY_WARNING_FAULTS*", TEXT_SOURCE_BATTERY_WARNING_FAULTS },
   { "*BATTERY_INFO_FAULTS*", TEXT_SOURCE_BATTERY_INFO_FAULTS },
   { "*BATTERY_ALL_FAULTS*", TEXT_SOURCE_BATTERY_ALL_FAULTS },
   { "*BATTERY_TIME*", TEXT_SOURCE_BATTERY_TIME },
   { "*BATTERY_TIME_MIN*", TEXT_SOURCE_BATTERY_TIME_MIN },
   { "*BATTERY_VOLTAGE*", TEXT_SOURCE_BATTERY_VOLTAGE },
   { "*BATTERY_CURRENT*", TEXT_SOURCE_BATTERY_CURRENT },
   { "*BATTERY_POWER*", TEXT_SOURCE_BATTERY_POWER },
   { "*BATTERY_TEMP*", TEXT_SOURCE_BATTERY_TEMP },
   { "*BATTERY_CHEMISTRY*", TEXT_SOURCE_BATTERY_CHEMISTRY },
   { "*BATTERY_CAPACITY*", TEXT_SOURCE_BATTERY_CAPACITY },
   { "*BATTERY_CELLS*", TEXT_SOURCE_BATTERY_CELLS },
   { "*LATLON*", TEXT_SOURCE_LATLON },
   { "*PITCH*", TEXT_SOURCE_PITCH },
   { "*COMPASS*", TEXT_SOURCE_COMPASS },
   { "*LOG*", TEXT_SOURCE_LOG },
   { "*ALERT*", TEXT_SOURCE_ALERT },
   /* Notification text sources */
   { "*CALLER_NAME*", TEXT_SOURCE_CALLER_NAME },
   { "*CALLER_NUMBER*", TEXT_SOURCE_CALLER_NUMBER },
   { "*CALL_STATUS*", TEXT_SOURCE_CALL_STATUS },
   { "*SMS_PREVIEW*", TEXT_SOURCE_SMS_PREVIEW },
   { "*NOTIFICATION_TITLE*", TEXT_SOURCE_NOTIFICATION_TITLE },
   { "*NOTIFICATION_SOURCE*", TEXT_SOURCE_NOTIFICATION_SOURCE },
};

text_source_t resolve_text_source(const char *text) {
   for (size_t i = 0; i < sizeof(text_source_map) / sizeof(text_source_map[0]); i++) {
      if (strcmp(text, text_source_map[i].key) == 0) {
         return text_source_map[i].id;
      }
   }
   return TEXT_SOURCE_STATIC;
}

/**
 * @brief Validates that a JSON config file is parseable without applying changes.
 *
 * @param config_filename Path to the configuration file to validate
 * @return SUCCESS if config is valid, FAILURE if invalid or unreadable
 */
static int validate_json_config(const char *config_filename) {
   FILE *config_file = NULL;
   struct json_object *parsed_json = NULL;
   char *config_string = NULL;
   int string_size = 0;
   int bytes_read = 0;
   char tmpstr[1024];

   /* Try to open and read the file */
   config_file = fopen(config_filename, "r");
   if (config_file == NULL) {
      LOG_ERROR("Cannot open config file for validation: %s", config_filename);
      return FAILURE;
   }

   /* Read entire file into memory */
   string_size = 0;
   while ((bytes_read = fread(tmpstr, 1, 1024, config_file)) > 0) {
      string_size += bytes_read;
      config_string = realloc(config_string, string_size + 1);
      if (string_size <= 1024) {
         strncpy(config_string, tmpstr, bytes_read);
      } else {
         strncat(config_string, tmpstr, bytes_read);
      }
      config_string[string_size] = '\0';
   }
   fclose(config_file);

   /* Try to parse JSON */
   parsed_json = json_tokener_parse(config_string);
   if (parsed_json == NULL) {
      LOG_ERROR("Invalid JSON in config file: %s", config_filename);
      free(config_string);
      return FAILURE;
   }

   /* Basic structure validation - check for required sections */
   struct json_object *tmpobj = NULL;

   /* Validate HUDs section exists */
   if (!json_object_object_get_ex(parsed_json, "HUDs", &tmpobj)) {
      LOG_ERROR("Missing 'HUDs' section in config file");
      json_object_put(parsed_json);
      free(config_string);
      return FAILURE;
   }

   /* Validate Elements section exists */
   if (!json_object_object_get_ex(parsed_json, "Elements", &tmpobj)) {
      LOG_ERROR("Missing 'Elements' section in config file");
      json_object_put(parsed_json);
      free(config_string);
      return FAILURE;
   }

   /* Could add more validation here if needed */

   json_object_put(parsed_json);
   free(config_string);
   return SUCCESS;
}

int reload_config(const char *config_filename) {
   // Save current HUD state for restoration
   hud_manager *hud_mgr = get_hud_manager();
   hud_screen *current_hud = hud_mgr->current_screen;
   char current_hud_name[MAX_TEXT_LENGTH] = { 0 };

   if (current_hud != NULL) {
      strncpy(current_hud_name, current_hud->name, MAX_TEXT_LENGTH - 1);
   }

   /* Validate new config before touching existing state */
   if (validate_json_config(config_filename) != SUCCESS) {
      LOG_ERROR("New config file is invalid, keeping current configuration");
      return FAILURE;
   }

   /* Config is valid, proceed with reload */

   // Free old elements
   element *old_first_element = get_first_element();
   armor_settings *this_as = get_armor_settings();

   if (old_first_element != NULL) {
      free_elements(old_first_element);
   }

   if (this_as->armor_elements != NULL) {
      free_elements(this_as->armor_elements);
   }

   // Clear pointers
   set_first_element(NULL);
   this_as->armor_elements = NULL;

   // Clean up existing HUD registry
   cleanup_hud_manager();

   // Re-initialize HUD manager
   init_hud_manager();

   /* Parse new config (should succeed since we validated) */
   if (parse_json_config(config_filename) != SUCCESS) {
      LOG_ERROR("Config parsing failed after validation - file may have changed during reload");
      LOG_ERROR("Application restart required");
      exit(EXIT_FAILURE);  // Just exit - system is in inconsistent state
   }

   /* Success - restore HUD state if possible */
   if (current_hud_name[0] != '\0') {
      hud_screen *restored_hud = find_hud_by_name(current_hud_name);
      if (restored_hud != NULL) {
         switch_to_hud(restored_hud, restored_hud->transition_type);
      } else {
         LOG_WARNING("Previous HUD '%s' not found in new config, using default", current_hud_name);
      }
   }

   // Trigger config reload notification alert
   active_alerts |= ALERT_CONFIG_RELOADED;

   return SUCCESS;
}

int check_and_reload_config(const char *config_filename) {
   struct stat file_stat;

   if (stat(config_filename, &file_stat) != 0) {
      LOG_WARNING("Cannot stat config file: %s", config_filename);
      return FAILURE;
   }

   // For initial load (config_last_modified == 0), always load
   if (config_last_modified == 0 || file_stat.st_mtime > config_last_modified) {
      if (config_last_modified == 0) {
         LOG_INFO("Loading initial config file: %s", config_filename);
      } else {
         LOG_INFO("Config file changed, reloading...");
      }

      if (reload_config(config_filename) == SUCCESS) {
         config_last_modified = file_stat.st_mtime;
         if (config_last_modified > 0) {
            LOG_INFO("Config successfully reloaded");
         }
         return SUCCESS;
      } else {
         LOG_ERROR("Config reload failed, keeping current config");
         return FAILURE;
      }
   }

   return SUCCESS;  // No change needed
}

/* Parse json animation files. */
int parse_animated_json(element *curr_element) {
   FILE *config_file = NULL;
   struct json_object *parsed_json = NULL;
   struct json_object *tmpobj, *tmpobj2, *tmpobj3, *tmpobj4;

   struct json_object_iterator it;
   struct json_object_iterator itEnd;

   char *config_string = NULL;
   int string_size = 0;
   int bytes_read = 0;

   frame *this_frame = NULL;

   char tmpstr[1024];

   config_file = fopen(curr_element->filename, "r");
   if (config_file == NULL) {
      LOG_ERROR("Unable to open config file: %s", curr_element->filename);
      return FAILURE;
   }

   string_size = 0;
   while ((bytes_read = fread(tmpstr, 1, 1024, config_file)) > 0) {
      string_size += bytes_read;
      config_string = realloc(config_string, string_size + 1);
      if (string_size <= 1024) {
         strncpy(config_string, tmpstr, bytes_read);
      } else {
         strncat(config_string, tmpstr, bytes_read);
      }
      config_string[string_size] = '\0';
   }

   fclose(config_file);

   //printf("Config String (%d): \"%s\"\n", string_size, config_string);

   parsed_json = json_tokener_parse(config_string);
   if (parsed_json != NULL) {
      /* frames section */
      json_object_object_get_ex(parsed_json, "frames", &tmpobj);

      /* Main Loop */
      it = json_object_iter_begin(tmpobj);
      itEnd = json_object_iter_end(tmpobj);

      while (!json_object_iter_equal(&it, &itEnd)) {
         //printf("%s\n", json_object_iter_peek_name(&it));
         tmpobj2 = json_object_iter_peek_value(&it);

         json_object_object_get_ex(tmpobj2, "frame", &tmpobj3);

         /* Init new frame */
         if (this_frame == NULL) {
            this_frame = malloc(sizeof(frame));
            if (curr_element->this_anim.first_frame == NULL) {
               curr_element->this_anim.first_frame = curr_element->this_anim.current_frame =
                   this_frame;
            }
         } else {
            this_frame->next = malloc(sizeof(frame));
            this_frame = this_frame->next;
         }
         this_frame->next = NULL;
         if (curr_element->this_anim.frame_count < MAX_FRAMES) {
            curr_element->this_anim.frame_lookup[curr_element->this_anim.frame_count] = this_frame;
         } else {
            LOG_WARNING("Max frame count reached: %d", MAX_FRAMES);
         }
         curr_element->this_anim.frame_count++;

         /* Frame Info */
         json_object_object_get_ex(tmpobj3, "x", &tmpobj4);
         //printf("frame x: %d\n", json_object_get_int(tmpobj4));
         this_frame->source_x = json_object_get_int(tmpobj4);

         json_object_object_get_ex(tmpobj3, "y", &tmpobj4);
         //printf("frame y: %d\n", json_object_get_int(tmpobj4));
         this_frame->source_y = json_object_get_int(tmpobj4);

         json_object_object_get_ex(tmpobj3, "w", &tmpobj4);
         //printf("frame w: %d\n", json_object_get_int(tmpobj4));
         this_frame->source_w = json_object_get_int(tmpobj4);

         json_object_object_get_ex(tmpobj3, "h", &tmpobj4);
         //printf("frame h: %d\n", json_object_get_int(tmpobj4));
         this_frame->source_h = json_object_get_int(tmpobj4);

         /* Rotated */
         json_object_object_get_ex(tmpobj2, "rotated", &tmpobj3);
         //printf("rotated: %d\n", json_object_get_boolean(tmpobj3));
         this_frame->rotated = json_object_get_boolean(tmpobj3);

         /* Trimmed */
         json_object_object_get_ex(tmpobj2, "trimmed", &tmpobj3);
         //printf("trimmed: %d\n", json_object_get_boolean(tmpobj3));
         this_frame->trimmed = json_object_get_boolean(tmpobj3);

         /* Sprite Info */
         json_object_object_get_ex(tmpobj2, "spriteSourceSize", &tmpobj3);

         json_object_object_get_ex(tmpobj3, "x", &tmpobj4);
         //printf("spriteSourceSize x: %d\n", json_object_get_int(tmpobj4));
         this_frame->dest_x = json_object_get_int(tmpobj4);

         json_object_object_get_ex(tmpobj3, "y", &tmpobj4);
         //printf("spriteSourceSize y: %d\n", json_object_get_int(tmpobj4));
         this_frame->dest_y = json_object_get_int(tmpobj4);

         /* Sprite Source Info */
         json_object_object_get_ex(tmpobj2, "sourceSize", &tmpobj3);

         json_object_object_get_ex(tmpobj3, "w", &tmpobj4);
         //printf("spriteSourceSize x: %d\n", json_object_get_int(tmpobj4));
         this_frame->source_size_w = json_object_get_int(tmpobj4);

         json_object_object_get_ex(tmpobj3, "h", &tmpobj4);
         //printf("spriteSourceSize y: %d\n", json_object_get_int(tmpobj4));
         this_frame->source_size_h = json_object_get_int(tmpobj4);

         json_object_iter_next(&it);
      }

      /* meta section */
      json_object_object_get_ex(parsed_json, "meta", &tmpobj);

      json_object_object_get_ex(tmpobj, "image", &tmpobj2);
      snprintf(curr_element->this_anim.image, MAX_FILENAME_LENGTH * 2, "%s/%s", get_image_path(),
               json_object_get_string(tmpobj2));

      json_object_object_get_ex(tmpobj, "format", &tmpobj2);
      strncpy(curr_element->this_anim.format, json_object_get_string(tmpobj2), 12);
   }

   json_object_put(parsed_json);

   free(config_string);

   return 0;
}

/* Parse a color string into its individual components. */
int parse_color(char *string,
                unsigned char *r,
                unsigned char *g,
                unsigned char *b,
                unsigned char *a) {
   char *tmpptr0 = NULL;
   char *tmpptr1 = NULL;
   int tmpi0 = 0;
   int tmpi1 = 0;
   int tmpi2 = 0;
   int tmpi3 = 0;
   int base = 10;

   tmpptr0 = strchr(string, ',');
   tmpptr0[0] = '\0';
   if (strchr(string, 'x') == NULL) {
      base = 10;
   } else {
      base = 16;
   }
   tmpi0 = strtol(string, NULL, base);
   tmpptr0++;

   tmpptr1 = strchr(tmpptr0, ',');
   tmpptr1[0] = '\0';
   if (strchr(tmpptr0, 'x') == NULL) {
      base = 10;
   } else {
      base = 16;
   }
   tmpi1 = strtol(tmpptr0, NULL, base);
   tmpptr1++;

   tmpptr0 = strchr(tmpptr1, ',');
   tmpptr0[0] = '\0';
   if (strchr(tmpptr1, 'x') == NULL) {
      base = 10;
   } else {
      base = 16;
   }
   tmpi2 = strtol(tmpptr1, NULL, base);
   tmpptr0++;

   if (strchr(tmpptr0, 'x') == NULL) {
      base = 10;
   } else {
      base = 16;
   }
   tmpi3 = strtol(tmpptr0, NULL, base);

   //printf("Parsed Color: %d, %d, %d, %d\n", tmpi0, tmpi1, tmpi2, tmpi3);
   *r = tmpi0;
   *g = tmpi1;
   *b = tmpi2;
   *a = tmpi3;

   return 0;
}

/* Add the new UI element based on which layer it's in.
 * Fancy linked list processing.
 */
void insert_element_by_layer(element *this_element) {
   element *first_element = get_first_element();
   element *curr_element = first_element;

   if (first_element == NULL) {
      set_first_element(this_element);

      return;
   }

   while ((curr_element->next != NULL) && (curr_element->layer < this_element->layer)) {
      curr_element = curr_element->next;
   }

   this_element->next = curr_element->next;

   if (curr_element->next != NULL) {
      this_element->next->prev = this_element;
   }
   curr_element->next = this_element;
   this_element->prev = curr_element;
   return;
}

/* Parse common properties shared by all element types */
static int parse_common_element_properties(struct json_object *element_obj, element *curr_element) {
   struct json_object *tmpobj = NULL;
   const char *tmpstr_ptr = NULL;

   /* Parse name if present */
   json_object_object_get_ex(element_obj, "name", &tmpobj);
   if (tmpobj != NULL) {
      strncpy(curr_element->name, json_object_get_string(tmpobj), MAX_TEXT_LENGTH - 1);
      curr_element->name[MAX_TEXT_LENGTH - 1] = '\0';
   }

   /* Parse position */
   json_object_object_get_ex(element_obj, "dest_x", &tmpobj);
   if (tmpobj != NULL) {
      curr_element->dest_x = curr_element->dst_rect.x = json_object_get_int(tmpobj);
   }

   json_object_object_get_ex(element_obj, "dest_y", &tmpobj);
   if (tmpobj != NULL) {
      curr_element->dest_y = curr_element->dst_rect.y = json_object_get_int(tmpobj);
   }

   /* Parse angle */
   json_object_object_get_ex(element_obj, "angle", &tmpobj);
   if (tmpobj != NULL) {
      if (json_object_get_type(tmpobj) == json_type_string) {
         const char *angle_str = json_object_get_string(tmpobj);
         if (strcmp(angle_str, "roll") == 0) {
            curr_element->angle = ANGLE_ROLL;
         } else if (strcmp(angle_str, "opposite roll") == 0) {
            curr_element->angle = ANGLE_OPPOSITE_ROLL;
         } else {
            LOG_WARNING("Error processing angle string: %s", angle_str);
         }
      } else {
         curr_element->angle = json_object_get_double(tmpobj);
      }
   }

   /* Parse fixed property if present */
   if (json_object_object_get_ex(element_obj, "fixed", &tmpobj)) {
      curr_element->fixed = json_object_get_int(tmpobj);
   } else {
      curr_element->fixed = FIXED_DEFAULT;
   }

   /* Parse layer */
   json_object_object_get_ex(element_obj, "layer", &tmpobj);
   if (tmpobj != NULL) {
      curr_element->layer = json_object_get_int(tmpobj);
   }

   /* Parse enabled flag if present */
   if (json_object_object_get_ex(element_obj, "enabled", &tmpobj)) {
      curr_element->enabled = json_object_get_int(tmpobj);
   }

   /* Parse hotkey if present */
   if (json_object_object_get_ex(element_obj, "hotkey", &tmpobj)) {
      tmpstr_ptr = json_object_get_string(tmpobj);
      if (tmpstr_ptr != NULL) {
         strncpy(curr_element->hotkey, tmpstr_ptr, 2);
      }
   }

   /* Parse notification group (resolved to enum for O(1) render dispatch) */
   if (json_object_object_get_ex(element_obj, "notification_group", &tmpobj)) {
      tmpstr_ptr = json_object_get_string(tmpobj);
      if (tmpstr_ptr != NULL) {
         curr_element->notification_group = (int)notif_group_resolve(tmpstr_ptr);
      }
   }

   /* Parse HUD associations */
   json_object_object_get_ex(element_obj, "huds", &tmpobj);
   if (tmpobj != NULL && json_object_get_type(tmpobj) == json_type_array) {
      /* Clear HUD flags first */
      memset(curr_element->hud_flags, 0, MAX_HUDS);

      int hud_count = json_object_array_length(tmpobj);
      for (int h = 0; h < hud_count; h++) {
         struct json_object *hud_name_obj = json_object_array_get_idx(tmpobj, h);
         const char *hud_name = json_object_get_string(hud_name_obj);

         hud_screen *screen = find_hud_by_name(hud_name);
         if (screen != NULL) {
            curr_element->hud_flags[screen->hud_id] = 1;
         } else {
            LOG_WARNING("Unknown HUD '%s' in element definition", hud_name);
         }
      }
   } else {
      /* If no HUDs specified, add to the default (first) HUD */
      hud_screen *default_hud = get_hud_manager()->screens;
      if (default_hud != NULL) {
         curr_element->hud_flags[default_hud->hud_id] = 1;
      }
   }

   return SUCCESS;
}

/* Parse the primary json config file to configure the UI. */
int parse_json_config(const char *filename) {
   FILE *config_file = NULL;
   struct json_object *parsed_json = NULL;
   struct json_object *tmpobj, *tmpobj2, *tmpobj3;
   struct json_object_iterator it;
   struct json_object_iterator itEnd;
   struct json_object_iterator itSub;
   struct json_object_iterator itSubEnd;
   int array_length = 0;
   int i = 0;
   double ratio = 0.0;

   char tmpstr[1024];

   char *config_string = NULL;
   int string_size = 0;
   int bytes_read = 0;

   element *default_element = get_default_element();
   element *intro_element = get_intro_element();
   element *curr_element = NULL;
   element *prev_element = NULL;

   hud_display_settings *this_hds = get_hud_display_settings();
   stream_settings *this_ss = get_stream_settings();
   armor_settings *this_as = get_armor_settings();

   const char *image_path = get_image_path();

   config_file = fopen(filename, "r");
   if (config_file == NULL) {
      LOG_ERROR("Unable to open config file: %s", filename);
      return FAILURE;
   }

   string_size = 0;
   while ((bytes_read = fread(tmpstr, 1, 1024, config_file)) > 0) {
      string_size += bytes_read;
      config_string = realloc(config_string, string_size + 1);
      if (string_size <= 1024) {
         strncpy(config_string, tmpstr, bytes_read);
      } else {
         strncat(config_string, tmpstr, bytes_read);
      }
      config_string[string_size] = '\0';
   }

   fclose(config_file);

   parsed_json = json_tokener_parse(config_string);
   if (parsed_json != NULL) {
      /* Main Loop */
      it = json_object_iter_begin(parsed_json);
      itEnd = json_object_iter_end(parsed_json);

      while (!json_object_iter_equal(&it, &itEnd)) {
         /* Global Section Loop */
         if (strcmp(json_object_iter_peek_name(&it), "Global") == 0) {
            tmpobj = json_object_iter_peek_value(&it);
            itSub = json_object_iter_begin(tmpobj);
            itSubEnd = json_object_iter_end(tmpobj);
            while (!json_object_iter_equal(&itSub, &itSubEnd)) {
               if (strcmp(json_object_iter_peek_name(&itSub), "Height") == 0) {
                  this_hds->eye_output_height = json_object_get_int(
                      json_object_iter_peek_value(&itSub));
                  if (this_ss->stream_height == -1) {
                     this_ss->stream_height = this_hds->eye_output_height;
                  }
               } else if (strcmp(json_object_iter_peek_name(&itSub), "Width") == 0) {
                  this_hds->eye_output_width = json_object_get_int(
                      json_object_iter_peek_value(&itSub));
                  if (this_ss->stream_width == -1) {
                     this_ss->stream_width = this_hds->eye_output_width * 2;
                  }
               } else if (strcmp(json_object_iter_peek_name(&itSub), "Camera Height") == 0) {
                  this_hds->cam_input_height = json_object_get_int(
                      json_object_iter_peek_value(&itSub));
               } else if (strcmp(json_object_iter_peek_name(&itSub), "Camera Width") == 0) {
                  this_hds->cam_input_width = json_object_get_int(
                      json_object_iter_peek_value(&itSub));
               } else if (strcmp(json_object_iter_peek_name(&itSub), "Camera FPS") == 0) {
                  this_hds->cam_input_fps = json_object_get_int(
                      json_object_iter_peek_value(&itSub));
               } else if (strcmp(json_object_iter_peek_name(&itSub), "Camera Crop X") == 0) {
                  this_hds->cam_crop_x = json_object_get_int(json_object_iter_peek_value(&itSub));
               } else if (strcmp(json_object_iter_peek_name(&itSub), "Camera Crop Width") == 0) {
                  this_hds->cam_crop_width = json_object_get_int(
                      json_object_iter_peek_value(&itSub));
               } else if (strcmp(json_object_iter_peek_name(&itSub), "Stereo Offset") == 0) {
                  this_hds->stereo_offset = json_object_get_int(
                      json_object_iter_peek_value(&itSub));
               } else if (strcmp(json_object_iter_peek_name(&itSub), "Pitch Offset") == 0) {
                  this_hds->pitch_offset = json_object_get_double(
                      json_object_iter_peek_value(&itSub));
               } else if (strcmp(json_object_iter_peek_name(&itSub), "Image Path") == 0) {
                  if (set_image_path(json_object_get_string(json_object_iter_peek_value(&itSub)),
                                     MAX_FILENAME_LENGTH - 1) == NULL) {
                     LOG_ERROR("Error setting image path!");
                  }
               } else if (strcmp(json_object_iter_peek_name(&itSub), "Font Path") == 0) {
                  if (set_font_path(json_object_get_string(json_object_iter_peek_value(&itSub)),
                                    MAX_FILENAME_LENGTH - 1) == NULL) {
                     LOG_ERROR("Error setting font path!");
                  }
               } else if (strcmp(json_object_iter_peek_name(&itSub), "Sound Path") == 0) {
                  if (set_sound_path(json_object_get_string(json_object_iter_peek_value(&itSub)),
                                     MAX_FILENAME_LENGTH - 1) == NULL) {
                     LOG_ERROR("Error setting sound path!");
                  }
               } else if (strcmp(json_object_iter_peek_name(&itSub), "Wifi") == 0) {
                  if (set_wifi_dev_name(json_object_get_string(json_object_iter_peek_value(&itSub)),
                                        MAX_WIFI_DEV_LENGTH - 1) == NULL) {
                     LOG_ERROR("Error settings Wifi device name!");
                  }
               } else if (strcmp(json_object_iter_peek_name(&itSub), "Invert Compass") == 0) {
                  set_inv_compass(json_object_get_boolean(json_object_iter_peek_value(&itSub)));
               } else if (strcmp(json_object_iter_peek_name(&itSub), "Stream Width") == 0) {
                  this_ss->stream_width = json_object_get_int(json_object_iter_peek_value(&itSub));
               } else if (strcmp(json_object_iter_peek_name(&itSub), "Stream Height") == 0) {
                  this_ss->stream_height = json_object_get_int(json_object_iter_peek_value(&itSub));
               } else if (strcmp(json_object_iter_peek_name(&itSub), "Stream Dest IP") == 0) {
                  strncpy(this_ss->stream_dest_ip,
                          json_object_get_string(json_object_iter_peek_value(&itSub)), 16);
               } else if (strcmp(json_object_iter_peek_name(&itSub), "Snapshot Overlay") == 0) {
                  this_hds->snapshot_overlay = json_object_get_boolean(
                      json_object_iter_peek_value(&itSub));
               } else if (strcmp(json_object_iter_peek_name(&itSub), "Vision Inline Data") == 0) {
                  this_hds->vision_inline_data = json_object_get_boolean(
                      json_object_iter_peek_value(&itSub));
               } else {
                  printf("%s\n", json_object_iter_peek_name(&itSub));
               }
               json_object_iter_next(&itSub);
            }
         }

         /* Parse MQTT section */
         if (strcmp(json_object_iter_peek_name(&it), "mqtt") == 0) {
            struct json_object *mqtt_obj = json_object_iter_peek_value(&it);
            struct json_object *mqtt_val = NULL;

            if (json_object_object_get_ex(mqtt_obj, "host", &mqtt_val) && mqtt_val != NULL) {
               set_mqtt_host(json_object_get_string(mqtt_val));
            }
            if (json_object_object_get_ex(mqtt_obj, "port", &mqtt_val) && mqtt_val != NULL) {
               set_mqtt_port(json_object_get_int(mqtt_val));
            }
            if (json_object_object_get_ex(mqtt_obj, "tls", &mqtt_val) && mqtt_val != NULL) {
               set_mqtt_tls(json_object_get_boolean(mqtt_val));
            }
            if (json_object_object_get_ex(mqtt_obj, "tls_ca_cert", &mqtt_val) && mqtt_val != NULL) {
               set_mqtt_tls_ca_cert(json_object_get_string(mqtt_val));
            }
            if (json_object_object_get_ex(mqtt_obj, "tls_cert_path", &mqtt_val) &&
                mqtt_val != NULL) {
               set_mqtt_tls_cert_path(json_object_get_string(mqtt_val));
            }
            if (json_object_object_get_ex(mqtt_obj, "tls_key_path", &mqtt_val) &&
                mqtt_val != NULL) {
               set_mqtt_tls_key_path(json_object_get_string(mqtt_val));
            }
         }

         /* Parse HUDs section */
         if (strcmp(json_object_iter_peek_name(&it), "HUDs") == 0) {
            tmpobj = json_object_iter_peek_value(&it);
            if (json_object_get_type(tmpobj) == json_type_array) {
               array_length = json_object_array_length(tmpobj);
               for (i = 0; i < array_length; i++) {
                  tmpobj2 = json_object_array_get_idx(tmpobj, i);

                  /* Get HUD name */
                  json_object_object_get_ex(tmpobj2, "name", &tmpobj3);
                  if (tmpobj3 == NULL) {
                     LOG_ERROR("HUD definition missing name");
                     continue;
                  }
                  const char *hud_name = json_object_get_string(tmpobj3);

                  /* Get hotkey if present */
                  const char *hotkey = NULL;
                  json_object_object_get_ex(tmpobj2, "hotkey", &tmpobj3);
                  if (tmpobj3 != NULL) {
                     hotkey = json_object_get_string(tmpobj3);
                  }

                  /* Get transition if present */
                  const char *transition = NULL;
                  json_object_object_get_ex(tmpobj2, "transition", &tmpobj3);
                  if (tmpobj3 != NULL) {
                     transition = json_object_get_string(tmpobj3);
                  }

                  /* Register the HUD */
                  register_hud(hud_name, hotkey, transition);
               }
            }
         }

         /* Elements Section Loop */
         if (strcmp(json_object_iter_peek_name(&it), "Elements") == 0) {
            tmpobj = json_object_iter_peek_value(&it);
            if (json_object_get_type(tmpobj) == json_type_array) {
               array_length = json_object_array_length(tmpobj);
               for (i = 0; i < array_length; i++) {
                  tmpobj2 = json_object_array_get_idx(tmpobj, i);
                  json_object_object_get_ex(tmpobj2, "type", &tmpobj3);

                  /* Special case for intro type. */
                  if ((tmpobj3 != NULL) &&
                      (strcmp("intro", json_object_get_string(tmpobj3)) == 0)) {
                     memcpy(intro_element, default_element, sizeof(element));
                     intro_element->enabled = 1;

                     json_object_object_get_ex(tmpobj2, "file", &tmpobj3);
                     snprintf(intro_element->filename, MAX_FILENAME_LENGTH * 2, "%s/%s", image_path,
                              json_object_get_string(tmpobj3));

                     json_object_object_get_ex(tmpobj2, "dest_x", &tmpobj3);
                     intro_element->dest_x = json_object_get_int(tmpobj3);

                     json_object_object_get_ex(tmpobj2, "dest_y", &tmpobj3);
                     intro_element->dest_y = json_object_get_int(tmpobj3);

                     json_object_object_get_ex(tmpobj2, "angle", &tmpobj3);
                     if (strcmp(json_object_get_string(tmpobj3), "roll") == 0) {
                        intro_element->angle = ANGLE_ROLL;
                     } else if (strcmp(json_object_get_string(tmpobj3), "opposite roll") == 0) {
                        intro_element->angle = ANGLE_OPPOSITE_ROLL;
                     } else {
                        intro_element->angle = json_object_get_double(tmpobj3);
                     }

                     parse_animated_json(intro_element);
                  } else if (tmpobj3 != NULL) {
                     const char *element_type = json_object_get_string(tmpobj3);

                     curr_element = malloc(sizeof(element));
                     if (curr_element == NULL) {
                        LOG_ERROR("Cannot malloc new element!");
                        exit(1);
                     }
                     memcpy(curr_element, default_element, sizeof(element));
                     curr_element->enabled = 1;

                     /* STATIC */
                     if (strcmp("static", element_type) == 0) {
                        curr_element->type = STATIC;

                        /* Parse common properties */
                        parse_common_element_properties(tmpobj2, curr_element);

                        /* Parse static-specific properties */
                        json_object_object_get_ex(tmpobj2, "file", &tmpobj3);
                        snprintf(curr_element->filename, MAX_FILENAME_LENGTH * 2, "%s/%s",
                                 image_path, json_object_get_string(tmpobj3));

                        json_object_object_get_ex(tmpobj2, "width", &tmpobj3);
                        if (tmpobj3 != NULL) {
                           curr_element->width = json_object_get_int(tmpobj3);
                        }

                        json_object_object_get_ex(tmpobj2, "height", &tmpobj3);
                        if (tmpobj3 != NULL) {
                           curr_element->height = json_object_get_int(tmpobj3);
                        }

                        /* Load texture */
                        curr_element->texture = get_cached_texture(curr_element->filename);
                        if (!curr_element->texture) {
                           SDL_Log("Couldn't load %s: %s\n", curr_element->filename,
                                   SDL_GetError());
                           json_object_put(parsed_json);
                           free(config_string);
                           return FAILURE;
                        }

                        /* Set up destination rectangle */
                        if ((curr_element->width == 0) && (curr_element->height == 0)) {
                           SDL_QueryTexture(curr_element->texture, NULL, NULL,
                                            &curr_element->dst_rect.w, &curr_element->dst_rect.h);
                        } else if ((curr_element->width == 0) && (curr_element->height != 0)) {
                           SDL_QueryTexture(curr_element->texture, NULL, NULL,
                                            &curr_element->dst_rect.w, &curr_element->dst_rect.h);
                           ratio = (double)curr_element->height / (double)curr_element->dst_rect.h;
                           curr_element->dst_rect.w = curr_element->width =
                               curr_element->dst_rect.w * ratio;
                        } else if ((curr_element->width != 0) && (curr_element->height == 0)) {
                           SDL_QueryTexture(curr_element->texture, NULL, NULL,
                                            &curr_element->dst_rect.w, &curr_element->dst_rect.h);
                           ratio = (double)curr_element->width / (double)curr_element->dst_rect.w;
                           curr_element->dst_rect.h = curr_element->height =
                               curr_element->dst_rect.h * ratio;
                        } else {
                           curr_element->dst_rect.w = curr_element->width;
                           curr_element->dst_rect.h = curr_element->height;
                        }
                        /* Record Graphic */
                     } else if (strcmp("record-ui", element_type) == 0) {
                        curr_element->type = STATIC;

                        /* Parse common properties */
                        parse_common_element_properties(tmpobj2, curr_element);

                        /* Parse record-ui specific properties */
                        json_object_object_get_ex(tmpobj2, "file", &tmpobj3);
                        snprintf(curr_element->filename, MAX_FILENAME_LENGTH * 2, "%s/%s",
                                 image_path, json_object_get_string(tmpobj3));

                        json_object_object_get_ex(tmpobj2, "file_r", &tmpobj3);
                        snprintf(curr_element->filename_r, MAX_FILENAME_LENGTH * 2, "%s/%s",
                                 image_path, json_object_get_string(tmpobj3));

                        json_object_object_get_ex(tmpobj2, "file_s", &tmpobj3);
                        snprintf(curr_element->filename_s, MAX_FILENAME_LENGTH * 2, "%s/%s",
                                 image_path, json_object_get_string(tmpobj3));

                        json_object_object_get_ex(tmpobj2, "file_rs", &tmpobj3);
                        snprintf(curr_element->filename_rs, MAX_FILENAME_LENGTH * 2, "%s/%s",
                                 image_path, json_object_get_string(tmpobj3));

                        /* Load textures */
                        curr_element->texture = get_cached_texture(curr_element->filename);
                        if (!curr_element->texture) {
                           SDL_Log("Couldn't load %s: %s\n", curr_element->filename,
                                   SDL_GetError());
                           json_object_put(parsed_json);
                           free(config_string);
                           return FAILURE;
                        }

                        curr_element->texture_r = get_cached_texture(curr_element->filename_r);
                        if (!curr_element->texture_r) {
                           SDL_Log("Couldn't load %s: %s\n", curr_element->filename_r,
                                   SDL_GetError());
                           json_object_put(parsed_json);
                           free(config_string);
                           return FAILURE;
                        }

                        curr_element->texture_s = get_cached_texture(curr_element->filename_s);
                        if (!curr_element->texture_s) {
                           SDL_Log("Couldn't load %s: %s\n", curr_element->filename_s,
                                   SDL_GetError());
                           json_object_put(parsed_json);
                           free(config_string);
                           return FAILURE;
                        }

                        curr_element->texture_rs = get_cached_texture(curr_element->filename_rs);
                        if (!curr_element->texture_rs) {
                           SDL_Log("Couldn't load %s: %s\n", curr_element->filename_rs,
                                   SDL_GetError());
                           json_object_put(parsed_json);
                           free(config_string);
                           return FAILURE;
                        }

                        SDL_QueryTexture(curr_element->texture, NULL, NULL,
                                         &curr_element->dst_rect.w, &curr_element->dst_rect.h);
                        /* AI Status Graphic */
                     } else if (strcmp("ai-ui", element_type) == 0) {
                        curr_element->type = STATIC;

                        /* Parse common properties */
                        parse_common_element_properties(tmpobj2, curr_element);

                        /* Parse ai-ui specific properties */
                        json_object_object_get_ex(tmpobj2, "file", &tmpobj3);
                        snprintf(curr_element->filename, MAX_FILENAME_LENGTH * 2, "%s/%s",
                                 image_path, json_object_get_string(tmpobj3));

                        /* AI Listening */
                        json_object_object_get_ex(tmpobj2, "file_l", &tmpobj3);
                        snprintf(curr_element->filename_l, MAX_FILENAME_LENGTH * 2, "%s/%s",
                                 image_path, json_object_get_string(tmpobj3));

                        /* AI Heard Wakeword */
                        json_object_object_get_ex(tmpobj2, "file_w", &tmpobj3);
                        snprintf(curr_element->filename_w, MAX_FILENAME_LENGTH * 2, "%s/%s",
                                 image_path, json_object_get_string(tmpobj3));

                        /* AI Processing */
                        json_object_object_get_ex(tmpobj2, "file_p", &tmpobj3);
                        snprintf(curr_element->filename_p, MAX_FILENAME_LENGTH * 2, "%s/%s",
                                 image_path, json_object_get_string(tmpobj3));

                        /* Load textures */
                        curr_element->texture = get_cached_texture(curr_element->filename);
                        if (!curr_element->texture) {
                           SDL_Log("Couldn't load %s: %s\n", curr_element->filename,
                                   SDL_GetError());
                           json_object_put(parsed_json);
                           free(config_string);
                           return FAILURE;
                        }

                        curr_element->texture_l = get_cached_texture(curr_element->filename_l);
                        if (!curr_element->texture_l) {
                           SDL_Log("Couldn't load %s: %s\n", curr_element->filename_l,
                                   SDL_GetError());
                           json_object_put(parsed_json);
                           free(config_string);
                           return FAILURE;
                        }

                        curr_element->texture_w = get_cached_texture(curr_element->filename_w);
                        if (!curr_element->texture_w) {
                           SDL_Log("Couldn't load %s: %s\n", curr_element->filename_w,
                                   SDL_GetError());
                           json_object_put(parsed_json);
                           free(config_string);
                           return FAILURE;
                        }

                        curr_element->texture_p = get_cached_texture(curr_element->filename_p);
                        if (!curr_element->texture_p) {
                           SDL_Log("Couldn't load %s: %s\n", curr_element->filename_p,
                                   SDL_GetError());
                           json_object_put(parsed_json);
                           free(config_string);
                           return FAILURE;
                        }

                        SDL_QueryTexture(curr_element->texture, NULL, NULL,
                                         &curr_element->dst_rect.w, &curr_element->dst_rect.h);
                        /* ANIMATED */
                     } else if (strcmp("animated", element_type) == 0) {
                        curr_element->type = ANIMATED;

                        /* Parse common properties */
                        parse_common_element_properties(tmpobj2, curr_element);

                        /* Parse animated-specific properties */
                        json_object_object_get_ex(tmpobj2, "file", &tmpobj3);
                        snprintf(curr_element->filename, MAX_FILENAME_LENGTH * 2, "%s/%s",
                                 image_path, json_object_get_string(tmpobj3));

                        json_object_object_get_ex(tmpobj2, "width", &tmpobj3);
                        if (tmpobj3 != NULL) {
                           curr_element->width = json_object_get_int(tmpobj3);
                        }

                        json_object_object_get_ex(tmpobj2, "height", &tmpobj3);
                        if (tmpobj3 != NULL) {
                           curr_element->height = json_object_get_int(tmpobj3);
                        }

                        /* Parse animation data */
                        parse_animated_json(curr_element);

                        /* Load texture */
                        curr_element->texture = get_cached_texture(curr_element->this_anim.image);
                        if (!curr_element->texture) {
                           SDL_Log("Couldn't load %s: %s\n", curr_element->this_anim.image,
                                   SDL_GetError());
                           json_object_put(parsed_json);
                           free(config_string);
                           return FAILURE;
                        }
                        /* TEXT */
                     } else if (strcmp("text", element_type) == 0) {
                        curr_element->type = TEXT;

                        /* Parse common properties */
                        parse_common_element_properties(tmpobj2, curr_element);

                        /* Parse text-specific properties */
                        json_object_object_get_ex(tmpobj2, "string", &tmpobj3);
                        if (tmpobj3 != NULL) {
                           strncpy(curr_element->text, json_object_get_string(tmpobj3),
                                   MAX_TEXT_LENGTH);
                           curr_element->text_source_id = resolve_text_source(curr_element->text);
                        }

                        json_object_object_get_ex(tmpobj2, "font", &tmpobj3);
                        if (tmpobj3 != NULL) {
                           snprintf(curr_element->font, MAX_FILENAME_LENGTH * 2, "%s/%s",
                                    get_font_path(), json_object_get_string(tmpobj3));
                        }

                        json_object_object_get_ex(tmpobj2, "color", &tmpobj3);
                        if (tmpobj3 != NULL) {
                           strncpy(tmpstr, json_object_get_string(tmpobj3), 1024);
                           parse_color(tmpstr, &curr_element->font_color.r,
                                       &curr_element->font_color.g, &curr_element->font_color.b,
                                       &curr_element->font_color.a);
                        }

                        json_object_object_get_ex(tmpobj2, "size", &tmpobj3);
                        if (tmpobj3 != NULL) {
                           curr_element->font_size = json_object_get_int(tmpobj3);
                        }

                        if ((strcmp(curr_element->font, "") != 0) &&
                            (curr_element->font_size > 0)) {
                           curr_element->ttf_font = get_local_font(curr_element->font,
                                                                   curr_element->font_size);
                        }

                        json_object_object_get_ex(tmpobj2, "halign", &tmpobj3);
                        if (tmpobj3 != NULL) {
                           strncpy(curr_element->halign, json_object_get_string(tmpobj3), 7);
                        }

                        /* These are currently just for the *LOG* text. */
                        // Parse width if present
                        json_object_object_get_ex(tmpobj2, "width", &tmpobj3);
                        if (tmpobj3 != NULL) {
                           curr_element->width = json_object_get_int(tmpobj3);
                        }

                        // Parse height if present
                        json_object_object_get_ex(tmpobj2, "height", &tmpobj3);
                        if (tmpobj3 != NULL) {
                           curr_element->height = json_object_get_int(tmpobj3);
                        }
                        /* SPECIAL */
                     } else if (strcmp("special", element_type) == 0) {
                        curr_element->type = SPECIAL;

                        /* Parse common properties */
                        parse_common_element_properties(tmpobj2, curr_element);

                        /* Parse special-specific properties */
                        json_object_object_get_ex(tmpobj2, "name", &tmpobj3);
                        if (tmpobj3 != NULL) {
                           strncpy(curr_element->name, json_object_get_string(tmpobj3),
                                   MAX_TEXT_LENGTH - 1);
                           curr_element->name[MAX_TEXT_LENGTH - 1] = '\0';

                           /* Default special_name to same as name for backward compatibility */
                           strncpy(curr_element->special_name, json_object_get_string(tmpobj3),
                                   MAX_TEXT_LENGTH - 1);
                           curr_element->special_name[MAX_TEXT_LENGTH - 1] = '\0';
                        }

                        /* ADD THIS: Check for explicit special_name field */
                        json_object_object_get_ex(tmpobj2, "special_name", &tmpobj3);
                        if (tmpobj3 != NULL) {
                           strncpy(curr_element->special_name, json_object_get_string(tmpobj3),
                                   MAX_TEXT_LENGTH - 1);
                           curr_element->special_name[MAX_TEXT_LENGTH - 1] = '\0';
                        }

                        json_object_object_get_ex(tmpobj2, "file", &tmpobj3);
                        if (tmpobj3 != NULL) {
                           snprintf(curr_element->filename, MAX_FILENAME_LENGTH * 2, "%s/%s",
                                    image_path, json_object_get_string(tmpobj3));
                        }

                        json_object_object_get_ex(tmpobj2, "width", &tmpobj3);
                        if (tmpobj3 != NULL) {
                           curr_element->width = json_object_get_int(tmpobj3);
                        }

                        json_object_object_get_ex(tmpobj2, "height", &tmpobj3);
                        if (tmpobj3 != NULL) {
                           curr_element->height = json_object_get_int(tmpobj3);
                        }

                        json_object_object_get_ex(tmpobj2, "download_count", &tmpobj3);
                        if (tmpobj3 != NULL) {
                           curr_element->download_count = json_object_get_int(tmpobj3);
                        }

                        /* Font properties for special elements that need text rendering */
                        json_object_object_get_ex(tmpobj2, "font", &tmpobj3);
                        if (tmpobj3 != NULL) {
                           snprintf(curr_element->font, MAX_FILENAME_LENGTH * 2, "%s/%s",
                                    get_font_path(), json_object_get_string(tmpobj3));
                        }

                        json_object_object_get_ex(tmpobj2, "color", &tmpobj3);
                        if (tmpobj3 != NULL) {
                           strncpy(tmpstr, json_object_get_string(tmpobj3), 1024);
                           parse_color(tmpstr, &curr_element->font_color.r,
                                       &curr_element->font_color.g, &curr_element->font_color.b,
                                       &curr_element->font_color.a);
                        }

                        json_object_object_get_ex(tmpobj2, "size", &tmpobj3);
                        if (tmpobj3 != NULL) {
                           curr_element->font_size = json_object_get_int(tmpobj3);
                        }

                        if ((strcmp(curr_element->font, "") != 0) &&
                            (curr_element->font_size > 0)) {
                           curr_element->ttf_font = get_local_font(curr_element->font,
                                                                   curr_element->font_size);
                        }

                        /* Special offset properties */
                        json_object_object_get_ex(tmpobj2, "center_x_offset", &tmpobj3);
                        if (tmpobj3 != NULL) {
                           curr_element->center_x_offset = json_object_get_int(tmpobj3);
                        }

                        json_object_object_get_ex(tmpobj2, "center_y_offset", &tmpobj3);
                        if (tmpobj3 != NULL) {
                           curr_element->center_y_offset = json_object_get_int(tmpobj3);
                        }

                        json_object_object_get_ex(tmpobj2, "text_x_offset", &tmpobj3);
                        if (tmpobj3 != NULL) {
                           curr_element->text_x_offset = json_object_get_int(tmpobj3);
                        }

                        json_object_object_get_ex(tmpobj2, "text_y_offset", &tmpobj3);
                        if (tmpobj3 != NULL) {
                           curr_element->text_y_offset = json_object_get_int(tmpobj3);
                        }

                        /* Handle animations for special elements like heading, pitch, etc. */
                        if ((strcmp("heading", curr_element->special_name) == 0) ||
                            (strcmp("pitch", curr_element->special_name) == 0) ||
                            (strcmp("altitude", curr_element->special_name) == 0) ||
                            (strcmp("wifi", curr_element->special_name) == 0) ||
                            (strcmp("detect", curr_element->special_name) == 0)) {
                           parse_animated_json(curr_element);

                           if (strcmp("detect", curr_element->special_name) != 0) {
                              curr_element->texture = get_cached_texture(
                                  curr_element->this_anim.image);
                              if (!curr_element->texture) {
                                 SDL_Log("Couldn't load %s: %s\n", curr_element->filename,
                                         SDL_GetError());
                                 json_object_put(parsed_json);
                                 free(config_string);
                                 return FAILURE;
                              }
                           }
                        }

                        /* Map elements, parse map-specific properties */
                        if (strcmp("map", curr_element->special_name) == 0) {
                           // Parse maptype
                           json_object_object_get_ex(tmpobj2, "maptype", &tmpobj3);
                           if (tmpobj3 != NULL) {
                              const char *maptype_str = json_object_get_string(tmpobj3);

                              // Convert string to enum
                              for (int i = 0; i < MAP_TYPE_COUNT; i++) {
                                 if (strcmp(maptype_str, MAP_TYPE_STRINGS[i]) == 0) {
                                    curr_element->map_type = (map_type_t)i;
                                    break;
                                 }
                              }
                           }

                           // Parse zoom_level
                           json_object_object_get_ex(tmpobj2, "zoom", &tmpobj3);
                           if (tmpobj3 != NULL) {
                              curr_element->map_zoom = json_object_get_int(tmpobj3);
                           }

                           // Parse update_interval
                           json_object_object_get_ex(tmpobj2, "update_interval", &tmpobj3);
                           if (tmpobj3 != NULL) {
                              curr_element->update_interval_sec = json_object_get_int(tmpobj3);
                           }

                           /* If we're parsing a config file, we should immediately refesh. */
                           curr_element->force_refresh = 1;
                        }

                        /* Battery display elements */
                        if (strcmp("battery", curr_element->special_name) == 0) {
                           /* We're reusing filenames and textures here. No reason for new ones. */
                           json_object_object_get_ex(tmpobj2, "file_100", &tmpobj3);
                           snprintf(curr_element->filename, MAX_FILENAME_LENGTH * 2, "%s/%s",
                                    image_path, json_object_get_string(tmpobj3));

                           json_object_object_get_ex(tmpobj2, "file_75", &tmpobj3);
                           snprintf(curr_element->filename_base, MAX_FILENAME_LENGTH * 2, "%s/%s",
                                    image_path, json_object_get_string(tmpobj3));

                           json_object_object_get_ex(tmpobj2, "file_50", &tmpobj3);
                           snprintf(curr_element->filename_online, MAX_FILENAME_LENGTH * 2, "%s/%s",
                                    image_path, json_object_get_string(tmpobj3));

                           json_object_object_get_ex(tmpobj2, "file_25", &tmpobj3);
                           snprintf(curr_element->filename_warning, MAX_FILENAME_LENGTH * 2,
                                    "%s/%s", image_path, json_object_get_string(tmpobj3));

                           json_object_object_get_ex(tmpobj2, "file_0", &tmpobj3);
                           snprintf(curr_element->filename_offline, MAX_FILENAME_LENGTH * 2,
                                    "%s/%s", image_path, json_object_get_string(tmpobj3));

                           /* Load textures */
                           curr_element->texture = get_cached_texture(curr_element->filename);
                           if (!curr_element->texture) {
                              SDL_Log("Couldn't load %s: %s\n", curr_element->filename,
                                      SDL_GetError());
                              json_object_put(parsed_json);
                              free(config_string);
                              return FAILURE;
                           }

                           curr_element->texture_base = get_cached_texture(
                               curr_element->filename_base);
                           if (!curr_element->texture_base) {
                              SDL_Log("Couldn't load %s: %s\n", curr_element->filename_base,
                                      SDL_GetError());
                              json_object_put(parsed_json);
                              free(config_string);
                              return FAILURE;
                           }

                           curr_element->texture_online = get_cached_texture(
                               curr_element->filename_online);
                           if (!curr_element->texture_online) {
                              SDL_Log("Couldn't load %s: %s\n", curr_element->filename_online,
                                      SDL_GetError());
                              json_object_put(parsed_json);
                              free(config_string);
                              return FAILURE;
                           }

                           curr_element->texture_warning = get_cached_texture(
                               curr_element->filename_warning);
                           if (!curr_element->texture_warning) {
                              SDL_Log("Couldn't load %s: %s\n", curr_element->filename_warning,
                                      SDL_GetError());
                              json_object_put(parsed_json);
                              free(config_string);
                              return FAILURE;
                           }

                           curr_element->texture_offline = get_cached_texture(
                               curr_element->filename_offline);
                           if (!curr_element->texture_offline) {
                              SDL_Log("Couldn't load %s: %s\n", curr_element->filename_offline,
                                      SDL_GetError());
                              json_object_put(parsed_json);
                              free(config_string);
                              return FAILURE;
                           }

                           SDL_QueryTexture(curr_element->texture, NULL, NULL,
                                            &curr_element->dst_rect.w, &curr_element->dst_rect.h);
                        }

                        /* Gauge elements - parse gauge-specific properties */
                        if (strcmp("gauge", curr_element->special_name) == 0) {
                           /* Parse gauge type */
                           json_object_object_get_ex(tmpobj2, "gauge_type", &tmpobj3);
                           if (tmpobj3 != NULL) {
                              strncpy(curr_element->gauge_type, json_object_get_string(tmpobj3),
                                      MAX_TEXT_LENGTH - 1);
                              curr_element->gauge_type[MAX_TEXT_LENGTH - 1] = '\0';
                           }

                           /* Parse min/max values */
                           json_object_object_get_ex(tmpobj2, "gauge_min", &tmpobj3);
                           if (tmpobj3 != NULL) {
                              curr_element->gauge_min_value = (float)json_object_get_double(
                                  tmpobj3);
                           }

                           json_object_object_get_ex(tmpobj2, "gauge_max", &tmpobj3);
                           if (tmpobj3 != NULL) {
                              curr_element->gauge_max_value = (float)json_object_get_double(
                                  tmpobj3);
                           }

                           /* Parse value source */
                           json_object_object_get_ex(tmpobj2, "gauge_value", &tmpobj3);
                           if (tmpobj3 != NULL) {
                              strncpy(curr_element->gauge_value_source,
                                      json_object_get_string(tmpobj3), MAX_TEXT_LENGTH - 1);
                              curr_element->gauge_value_source[MAX_TEXT_LENGTH - 1] = '\0';
                           }

                           /* Parse warning threshold */
                           json_object_object_get_ex(tmpobj2, "gauge_warning", &tmpobj3);
                           if (tmpobj3 != NULL) {
                              curr_element->gauge_warning_threshold = (float)json_object_get_double(
                                  tmpobj3);
                           }

                           /* Parse primary color */
                           json_object_object_get_ex(tmpobj2, "gauge_color", &tmpobj3);
                           if (tmpobj3 != NULL) {
                              strncpy(tmpstr, json_object_get_string(tmpobj3), 1024);
                              parse_color(tmpstr, &curr_element->gauge_primary_color.r,
                                          &curr_element->gauge_primary_color.g,
                                          &curr_element->gauge_primary_color.b,
                                          &curr_element->gauge_primary_color.a);
                           }

                           /* Parse warning color */
                           json_object_object_get_ex(tmpobj2, "gauge_warning_color", &tmpobj3);
                           if (tmpobj3 != NULL) {
                              strncpy(tmpstr, json_object_get_string(tmpobj3), 1024);
                              parse_color(tmpstr, &curr_element->gauge_warning_color.r,
                                          &curr_element->gauge_warning_color.g,
                                          &curr_element->gauge_warning_color.b,
                                          &curr_element->gauge_warning_color.a);
                           }

                           /* Parse orientation (linear gauges only) */
                           json_object_object_get_ex(tmpobj2, "gauge_orientation", &tmpobj3);
                           if (tmpobj3 != NULL) {
                              curr_element->gauge_orientation = json_object_get_int(tmpobj3);
                           }

                           /* Parse arc start angle (arc/ring gauges) */
                           json_object_object_get_ex(tmpobj2, "gauge_arc_start", &tmpobj3);
                           if (tmpobj3 != NULL) {
                              curr_element->gauge_arc_start = (float)json_object_get_double(
                                  tmpobj3);
                           }

                           /* Parse arc sweep angle (arc/ring gauges) */
                           json_object_object_get_ex(tmpobj2, "gauge_arc_sweep", &tmpobj3);
                           if (tmpobj3 != NULL) {
                              curr_element->gauge_arc_sweep = (float)json_object_get_double(
                                  tmpobj3);
                           }

                           /* Parse thickness (ring gauges) */
                           json_object_object_get_ex(tmpobj2, "gauge_thickness", &tmpobj3);
                           if (tmpobj3 != NULL) {
                              curr_element->gauge_thickness = json_object_get_int(tmpobj3);
                           }

                           /* Parse tick marks (arc gauges) */
                           json_object_object_get_ex(tmpobj2, "gauge_ticks", &tmpobj3);
                           if (tmpobj3 != NULL) {
                              curr_element->gauge_ticks = json_object_get_int(tmpobj3);
                           }

                           /* Parse smooth interpolation flag and factor */
                           json_object_object_get_ex(tmpobj2, "gauge_smooth", &tmpobj3);
                           if (tmpobj3 != NULL) {
                              curr_element->gauge_smooth = json_object_get_int(tmpobj3);
                           }
                           curr_element->gauge_smooth_factor = 0.2f; /* Default */
                           json_object_object_get_ex(tmpobj2, "gauge_smooth_factor", &tmpobj3);
                           if (tmpobj3 != NULL) {
                              curr_element->gauge_smooth_factor = (float)json_object_get_double(
                                  tmpobj3);
                           }

                           /* Parse glow effect flag */
                           json_object_object_get_ex(tmpobj2, "gauge_glow", &tmpobj3);
                           if (tmpobj3 != NULL) {
                              curr_element->gauge_glow = json_object_get_int(tmpobj3);
                           }

                           /* Parse value label display */
                           json_object_object_get_ex(tmpobj2, "gauge_show_value", &tmpobj3);
                           if (tmpobj3 != NULL) {
                              curr_element->gauge_show_value = json_object_get_int(tmpobj3);
                           }

                           /* Parse value label format */
                           json_object_object_get_ex(tmpobj2, "gauge_value_format", &tmpobj3);
                           if (tmpobj3 != NULL) {
                              strncpy(curr_element->gauge_value_format,
                                      json_object_get_string(tmpobj3),
                                      sizeof(curr_element->gauge_value_format) - 1);
                              curr_element
                                  ->gauge_value_format[sizeof(curr_element->gauge_value_format) -
                                                       1] = '\0';
                           }

                           /* Parse value label color */
                           json_object_object_get_ex(tmpobj2, "gauge_value_color", &tmpobj3);
                           if (tmpobj3 != NULL) {
                              strncpy(tmpstr, json_object_get_string(tmpobj3), 1024);
                              parse_color(tmpstr, &curr_element->gauge_value_color.r,
                                          &curr_element->gauge_value_color.g,
                                          &curr_element->gauge_value_color.b,
                                          &curr_element->gauge_value_color.a);
                           }

                           /* Parse value label size */
                           json_object_object_get_ex(tmpobj2, "gauge_value_size", &tmpobj3);
                           if (tmpobj3 != NULL) {
                              curr_element->gauge_value_size = json_object_get_int(tmpobj3);
                           }
                        }

                        /* Check if this is an armor display element */
                        if (strcmp("armor_display", curr_element->special_name) == 0) {
                           json_object_object_get_ex(tmpobj2, "notice_x", &tmpobj3);
                           if (tmpobj3 != NULL) {
                              curr_element->notice_x = json_object_get_int(tmpobj3);
                           }

                           json_object_object_get_ex(tmpobj2, "notice_y", &tmpobj3);
                           if (tmpobj3 != NULL) {
                              curr_element->notice_y = json_object_get_int(tmpobj3);
                           }

                           json_object_object_get_ex(tmpobj2, "notice_width", &tmpobj3);
                           if (tmpobj3 != NULL) {
                              curr_element->notice_width = json_object_get_int(tmpobj3);
                           }

                           json_object_object_get_ex(tmpobj2, "notice_height", &tmpobj3);
                           if (tmpobj3 != NULL) {
                              curr_element->notice_height = json_object_get_int(tmpobj3);
                           }

                           json_object_object_get_ex(tmpobj2, "notice_timeout", &tmpobj3);
                           if (tmpobj3 != NULL) {
                              curr_element->notice_timeout = json_object_get_int(tmpobj3);
                           }

                           json_object_object_get_ex(tmpobj2, "show_metrics", &tmpobj3);
                           if (tmpobj3 != NULL) {
                              curr_element->show_metrics = json_object_get_boolean(tmpobj3);
                           }

                           json_object_object_get_ex(tmpobj2, "metrics_font", &tmpobj3);
                           if (tmpobj3 != NULL) {
                              snprintf(curr_element->metrics_font, MAX_FILENAME_LENGTH * 2, "%s/%s",
                                       get_font_path(), json_object_get_string(tmpobj3));
                           }

                           json_object_object_get_ex(tmpobj2, "metrics_font_size", &tmpobj3);
                           if (tmpobj3 != NULL) {
                              curr_element->metrics_font_size = json_object_get_int(tmpobj3);
                           }
                        }
                     }

                     /* Add the element to the element list */
                     insert_element_by_layer(curr_element);
                  }
               }
            }
         }

         /* Components Section Loop */
         if (strcmp(json_object_iter_peek_name(&it), "Components") == 0) {
            tmpobj = json_object_iter_peek_value(&it);
            if (json_object_get_type(tmpobj) == json_type_array) {
               array_length = json_object_array_length(tmpobj);
               for (i = 0; i < array_length; i++) {
                  curr_element = malloc(sizeof(element));
                  if (curr_element == NULL) {
                     LOG_ERROR("Cannot malloc new element!");
                     exit(1);
                  }
                  memcpy(curr_element, default_element, sizeof(element));
                  curr_element->type = ARMOR_COMPONENT, curr_element->enabled = 1;
                  curr_element->mqtt_registered = 0;

                  if (this_as->armor_elements == NULL) {
                     this_as->armor_elements = curr_element;
                  } else {
                     prev_element->next = curr_element;
                     curr_element->prev = prev_element;
                  }

                  tmpobj2 = json_object_array_get_idx(tmpobj, i);

                  json_object_object_get_ex(tmpobj2, "name", &tmpobj3);
                  strncpy(curr_element->name, json_object_get_string(tmpobj3), MAX_TEXT_LENGTH);

                  json_object_object_get_ex(tmpobj2, "device", &tmpobj3);
                  strncpy(curr_element->mqtt_device, json_object_get_string(tmpobj3),
                          MAX_TEXT_LENGTH);


                  json_object_object_get_ex(tmpobj2, "base file", &tmpobj3);
                  snprintf(curr_element->filename_base, MAX_FILENAME_LENGTH * 2, "%s/%s",
                           image_path, json_object_get_string(tmpobj3));

                  json_object_object_get_ex(tmpobj2, "online file", &tmpobj3);
                  snprintf(curr_element->filename_online, MAX_FILENAME_LENGTH * 2, "%s/%s",
                           image_path, json_object_get_string(tmpobj3));

                  json_object_object_get_ex(tmpobj2, "warning file", &tmpobj3);
                  snprintf(curr_element->filename_warning, MAX_FILENAME_LENGTH * 2, "%s/%s",
                           image_path, json_object_get_string(tmpobj3));

                  json_object_object_get_ex(tmpobj2, "offline file", &tmpobj3);
                  snprintf(curr_element->filename_offline, MAX_FILENAME_LENGTH * 2, "%s/%s",
                           image_path, json_object_get_string(tmpobj3));

                  if (json_object_object_get_ex(tmpobj2, "warning temp", &tmpobj3)) {
                     curr_element->warning_temp = json_object_get_double(tmpobj3);
                  }

                  if (json_object_object_get_ex(tmpobj2, "warning voltage", &tmpobj3)) {
                     curr_element->warning_voltage = json_object_get_double(tmpobj3);
                  }

                  /* Parse armor display properties */
                  json_object *metrics_x_offset_obj;
                  json_object *metrics_y_offset_obj;

                  /* Default values if not specified */
                  curr_element->metrics_x_offset = 0.5f;
                  curr_element->metrics_y_offset = 0.5f;

                  /* Check if metrics_x_offset is specified */
                  if (json_object_object_get_ex(tmpobj2, "metrics_x_offset",
                                                &metrics_x_offset_obj)) {
                     curr_element->metrics_x_offset = (float)json_object_get_double(
                         metrics_x_offset_obj);

                     /* Clamp to valid range */
                     if (curr_element->metrics_x_offset < 0.0f) {
                        curr_element->metrics_x_offset = 0.0f;
                     }
                     if (curr_element->metrics_x_offset > 1.0f) {
                        curr_element->metrics_x_offset = 1.0f;
                     }
                  }

                  /* Check if metrics_y_offset is specified */
                  if (json_object_object_get_ex(tmpobj2, "metrics_y_offset",
                                                &metrics_y_offset_obj)) {
                     curr_element->metrics_y_offset = (float)json_object_get_double(
                         metrics_y_offset_obj);

                     /* Clamp to valid range */
                     if (curr_element->metrics_y_offset < 0.0f) {
                        curr_element->metrics_y_offset = 0.0f;
                     }
                     if (curr_element->metrics_y_offset > 1.0f) {
                        curr_element->metrics_y_offset = 1.0f;
                     }
                  }

                  curr_element->texture_base = get_cached_texture(curr_element->filename_base);
                  if (!curr_element->texture_base) {
                     LOG_ERROR("Couldn't load %s: %s\n", curr_element->filename, SDL_GetError());
                     json_object_put(parsed_json);
                     free(config_string);
                     return FAILURE;
                  }

                  curr_element->texture_online = get_cached_texture(curr_element->filename_online);
                  if (!curr_element->texture_online) {
                     LOG_ERROR("Couldn't load %s: %s\n", curr_element->filename_online,
                               SDL_GetError());
                     json_object_put(parsed_json);
                     free(config_string);
                     return FAILURE;
                  }

                  curr_element->texture_warning = get_cached_texture(
                      curr_element->filename_warning);
                  if (!curr_element->texture_warning) {
                     LOG_ERROR("Couldn't load %s: %s\n", curr_element->filename_warning,
                               SDL_GetError());
                     json_object_put(parsed_json);
                     free(config_string);
                     return FAILURE;
                  }

                  curr_element->texture_offline = get_cached_texture(
                      curr_element->filename_offline);
                  if (!curr_element->texture_offline) {
                     LOG_ERROR("Couldn't load %s: %s\n", curr_element->filename_offline,
                               SDL_GetError());
                     json_object_put(parsed_json);
                     free(config_string);
                     return FAILURE;
                  }

                  curr_element->texture = curr_element->texture_base;

                  SDL_QueryTexture(curr_element->texture, NULL, NULL, &curr_element->dst_rect.w,
                                   &curr_element->dst_rect.h);

                  curr_element->dst_rect.x = curr_element->dest_x = 0;
                  curr_element->dst_rect.y = curr_element->dest_y = 0;

                  prev_element = curr_element;
               }
            }
         }

         /* Parse Transitions section */
         if (strcmp(json_object_iter_peek_name(&it), "Transitions") == 0) {
            tmpobj = json_object_iter_peek_value(&it);

            /* Default transition type */
            json_object_object_get_ex(tmpobj, "default_type", &tmpobj2);
            if (tmpobj2 != NULL) {
               const char *type_name = json_object_get_string(tmpobj2);
               transition_t transition_type = find_transition_by_name(type_name);
               get_hud_manager()->transition_type = transition_type;
            }

            /* Default transition duration */
            json_object_object_get_ex(tmpobj, "default_duration", &tmpobj2);
            if (tmpobj2 != NULL) {
               int duration = json_object_get_int(tmpobj2);
               if (duration > 0) {
                  get_hud_manager()->transition_duration_ms = duration;
               }
            }
         }

         json_object_iter_next(&it);
      }
   }

   json_object_put(parsed_json);
   free(config_string);

   return SUCCESS;
}
