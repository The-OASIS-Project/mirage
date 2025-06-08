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

/* Threading Information */
/* vid_out_thread    - Video output processing. Disk and/or streaming.
 * thread_handles[#] - NUM_AUDIO_THREADS number of audio threads.
 * video_proc_thread - Video input processing. Cameras to SDL.
 * command_proc_thread - USB/Serial input handling.
 * od_[LR]_thread    - If object detection is enabled. These will handle
 *                     object detection for each eye.
 * cpu_util_thread   - If a CPU utilization UI element is enabled, this
 *                     will keep that up-to-date.
 * map_download_thread - If a map UI element is enabled, this will keep
 *                       that up-to-date.
 * mosquitto loop    - There is a background process monitoring MQTT.
 *                     The thread is defined externally with callbacks
 *                     here.
 */

#define _GNU_SOURCE
/* Std C */
#include <ctype.h>
#include <getopt.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Serial Port */
#include <termios.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>

/* Sockets */
#include <netdb.h>
#include <netinet/in.h>

/* Vorbis */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#include "vorbis/codec.h"
#include "vorbisfile.h"
#pragma GCC diagnostic pop

/* POSIX Message Queue */
#include <fcntl.h>
#include <mqueue.h>
#include <sys/stat.h>

/* SDL2 */
#include "SDL2/SDL.h"
#include "SDL2/SDL_image.h"
#include "SDL2/SDL_ttf.h"

/* JSON */
#include <json-c/json.h>

/* CURL */
#include <curl/curl.h>

/* GStreamer */
#include <glib-2.0/glib.h>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>

/* Mosquitto */
#include <mosquitto.h>

/* NVIDIA/CUDA */
#ifdef USE_CUDA
#include <cuda_runtime.h>
//#include <nvbuf_utils.h>
#endif

#include <GL/glew.h>
#include <stdbool.h>

/* Local */
#include "defines.h" /* Out of order due to dependencies */
#include "audio.h"
#include "armor.h"
#include "command_processing.h"
#include "config_manager.h"
#include "config_parser.h"
#include "curl_download.h"
#include "devices.h"
#include "element_renderer.h"
#include "frame_rate_tracker.h"
#include "hud_manager.h"
#include "image_utils.h"
#include "logging.h"
#include "mirage.h"
#include "mosquitto_comms.h"
#include "recording.h"
#include "screenshot.h"
#include "secrets.h"
#include "utils.h"
#include "version.h"

static pthread_mutex_t v_mutex = PTHREAD_MUTEX_INITIALIZER;  /* Mutex for video buffer access. */
static GstMapInfo mapL[2], mapR[2];        /* Video memory maps. */
#ifdef DISPLAY_TIMING
struct timespec ts_cap[2];          /* Store the display latency. */
#endif
static int video_posted = 0;               /* Notify that the new video frames are ready. */
static int buffer_num = 0;                 /* Video is double buffered. This swaps between them. */

static int window_width = 0;
static int window_height = 0;
static pthread_mutex_t windowSizeMutex = PTHREAD_MUTEX_INITIALIZER;

pthread_t od_L_thread = 0, od_R_thread = 0;  /* Object detection thread IDs */
static int single_cam = 0;                   /* Single Camera Mode Enable */
static int cam1_id = -1, cam2_id = -1;       /* Camera IDs for CSI or USB */

static struct mosquitto *mosq = NULL;        /* MQTT pointer */

static int quit = 0;                         /* Global to sync exiting of threads */
int detect_enabled = 0;                      /* Is object detection enabled? */

double averageFrameRate = 0.0;
static int curr_fps = 60;

/* Right now we only support one instance of each. These are their objects. */
motion this_motion = {
   .format = 0,
   .heading = 0.0,
   .pitch = 0.0,
   .roll = 0.0,
   .w = 1.0,    /* Identity quaternion values */
   .x = 0.0,
   .y = 0.0,
   .z = 0.0
};
enviro this_enviro = {
   .temp = 0.0,
   .humidity = 0.0,
   .air_quality = 0.0,
   .air_quality_description = "",
   .tvoc_ppb = 0.0,
   .eco2_ppm = 0.0,
   .co2_ppm = 0.0,
   .co2_quality_description = "",
   .co2_eco2_diff = 0,
   .co2_source_analysis = "",
   .heat_index_c = 0.0,
   .dew_point = 0.0
};
gps this_gps = {
   .time = "00:00:00",
   .date = "2021/01/01",
   .fix = 0,
   .quality = 0,
   .latitude = 0.0,
   .latitudeDegrees = 0.0,
   .lat = "0N",
   .longitude = 0.0,
   .longitudeDegrees = 0.0,
   .lon = "0W",
   .speed = 0.0,
   .angle = 0.0,
   .altitude = 0.0,
   .satellites = 0
};
#define AI_NAME_MAX_LENGTH 32
#define AI_STATE_MAX_LENGTH 18   /* This is actually defined by the states in DAWN. */
static char aiName[AI_NAME_MAX_LENGTH] = "";
static char aiState[AI_STATE_MAX_LENGTH] = "";

/* Audio */
extern thread_info audio_threads[NUM_AUDIO_THREADS];
mqd_t qd_server;
extern mqd_t qd_clients[NUM_AUDIO_THREADS];

const struct Alert alert_messages[ALERT_MAX] = {
   {ALERT_RECORDING, "ERROR: Recording failed!"}
};

alert_t active_alerts = ALERT_NONE;

element default_element =
{
   .type = STATIC,
   .enabled = 0,

   .name = "",
   .hotkey = "",

   .filename = "",
   .filename_r = "",
   .filename_s = "",
   .filename_rs = "",

   .filename_online = "",
   .filename_warning = "",
   .filename_offline = "",

   .text = "",
   .font = "",
   //SDL_Color font_color;
   .ttf_font = NULL,
   .font_size = -1,
   .halign = "left",

   .dest_x = 0,
   .dest_y = 0,
   .angle = 0.0,
   .fixed = 0,

   .layer = 0,

   .surface = NULL,
   .texture = NULL,
   .texture_r = NULL,
   .texture_s = NULL,
   .texture_rs = NULL,
   .texture_l = NULL,
   .texture_w = NULL,
   .texture_p = NULL,

   .texture_base = NULL,
   .texture_online = NULL,
   .texture_warning = NULL,
   .texture_offline = NULL,

   .dst_rect = {0, 0, 0, 0},

   .special_name = "",
   .mqtt_device = "",
   .mqtt_registered = 0,
   .mqtt_last_time = 0,

   .width = 0,
   .height = 0,

   .download_count = 0,

   .center_x_offset = 0,
   .center_y_offset = 0,

   .text_x_offset = 0,
   .text_y_offset = 0,

   .this_anim.first_frame = NULL,
   .this_anim.frame_count = 0,

   .warning_temp = -1.0,
   .warning_voltage = -1.0,

   .last_temp = -1.0,
   .last_voltage = -1.0,

   .notice_x = 0,
   .notice_y = 0,
   .notice_width = 0,
   .notice_height = 0,
   .notice_timeout = DEFAULT_ARMOR_NOTICE_TIMEOUT,
   .show_metrics = 0,
   .metrics_font = "",
   .metrics_font_size = 20,

   .metrics_textures = NULL,
   .last_metrics_text = NULL,
   .metrics_texture_count = 0,

   .warn_state = WARN_NORMAL,

   .transition_alpha = 0.0f,
   .in_transition = 0,
   .scale = 1.0f,

   .prev = NULL,
   .next = NULL
};

element *first_element = NULL;               /* Pointer to first UI element. */
element intro_element;                       /* Special intro element. */

/* Local font elements to store each loaded font. */
typedef struct _local_fonts {
   TTF_Font *ttf_font;
   char font[MAX_FILENAME_LENGTH * 2];
   int font_size;

   struct _local_fonts *next;
} local_font;
local_font *font_list = NULL;

/* Detected Objects */
detect this_detect[2][MAX_DETECT];
detect this_detect_sorted[2][MAX_DETECT];

static SDL_Renderer *renderer = NULL;               /* Global SDL Renderer */
static SDL_threadID main_thread_id = 0;

od_data oddataL, oddataR;

/**
 * Returns a pointer to the global motion data structure.
 */
motion *get_motion_dev(void)
{
   return &this_motion;
}

/**
 * Returns a pointer to the global environmental data structure.
 */
enviro *get_enviro_dev(void)
{
   return &this_enviro;
}

/**
 * Returns a pointer to the global GPS data structure.
 */
gps *get_gps_dev(void)
{
   return &this_gps;
}

/**
 * Returns a pointer to the global intro element.
 */
element *get_intro_element(void)
{
   return &intro_element;
}

/**
 * Returns a pointer to the default element template.
 */
element *get_default_element(void)
{
   return &default_element;
}

/**
 * Returns a pointer to the first element in the UI element linked list.
 */
element *get_first_element(void)
{
   return first_element;
}

/**
 * Sets the first element in the UI element linked list.
 */
element *set_first_element(element *this_element)
{
   first_element = this_element;

   return first_element;
}

/**
 * Returns a pointer to the global SDL renderer.
 */
SDL_Renderer *get_sdl_renderer(void)
{
   // Check if main thread ID has been initialized
   if (main_thread_id == 0) {
      LOG_ERROR("Main thread ID not initialized!");
      return NULL;
   }

   // Check if current thread is the main thread
   if (SDL_ThreadID() != main_thread_id) {
      LOG_ERROR("get_sdl_renderer() called from non-main thread!");
      return NULL;
   }

   return renderer;
}

/**
 * Enables or disables object detection.
 */
int set_detect_enabled(int enable)
{
   detect_enabled = enable;

   return detect_enabled;
}

/**
 * Checks if the application is in the process of shutting down.
 */
int checkShutdown(void)
{
   return quit;
}

/**
 * Gets the current window dimensions.
 */
int get_window_size(int *width, int *height)
{
   *width = window_width;
   *height = window_height;

   return SUCCESS;
}

