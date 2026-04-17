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
 *
 * Notification system — phone call/SMS popups and image display on HUD.
 * State machine manages fade in/out, compact mode, and TTL expiry.
 * Photo textures decoded from base64 on MQTT thread, created on render thread.
 */

#include "ui/notification.h"

#include <SDL2/SDL_image.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <stdlib.h>
#include <string.h>

#include "config/config_parser.h"
#include "util/logging.h"

/* =============================================================================
 * State
 * ============================================================================= */

static notif_phone_t s_phone = { 0 };
static notif_image_t s_image = { 0 };

notif_group_t notif_group_resolve(const char *group) {
   if (!group || !group[0]) {
      return NOTIF_GROUP_NONE;
   }
   if (strcmp(group, "phone") == 0) {
      return NOTIF_GROUP_PHONE;
   }
   if (strcmp(group, "image") == 0) {
      return NOTIF_GROUP_IMAGE;
   }
   return NOTIF_GROUP_NONE;
}

/* =============================================================================
 * Base64 Decode Helper
 * ============================================================================= */

/**
 * @brief Decode a base64 string to raw bytes.
 * @param input Base64 string
 * @param out_size Output: decoded size
 * @return malloc'd buffer (caller frees), or NULL on failure
 */
static unsigned char *base64_decode(const char *input, size_t *out_size) {
   if (!input || !out_size) {
      return NULL;
   }

   size_t input_len = strlen(input);
   if (input_len == 0) {
      return NULL;
   }

   /* Cap at 512KB base64 (~384KB decoded) to prevent OOM from rogue MQTT payloads */
   if (input_len > 512 * 1024) {
      LOG_WARNING("notification: base64 input too large (%zu bytes), rejecting", input_len);
      return NULL;
   }

   /* Output is at most 3/4 of input length */
   size_t max_out = (input_len * 3) / 4 + 4;
   unsigned char *output = malloc(max_out);
   if (!output) {
      return NULL;
   }

   BIO *b64 = BIO_new(BIO_f_base64());
   BIO *mem = BIO_new_mem_buf(input, (int)input_len);
   if (!b64 || !mem) {
      free(output);
      if (b64) {
         BIO_free(b64);
      }
      if (mem) {
         BIO_free(mem);
      }
      return NULL;
   }

   BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
   BIO_push(b64, mem);

   int decoded_len = BIO_read(b64, output, (int)max_out);
   BIO_free_all(b64);

   if (decoded_len <= 0) {
      free(output);
      return NULL;
   }

   *out_size = (size_t)decoded_len;
   return output;
}

/* =============================================================================
 * State Machine Helpers
 * ============================================================================= */

static uint32_t now_ms(void) {
   return SDL_GetTicks();
}

static void phone_set_state(notif_state_t new_state, uint32_t fade_ms) {
   s_phone.state = new_state;
   if (new_state == NOTIF_STATE_SHOWING || new_state == NOTIF_STATE_HIDING) {
      s_phone.fade_start_ms = now_ms();
      s_phone.fade_duration_ms = fade_ms;
   }
}

static void image_set_state(notif_state_t new_state, uint32_t fade_ms) {
   s_image.state = new_state;
   if (new_state == NOTIF_STATE_SHOWING || new_state == NOTIF_STATE_HIDING) {
      s_image.fade_start_ms = now_ms();
      s_image.fade_duration_ms = fade_ms;
   }
}

static void phone_clear(void) {
   s_phone.state = NOTIF_STATE_HIDDEN;
   s_phone.event = NOTIF_EVENT_NONE;
   s_phone.caller_name[0] = '\0';
   s_phone.caller_number[0] = '\0';
   s_phone.call_status[0] = '\0';
   s_phone.sms_preview[0] = '\0';
   s_phone.alpha = 0.0f;
   s_phone.call_duration_sec = 0;

   /* Don't free photo here — it persists for reuse across events from same contact */
}

