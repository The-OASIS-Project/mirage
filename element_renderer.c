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

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

/* SDL Libraries */
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

/* Local Headers */
#include "armor.h"
#include "command_processing.h"
#include "config_parser.h"
#include "config_manager.h"
#include "curl_download.h"
#include "defines.h"
#include "devices.h"
#include "element_renderer.h"
#include "fan_monitoring.h"
#include "hud_manager.h"
#include "logging.h"
#include "mirage.h"
#include "recording.h"
#include "secrets.h"

/* Globals and external references */
// video elements
extern od_data oddataL, oddataR;
extern pthread_t od_L_thread, od_R_thread;

// detection elements
extern int detect_enabled;
extern detect this_detect[2][MAX_DETECT];
extern detect this_detect_sorted[2][MAX_DETECT];

// alert elements
extern alert_t active_alerts;
extern const struct Alert alert_messages[];

extern double averageFrameRate;

/* Reset alpha values for all textures of an element */
static void reset_texture_alpha(element *curr_element) {
   if (curr_element->texture)
      SDL_SetTextureAlphaMod(curr_element->texture, 255);
   if (curr_element->texture_r)
      SDL_SetTextureAlphaMod(curr_element->texture_r, 255);
   if (curr_element->texture_s)
      SDL_SetTextureAlphaMod(curr_element->texture_s, 255);
   if (curr_element->texture_rs)
      SDL_SetTextureAlphaMod(curr_element->texture_rs, 255);
   if (curr_element->texture_l)
      SDL_SetTextureAlphaMod(curr_element->texture_l, 255);
   if (curr_element->texture_w)
      SDL_SetTextureAlphaMod(curr_element->texture_w, 255);
   if (curr_element->texture_p)
      SDL_SetTextureAlphaMod(curr_element->texture_p, 255);

   /* For text elements, also reset the font color alpha */
   if (curr_element->type == TEXT) {
      curr_element->font_color.a = 255;
   }
}

static void calculate_zoom_rect(SDL_Rect *dst_rect_l, SDL_Rect *dst_rect_r, float scale) {
   int center_x_l = dst_rect_l->x + (dst_rect_l->w / 2);
   int center_y_l = dst_rect_l->y + (dst_rect_l->h / 2);
   int center_x_r = dst_rect_r->x + (dst_rect_r->w / 2);
   int center_y_r = dst_rect_r->y + (dst_rect_r->h / 2);

   /* Apply scale, currently used for zoom effect */
   dst_rect_l->w = dst_rect_r->w = (int)round((float)dst_rect_l->w * scale);
   dst_rect_l->h = dst_rect_r->h = (int)round((float)dst_rect_l->h * scale);
   dst_rect_l->x = center_x_l - (dst_rect_l->w / 2);
   dst_rect_l->y = center_y_l - (dst_rect_l->h / 2);
   dst_rect_r->x = center_x_r - (dst_rect_r->w / 2);
   dst_rect_r->y = center_y_r - (dst_rect_r->h / 2);
}

/**
 * Element renderer implementation
 * 
 * This file contains the implementation of all element rendering functions.
 * The functions handle the rendering of different element types (static, animated,
 * text, special) with various effects (alpha blending, sliding, scaling) for
 * HUD transitions.
 */

/* Render a static element */
void render_static_element(element *curr_element) {
   SDL_Rect dst_rect_l, dst_rect_r;
   SDL_Texture *this_texture = NULL;
   hud_display_settings *this_hds = get_hud_display_settings();
   motion *this_motion = get_motion_dev();
   const char *aiState = get_ai_state();

   dst_rect_l.x = dst_rect_r.x = curr_element->dst_rect.x;
   dst_rect_l.y = dst_rect_r.y = curr_element->dst_rect.y;
   dst_rect_l.w = dst_rect_r.w = curr_element->dst_rect.w;
   dst_rect_l.h = dst_rect_r.h = curr_element->dst_rect.h;

   /* Apply fixed/stereo offset */
   if (!curr_element->fixed) {
      dst_rect_l.x -= this_hds->stereo_offset;
      dst_rect_r.x += this_hds->stereo_offset;
   }

   /* Select appropriate texture based on state */
   if (get_recording_started() && (get_recording_state() == RECORD_STREAM) && curr_element->texture_rs) {
      this_texture = curr_element->texture_rs;
   } else if (get_recording_started() && (get_recording_state() == RECORD) && curr_element->texture_r) {
      this_texture = curr_element->texture_r;
   } else if (get_recording_started() && (get_recording_state() == STREAM) && curr_element->texture_s) {
      this_texture = curr_element->texture_s;
   } else if (curr_element->texture_l && strcmp("SILENCE", aiState) == 0) {
      this_texture = curr_element->texture_l;
   } else if (curr_element->texture_w && strcmp("WAKEWORD_LISTEN", aiState) == 0) {
      this_texture = curr_element->texture_w;
   } else if (curr_element->texture_l && strcmp("COMMAND_RECORDING", aiState) == 0) {
      this_texture = curr_element->texture_l;
   } else if (curr_element->texture_p && strcmp("PROCESS_COMMAND", aiState) == 0) {
      this_texture = curr_element->texture_p;
   } else if (curr_element->texture_p && strcmp("VISION_AI_READY", aiState) == 0) {
      this_texture = curr_element->texture_p;
   } else {
      this_texture = curr_element->texture;
   }

   /* Apply scale, currently used for zoom effect */
   calculate_zoom_rect(&dst_rect_l, &dst_rect_r, curr_element->scale);

   /* Render the element */
   if (this_texture != NULL) {
      if (curr_element->angle == ANGLE_OPPOSITE_ROLL) {
         renderStereo(this_texture, NULL, &dst_rect_l, &dst_rect_r, -1.0 * this_motion->roll);
      } else if (curr_element->angle == ANGLE_ROLL) {
         renderStereo(this_texture, NULL, &dst_rect_l, &dst_rect_r, this_motion->roll);
      } else {
         renderStereo(this_texture, NULL, &dst_rect_l, &dst_rect_r, curr_element->angle);
      }
   }
}

/* Render an animated element */
void render_animated_element(element *curr_element) {
   SDL_Rect src_rect;
   SDL_Rect dst_rect_l, dst_rect_r;
   SDL_Texture *this_texture = NULL;
   double ratio = 0.0;
   hud_display_settings *this_hds = get_hud_display_settings();
   motion *this_motion = get_motion_dev();

   /* Set up source rectangle from animation frame */
   src_rect.x = curr_element->this_anim.current_frame->source_x;
   src_rect.y = curr_element->this_anim.current_frame->source_y;
   src_rect.w = curr_element->this_anim.current_frame->source_w;
   src_rect.h = curr_element->this_anim.current_frame->source_h;

   /* Set up destination rectangle based on animation properties */
   if ((curr_element->width == 0) && (curr_element->height == 0)) {
      dst_rect_l.x = dst_rect_r.x =
         curr_element->dest_x + curr_element->this_anim.current_frame->dest_x;
      dst_rect_l.y = dst_rect_r.y =
         curr_element->dest_y + curr_element->this_anim.current_frame->dest_y;
      dst_rect_l.w = dst_rect_r.w = curr_element->this_anim.current_frame->source_w;
      dst_rect_l.h = dst_rect_r.h = curr_element->this_anim.current_frame->source_h;
   } else if ((curr_element->width == 0) && (curr_element->height != 0)) {
      ratio = (double) curr_element->height /
              (double) curr_element->this_anim.current_frame->source_size_h;

      dst_rect_l.x = dst_rect_r.x =
         curr_element->dest_x + (curr_element->this_anim.current_frame->dest_x * ratio);
      dst_rect_l.y = dst_rect_r.y =
         curr_element->dest_y + (curr_element->this_anim.current_frame->dest_y * ratio);

      dst_rect_l.w = dst_rect_r.w =
         curr_element->this_anim.current_frame->source_w * ratio;
      dst_rect_l.h = dst_rect_r.h =
         curr_element->height - (curr_element->this_anim.current_frame->dest_y * ratio);
   } else if ((curr_element->width != 0) && (curr_element->height == 0)) {
      ratio = (double) curr_element->width /
              (double) curr_element->this_anim.current_frame->source_size_w;

      dst_rect_l.x = dst_rect_r.x =
         curr_element->dest_x + (curr_element->this_anim.current_frame->dest_x * ratio);
      dst_rect_l.y = dst_rect_r.y =
         curr_element->dest_y + (curr_element->this_anim.current_frame->dest_y * ratio);

      dst_rect_l.w = dst_rect_r.w =
         curr_element->width - (curr_element->this_anim.current_frame->dest_x * ratio);
      dst_rect_l.h = dst_rect_r.h =
         curr_element->this_anim.current_frame->source_h * ratio;
   } else {
      /* Both width and height specified */
      /* Width Ratio */
      ratio = (double) curr_element->width /
              (double) curr_element->this_anim.current_frame->source_size_w;

      dst_rect_l.x = dst_rect_r.x =
         curr_element->dest_x + (curr_element->this_anim.current_frame->dest_x * ratio);
      dst_rect_l.w = dst_rect_r.w =
         curr_element->width - (curr_element->this_anim.current_frame->dest_x * ratio);

      /* Height Ratio */
      ratio = (double) curr_element->height /
              (double) curr_element->this_anim.current_frame->source_size_h;

      dst_rect_l.y = dst_rect_r.y =
         curr_element->dest_y + (curr_element->this_anim.current_frame->dest_y * ratio);
      dst_rect_l.h = dst_rect_r.h =
         curr_element->height - (curr_element->this_anim.current_frame->dest_y * ratio);
   }

   /* Apply fixed/stereo offset */
   if (!curr_element->fixed) {
      dst_rect_l.x -= this_hds->stereo_offset;
      dst_rect_r.x += this_hds->stereo_offset;
   }

   /* Apply scale, currently used for zoom effect */
   calculate_zoom_rect(&dst_rect_l, &dst_rect_r, curr_element->scale);

   /* Select the appropriate texture */
   this_texture = curr_element->texture; /* For animated, we usually only have one texture option */

   /* Render the element */
   if (this_texture != NULL) {
      if (curr_element->angle == ANGLE_OPPOSITE_ROLL) {
         renderStereo(this_texture, &src_rect, &dst_rect_l, &dst_rect_r, -1.0 * this_motion->roll);
      } else if (curr_element->angle == ANGLE_ROLL) {
         renderStereo(this_texture, &src_rect, &dst_rect_l, &dst_rect_r, this_motion->roll);
      } else {
         renderStereo(this_texture, &src_rect, &dst_rect_l, &dst_rect_r, curr_element->angle);
      }
   }

   /* Animation frame update logic */
   Uint32 currTime = SDL_GetTicks();
   float dT = (currTime - curr_element->this_anim.last_update) / 1000.0f;
   int curr_fps = get_curr_fps();
   int framesToUpdate = floor(dT / (1.0f / curr_fps));
   if (framesToUpdate > 0) {
      if ((currTime %
         (int)ceil((double)curr_fps / curr_element->this_anim.frame_count)) == 0) {
         if (curr_element->this_anim.current_frame->next != NULL) {
             curr_element->this_anim.current_frame =
                curr_element->this_anim.current_frame->next;
         } else {
             curr_element->this_anim.current_frame = curr_element->this_anim.first_frame;
         }
      }
      curr_element->this_anim.last_update = currTime;
   }
}

