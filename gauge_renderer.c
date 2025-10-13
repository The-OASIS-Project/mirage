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
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* SDL Libraries */
#include <SDL2/SDL.h>
#include <SDL2/SDL2_gfxPrimitives.h>

/* Local Headers */
#include "gauge_renderer.h"
#include "config_parser.h"
#include "config_manager.h"
#include "logging.h"
#include "mirage.h"

/**
 * @brief Interpolate between two colors
 *
 * @param c1 Starting color
 * @param c2 Ending color
 * @param t Interpolation factor (0.0 to 1.0)
 * @return Interpolated SDL_Color
 */
static SDL_Color interpolate_color(SDL_Color c1, SDL_Color c2, float t) {
   SDL_Color result;
   
   /* Clamp t to valid range */
   if (t < 0.0f) t = 0.0f;
   if (t > 1.0f) t = 1.0f;
   
   result.r = (Uint8)(c1.r + (c2.r - c1.r) * t);
   result.g = (Uint8)(c1.g + (c2.g - c1.g) * t);
   result.b = (Uint8)(c1.b + (c2.b - c1.b) * t);
   result.a = (Uint8)(c1.a + (c2.a - c1.a) * t);
   
   return result;
}

/**
 * @brief Invalidate and destroy gauge cache texture
 *
 * @param curr_element Pointer to the gauge element
 */
static void invalidate_gauge_cache(element *curr_element) {
   if (curr_element->gauge_cache_texture) {
      SDL_DestroyTexture(curr_element->gauge_cache_texture);
      curr_element->gauge_cache_texture = NULL;
   }
   curr_element->gauge_cache_dirty = 1;
}

/**
 * @brief Generate cached background for linear gauge
 *
 * Always renders at full opacity. Transition alpha applied during blit.
 */
static void generate_linear_cache(element *curr_element) {
   SDL_Renderer *renderer = get_sdl_renderer();

   if (!renderer) {
      LOG_ERROR("Cannot generate linear cache: renderer is NULL");
      return;
   }

   int width = curr_element->width;
   int height = curr_element->height;

   /* Destroy old cache if exists */
   if (curr_element->gauge_cache_texture) {
      SDL_DestroyTexture(curr_element->gauge_cache_texture);
   }

   /* Create new texture for caching */
   curr_element->gauge_cache_texture = SDL_CreateTexture(renderer,
                                                          SDL_PIXELFORMAT_RGBA8888,
                                                          SDL_TEXTUREACCESS_TARGET,
                                                          width, height);

   if (!curr_element->gauge_cache_texture) {
      LOG_ERROR("Failed to create linear gauge cache texture: %s", SDL_GetError());
      return;
   }

   SDL_SetTextureBlendMode(curr_element->gauge_cache_texture, SDL_BLENDMODE_BLEND);
   SDL_SetRenderTarget(renderer, curr_element->gauge_cache_texture);
   SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
   SDL_RenderClear(renderer);

   /* Always render at full opacity */
   Uint8 alpha = 255;

   /* Draw background */
   SDL_Color bg_color = {40, 40, 40, alpha};
   roundedBoxRGBA(renderer, 0, 0, width, height, 3,
                  bg_color.r, bg_color.g, bg_color.b, bg_color.a);

   /* Draw border */
   SDL_Color border_color = {100, 100, 100, alpha};
   roundedRectangleRGBA(renderer, 0, 0, width, height, 3,
                        border_color.r, border_color.g, border_color.b, border_color.a);

   SDL_SetRenderTarget(renderer, NULL);
   curr_element->gauge_cache_dirty = 0;
}

/**
 * @brief Generate cached background for ring gauge
 *
 * Always renders at full opacity. Transition alpha applied during blit.
 */
