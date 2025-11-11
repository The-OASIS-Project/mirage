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
#include "data_sources.h"
#include "gauge_renderer.h"
#include "config_parser.h"
#include "config_manager.h"
#include "logging.h"
#include "mirage.h"

/**
 * @brief Smooth interpolation for gauge values
 *
 * @param current Current displayed value
 * @param target Target value to reach
 * @param smooth_factor Interpolation speed (0.0-1.0, higher = faster)
 * @return Interpolated value
 */
static float smooth_gauge_value(float current, float target, float smooth_factor) {
   /* Simple lerp: current + (target - current) * factor */
   /* Higher factor = faster response (0.3 = smooth, 0.1 = very smooth) */
   return current + (target - current) * smooth_factor;
}

/**
 * @brief Render glow effect around gauge elements
 *
 * Renders multiple passes with decreasing alpha and increasing size
 * to create a soft glow effect.
 *
 * @param renderer SDL renderer
 * @param x Center X position
 * @param y Center Y position
 * @param width Width of element
 * @param height Height of element
 * @param color Base glow color
 * @param intensity Glow intensity (0.0-1.0)
 */
static void render_glow_effect(SDL_Renderer *renderer, int x, int y,
                                int width, int height, SDL_Color color,
                                float intensity) {
   /* Render 3 passes for soft glow */
   for (int pass = 0; pass < 3; pass++) {
      float expansion = (pass + 1) * 4.0f;  /* Expand by 4, 8, 12 pixels */
      float alpha_mult = (3 - pass) / 3.0f;  /* Fade outer passes */

      SDL_Color glow_color = color;
      glow_color.a = (Uint8)(color.a * intensity * alpha_mult * 0.5f);

      /* Expanded rectangle for glow */
      SDL_Rect glow_rect = {
         x - (int)expansion,
         y - (int)expansion,
         width + (int)(expansion * 2),
         height + (int)(expansion * 2)
      };

      SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);
      SDL_SetRenderDrawColor(renderer, glow_color.r, glow_color.g,
                            glow_color.b, glow_color.a);
      SDL_RenderFillRect(renderer, &glow_rect);
   }

   SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
}

/**
 * @brief Render circular glow effect (for arc/ring gauges)
 *
 * @param renderer SDL renderer
 * @param cx Center X
 * @param cy Center Y
 * @param radius Base radius
 * @param color Glow color
 * @param intensity Glow intensity (0.0-1.0)
 */
static void render_circular_glow(SDL_Renderer *renderer, int cx, int cy,
                                  int radius, SDL_Color color, float intensity) {
   /* Use SDL2_gfx for circular glows */
   for (int pass = 0; pass < 3; pass++) {
      int glow_radius = radius + (pass + 1) * 6;
      float alpha_mult = (3 - pass) / 3.0f;

      SDL_Color glow_color = color;
      glow_color.a = (Uint8)(color.a * intensity * alpha_mult * 0.4f);

      /* Render with additive blending */
      filledCircleRGBA(renderer, cx, cy, glow_radius,
                      glow_color.r, glow_color.g, glow_color.b, glow_color.a);
   }
}

/**
 * @brief Render value label on gauge
 *
 * @param curr_element Gauge element
 * @param value Value to display
 * @param x Center X position
 * @param y Center Y position
 * @param alpha Transparency
 */