static void phone_free_photo(void) {
   pthread_mutex_lock(&s_phone.mutex);
   if (s_phone.photo_data) {
      free(s_phone.photo_data);
      s_phone.photo_data = NULL;
      s_phone.photo_data_size = 0;
   }
   s_phone.photo_dirty = false;
   pthread_mutex_unlock(&s_phone.mutex);

   if (s_phone.photo_texture) {
      SDL_DestroyTexture(s_phone.photo_texture);
      s_phone.photo_texture = NULL;
   }
}

/* =============================================================================
 * Public API — Lifecycle
 * ============================================================================= */

void notification_init(void) {
   memset(&s_phone, 0, sizeof(s_phone));
   memset(&s_image, 0, sizeof(s_image));
   pthread_mutex_init(&s_phone.mutex, NULL);
   pthread_mutex_init(&s_image.image_mutex, NULL);
   LOG_INFO("Notification system initialized");
}

void notification_shutdown(void) {
   phone_free_photo();

   pthread_mutex_lock(&s_image.image_mutex);
   if (s_image.image_data) {
      free(s_image.image_data);
      s_image.image_data = NULL;
   }
   pthread_mutex_unlock(&s_image.image_mutex);

   if (s_image.image_texture) {
      SDL_DestroyTexture(s_image.image_texture);
      s_image.image_texture = NULL;
   }

   pthread_mutex_destroy(&s_phone.mutex);
   pthread_mutex_destroy(&s_image.image_mutex);
   LOG_INFO("Notification system shut down");
}

/* =============================================================================
 * MQTT Event Handlers (called from MQTT thread)
 * ============================================================================= */

