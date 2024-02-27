#ifndef MOSQUITTO_COMMS_H
#define MOSQUITTO_COMMS_H

void on_connect(struct mosquitto *mosq, void *obj, int reason_code);
void on_subscribe(struct mosquitto *mosq, void *obj, int mid, int qos_count, const int *granted_qos);
void on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *msg);

#endif // MOSQUITTO_COMMS_H