void mqttViewingSnapshot(const char *filename);

/*
 * Updates the AI assistant name and state.
 */
void process_ai_state(const char *newAIName, const char *newAIState) {
   snprintf(aiName, AI_NAME_MAX_LENGTH, "%s", newAIName);
   snprintf(aiState, AI_STATE_MAX_LENGTH, "%s", newAIState);
}

/*
 * Returns the AI given name for sending back MQTT messages.
 */
const char *get_ai_name(void) {
   return (const char *) aiName;
};

/*
 * Returns the current AI state that was last send by the AI.
 */
const char *get_ai_state(void) {
   return (const char *) aiState;
};

/*
 * Returns the current FPS calculation from the main loop.
 */
int get_curr_fps(void) {
   return curr_fps;
}

/* Free the UI element list. */
void free_elements(element *start_element)
{
   element *this_element = start_element;
   element *next_element = NULL;

   if (start_element == NULL) {
      LOG_ERROR("Unable to free NULL elements!");
      return;
   }

   while (this_element != NULL) {
      // Save next element before freeing current
      next_element = this_element->next;

#ifdef DEBUG_SHUTDOWN
      LOG_INFO("Freeing: %d.", this_element->type);
#endif

      if (this_element->surface != NULL) {
#ifdef DEBUG_SHUTDOWN
         LOG_INFO("Freeing surface.");
#endif
         SDL_FreeSurface(this_element->surface);
      }

      if (this_element->texture != NULL) {
#ifdef DEBUG_SHUTDOWN
         LOG_INFO("Freeing texture.");
#endif
         SDL_DestroyTexture(this_element->texture);
      }

      if (this_element->texture_r != NULL) {
#ifdef DEBUG_SHUTDOWN
         LOG_INFO("Freeing texture (r).");
#endif
         SDL_DestroyTexture(this_element->texture_r);
      }

      if (this_element->texture_s != NULL) {
#ifdef DEBUG_SHUTDOWN
         LOG_INFO("Freeing texture (s).");
#endif
         SDL_DestroyTexture(this_element->texture_s);
      }

      if (this_element->texture_rs != NULL) {
#ifdef DEBUG_SHUTDOWN
         LOG_INFO("Freeing texture (rs).");
#endif
         SDL_DestroyTexture(this_element->texture_rs);
      }

      if (this_element->texture_l != NULL) {
#ifdef DEBUG_SHUTDOWN
         LOG_INFO("Freeing texture (l).");
#endif
         SDL_DestroyTexture(this_element->texture_l);
      }

      if (this_element->texture_w != NULL) {
#ifdef DEBUG_SHUTDOWN
         LOG_INFO("Freeing texture (w).");
#endif
         SDL_DestroyTexture(this_element->texture_w);
      }

      if (this_element->texture_p != NULL) {
#ifdef DEBUG_SHUTDOWN
         LOG_INFO("Freeing texture (p).");
#endif
         SDL_DestroyTexture(this_element->texture_p);
      }

      if (this_element->texture_base != NULL) {
#ifdef DEBUG_SHUTDOWN
         LOG_INFO("Freeing texture (base).");
#endif
         SDL_DestroyTexture(this_element->texture_base);
      }

      if (this_element->texture_online != NULL) {
#ifdef DEBUG_SHUTDOWN
         LOG_INFO("Freeing texture (online).");
#endif
         SDL_DestroyTexture(this_element->texture_online);
      }

      if (this_element->texture_warning != NULL) {
#ifdef DEBUG_SHUTDOWN
         LOG_INFO("Freeing texture (warning).");
#endif
         SDL_DestroyTexture(this_element->texture_warning);
      }

      if (this_element->texture_offline != NULL) {
#ifdef DEBUG_SHUTDOWN
         LOG_INFO("Freeing texture (offline).");
#endif
         SDL_DestroyTexture(this_element->texture_offline);
      }

      if (this_element->metrics_textures != NULL) {
         for (int i = 0; i < this_element->metrics_texture_count; i++) {
            if (this_element->metrics_textures[i] != NULL) {
#ifdef DEBUG_SHUTDOWN
               LOG_INFO("Freeing texture (metrics_textures).");
#endif
               SDL_DestroyTexture(this_element->metrics_textures[i]);
            }
         }
         free(this_element->metrics_textures);
         this_element->metrics_textures = NULL;
      }

      if (this_element->last_metrics_text != NULL) {
         for (int i = 0; i < this_element->metrics_texture_count; i++) {
            if (this_element->last_metrics_text[i] != NULL) {
#ifdef DEBUG_SHUTDOWN
               LOG_INFO("Freeing text (last_metrics_text).");
#endif
               free(this_element->last_metrics_text[i]);
            }
         }
         free(this_element->last_metrics_text);
         this_element->last_metrics_text = NULL;
      }

      for (int i = 0; i < this_element->this_anim.frame_count; i++) {
#ifdef DEBUG_SHUTDOWN
         LOG_INFO("Freeing frame %d.", i);
#endif
         free(this_element->this_anim.frame_lookup[i]);
      }

#ifdef DEBUG_SHUTDOWN
      LOG_INFO("Freeing element.");
#endif
      free(this_element);

      // Move to next element
      this_element = next_element;
   }
}

/* Debug function to check proper processing of UI elements. */
void dump_element_list(void)
{
   element *curr_element = first_element;
   int count = 0;

   while (curr_element != NULL) {
      switch (curr_element->type) {
      case STATIC:
         LOG_INFO("Element[%d]:\n"
                "\ttype:\tSTATIC\n"
                "\tfile:\t%s\n"
                "\tdest_x:\t%d\n"
                "\tdest_y:\t%d\n"
                "\tangle:\t%f\n"
                "\tlayer:\t%d",
                count,
                curr_element->filename, curr_element->dest_x, curr_element->dest_y,
                curr_element->angle, curr_element->layer);
         break;
      case ANIMATED:
         LOG_INFO("Element[%d]:\n"
                "\ttype:\tANIMATED\n"
                "\tfile:\t%s\n"
                "\tdest_x:\t%d\n"
                "\tdest_y:\t%d\n"
                "\tangle:\t%f\n"
                "\tlayer:\t%d",
                count,
                curr_element->filename, curr_element->dest_x, curr_element->dest_y,
                curr_element->angle, curr_element->layer);
         break;
      case TEXT:
         LOG_INFO("Element[%d]:\n"
                "\ttype:\tTEXT\n"
                "\tstring:\t%s\n"
                "\tfont:\t%s\n"
                "\tsize:\t%d\n"
                "\tdest_x:\t%d\n"
                "\tdest_y:\t%d\n"
                "\thalign:\t%s\n"
                "\tangle:\t%f\n"
                "\tlayer:\t%d",
                count,
                curr_element->text, curr_element->font, curr_element->font_size,
                curr_element->dest_x, curr_element->dest_y,
                curr_element->halign, curr_element->angle, curr_element->layer);
         break;
      case SPECIAL:
         LOG_INFO("Element[%d]:\n"
                "\ttype:\tSPECIAL\n"
                "\tname:\t%s\n"
                "\tfile:\t%s\n"
                "\tdest_x:\t%d\n"
                "\tdest_y:\t%d\n"
                "\tangle:\t%f\n"
                "\tlayer:\t%d",
                count,
                curr_element->special_name, curr_element->filename,
                curr_element->dest_x, curr_element->dest_y,
                curr_element->angle, curr_element->layer);
         break;
      case ANIMATED_DYNAMIC:
         LOG_INFO("Not implemented.");
         break;
      case ARMOR_COMPONENT:
         LOG_INFO("Not implemented.");
         break;
      }

      count++;
      curr_element = curr_element->next;
   }
}

/* This function plays the intro animation on power up. It is designed to minimize dead loading time.
 * I'd still like to find a better way to do this but due to the way SDL handles threads, it's not
 * that easy. */