/* Render a text element */
void render_text_element(element *curr_element) {
   static int cpu_thread_started = 0;
   static pthread_t cpu_util_thread = 0;
   SDL_Rect dst_rect_l, dst_rect_r;
   hud_display_settings *this_hds = get_hud_display_settings();
   char render_text[MAX_TEXT_LENGTH] = "";
   static FILE *fan_file = NULL;
   unsigned int currTime = SDL_GetTicks();
   int override_dst_rect = curr_element->in_transition;
   float alpha_override = curr_element->transition_alpha;
   static unsigned int last_log = 0;
   char (*raw_log)[LOG_LINE_LENGTH] = get_raw_log();
   motion *this_motion = get_motion_dev();
   enviro *this_enviro = get_enviro_dev();
   gps *this_gps = get_gps_dev();

   // Initialize first pass.
   if (last_log == 0) {
      last_log = currTime;
   }

   /* Process dynamic text content */
   if (strcmp("*FPS*", curr_element->text) == 0) {
      /* FPS display */
      snprintf(render_text, MAX_TEXT_LENGTH, "Current FPS: %d", (int)averageFrameRate);
   } else if (strcmp("*DATETIME*", curr_element->text) == 0) {
      /* Date/time display */
      snprintf(render_text, MAX_TEXT_LENGTH, "%s %s", this_gps->date, this_gps->time);
   } else if (strcmp("*GPSTIME*", curr_element->text) == 0) {
      /* TODO: Use Google API to convert GPS location into correct local time. */
      /* https://maps.googleapis.com/maps/api/timezone/json?language=es&location=39.6034810%2C-119.6822510&timestamp=1331766000&key=GoogleAPIKey */
      snprintf(render_text, MAX_TEXT_LENGTH, "%s", this_gps->time);
   } else if (strcmp("*SYSTIME*", curr_element->text) == 0) {
      time_t stime;
      struct tm *ltime;
      stime = time(NULL);
      ltime = localtime(&stime);
      snprintf(render_text, MAX_TEXT_LENGTH, "%02d:%02d:%02d",
              ltime->tm_hour, ltime->tm_min, ltime->tm_sec);
   } else if (strcmp("*AINAME*", curr_element->text) == 0) {
      snprintf(render_text, MAX_TEXT_LENGTH, "%s", get_ai_name());
   } else if (strcmp("*CPU*", curr_element->text) == 0) {
      if (cpu_thread_started == 0) {
         if (pthread_create(&cpu_util_thread, NULL, cpu_utilization_thread, NULL) != 0) {
            LOG_ERROR("Error creating cpu utilization thread.");
            cpu_thread_started = 0;
         } else {
            cpu_thread_started = 1;
         }
      }

      snprintf(render_text, MAX_TEXT_LENGTH, "%03.0Lf", get_loadavg());
   } else if (strcmp("*MEM*", curr_element->text) == 0) {
      snprintf(render_text, MAX_TEXT_LENGTH, "%03.0Lf", get_mem_usage());
   } else if (strcmp("*HELMTEMP*", curr_element->text) == 0) {
      snprintf(render_text, MAX_TEXT_LENGTH, "%0.1f C", this_enviro->temp);
   } else if (strcmp("*HELMTEMP_F*", curr_element->text) == 0) {
      snprintf(render_text, MAX_TEXT_LENGTH, "%03.0f F", this_enviro->temp * 9/5 + 32.0);
   } else if (strcmp("*HELMHUM*", curr_element->text) == 0) {
      snprintf(render_text, MAX_TEXT_LENGTH, "%03.0f", this_enviro->humidity);

   } else if (strcmp("*AIRQUALITY*", curr_element->text) == 0) {
      snprintf(render_text, MAX_TEXT_LENGTH, "%03.0f", this_enviro->air_quality);
   } else if (strcmp("*AIRQUALITYDESC*", curr_element->text) == 0) {
      snprintf(render_text, MAX_TEXT_LENGTH, "%s", this_enviro->air_quality_description);
   } else if (strcmp("*TVOC*", curr_element->text) == 0) {
      snprintf(render_text, MAX_TEXT_LENGTH, "%03.0f", this_enviro->tvoc_ppb);
   } else if (strcmp("*ECO2*", curr_element->text) == 0) {
      snprintf(render_text, MAX_TEXT_LENGTH, "%03.0f", this_enviro->eco2_ppm);
   } else if (strcmp("*CO2*", curr_element->text) == 0) {
      snprintf(render_text, MAX_TEXT_LENGTH, "%03.0f", this_enviro->co2_ppm);
   } else if (strcmp("*CO2QUALITY*", curr_element->text) == 0) {
      snprintf(render_text, MAX_TEXT_LENGTH, "%s", this_enviro->co2_quality_description);
   } else if (strcmp("*CO2ECO2DIFF*", curr_element->text) == 0) {
      snprintf(render_text, MAX_TEXT_LENGTH, "%03d", this_enviro->co2_eco2_diff);
   } else if (strcmp("*CO2SOURCEANALYSIS*", curr_element->text) == 0) {
      snprintf(render_text, MAX_TEXT_LENGTH, "%s", this_enviro->co2_source_analysis);
   } else if (strcmp("*HEATINDEX_C*", curr_element->text) == 0) {
      snprintf(render_text, MAX_TEXT_LENGTH, "%0.1f", this_enviro->heat_index_c);
   } else if (strcmp("*DEWPOINT*", curr_element->text) == 0) {
      snprintf(render_text, MAX_TEXT_LENGTH, "%0.1f", this_enviro->dew_point);

   } else if (strcmp("*FAN*", curr_element->text) == 0) {
      // Initialize fan monitoring if not already done
      if (fan_file == NULL) {
         init_fan_monitoring();
      }

      // Get fan percentage and format text
      int fan_percent = get_fan_load_percent();
      snprintf(render_text, MAX_TEXT_LENGTH, "%03d", fan_percent);
   } else if (strcmp("*LATLON*", curr_element->text) == 0) {
      if (this_gps->latitudeDegrees != 0.0) {
         snprintf(render_text, MAX_TEXT_LENGTH, "%0.02f, %0.02f",
                 this_gps->latitudeDegrees, this_gps->longitudeDegrees);
      } else {
         snprintf(render_text, MAX_TEXT_LENGTH, "%0.02f%s, %0.02f%s",
                 this_gps->latitude, this_gps->lat, this_gps->longitude, this_gps->lon);
      }
   } else if (strcmp("*PITCH*", curr_element->text) == 0) {
      snprintf(render_text, MAX_TEXT_LENGTH, "%d", (int)this_motion->pitch + (int)this_hds->pitch_offset);
   } else if (strcmp("*COMPASS*", curr_element->text) == 0) {
      if ((this_motion->heading > 337.5) || (this_motion->heading <= 22.5)) {
         snprintf(render_text, MAX_TEXT_LENGTH, "%s", "N");
      } else if ((this_motion->heading > 22.5) && (this_motion->heading <= 67.5)) {
         snprintf(render_text, MAX_TEXT_LENGTH, "%s", "NE");
      } else if ((this_motion->heading > 67.5) && (this_motion->heading <= 112.5)) {
         snprintf(render_text, MAX_TEXT_LENGTH, "%s", "E");
      } else if ((this_motion->heading > 112.5) && (this_motion->heading <= 157.5)) {
         snprintf(render_text, MAX_TEXT_LENGTH, "%s", "SE");
      } else if ((this_motion->heading > 157.5) && (this_motion->heading <= 202.5)) {
         snprintf(render_text, MAX_TEXT_LENGTH, "%s", "S");
      } else if ((this_motion->heading > 202.5) && (this_motion->heading <= 247.5)) {
         snprintf(render_text, MAX_TEXT_LENGTH, "%s", "SW");
      } else if ((this_motion->heading > 247.5) && (this_motion->heading <= 292.5)) {
         snprintf(render_text, MAX_TEXT_LENGTH, "%s", "W");
      } else if ((this_motion->heading > 292.5) && (this_motion->heading <= 337.5)) {
         snprintf(render_text, MAX_TEXT_LENGTH, "%s", "NW");
      }
   } else if (strcmp("*LOG*", curr_element->text) == 0) {
      if ((currTime - last_log) > 500) {
         last_log = currTime;
         if (curr_element->texture != NULL) {
            SDL_DestroyTexture(curr_element->texture);
            curr_element->texture = NULL;
         }
         if (curr_element->surface != NULL) {
            SDL_FreeSurface(curr_element->surface);
            curr_element->surface = NULL;
         }

         curr_element->surface = SDL_CreateRGBSurface(0, 615, 345, 32, 0, 0, 0, 0);
            SDL_SetColorKey(curr_element->surface, SDL_TRUE,
            SDL_MapRGB(curr_element->surface->format, 0, 0,
            SDL_ALPHA_TRANSPARENT));

         for (int ii = 0; ii < LOG_ROWS; ii++) {
            if (strcmp("", raw_log[ii]) != 0) {
               SDL_Surface *tmpsfc = NULL;
               SDL_Rect tmprect;

               tmpsfc = TTF_RenderText_Blended(curr_element->ttf_font, raw_log[ii],
                                               curr_element->font_color);
               if (tmpsfc != NULL) {
                  tmprect.x = 0;
                  tmprect.y = ii * curr_element->font_size;
                  tmprect.w = tmpsfc->w;
                  tmprect.h = tmpsfc->h;

                  SDL_BlitSurface(tmpsfc, NULL, curr_element->surface, &tmprect);
                  SDL_FreeSurface(tmpsfc);
               } else {
                  LOG_ERROR("Error creating log render, %d.", ii);
               }
            }
         }
      }
   } else if (strcmp("*ALERT*", curr_element->text) == 0) {
      char alert_text[MAX_TEXT_LENGTH] = "";

      for (int k = 0; k < ALERT_MAX; k++) {
         if (active_alerts & alert_messages[k].flag) {
            /* TODO: Bounds checking */
            strcat(alert_text, alert_messages[k].message);
         }
      }

      snprintf(render_text, MAX_TEXT_LENGTH, "%s", alert_text);
      alert_text[0] = '\0';
   } else {
      strncpy(render_text, curr_element->text, MAX_TEXT_LENGTH-1);
      render_text[MAX_TEXT_LENGTH-1] = '\0';
   }

   /* Recreate texture if needed */
   if (curr_element->texture == NULL ||
      render_text[0] != '\0') { /* Don't recreate for static text */

      /* Clean up old texture and surface if they exist */
      if (curr_element->texture != NULL) {
         SDL_DestroyTexture(curr_element->texture);
         curr_element->texture = NULL;
      }

      if (curr_element->surface != NULL) {
         SDL_FreeSurface(curr_element->surface);
         curr_element->surface = NULL;
      }

      /* Create new text surface and texture */
      if (strlen(render_text) == 0) {
         strcpy(render_text, " "); /* Prevent empty text */
      }

      curr_element->surface = TTF_RenderText_Blended(
         curr_element->ttf_font, render_text, curr_element->font_color);

      if (curr_element->surface != NULL) {
         curr_element->dst_rect.w = curr_element->surface->w;
         curr_element->dst_rect.h = curr_element->surface->h;

         /* Only recalculate position if not in transition */
         if (!override_dst_rect) {
            if (strcmp("left", curr_element->halign) == 0) {
               curr_element->dst_rect.x = curr_element->dest_x;
               curr_element->dst_rect.y = curr_element->dest_y;
            } else if (strcmp("center", curr_element->halign) == 0) {
               curr_element->dst_rect.x = curr_element->dest_x - (curr_element->dst_rect.w / 2);
               curr_element->dst_rect.y = curr_element->dest_y;
            } else if (strcmp("right", curr_element->halign) == 0) {
               curr_element->dst_rect.x = curr_element->dest_x - curr_element->dst_rect.w;
               curr_element->dst_rect.y = curr_element->dest_y;
            }
         }

         if (curr_element->surface != NULL && alpha_override > 0.0f) {
            // Apply alpha to the surface color
            curr_element->font_color.a = (Uint8)(alpha_override * 255);
         }

         curr_element->texture = SDL_CreateTextureFromSurface(get_sdl_renderer(), curr_element->surface);

         /* Set alpha on new texture if needed */
         if (curr_element->texture != NULL && alpha_override > 0.0f) {
            SDL_SetTextureAlphaMod(curr_element->texture, (Uint8)(alpha_override * 255));
         }

         if (curr_element->texture == NULL) {
            LOG_ERROR("SDL_CreateTextureFromSurface failed: %s", SDL_GetError());
         }
      }
   }

   /* Set up render coordinates */
   dst_rect_l.x = dst_rect_r.x = curr_element->dst_rect.x;
   dst_rect_l.y = dst_rect_r.y = curr_element->dst_rect.y;
   dst_rect_l.w = dst_rect_r.w = curr_element->dst_rect.w;
   dst_rect_l.h = dst_rect_r.h = curr_element->dst_rect.h;

   /* Apply fixed/stereo offset */
   if (!curr_element->fixed) {
      dst_rect_l.x -= this_hds->stereo_offset;
      dst_rect_r.x += this_hds->stereo_offset;
   }

   /* Apply scale, currently used for zoom effect */
   calculate_zoom_rect(&dst_rect_l, &dst_rect_r, curr_element->scale);

   /* Render the text */
   if (curr_element->texture != NULL) {
      if (curr_element->angle == ANGLE_OPPOSITE_ROLL) {
         renderStereo(curr_element->texture, NULL, &dst_rect_l, &dst_rect_r, -1.0 * this_motion->roll);
      } else if (curr_element->angle == ANGLE_ROLL) {
         renderStereo(curr_element->texture, NULL, &dst_rect_l, &dst_rect_r, this_motion->roll);
      } else {
         renderStereo(curr_element->texture, NULL, &dst_rect_l, &dst_rect_r, curr_element->angle);
      }
   }
}

