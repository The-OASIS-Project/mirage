#include <stdio.h>
#include <mosquitto.h>

#include "defines.h"
#include "armor.h"
#include "command_processing.h"
#include "config_parser.h"
#include "config_manager.h"

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

   printf("Moquitto successfully connected. Subscribing to...");
   /* This works. I think I like the idea of a registration service better but... */
   while (this_element != NULL) {
      printf(" %s\n", this_element->mqtt_device);
	   rc = mosquitto_subscribe(mosq, NULL, this_element->mqtt_device, 1);
	   if(rc != MOSQ_ERR_SUCCESS){
		   fprintf(stderr, "Error subscribing: %s\n", mosquitto_strerror(rc));
		   mosquitto_disconnect(mosq);
	   }

      this_element = this_element->next;
   }
   printf("\n");
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
		fprintf(stderr, "Error: All subscriptions rejected.\n");
		mosquitto_disconnect(mosq);
	}
}

/* Callback called when the client receives a message. */
void on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *msg)
{
	printf("%s %d %s\n", msg->topic, msg->qos, (char *)msg->payload);

   if (strcmp(msg->topic, "hud") != 0) {
      /* FIXME: Right now if it's not for "hud," I'm assuming it's from an armor component.
       * I probably need a better way. ;-)
       */
      registerArmor(msg->topic);
   }

   parse_json_command((char *)msg->payload);
}
/* End Mosquitto Stuff */


