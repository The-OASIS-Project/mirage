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

#ifndef GAUGE_RENDERER_H
#define GAUGE_RENDERER_H

#include <SDL2/SDL.h>
#include "config_parser.h"

/**
 * @file gauge_renderer.h
 * @brief Gauge widget rendering system for HUD displays
 *
 * This module provides gauge rendering capabilities for the Mirage HUD system.
 * Gauges are implemented as SPECIAL elements with special_name "gauge".
 * 
 * Supported gauge types:
 * - "linear" - Horizontal or vertical bar gauges
 * - "arc" - Circular arc gauges with needle (speedometer style)
 * - "ring" - Circular progress rings (modern style)
 */

/**
 * @brief Main gauge rendering dispatcher
 *
 * Called from render_special_element() when a gauge element is encountered.
 * Routes to specific gauge rendering functions based on gauge_type.
 *
 * @param curr_element Pointer to the gauge element to render
 */
void render_gauge_element(element *curr_element);

#endif /* GAUGE_RENDERER_H */
