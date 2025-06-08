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

#include <time.h>

#include "defines.h"
#include "armor.h"
#include "config_manager.h"
#include "element_renderer.h"
#include "logging.h"
#include "mirage.h"

static int armor_enabled = 1;    /* Variable to turn on/off displaying armor. */

void setArmorEnabled(int enabled) {
   if (enabled) {
      armor_enabled = 1;
   } else {
      armor_enabled = 0;
   }
}

/* Register armor component when MQTT message received */
void registerArmor(char *mqtt_device_in)
{
   char text[2048] = "";
   armor_settings *this_as = get_armor_settings();
   element *this_element = this_as->armor_elements;
   element *armor_display = NULL;
   element *curr_element = get_first_element();
   time_t current_time = time(NULL);

   if (this_as->armor_elements == NULL) {
      return;
   }

   /* Find the armor_display element to get timeout value */
   while (curr_element != NULL) {
      if (curr_element->type == SPECIAL && strcmp(curr_element->name, "armor_display") == 0) {
         armor_display = curr_element;
         break;
      }
      curr_element = curr_element->next;
   }

   /* Find the armor component by mqtt_device */
   while (this_element != NULL) {
      if (strcmp(this_element->mqtt_device, mqtt_device_in) == 0) {
         if (!this_element->mqtt_registered) {
            /* New registration */
            this_element->mqtt_registered = 1;
            this_element->mqtt_last_time = current_time;
            this_element->texture = this_element->texture_online;

            /* Trigger notification timeout */
            int notice_timeout = (armor_display != NULL && armor_display->notice_timeout > 0) ?
                                 armor_display->notice_timeout : 5;
            trigger_armor_notification_timeout(notice_timeout);

            /* TTS for connection */
            snprintf(text, 2048, "%s connected.", this_element->name);
            mqttTextToSpeech(text);

            LOG_INFO("Armor element connected: %s", this_element->name);
         } else {
            /* Update existing registration timestamp */
            this_element->mqtt_last_time = current_time;
         }

         return;
      }

      this_element = this_element->next;
   }
}