/* Forward declarations for special element types */
void render_map_element(element *curr_element);
void render_pitch_element(element *curr_element);
void render_heading_element(element *curr_element);
void render_altitude_element(element *curr_element);
void render_wifi_element(element *curr_element);
void render_detect_element(element *curr_element);

/* Render a special element - dispatch to specific handlers */
void render_special_element(element *curr_element) {
   if (strcmp("map", curr_element->special_name) == 0) {
      render_map_element(curr_element);
   } else if (strcmp("pitch", curr_element->special_name) == 0) {
      render_pitch_element(curr_element);
   } else if (strcmp("heading", curr_element->special_name) == 0) {
      render_heading_element(curr_element);
   } else if (strcmp("altitude", curr_element->special_name) == 0) {
      render_altitude_element(curr_element);
   } else if (strcmp("wifi", curr_element->special_name) == 0) {
      render_wifi_element(curr_element);
   } else if (strcmp("detect", curr_element->special_name) == 0) {
     if (detect_enabled) {
        render_detect_element(curr_element);
     }
   } else if (strcmp("armor_display", curr_element->name) == 0) {
      render_armor_display_element(curr_element);
   } else {
     LOG_ERROR("Unknown special element type: %s", curr_element->special_name);
   }
}

void render_map_element(element *curr_element) {
   static pthread_t map_download_thread;
   static int map_thread_started = 0;
   static struct curl_data map_data = {0};

   SDL_Rect dst_rect_l, dst_rect_r;
   SDL_Texture *this_texture = NULL;
   hud_display_settings *this_hds = get_hud_display_settings();
   double lat = 0, lon = 0;

   SDL_Renderer *renderer = get_sdl_renderer();
   motion *this_motion = get_motion_dev();
   gps *this_gps = get_gps_dev();

   /* Get GPS coordinates */
   if (this_gps->latitudeDegrees != 0.0) {
      lat = this_gps->latitudeDegrees;
   } else {
      if (strcmp(this_gps->lat, "S") == 0) {
         lat = this_gps->latitude * -1;
      } else {
         lat = this_gps->latitude;
      }
   }

   if (this_gps->longitudeDegrees != 0.0) {
      lon = this_gps->longitudeDegrees;
   } else {
      if (strcmp(this_gps->lon, "W") == 0) {
         lon = this_gps->longitude * -1;
      } else {
         lon = this_gps->longitude;
      }
   }

   /* Use default location if no GPS data available */
   if ((lat == 0) && (lon == 0)) {
      lat = DEFAULT_LATITUDE;
      lon = DEFAULT_LONGITUDE;
   }

   // Initialize mutex on first call
   // TODO: We need to pthread_mutex_destroy(&map_data.mutex); somewheres.
   if (map_thread_started == 0) {
      pthread_mutex_init(&map_data.mutex, NULL);
   }

   /* Update map URL and start download thread if needed */
   snprintf(map_data.url, 512, GOOGLE_MAPS_API, lat, lon,
            curr_element->width, curr_element->height,
            lat, lon, GOOGLE_API_KEY);

   if (map_thread_started == 0) {
      map_data.update_interval_sec = MAP_UPDATE_SEC;
      map_data.download_count = curr_element->download_count;
      map_data.updated = 0;
      map_data.size = 0;
      map_data.data = NULL;

      if (pthread_create(&map_download_thread, NULL, image_download_thread, &map_data) != 0) {
         LOG_ERROR("Error creating map download thread.");
         map_thread_started = 0;
      } else {
         map_thread_started = 1;
      }
   }

   // Check for new map data with mutex protection
   pthread_mutex_lock(&map_data.mutex);

   /* Update texture if new map data available */
   if (map_data.updated && map_data.data != NULL) {
      // We have new map data, convert to surface and texture
      //
      SDL_RWops *rw = SDL_RWFromMem(map_data.data, map_data.size);
      if (rw != NULL) {
         // Create the surface in the main thread
         SDL_Surface *new_surface = IMG_Load_RW(rw, 0);
         SDL_RWclose(rw);

         if (new_surface != NULL) {
            // Update the element's texture
            if (curr_element->texture != NULL) {
               SDL_DestroyTexture(curr_element->texture);
               curr_element->texture = NULL;
            }

            curr_element->dst_rect.w = new_surface->w;
            curr_element->dst_rect.h = new_surface->h;
            curr_element->dst_rect.x = curr_element->dest_x;
            curr_element->dst_rect.y = curr_element->dest_y;
            curr_element->texture = SDL_CreateTextureFromSurface(renderer, new_surface);

            // Clean up
            SDL_FreeSurface(new_surface);
         }
      }

      // Mark as consumed
      map_data.updated = 0;
   }

   pthread_mutex_unlock(&map_data.mutex);

   /* Set up destination rectangles */
   dst_rect_l.x = dst_rect_r.x = curr_element->dst_rect.x;
   dst_rect_l.y = dst_rect_r.y = curr_element->dst_rect.y;
   dst_rect_l.w = dst_rect_r.w = curr_element->dst_rect.w;
   dst_rect_l.h = dst_rect_r.h = curr_element->dst_rect.h;

   /* Apply stereo offset if not fixed */
   if (!curr_element->fixed) {
      dst_rect_l.x -= this_hds->stereo_offset;
      dst_rect_r.x += this_hds->stereo_offset;
   }

   /* Apply scale, currently used for zoom effect */
   calculate_zoom_rect(&dst_rect_l, &dst_rect_r, curr_element->scale);

   /* Render the map */
   this_texture = curr_element->texture;
   if (this_texture != NULL) {
      if (curr_element->angle == ANGLE_OPPOSITE_ROLL) {
         renderStereo(this_texture, NULL, &dst_rect_l, &dst_rect_r, -1.0 * this_motion->roll);
      } else if (curr_element->angle == ANGLE_ROLL) {
         renderStereo(this_texture, NULL, &dst_rect_l, &dst_rect_r, this_motion->roll);
      } else {
         renderStereo(this_texture, NULL, &dst_rect_l, &dst_rect_r, curr_element->angle);
      }
   }
}

