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
 * All contributions to this project are agreed to be licensed under the
 * GPLv3 or any later version. Contributions are understood to be
 * any modifications, enhancements, or additions to the project
 * and become the property of the original author Kris Kersey.
 */

#ifndef MAIN_H
#define MAIN_H

#include "SDL2/SDL.h"
#include "SDL2/SDL_ttf.h"
#include "config_parser.h"
#include "devices.h"

motion *get_motion_dev(void);
enviro *get_enviro_dev(void);
gps *get_gps_dev(void);
element *get_default_element(void);
element *get_intro_element(void);
element *get_first_element(void);
element *set_first_element(element *this_element);
int *get_sfd(void);
int set_detect_enabled(int enable);
void set_recording_state(DestinationType state);
SDL_Renderer *get_sdl_renderer(void);
TTF_Font *get_local_font(char *font_name, int font_size);
int checkShutdown(void);
void renderStereo(SDL_Texture * tex, SDL_Rect * src, SDL_Rect * dest, SDL_Rect * dest2,
                  double angle);
void mqttTextToSpeech(const char *text);

#endif // DEFINES_H