static void generate_ring_cache(element *curr_element) {
   SDL_Renderer *renderer = get_sdl_renderer();

   if (!renderer) {
      LOG_ERROR("Cannot generate ring cache: renderer is NULL");
      return;
   }

   int width = curr_element->width;
   int height = curr_element->height;

   if (curr_element->gauge_cache_texture) {
      SDL_DestroyTexture(curr_element->gauge_cache_texture);
   }

   curr_element->gauge_cache_texture = SDL_CreateTexture(renderer,
                                                          SDL_PIXELFORMAT_RGBA8888,
                                                          SDL_TEXTUREACCESS_TARGET,
                                                          width, height);

   if (!curr_element->gauge_cache_texture) {
      LOG_ERROR("Failed to create ring gauge cache texture: %s", SDL_GetError());
      return;
   }

   SDL_SetTextureBlendMode(curr_element->gauge_cache_texture, SDL_BLENDMODE_BLEND);
   SDL_SetRenderTarget(renderer, curr_element->gauge_cache_texture);
   SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
   SDL_RenderClear(renderer);

   int diameter = (width < height) ? width : height;
   int outer_radius = diameter / 2;
   int thickness = curr_element->gauge_thickness;
   if (thickness <= 0) thickness = 10;
   if (thickness >= outer_radius) thickness = outer_radius - 2;
   int inner_radius = outer_radius - thickness;

   int cx = width / 2;
   int cy = height / 2;

   /* Always render at full opacity */
   Uint8 alpha = 255;

   float start_angle = curr_element->gauge_arc_start;
   float sweep_angle = curr_element->gauge_arc_sweep;
   if (sweep_angle <= 0.0f) sweep_angle = 360.0f;
   float end_angle = start_angle + sweep_angle;

   /* Draw background ring */
   SDL_Color bg_color = {40, 40, 40, alpha};
   filledPieRGBA(renderer, cx, cy, outer_radius,
                 (Sint16)start_angle, (Sint16)end_angle,
                 bg_color.r, bg_color.g, bg_color.b, bg_color.a);

   filledCircleRGBA(renderer, cx, cy, inner_radius, 0, 0, 0, 0);

   SDL_Color border_color = {80, 80, 80, alpha};
   aacircleRGBA(renderer, cx, cy, outer_radius,
                border_color.r, border_color.g, border_color.b, border_color.a);

   SDL_SetRenderTarget(renderer, NULL);
   curr_element->gauge_cache_dirty = 0;
}

/**
 * @brief Generate cached background for arc gauge
 *
 * Always renders at full opacity. Transition alpha applied during blit.
 */
static void generate_arc_cache(element *curr_element) {
   SDL_Renderer *renderer = get_sdl_renderer();

   if (!renderer) {
      LOG_ERROR("Cannot generate arc cache: renderer is NULL");
      return;
   }

   int width = curr_element->width;
   int height = curr_element->height;

   if (curr_element->gauge_cache_texture) {
      SDL_DestroyTexture(curr_element->gauge_cache_texture);
   }

   curr_element->gauge_cache_texture = SDL_CreateTexture(renderer,
                                                          SDL_PIXELFORMAT_RGBA8888,
                                                          SDL_TEXTUREACCESS_TARGET,
                                                          width, height);

   if (!curr_element->gauge_cache_texture) {
      LOG_ERROR("Failed to create arc gauge cache texture: %s", SDL_GetError());
      return;
   }

   SDL_SetTextureBlendMode(curr_element->gauge_cache_texture, SDL_BLENDMODE_BLEND);
   SDL_SetRenderTarget(renderer, curr_element->gauge_cache_texture);
   SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
   SDL_RenderClear(renderer);

   int diameter = (width < height) ? width : height;
   int radius = diameter / 2;
   int thickness = curr_element->gauge_thickness;
   if (thickness <= 0) thickness = 8;

   int cx = width / 2;
   int cy = height / 2;

   /* Always render at full opacity */
   Uint8 alpha = 255;

   float start_angle = curr_element->gauge_arc_start;
   float sweep_angle = curr_element->gauge_arc_sweep;
   if (sweep_angle <= 0.0f) sweep_angle = 180.0f;
   float end_angle = start_angle + sweep_angle;

   /* Draw background arc */
   SDL_Color bg_color = {60, 60, 60, alpha};
   for (int t = 0; t < thickness; t++) {
      int arc_radius = radius - t;
      arcRGBA(renderer, cx, cy, arc_radius,
              (Sint16)start_angle, (Sint16)end_angle,
              bg_color.r, bg_color.g, bg_color.b, bg_color.a);
   }

   /* Draw tick marks */
   if (curr_element->gauge_ticks > 0) {
      SDL_Color tick_color = {180, 180, 180, alpha};
      int tick_length = 15;
      int num_ticks = curr_element->gauge_ticks;

      for (int i = 0; i <= num_ticks; i++) {
         float tick_angle = start_angle + (sweep_angle * i / (float)num_ticks);
         float rad = tick_angle * M_PI / 180.0f;

         int x1 = cx + (int)(cos(rad) * radius);
         int y1 = cy + (int)(sin(rad) * radius);
         int x2 = cx + (int)(cos(rad) * (radius - tick_length));
         int y2 = cy + (int)(sin(rad) * (radius - tick_length));

         thickLineRGBA(renderer, x1, y1, x2, y2, 2,
                       tick_color.r, tick_color.g, tick_color.b, tick_color.a);
      }
   }

   /* Draw center hub */
   int hub_radius = 10;
   SDL_Color hub_color = {80, 80, 80, alpha};
   filledCircleRGBA(renderer, cx, cy, hub_radius,
                    hub_color.r, hub_color.g, hub_color.b, hub_color.a);

   SDL_Color hub_outline = {120, 120, 120, alpha};
   aacircleRGBA(renderer, cx, cy, hub_radius,
                hub_outline.r, hub_outline.g, hub_outline.b, hub_outline.a);

   SDL_SetRenderTarget(renderer, NULL);
   curr_element->gauge_cache_dirty = 0;
}