void notification_handle_phone_event(struct json_object *root) {
   if (!root) {
      return;
   }

   struct json_object *j_event;
   if (!json_object_object_get_ex(root, "event", &j_event)) {
      return;
   }
   const char *event = json_object_get_string(j_event);
   if (!event) {
      return;
   }

   /* Extract common fields */
   const char *name = "";
   const char *number = "";
   struct json_object *j_tmp;
   if (json_object_object_get_ex(root, "name", &j_tmp)) {
      name = json_object_get_string(j_tmp);
   }
   if (json_object_object_get_ex(root, "number", &j_tmp)) {
      number = json_object_get_string(j_tmp);
   }

   /* Decode photo if present */
   struct json_object *j_photo;
   if (json_object_object_get_ex(root, "photo", &j_photo)) {
      struct json_object *j_data;
      if (json_object_object_get_ex(j_photo, "data", &j_data)) {
         const char *b64 = json_object_get_string(j_data);
         if (b64 && b64[0]) {
            size_t decoded_size = 0;
            unsigned char *decoded = base64_decode(b64, &decoded_size);
            if (decoded && decoded_size > 0) {
               pthread_mutex_lock(&s_phone.mutex);
               free(s_phone.photo_data);
               s_phone.photo_data = decoded;
               s_phone.photo_data_size = decoded_size;
               s_phone.photo_dirty = true;
               pthread_mutex_unlock(&s_phone.mutex);
            }
         }
      }
   }

   /* Lock for all field writes (render thread reads under same lock) */
   pthread_mutex_lock(&s_phone.mutex);

   if (strcmp(event, "incoming_call") == 0) {
      snprintf(s_phone.caller_name, sizeof(s_phone.caller_name), "%s",
               (name && name[0]) ? name : "Unknown");
      snprintf(s_phone.caller_number, sizeof(s_phone.caller_number), "%s", number);
      snprintf(s_phone.call_status, sizeof(s_phone.call_status), "INCOMING CALL");
      s_phone.sms_preview[0] = '\0';
      s_phone.event = NOTIF_EVENT_INCOMING_CALL;
      s_phone.start_time_ms = now_ms();
      s_phone.ttl_ms = 8000; /* Short TTL — extended by each ring event */

      s_phone.call_duration_sec = 0;
      phone_set_state(NOTIF_STATE_SHOWING, NOTIF_FADE_IN_PHONE_MS);

   } else if (strcmp(event, "ring") == 0) {
      /* Subsequent ring — reset TTL to keep notification visible while ringing */
      if (s_phone.event == NOTIF_EVENT_INCOMING_CALL && s_phone.state != NOTIF_STATE_HIDDEN) {
         s_phone.start_time_ms = now_ms();
      }

   } else if (strcmp(event, "call_active") == 0) {
      snprintf(s_phone.caller_name, sizeof(s_phone.caller_name), "%s",
               (name && name[0]) ? name : "Unknown");
      snprintf(s_phone.caller_number, sizeof(s_phone.caller_number), "%s", number);
      snprintf(s_phone.call_status, sizeof(s_phone.call_status), "CALL ACTIVE");
      s_phone.sms_preview[0] = '\0';
      s_phone.event = NOTIF_EVENT_CALL_ACTIVE;
      s_phone.start_time_ms = now_ms();
      s_phone.ttl_ms = 0; /* No auto-dismiss — stays until call_ended */
      s_phone.call_duration_sec = 0;
      phone_set_state(NOTIF_STATE_SHOWING, NOTIF_FADE_IN_PHONE_MS);

   } else if (strcmp(event, "call_ended") == 0) {
      int duration = 0;
      if (json_object_object_get_ex(root, "duration", &j_tmp)) {
         duration = json_object_get_int(j_tmp);
      }
      s_phone.call_duration_sec = duration;

      if (duration > 0) {
         int mins = duration / 60;
         int secs = duration % 60;
         snprintf(s_phone.call_status, sizeof(s_phone.call_status), "CALL ENDED %d:%02d", mins,
                  secs);
      } else {
         snprintf(s_phone.call_status, sizeof(s_phone.call_status), "CALL ENDED");
      }

      s_phone.event = NOTIF_EVENT_CALL_ENDED;
      s_phone.start_time_ms = now_ms();
      s_phone.ttl_ms = NOTIF_CALL_ENDED_TTL_MS;
      phone_set_state(NOTIF_STATE_SHOWING, NOTIF_FADE_IN_PHONE_MS);

   } else if (strcmp(event, "sms_received") == 0) {
      snprintf(s_phone.caller_name, sizeof(s_phone.caller_name), "%s",
               (name && name[0]) ? name : "Unknown");
      snprintf(s_phone.caller_number, sizeof(s_phone.caller_number), "%s", number);
      snprintf(s_phone.call_status, sizeof(s_phone.call_status), "SMS RECEIVED");

      const char *preview = "";
      if (json_object_object_get_ex(root, "preview", &j_tmp)) {
         preview = json_object_get_string(j_tmp);
      }
      snprintf(s_phone.sms_preview, sizeof(s_phone.sms_preview), "%s", preview ? preview : "");

      s_phone.event = NOTIF_EVENT_SMS_RECEIVED;
      s_phone.start_time_ms = now_ms();
      s_phone.ttl_ms = NOTIF_SMS_TTL_MS;

      struct json_object *j_ttl;
      if (json_object_object_get_ex(root, "ttl", &j_ttl)) {
         s_phone.ttl_ms = (uint32_t)(json_object_get_int(j_ttl) * 1000);
      }

      phone_set_state(NOTIF_STATE_SHOWING, NOTIF_FADE_IN_PHONE_MS);
   }

   pthread_mutex_unlock(&s_phone.mutex);
}

void notification_handle_image_request(struct json_object *root) {
   if (!root) {
      return;
   }

   struct json_object *j_tmp;

   const char *title = "";
   if (json_object_object_get_ex(root, "title", &j_tmp)) {
      title = json_object_get_string(j_tmp);
   }
   pthread_mutex_lock(&s_image.image_mutex);

   snprintf(s_image.title, sizeof(s_image.title), "%s", title ? title : "");

   const char *source = "";
   if (json_object_object_get_ex(root, "source", &j_tmp)) {
      source = json_object_get_string(j_tmp);
   }
   snprintf(s_image.source, sizeof(s_image.source), "%s", source ? source : "");

   const char *url = "";
   if (json_object_object_get_ex(root, "image_url", &j_tmp)) {
      url = json_object_get_string(j_tmp);
   }
   snprintf(s_image.image_url, sizeof(s_image.image_url), "%s", url ? url : "");

   s_image.start_time_ms = now_ms();
   s_image.ttl_ms = NOTIF_IMAGE_TTL_MS;

   struct json_object *j_ttl;
   if (json_object_object_get_ex(root, "ttl", &j_ttl)) {
      s_image.ttl_ms = (uint32_t)(json_object_get_int(j_ttl) * 1000);
   }

   image_set_state(NOTIF_STATE_SHOWING, NOTIF_FADE_IN_IMAGE_MS);

   pthread_mutex_unlock(&s_image.image_mutex);

   /* TODO: Kick off async HTTP fetch for image_url using service token */
}

