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
 * Suit telemetry republish — see include/comm/suit_telemetry.h.  Builds an OCP
 * v1.4 telemetry event ({device, msg_type:"telemetry", type, timestamp, ...})
 * from the already-parsed AURA/SPARK data and publishes it via the quiet MQTT
 * path.  Helmet feeds are rate-limited per type so the streaming Motion sensor
 * cannot flood the broker; armor updates are low-rate and published as they
 * arrive (a shared gate could starve a second piece).
 */

#include "comm/suit_telemetry.h"

#include <json-c/json.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "core/mirage.h"
#include "logging.h"

#define SUIT_HELMET_TOPIC "helmet/telemetry"
#define SUIT_ARMOR_TOPIC "armor/telemetry"

/* Minimum spacing between republished helmet samples of one type.  DAWN caches
 * latest-value + treats the feed as stale after ~30 s, so ~1 Hz is ample and
 * keeps the compass-rate Motion stream off the broker. */
#define SUIT_HELMET_MIN_INTERVAL_MS 1000

/* Per-type last-publish timestamps (ms).  parse_json_command runs on a few
 * threads (serial / socket / MQTT).  On the aarch64 deployment target an aligned
 * 64-bit load/store is atomic, so these are never torn in practice; even on a
 * hypothetical 32-bit build a torn read would only drop or duplicate one
 * telemetry sample (never corrupt state) — acceptable for a latest-value feed. */
static int64_t s_last_enviro_ms = 0;
static int64_t s_last_motion_ms = 0;
static int64_t s_last_gps_ms = 0;

static int64_t now_ms(void) {
   struct timespec ts;
   clock_gettime(CLOCK_REALTIME, &ts);
   return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/* True if enough time has elapsed since *last_ms; updates *last_ms when it does. */
static bool rate_ok(int64_t *last_ms) {
   int64_t now = now_ms();
   if (now - *last_ms < SUIT_HELMET_MIN_INTERVAL_MS) {
      return false;
   }
   *last_ms = now;
   return true;
}

/* Start an OCP telemetry envelope: {device, msg_type:"telemetry", type, timestamp}. */
static struct json_object *envelope_new(const char *device, const char *type) {
   struct json_object *o = json_object_new_object();
   json_object_object_add(o, "device", json_object_new_string(device));
   json_object_object_add(o, "msg_type", json_object_new_string("telemetry"));
   json_object_object_add(o, "type", json_object_new_string(type));
   json_object_object_add(o, "timestamp", json_object_new_int64(now_ms()));
   return o;
}

static void publish_and_free(const char *topic, struct json_object *o) {
   const char *json = json_object_to_json_string(o);
   mqttSendMessageQuiet(topic, json);
   json_object_put(o);
}

void suit_telemetry_publish_enviro(const enviro *e) {
   if (e == NULL || !rate_ok(&s_last_enviro_ms)) {
      return;
   }
   struct json_object *o = envelope_new("aura", "Enviro");
   json_object_object_add(o, "temp", json_object_new_double(e->temp));
   json_object_object_add(o, "humidity", json_object_new_double(e->humidity));
   json_object_object_add(o, "air_quality", json_object_new_double(e->air_quality));
   json_object_object_add(o, "air_quality_description",
                          json_object_new_string(e->air_quality_description));
   json_object_object_add(o, "tvoc_ppb", json_object_new_double(e->tvoc_ppb));
   json_object_object_add(o, "eco2_ppm", json_object_new_double(e->eco2_ppm));
   json_object_object_add(o, "co2_ppm", json_object_new_double(e->co2_ppm));
   json_object_object_add(o, "co2_quality", json_object_new_string(e->co2_quality_description));
   json_object_object_add(o, "heat_index_c", json_object_new_double(e->heat_index_c));
   json_object_object_add(o, "dew_point", json_object_new_double(e->dew_point));
   publish_and_free(SUIT_HELMET_TOPIC, o);
}

void suit_telemetry_publish_motion(const motion *m) {
   if (m == NULL || !rate_ok(&s_last_motion_ms)) {
      return;
   }
   struct json_object *o = envelope_new("aura", "Motion");
   json_object_object_add(o, "heading", json_object_new_double(m->heading));
   json_object_object_add(o, "pitch", json_object_new_double(m->pitch));
   json_object_object_add(o, "roll", json_object_new_double(m->roll));
   publish_and_free(SUIT_HELMET_TOPIC, o);
}

void suit_telemetry_publish_gps(const gps *g) {
   if (g == NULL || !rate_ok(&s_last_gps_ms)) {
      return;
   }
   struct json_object *o = envelope_new("aura", "GPS");
   json_object_object_add(o, "fix", json_object_new_int(g->fix));
   json_object_object_add(o, "quality", json_object_new_int(g->quality));
   json_object_object_add(o, "satellites", json_object_new_int(g->satellites));
   /* AURA sends the decimal-degree position as latitudeDegrees/longitudeDegrees
    * (the raw latitude/longitude NMEA fields are never populated), so publish the
    * *Degrees fields under the DAWN-side latitude/longitude keys. */
   json_object_object_add(o, "latitude", json_object_new_double(g->latitudeDegrees));
   json_object_object_add(o, "longitude", json_object_new_double(g->longitudeDegrees));
   json_object_object_add(o, "altitude", json_object_new_double(g->altitude));
   json_object_object_add(o, "speed", json_object_new_double(g->speed));
   publish_and_free(SUIT_HELMET_TOPIC, o);
}

void suit_telemetry_publish_armor(const char *piece,
                                  double temp,
                                  double voltage,
                                  bool have_temp,
                                  bool have_voltage) {
   if (piece == NULL || piece[0] == '\0') {
      return;
   }
   struct json_object *o = envelope_new("spark", "Armor");
   json_object_object_add(o, "piece", json_object_new_string(piece));
   if (have_temp) {
      json_object_object_add(o, "temp", json_object_new_double(temp));
   }
   if (have_voltage) {
      json_object_object_add(o, "voltage", json_object_new_double(voltage));
   }
   publish_and_free(SUIT_ARMOR_TOPIC, o);
}
