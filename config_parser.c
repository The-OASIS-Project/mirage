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

#include <stdio.h>
#include <json-c/json.h>

#include "SDL2/SDL_image.h"

#include "mirage.h"
#include "config_manager.h"
#include "config_parser.h"
#include "logging.h"

/* Parse json animation files. */
int parse_animated_json(element * curr_element)
{
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
      snprintf(curr_element->this_anim.image, MAX_FILENAME_LENGTH * 2, "%s/%s",
               get_image_path(), json_object_get_string(tmpobj2));

      json_object_object_get_ex(tmpobj, "format", &tmpobj2);
      strncpy(curr_element->this_anim.format, json_object_get_string(tmpobj2), 12);
   }

   json_object_put(parsed_json);

   free(config_string);

   return 0;
}

/* Parse a color string into its individual components. */
int parse_color(char *string, unsigned char *r, unsigned char *g, unsigned char *b,
                unsigned char *a)
{
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
void insert_element_by_layer(element * this_element)
{
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

/* Parse the primary json config file to configure the UI. */
int parse_json_config(char *filename)
{
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
   const char *tmpstr_ptr = NULL;

   char *config_string = NULL;
   int string_size = 0;
   int bytes_read = 0;

   SDL_Renderer *renderer = get_sdl_renderer();

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

   //printf("Config String (%d): \"%s\"\n", string_size, config_string);

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
                  this_hds->eye_output_height = json_object_get_int(json_object_iter_peek_value(&itSub));
                  //printf("OUTPUT_HEIGHT: %d\n", this_hds->eye_output_height);
                  if (this_ss->stream_height == -1) {
                     this_ss->stream_height = this_hds->eye_output_height;
                  }
               } else if (strcmp(json_object_iter_peek_name(&itSub), "Width") == 0) {
                  this_hds->eye_output_width = json_object_get_int(json_object_iter_peek_value(&itSub));
                  //printf("OUTPUT_WIDTH: %d\n", this_hds->eye_output_width);
                  if (this_ss->stream_width == -1) {
                     this_ss->stream_width = this_hds->eye_output_width * 2;
                  }
               } else if (strcmp(json_object_iter_peek_name(&itSub), "Stereo Offset") == 0) {
                  this_hds->stereo_offset = json_object_get_int(json_object_iter_peek_value(&itSub));
                  //printf("STEREO OFFSET: %d\n", this_hds->stereo_offset);
               } else if (strcmp(json_object_iter_peek_name(&itSub), "Pitch Offset") == 0) {
                  this_hds->pitch_offset = json_object_get_double(json_object_iter_peek_value(&itSub));
                  //printf("PITCH OFFSET: %f\n", this_hds->pitch_offset);
               } else if (strcmp(json_object_iter_peek_name(&itSub), "Image Path") == 0) {
                  if(set_image_path(json_object_get_string(json_object_iter_peek_value(&itSub)),
                           MAX_FILENAME_LENGTH - 1) == NULL)
                  {
                     LOG_ERROR("Error setting image path!");
                  }
                  //printf("IMAGE PATH: %s\n", get_image_path);
               } else if (strcmp(json_object_iter_peek_name(&itSub), "Font Path") == 0) {
                  if (set_font_path(json_object_get_string(json_object_iter_peek_value(&itSub)),
                           MAX_FILENAME_LENGTH - 1) == NULL)
                  {
                     LOG_ERROR("Error setting font path!");
                  }
                  //printf("FONT PATH: %s\n", get_font_path());
               } else if (strcmp(json_object_iter_peek_name(&itSub), "Sound Path") == 0) {
                  if (set_sound_path(json_object_get_string(json_object_iter_peek_value(&itSub)), MAX_FILENAME_LENGTH - 1) == NULL)
                  {
                     LOG_ERROR("Error setting sound path!");
                  }
                  //printf("SOUND PATH: %s\n", get_sound_path());
               } else if (strcmp(json_object_iter_peek_name(&itSub), "Wifi") == 0) {
                  if (set_wifi_dev_name(json_object_get_string(json_object_iter_peek_value(&itSub)),
                                        MAX_WIFI_DEV_LENGTH - 1) == NULL)
                  {
                     LOG_ERROR("Error settings Wifi device name!");
                  }
                  //printf("Wifi: %s\n", get_wifi_dev_name());
               } else if (strcmp(json_object_iter_peek_name(&itSub), "Invert Compass") == 0) {
                  set_inv_compass(json_object_get_boolean(json_object_iter_peek_value(&itSub)));
                  //printf("Invert Compass: %d\n", get_inv_compass());
               } else if (strcmp(json_object_iter_peek_name(&itSub), "Stream Width") == 0) {
                  this_ss->stream_width = json_object_get_int(json_object_iter_peek_value(&itSub));
                  //printf("Stream Width: %d\n", stream_width);
               } else if (strcmp(json_object_iter_peek_name(&itSub), "Stream Height") == 0) {
                  this_ss->stream_height = json_object_get_int(json_object_iter_peek_value(&itSub));
                  //printf("Stream Width: %d\n", stream_height);
               } else if (strcmp(json_object_iter_peek_name(&itSub), "Stream Dest IP") == 0) {
                  strncpy(this_ss->stream_dest_ip, json_object_get_string(json_object_iter_peek_value(&itSub)),
                          16);
                  //printf("Stream Dest IP: %s\n", this_ss->stream_dest_ip);

               } else if (strcmp(json_object_iter_peek_name(&itSub), "Armor dest_x") == 0) {
                  this_as->armor_dest.x = json_object_get_int(json_object_iter_peek_value(&itSub));
                  //printf("Armor dest_x: %d\n", this_as->armor_dest.x);
               } else if (strcmp(json_object_iter_peek_name(&itSub), "Armor dest_y") == 0) {
                  this_as->armor_dest.y = json_object_get_int(json_object_iter_peek_value(&itSub));
                  //printf("Armor dest_y: %d\n", this_as->armor_dest.y);
               } else if (strcmp(json_object_iter_peek_name(&itSub), "Armor dest_w") == 0) {
                  this_as->armor_dest.w = json_object_get_int(json_object_iter_peek_value(&itSub));
                  //printf("Armor dest_w: %d\n", this_as->armor_dest.w);
               } else if (strcmp(json_object_iter_peek_name(&itSub), "Armor dest_h") == 0) {
                  this_as->armor_dest.h = json_object_get_int(json_object_iter_peek_value(&itSub));
                  //printf("Armor dest_h: %d\n", this_as->armor_dest.h);

               } else if (strcmp(json_object_iter_peek_name(&itSub), "Armor notice dest_x") == 0) {
                  this_as->armor_notice_dest.x = json_object_get_int(json_object_iter_peek_value(&itSub));
                  //printf("Armor notice dest_x: %d\n", this_as->armor_notice_dest.x);
               } else if (strcmp(json_object_iter_peek_name(&itSub), "Armor notice dest_y") == 0) {
                  this_as->armor_notice_dest.y = json_object_get_int(json_object_iter_peek_value(&itSub));
                  //printf("Armor notice dest_y: %d\n", this_as->armor_notice_dest.y);
               } else if (strcmp(json_object_iter_peek_name(&itSub), "Armor notice dest_w") == 0) {
                  this_as->armor_notice_dest.w = json_object_get_int(json_object_iter_peek_value(&itSub));
                  //printf("Armor notice dest_w: %d\n", this_as->armor_notice_dest.w);
               } else if (strcmp(json_object_iter_peek_name(&itSub), "Armor notice dest_h") == 0) {
                  this_as->armor_notice_dest.h = json_object_get_int(json_object_iter_peek_value(&itSub));
                  //printf("Armor notice dest_h: %d\n", this_as->armor_notice_dest.h);
               } else if (strcmp(json_object_iter_peek_name(&itSub), "Armor notice timeout") == 0) {
                  this_as->armor_notice_timeout = json_object_get_int(json_object_iter_peek_value(&itSub));
                  //printf("Armor notice timeout: %d\n", this_as->armor_notice_timeout);

               } else {
                  printf("%s\n", json_object_iter_peek_name(&itSub));
               }
               json_object_iter_next(&itSub);
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
                  /* Special case for special types. */
                  if ((tmpobj3 != NULL) && (strcmp("intro", json_object_get_string(tmpobj3)) == 0)) {
                     memcpy(intro_element, default_element, sizeof(element));
                     intro_element->enabled = 1;

                     json_object_object_get_ex(tmpobj2, "file", &tmpobj3);
                     //printf("file: %s\n", json_object_get_string(tmpobj3));
                     snprintf(intro_element->filename, MAX_FILENAME_LENGTH * 2,
                              "%s/%s", image_path, json_object_get_string(tmpobj3));

                     json_object_object_get_ex(tmpobj2, "dest_x", &tmpobj3);
                     //printf("dest_x: %d\n", json_object_get_int(tmpobj3));
                     intro_element->dest_x = json_object_get_int(tmpobj3);

                     json_object_object_get_ex(tmpobj2, "dest_y", &tmpobj3);
                     //printf("dest_y: %d\n", json_object_get_int(tmpobj3));
                     intro_element->dest_y = json_object_get_int(tmpobj3);

                     json_object_object_get_ex(tmpobj2, "angle", &tmpobj3);
                     //printf("angle: %s\n", json_object_get_string(tmpobj3));
                     if (strcmp(json_object_get_string(tmpobj3), "roll") == 0) {
                        intro_element->angle = ANGLE_ROLL;
                     } else if (strcmp(json_object_get_string(tmpobj3), "opposite roll") == 0) {
                        intro_element->angle = ANGLE_OPPOSITE_ROLL;
                     } else {
                        intro_element->angle = json_object_get_double(tmpobj3);
                     }

                     parse_animated_json(intro_element);
                  } else if (tmpobj3 != NULL) {
                     //printf("Element[%d]: Type: %s\n", i, json_object_get_string(tmpobj3));
                     curr_element = malloc(sizeof(element));
                     if (curr_element == NULL) {
                        LOG_ERROR("Cannot malloc new element!");
                        exit(1);
                     }
                     memcpy(curr_element, default_element, sizeof(element));
                     curr_element->enabled = 1;

                     /* STATIC */
                     if (strcmp("static", json_object_get_string(tmpobj3)) == 0) {
                        curr_element->type = STATIC;

                        json_object_object_get_ex(tmpobj2, "name", &tmpobj3);
                        if (tmpobj3 != NULL) {
                           //printf("name: %s\n", json_object_get_string(tmpobj3));
                           strncpy(curr_element->name, json_object_get_string(tmpobj3), MAX_TEXT_LENGTH - 1);
                           curr_element->name[MAX_TEXT_LENGTH - 1] = '\0';
                        }

                        json_object_object_get_ex(tmpobj2, "file", &tmpobj3);
                        //printf("file: %s\n", json_object_get_string(tmpobj3));
                        snprintf(curr_element->filename, MAX_FILENAME_LENGTH * 2,
                                 "%s/%s", image_path, json_object_get_string(tmpobj3));

                        json_object_object_get_ex(tmpobj2, "dest_x", &tmpobj3);
                        //printf("dest_x: %d\n", json_object_get_int(tmpobj3));
                        curr_element->dest_x = json_object_get_int(tmpobj3);

                        json_object_object_get_ex(tmpobj2, "dest_y", &tmpobj3);
                        //printf("dest_y: %d\n", json_object_get_int(tmpobj3));
                        curr_element->dest_y = json_object_get_int(tmpobj3);

                        json_object_object_get_ex(tmpobj2, "width", &tmpobj3);
                        //printf("width: %d\n", json_object_get_int(tmpobj3));
                        curr_element->width = json_object_get_int(tmpobj3);

                        json_object_object_get_ex(tmpobj2, "height", &tmpobj3);
                        //printf("height: %d\n", json_object_get_int(tmpobj3));
                        curr_element->height = json_object_get_int(tmpobj3);

                        json_object_object_get_ex(tmpobj2, "angle", &tmpobj3);
                        //printf("angle: %s\n", json_object_get_string(tmpobj3));
                        if (strcmp(json_object_get_string(tmpobj3), "roll") == 0) {
                           curr_element->angle = ANGLE_ROLL;
                        } else if (strcmp(json_object_get_string(tmpobj3), "opposite roll") == 0) {
                           curr_element->angle = ANGLE_OPPOSITE_ROLL;
                        } else {
                           curr_element->angle = json_object_get_double(tmpobj3);
                        }

                        if (json_object_object_get_ex(tmpobj2, "fixed", &tmpobj3)) {
                           //printf("fixed: %d\n", json_object_get_int(tmpobj3));
                           curr_element->fixed = json_object_get_int(tmpobj3);
                        } else {
                           curr_element->fixed = FIXED_DEFAULT;
                        }

                        json_object_object_get_ex(tmpobj2, "layer", &tmpobj3);
                        //printf("layer: %d\n", json_object_get_int(tmpobj3));
                        curr_element->layer = json_object_get_int(tmpobj3);

                        if (json_object_object_get_ex(tmpobj2, "enabled", &tmpobj3)) {
                           //printf("enabled: %d\n", json_object_get_int(tmpobj3));
                           curr_element->enabled = json_object_get_int(tmpobj3);
                        }

                        if (json_object_object_get_ex(tmpobj2, "hotkey", &tmpobj3)) {
                           tmpstr_ptr = json_object_get_string(tmpobj3);
                           if (tmpstr_ptr != NULL) {
                              //printf("hotkey: %s\n", tmpstr_ptr);
                              strcpy(tmpstr, json_object_get_string(tmpobj3));
                              strncpy(curr_element->hotkey, tmpstr_ptr, 2);
                           }
                        }
                        //printf("Loading static element: %s\n", curr_element->filename);
                        curr_element->texture = IMG_LoadTexture(renderer, curr_element->filename);
                        if (!curr_element->texture) {
                           SDL_Log("Couldn't load %s: %s\n", curr_element->filename,
                                   SDL_GetError());
                           return FAILURE;
                        }

                        if ((curr_element->width == 0) && (curr_element->height == 0)) {
                           SDL_QueryTexture(curr_element->texture, NULL, NULL,
                                            &curr_element->dst_rect.w, &curr_element->dst_rect.h);
                        } else if ((curr_element->width == 0) && (curr_element->height != 0)) {
                           SDL_QueryTexture(curr_element->texture, NULL, NULL,
                                            &curr_element->dst_rect.w, &curr_element->dst_rect.h);
                           ratio = (double) curr_element->height / (double) curr_element->dst_rect.h;
                           curr_element->dst_rect.w = curr_element->width =
                                                      curr_element->dst_rect.w * ratio;
                        } else if ((curr_element->width != 0) && (curr_element->height == 0)) {
                           SDL_QueryTexture(curr_element->texture, NULL, NULL,
                                            &curr_element->dst_rect.w, &curr_element->dst_rect.h);
                           ratio = (double) curr_element->width / (double) curr_element->dst_rect.w;
                           curr_element->dst_rect.h = curr_element->height =
                                                      curr_element->dst_rect.h * ratio;
                        } else {
                           curr_element->dst_rect.w = curr_element->width;
                           curr_element->dst_rect.h = curr_element->height;
                        }
                        curr_element->dst_rect.x = curr_element->dest_x;
                        curr_element->dst_rect.y = curr_element->dest_y;
                     } else
                     /* Record Graphic */
                     if (strcmp("record-ui", json_object_get_string(tmpobj3)) == 0) {
                        curr_element->type = STATIC;

                        json_object_object_get_ex(tmpobj2, "file", &tmpobj3);
                        //printf("file: %s\n", json_object_get_string(tmpobj3));
                        snprintf(curr_element->filename, MAX_FILENAME_LENGTH * 2,
                                 "%s/%s", image_path, json_object_get_string(tmpobj3));

                        json_object_object_get_ex(tmpobj2, "file_r", &tmpobj3);
                        //printf("file_r: %s\n", json_object_get_string(tmpobj3));
                        snprintf(curr_element->filename_r, MAX_FILENAME_LENGTH * 2,
                                 "%s/%s", image_path, json_object_get_string(tmpobj3));

                        json_object_object_get_ex(tmpobj2, "file_s", &tmpobj3);
                        //printf("file_s: %s\n", json_object_get_string(tmpobj3));
                        snprintf(curr_element->filename_s, MAX_FILENAME_LENGTH * 2,
                                 "%s/%s", image_path, json_object_get_string(tmpobj3));

                        json_object_object_get_ex(tmpobj2, "file_rs", &tmpobj3);
                        //printf("file_rs: %s\n", json_object_get_string(tmpobj3));
                        snprintf(curr_element->filename_rs, MAX_FILENAME_LENGTH * 2,
                                 "%s/%s", image_path, json_object_get_string(tmpobj3));

                        json_object_object_get_ex(tmpobj2, "dest_x", &tmpobj3);
                        //printf("dest_x: %d\n", json_object_get_int(tmpobj3));
                        curr_element->dest_x = json_object_get_int(tmpobj3);

                        json_object_object_get_ex(tmpobj2, "dest_y", &tmpobj3);
                        //printf("dest_y: %d\n", json_object_get_int(tmpobj3));
                        curr_element->dest_y = json_object_get_int(tmpobj3);

                        json_object_object_get_ex(tmpobj2, "angle", &tmpobj3);
                        //printf("angle: %s\n", json_object_get_string(tmpobj3));
                        if (strcmp(json_object_get_string(tmpobj3), "roll") == 0) {
                           curr_element->angle = ANGLE_ROLL;
                        } else if (strcmp(json_object_get_string(tmpobj3), "opposite roll") == 0) {
                           curr_element->angle = ANGLE_OPPOSITE_ROLL;
                        } else {
                           curr_element->angle = json_object_get_double(tmpobj3);
                        }

                        if (json_object_object_get_ex(tmpobj2, "fixed", &tmpobj3)) {
                           //printf("fixed: %d\n", json_object_get_int(tmpobj3));
                           curr_element->fixed = json_object_get_int(tmpobj3);
                        } else {
                           curr_element->fixed = FIXED_DEFAULT;
                        }

                        json_object_object_get_ex(tmpobj2, "layer", &tmpobj3);
                        //printf("layer: %d\n", json_object_get_int(tmpobj3));
                        curr_element->layer = json_object_get_int(tmpobj3);

                        if (json_object_object_get_ex(tmpobj2, "enabled", &tmpobj3)) {
                           //printf("enabled: %d\n", json_object_get_int(tmpobj3));
                           curr_element->enabled = json_object_get_int(tmpobj3);
                        }

                        if (json_object_object_get_ex(tmpobj2, "hotkey", &tmpobj3)) {
                           tmpstr_ptr = json_object_get_string(tmpobj3);
                           if (tmpstr_ptr != NULL) {
                              //printf("hotkey: %s\n", tmpstr_ptr);
                              strcpy(tmpstr, json_object_get_string(tmpobj3));
                              strncpy(curr_element->hotkey, tmpstr_ptr, 2);
                           }
                        }
                        //printf("Loading static element: %s\n", curr_element->filename);
                        curr_element->texture = IMG_LoadTexture(renderer, curr_element->filename);
                        if (!curr_element->texture) {
                           SDL_Log("Couldn't load %s: %s\n", curr_element->filename,
                                   SDL_GetError());
                           return FAILURE;
                        }
                        //printf("Loading static element (r): %s\n", curr_element->filename_r);
                        curr_element->texture_r = IMG_LoadTexture(renderer, curr_element->filename_r);
                        if (!curr_element->texture_r) {
                           SDL_Log("Couldn't load %s: %s\n", curr_element->filename_r,
                                   SDL_GetError());
                           return FAILURE;
                        }
                        //printf("Loading static element (s): %s\n", curr_element->filename_s);
                        curr_element->texture_s = IMG_LoadTexture(renderer, curr_element->filename_s);
                        if (!curr_element->texture_s) {
                           SDL_Log("Couldn't load %s: %s\n", curr_element->filename_s,
                                   SDL_GetError());
                           return FAILURE;
                        }
                        //printf("Loading static element (rs): %s\n", curr_element->filename_rs);
                        curr_element->texture_rs = IMG_LoadTexture(renderer, curr_element->filename_rs);
                        if (!curr_element->texture_rs) {
                           SDL_Log("Couldn't load %s: %s\n", curr_element->filename_rs,
                                   SDL_GetError());
                           return FAILURE;
                        }

                        SDL_QueryTexture(curr_element->texture, NULL, NULL,
                                         &curr_element->dst_rect.w, &curr_element->dst_rect.h);
                        curr_element->dst_rect.x = curr_element->dest_x;
                        curr_element->dst_rect.y = curr_element->dest_y;
                     } else
                     /* ANIMATED */
                     if (strcmp("animated", json_object_get_string(tmpobj3)) == 0) {
                        curr_element->type = ANIMATED;

                        json_object_object_get_ex(tmpobj2, "name", &tmpobj3);
                        if (tmpobj3 != NULL) {
                           //printf("name: %s\n", json_object_get_string(tmpobj3));
                           strncpy(curr_element->name, json_object_get_string(tmpobj3), MAX_TEXT_LENGTH - 1);
                           curr_element->name[MAX_TEXT_LENGTH - 1] = '\0';
                        }

                        json_object_object_get_ex(tmpobj2, "file", &tmpobj3);
                        //printf("file: %s\n", json_object_get_string(tmpobj3));
                        snprintf(curr_element->filename, MAX_FILENAME_LENGTH * 2,
                                 "%s/%s", image_path, json_object_get_string(tmpobj3));

                        json_object_object_get_ex(tmpobj2, "dest_x", &tmpobj3);
                        //printf("dest_x: %d\n", json_object_get_int(tmpobj3));
                        curr_element->dest_x = json_object_get_int(tmpobj3);

                        json_object_object_get_ex(tmpobj2, "dest_y", &tmpobj3);
                        //printf("dest_y: %d\n", json_object_get_int(tmpobj3));
                        curr_element->dest_y = json_object_get_int(tmpobj3);

                        json_object_object_get_ex(tmpobj2, "width", &tmpobj3);
                        //printf("width: %d\n", json_object_get_int(tmpobj3));
                        curr_element->width = json_object_get_int(tmpobj3);

                        json_object_object_get_ex(tmpobj2, "height", &tmpobj3);
                        //printf("height: %d\n", json_object_get_int(tmpobj3));
                        curr_element->height = json_object_get_int(tmpobj3);

                        json_object_object_get_ex(tmpobj2, "angle", &tmpobj3);
                        //printf("angle: %s\n", json_object_get_string(tmpobj3));
                        if (strcmp(json_object_get_string(tmpobj3), "roll") == 0) {
                           curr_element->angle = ANGLE_ROLL;
                        } else if (strcmp(json_object_get_string(tmpobj3), "opposite roll") == 0) {
                           curr_element->angle = ANGLE_OPPOSITE_ROLL;
                        } else {
                           curr_element->angle = json_object_get_double(tmpobj3);
                        }

                        if (json_object_object_get_ex(tmpobj2, "fixed", &tmpobj3)) {
                           //printf("fixed: %d\n", json_object_get_int(tmpobj3));
                           curr_element->fixed = json_object_get_int(tmpobj3);
                        } else {
                           curr_element->fixed = FIXED_DEFAULT;
                        }

                        json_object_object_get_ex(tmpobj2, "layer", &tmpobj3);
                        //printf("layer: %d\n", json_object_get_int(tmpobj3));
                        curr_element->layer = json_object_get_int(tmpobj3);

                        if (json_object_object_get_ex(tmpobj2, "enabled", &tmpobj3)) {
                           //printf("enabled: %d\n", json_object_get_int(tmpobj3));
                           curr_element->enabled = json_object_get_int(tmpobj3);
                        }

                        if (json_object_object_get_ex(tmpobj2, "hotkey", &tmpobj3)) {
                           tmpstr_ptr = json_object_get_string(tmpobj3);
                           if (tmpstr_ptr != NULL) {
                              //printf("hotkey: %s\n", tmpstr_ptr);
                              strcpy(tmpstr, json_object_get_string(tmpobj3));
                              strncpy(curr_element->hotkey, tmpstr_ptr, 2);
                           }
                        }

                        parse_animated_json(curr_element);

                        //printf("Loading animation source: %s\n", curr_element->this_anim.image);
                        curr_element->texture =
                            IMG_LoadTexture(renderer, curr_element->this_anim.image);
                        if (!curr_element->texture) {
                           SDL_Log("Couldn't load %s: %s\n", curr_element->this_anim.image,
                                   SDL_GetError());
                           return FAILURE;
                        }
                     } else
                     /* TEXT */
                     if (strcmp("text", json_object_get_string(tmpobj3)) == 0) {
                        curr_element->type = TEXT;

                        json_object_object_get_ex(tmpobj2, "name", &tmpobj3);
                        if (tmpobj3 != NULL) {
                           //printf("name: %s\n", json_object_get_string(tmpobj3));
                           strncpy(curr_element->name, json_object_get_string(tmpobj3), MAX_TEXT_LENGTH - 1);
                           curr_element->name[MAX_TEXT_LENGTH - 1] = '\0';
                        }

                        json_object_object_get_ex(tmpobj2, "string", &tmpobj3);
                        //printf("string: %s\n", json_object_get_string(tmpobj3));
                        strncpy(curr_element->text, json_object_get_string(tmpobj3),
                                MAX_TEXT_LENGTH);

                        json_object_object_get_ex(tmpobj2, "font", &tmpobj3);
                        //printf("font: %s\n", json_object_get_string(tmpobj3));
                        snprintf(curr_element->font, MAX_FILENAME_LENGTH * 2,
                                 "%s/%s", get_font_path(), json_object_get_string(tmpobj3));

                        json_object_object_get_ex(tmpobj2, "color", &tmpobj3);
                        //printf("color: %s\n", json_object_get_string(tmpobj3));
                        strncpy(tmpstr, json_object_get_string(tmpobj3), 1024);
                        parse_color(tmpstr, &curr_element->font_color.r,
                                    &curr_element->font_color.g, &curr_element->font_color.b,
                                    &curr_element->font_color.a);

                        json_object_object_get_ex(tmpobj2, "size", &tmpobj3);
                        //printf("size: %d\n", json_object_get_int(tmpobj3));
                        curr_element->font_size = json_object_get_int(tmpobj3);

                        if ((strcmp(curr_element->font, "") != 0) && (curr_element->font_size > 0)) {
                           curr_element->ttf_font =
                               get_local_font(curr_element->font, curr_element->font_size);
                        }

                        json_object_object_get_ex(tmpobj2, "dest_x", &tmpobj3);
                        //printf("dest_x: %d\n", json_object_get_int(tmpobj3));
                        curr_element->dest_x = json_object_get_int(tmpobj3);

                        json_object_object_get_ex(tmpobj2, "dest_y", &tmpobj3);
                        //printf("dest_y: %d\n", json_object_get_int(tmpobj3));
                        curr_element->dest_y = json_object_get_int(tmpobj3);

                        json_object_object_get_ex(tmpobj2, "halign", &tmpobj3);
                        //printf("halign: %s\n", json_object_get_string(tmpobj3));
                        strncpy(curr_element->halign, json_object_get_string(tmpobj3), 7);

                        json_object_object_get_ex(tmpobj2, "angle", &tmpobj3);
                        //printf("angle: %s\n", json_object_get_string(tmpobj3));
                        if (strcmp(json_object_get_string(tmpobj3), "roll") == 0) {
                           curr_element->angle = ANGLE_ROLL;
                        } else if (strcmp(json_object_get_string(tmpobj3), "opposite roll") == 0) {
                           curr_element->angle = ANGLE_OPPOSITE_ROLL;
                        } else {
                           curr_element->angle = json_object_get_double(tmpobj3);
                        }

                        if (json_object_object_get_ex(tmpobj2, "fixed", &tmpobj3)) {
                           //printf("fixed: %d\n", json_object_get_int(tmpobj3));
                           curr_element->fixed = json_object_get_int(tmpobj3);
                        } else {
                           curr_element->fixed = FIXED_DEFAULT;
                        }

                        json_object_object_get_ex(tmpobj2, "layer", &tmpobj3);
                        //printf("layer: %d\n", json_object_get_int(tmpobj3));
                        curr_element->layer = json_object_get_int(tmpobj3);

                        if (json_object_object_get_ex(tmpobj2, "enabled", &tmpobj3)) {
                           //printf("enabled: %d\n", json_object_get_int(tmpobj3));
                           curr_element->enabled = json_object_get_int(tmpobj3);
                        }

                        if (json_object_object_get_ex(tmpobj2, "hotkey", &tmpobj3)) {
                           tmpstr_ptr = json_object_get_string(tmpobj3);
                           if (tmpstr_ptr != NULL) {
                              //printf("hotkey: %s\n", tmpstr_ptr);
                              strcpy(tmpstr, json_object_get_string(tmpobj3));
                              strncpy(curr_element->hotkey, tmpstr_ptr, 2);
                           }
                        }

                     } else
                        /* SPECIAL */
                     if (strcmp("special", json_object_get_string(tmpobj3)) == 0) {
                        curr_element->type = SPECIAL;

                        json_object_object_get_ex(tmpobj2, "name", &tmpobj3);
                        //printf("name: %s\n", json_object_get_string(tmpobj3));
                        strncpy(curr_element->special_name, json_object_get_string(tmpobj3),
                                MAX_TEXT_LENGTH - 1);
                        curr_element->special_name[MAX_TEXT_LENGTH - 1] = '\0';
                        strncpy(curr_element->name, json_object_get_string(tmpobj3), MAX_TEXT_LENGTH - 1);
                        curr_element->name[MAX_TEXT_LENGTH - 1] = '\0';

                        if (strncmp(curr_element->special_name, "detect", 6) == 0) {
                           set_detect_enabled(1);
                        }

                        json_object_object_get_ex(tmpobj2, "file", &tmpobj3);
                        //printf("file: %s\n", json_object_get_string(tmpobj3));
                        snprintf(curr_element->filename, MAX_FILENAME_LENGTH * 2,
                                 "%s/%s", image_path, json_object_get_string(tmpobj3));

                        json_object_object_get_ex(tmpobj2, "width", &tmpobj3);
                        //printf("width: %d\n", json_object_get_int(tmpobj3));
                        curr_element->width = json_object_get_int(tmpobj3);

                        json_object_object_get_ex(tmpobj2, "height", &tmpobj3);
                        //printf("height: %d\n", json_object_get_int(tmpobj3));
                        curr_element->height = json_object_get_int(tmpobj3);

                        json_object_object_get_ex(tmpobj2, "dest_x", &tmpobj3);
                        //printf("dest_x: %d\n", json_object_get_int(tmpobj3));
                        curr_element->dest_x = json_object_get_int(tmpobj3);

                        json_object_object_get_ex(tmpobj2, "dest_y", &tmpobj3);
                        //printf("dest_y: %d\n", json_object_get_int(tmpobj3));
                        curr_element->dest_y = json_object_get_int(tmpobj3);

                        json_object_object_get_ex(tmpobj2, "angle", &tmpobj3);
                        //printf("angle: %s\n", json_object_get_string(tmpobj3));
                        if (strcmp(json_object_get_string(tmpobj3), "roll") == 0) {
                           curr_element->angle = ANGLE_ROLL;
                        } else if (strcmp(json_object_get_string(tmpobj3), "opposite roll") == 0) {
                           curr_element->angle = ANGLE_OPPOSITE_ROLL;
                        } else {
                           curr_element->angle = json_object_get_double(tmpobj3);
                        }

                        if (json_object_object_get_ex(tmpobj2, "fixed", &tmpobj3)) {
                           //printf("fixed: %d\n", json_object_get_int(tmpobj3));
                           curr_element->fixed = json_object_get_int(tmpobj3);
                        } else {
                           curr_element->fixed = FIXED_DEFAULT;
                        }

                        json_object_object_get_ex(tmpobj2, "font", &tmpobj3);
                        if (tmpobj3 != NULL) {
                           //printf("font: %s\n", json_object_get_string(tmpobj3));
                           snprintf(curr_element->font, MAX_FILENAME_LENGTH * 2,
                                    "%s/%s", get_font_path(), json_object_get_string(tmpobj3));
                        }

                        json_object_object_get_ex(tmpobj2, "color", &tmpobj3);
                        if (tmpobj3 != NULL) {
                           //printf("color: %s\n", json_object_get_string(tmpobj3));
                           strncpy(tmpstr, json_object_get_string(tmpobj3), 1024);
                           parse_color(tmpstr, &curr_element->font_color.r,
                                       &curr_element->font_color.g, &curr_element->font_color.b,
                                       &curr_element->font_color.a);
                        }

                        json_object_object_get_ex(tmpobj2, "size", &tmpobj3);
                        if (tmpobj3 != NULL) {
                           //printf("size: %d\n", json_object_get_int(tmpobj3));
                           curr_element->font_size = json_object_get_int(tmpobj3);
                        }

                        if ((strcmp(curr_element->font, "") != 0) && (curr_element->font_size > 0)) {
                           curr_element->ttf_font =
                               get_local_font(curr_element->font, curr_element->font_size);
                        }

                        json_object_object_get_ex(tmpobj2, "layer", &tmpobj3);
                        //printf("layer: %d\n", json_object_get_int(tmpobj3));
                        curr_element->layer = json_object_get_int(tmpobj3);

                        if (json_object_object_get_ex(tmpobj2, "enabled", &tmpobj3)) {
                           //printf("enabled: %d\n", json_object_get_int(tmpobj3));
                           curr_element->enabled = json_object_get_int(tmpobj3);
                        }

                        if (json_object_object_get_ex(tmpobj2, "hotkey", &tmpobj3)) {
                           tmpstr_ptr = json_object_get_string(tmpobj3);
                           if (tmpstr_ptr != NULL) {
                              //printf("hotkey: %s\n", tmpstr_ptr);
                              strcpy(tmpstr, json_object_get_string(tmpobj3));
                              strncpy(curr_element->hotkey, tmpstr_ptr, 2);
                           }
                        }

                        json_object_object_get_ex(tmpobj2, "center_x_offset", &tmpobj3);
                        if (tmpobj3 != NULL) {
                           //printf("center_x_offset: %d\n", json_object_get_int(tmpobj3));
                           curr_element->center_x_offset = json_object_get_int(tmpobj3);
                        } else {
                           curr_element->center_x_offset = 0;
                        }

                        json_object_object_get_ex(tmpobj2, "center_y_offset", &tmpobj3);
                        if (tmpobj3 != NULL) {
                           //printf("center_y_offset: %d\n", json_object_get_int(tmpobj3));
                           curr_element->center_y_offset = json_object_get_int(tmpobj3);
                        } else {
                           curr_element->center_y_offset = 0;
                        }

                        json_object_object_get_ex(tmpobj2, "text_x_offset", &tmpobj3);
                        if (tmpobj3 != NULL) {
                           //printf("text_x_offset: %d\n", json_object_get_int(tmpobj3));
                           curr_element->text_x_offset = json_object_get_int(tmpobj3);
                        } else {
                           curr_element->text_x_offset = 0;
                        }

                        json_object_object_get_ex(tmpobj2, "text_y_offset", &tmpobj3);
                        if (tmpobj3 != NULL) {
                           //printf("text_y_offset: %d\n", json_object_get_int(tmpobj3));
                           curr_element->text_y_offset = json_object_get_int(tmpobj3);
                        } else {
                           curr_element->text_y_offset = 0;
                        }

                        /* HEADING or PITCH or ALTITUDE or WIFI */
                        if ((strcmp("heading", curr_element->special_name) == 0) ||
                            (strcmp("pitch", curr_element->special_name) == 0) ||
                            (strcmp("altitude", curr_element->special_name) == 0) ||
                            (strcmp("wifi", curr_element->special_name) == 0) ||
                            (strcmp("detect", curr_element->special_name) == 0)) {
                           parse_animated_json(curr_element);

                           if (strcmp("detect", curr_element->special_name) != 0) {
                              //printf("Loading special source: %s\n", curr_element->this_anim.image);
                              curr_element->texture = IMG_LoadTexture(renderer,
                                                                      curr_element->this_anim.
                                                                      image);
                              if (!curr_element->texture) {
                                 SDL_Log("Couldn't load %s: %s\n",
                                         curr_element->filename, SDL_GetError());
                                 return 1;
                              }
                           }
                        }
                     }
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
                  curr_element->type = ARMOR_COMPONENT,
                  curr_element->enabled = 1;
                  curr_element->mqtt_registered = 0;

                  if (this_as->armor_elements == NULL) {
                     this_as->armor_elements = curr_element;
                  } else {
                     prev_element->next = curr_element;
                     curr_element->prev = prev_element;
                  }

                  tmpobj2 = json_object_array_get_idx(tmpobj, i);

                  json_object_object_get_ex(tmpobj2, "name", &tmpobj3);
                  //printf("name: %s\n", json_object_get_string(tmpobj3));
                  strncpy(curr_element->name, json_object_get_string(tmpobj3),
                          MAX_TEXT_LENGTH);

                  json_object_object_get_ex(tmpobj2, "device", &tmpobj3);
                  //printf("device: %s\n", json_object_get_string(tmpobj3));
                  strncpy(curr_element->mqtt_device, json_object_get_string(tmpobj3),
                          MAX_TEXT_LENGTH);


                  json_object_object_get_ex(tmpobj2, "base file", &tmpobj3);
                  //printf("base file: %s\n", json_object_get_string(tmpobj3));
                  snprintf(curr_element->filename, MAX_FILENAME_LENGTH * 2,
                           "%s/%s", image_path, json_object_get_string(tmpobj3));

                  json_object_object_get_ex(tmpobj2, "online file", &tmpobj3);
                  //printf("online file: %s\n", json_object_get_string(tmpobj3));
                  snprintf(curr_element->filename_online, MAX_FILENAME_LENGTH * 2,
                           "%s/%s", image_path, json_object_get_string(tmpobj3));

                  json_object_object_get_ex(tmpobj2, "warning file", &tmpobj3);
                  //printf("warning file: %s\n", json_object_get_string(tmpobj3));
                  snprintf(curr_element->filename_warning, MAX_FILENAME_LENGTH * 2,
                           "%s/%s", image_path, json_object_get_string(tmpobj3));

                  json_object_object_get_ex(tmpobj2, "offline file", &tmpobj3);
                  //printf("offline file: %s\n", json_object_get_string(tmpobj3));
                  snprintf(curr_element->filename_offline, MAX_FILENAME_LENGTH * 2,
                           "%s/%s", image_path, json_object_get_string(tmpobj3));

                  if (json_object_object_get_ex(tmpobj2, "warning temp", &tmpobj3)) {
                     //printf("warning temp: %0.2f\n", json_object_get_double(tmpobj3));
                     curr_element->warning_temp = json_object_get_double(tmpobj3);
                  }

                  if (json_object_object_get_ex(tmpobj2, "warning voltage", &tmpobj3)) {
                     //printf("warning voltage: %0.2f\n", json_object_get_double(tmpobj3));
                     curr_element->warning_voltage = json_object_get_double(tmpobj3);
                  }

                  curr_element->texture_base = IMG_LoadTexture(renderer, curr_element->filename);
                  if (!curr_element->texture_base) {
                     SDL_Log("Couldn't load %s: %s\n",
                             curr_element->filename, SDL_GetError());
                     return FAILURE;
                  }

                  curr_element->texture_online = IMG_LoadTexture(renderer, curr_element->filename_online);
                  if (!curr_element->texture_online) {
                     SDL_Log("Couldn't load %s: %s\n",
                             curr_element->filename_online, SDL_GetError());
                     return FAILURE;
                  }

                  curr_element->texture_warning = IMG_LoadTexture(renderer, curr_element->filename_warning);
                  if (!curr_element->texture_warning) {
                     SDL_Log("Couldn't load %s: %s\n",
                             curr_element->filename_warning, SDL_GetError());
                     return FAILURE;
                  }

                  curr_element->texture_offline = IMG_LoadTexture(renderer, curr_element->filename_offline);
                  if (!curr_element->texture_offline) {
                     SDL_Log("Couldn't load %s: %s\n",
                             curr_element->filename_offline, SDL_GetError());
                     return FAILURE;
                  }

                  curr_element->texture = curr_element->texture_base;

                  SDL_QueryTexture(curr_element->texture, NULL, NULL,
                                   &curr_element->dst_rect.w, &curr_element->dst_rect.h);
                  //printf("Armor Element \"%s\" Size %dx%d\n", curr_element->filename,
                  //       curr_element->dst_rect.w, curr_element->dst_rect.h);

                  curr_element->dst_rect.x = curr_element->dest_x = 0;
                  curr_element->dst_rect.y = curr_element->dest_y = 0;

                  prev_element = curr_element;
               }
            }
         }

         json_object_iter_next(&it);
      }
   }

   json_object_put(parsed_json);

   //dump_element_list();

   return SUCCESS;
}


