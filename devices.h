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

