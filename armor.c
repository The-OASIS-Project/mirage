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
#include "logging.h"
#include "mirage.h"

static time_t armor_timeout = 0; /* This is the variable tracking when the timeout occurs. */
static int armor_enabled = 1;    /* Variable to turn on/off displaying armor. */

void setArmorEnabled(int enabled) {
   if (enabled) {
      armor_enabled = 1;
   } else {
      armor_enabled = 0;
   }
}

/* Render the armor overlays into the given texture in the given rectangle. */
void renderArmor(void)
{
   armor_settings *this_as = get_armor_settings();
   element *this_element = this_as->armor_elements;
   SDL_Rect dest_rect_l, dest_rect_r;
   time_t curr_time;
   char text[2048] = "";

   hud_display_settings *this_hds = get_hud_display_settings();

   if (this_as->armor_elements == NULL)
   {
      return;
   }

   dest_rect_l.x = this_as->armor_dest.x - this_hds->stereo_offset;
   dest_rect_r.x = this_as->armor_dest.x + this_hds->stereo_offset;
   dest_rect_l.y = dest_rect_r.y = this_as->armor_dest.y;
   dest_rect_l.w = dest_rect_r.w = this_as->armor_dest.w;
   dest_rect_l.h = dest_rect_r.h = this_as->armor_dest.h;

   while (this_element != NULL) {
      time(&curr_time);

      if ((armor_timeout > 0) && (curr_time > armor_timeout)) {
         armor_timeout = 0;
      } else if (armor_timeout > 0) {
         dest_rect_l.x = this_as->armor_notice_dest.x - this_hds->stereo_offset;
         dest_rect_r.x = this_as->armor_notice_dest.x + this_hds->stereo_offset;
         dest_rect_l.y = dest_rect_r.y = this_as->armor_notice_dest.y;
         dest_rect_l.w = dest_rect_r.w = this_as->armor_notice_dest.w;
         dest_rect_l.h = dest_rect_r.h = this_as->armor_notice_dest.h;
      }

      /* Warning states for armor components. */
      if ((this_element->warning_temp >= 0) && (this_element->last_temp >= 0)) {
         if (!(this_element->warn_state & WARN_OVER_TEMP) &&
             (this_element->last_temp > this_element->warning_temp)) {
            this_element->texture = this_element->texture_warning;
            this_element->warn_state |= WARN_OVER_TEMP;
            time(&armor_timeout);
            armor_timeout += this_as->armor_notice_timeout;
         } else if (this_element->warn_state & WARN_OVER_TEMP) {
            if (this_element->last_temp < (this_element->warning_temp * 0.97)) {
               this_element->warn_state &= ~WARN_OVER_TEMP;
               if (!this_element->warn_state) {
                  this_element->texture = this_element->texture_online;
               }
            }
         }
      }

      if ((this_element->warning_voltage >= 0) && (this_element->last_voltage >= 0)) {
         if (!(this_element->warn_state & WARN_OVER_VOLT) &&
             (this_element->last_voltage < this_element->warning_voltage)) {
            this_element->texture = this_element->texture_warning;
            this_element->warn_state |= WARN_OVER_VOLT;
            time(&armor_timeout);
            armor_timeout += this_as->armor_notice_timeout;
         } else if (this_element->warn_state & WARN_OVER_VOLT) {
            if (this_element->last_voltage > (this_element->warning_voltage * 1.03)) {
               this_element->warn_state &= ~WARN_OVER_VOLT;
               if (!this_element->warn_state) {
                  this_element->texture = this_element->texture_online;
               }
            }
         }
      }

      /* Deregister armor component on timeout. */
      if (this_element->mqtt_registered && ((curr_time - this_as->armor_deregister) > this_element->mqtt_last_time))
      {
         this_element->mqtt_registered = 0;
         this_element->last_temp = this_element->last_voltage = -1.0;
         this_element->warn_state = WARN_NORMAL;
         this_element->texture = this_element->texture_offline;

         time(&armor_timeout);
         armor_timeout += this_as->armor_notice_timeout;

         snprintf(text, 2048, "%s disconnected.", this_element->name);
         mqttTextToSpeech(text);
      }

      if (armor_enabled) {
         renderStereo(this_element->texture, NULL, &dest_rect_l, &dest_rect_r, 0.0);
      }

      this_element = this_element->next;
   }
}

/* Render the armor overlays into the given texture in the given rectangle. */
void registerArmor(char *mqtt_device_in)
{
   char text[2048] = "";
   armor_settings *this_as = get_armor_settings();
   element *this_element = this_as->armor_elements;

   if (this_as->armor_elements == NULL)
   {
      return;
   }

   while (this_element != NULL) {
      if (strcmp(this_element->mqtt_device, mqtt_device_in) == 0)
      {
         if (!this_element->mqtt_registered)
         {
            this_element->mqtt_registered = 1;
            time(&this_element->mqtt_last_time);
            armor_timeout = this_element->mqtt_last_time + this_as->armor_notice_timeout;
            this_element->texture = this_element->texture_online;
            snprintf(text, 2048, "%s connected.", this_element->name);
            mqttTextToSpeech(text);
         } else {
            time(&this_element->mqtt_last_time);
         }

         break;
      }

      this_element = this_element->next;
   }
}