int play_intro(int frames, int clear, int *finished)
{
   SDL_Rect src_rect;
   SDL_Rect dst_rect_l, dst_rect_r;
   int frame_count = 0;

   video_out_data *this_vod = get_video_out_data();

#ifdef ENCODE_TIMING
   int cur_time = 0, max_time = 0, min_time = 0, weight = 0;
   double avg_time = 0.0;
#endif

   if (intro_element.texture == NULL) {
      intro_element.texture = IMG_LoadTexture(renderer, intro_element.this_anim.image);
      if (!intro_element.texture) {
         SDL_Log("Couldn't load %s: %s\n", intro_element.filename, SDL_GetError());
         return 1;
      }
   }

   while (frame_count < frames) {
      frame_count++;

      if (clear) {
         SDL_RenderClear(renderer);
      }

      src_rect.x = intro_element.this_anim.current_frame->source_x;
      src_rect.y = intro_element.this_anim.current_frame->source_y;
      src_rect.w = intro_element.this_anim.current_frame->source_w;
      src_rect.h = intro_element.this_anim.current_frame->source_h;

      dst_rect_l.x = dst_rect_r.x =
          intro_element.dest_x + intro_element.this_anim.current_frame->dest_x;
      dst_rect_l.y = dst_rect_r.y =
          intro_element.dest_y + intro_element.this_anim.current_frame->dest_y;
      dst_rect_l.w = dst_rect_r.w = intro_element.this_anim.current_frame->source_w;
      dst_rect_l.h = dst_rect_r.h = intro_element.this_anim.current_frame->source_h;

      if (intro_element.this_anim.current_frame->next != NULL) {
         if (finished != NULL) {
            *finished = 0;
         }
         intro_element.this_anim.current_frame = intro_element.this_anim.current_frame->next;
      } else {
         if (finished != NULL) {
            *finished = 1;
         }
         intro_element.this_anim.current_frame = intro_element.this_anim.first_frame;
      }

      renderStereo(intro_element.texture, &src_rect, &dst_rect_l, &dst_rect_r, intro_element.angle);

      SDL_RenderPresent(renderer);

      if (get_recording_state()) {
#ifdef ENCODE_TIMING
         Uint32 start = 0, stop = 0;
#endif

         this_vod->rgb_out_pixels[this_vod->write_index] =
             malloc(window_width * RGB_OUT_SIZE * window_height);
         if (this_vod->rgb_out_pixels[this_vod->write_index] == NULL) {
            LOG_ERROR("Unable to malloc rgb frame 0.");
            return 2;
         }
#ifdef ENCODE_TIMING
         start = SDL_GetTicks();
#endif
         if (OpenGL_RenderReadPixelsAsync(renderer, NULL, PIXEL_FORMAT_OUT,
                                  this_vod->rgb_out_pixels[this_vod->write_index],
                                  window_width * RGB_OUT_SIZE) != 0) {
            LOG_ERROR("OpenGL_RenderReadPixelsAsync() failed");
            free(this_vod->rgb_out_pixels[this_vod->write_index]);
            this_vod->rgb_out_pixels[this_vod->write_index] = NULL;
#ifdef ENCODE_TIMING
         } else {
            stop = SDL_GetTicks();
            cur_time = stop - start;
            avg_time = ((avg_time * weight) + cur_time) / (weight + 1);
            weight++;
            if (cur_time > max_time)
               max_time = cur_time;
            if ((cur_time < min_time) || (min_time == 0))
               min_time = cur_time;
            LOG_INFO("OpenGL_RenderReadPixelsAsync(): %0.2f ms, min: %d, max: %d. weight: %d",
                   avg_time, min_time, max_time, weight);
#endif
         }

         pthread_mutex_lock(&this_vod->p_mutex);
         if (this_vod->rgb_out_pixels[this_vod->buffer_num] != NULL) {
            free(this_vod->rgb_out_pixels[this_vod->buffer_num]);
            this_vod->rgb_out_pixels[this_vod->buffer_num] = NULL;
         }
         rotate_triple_buffer_indices(this_vod);
         pthread_mutex_unlock(&this_vod->p_mutex);

         if (get_video_out_thread() == 0) {
            pthread_t thread_id;
            if (pthread_create(&thread_id, NULL, video_next_thread, NULL) != 0) {
               LOG_ERROR("Error creating video output thread.");
               set_recording_state(DISABLED);
            } else {
               set_video_out_thread(thread_id);
            }
         }
      }

      SDL_Delay(33);
   }

   return 0;
}

/* Pthread function to background object detection. Runs to completion once per frame. */
void *object_detection_thread(void *arg)
{
   //int detects = 0;
   od_data *my_data = (od_data *) arg;

   detect_image(&my_data->detect_obj, my_data->pix_data, this_detect[my_data->eye], MAX_DETECT);
   //detects = detect_image(&my_data->detect_obj, my_data->pix_data, this_detect[my_data->eye], MAX_DETECT);
   //printf("Objects detected: %d\n", detects);

   my_data->complete = 1;

   return NULL;
}

/**
 * Builds a complete GStreamer pipeline string for stereo camera setup
 * @param descr Output buffer for the complete pipeline string
 * @param descr_size Size of the output buffer
 * @param cam_type Type of camera ("csi" or "usb")
 * @param this_hds Struct containing camera configuration parameters
 *
 * The function constructs separate pipelines for left and right cameras
 * based on the camera type and combines them into a single pipeline string.
 * For CSI cameras, sensor_id 0 is used for left and 1 for right.
 * For USB cameras, /dev/video0 is used for left and /dev/video2 for right.
 */
static void build_pipeline_string(char* descr, size_t descr_size, const char* cam_type,
                                  const hud_display_settings* this_hds) {
   char left_pipeline[GSTREAMER_PIPELINE_LENGTH/2];
   char right_pipeline[GSTREAMER_PIPELINE_LENGTH/2];
   bool is_csi = (cam_type == NULL) || (strncmp(cam_type, "csi", 3) == 0);

   if (is_csi) {
      if (cam1_id == -1) {
         cam1_id = DEFAULT_CSI_CAM1;
         cam2_id = DEFAULT_CSI_CAM2;
      }
      g_snprintf(left_pipeline, sizeof(left_pipeline), GST_CAM_PIPELINE_CSI_INPUT GST_CAM_PIPELINE_OUTPUT,
                 cam1_id, this_hds->cam_input_width, this_hds->cam_input_height,
                 this_hds->cam_input_fps, this_hds->cam_frame_duration,
                 single_cam ? "" : "L");

      if (!single_cam) {
         g_snprintf(right_pipeline, sizeof(right_pipeline), GST_CAM_PIPELINE_CSI_INPUT GST_CAM_PIPELINE_OUTPUT,
                    cam2_id, this_hds->cam_input_width, this_hds->cam_input_height,
                    this_hds->cam_input_fps, this_hds->cam_frame_duration, "R");
      }
   } else {
      if (cam1_id == -1) {
         cam1_id = DEFAULT_USB_CAM1;
         cam2_id = DEFAULT_USB_CAM2;
      }
      g_snprintf(left_pipeline, sizeof(left_pipeline), GST_CAM_PIPELINE_USB_INPUT GST_CAM_PIPELINE_OUTPUT,
                 cam1_id, this_hds->cam_input_width, this_hds->cam_input_height,
                 this_hds->cam_input_fps, this_hds->cam_frame_duration,
                 single_cam ? "" : "L");

      if (!single_cam) {
         g_snprintf(right_pipeline, sizeof(right_pipeline), GST_CAM_PIPELINE_USB_INPUT GST_CAM_PIPELINE_OUTPUT,
                    cam2_id, this_hds->cam_input_width, this_hds->cam_input_height,
                    this_hds->cam_input_fps, this_hds->cam_frame_duration, "R");
      }
   }

   if (single_cam) {
      g_snprintf(descr, descr_size, "%s", left_pipeline);
   } else {
      g_snprintf(descr, descr_size, "%s %s", left_pipeline, right_pipeline);
   }
}

/* Video input handling thread.
 * This thread handles input from the cameras and camera sync using timestamps.
 */
void *video_processing_thread(void *arg)
{
   GstElement *pipeline = NULL, *sinkL = NULL, *sinkR = NULL;
   GstSample *sampleL[2] = { NULL }, *sampleR[2] = { NULL };
   GstBuffer *bufferL[2] = { NULL }, *bufferR[2] = { NULL };
   gchar descr[GSTREAMER_PIPELINE_LENGTH] = "";
   GError *error = NULL;
   gboolean eosL = false, eosR = false;
   const char *cam_type = (const char *) arg;

#ifdef DEBUG_BUFFERS
   int sync_comp = 0;
#endif

   hud_display_settings *this_hds = get_hud_display_settings();

   /* build and start pipeline */
   build_pipeline_string(descr, sizeof(descr), cam_type, this_hds);

   pipeline = gst_parse_launch(descr, &error);
   if (error != NULL) {
      SDL_Log("could not construct pipeline: %s\n", error->message);
      g_error_free(error);
      return NULL;
   }

   /* get sink */
   if (single_cam) {
      sinkL = gst_bin_get_by_name(GST_BIN(pipeline), "sink");
   } else {
      sinkL = gst_bin_get_by_name(GST_BIN(pipeline), "sinkL");
      sinkR = gst_bin_get_by_name(GST_BIN(pipeline), "sinkR");
   }
   // From here on out, we'll check sinkR to see if we need to process it.

   gst_element_set_state(pipeline, GST_STATE_PLAYING);

   while (!quit) {
      // Unmap buffers and unref the samples
      if (bufferL[!buffer_num] != NULL) {
         gst_buffer_unmap(bufferL[!buffer_num], &mapL[!buffer_num]);
      }
      if (sampleL[!buffer_num] != NULL) {
         gst_sample_unref(sampleL[!buffer_num]);
      }
      if (sinkR) {
         if (bufferR[!buffer_num] != NULL) {
            gst_buffer_unmap(bufferR[!buffer_num], &mapR[!buffer_num]);
         }
         if (sampleR[!buffer_num] != NULL) {
            gst_sample_unref(sampleR[!buffer_num]);
         }
      }

      /* get the preroll buffer from appsink */
      g_signal_emit_by_name(sinkL, "pull-sample", &sampleL[!buffer_num], NULL);
      if (sinkR) {
         g_signal_emit_by_name(sinkR, "pull-sample", &sampleR[!buffer_num], NULL);
      }

      if (sampleL[!buffer_num] == NULL) {
         eosL = gst_app_sink_is_eos(GST_APP_SINK(sinkL));
         if (eosL) {
            LOG_ERROR("sinkL returned NULL. It is EOS.");
            quit = 1;
            return NULL;
         } else {
            LOG_ERROR("sinkL returned NULL. It is NOT EOS!?!?");
            continue;
         }
      }

      if (sinkR) {
         if (sampleR[!buffer_num] == NULL) {
            eosR = gst_app_sink_is_eos(GST_APP_SINK(sinkR));
            if (eosR) {
               LOG_ERROR("sinkR returned NULL. It is EOS.");
               quit = 1;
               return NULL;
            } else {
               LOG_ERROR("sinkR returned NULL. It is NOT EOS!?!?");
               continue;
            }
         }
      }

#ifdef DISPLAY_TIMING
      clock_gettime(CLOCK_REALTIME, &ts_cap[!buffer_num]);
#endif

      /* if we have a buffer now, convert it to a pixbuf */
      /* We only need to sync left and right if they're from different cameras. */
      if (sinkR) {
         if (sampleL[!buffer_num] && sampleR[!buffer_num]) {
            /* get the pixbuf */
            bufferL[!buffer_num] = gst_sample_get_buffer(sampleL[!buffer_num]);
            bufferR[!buffer_num] = gst_sample_get_buffer(sampleR[!buffer_num]);

            /* handle sync */
            while (((long)bufferL[!buffer_num]->pts - (long)bufferR[!buffer_num]->pts) >
                  this_hds->cam_frame_duration) {
               gst_sample_unref(sampleR[!buffer_num]);
               g_signal_emit_by_name(sinkR, "pull-sample", &sampleR[!buffer_num], NULL);
               bufferR[!buffer_num] = gst_sample_get_buffer(sampleR[!buffer_num]);
#ifdef DEBUG_BUFFERS
               LOG_WARNING("Catching up R buffer.");
               LOG_WARNING("bufferL PTS: %lu, bufferR PTS: %lu, %10ld: %d, sync_comp: %d",
                     bufferL[!buffer_num]->pts, bufferR[!buffer_num]->pts,
                     (long)bufferL[!buffer_num]->pts - (long)bufferR[!buffer_num]->pts,
                     (this_hds->cam_frame_duration) >
                     abs((long)bufferL[!buffer_num]->pts - (long)bufferR[!buffer_num]->pts),
                     sync_comp);
               sync_comp++;
#endif
            }
            while (((long)bufferR[!buffer_num]->pts - (long)bufferL[!buffer_num]->pts) >
                  this_hds->cam_frame_duration) {
               gst_sample_unref(sampleL[!buffer_num]);
               g_signal_emit_by_name(sinkL, "pull-sample", &sampleL[!buffer_num], NULL);
               bufferL[!buffer_num] = gst_sample_get_buffer(sampleL[!buffer_num]);
#ifdef DEBUG_BUFFERS
               LOG_WARNING("Catching up L buffer.");
               LOG_WARNING("bufferL PTS: %lu, bufferR PTS: %lu, %10ld: %d, sync_comp: %d",
                     bufferL[!buffer_num]->pts, bufferR[!buffer_num]->pts,
                     (long)bufferL[!buffer_num]->pts - (long)bufferR[!buffer_num]->pts,
                     (this_hds->cam_frame_duration) >
                     abs((long)bufferL[!buffer_num]->pts - (long)bufferR[!buffer_num]->pts),
                     sync_comp);
               sync_comp++;
#endif
            }

#ifdef DEBUG_BUFFERS
            LOG_INFO("bufferL PTS: %lu, bufferR PTS: %lu, %10ld: %d, sync_comp: %d",
                  bufferL[!buffer_num]->pts, bufferR[!buffer_num]->pts,
                  (long)bufferL[!buffer_num]->pts - (long)bufferR[!buffer_num]->pts,
                  (this_hds->cam_frame_duration) >
                  abs((long)bufferL[!buffer_num]->pts - (long)bufferR[!buffer_num]->pts), sync_comp);
#endif

            gst_buffer_map(bufferL[!buffer_num], &mapL[!buffer_num], GST_MAP_READ);
            gst_buffer_map(bufferR[!buffer_num], &mapR[!buffer_num], GST_MAP_READ);

            pthread_mutex_lock(&v_mutex);
            video_posted = 1;
            buffer_num = !buffer_num;
            pthread_mutex_unlock(&v_mutex);
         } else {
            g_print("could not make snapshot\n");
         }
      } else {
         if (sampleL[!buffer_num]) {
            /* get the pixbuf */
            bufferL[!buffer_num] = gst_sample_get_buffer(sampleL[!buffer_num]);

            gst_buffer_map(bufferL[!buffer_num], &mapL[!buffer_num], GST_MAP_READ);

            pthread_mutex_lock(&v_mutex);
            video_posted = 1;
            buffer_num = !buffer_num;
            pthread_mutex_unlock(&v_mutex);
         } else {
            g_print("could not make snapshot\n");
         }
      }
   }

   /* Video */
   gst_element_set_state(pipeline, GST_STATE_NULL);
   gst_object_unref(pipeline);

   return NULL;
}