void render_pitch_element(element *curr_element) {
    SDL_Rect src_rect;
    SDL_Rect dst_rect_l, dst_rect_r;
    SDL_Texture *this_texture = NULL;
    hud_display_settings *this_hds = get_hud_display_settings();
    motion *this_motion = get_motion_dev();

    /* Select animation frame based on pitch value */
    int frame_index = (int)round((this_motion->pitch + 90.0 + this_hds->pitch_offset) * 2.0);

    /* Clamp frame index to valid range */
    if (frame_index < 0) frame_index = 0;
    if (frame_index >= curr_element->this_anim.frame_count)
        frame_index = curr_element->this_anim.frame_count - 1;

    /* Get the frame */
    curr_element->this_anim.current_frame = curr_element->this_anim.frame_lookup[frame_index];

    /* Set up source rectangle */
    src_rect.x = curr_element->this_anim.current_frame->source_x;
    src_rect.y = curr_element->this_anim.current_frame->source_y;
    src_rect.w = curr_element->this_anim.current_frame->source_w;
    src_rect.h = curr_element->this_anim.current_frame->source_h;

    /* Set up destination rectangles */
    dst_rect_l.x = dst_rect_r.x =
        curr_element->dest_x + curr_element->this_anim.current_frame->dest_x;
    dst_rect_l.y = dst_rect_r.y =
        curr_element->dest_y + curr_element->this_anim.current_frame->dest_y;
    dst_rect_l.w = dst_rect_r.w = curr_element->this_anim.current_frame->source_w;
    dst_rect_l.h = dst_rect_r.h = curr_element->this_anim.current_frame->source_h;

    /* Apply stereo offset if not fixed */
    if (!curr_element->fixed) {
        dst_rect_l.x -= this_hds->stereo_offset;
        dst_rect_r.x += this_hds->stereo_offset;
    }

   /* Apply scale, currently used for zoom effect */
   calculate_zoom_rect(&dst_rect_l, &dst_rect_r, curr_element->scale);

    /* Render the pitch element */
    this_texture = curr_element->texture;
    if (this_texture != NULL) {
        if (curr_element->angle == ANGLE_OPPOSITE_ROLL) {
            renderStereo(this_texture, &src_rect, &dst_rect_l, &dst_rect_r, -1.0 * this_motion->roll);
        } else if (curr_element->angle == ANGLE_ROLL) {
            renderStereo(this_texture, &src_rect, &dst_rect_l, &dst_rect_r, this_motion->roll);
        } else {
            renderStereo(this_texture, &src_rect, &dst_rect_l, &dst_rect_r, curr_element->angle);
        }
    }
}

void render_heading_element(element *curr_element) {
    SDL_Rect src_rect;
    SDL_Rect dst_rect_l, dst_rect_r;
    SDL_Texture *this_texture = NULL;
    hud_display_settings *this_hds = get_hud_display_settings();
    motion *this_motion = get_motion_dev();

    /* Select animation frame based on heading value */
    int frame_index = (int)this_motion->heading;

    /* Clamp frame index to valid range */
    if (frame_index < 0) frame_index = 0;
    if (frame_index >= 360) frame_index = 359;
    if (frame_index >= curr_element->this_anim.frame_count)
        frame_index = curr_element->this_anim.frame_count - 1;

    /* Get the frame */
    curr_element->this_anim.current_frame = curr_element->this_anim.frame_lookup[frame_index];

    /* Set up source rectangle */
    src_rect.x = curr_element->this_anim.current_frame->source_x;
    src_rect.y = curr_element->this_anim.current_frame->source_y;
    src_rect.w = curr_element->this_anim.current_frame->source_w;
    src_rect.h = curr_element->this_anim.current_frame->source_h;

    /* Set up destination rectangles */
    dst_rect_l.x = dst_rect_r.x =
        curr_element->dest_x + curr_element->this_anim.current_frame->dest_x;
    dst_rect_l.y = dst_rect_r.y =
        curr_element->dest_y + curr_element->this_anim.current_frame->dest_y;
    dst_rect_l.w = dst_rect_r.w = curr_element->this_anim.current_frame->source_w;
    dst_rect_l.h = dst_rect_r.h = curr_element->this_anim.current_frame->source_h;

    /* Apply stereo offset if not fixed */
    if (!curr_element->fixed) {
        dst_rect_l.x -= this_hds->stereo_offset;
        dst_rect_r.x += this_hds->stereo_offset;
    }

   /* Apply scale, currently used for zoom effect */
   calculate_zoom_rect(&dst_rect_l, &dst_rect_r, curr_element->scale);

    /* Render the heading element */
    this_texture = curr_element->texture;
    if (this_texture != NULL) {
        if (curr_element->angle == ANGLE_OPPOSITE_ROLL) {
            renderStereo(this_texture, &src_rect, &dst_rect_l, &dst_rect_r, -1.0 * this_motion->roll);
        } else if (curr_element->angle == ANGLE_ROLL) {
            renderStereo(this_texture, &src_rect, &dst_rect_l, &dst_rect_r, this_motion->roll);
        } else {
            renderStereo(this_texture, &src_rect, &dst_rect_l, &dst_rect_r, curr_element->angle);
        }
    }
}

void render_altitude_element(element *curr_element) {
   SDL_Rect src_rect;
   SDL_Rect dst_rect_l, dst_rect_r;
   SDL_Texture *this_texture = NULL;
   hud_display_settings *this_hds = get_hud_display_settings();
   motion *this_motion = get_motion_dev();
   gps *this_gps = get_gps_dev();

    /* Select animation frame based on altitude value (in multiples of 10) */
    int altitude = (int)this_gps->altitude;

    /* Clamp altitude to valid range */
    if (altitude < 0) altitude = 0;
    if (altitude >= curr_element->this_anim.frame_count * 10)
        altitude = (curr_element->this_anim.frame_count * 10) - 1;

    /* Calculate frame index (altitude is in multiples of 10) */
    int frame_index = altitude / 10;

    /* Get the frame */
    curr_element->this_anim.current_frame = curr_element->this_anim.frame_lookup[frame_index];

    /* Set up source rectangle */
    src_rect.x = curr_element->this_anim.current_frame->source_x;
    src_rect.y = curr_element->this_anim.current_frame->source_y;
    src_rect.w = curr_element->this_anim.current_frame->source_w;
    src_rect.h = curr_element->this_anim.current_frame->source_h;

    /* Set up destination rectangles */
    dst_rect_l.x = dst_rect_r.x =
        curr_element->dest_x + curr_element->this_anim.current_frame->dest_x;
    dst_rect_l.y = dst_rect_r.y =
        curr_element->dest_y + curr_element->this_anim.current_frame->dest_y;
    dst_rect_l.w = dst_rect_r.w = curr_element->this_anim.current_frame->source_w;
    dst_rect_l.h = dst_rect_r.h = curr_element->this_anim.current_frame->source_h;

    /* Apply stereo offset if not fixed */
    if (!curr_element->fixed) {
        dst_rect_l.x -= this_hds->stereo_offset;
        dst_rect_r.x += this_hds->stereo_offset;
    }

   /* Apply scale, currently used for zoom effect */
   calculate_zoom_rect(&dst_rect_l, &dst_rect_r, curr_element->scale);

    /* Render the altitude element */
    this_texture = curr_element->texture;
    if (this_texture != NULL) {
        if (curr_element->angle == ANGLE_OPPOSITE_ROLL) {
            renderStereo(this_texture, &src_rect, &dst_rect_l, &dst_rect_r, -1.0 * this_motion->roll);
        } else if (curr_element->angle == ANGLE_ROLL) {
            renderStereo(this_texture, &src_rect, &dst_rect_l, &dst_rect_r, this_motion->roll);
        } else {
            renderStereo(this_texture, &src_rect, &dst_rect_l, &dst_rect_r, curr_element->angle);
        }
    }
}