/**
 * @brief Calculate scaled rectangle for zoom transitions
 *
 * @param base_x Base X position
 * @param base_y Base Y position
 * @param width Original width
 * @param height Original height
 * @param scale Scale factor (1.0 = normal, >1.0 = zoomed)
 * @param result Output scaled rectangle
 */
static void calculate_gauge_zoom_rect(int base_x, int base_y, int width, int height,
                                       float scale, SDL_Rect *result) {
   /* Calculate center point */
   int center_x = base_x + (width / 2);
   int center_y = base_y + (height / 2);

   /* Apply scale */
   result->w = (int)((float)width * scale);
   result->h = (int)((float)height * scale);

   /* Recenter after scaling */
   result->x = center_x - (result->w / 2);
   result->y = center_y - (result->h / 2);
}

/* ============================================================================
 * UPDATED RENDER FUNCTIONS WITH ZOOM SUPPORT
 * ============================================================================ */

/**
 * @brief Render a linear bar gauge with caching and zoom transitions
 */
static void render_linear_gauge(element *curr_element) {
   SDL_Renderer *renderer = get_sdl_renderer();
   hud_display_settings *this_hds = get_hud_display_settings();
   
   if (!renderer) {
      LOG_ERROR("Cannot render gauge: renderer is NULL");
      return;
   }

   /* Generate cache if needed */
   if (curr_element->gauge_cache_dirty ||
       curr_element->gauge_cache_texture == NULL) {
      generate_linear_cache(curr_element);
   }

   /* Get current value */
   curr_element->gauge_current_value = atof(curr_element->gauge_value_source);

   /* Calculate fill percentage */
   float range = curr_element->gauge_max_value - curr_element->gauge_min_value;
   if (range <= 0.0f) {
      LOG_WARNING("Gauge '%s' has invalid range", curr_element->name);
      return;
   }

   float percentage = (curr_element->gauge_current_value - curr_element->gauge_min_value) / range;
   if (percentage < 0.0f) percentage = 0.0f;
   if (percentage > 1.0f) percentage = 1.0f;

   /* Determine color */
   SDL_Color bar_color = curr_element->gauge_primary_color;
   if (curr_element->gauge_warning_threshold > 0.0f &&
       curr_element->gauge_current_value >= curr_element->gauge_warning_threshold) {
      bar_color = curr_element->gauge_warning_color;
   }

   /* Get transition alpha and scale */
   Uint8 alpha = 255;
   float scale = 1.0f;
   if (curr_element->in_transition && curr_element->transition_alpha > 0.0f) {
      alpha = (Uint8)(curr_element->transition_alpha * 255);
      scale = curr_element->scale;
   }
   bar_color.a = alpha;

   /* Calculate base positions */
   int x_left = curr_element->dest_x;
   int x_right = curr_element->dest_x;
   int y = curr_element->dest_y;
   int width = curr_element->width;
   int height = curr_element->height;

   if (!curr_element->fixed) {
      x_left -= this_hds->stereo_offset;
      x_right += this_hds->stereo_offset;
   }

   /* Render for both eyes */
   int eye_offsets[2] = {x_left, x_right + this_hds->eye_output_width};
   
   for (int eye = 0; eye < 2; eye++) {
      int x = eye_offsets[eye];
      
      /* Calculate zoom rect for background */
      SDL_Rect dst_rect;
      calculate_gauge_zoom_rect(x, y, width, height, scale, &dst_rect);

      /* Apply transition alpha to cached background */
      SDL_SetTextureAlphaMod(curr_element->gauge_cache_texture, alpha);

      /* Blit cached background with zoom */
      SDL_RenderCopy(renderer, curr_element->gauge_cache_texture, NULL, &dst_rect);

      /* Restore texture alpha */
      SDL_SetTextureAlphaMod(curr_element->gauge_cache_texture, 255);
      
      /* Draw dynamic filled portion with zoom */
      if (curr_element->gauge_orientation == 0) {
         /* Horizontal */
         int fill_width = (int)((float)width * percentage * scale);
         if (fill_width > 0) {
            roundedBoxRGBA(renderer, dst_rect.x, dst_rect.y,
                           dst_rect.x + fill_width, dst_rect.y + dst_rect.h, 3,
                           bar_color.r, bar_color.g, bar_color.b, bar_color.a);
         }
      } else {
         /* Vertical */
         int fill_height = (int)((float)height * percentage * scale);
         if (fill_height > 0) {
            int fill_y = dst_rect.y + dst_rect.h - fill_height;
            roundedBoxRGBA(renderer, dst_rect.x, fill_y,
                           dst_rect.x + dst_rect.w, dst_rect.y + dst_rect.h, 3,
                           bar_color.r, bar_color.g, bar_color.b, bar_color.a);
         }
      }
   }
}