/*
 * Retrieves a font from the font cache or loads it if not present.
 */
TTF_Font *get_local_font(char *font_name, int font_size)
{
   local_font *this_font = NULL;

   if (font_list == NULL) {
      font_list = malloc(sizeof(local_font));
      if (font_list == NULL) {
         return NULL;
      }
      this_font = font_list;
   } else {
      /* Search for existing allocated font. */
      this_font = font_list;
      while (this_font != NULL) {
         if ((strcmp(font_name, this_font->font) == 0) && (font_size == this_font->font_size)) {
            return this_font->ttf_font;
         }

         if (this_font->next == NULL) {
            break;
         } else {
            this_font = this_font->next;
         }
      }

      /* None found. Create new one. */
      this_font->next = malloc(sizeof(local_font));
      if (this_font->next == NULL) {
         return NULL;
      }
      this_font = this_font->next;
   }

   this_font->ttf_font = TTF_OpenFont(font_name, font_size);
   if (!this_font->ttf_font) {
      SDL_Log("Error loading font: %s\n", TTF_GetError());
      free(this_font);
      return NULL;
   }

   strncpy(this_font->font, font_name, MAX_FILENAME_LENGTH * 2);
   this_font->font_size = font_size;
   this_font->next = NULL;

   return this_font->ttf_font;
}

/*
 * Copies the latest camera frame from the left camera into the provided buffer.
 */
void *grab_latest_camera_frame(void *temp_buffer) {
   hud_display_settings *this_hds = get_hud_display_settings();

   /* Use camera frame buffer if available */
   pthread_mutex_lock(&v_mutex); // Lock the video mutex to safely access mapL
   if (video_posted && mapL[buffer_num].data != NULL) {
      /* Allocate temporary buffer for the screenshot */
      temp_buffer = malloc(this_hds->cam_input_width * this_hds->cam_input_height * 4);
      if (temp_buffer != NULL) {
         /* Copy camera data */
         memcpy(temp_buffer, mapL[buffer_num].data,
               this_hds->cam_input_width * this_hds->cam_input_height * 4);
      }
   }
   pthread_mutex_unlock(&v_mutex);

   return temp_buffer;
}

/*
 * Renders a texture to both eyes in a stereo display.
 */
void renderStereo(SDL_Texture *tex, SDL_Rect *src, SDL_Rect *dest,
                  SDL_Rect *dest2, double angle)
{
   SDL_Rect src_rect_l, src_rect_r;
   SDL_Rect dest_rect_l, dest_rect_r;
   int overage = 0;
   double scale = 1.0;

   hud_display_settings *this_hds = get_hud_display_settings();

   if (dest == NULL) {
      LOG_ERROR("ERROR: renderStereo() was passed a NULL dest value!");

      return;
   }

   if (src == NULL) {
      SDL_QueryTexture(tex, NULL, NULL, &src_rect_l.w, &src_rect_l.h);
      src_rect_l.x = src_rect_l.y = 0;

      memcpy(&src_rect_r, &src_rect_l, sizeof(SDL_Rect));
   } else {
      memcpy(&src_rect_l, src, sizeof(SDL_Rect));
      memcpy(&src_rect_r, src, sizeof(SDL_Rect));
   }

   memcpy(&dest_rect_l, dest, sizeof(SDL_Rect));
   if (dest2 == NULL) {
      memcpy(&dest_rect_r, dest, sizeof(SDL_Rect));
   } else {
      memcpy(&dest_rect_r, dest2, sizeof(SDL_Rect));
   }
   dest_rect_r.x += this_hds->eye_output_width;

   /* Handle bounds for left eye */

   /* Left edge of left eye */
   if (dest_rect_l.x < 0) {
      if (src_rect_l.w != dest_rect_l.w) {
         scale = ((double) src_rect_l.w) / ((double) dest_rect_l.w);
      }

      overage = -dest_rect_l.x;
      src_rect_l.x += (scale * overage);
      src_rect_l.w -= (scale * overage);
      dest_rect_l.w -= overage;
      dest_rect_l.x = 0;
   }

   /* Right edge of left eye */
   if ((dest_rect_l.x + dest_rect_l.w) > this_hds->eye_output_width) {
      if (src_rect_l.w != dest_rect_l.w) {
         scale = ((double) src_rect_l.w) / ((double) dest_rect_l.w);
      }

      overage = (dest_rect_l.x + dest_rect_l.w) - this_hds->eye_output_width;
      dest_rect_l.w = dest_rect_l.w - overage;
      src_rect_l.w = src_rect_l.w - (scale * overage);
   }

   /* Top edge of left eye */
   if (dest_rect_l.y < 0) {
      if (src_rect_l.h != dest_rect_l.h) {
         scale = ((double) src_rect_l.h) / ((double) dest_rect_l.h);
      }

      overage = -dest_rect_l.y;
      src_rect_l.y += (scale * overage);
      src_rect_l.h -= (scale * overage);
      dest_rect_l.h -= overage;
      dest_rect_l.y = 0;
   }

   /* Bottom edge of left eye */
   if ((dest_rect_l.y + dest_rect_l.h) > this_hds->eye_output_height) {
      if (src_rect_l.h != dest_rect_l.h) {
         scale = ((double) src_rect_l.h) / ((double) dest_rect_l.h);
      }

      overage = (dest_rect_l.y + dest_rect_l.h) - this_hds->eye_output_height;
      dest_rect_l.h = dest_rect_l.h - overage;
      src_rect_l.h = src_rect_l.h - (scale * overage);
   }

   /* Handle bounds for right eye */

   /* Left edge of right eye */
   if (dest_rect_r.x < this_hds->eye_output_width) {
      if (src_rect_r.w != dest_rect_r.w) {
         scale = ((double) src_rect_r.w) / ((double) dest_rect_r.w);
      }

      overage = this_hds->eye_output_width - dest_rect_r.x;
      src_rect_r.x += (scale * overage);
      src_rect_r.w -= (scale * overage);
      dest_rect_r.w -= overage;
      dest_rect_r.x = this_hds->eye_output_width;
   }

   /* Right edge of right eye */
   if ((dest_rect_r.x + dest_rect_r.w) > (2 * this_hds->eye_output_width)) {
      if (src_rect_r.w != dest_rect_r.w) {
         scale = ((double) src_rect_r.w) / ((double) dest_rect_r.w);
      }

      overage = (dest_rect_r.x + dest_rect_r.w) - (2 * this_hds->eye_output_width);
      dest_rect_r.w -= overage;
      src_rect_r.w -= (scale * overage);
   }

   /* Top edge of right eye */
   if (dest_rect_r.y < 0) {
      if (src_rect_r.h != dest_rect_r.h) {
         scale = ((double) src_rect_r.h) / ((double) dest_rect_r.h);
      }

      overage = -dest_rect_r.y;
      src_rect_r.y += (scale * overage);
      src_rect_r.h -= (scale * overage);
      dest_rect_r.h -= overage;
      dest_rect_r.y = 0;
   }

   /* Bottom edge of right eye */
   if ((dest_rect_r.y + dest_rect_r.h) > this_hds->eye_output_height) {
      if (src_rect_r.h != dest_rect_r.h) {
         scale = ((double) src_rect_r.h) / ((double) dest_rect_r.h);
      }

      overage = (dest_rect_r.y + dest_rect_r.h) - this_hds->eye_output_height;
      dest_rect_r.h = dest_rect_r.h - overage;
      src_rect_r.h = src_rect_r.h - (scale * overage);
   }

   /* Render each eye independently */
   /* Left eye */
   if (dest_rect_l.w > 0 && dest_rect_l.h > 0 && src_rect_l.w > 0 && src_rect_l.h > 0) {
      if (!angle) {
         SDL_RenderCopy(renderer, tex, &src_rect_l, &dest_rect_l);
      } else {
         SDL_RenderCopyEx(renderer, tex, &src_rect_l, &dest_rect_l, angle, NULL, SDL_FLIP_NONE);
      }
   }

   /* Right eye */
   if (dest_rect_r.w > 0 && dest_rect_r.h > 0 && src_rect_r.w > 0 && src_rect_r.h > 0) {
      if (!angle) {
         SDL_RenderCopy(renderer, tex, &src_rect_r, &dest_rect_r);
      } else {
         SDL_RenderCopyEx(renderer, tex, &src_rect_r, &dest_rect_r, angle, NULL, SDL_FLIP_NONE);
      }
   }
}