void render_wifi_element(element *curr_element) {
   SDL_Rect src_rect;
   SDL_Rect dst_rect_l, dst_rect_r;
   SDL_Texture *this_texture = NULL;
   hud_display_settings *this_hds = get_hud_display_settings();
   motion *this_motion = get_motion_dev();

   /* Get wifi signal level (0-9) */
   int signal_level = get_wifi_signal_level();

   /* Clamp signal level to valid range for animation frames */
   if (signal_level < 0) signal_level = 0;
   if (signal_level >= curr_element->this_anim.frame_count)
      signal_level = curr_element->this_anim.frame_count - 1;

   /* Get the frame */
   curr_element->this_anim.current_frame = curr_element->this_anim.frame_lookup[signal_level];

   /* Set up source rectangle */
   src_rect.x = curr_element->this_anim.current_frame->source_x;
   src_rect.y = curr_element->this_anim.current_frame->source_y;
   src_rect.w = curr_element->this_anim.current_frame->source_w;
   src_rect.h = curr_element->this_anim.current_frame->source_h;

   /* Normal rendering, calculate from original position */
   dst_rect_l.x = dst_rect_r.x = curr_element->dest_x + curr_element->this_anim.current_frame->dest_x;
   dst_rect_l.y = dst_rect_r.y = curr_element->dest_y + curr_element->this_anim.current_frame->dest_y;
   dst_rect_l.w = dst_rect_r.w = curr_element->this_anim.current_frame->source_w;
   dst_rect_l.h = dst_rect_r.h = curr_element->this_anim.current_frame->source_h;

   /* Apply stereo offset if not fixed */
   if (!curr_element->fixed) {
      dst_rect_l.x -= this_hds->stereo_offset;
      dst_rect_r.x += this_hds->stereo_offset;
   }

   /* Apply scale, currently used for zoom effect */
   calculate_zoom_rect(&dst_rect_l, &dst_rect_r, curr_element->scale);

   /* Render the wifi element */
   this_texture = curr_element->texture;
   if (this_texture != NULL) {
      if (curr_element->angle == ANGLE_OPPOSITE_ROLL) {
         renderStereo(this_texture, &src_rect, &dst_rect_l, &dst_rect_r, -1.0 * this_motion->roll);
      } else if (curr_element->angle == ANGLE_ROLL) {
         renderStereo(this_texture, &src_rect, &dst_rect_l, &dst_rect_r, this_motion->roll);
      } else {
         renderStereo(this_texture, &src_rect, &dst_rect_l, &dst_rect_r, curr_element->angle);
      }
   }
}

/**
 * @brief Renders an armor display element - COMPLETE FIXED VERSION
 *
 * @param curr_element The element to render
 */
static time_t armor_timeout = 0;
static time_t armor_timeout_trigger = 0;

