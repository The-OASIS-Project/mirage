#ifndef DEVICES_H
#define DEVICES_H

#include "defines.h"

/* Motion Object */
typedef struct _motion {
   int format;
   double heading, pitch, roll;
   double w, x, y, z;
} motion;

/* Environmental Object */
typedef struct _enviro {
   double temp;
   double humidity;
} enviro;

/* GPS Object */
typedef struct _gps {
   char time[9];
   char date[11];
   int fix;
   int quality;
   double latitude;
   double latitudeDegrees;
   char lat[2];
   double longitude;
   double longitudeDegrees;
   char lon[2];
   double speed;
   double angle;
   double altitude;
   int satellites;
} gps;

void *cpu_utilization_thread(void *arg);
long double get_loadavg(void);
long double get_mem_usage(void);
int get_wifi_signal_level(void);

#endif // DEVICES_H