/*
 * Sends a text message to be spoken via text-to-speech over MQTT.
 */
void mqttTextToSpeech(const char *text) {
   char mqtt_command[1024] = "";
   int rc = 0;

   // Construct the MQTT command with the provided text
   snprintf(mqtt_command, sizeof(mqtt_command),
            "{ \"device\": \"text to speech\", \"action\": \"play\", \"value\": \"%s\" }",
            text);

   if (mosq == NULL) {
      LOG_ERROR("MQTT not initialized trying to send: \"%s\"", text);
   } else {
      rc = mosquitto_publish(mosq, NULL, "dawn", strlen(mqtt_command), mqtt_command, 0, false);
      if (rc != MOSQ_ERR_SUCCESS) {
         LOG_ERROR("Error publishing: %s", mosquitto_strerror(rc));
      }
   }
}

/*
 * Sends a text string (JSON hopefully) over MQTT.
 */
void mqttSendMessage(const char *topic, const char *text) {
   int rc = 0;

   if (mosq == NULL) {
      LOG_ERROR("MQTT not initialized.");
   } else {
      rc = mosquitto_publish(mosq, NULL, topic, strlen(text), text, 0, false);
      if (rc != MOSQ_ERR_SUCCESS) {
         LOG_ERROR("Error publishing: %s", mosquitto_strerror(rc));
      } else {
         LOG_INFO("Successfully send via MQTT: %s", text);
      }
   }
}

void display_help(int argc, char *argv[]) {
   if (argc > 0) {
      printf("Usage: %s [options]\n", argv[0]);
   } else {
      printf("Usage: [options]\n");  // Fallback if command name is not available
   }

   printf("Options:\n");
   printf("  -f, --fullscreen       Run in fullscreen mode.\n");
   printf("  -h, --help             Display this help message and exit.\n");
   printf("  -l, --logfile LOGFILE  Specify the log filename instead of stdout/stderr.\n");
   printf("\n");
   printf("  -p, --record_path PATH Specify the path for recordings.\n");
   printf("  -r, --record           Start recording on startup.\n");
   printf("  -s, --stream           Start streaming on startup.\n");
   printf("  -t, --record_stream    Start both recording and streaming on startup.\n");
   printf("\n");
   printf("  -u, --usb              Connect via USB/serial.\n");
   printf("  -d, --device DEVICE    Specify the device for USB/serial connection.\n");
   printf("  -c, --camera TYPE      Specify the camera type, csi or usb.\n");
   printf("  -n, --camcount [1/2]   Specify the number of cameras for display, 1 or 2.\n");
   printf("  -b, --black-background  Disable cameras and use black background (for UI design/transparency).\n");
   printf("\n");
}