void render_armor_display_element(element *curr_element) {
   armor_settings *this_as = get_armor_settings();
   element *armor_element = this_as->armor_elements;
   time_t current_time = time(NULL);
   SDL_Renderer *renderer = get_sdl_renderer();
   char text[2048] = "";
   hud_display_settings *this_hds = get_hud_display_settings();

   /* Check for external timeout trigger from registerArmor */
   if (armor_timeout_trigger > 0) {
      armor_timeout = armor_timeout_trigger;
      armor_timeout_trigger = 0;  /* Clear the trigger */
   }

   /* Timeout checking and reset logic */
   if ((armor_timeout > 0) && (current_time > armor_timeout)) {
      armor_timeout = 0;
   }

   /* Determine if we're showing a notification or the regular display */
   SDL_Rect armor_dest_l, armor_dest_r;

   /* Initialize destination rectangle from element properties */
   armor_dest_l.x = armor_dest_r.x = curr_element->dest_x;
   armor_dest_l.y = armor_dest_r.y = curr_element->dest_y;
   armor_dest_l.w = armor_dest_r.w = curr_element->width;
   armor_dest_l.h = armor_dest_r.h = curr_element->height;

   /* Use notification position/size when timeout is active */
   if (armor_timeout > 0) {
      if (curr_element->notice_x > 0 && curr_element->notice_y > 0 &&
          curr_element->notice_width > 0 && curr_element->notice_height > 0) {
         armor_dest_l.x = armor_dest_r.x = curr_element->notice_x;
         armor_dest_l.y = armor_dest_r.y = curr_element->notice_y;
         armor_dest_l.w = armor_dest_r.w = curr_element->notice_width;
         armor_dest_l.h = armor_dest_r.h = curr_element->notice_height;
      }
   }

   /* Apply stereo offset if not fixed */
   if (!curr_element->fixed) {
      armor_dest_l.x -= this_hds->stereo_offset;
      armor_dest_r.x += this_hds->stereo_offset;
   }

   /* Apply scale for zoom transitions */
   calculate_zoom_rect(&armor_dest_l, &armor_dest_r, curr_element->scale);

   /* Initialize metrics texture caching if needed */
   if (curr_element->show_metrics && curr_element->metrics_textures == NULL) {
      /* Count armor components */
      int component_count = 0;
      element *count_element = this_as->armor_elements;
      while (count_element != NULL) {
         component_count++;
         count_element = count_element->next;
      }

      if (component_count > 0) {
         /* Allocate arrays for texture caching */
         curr_element->metrics_textures = calloc(component_count, sizeof(SDL_Texture*));
         curr_element->last_metrics_text = calloc(component_count, sizeof(char*));

         if (curr_element->metrics_textures && curr_element->last_metrics_text) {
            for (int i = 0; i < component_count; i++) {
               curr_element->metrics_textures[i] = NULL;
               curr_element->last_metrics_text[i] = calloc(64, sizeof(char));
               if (curr_element->last_metrics_text[i]) {
                  curr_element->last_metrics_text[i][0] = '\0';
               }
            }
            curr_element->metrics_texture_count = component_count;
         }
      }
   }

   /* Process each armor component with full state management */
   int component_index = 0;
   while (armor_element != NULL) {
      SDL_Texture *texture_to_use = armor_element->texture_base;

      /* Warning state management */
      if ((armor_element->warning_temp >= 0) && (armor_element->last_temp >= 0)) {
         if (!(armor_element->warn_state & WARN_OVER_TEMP) &&
             (armor_element->last_temp > armor_element->warning_temp)) {
            armor_element->texture = armor_element->texture_warning;
            armor_element->warn_state |= WARN_OVER_TEMP;
            time(&armor_timeout);
            armor_timeout += curr_element->notice_timeout > 0 ? curr_element->notice_timeout : 5;
         } else if (armor_element->warn_state & WARN_OVER_TEMP) {
            if (armor_element->last_temp < (armor_element->warning_temp * 0.97)) {
               armor_element->warn_state &= ~WARN_OVER_TEMP;
               if (!armor_element->warn_state) {
                  armor_element->texture = armor_element->texture_online;
               }
            }
         }
      }

      if ((armor_element->warning_voltage >= 0) && (armor_element->last_voltage >= 0)) {
         if (!(armor_element->warn_state & WARN_OVER_VOLT) &&
             (armor_element->last_voltage < armor_element->warning_voltage)) {
            armor_element->texture = armor_element->texture_warning;
            armor_element->warn_state |= WARN_OVER_VOLT;
            time(&armor_timeout);
            armor_timeout += curr_element->notice_timeout > 0 ? curr_element->notice_timeout : 5;
         } else if (armor_element->warn_state & WARN_OVER_VOLT) {
            if (armor_element->last_voltage > (armor_element->warning_voltage * 1.03)) {
               armor_element->warn_state &= ~WARN_OVER_VOLT;
               if (!armor_element->warn_state) {
                  armor_element->texture = armor_element->texture_online;
               }
            }
         }
      }

      /* Complete deregistration logic with TTS */
      if (armor_element->mqtt_registered && armor_element->mqtt_last_time > 0 &&
          ((current_time - this_as->armor_deregister) > armor_element->mqtt_last_time)) {
         armor_element->mqtt_registered = 0;
         armor_element->last_temp = armor_element->last_voltage = -1.0;
         armor_element->warn_state = WARN_NORMAL;
         armor_element->texture = armor_element->texture_offline;

         time(&armor_timeout);
         armor_timeout += curr_element->notice_timeout > 0 ? curr_element->notice_timeout : 5;

         snprintf(text, 2048, "%s disconnected.", armor_element->name);
         mqttTextToSpeech(text);
      }

      /* Select texture based on component status */
      if (armor_element->mqtt_last_time == 0) {
         /* Never been registered - use base texture (blue) */
         texture_to_use = armor_element->texture_base;
      } else if (armor_element->mqtt_registered) {
         /* Currently registered - check for warnings */
         if ((current_time - armor_element->mqtt_last_time) < this_as->armor_deregister) {
            if ((armor_element->warning_temp > 0 &&
                 armor_element->last_temp >= armor_element->warning_temp) ||
                (armor_element->warning_voltage > 0 &&
                 armor_element->last_voltage <= armor_element->warning_voltage)) {
               texture_to_use = armor_element->texture_warning;
            } else {
               texture_to_use = armor_element->texture_online;
            }
         } else {
            /* Registered but timed out - use offline */
            texture_to_use = armor_element->texture_offline;
         }
      } else {
         /* Was registered but now disconnected - use offline (red) */
         texture_to_use = armor_element->texture_offline;
      }

      /* Render the component with proper alpha support */
      if (texture_to_use != NULL) {
         /* Apply alpha for transitions */
         if (curr_element->in_transition && curr_element->transition_alpha > 0.0f) {
            SDL_SetTextureAlphaMod(texture_to_use, (Uint8)(curr_element->transition_alpha * 255));
         }

         renderStereo(texture_to_use, NULL, &armor_dest_l, &armor_dest_r, curr_element->angle);

         /* Reset alpha after rendering */
         if (curr_element->in_transition && curr_element->transition_alpha > 0.0f) {
            SDL_SetTextureAlphaMod(texture_to_use, 255);
         }
      }

      /* Render metrics if enabled and component is active */
      if (curr_element->show_metrics &&
          armor_element->mqtt_registered &&
          (current_time - armor_element->mqtt_last_time) < this_as->armor_deregister &&
          component_index < curr_element->metrics_texture_count) {

         char metrics_text[64] = "";

         // Check if temperature is valid
         if (armor_element->last_temp > -1.0) {
            snprintf(metrics_text, sizeof(metrics_text), "%.1f C", armor_element->last_temp);
         }

         // Check if voltage is valid
         if (armor_element->last_voltage > -1.0) {
            // If temperature was added, add a separator
            if (metrics_text[0] != '\0') {
               strncat(metrics_text, " | ", sizeof(metrics_text) - strlen(metrics_text) - 1);
            }

            // Buffer for voltage text
            char voltage_text[16];
            snprintf(voltage_text, sizeof(voltage_text), "%.2f V", armor_element->last_voltage);

            // Append voltage to metrics text
            strncat(metrics_text, voltage_text, sizeof(metrics_text) - strlen(metrics_text) - 1);
         }

         /* Check if we need to update the texture */
         if (curr_element->last_metrics_text[component_index] == NULL ||
             strcmp(metrics_text, curr_element->last_metrics_text[component_index]) != 0) {

            /* Update cached text */
            if (curr_element->last_metrics_text[component_index]) {
               strncpy(curr_element->last_metrics_text[component_index],
                      metrics_text, 63);
               curr_element->last_metrics_text[component_index][63] = '\0';
            }

            /* Clean up old texture */
            if (curr_element->metrics_textures[component_index]) {
               SDL_DestroyTexture(curr_element->metrics_textures[component_index]);
               curr_element->metrics_textures[component_index] = NULL;
            }

            /* Create new texture */
            SDL_Color text_color = {255, 255, 255, 255}; /* White text */
            TTF_Font *metrics_font = get_local_font(
               curr_element->metrics_font[0] ? curr_element->metrics_font : "ui_assets/fonts/Aldrich-Regular.ttf",
               curr_element->metrics_font_size > 0 ? curr_element->metrics_font_size : 20
            );

            if (metrics_font) {
               SDL_Surface *metrics_surface = TTF_RenderText_Blended(
                  metrics_font, metrics_text, text_color);

               if (metrics_surface) {
                  curr_element->metrics_textures[component_index] =
                     SDL_CreateTextureFromSurface(renderer, metrics_surface);
                  SDL_FreeSurface(metrics_surface);
               }
            }
         }

         /* Render the metrics texture if it exists */
         if (curr_element->metrics_textures[component_index]) {
            /* Get texture dimensions */
            int tex_w, tex_h;
            SDL_QueryTexture(curr_element->metrics_textures[component_index],
                           NULL, NULL, &tex_w, &tex_h);

            /* Use the component's metrics positioning offsets (with defaults) */
            float x_offset = 0.5f;  /* Default to center */
            float y_offset = 0.5f;  /* Default to center */

            /* Use the component's configured offsets if they're valid */
            if (armor_element->metrics_x_offset >= 0.0f && armor_element->metrics_x_offset <= 1.0f) {
               x_offset = armor_element->metrics_x_offset;
            }

            if (armor_element->metrics_y_offset >= 0.0f && armor_element->metrics_y_offset <= 1.0f) {
               y_offset = armor_element->metrics_y_offset;
            }

            /* Calculate the position based on the armor display area and offsets */
            SDL_Rect metrics_rect_l, metrics_rect_r;
            metrics_rect_l.w = metrics_rect_r.w = tex_w;
            metrics_rect_l.h = metrics_rect_r.h = tex_h;

            /* Position the metrics text according to the offsets */
            metrics_rect_l.x = (int)(armor_dest_l.x + (armor_dest_l.w * x_offset) - (tex_w / 2));
            metrics_rect_l.y = (int)(armor_dest_l.y + (armor_dest_l.h * y_offset) - (tex_h / 2));
            metrics_rect_r.x = (int)(armor_dest_r.x + (armor_dest_r.w * x_offset) - (tex_w / 2));
            metrics_rect_r.y = (int)(armor_dest_r.y + (armor_dest_r.h * y_offset) - (tex_h / 2));

            /* Apply alpha for transitions */
            if (curr_element->in_transition && curr_element->transition_alpha > 0.0f) {
               SDL_SetTextureAlphaMod(curr_element->metrics_textures[component_index],
                                      (Uint8)(curr_element->transition_alpha * 255));
            }

            /* Render using renderStereo for proper eye handling */
            renderStereo(curr_element->metrics_textures[component_index],
                        NULL, &metrics_rect_l, &metrics_rect_r, curr_element->angle);

            /* Reset alpha after rendering */
            if (curr_element->in_transition && curr_element->transition_alpha > 0.0f) {
               SDL_SetTextureAlphaMod(curr_element->metrics_textures[component_index], 255);
            }
         }
      }

      armor_element = armor_element->next;
      component_index++;
   }
}

/* Helper function to trigger armor notification timeout from external sources */
void trigger_armor_notification_timeout(int timeout_seconds) {
   time_t current_time;
   time(&current_time);
   armor_timeout_trigger = current_time + timeout_seconds;
}

/*
 * This function takes the arrays from the left and right eyes and validates the
 * detections to insure you get graphics in both eyes.
 */