/* =============================================================================
 * Frame Update (called from render thread)
 * ============================================================================= */

void notification_update(void) {
   uint32_t t = now_ms();

   pthread_mutex_lock(&s_phone.mutex);

   /* --- Phone slot state machine --- */
   switch (s_phone.state) {
      case NOTIF_STATE_SHOWING: {
         uint32_t elapsed = t - s_phone.fade_start_ms;
         if (elapsed >= s_phone.fade_duration_ms) {
            s_phone.alpha = 1.0f;
            s_phone.state = NOTIF_STATE_VISIBLE;
         } else {
            s_phone.alpha = (float)elapsed / (float)s_phone.fade_duration_ms;
         }
         break;
      }
      case NOTIF_STATE_VISIBLE: {
         /* Update call timer for active calls */
         if (s_phone.event == NOTIF_EVENT_CALL_ACTIVE) {
            int secs = (int)(t - s_phone.start_time_ms) / 1000;
            int mins = secs / 60;
            secs = secs % 60;
            snprintf(s_phone.call_status, sizeof(s_phone.call_status), "CALL ACTIVE %d:%02d", mins,
                     secs);

            /* Transition to compact after NOTIF_COMPACT_AFTER_MS */
            if ((t - s_phone.start_time_ms) >= NOTIF_COMPACT_AFTER_MS) {
               s_phone.state = NOTIF_STATE_COMPACT;
            }
         }

         /* TTL expiry (0 = no auto-dismiss) */
         if (s_phone.ttl_ms > 0 && (t - s_phone.start_time_ms) >= s_phone.ttl_ms) {
            phone_set_state(NOTIF_STATE_HIDING, NOTIF_FADE_OUT_MS);
         }
         break;
      }
      case NOTIF_STATE_COMPACT: {
         /* Still update timer */
         if (s_phone.event == NOTIF_EVENT_CALL_ACTIVE) {
            int secs = (int)(t - s_phone.start_time_ms) / 1000;
            int mins = secs / 60;
            secs = secs % 60;
            snprintf(s_phone.call_status, sizeof(s_phone.call_status), "CALL ACTIVE %d:%02d", mins,
                     secs);
         }
         /* Compact mode stays until call_ended or dismiss */
         break;
      }
      case NOTIF_STATE_HIDING: {
         uint32_t elapsed = t - s_phone.fade_start_ms;
         if (elapsed >= s_phone.fade_duration_ms) {
            phone_clear();
         } else {
            s_phone.alpha = 1.0f - ((float)elapsed / (float)s_phone.fade_duration_ms);
         }
         break;
      }
      default:
         break;
   }

   pthread_mutex_unlock(&s_phone.mutex);

   /* --- Image slot state machine --- */
   pthread_mutex_lock(&s_image.image_mutex);
   switch (s_image.state) {
      case NOTIF_STATE_SHOWING: {
         uint32_t elapsed = t - s_image.fade_start_ms;
         if (elapsed >= s_image.fade_duration_ms) {
            s_image.alpha = 1.0f;
            s_image.state = NOTIF_STATE_VISIBLE;
         } else {
            s_image.alpha = (float)elapsed / (float)s_image.fade_duration_ms;
         }
         break;
      }
      case NOTIF_STATE_VISIBLE: {
         if (s_image.ttl_ms > 0 && (t - s_image.start_time_ms) >= s_image.ttl_ms) {
            image_set_state(NOTIF_STATE_HIDING, NOTIF_FADE_OUT_MS);
         }
         break;
      }
      case NOTIF_STATE_HIDING: {
         uint32_t elapsed = t - s_image.fade_start_ms;
         if (elapsed >= s_image.fade_duration_ms) {
            s_image.state = NOTIF_STATE_HIDDEN;
            s_image.alpha = 0.0f;
            s_image.title[0] = '\0';
            s_image.source[0] = '\0';
         } else {
            s_image.alpha = 1.0f - ((float)elapsed / (float)s_image.fade_duration_ms);
         }
         break;
      }
      default:
         break;
   }
   pthread_mutex_unlock(&s_image.image_mutex);
}