/* MAIN: Start here. */
int main(int argc, char **argv)
{
   /* Variable Inits */
   hud_display_settings *this_hds = get_hud_display_settings();
   armor_settings *this_as = get_armor_settings();

   /* Graphics */
   SDL_Window *window = NULL;
   window_width = this_hds->eye_output_width * 2;
   window_height = this_hds->eye_output_height;
   int native_width = this_hds->eye_output_width * 2;
   int native_height = this_hds->eye_output_height;
   Uint32 sdl_flags = 0;
   SDL_Event event;

   element *curr_element = NULL;

   /* getopt */
   int opt = 0;
   int fullscreen = 0;
   char record_path[PATH_MAX];   /* Where do we store recordings? */
   int no_camera_mode = 0;       /* Flag to enable no camera mode */
   DestinationType initial_recording_state = DISABLED;

   /* Threads */
   pthread_t thread_handles[NUM_AUDIO_THREADS];
   int current_thread = 0;
   pthread_t video_proc_thread = 0;

   /* Serial Port */
   char usb_enable = 0;
   char usb_port[24] = USB_PORT;

   int rc = -1;

   pthread_t command_proc_thread = 0;

   /* Video */
   SDL_Texture *textureL = NULL, *textureR = NULL;

#ifdef DISPLAY_TIMING
   unsigned long last_ts_cap = 0, present_time = 0, ts_total = 0;
   unsigned int ts_count = 0;
   struct timespec display_time = { .tv_sec = 0, .tv_nsec = 0};
#endif

   init_video_out_data();

   local_font *this_font = NULL;

   unsigned int totalFrames = 0;
   unsigned int currTime = SDL_GetTicks();
   unsigned int last_file_check = 0;

   Uint64 thisPTime, lastPTime;
   double elapsed = 0.0;

#ifdef ENCODE_TIMING
   int cur_time = 0, max_time = 0, min_time = 0, weight = 0;
   double avg_time = 0.0;
#endif

   FrameRateTracker tracker;
   initializeFrameRateTracker(&tracker);

   char config_file[] = "config.json";

   int intro_finished = 0;

   off_t last_size = -1;
   off_t last_last_size = -1;
   /* End Variable Inits */

   sdl_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_BORDERLESS | SDL_WINDOW_ALLOW_HIGHDPI;

   /*
    * Process Command Line
    * c: - camera type, usb/csi
    * f  - fullscreen
    * h  - help text
    * l  - log filename
    * n: - number of cameras
    * p: - path for recordings
    * r  - record on startup
    * s  - stream on startup
    * t  - record and stream on startup
    * u: - USB/serial with port
    */
   static struct option long_options[] = {
      {"black-background", no_argument, NULL, 'b'},
      {"camera", required_argument, NULL, 'c'},
      {"device", required_argument, NULL, 'd'},
      {"fullscreen", no_argument, NULL, 'f'},
      {"help", no_argument, NULL, 'h'},
      {"logfile", required_argument, NULL, 'l'},
      {"camcount", required_argument, NULL, 'n'},
      {"record_path", required_argument, NULL, 'p'},
      {"record", no_argument, NULL, 'r'},
      {"stream", no_argument, NULL, 's'},
      {"record_stream", no_argument, NULL, 't'},
      {"usb", no_argument, NULL, 'u'},
      {0, 0, 0, 0}
   };
   int option_index = 0;

   const char *log_filename = NULL;
   const char *cam_type = NULL;

   printf("%s Version %s: %s\n", APP_NAME, VERSION_NUMBER, GIT_SHA);

   if (SDL_Init(SDL_INIT_VIDEO) == -1) {
      fprintf(stderr, "SDL_Init(SDL_INIT_VIDEO) failed: %s\n", SDL_GetError());
      return EXIT_FAILURE;
   }

   /* This is a safe guard to protect SDL, since it's not thread safe. */
   main_thread_id = SDL_ThreadID();

   if (getcwd(record_path, sizeof(record_path)) == NULL) {
      fprintf(stderr, "getcwd() error!");

      return EXIT_FAILURE;
   }

   while (1) {
      opt = getopt_long(argc, argv, "bc:d:fhl:n:p:rstu", long_options, &option_index);

      if (opt == -1) {
         break;
      }

      switch (opt) {
      case 'b':
         no_camera_mode = 1;
         printf("No camera mode enabled - cameras disabled");
         break;
      case 'c':
         if ((strncmp(optarg, "usb", 3) != 0) && (strncmp(optarg, "csi", 3) != 0)) {
            fprintf(stderr, "Camera type must be \"usb\" or \"csi\".\n");
            return EXIT_FAILURE;
         }

         cam_type = optarg;
         break;
      case 'f':
         fullscreen = 1;

         break;
      case 'h':
         display_help(argc, argv);
         return EXIT_SUCCESS;
      case 'l':
         log_filename = optarg;
         break;
      case 'n':
         char *token = NULL;
         int values[3] = {-1, -1, -1}; // Store camcount and optional camera IDs
         int index = 0;

         // Create a copy of optarg since strtok modifies the string
         char *optarg_copy = strdup(optarg);
         if (!optarg_copy) {
            fprintf(stderr, "Memory allocation failed!\n");
            return EXIT_FAILURE;
         }

         // Parse comma-separated values
         token = strtok(optarg_copy, ",");
         while (token && index < 3) {
            values[index] = atoi(token);
            token = strtok(NULL, ",");
            index++;
         }

         // Validate camera count
         if (values[0] != 1 && values[0] != 2) {
            fprintf(stderr, "camcount (number of cameras) must be 1 or 2!\n");
            free(optarg_copy);
            return EXIT_FAILURE;
         }

         single_cam = (values[0] == 1);

         // Handle camera IDs if provided
         if (values[0] == 2) {
            if (index > 1) {
               cam1_id = values[1]; // First camera ID
            }
            if (index > 2) {
               cam2_id = values[2]; // Second camera ID
            }
         } else if (values[0] == 1 && index > 1) {
            cam1_id = values[1]; // Single camera ID
         }

         free(optarg_copy);
         break;
      case 'p':
         snprintf(record_path, 256, "%s", optarg);
         break;
      case 'r':
         initial_recording_state = RECORD;
         break;
      case 's':
         initial_recording_state = STREAM;
         break;
      case 't':
         initial_recording_state = RECORD_STREAM;
         break;
      case 'u':
         usb_enable = 1;
         serial_set_state(1, NULL, -1);
         break;
      case 'd':
         strncpy(usb_port, optarg, 24);
         serial_set_state(-1, usb_port, -1);
         break;
      default:
         display_help(argc, argv);
         return EXIT_FAILURE;
      }
   }

   // Initialize logging
   if (log_filename) {
      if (init_logging(log_filename, LOG_TO_FILE) != 0) {
         fprintf(stderr, "Failed to initialize logging to file: %s\n", log_filename);
         return EXIT_FAILURE;
      }
   } else {
      if (init_logging(NULL, LOG_TO_CONSOLE) != 0) {
         fprintf(stderr, "Failed to initialize logging to console\n");
         return EXIT_FAILURE;
      }
   }

   curl_global_init(CURL_GLOBAL_DEFAULT);

   /* Create server message queue */
   struct mq_attr attr;

   attr.mq_flags = 0;
   attr.mq_maxmsg = MAX_MESSAGES;
   attr.mq_msgsize = MAX_MSG_SIZE;
   attr.mq_curmsgs = 0;

   if (mq_unlink(SERVER_QUEUE_NAME) == -1) {
      // Checking but failure isn't important.
      //perror("Server: mq_unlink");
   }

   if ((qd_server = mq_open(SERVER_QUEUE_NAME, O_RDONLY | O_CREAT, QUEUE_PERMISSIONS, &attr)) == -1) {
      perror("Server: mq_open (server)");
      return EXIT_FAILURE;
   }

   /* If we don't get an argument, read from stdin. */
   if (!usb_enable) {
      LOG_WARNING("No serial port reading from stdin.");
   }

   for (current_thread = 0; current_thread < NUM_AUDIO_THREADS; current_thread++) {
      /* Setup Output Device */
      snprintf(audio_threads[current_thread].client_queue_name,
               MAX_FILENAME_LENGTH, "/stark-sound-client-%d", current_thread);
      if (mq_unlink(audio_threads[current_thread].client_queue_name) == -1) {
         // Checking but failure isn't important.
         //perror("Client: mq_unlink");
      }
      /* Create client message queue/read */
      if ((audio_threads[current_thread].qd_client =
           mq_open(audio_threads[current_thread].client_queue_name,
                   O_RDONLY | O_CREAT, QUEUE_PERMISSIONS, &attr)) == -1) {
         perror("Client: mq_open (client)");
      }
      /* server side */
      if ((qd_clients[current_thread] =
           mq_open(audio_threads[current_thread].client_queue_name, O_WRONLY)) == 1) {
         perror("Server: Not able to open client queue");
      }
      if ((audio_threads[current_thread].qd_server = mq_open(SERVER_QUEUE_NAME, O_WRONLY)) == -1) {
         perror("Client: mq_open (server)");
      }

      /* Create Thread */
      audio_threads[current_thread].thread_id = current_thread;
      //strncpy( audio_threads[current_thread].filename, file, MAX_FILENAME_LENGTH );
      //audio_threads[current_thread].start_percent = start_percent;
      audio_threads[current_thread].stop = 1;

      if (pthread_create
          (&thread_handles[current_thread], NULL, audio_thread, &audio_threads[current_thread])) {
         LOG_ERROR("Error creating thread [%d]", current_thread);
         return EXIT_FAILURE;
      }
   }

   /* Send test messages */
#ifdef STARTUP_SOUND
   process_audio_command(SOUND_PLAY, STARTUP_SOUND, 0.0);
#endif

   if (IMG_Init(IMG_INIT_PNG) < 0) {
      LOG_ERROR("Error initializing SDL_image: %s\n", IMG_GetError());
      return EXIT_FAILURE;
   }

   if (TTF_Init() < 0) {
      LOG_ERROR("Error initializing SDL_ttf: %s\n", TTF_GetError());
      return EXIT_FAILURE;
   }

   SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

   // Create window with native dimensions
   if ((window =
        SDL_CreateWindow(argv[0], SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         native_width, native_height,
                         sdl_flags)) == NULL) {
      LOG_ERROR("SDL_CreateWindow() failed: %s\n", SDL_GetError());
      return EXIT_FAILURE;
   }

   // Apply fullscreen immediately if requested
   if (fullscreen) {
      SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
      SDL_ShowCursor(0);
      SDL_GetWindowSize(window, &window_width, &window_height);
   }

   // Create GL context
   SDL_GLContext glContext = SDL_GL_CreateContext(window);
   if (!glContext) {
      LOG_ERROR("Failed to create GL context: %s", SDL_GetError());
      return EXIT_FAILURE;
   }

   // Make the context current
   if (SDL_GL_MakeCurrent(window, glContext) < 0) {
      LOG_ERROR("SDL_GL_MakeCurrent failed: %s", SDL_GetError());
      return EXIT_FAILURE;
   }

   // Initialize GLEW (if you're using GLEW)
   GLenum glewError = glewInit();
   if (GLEW_OK != glewError) {
      LOG_ERROR("GLEW Error: %s", glewGetErrorString(glewError));
      return EXIT_FAILURE;
   }

#ifdef REFRESH_SYNC
   if ((renderer =
        SDL_CreateRenderer(window, -1,
                           SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED)) == NULL) {
#else
   if ((renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED)) == NULL) {
#endif
      LOG_ERROR("SDL_CreateRenderer() failed: %s\n", SDL_GetError());
      return EXIT_FAILURE;
   }

   // Set the logical size to your native resolution
   if (SDL_RenderSetLogicalSize(renderer, native_width, native_height) != 0) {
      SDL_Log("Could not set logical size: %s", SDL_GetError());
   }

   init_pbo_system();
   set_screenshot_recording_path(record_path);
   set_video_recording_path(record_path);
   init_video_out_data();

   /* Init detect array */
   for (int j = 0; j < MAX_DETECT; j++) {
      this_detect[0][j].active = 0;
      this_detect[1][j].active = 0;
   }

   init_hud_manager();

   intro_element.enabled = 0;

   if (parse_json_config(config_file) == FAILURE) {
      LOG_ERROR("Failed to parse config file. Exiting.");
      return EXIT_FAILURE;
   }

#ifndef ORIGINAL_RATIO
   SDL_Rect v_src_rect = { this_hds->cam_crop_x, 0, this_hds->cam_crop_width, this_hds->cam_input_height };
   SDL_Rect v_dst_rectL = { 0, 0, this_hds->eye_output_width, this_hds->eye_output_height };
   SDL_Rect v_dst_rectR = { this_hds->eye_output_width, 0, this_hds->eye_output_width, this_hds->eye_output_height };
#else
   double input_ratio = (double) this_hds->cam_input_height / this_hds->cam_input_width;
   SDL_Rect v_src_rect = { 0, 0, this_hds->cam_input_width, this_hds->cam_input_height };
   SDL_Rect v_dst_rectL = { 0, (this_hds->eye_output_height-(this_hds->eye_output_height*input_ratio))/2,
                            this_hds->eye_output_width, this_hds->eye_output_height*input_ratio };
   SDL_Rect v_dst_rectR = { this_hds->eye_output_width,
                            (this_hds->eye_output_height-(this_hds->eye_output_height*input_ratio))/2,
                            this_hds->eye_output_width, this_hds->eye_output_height*input_ratio };
#endif


   textureL =
       SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC,
                         this_hds->cam_input_width, this_hds->cam_input_height);
   if (textureL == NULL) {
      SDL_Log("SDL_CreateTexture() failed on textureL: %s\n", SDL_GetError());
      return EXIT_FAILURE;
   }
   textureR =
       SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC,
                         this_hds->cam_input_width, this_hds->cam_input_height);
   if (textureR == NULL) {
      SDL_Log("SDL_CreateTexture() failed on textureR: %s\n", SDL_GetError());
      return EXIT_FAILURE;
   }

   /* Draw a background pattern in case the image has transparency */
   SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
   SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);

   gst_init(&argc, &argv);

   if (intro_element.enabled) {
      play_intro(30, 1, NULL);
   }

   mosquitto_lib_init();

   mosq = mosquitto_new(NULL, true, NULL);
   if(mosq == NULL){
      LOG_ERROR("Error: Out of memory.");
      return EXIT_FAILURE;
   }

   /* Configure callbacks. This should be done before connecting ideally. */
   mosquitto_connect_callback_set(mosq, on_connect);
   mosquitto_subscribe_callback_set(mosq, on_subscribe);
   mosquitto_message_callback_set(mosq, on_message);

   /* Connect to local MQTT server. */
   //rc = mosquitto_connect(mosq, "192.168.10.1", 1883, 60);
   rc = mosquitto_connect(mosq, "127.0.0.1", 1883, 60);
   if(rc != MOSQ_ERR_SUCCESS){
      mosquitto_destroy(mosq);
      LOG_ERROR("Error: %s", mosquitto_strerror(rc));
      return EXIT_FAILURE;
   }

   /* Start processing MQTT events. */
   mosquitto_loop_start(mosq);
   /* End Setup */

   if (initial_recording_state != DISABLED) {
      set_recording_state(initial_recording_state);
   }

   if (intro_element.enabled) {
      play_intro(15, 1, NULL);
   }

   //od_data oddataL, oddataR;
   oddataL.complete = 1;
   oddataL.processed = 1;
   oddataL.pix_data = NULL;
   oddataR.complete = 1;
   oddataR.processed = 1;
   oddataR.pix_data = NULL;
   detect_enabled = 0;     /* Disabling due to bug. */
   if (detect_enabled) {
      if (init_detect(&oddataL.detect_obj, argc, argv, this_hds->cam_input_width, this_hds->cam_input_height))
      {
         LOG_ERROR("Error initializing detect!!!");
         detect_enabled = 0;
      } else {
         if (intro_element.enabled) {
            play_intro(15, 1, NULL);
         }
         if (init_detect(&oddataR.detect_obj, argc, argv, this_hds->cam_input_width, this_hds->cam_input_height))
         {
            LOG_ERROR("Error initializing detect!!!");
            detect_enabled = 0;
         } else {
            //oddataL.detect_obj.v_mutex = &v_mutex;
            //oddataR.detect_obj.v_mutex = &v_mutex;
         }
      }
   }

   if (!no_camera_mode) {
      if (pthread_create(&video_proc_thread, NULL, video_processing_thread, (void *) cam_type) != 0) {
         LOG_ERROR("Error creating video processing thread.");
         return EXIT_FAILURE;
      }
   } else {
      LOG_INFO("Running in no camera mode, video processing thread not started");
   }


   lastPTime = SDL_GetPerformanceCounter();

   if (intro_element.enabled) {
      play_intro(15, 1, NULL);
   }

   if (!usb_enable) {
      strcpy(usb_port, "");

      if (pthread_create(&command_proc_thread, NULL, socket_command_processing_thread, NULL) != 0) {
         LOG_ERROR("Error creating command processing thread.");
         return EXIT_FAILURE;
      }
   } else {
      if (pthread_create(&command_proc_thread, NULL, serial_command_processing_thread, (void *) usb_port) != 0) {
         LOG_ERROR("Error creating command processing thread.");
         return EXIT_FAILURE;
      }
   }

   mqttTextToSpeech("Your hud is now online boss.");

   while (!quit) {
      totalFrames++;

      while (SDL_PollEvent(&event)) {
         switch (event.type) {
         case SDL_KEYUP:
            /* Check for hotkeys. */
            curr_element = first_element;
            while (curr_element != NULL) {
               if (strncmp(curr_element->hotkey, "", 2) != 0) {
                  //printf("KEY: %d, %d\n", event.key.keysym.sym,
                  //       SDL_GetKeyFromName(curr_element->hotkey));
                  if (event.key.keysym.sym == SDL_GetKeyFromName(curr_element->hotkey)) {
                     curr_element->enabled = !curr_element->enabled;
                     if (strncmp(curr_element->special_name, "detect", 6) == 0) {
                        LOG_INFO("Changing detect status.");
                        detect_enabled = !detect_enabled;
                     }
                     LOG_INFO("Changing status.");
                  }
               }

               curr_element = curr_element->next;
            }

            /* Handle hotkeys for HUD switching */
            hud_screen *screen = get_hud_manager()->screens;
            while (screen != NULL) {
               if (screen->hotkey[0] != '\0') {
                  if (event.key.keysym.sym == SDL_GetKeyFromName(screen->hotkey)) {
                     /* Use default transition settings when using hotkeys */
                     switch_to_hud(screen->name, get_hud_manager()->transition_type,
                                  get_hud_manager()->transition_duration_ms);
                     break;
                  }
               }
               screen = screen->next;
            }

            /* Process special keys. */
            switch (event.key.keysym.sym) {
            case SDLK_f:
               if (get_recording_state() != DISABLED) {
                  LOG_WARNING("Unable to change window size while recording.");
               } else {
                  if (fullscreen) {
                     LOG_INFO("Switching to windowed mode.");
                     SDL_SetWindowFullscreen(window, 0);
                     SDL_ShowCursor(1);
                  } else {
                     LOG_INFO("Switching to fullscreen mode.");
                     SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
                     SDL_ShowCursor(0);
                  }
                  SDL_GetWindowSize(window, &window_width, &window_height);
                  fullscreen = !fullscreen;
               }
               break;

            case SDLK_p: // 'P' for photo/screenshot
               LOG_INFO("Requesting full-resolution screenshot with overlay...");
               request_screenshot(1, 1, NULL, SCREENSHOT_MANUAL); // With overlay, full resolution
               break;

            case SDLK_o: // 'O' for raw screenshot (without overlay)
               LOG_INFO("Requesting full-resolution raw camera screenshot...");
               request_screenshot(0, 1, NULL, SCREENSHOT_MANUAL); // Without overlay, full resolution
               break;

            case SDLK_r:
               if (get_recording_state() == DISABLED) {
                  set_recording_state(RECORD);
                  last_file_check = currTime;
                  LOG_INFO("Starting recording.");
               } else {
                  set_recording_state(DISABLED);
                  last_size = last_last_size = -1;
                  LOG_INFO("Stopping recording.");
               }
               break;

            case SDLK_s:
               if (get_recording_state() == DISABLED) {
                  set_recording_state(STREAM);
                  last_file_check = currTime;
                  LOG_INFO("Starting streaming.");
               } else {
                  set_recording_state(DISABLED);
                  last_size = last_last_size = -1;
                  LOG_INFO("Stopping streaming.");
               }
               break;

            case SDLK_t:
               if (get_recording_state() == DISABLED) {
                  set_recording_state(RECORD_STREAM);
                  last_file_check = currTime;
                  LOG_INFO("Starting recording and streaming.");
               } else {
                  set_recording_state(DISABLED);
                  last_size = last_last_size = -1;
                  LOG_INFO("Stopping recording and streaming.");
               }
               break;

            case SDLK_LEFT:
               this_hds->stereo_offset -= 10;
               LOG_INFO("Stereo Offset: %d", this_hds->stereo_offset);
               break;

            case SDLK_RIGHT:
               this_hds->stereo_offset += 10;
               LOG_INFO("Stereo Offset: %d", this_hds->stereo_offset);
               break;

            case SDLK_TAB:  // Tab key to cycle through HUDs
               switch_to_next_hud();
               break;

            case SDLK_ESCAPE:
            case SDLK_q:
               quit = 1;
               break;

            default:
               break;
            }
            break;
         case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                int newWidth = event.window.data1;
                int newHeight = event.window.data2;

                // Safely update the shared width and height
                pthread_mutex_lock(&windowSizeMutex);
                window_width = newWidth;
                window_height = newHeight;
                pthread_mutex_unlock(&windowSizeMutex);

                LOG_INFO("Window resized to: %dx%d", newWidth, newHeight);
            }
            break;
         case SDL_QUIT:
            quit = 1;
            break;
         default:
            break;
         }
      }

      SDL_RenderClear(renderer);

      currTime = SDL_GetTicks();

      thisPTime = SDL_GetPerformanceCounter();

      elapsed = (thisPTime - lastPTime) / (float)SDL_GetPerformanceFrequency();
      curr_fps = (int)(1.0f / elapsed);
      if (curr_fps == 0) {
         curr_fps = 60;
      }
      lastPTime = thisPTime;

      updateFrameRateTracker(&tracker, elapsed);

      if (tracker.elapsedTime > 1.0) {
         averageFrameRate = calculateAverageFrameRate(&tracker);
         tracker.elapsedTime = 0.0;
      }

