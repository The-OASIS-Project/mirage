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

