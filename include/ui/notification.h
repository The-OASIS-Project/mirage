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
 * Notification system for HUD phone/SMS/image popups.
 * Config-driven: elements link to slots via notification_group field.
 */

#ifndef NOTIFICATION_H
#define NOTIFICATION_H

#include <SDL2/SDL.h>
#include <json-c/json.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

/* =============================================================================
 * Constants
 * ============================================================================= */

#define NOTIF_MAX_NAME 64
#define NOTIF_MAX_NUMBER 24
#define NOTIF_MAX_PREVIEW 128
#define NOTIF_MAX_TITLE 128
#define NOTIF_MAX_SOURCE 64

/* Notification group IDs — resolved once at config parse time for O(1) render dispatch */
typedef enum {
   NOTIF_GROUP_NONE = 0,
   NOTIF_GROUP_PHONE,
   NOTIF_GROUP_IMAGE,
} notif_group_t;

/**
 * @brief Resolve a notification_group string to an enum ID.
 * @param group Group name from config.json ("phone", "image")
 * @return Enum ID, or NOTIF_GROUP_NONE if not recognized
 */
notif_group_t notif_group_resolve(const char *group);

/* Timing (milliseconds) */
#define NOTIF_FADE_IN_PHONE_MS 150
#define NOTIF_FADE_IN_IMAGE_MS 250
#define NOTIF_FADE_OUT_MS 250
#define NOTIF_COMPACT_AFTER_MS 5000 /* Collapse active call to single line */
#define NOTIF_CALL_ENDED_TTL_MS 3000
#define NOTIF_SMS_TTL_MS 15000
#define NOTIF_IMAGE_TTL_MS 30000

/* =============================================================================
 * Types
 * ============================================================================= */

typedef enum {
   NOTIF_STATE_HIDDEN = 0,
   NOTIF_STATE_SHOWING, /* Fading in */
   NOTIF_STATE_VISIBLE, /* Full display, timeout counting */
   NOTIF_STATE_COMPACT, /* Phone only — collapsed single-line timer */
   NOTIF_STATE_HIDING,  /* Fading out */
} notif_state_t;

typedef enum {
   NOTIF_EVENT_NONE = 0,
   NOTIF_EVENT_INCOMING_CALL,
   NOTIF_EVENT_CALL_ACTIVE,
   NOTIF_EVENT_CALL_ENDED,
   NOTIF_EVENT_SMS_RECEIVED,
   NOTIF_EVENT_IMAGE_DISPLAY,
} notif_event_t;

/* Phone notification slot */
typedef struct {
   notif_state_t state;
   notif_event_t event;
   char caller_name[NOTIF_MAX_NAME];
   char caller_number[NOTIF_MAX_NUMBER];
   char call_status[NOTIF_MAX_NAME]; /* "INCOMING CALL", "CALL ACTIVE 03:45", etc. */
   char sms_preview[NOTIF_MAX_PREVIEW];
   uint32_t start_time_ms; /* When notification was triggered */
   uint32_t ttl_ms;        /* Time-to-live from start */
   uint32_t fade_start_ms; /* When current fade began */
   uint32_t fade_duration_ms;
   int call_duration_sec; /* For active call timer */
   float alpha;           /* 0.0 = hidden, 1.0 = fully visible */

   /* Contact photo (decoded from base64 MQTT payload) */
   unsigned char *photo_data; /* Raw image bytes (malloc'd) */
   size_t photo_data_size;
   bool photo_dirty;           /* Set by MQTT thread, consumed by render thread */
   SDL_Texture *photo_texture; /* Created on render thread */
   pthread_mutex_t mutex;      /* Protects all fields (MQTT writer, render reader) */
} notif_phone_t;

/* Image notification slot */
typedef struct {
   notif_state_t state;
   char title[NOTIF_MAX_TITLE];
   char source[NOTIF_MAX_SOURCE];
   char image_url[256]; /* /api/images/img_xxxxxxxxxxxx */
   uint32_t start_time_ms;
   uint32_t ttl_ms;
   uint32_t fade_start_ms;
   uint32_t fade_duration_ms;
   float alpha;

   /* Image data (fetched via HTTP) */
   unsigned char *image_data;
   size_t image_data_size;
   bool image_dirty;
   SDL_Texture *image_texture;
   pthread_mutex_t image_mutex;
} notif_image_t;

/* =============================================================================
 * Public API
 * ============================================================================= */

/**
 * @brief Initialize the notification system.
 */
void notification_init(void);

/**
 * @brief Shut down and free resources.
 */
void notification_shutdown(void);

/**
 * @brief Handle a phone event from MQTT (incoming_call, call_active, etc.)
 *
 * Called from MQTT callback thread. Thread-safe.
 *
 * @param root Parsed JSON object (not consumed — caller frees)
 */
void notification_handle_phone_event(struct json_object *root);

/**
 * @brief Handle an image display request from MQTT.
 *
 * Called from MQTT callback thread. Thread-safe.
 *
 * @param root Parsed JSON object (not consumed — caller frees)
 */
void notification_handle_image_request(struct json_object *root);

/**
 * @brief Update notification timers and state transitions.
 *
 * Called every frame from the render thread.
 */
void notification_update(void);

/**
 * @brief Check if elements in a notification group should be visible.
 *
 * @param group Notification group name ("phone" or "image")
 * @return Current alpha (0.0 = hidden, 1.0 = visible)
 */
float notification_get_alpha(int group);

/**
 * @brief Check if a notification group is active (any state except HIDDEN).
 *
 * @param group Notification group name
 * @return true if active
 */
bool notification_is_active(int group);

/**
 * @brief Get current dynamic text for notification text sources.
 *
 * @param source_id Text source enum value
 * @param out Output buffer
 * @param out_size Size of output buffer
 */
void notification_get_text(int source_id, char *out, size_t out_size);

/**
 * @brief Get the contact photo texture (or NULL if none).
 *
 * Must be called from the render thread. Creates SDL texture from
 * decoded photo data if dirty flag is set.
 *
 * @param renderer SDL renderer for texture creation
 * @return SDL_Texture pointer or NULL
 */
SDL_Texture *notification_get_photo_texture(SDL_Renderer *renderer);

/**
 * @brief Get the phone notification slot (read-only access for renderer).
 */
const notif_phone_t *notification_get_phone(void);

/**
 * @brief Get the image notification slot (read-only access for renderer).
 */
const notif_image_t *notification_get_image(void);

#endif /* NOTIFICATION_H */