#ifdef FPS_STATS
      printf("FPS Stats: %03d, avg: %03.0f, min: %03.0f, max: %03.0f\n",
             curr_fps, averageFrameRate, tracker.minFrameRate, tracker.maxFrameRate);
#endif

      /* Video Processing */
      if (!no_camera_mode) {
      pthread_mutex_lock(&v_mutex);
      if (video_posted) {
         if (detect_enabled) {
#if defined(OD_PROPER_WAIT) && defined(USE_CUDA)
            oddataL.pix_data = mapL[buffer_num].data;
            oddataL.eye = 0;
            cudaMemcpy(oddataL.detect_obj.d_image, oddataL.pix_data,
                       oddataL.detect_obj.l_width * oddataL.detect_obj.l_height * sizeof(uchar4), cudaMemcpyHostToDevice);
            detect_image(&oddataL.detect_obj, oddataL.pix_data, this_detect[oddataL.eye], MAX_DETECT);

            if (!single_cam) {
               oddataR.pix_data = mapR[buffer_num].data;
               oddataR.eye = 1;
               cudaMemcpy(oddataR.detect_obj.d_image, oddataR.pix_data,
                          oddataR.detect_obj.l_width * oddataR.detect_obj.l_height * sizeof(uchar4), cudaMemcpyHostToDevice);
               detect_image(&oddataR.detect_obj, oddataR.pix_data, this_detect[oddataR.eye], MAX_DETECT);
            }
#else
            if (single_cam) {
               if (oddataL.processed == 1) {
                  oddataL.pix_data = mapL[buffer_num].data;
                  oddataL.eye = 0;
                  oddataL.complete = 0;
                  oddataL.processed = 0;
                  pthread_create(&od_L_thread, NULL, object_detection_thread, &oddataL);
               }
            } else {
               if ((oddataL.processed == 1) && (oddataR.processed == 1)) {
                  oddataL.pix_data = mapL[buffer_num].data;
                  oddataL.eye = 0;
                  oddataL.complete = 0;
                  oddataL.processed = 0;
                  pthread_create(&od_L_thread, NULL, object_detection_thread, &oddataL);

                  oddataR.pix_data = mapR[buffer_num].data;
                  oddataR.eye = 1;
                  oddataR.complete = 0;
                  oddataR.processed = 0;
                  pthread_create(&od_R_thread, NULL, object_detection_thread, &oddataR);
               }
            }
#endif
         }

         SDL_UpdateTexture(textureL, NULL, mapL[buffer_num].data, this_hds->cam_input_width * 4);
         SDL_RenderCopy(renderer, textureL, &v_src_rect, &v_dst_rectL);

         if (single_cam) {
            SDL_RenderCopy(renderer, textureL, &v_src_rect, &v_dst_rectR);
         } else {
            SDL_UpdateTexture(textureR, NULL, mapR[buffer_num].data, this_hds->cam_input_width * 4);
            SDL_RenderCopy(renderer, textureR, &v_src_rect, &v_dst_rectR);
         }

#ifdef DISPLAY_TIMING
         last_ts_cap = (unsigned long) ts_cap[buffer_num].tv_sec * 1000000000 + ts_cap[buffer_num].tv_nsec;
#endif
      }
      pthread_mutex_unlock(&v_mutex);
      } else {
         /* When in black background mode, simply render black rectangles as backgrounds */
         SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
         SDL_RenderFillRect(renderer, &v_dst_rectL);
         SDL_RenderFillRect(renderer, &v_dst_rectR);
      }

      /* Element Processing */
      if (intro_element.enabled && !intro_finished) {
         play_intro(1, 0, &intro_finished);
      } else {
         render_hud_elements();

#ifdef DISPLAY_TIMING
         clock_gettime(CLOCK_REALTIME, &display_time);
         present_time = (unsigned long) display_time.tv_sec * 1000000000 + display_time.tv_nsec;
         ts_count++;
         ts_total += (present_time - last_ts_cap) / 1000000;
         LOG_INFO("Display latency: %lu ms, avg: %lu ms", (present_time - last_ts_cap) / 1000000,
                                                    ts_total / (unsigned long) ts_count);
         LOG_INFO("ts_total: %lu ms, ts_count: %u", ts_total, ts_count);
#endif

         if (get_recording_state() != DISABLED) {
#ifdef ENCODE_TIMING
            Uint32 start = 0, stop = 0;
#endif

            /* Is recording working? */
            video_out_data *this_vod = get_video_out_data();
            if (((get_recording_state() == RECORD) || (get_recording_state() == RECORD_STREAM)) && 
               this_vod->started && ((currTime - last_file_check) > 5000)) {
               last_last_size = last_size;
               if (has_file_grown(this_vod->filename, &last_last_size)) {
                  if (!(active_alerts & ALERT_RECORDING)) {
                     LOG_ERROR("ERROR: %s: File size is not increasing. %ld ? %ld",
                            this_vod->filename, last_last_size, last_size);
                     active_alerts |= ALERT_RECORDING;
                     mqttTextToSpeech("There is potentially and error with recording.");
                  }
               } else {
                  active_alerts &= ~ALERT_RECORDING;
                  last_size = last_last_size;
               }
               last_file_check = currTime;
            }

            this_vod->rgb_out_pixels[this_vod->write_index] =
                malloc(window_width * RGB_OUT_SIZE * window_height);
            if (this_vod->rgb_out_pixels[this_vod->write_index] == NULL) {
               LOG_ERROR("Unable to malloc rgb frame 0.");
               return (2);
            }

#ifdef ENCODE_TIMING
            start = SDL_GetTicks();
#endif

            if (OpenGL_RenderReadPixelsAsync(renderer, NULL, PIXEL_FORMAT_OUT,
                                     this_vod->rgb_out_pixels[this_vod->write_index],
                                     window_width * RGB_OUT_SIZE) != 0 ) {
               LOG_ERROR("OpenGL_RenderReadPixelsAsync() failed");
               /* Free the buffer on failure */
               free(this_vod->rgb_out_pixels[this_vod->write_index]);
               this_vod->rgb_out_pixels[this_vod->write_index] = NULL;
#ifdef ENCODE_TIMING
            } else {
               stop = SDL_GetTicks();
               cur_time = stop - start;
               avg_time = ((avg_time * weight) + cur_time) / (weight + 1);
               weight++;
               if (cur_time > max_time)
                  max_time = cur_time;
               if ((cur_time < min_time) || (min_time == 0))
                  min_time = cur_time;
               printf("OpenGL_RenderReadPixelsAsync(): %0.2f ms, min: %d, max: %d. weight: %d\r",
                      avg_time, min_time, max_time, weight);
#endif
            }

            pthread_mutex_lock(&this_vod->p_mutex);
            if (this_vod->rgb_out_pixels[this_vod->buffer_num] != NULL) {
               free(this_vod->rgb_out_pixels[this_vod->buffer_num]);
               this_vod->rgb_out_pixels[this_vod->buffer_num] = NULL;
            }

            /* Rotate indices */
            rotate_triple_buffer_indices(this_vod);

            pthread_mutex_unlock(&this_vod->p_mutex);

            if (get_recording_state() != DISABLED && get_video_out_thread() == 0) {
               pthread_t thread_id;
               if (pthread_create(&thread_id, NULL, video_next_thread, NULL) != 0) {
                  LOG_ERROR("Error creating video output thread.");
                  set_recording_state(DISABLED);
               } else {
                  set_video_out_thread(thread_id);
               }
            }
         } else {
            if (active_alerts & ALERT_RECORDING) {
               active_alerts &= ~ALERT_RECORDING;
            }
         }

         // Process any pending screenshot requests
         process_screenshot_requests(no_camera_mode);

         SDL_RenderPresent(renderer);
      }
   }

   mqttTextToSpeech("Your hud is shutting down.");

   set_recording_state(DISABLED);
   cleanup_video_out_data();
   pthread_mutex_destroy(&v_mutex);
   pthread_mutex_destroy(&windowSizeMutex);

   /* Close audio threads. */
