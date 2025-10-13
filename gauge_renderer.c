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
 * @brief Render a linear bar gauge (horizontal or vertical)
 *
 * @param curr_element Pointer to the gauge element
 */
static void render_linear_gauge(element *curr_element) {
   SDL_Renderer *renderer = get_sdl_renderer();
   hud_display_settings *this_hds = get_hud_display_settings();
   
   if (!renderer) {
      LOG_ERROR("Cannot render gauge: renderer is NULL");
      return;
   }

   /* Get current value from gauge_value_source (Phase 4 will parse dynamic sources) */
   /* For now, use static value */
   curr_element->gauge_current_value = atof(curr_element->gauge_value_source);

   /* Calculate fill percentage */
   float range = curr_element->gauge_max_value - curr_element->gauge_min_value;
   if (range <= 0.0f) {
      LOG_WARNING("Gauge '%s' has invalid range (min=%.1f, max=%.1f)",
                  curr_element->name, curr_element->gauge_min_value, 
                  curr_element->gauge_max_value);
      return;
   }

   float percentage = (curr_element->gauge_current_value - curr_element->gauge_min_value) / range;
   
   /* Clamp to 0-1 range */
   if (percentage < 0.0f) percentage = 0.0f;
   if (percentage > 1.0f) percentage = 1.0f;

   /* Determine color based on warning threshold */
   SDL_Color bar_color = curr_element->gauge_primary_color;
   if (curr_element->gauge_warning_threshold > 0.0f &&
       curr_element->gauge_current_value >= curr_element->gauge_warning_threshold) {
      bar_color = curr_element->gauge_warning_color;
   }

   /* Apply transition alpha if in transition */
   Uint8 alpha = 255;
   if (curr_element->in_transition && curr_element->transition_alpha > 0.0f) {
      alpha = (Uint8)(curr_element->transition_alpha * 255);
      bar_color.a = alpha;
   }

   /* Calculate positions for stereo rendering */
   int x_left = curr_element->dest_x;
   int x_right = curr_element->dest_x;
   int y = curr_element->dest_y;
   int width = curr_element->width;
   int height = curr_element->height;

   /* Apply stereo offset if not fixed */
   if (!curr_element->fixed) {
      x_left -= this_hds->stereo_offset;
      x_right += this_hds->stereo_offset;
   }

   /* Render for both eyes (left and right) */
   int eye_offsets[2] = {x_left, x_right + this_hds->eye_output_width};
   
   for (int eye = 0; eye < 2; eye++) {
      int x = eye_offsets[eye];
      
      /* Draw background (dark) */
      SDL_Color bg_color = {40, 40, 40, alpha};
      roundedBoxRGBA(renderer, x, y, x + width, y + height, 3,
                     bg_color.r, bg_color.g, bg_color.b, bg_color.a);
      
      /* Draw filled portion based on orientation */
      if (curr_element->gauge_orientation == 0) {
         /* Horizontal */
         int fill_width = (int)(width * percentage);
         if (fill_width > 0) {
            roundedBoxRGBA(renderer, x, y, x + fill_width, y + height, 3,
                           bar_color.r, bar_color.g, bar_color.b, bar_color.a);
         }
      } else {
         /* Vertical - fill from bottom up */
         int fill_height = (int)(height * percentage);
         if (fill_height > 0) {
            int fill_y = y + height - fill_height;
            roundedBoxRGBA(renderer, x, fill_y, x + width, y + height, 3,
                           bar_color.r, bar_color.g, bar_color.b, bar_color.a);
         }
      }
      
      /* Draw border (outline) */
      SDL_Color border_color = {100, 100, 100, alpha};
      roundedRectangleRGBA(renderer, x, y, x + width, y + height, 3,
                           border_color.r, border_color.g, border_color.b, border_color.a);
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
   } else {
      LOG_WARNING("Unknown gauge type: '%s' for element '%s'",
                  curr_element->gauge_type, curr_element->name);
   }
}
