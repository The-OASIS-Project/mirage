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

#ifndef ELEMENT_RENDERER_H
#define ELEMENT_RENDERER_H

#include "config_parser.h"
#include "hud_manager.h"

/* Basic element rendering functions */
void render_static_element(element *curr_element);
void render_animated_element(element *curr_element);
void render_text_element(element *curr_element);
void render_special_element(element *curr_element);

/* Special element renderers */
void render_map_element(element *curr_element);
void render_pitch_element(element *curr_element);
void render_heading_element(element *curr_element);
void render_altitude_element(element *curr_element);
void render_wifi_element(element *curr_element);
void render_detect_element(element *curr_element);
void render_armor_display_element(element *curr_element);

void trigger_armor_notification_timeout(int timeout_seconds);

/* Element rendering with effects */
void render_element(element *curr_element);
void render_element_with_alpha(element *curr_element, float alpha);
void render_element_with_slide(element *curr_element, int offset_x, int offset_y);
void render_element_with_scale(element *curr_element, float scale, float alpha);

/* Main HUD rendering function */
void render_hud_elements(void);

/* Extra Cleanups */
void cleanup_fan_monitoring(void);

#endif /* ELEMENT_RENDERER_H */