#ifdef DEBUG_SHUTDOWN
   LOG_INFO("Waiting on audio threads to stop.");
#endif
   for (int i = 0; i < NUM_AUDIO_THREADS; i++) {
      pthread_join(thread_handles[i], NULL);
   }
#ifdef DEBUG_SHUTDOWN
   LOG_INFO("Done.");

   LOG_INFO("Waiting on command processing thread to stop.");
#endif
   //pthread_join(command_proc_thread, NULL); // TODO: This is hanging.
   pthread_cancel(command_proc_thread);
#ifdef DEBUG_SHUTDOWN
   LOG_INFO("Done.");

   LOG_INFO("Freeing elements.");
#endif
   /* Free elements. */
   free_elements(first_element);
   free_elements(this_as->armor_elements);
#ifdef DEBUG_SHUTDOWN
   LOG_INFO("Done.");

   LOG_INFO("Freeing fonts.");
#endif
   /* Free fonts. */
   this_font = font_list;
   while (this_font != NULL) {
      local_font *next_font = this_font->next;
      TTF_CloseFont(this_font->ttf_font);
      free(this_font);
      this_font = next_font;
   }
#ifdef DEBUG_SHUTDOWN
   LOG_INFO("Done.");
#endif

   cleanup_hud_manager();
   cleanup_fan_monitoring();

#ifdef DEBUG_SHUTDOWN
   LOG_INFO("Waiting on MQTT disconnect.");
#endif
   mosquitto_disconnect(mosq);
#ifdef DEBUG_SHUTDOWN
   LOG_INFO("Done.");

   LOG_INFO("Waiting on MQTT loop stop.");
#endif
   mosquitto_loop_stop(mosq, false);
#ifdef DEBUG_SHUTDOWN
   LOG_INFO("Done.");
#endif

   if (!no_camera_mode) {
#ifdef DEBUG_SHUTDOWN
      LOG_INFO("Wainting on video processing to stop.");
#endif
      pthread_join(video_proc_thread, NULL);
#ifdef DEBUG_SHUTDOWN
      LOG_INFO("Done.");
#endif
   }

   pthread_t vid_out_thread = get_video_out_thread();
   if (vid_out_thread != 0) {
#ifdef DEBUG_SHUTDOWN
      LOG_INFO("Waiting on final video thread to stop.");
#endif
      pthread_join(vid_out_thread, NULL);
      reset_video_out_thread();
#ifdef DEBUG_SHUTDOWN
      LOG_INFO("Done.");
#endif
   }

   cleanup_video_out_data();

#ifdef DEBUG_SHUTDOWN
   LOG_INFO("Destroy primary textures.");
#endif
   if (textureL != NULL) {
      SDL_DestroyTexture(textureL);
      textureL = NULL;
   }
   if (textureR != NULL) {
      SDL_DestroyTexture(textureR);
      textureR = NULL;
   }

#ifdef DEBUG_SHUTDOWN
   LOG_INFO("Delete GL buffers.");
#endif
   cleanup_pbo_system();

#ifdef DEBUG_SHUTDOWN
   LOG_INFO("Delete the GL context.");
#endif
   SDL_GL_DeleteContext(glContext);

#ifdef DEBUG_SHUTDOWN
   LOG_INFO("Waiting on SDL renderer and window destruction.");
#endif
   SDL_DestroyRenderer(renderer);
   SDL_DestroyWindow(window);
#ifdef DEBUG_SHUTDOWN
   LOG_INFO("Done.");
#endif

#ifdef DEBUG_SHUTDOWN
   LOG_INFO("Waiting on SDL libraries to quit.");
#endif
   TTF_Quit();
   IMG_Quit();
   SDL_Quit();
#ifdef DEBUG_SHUTDOWN
   LOG_INFO("Done.");
#endif

   if (detect_enabled)
   {
#ifdef DEBUG_SHUTDOWN
      LOG_INFO("Waiting for detection to clean up.");
#endif
      free_detect(&oddataL.detect_obj);
      free_detect(&oddataR.detect_obj);
#ifdef DEBUG_SHUTDOWN
      LOG_INFO("Done.");
#endif
   }

#ifdef DEBUG_SHUTDOWN
   LOG_INFO("Waiting for other library clean up.");
#endif
   mosquitto_destroy(mosq);
   mosquitto_lib_cleanup();
   curl_global_cleanup();
#ifdef DEBUG_SHUTDOWN
   LOG_INFO("Done.");
#endif

   // Close the log file properly
   close_logging();

   return (EXIT_SUCCESS);
}