/**
 * @brief Render a ring gauge with caching and zoom transitions
 */
static void render_ring_gauge(element *curr_element) {
   SDL_Renderer *renderer = get_sdl_renderer();
   hud_display_settings *this_hds = get_hud_display_settings();

   if (!renderer) {
      LOG_ERROR("Cannot render ring gauge: renderer is NULL");
      return;
   }

   /* Generate cache if needed */
   if (curr_element->gauge_cache_dirty ||
       curr_element->gauge_cache_texture == NULL) {
      generate_ring_cache(curr_element);
   }

   /* Get current value */
   curr_element->gauge_current_value = atof(curr_element->gauge_value_source);

   /* Calculate fill percentage */
   float range = curr_element->gauge_max_value - curr_element->gauge_min_value;
   if (range <= 0.0f) {
      LOG_WARNING("Ring gauge '%s' has invalid range", curr_element->name);
      return;
   }

   float percentage = (curr_element->gauge_current_value - curr_element->gauge_min_value) / range;
   if (percentage < 0.0f) percentage = 0.0f;
   if (percentage > 1.0f) percentage = 1.0f;

   /* Determine color */
   SDL_Color ring_color = curr_element->gauge_primary_color;
   if (curr_element->gauge_warning_threshold > 0.0f &&
       curr_element->gauge_current_value >= curr_element->gauge_warning_threshold) {
      ring_color = curr_element->gauge_warning_color;
   }

   /* Get transition alpha and scale */
   Uint8 alpha = 255;
   float scale = 1.0f;
   if (curr_element->in_transition && curr_element->transition_alpha > 0.0f) {
      alpha = (Uint8)(curr_element->transition_alpha * 255);
      scale = curr_element->scale;
   }
   ring_color.a = alpha;

   /* Calculate geometry */
   int diameter = (curr_element->width < curr_element->height) ?
                  curr_element->width : curr_element->height;
   int outer_radius = (int)((float)(diameter / 2) * scale);
   int thickness = curr_element->gauge_thickness;
   if (thickness <= 0) thickness = 10;
   thickness = (int)((float)thickness * scale);
   if (thickness >= outer_radius) thickness = outer_radius - 2;
   int inner_radius = outer_radius - thickness;

   int center_x_base = curr_element->dest_x + (curr_element->width / 2);
   int center_y = curr_element->dest_y + (curr_element->height / 2);

   /* Calculate angles */
   float start_angle = curr_element->gauge_arc_start;
   float sweep_angle = curr_element->gauge_arc_sweep;
   if (sweep_angle <= 0.0f) sweep_angle = 360.0f;
   float progress_end_angle = start_angle + (sweep_angle * percentage);

   /* Calculate stereo positions */
   int center_x_left = center_x_base;
   int center_x_right = center_x_base;
   if (!curr_element->fixed) {
      center_x_left -= this_hds->stereo_offset;
      center_x_right += this_hds->stereo_offset;
   }

   /* Render for both eyes */
   int center_x_offsets[2] = {center_x_left, center_x_right + this_hds->eye_output_width};

   for (int eye = 0; eye < 2; eye++) {
      int base_x = eye == 0 ? curr_element->dest_x - this_hds->stereo_offset :
                              curr_element->dest_x + this_hds->stereo_offset + this_hds->eye_output_width;
      if (curr_element->fixed) {
         base_x = curr_element->dest_x + (eye == 1 ? this_hds->eye_output_width : 0);
      }

      /* Calculate zoom rect for background */
      SDL_Rect dst_rect;
      calculate_gauge_zoom_rect(base_x, curr_element->dest_y,
                                 curr_element->width, curr_element->height,
                                 scale, &dst_rect);

      /* Apply transition alpha to cached background */
      SDL_SetTextureAlphaMod(curr_element->gauge_cache_texture, alpha);

      /* Blit cached background with zoom */
      SDL_RenderCopy(renderer, curr_element->gauge_cache_texture, NULL, &dst_rect);

      /* Restore texture alpha */
      SDL_SetTextureAlphaMod(curr_element->gauge_cache_texture, 255);

      /* Draw dynamic progress arc with scaled radius */
      int cx = center_x_offsets[eye];

      if (percentage >= 0.999f) {
         /* 100% - draw full ring */
         filledCircleRGBA(renderer, cx, center_y, outer_radius,
                          ring_color.r, ring_color.g, ring_color.b, ring_color.a);
         filledCircleRGBA(renderer, cx, center_y, inner_radius, 0, 0, 0, 255);
      } else if (percentage > 0.0f) {
         /* Draw progress arc */
         filledPieRGBA(renderer, cx, center_y, outer_radius,
                       (Sint16)start_angle, (Sint16)progress_end_angle,
                       ring_color.r, ring_color.g, ring_color.b, ring_color.a);
         filledCircleRGBA(renderer, cx, center_y, inner_radius, 0, 0, 0, 255);
      }
   }
}

