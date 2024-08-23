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

#include <stdio.h>
#include <mosquitto.h>

#include "defines.h"
#include "armor.h"
#include "command_processing.h"
#include "config_parser.h"
#include "config_manager.h"
#include "logging.h"

/* Mosquitto STUFF */
/* Callback called when the client receives a CONNACK message from the broker. */
void on_connect(struct mosquitto *mosq, void *obj, int reason_code)
{
   int rc;
   armor_settings *this_as = get_armor_settings();
   element *this_element = this_as->armor_elements;

   if (this_as->armor_elements == NULL)
   {
      return;
   }

   if(reason_code != 0){
      mosquitto_disconnect(mosq);
      return;
   }

   LOG_INFO("Moquitto successfully connected. Subscribing to...");
   /* This works. I think I like the idea of a registration service better but... */
   while (this_element != NULL) {
      LOG_INFO(" %s\n", this_element->mqtt_device);
	   rc = mosquitto_subscribe(mosq, NULL, this_element->mqtt_device, 1);
	   if(rc != MOSQ_ERR_SUCCESS){
		   LOG_ERROR("Error subscribing: %s", mosquitto_strerror(rc));
		   mosquitto_disconnect(mosq);
	   }

      this_element = this_element->next;
   }
}

/* Callback called when the broker sends a SUBACK in response to a SUBSCRIBE. */
void on_subscribe(struct mosquitto *mosq, void *obj, int mid, int qos_count, const int *granted_qos)
{
	int i;
	bool have_subscription = false;

	for(i=0; i<qos_count; i++){
		if(granted_qos[i] <= 2){
			have_subscription = true;
		}
	}
	if(have_subscription == false){
		LOG_ERROR("Error: All subscriptions rejected.");
		mosquitto_disconnect(mosq);
	}
}

/* Callback called when the client receives a message. */
void on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *msg)
{
   LOG_INFO("%s %d %s", msg->topic, msg->qos, (char *)msg->payload);

   if (strcmp(msg->topic, "hud") != 0) {
      /* FIXME: Right now if it's not for "hud," I'm assuming it's from an armor component.
       * I probably need a better way. ;-)
       */
      registerArmor(msg->topic);
   }

   parse_json_command((char *)msg->payload, (char *)msg->topic);
}
/* End Mosquitto Stuff */