static void render_gauge_value_label(element *curr_element, float value,
                                     int x, int y, Uint8 alpha) {
   if (!curr_element->gauge_show_value) {
      return;
   }

   SDL_Renderer *renderer = get_sdl_renderer();
   if (!renderer) return;

   /* Determine if we need to regenerate the label */
   float value_change = fabsf(value - curr_element->gauge_last_rendered_value);
   float change_threshold = 0.5f;  /* Don't redraw unless value changed by 0.5+ */

   int need_regenerate = 0;

   if (curr_element->gauge_value_label_texture == NULL) {
      need_regenerate = 1;  /* First time */
   } else if (value_change >= change_threshold) {
      need_regenerate = 1;  /* Value changed significantly */
   }

   /* Regenerate texture if needed */
   if (need_regenerate) {
      /* Format the value */
      char value_text[64];
      if (strlen(curr_element->gauge_value_format) > 0) {
         snprintf(value_text, sizeof(value_text),
                 curr_element->gauge_value_format, value);
      } else {
         snprintf(value_text, sizeof(value_text), "%.0f", value);
      }

      /* Get cached font */
      TTF_Font *font = get_local_font("ui_assets/fonts/Aldrich-Regular.ttf",
                                       curr_element->gauge_value_size);
      if (!font) {
         return;
      }

      /* Create text surface */
      SDL_Color text_color = curr_element->gauge_value_color;
      text_color.a = 255;  /* Always render at full opacity to texture */

      SDL_Surface *surface = TTF_RenderUTF8_Blended(font, value_text, text_color);
      if (!surface) {
         return;
      }

      /* Destroy old texture if exists */
      if (curr_element->gauge_value_label_texture) {
         SDL_DestroyTexture(curr_element->gauge_value_label_texture);
      }

      /* Create new texture */
      curr_element->gauge_value_label_texture = SDL_CreateTextureFromSurface(renderer, surface);
      if (curr_element->gauge_value_label_texture) {
         SDL_SetTextureBlendMode(curr_element->gauge_value_label_texture, SDL_BLENDMODE_BLEND);
         curr_element->gauge_value_label_width = surface->w;
         curr_element->gauge_value_label_height = surface->h;
      }

      SDL_FreeSurface(surface);
      curr_element->gauge_last_rendered_value = value;
   }

   /* Render cached texture with current alpha */
   if (curr_element->gauge_value_label_texture) {
      SDL_SetTextureAlphaMod(curr_element->gauge_value_label_texture, alpha);

      SDL_Rect dst_rect = {
         x - curr_element->gauge_value_label_width / 2,
         y - curr_element->gauge_value_label_height / 2,
         curr_element->gauge_value_label_width,
         curr_element->gauge_value_label_height
      };

      SDL_RenderCopy(renderer, curr_element->gauge_value_label_texture, NULL, &dst_rect);
   }
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

   /* Draw permanent warning zone arc ===== */
   if (curr_element->gauge_warning_threshold > 0.0f) {
      float range = curr_element->gauge_max_value - curr_element->gauge_min_value;
      if (range > 0.0f) {
         /* Calculate warning zone percentage */
         float warn_percentage = (curr_element->gauge_warning_threshold - curr_element->gauge_min_value) / range;

         /* Calculate angles for warning zone */
         float warn_start_angle = curr_element->gauge_arc_start + (curr_element->gauge_arc_sweep * warn_percentage);
         float warn_end_angle = curr_element->gauge_arc_start + curr_element->gauge_arc_sweep;

         /* Only draw if there's a valid warning zone */
         if (warn_start_angle < warn_end_angle) {
            SDL_Color warn_color = curr_element->gauge_warning_color;

            /* Draw multi-pass warning arc for thickness */
            for (int t = 0; t < thickness; t++) {
               int arc_radius = radius - t;
               arcRGBA(renderer, cx, cy, arc_radius,
                       (Sint16)warn_start_angle, (Sint16)warn_end_angle,
                       warn_color.r, warn_color.g, warn_color.b, 160);
            }

            /* Add inner warning arc edge for definition */
            arcRGBA(renderer, cx, cy, radius - thickness,
                    (Sint16)warn_start_angle, (Sint16)warn_end_angle,
                    warn_color.r, warn_color.g, warn_color.b, 200);
         }
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

   /* Smooth value interpolation */
   float target_value = resolve_data_source_float(curr_element->gauge_value_source);

   /* Apply smooth interpolation if enabled */
   if (curr_element->gauge_smooth) {
      /* First frame: initialize display value */
      if (curr_element->gauge_display_value == 0.0f && target_value != 0.0f) {
         curr_element->gauge_display_value = target_value;
      } else {
         /* Smooth interpolation (0.2 = nice smooth speed) */
         curr_element->gauge_display_value = smooth_gauge_value(
            curr_element->gauge_display_value, target_value, 0.2f);
      }
      curr_element->gauge_current_value = curr_element->gauge_display_value;
   } else {
      /* Instant update */
      curr_element->gauge_current_value = target_value;
      curr_element->gauge_display_value = target_value;
   }

   /* Calculate fill percentage */
   float range = curr_element->gauge_max_value - curr_element->gauge_min_value;
   if (range <= 0.0f) {
      LOG_WARNING("Gauge '%s' has invalid range", curr_element->name);
      return;
   }

   float percentage = (curr_element->gauge_current_value - curr_element->gauge_min_value) / range;
   if (percentage < 0.0f) percentage = 0.0f;
   if (percentage > 1.0f) percentage = 1.0f;

   /* Determine color based on warning threshold */
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

   /* Calculate scaled destination rectangle */
   SDL_Rect dst_rect;
   calculate_gauge_zoom_rect(curr_element->dest_x, curr_element->dest_y,
                             curr_element->width, curr_element->height,
                             scale, &dst_rect);

   /* Render for both eyes (stereo) */
   for (int eye = 0; eye < 2; eye++) {
      int stereo_offset = curr_element->fixed ? 0 : this_hds->stereo_offset;
      int x_offset = (eye == 0) ? -stereo_offset :
                     (this_hds->eye_output_width + stereo_offset);
      
      SDL_Rect eye_rect = dst_rect;
      eye_rect.x += x_offset;

      /* Render glow effect */
      if (curr_element->gauge_glow) {
         render_glow_effect(renderer, eye_rect.x, eye_rect.y,
                           eye_rect.w, eye_rect.h, bar_color, 0.6f);
      }

      /* Render cached background */
      SDL_SetTextureAlphaMod(curr_element->gauge_cache_texture, alpha);
      SDL_RenderCopy(renderer, curr_element->gauge_cache_texture, NULL, &eye_rect);
      SDL_SetTextureAlphaMod(curr_element->gauge_cache_texture, 255);

      /* Render dynamic fill bar */
      if (percentage > 0.0f) {
         if (curr_element->gauge_orientation == 0) {
            /* Horizontal fill */
            int fill_width = (int)((float)eye_rect.w * percentage * scale);
            if (fill_width > 0) {
               roundedBoxRGBA(renderer, eye_rect.x, eye_rect.y,
                             eye_rect.x + fill_width, eye_rect.y + eye_rect.h, 3,
                             bar_color.r, bar_color.g, bar_color.b, bar_color.a);
            }
         } else {
            /* Vertical fill (bottom to top) */
            int fill_height = (int)((float)eye_rect.h * percentage * scale);
            if (fill_height > 0) {
               int fill_y = eye_rect.y + eye_rect.h - fill_height;
               roundedBoxRGBA(renderer, eye_rect.x, fill_y,
                             eye_rect.x + eye_rect.w, eye_rect.y + eye_rect.h, 3,
                             bar_color.r, bar_color.g, bar_color.b, bar_color.a);
            }
         }
      }

      /* Render value label */
      if (curr_element->gauge_show_value) {
         int center_x = eye_rect.x + eye_rect.w / 2;
         int center_y = eye_rect.y + eye_rect.h / 2;
         render_gauge_value_label(curr_element, curr_element->gauge_current_value,
                                 center_x, center_y, bar_color.a);
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

   /* Smooth value interpolation */
   float target_value = resolve_data_source_float(curr_element->gauge_value_source);

   /* Apply smooth interpolation if enabled */
   if (curr_element->gauge_smooth) {
      /* First frame: initialize display value */
      if (curr_element->gauge_display_value == 0.0f && target_value != 0.0f) {
         curr_element->gauge_display_value = target_value;
      } else {
         /* Smooth interpolation (0.2 = nice smooth speed) */
         curr_element->gauge_display_value = smooth_gauge_value(
            curr_element->gauge_display_value, target_value, 0.2f);
      }
      curr_element->gauge_current_value = curr_element->gauge_display_value;
   } else {
      /* Instant update */
      curr_element->gauge_current_value = target_value;
      curr_element->gauge_display_value = target_value;
   }

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
   int outer_radius = (int)((diameter / 2) * scale);
   int inner_radius = outer_radius - (int)(curr_element->gauge_thickness * scale);
   if (inner_radius < 1) inner_radius = 1;

   /* Calculate angles */
   float start_angle = curr_element->gauge_arc_start;
   float sweep_angle = curr_element->gauge_arc_sweep;
   float progress_angle = percentage * sweep_angle;
   float progress_end_angle = start_angle + progress_angle;

   /* Calculate scaled destination rectangle */
   SDL_Rect dst_rect;
   calculate_gauge_zoom_rect(curr_element->dest_x, curr_element->dest_y,
                             curr_element->width, curr_element->height,
                             scale, &dst_rect);

   /* Center coordinates */
   int center_y = dst_rect.y + dst_rect.h / 2;

   /* Render for both eyes */
   for (int eye = 0; eye < 2; eye++) {
      int stereo_offset = curr_element->fixed ? 0 : this_hds->stereo_offset;
      int x_offset = (eye == 0) ? -stereo_offset :
                     (this_hds->eye_output_width + stereo_offset);

      int cx = dst_rect.x + dst_rect.w / 2 + x_offset;

      /* Render circular glow */
      if (curr_element->gauge_glow) {
         render_circular_glow(renderer, cx, center_y, outer_radius,
                             ring_color, 0.5f);
      }

      /* Render cached background */
      SDL_Rect eye_rect = dst_rect;
      eye_rect.x += x_offset;
      SDL_SetTextureAlphaMod(curr_element->gauge_cache_texture, alpha);
      SDL_RenderCopy(renderer, curr_element->gauge_cache_texture, NULL, &eye_rect);
      SDL_SetTextureAlphaMod(curr_element->gauge_cache_texture, 255);

      /* Render dynamic progress arc */
      if (sweep_angle >= 360.0f) {
         /* Full circle - render as filled circle with hole */
         filledCircleRGBA(renderer, cx, center_y, outer_radius,
                         ring_color.r, ring_color.g, ring_color.b, ring_color.a);
         filledCircleRGBA(renderer, cx, center_y, inner_radius, 0, 0, 0, 255);
      } else if (percentage > 0.0f) {
         /* Partial arc - draw progress */
         filledPieRGBA(renderer, cx, center_y, outer_radius,
                      (Sint16)start_angle, (Sint16)progress_end_angle,
                      ring_color.r, ring_color.g, ring_color.b, ring_color.a);
         filledCircleRGBA(renderer, cx, center_y, inner_radius, 0, 0, 0, 255);
      }

      /* Render value label */
      if (curr_element->gauge_show_value) {
         render_gauge_value_label(curr_element, curr_element->gauge_current_value,
                                 cx, center_y, ring_color.a);
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

   /* Smooth value interpolation */
   float target_value = resolve_data_source_float(curr_element->gauge_value_source);

   /* Apply smooth interpolation if enabled */
   if (curr_element->gauge_smooth) {
      /* First frame: initialize display value */
      if (curr_element->gauge_display_value == 0.0f && target_value != 0.0f) {
         curr_element->gauge_display_value = target_value;
      } else {
         /* Smooth interpolation (0.2 = nice smooth speed) */
         curr_element->gauge_display_value = smooth_gauge_value(
            curr_element->gauge_display_value, target_value, 0.2f);
      }
      curr_element->gauge_current_value = curr_element->gauge_display_value;
   } else {
      /* Instant update */
      curr_element->gauge_current_value = target_value;
      curr_element->gauge_display_value = target_value;
   }

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
   int radius = (int)((diameter / 2) * scale);

   /* Calculate needle angle */
   float sweep_angle = curr_element->gauge_arc_sweep;
   float needle_angle = curr_element->gauge_arc_start + (percentage * sweep_angle);

   /* Determine needle color */
   SDL_Color needle_color = in_warning ?
                           curr_element->gauge_warning_color :
                           curr_element->gauge_primary_color;
   needle_color.a = alpha;

   /* Calculate scaled destination rectangle */
   SDL_Rect dst_rect;
   calculate_gauge_zoom_rect(curr_element->dest_x, curr_element->dest_y,
                             curr_element->width, curr_element->height,
                             scale, &dst_rect);

   /* Center coordinates */
   int center_y = dst_rect.y + dst_rect.h / 2;

   /* Render for both eyes */
   for (int eye = 0; eye < 2; eye++) {
      int stereo_offset = curr_element->fixed ? 0 : this_hds->stereo_offset;
      int x_offset = (eye == 0) ? -stereo_offset :
                     (this_hds->eye_output_width + stereo_offset);

      int cx = dst_rect.x + dst_rect.w / 2 + x_offset;

      /* Render circular glow */
      if (curr_element->gauge_glow) {
         render_circular_glow(renderer, cx, center_y, radius,
                             needle_color, 0.5f);
      }

      /* Render cached background */
      SDL_Rect eye_rect = dst_rect;
      eye_rect.x += x_offset;
      SDL_SetTextureAlphaMod(curr_element->gauge_cache_texture, alpha);
      SDL_RenderCopy(renderer, curr_element->gauge_cache_texture, NULL, &eye_rect);
      SDL_SetTextureAlphaMod(curr_element->gauge_cache_texture, 255);

      /* Render dynamic needle */
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

      /* Draw filled triangle */
      filledTrigonRGBA(renderer,
                       tip_x, tip_y,
                       base_left_x, base_left_y,
                       base_right_x, base_right_y,
                       needle_color.r, needle_color.g, needle_color.b, needle_color.a);

      /* Draw anti-aliased outline */
      aatrigonRGBA(renderer,
                   tip_x, tip_y,
                   base_left_x, base_left_y,
                   base_right_x, base_right_y,
                   needle_color.r, needle_color.g, needle_color.b, needle_color.a);

      /* Render value label */
      if (curr_element->gauge_show_value) {
         /* Position label below needle pivot point */
         int label_y = center_y + radius / 2;
         render_gauge_value_label(curr_element, curr_element->gauge_current_value,
                                 cx, label_y, needle_color.a);
      }
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