/* =============================================================================
 * Query API (called from render thread)
 * ============================================================================= */

float notification_get_alpha(int group) {
   if (group == NOTIF_GROUP_PHONE) {
      return s_phone.alpha;
   }
   if (group == NOTIF_GROUP_IMAGE) {
      return s_image.alpha;
   }
   return 0.0f;
}

bool notification_is_active(int group) {
   if (group == NOTIF_GROUP_PHONE) {
      return s_phone.state != NOTIF_STATE_HIDDEN;
   }
   if (group == NOTIF_GROUP_IMAGE) {
      return s_image.state != NOTIF_STATE_HIDDEN;
   }
   return false;
}

void notification_get_text(int source_id, char *out, size_t out_size) {
   if (!out || out_size == 0) {
      return;
   }
   out[0] = '\0';

   switch (source_id) {
      case TEXT_SOURCE_CALLER_NAME:
         pthread_mutex_lock(&s_phone.mutex);
         snprintf(out, out_size, "%s", s_phone.caller_name);
         pthread_mutex_unlock(&s_phone.mutex);
         break;
      case TEXT_SOURCE_CALLER_NUMBER:
         pthread_mutex_lock(&s_phone.mutex);
         snprintf(out, out_size, "%s", s_phone.caller_number);
         pthread_mutex_unlock(&s_phone.mutex);
         break;
      case TEXT_SOURCE_CALL_STATUS:
         pthread_mutex_lock(&s_phone.mutex);
         snprintf(out, out_size, "%s", s_phone.call_status);
         pthread_mutex_unlock(&s_phone.mutex);
         break;
      case TEXT_SOURCE_SMS_PREVIEW:
         pthread_mutex_lock(&s_phone.mutex);
         snprintf(out, out_size, "%s", s_phone.sms_preview);
         pthread_mutex_unlock(&s_phone.mutex);
         break;
      case TEXT_SOURCE_NOTIFICATION_TITLE:
         pthread_mutex_lock(&s_image.image_mutex);
         snprintf(out, out_size, "%s", s_image.title);
         pthread_mutex_unlock(&s_image.image_mutex);
         break;
      case TEXT_SOURCE_NOTIFICATION_SOURCE:
         pthread_mutex_lock(&s_image.image_mutex);
         snprintf(out, out_size, "%s", s_image.source);
         pthread_mutex_unlock(&s_image.image_mutex);
         break;
      default:
         break;
   }
}

SDL_Texture *notification_get_photo_texture(SDL_Renderer *renderer) {
   if (!renderer) {
      return s_phone.photo_texture;
   }

   /* Check if MQTT thread delivered new photo data */
   pthread_mutex_lock(&s_phone.mutex);
   if (s_phone.photo_dirty && s_phone.photo_data && s_phone.photo_data_size > 0) {
      /* Destroy old texture */
      if (s_phone.photo_texture) {
         SDL_DestroyTexture(s_phone.photo_texture);
         s_phone.photo_texture = NULL;
      }

      /* Create texture from raw bytes */
      SDL_RWops *rw = SDL_RWFromMem(s_phone.photo_data, (int)s_phone.photo_data_size);
      if (rw) {
         SDL_Surface *surface = IMG_Load_RW(rw, 1); /* 1 = auto-close RW */
         if (surface) {
            s_phone.photo_texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_FreeSurface(surface);
         }
      }
      s_phone.photo_dirty = false;
   }
   pthread_mutex_unlock(&s_phone.mutex);

   return s_phone.photo_texture;
}

const notif_phone_t *notification_get_phone(void) {
   return &s_phone;
}

const notif_image_t *notification_get_image(void) {
   return &s_image;
}