void validate_detection(void)
{
   int i = 0, j = 0;
   int next_valid = 0;

   /* Clear the past sorted detecttions. */
   for (i = 0; i < MAX_DETECT; i++) {
      this_detect_sorted[0][i].active = 0;
      this_detect_sorted[1][i].active = 0;
   }

   /* Sort through both arrays looking for similar detections on both eyes. */
   for (i = 0; i < MAX_DETECT; i++) {
      for (j = 0; j < MAX_DETECT; j++) {
         if (this_detect[0][i].active && this_detect[1][j].active) {
#if 1
            if ((strcmp(this_detect[0][i].description, this_detect[1][j].description) == 0) &&
                (this_detect[1][j].left > (this_detect[0][i].left * 0.4)) &&
                (this_detect[1][j].left < (this_detect[0][i].left * 1.6)) &&
                (this_detect[1][j].top > (this_detect[0][i].top * 0.4)) &&
                (this_detect[1][j].top < (this_detect[0][i].top * 1.6))) {
#else
            if (strcmp(this_detect[0][i].description, this_detect[1][j].description) == 0) {
#endif
               memcpy(&this_detect_sorted[0][next_valid], &this_detect[0][i], sizeof(detect));
               memcpy(&this_detect_sorted[1][next_valid], &this_detect[1][j], sizeof(detect));
               this_detect[0][i].active = 0;
               this_detect[1][j].active = 0;
               next_valid++;
               break;
            }
         }
      }
   }

   /* Clear the detections from the original arrays. */
   for (i = 0; i < MAX_DETECT; i++) {
      this_detect[0][i].active = 0;
      this_detect[1][i].active = 0;
   }
}

void render_detect_element(element *curr_element) {
    SDL_Rect detect_src_l, detect_src_r;
    SDL_Rect dst_rect_l, dst_rect_r;
    SDL_Texture *detect_texture = NULL;
    SDL_Texture *detect_text_texture = NULL;
    int r_offset = 0, l_offset = 0;
    hud_display_settings *this_hds = get_hud_display_settings();
    SDL_Renderer *renderer = get_sdl_renderer();

    /* Lazily create detection texture if needed */
    if (detect_texture == NULL) {
        LOG_INFO("Loading animation source: %s", curr_element->this_anim.image);
        detect_texture = IMG_LoadTexture(renderer, curr_element->this_anim.image);
        if (!detect_texture) {
            SDL_Log("Couldn't load %s: %s\n", curr_element->this_anim.image, SDL_GetError());
            return;
        }
        SDL_SetTextureAlphaMod(detect_texture, 255);
    }

    /* Check for valid detection data */
    if (oddataL.complete != 1 || oddataL.pix_data == NULL ||
        oddataR.complete != 1 || oddataR.pix_data == NULL) {
        return;
    }

    /* Update detection data */
    pthread_join(od_L_thread, NULL);
    pthread_join(od_R_thread, NULL);
    validate_detection();
    oddataL.processed = 1;
    oddataR.processed = 1;

    /* Render each detected object */
    for (int j = 0; j < MAX_DETECT; j++) {
        /* Setup src rectangle for detection box animation */
        detect_src_l.x = curr_element->this_anim.current_frame->source_x;
        detect_src_l.y = curr_element->this_anim.current_frame->source_y;
        detect_src_l.w = curr_element->this_anim.current_frame->source_w;
        detect_src_l.h = curr_element->this_anim.current_frame->source_h;

        detect_src_r.x = curr_element->this_anim.current_frame->source_x;
        detect_src_r.y = curr_element->this_anim.current_frame->source_y;
        detect_src_r.w = curr_element->this_anim.current_frame->source_w;
        detect_src_r.h = curr_element->this_anim.current_frame->source_h;

        if (this_detect_sorted[0][j].active && this_detect_sorted[1][j].active) {
            /* Set up detection box positions */
            dst_rect_l.x =
                this_detect_sorted[0][j].left + (this_detect_sorted[0][j].width / 2) -
                (curr_element->this_anim.current_frame->source_size_w / 2) +
                curr_element->this_anim.current_frame->dest_x -
                this_hds->cam_crop_x + curr_element->center_x_offset;

            dst_rect_l.y =
                this_detect_sorted[0][j].top + (this_detect_sorted[0][j].height / 2) -
                (curr_element->this_anim.current_frame->source_size_h / 2) +
                curr_element->this_anim.current_frame->dest_y +
                curr_element->center_y_offset;

            dst_rect_r.x =
                this_hds->eye_output_width +
                this_detect_sorted[1][j].left + (this_detect_sorted[1][j].width / 2) -
                (curr_element->this_anim.current_frame->source_size_w / 2) +
                curr_element->this_anim.current_frame->dest_x -
                this_hds->cam_crop_x + curr_element->center_x_offset;

            dst_rect_r.y =
                this_detect_sorted[1][j].top + (this_detect_sorted[1][j].height / 2) -
                (curr_element->this_anim.current_frame->source_size_h / 2) +
                curr_element->this_anim.current_frame->dest_y +
                curr_element->center_y_offset;

            dst_rect_l.w = dst_rect_r.w = curr_element->this_anim.current_frame->source_w;
            dst_rect_l.h = dst_rect_r.h = curr_element->this_anim.current_frame->source_h;

            /* Handle edge cases for detection boxes */
            if ((dst_rect_l.x + dst_rect_l.w) > this_hds->eye_output_width) {
                l_offset = dst_rect_l.x + dst_rect_l.w - this_hds->eye_output_width;
                detect_src_l.w -= l_offset;
                dst_rect_l.w = detect_src_l.w;
            } else {
                l_offset = 0;
            }

            if (dst_rect_r.x < this_hds->eye_output_width) {
                r_offset = this_hds->eye_output_width - dst_rect_r.x;
                detect_src_r.x += r_offset;
                detect_src_r.w -= r_offset;
                dst_rect_r.x = this_hds->eye_output_width;
                dst_rect_r.w = detect_src_r.w;
            } else {
                r_offset = 0;
            }

            /* Render detection boxes */
            SDL_RenderCopy(renderer, detect_texture, &detect_src_l, &dst_rect_l);
            SDL_RenderCopy(renderer, detect_texture, &detect_src_r, &dst_rect_r);

            /* Render text labels for detections */
            curr_element->surface =
                TTF_RenderText_Blended(curr_element->ttf_font,
                                       this_detect_sorted[0][j].description,
                                       curr_element->font_color);
            if (curr_element->surface != NULL) {
                detect_text_texture = SDL_CreateTextureFromSurface(renderer, curr_element->surface);

                detect_src_r.w = detect_src_l.w = curr_element->surface->w;
                detect_src_r.h = detect_src_l.h = curr_element->surface->h;
                detect_src_r.x = detect_src_l.x = 0;
                detect_src_r.y = detect_src_l.y = 0;

                /* Position text labels */
                dst_rect_l.x =
                    this_detect_sorted[0][j].left + (this_detect_sorted[0][j].width / 2) -
                    (curr_element->this_anim.current_frame->source_size_w / 2) -
                    this_hds->cam_crop_x + curr_element->center_x_offset + curr_element->text_x_offset;

                dst_rect_l.y =
                    this_detect_sorted[0][j].top + (this_detect_sorted[0][j].height / 2) -
                    (curr_element->this_anim.current_frame->source_size_h / 2) +
                    curr_element->center_y_offset + curr_element->text_y_offset;

                dst_rect_r.x =
                    this_hds->eye_output_width +
                    this_detect_sorted[1][j].left + (this_detect_sorted[1][j].width / 2) -
                    (curr_element->this_anim.current_frame->source_size_w / 2) -
                    this_hds->cam_crop_x + curr_element->center_x_offset + curr_element->text_x_offset;

                dst_rect_r.y =
                    this_detect_sorted[1][j].top + (this_detect_sorted[1][j].height / 2) -
                    (curr_element->this_anim.current_frame->source_size_h / 2) +
                    curr_element->center_y_offset + curr_element->text_y_offset;

                dst_rect_l.w = dst_rect_r.w = curr_element->surface->w;
                dst_rect_l.h = dst_rect_r.h = curr_element->surface->h;

                /* Handle text wrapping at screen edges */
                if ((dst_rect_l.x + dst_rect_l.w) > this_hds->eye_output_width) {
                    l_offset = dst_rect_l.x + dst_rect_l.w - this_hds->eye_output_width;
                    detect_src_l.w -= l_offset;
                    dst_rect_l.w = detect_src_l.w;
                }

                /* Render detection text labels */
                SDL_RenderCopy(renderer, detect_text_texture, &detect_src_l, &dst_rect_l);
                SDL_RenderCopy(renderer, detect_text_texture, &detect_src_r, &dst_rect_r);

                /* Cleanup text textures */
                SDL_DestroyTexture(detect_text_texture);
                detect_text_texture = NULL;
                SDL_FreeSurface(curr_element->surface);
                curr_element->surface = NULL;
            }
        }
    }

    /* Animation update for detection box graphics */
    Uint32 currTime = SDL_GetTicks();
    float dT = (currTime - curr_element->this_anim.last_update) / 1000.0f;
    int curr_fps = get_curr_fps();
    int framesToUpdate = floor(dT / (1.0f / curr_fps));

    if (framesToUpdate > 0) {
        if ((currTime % (int)ceil((double)curr_fps / curr_element->this_anim.frame_count)) == 0) {
            if (curr_element->this_anim.current_frame->next != NULL) {
                curr_element->this_anim.current_frame = curr_element->this_anim.current_frame->next;
            } else {
                curr_element->this_anim.current_frame = curr_element->this_anim.first_frame;
            }
        }
        curr_element->this_anim.last_update = currTime;
    }
}

/* Main element rendering dispatcher */
void render_element(element *curr_element) {
   if (!curr_element->enabled) {
      return;
   }

   switch (curr_element->type) {
      case STATIC:
         render_static_element(curr_element);
         break;
      case ANIMATED:
         render_animated_element(curr_element);
         break;
      case TEXT:
         render_text_element(curr_element);
         break;
      case SPECIAL:
         render_special_element(curr_element);
         break;
      default:
         LOG_ERROR("Unknown element type: %d", curr_element->type);
         break;
   }
}

/* The rest of the helper functions remain similar */
void render_element_with_alpha(element *curr_element, float alpha) {
   if (!curr_element->enabled) {
      return;
   }

   /* Set transition state */
   curr_element->transition_alpha = alpha;
   curr_element->in_transition = 1;

   /* Save original alpha values */
   Uint8 original_alpha_main = 255;
   if (curr_element->texture != NULL) {
      SDL_GetTextureAlphaMod(curr_element->texture, &original_alpha_main);
   }

   /* Set new alpha value */
   if (curr_element->texture != NULL) {
      SDL_SetTextureAlphaMod(curr_element->texture, (Uint8)(alpha * 255));
   }

   render_element(curr_element);

   /* Restore original alpha */
   SDL_SetTextureAlphaMod(curr_element->texture, original_alpha_main);
}

/* Apply a slide offset to an element during rendering */
void render_element_with_slide(element *curr_element, int offset_x, int offset_y) {
   if (!curr_element->enabled) {
      return;
   }

   /* Set transition state */
   curr_element->in_transition = 1;
   curr_element->transition_alpha = 0.0f; // Not using alpha

   /* Save original position */
   int original_x = curr_element->dst_rect.x;
   int original_y = curr_element->dst_rect.y;
   int original_dest_x = curr_element->dest_x;
   int original_dest_y = curr_element->dest_y;

   /* Apply offset */
   curr_element->dst_rect.x += offset_x;
   curr_element->dst_rect.y += offset_y;
   curr_element->dest_x += offset_x;
   curr_element->dest_y += offset_y;

   /* Get display settings for bounds checking */
   hud_display_settings *this_hds = get_hud_display_settings();

   /* Check if element is entirely off-screen in both eyes - don't render if so */
   if ((curr_element->dst_rect.x + curr_element->dst_rect.w < 0 &&
       curr_element->dst_rect.x + curr_element->dst_rect.w + this_hds->eye_output_width < this_hds->eye_output_width) ||
       (curr_element->dst_rect.x >= 2 * this_hds->eye_output_width) ||
       (curr_element->dst_rect.y + curr_element->dst_rect.h < 0) ||
       (curr_element->dst_rect.y >= this_hds->eye_output_height)) {
       /* Element is entirely off-screen, don't render */
      curr_element->dst_rect.x = original_x;
      curr_element->dst_rect.y = original_y;
      curr_element->dest_x = original_dest_x;
      curr_element->dest_y = original_dest_y;
      return;
   }

   /* Render with offset - renderStereo will handle partial clipping */
   render_element(curr_element);

   /* Restore original position */
   curr_element->dst_rect.x = original_x;
   curr_element->dst_rect.y = original_y;
   curr_element->dest_x = original_dest_x;
   curr_element->dest_y = original_dest_y;
}

/* Apply scaling to an element during rendering */
void render_element_with_scale(element *curr_element, float scale, float alpha) {
   if (!curr_element->enabled) {
      return;
   }

   /* Set transition state */
   curr_element->in_transition = 1;
   curr_element->transition_alpha = alpha;

   int original_scale = curr_element->scale;
   curr_element->scale = scale;

   /* Save original dimensions and position */
   SDL_Rect original_rect = curr_element->dst_rect;
   int original_dest_x = curr_element->dest_x;
   int original_dest_y = curr_element->dest_y;

   /* Render the scaled element with alpha */
   render_element_with_alpha(curr_element, alpha);

   /* Clear transition state */
   curr_element->in_transition = 0;

   /* Restore original dimensions and position */
   curr_element->dst_rect = original_rect;
   curr_element->dest_x = original_dest_x;
   curr_element->dest_y = original_dest_y;
   curr_element->scale = original_scale;
}

/* Main HUD rendering function */
void render_hud_elements(void) {
   element *curr_element = NULL;
   hud_manager *hud_mgr = get_hud_manager();
   hud_display_settings *this_hds = get_hud_display_settings();
   element *first_element = get_first_element();

   if (hud_mgr->transition_from != NULL) {
      /* In transition between HUDs */
      Uint32 elapsed = SDL_GetTicks() - hud_mgr->transition_start_time;
      hud_mgr->transition_progress = (float)elapsed / hud_mgr->transition_duration_ms;

      if (hud_mgr->transition_progress >= 1.0) {
         /* Transition finished */
         hud_mgr->transition_from = NULL;
         hud_mgr->transition_progress = 0.0;

         /* Render current HUD normally */
         curr_element = first_element;
         while (curr_element != NULL) {
            if (curr_element->hud_flags[hud_mgr->current_screen->hud_id]) {
               render_element(curr_element);
            }
            curr_element = curr_element->next;
         }
      } else {
         /* We're in the middle of a transition */
         float from_alpha = 1.0f - hud_mgr->transition_progress;
         float to_alpha = hud_mgr->transition_progress;

         /* Apply transition effect based on transition type */
         switch (hud_mgr->current_screen->transition_type) {
            case TRANSITION_MAX:
               LOG_ERROR("Invalid transition type: %s", get_transition_name(TRANSITION_MAX));
               LOG_ERROR("Changing to valid default transition: %s", get_transition_name(TRANSITION_FADE));
               hud_mgr->current_screen->transition_type = TRANSITION_FADE;

            case TRANSITION_FADE: {
               /* Render elements from the previous HUD that aren't in the new HUD */
               curr_element = first_element;
               while (curr_element != NULL) {
                  if (curr_element->hud_flags[hud_mgr->transition_from->hud_id] &&
                      !curr_element->hud_flags[hud_mgr->current_screen->hud_id]) {
                     render_element_with_alpha(curr_element, from_alpha);
                  }
                  curr_element = curr_element->next;
               }

               /* Render elements from the new HUD that aren't in the old HUD */
               curr_element = first_element;
               while (curr_element != NULL) {
                  if (!curr_element->hud_flags[hud_mgr->transition_from->hud_id] &&
                      curr_element->hud_flags[hud_mgr->current_screen->hud_id]) {
                     render_element_with_alpha(curr_element, to_alpha);
                  }
                  curr_element = curr_element->next;
               }

               /* Render elements that are in both HUDs */
               curr_element = first_element;
               while (curr_element != NULL) {
                  if (curr_element->hud_flags[hud_mgr->transition_from->hud_id] &&
                      curr_element->hud_flags[hud_mgr->current_screen->hud_id]) {
                     render_element(curr_element);
                  }
                  curr_element = curr_element->next;
               }
               break;
            }

            case TRANSITION_SLIDE_LEFT: {
               int from_offset = (int)(-(hud_mgr->transition_progress) * this_hds->eye_output_width);
               int to_offset = (int)((1.0f - hud_mgr->transition_progress) * this_hds->eye_output_width);

               /* First, render the shared elements that appear in both HUDs normally */
               curr_element = first_element;
               while (curr_element != NULL) {
                  if (curr_element->hud_flags[hud_mgr->transition_from->hud_id] &&
                      curr_element->hud_flags[hud_mgr->current_screen->hud_id]) {
                     render_element(curr_element);
                  }
                     curr_element = curr_element->next;
               }

               /* Then render "from" HUD sliding left (only elements not in new HUD) */
               curr_element = first_element;
               while (curr_element != NULL) {
                  if (curr_element->hud_flags[hud_mgr->transition_from->hud_id] &&
                      !curr_element->hud_flags[hud_mgr->current_screen->hud_id]) {
                     render_element_with_slide(curr_element, from_offset, 0);
                  }
                  curr_element = curr_element->next;
               }

               /* Finally render "to" HUD sliding in from right (only elements not in old HUD) */
               curr_element = first_element;
               while (curr_element != NULL) {
                  if (!curr_element->hud_flags[hud_mgr->transition_from->hud_id] &&
                      curr_element->hud_flags[hud_mgr->current_screen->hud_id]) {
                     render_element_with_slide(curr_element, to_offset, 0);
                  }
                  curr_element = curr_element->next;
                }
                break;
             }

             case TRANSITION_SLIDE_RIGHT: {
                int from_offset = (int)((hud_mgr->transition_progress) * this_hds->eye_output_width);
                int to_offset = (int)(-(1.0f - hud_mgr->transition_progress) * this_hds->eye_output_width);

                /* First, render the shared elements that appear in both HUDs normally */
                curr_element = first_element;
                while (curr_element != NULL) {
                   if (curr_element->hud_flags[hud_mgr->transition_from->hud_id] &&
                       curr_element->hud_flags[hud_mgr->current_screen->hud_id]) {
                      render_element(curr_element);
                   }
                   curr_element = curr_element->next;
                }

                /* Then render "from" HUD sliding right (only elements not in new HUD) */
                curr_element = first_element;
                while (curr_element != NULL) {
                   if (curr_element->hud_flags[hud_mgr->transition_from->hud_id] &&
                       !curr_element->hud_flags[hud_mgr->current_screen->hud_id]) {
                      render_element_with_slide(curr_element, from_offset, 0);
                   }
                   curr_element = curr_element->next;
                }

                /* Finally render "to" HUD sliding in from left (only elements not in old HUD) */
                curr_element = first_element;
                while (curr_element != NULL) {
                   if (!curr_element->hud_flags[hud_mgr->transition_from->hud_id] &&
                       curr_element->hud_flags[hud_mgr->current_screen->hud_id]) {
                      render_element_with_slide(curr_element, to_offset, 0);
                   }
                   curr_element = curr_element->next;
                }
                break;
             }

             case TRANSITION_ZOOM: {
                float from_scale = 1.0f + hud_mgr->transition_progress;
                float to_scale = 2.0f - hud_mgr->transition_progress;

                /* Render elements from previous HUD zooming out */
                curr_element = first_element;
                while (curr_element != NULL) {
                   if (curr_element->hud_flags[hud_mgr->transition_from->hud_id] &&
                       !curr_element->hud_flags[hud_mgr->current_screen->hud_id]) {
                      render_element_with_scale(curr_element, from_scale, from_alpha);
                   }
                   curr_element = curr_element->next;
                }

                /* Render elements from new HUD zooming in */
                curr_element = first_element;
                while (curr_element != NULL) {
                   if (!curr_element->hud_flags[hud_mgr->transition_from->hud_id] &&
                       curr_element->hud_flags[hud_mgr->current_screen->hud_id]) {
                      render_element_with_scale(curr_element, to_scale, to_alpha);
                   }
                   curr_element = curr_element->next;
                }

                /* Render elements that are in both HUDs normally */
                curr_element = first_element;
                while (curr_element != NULL) {
                   if (curr_element->hud_flags[hud_mgr->transition_from->hud_id] &&
                       curr_element->hud_flags[hud_mgr->current_screen->hud_id]) {
                      render_element(curr_element);
                   }
                   curr_element = curr_element->next;
                }
                break;
             }
          }

          /* Reset all texture alphas after transition rendering */
          curr_element = first_element;
          while (curr_element != NULL) {
             reset_texture_alpha(curr_element);
             curr_element = curr_element->next;
          }

          /* Reset transition states in all elements */
          curr_element = first_element;
          while (curr_element != NULL) {
             curr_element->in_transition = 0;
             curr_element->transition_alpha = 0.0f;
             curr_element = curr_element->next;
          }
       }
    } else {
        /* Normal rendering - just the current HUD */
        curr_element = first_element;
        while (curr_element != NULL) {
            if (curr_element->hud_flags[hud_mgr->current_screen->hud_id]) {
                render_element(curr_element);
            }
            curr_element = curr_element->next;
        }
    }
}