/**
 * @brief Render an arc gauge with needle with caching and zoom transitions
 */
static void render_arc_gauge(element *curr_element) {
   SDL_Renderer *renderer = get_sdl_renderer();
   hud_display_settings *this_hds = get_hud_display_settings();

   if (!renderer) {
      LOG_ERROR("Cannot render arc gauge: renderer is NULL");
      return;
   }

   /* Generate cache if needed */
   if (curr_element->gauge_cache_dirty ||
       curr_element->gauge_cache_texture == NULL) {
      generate_arc_cache(curr_element);
   }

   /* Get current value */
   curr_element->gauge_current_value = atof(curr_element->gauge_value_source);

   /* Calculate fill percentage */
   float range = curr_element->gauge_max_value - curr_element->gauge_min_value;
   if (range <= 0.0f) {
      LOG_WARNING("Arc gauge '%s' has invalid range", curr_element->name);
      return;
   }

   float percentage = (curr_element->gauge_current_value - curr_element->gauge_min_value) / range;
   if (percentage < 0.0f) percentage = 0.0f;
   if (percentage > 1.0f) percentage = 1.0f;

   /* Determine if in warning state */
   int in_warning = (curr_element->gauge_warning_threshold > 0.0f &&
                     curr_element->gauge_current_value >= curr_element->gauge_warning_threshold);

   /* Get transition alpha and scale */
   Uint8 alpha = 255;
   float scale = 1.0f;
   if (curr_element->in_transition && curr_element->transition_alpha > 0.0f) {
      alpha = (Uint8)(curr_element->transition_alpha * 255);
      scale = curr_element->scale;
   }

   /* Calculate geometry with scale */
   int diameter = (curr_element->width < curr_element->height) ?
                  curr_element->width : curr_element->height;
   int radius = (int)((float)(diameter / 2) * scale);
   int thickness = curr_element->gauge_thickness;
   if (thickness <= 0) thickness = 8;
   thickness = (int)((float)thickness * scale);

   int center_x_base = curr_element->dest_x + (curr_element->width / 2);
   int center_y = curr_element->dest_y + (curr_element->height / 2);

   /* Get arc parameters */
   float start_angle = curr_element->gauge_arc_start;
   float sweep_angle = curr_element->gauge_arc_sweep;
   if (sweep_angle <= 0.0f) sweep_angle = 180.0f;
   float needle_angle = start_angle + (sweep_angle * percentage);

   /* Calculate stereo positions */
   int center_x_left = center_x_base;
   int center_x_right = center_x_base;
   if (!curr_element->fixed) {
      center_x_left -= this_hds->stereo_offset;
      center_x_right += this_hds->stereo_offset;
   }

   /* Render for both eyes */
   int center_x_offsets[2] = {center_x_left, center_x_right + this_hds->eye_output_width};

   for (int eye = 0; eye < 2; eye++) {
      int base_x = eye == 0 ? curr_element->dest_x - this_hds->stereo_offset :
                              curr_element->dest_x + this_hds->stereo_offset + this_hds->eye_output_width;
      if (curr_element->fixed) {
         base_x = curr_element->dest_x + (eye == 1 ? this_hds->eye_output_width : 0);
      }

      /* Calculate zoom rect for background */
      SDL_Rect dst_rect;
      calculate_gauge_zoom_rect(base_x, curr_element->dest_y,
                                 curr_element->width, curr_element->height,
                                 scale, &dst_rect);

      /* Apply transition alpha to cached background */
      SDL_SetTextureAlphaMod(curr_element->gauge_cache_texture, alpha);

      /* Blit cached background with zoom */
      SDL_RenderCopy(renderer, curr_element->gauge_cache_texture, NULL, &dst_rect);

      /* Restore texture alpha */
      SDL_SetTextureAlphaMod(curr_element->gauge_cache_texture, 255);

      /* Draw dynamic warning arc if needed (with scaled radius) */
      int cx = center_x_offsets[eye];

      if (in_warning) {
         SDL_Color warn_color = curr_element->gauge_warning_color;
         warn_color.a = alpha;

         float warn_percentage = (curr_element->gauge_warning_threshold - curr_element->gauge_min_value) / range;
         float warn_start_angle = start_angle + (sweep_angle * warn_percentage);
         float end_angle = start_angle + sweep_angle;

         for (int t = 0; t < thickness; t++) {
            int arc_radius = radius - t;
            arcRGBA(renderer, cx, center_y, arc_radius,
                    (Sint16)warn_start_angle, (Sint16)end_angle,
                    warn_color.r, warn_color.g, warn_color.b, warn_color.a);
         }
      }

      /* Draw dynamic needle with scale */
      SDL_Color needle_color = in_warning ? curr_element->gauge_warning_color : curr_element->gauge_primary_color;
      needle_color.a = alpha;

      int needle_length = radius - (int)(15.0f * scale);
      float needle_rad = needle_angle * M_PI / 180.0f;

      int tip_x = cx + (int)(cos(needle_rad) * needle_length);
      int tip_y = center_y + (int)(sin(needle_rad) * needle_length);

      int base_width = (int)(12.0f * scale);
      float perp_angle_left = needle_rad - M_PI / 2.0f;
      float perp_angle_right = needle_rad + M_PI / 2.0f;

      int base_left_x = cx + (int)(cos(perp_angle_left) * base_width / 2);
      int base_left_y = center_y + (int)(sin(perp_angle_left) * base_width / 2);
      int base_right_x = cx + (int)(cos(perp_angle_right) * base_width / 2);
      int base_right_y = center_y + (int)(sin(perp_angle_right) * base_width / 2);

      filledTrigonRGBA(renderer,
                       tip_x, tip_y,
                       base_left_x, base_left_y,
                       base_right_x, base_right_y,
                       needle_color.r, needle_color.g, needle_color.b, needle_color.a);

      aatrigonRGBA(renderer,
                   tip_x, tip_y,
                   base_left_x, base_left_y,
                   base_right_x, base_right_y,
                   needle_color.r, needle_color.g, needle_color.b, needle_color.a);
   }
}

/**
 * @brief Main gauge rendering dispatcher
 *
 * Routes to specific gauge rendering functions based on gauge_type.
 *
 * @param curr_element Pointer to the gauge element to render
 */
void render_gauge_element(element *curr_element) {
   if (!curr_element->enabled) {
      return;
   }

   /* Dispatch based on gauge type */
   if (strcmp(curr_element->gauge_type, "linear") == 0) {
      render_linear_gauge(curr_element);
   } else if (strcmp(curr_element->gauge_type, "ring") == 0) {
      render_ring_gauge(curr_element);
   } else if (strcmp(curr_element->gauge_type, "arc") == 0) {
      render_arc_gauge(curr_element);
   } else {
      LOG_WARNING("Unknown gauge type: '%s' for element '%s'",
                  curr_element->gauge_type, curr_element->name);
   }
}
