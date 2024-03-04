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
 * All contributions to this project are agreed to be licensed under the
 * GPLv3 or any later version. Contributions are understood to be
 * any modifications, enhancements, or additions to the project
 * and become the property of the original author Kris Kersey.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>

#include "devices.h"
#include "main.h"
#include "config_manager.h"

static long double loadavg = 0.0;         /* Load average of the CPU(s) in percent. */

/* If we have a CPU monitoring element, this thread is launched to maintain the stat.
 * Calculated every 2 seconds. */
void *cpu_utilization_thread(void *arg)
{
   long double a[6], b[6], cpu_delta;
   FILE *fp;

   while (!checkShutdown()) {
      fp = fopen("/proc/stat", "r");
      fscanf(fp, "%*s %Lf %Lf %Lf %Lf %Lf %Lf", &a[0], &a[1], &a[2], &a[3], &a[4], &a[5]);
      fclose(fp);
      sleep(2);

      fp = fopen("/proc/stat", "r");
      fscanf(fp, "%*s %Lf %Lf %Lf %Lf %Lf %Lf", &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]);
      fclose(fp);

      cpu_delta =
          (b[0] + b[1] + b[2] + b[3] + b[4] + b[5]) - (a[0] + a[1] + a[2] + a[3] + a[4] + a[5]);
      loadavg = (100 * (cpu_delta - (b[3] - a[3]))) / cpu_delta;
   }

   return NULL;
}

long double get_loadavg(void)
{
   return loadavg;
}

/* If we have a memory monitoring element, this function calculate memory usage. */
long double get_mem_usage(void)
{
   char buf[100];
   char *cp = NULL;
   long double mem_total = 0.0;
   long double mem_avail = 0.0;
   long double mem_used = 0.0;
   FILE *fp;

   fp = fopen("/proc/meminfo", "r");
   fgets(buf, 100, fp);
   cp = &buf[9];
   mem_total = strtol(cp, NULL, 10);

   fgets(buf, 100, fp);
   fgets(buf, 100, fp);
   cp = &buf[13];
   mem_avail = strtol(cp, NULL, 10);

   fclose(fp);

   mem_used = ((mem_total - mem_avail) / mem_total) * 100.0;

   //printf("mem_total: %Lf, mem_avail: %Lf, mem_used: %Lf\n", mem_total, mem_avail, mem_used);

   return mem_used;
}

/* Get the wifi signal level from the wireless driver.
 * This returns 0-9 for display purposes. */
int get_wifi_signal_level(void)
{
   FILE *fp = NULL;
   char buf[125];
   char *found = NULL;
   char s_signal[7];
   int signal = -1;
   int level = 0;               /* 0-9 based on -30 to -90 dBm) */

   fp = fopen("/proc/net/wireless", "r");
   if (fp == NULL) {
      printf("No wireless found.\n");
      return 0;
   }

   while (fgets(buf, 125, fp) != NULL) {
      if ((found = strstr(buf, get_wifi_dev_name())) != NULL) {
         break;
      }
   }

   fclose(fp);

   if (found != NULL) {
      memset((void *)s_signal, '\0', 7);
      strncpy(s_signal, &found[19], 6);
      signal = atoi(s_signal);

      // Map from 0-9. Arduino map equation.
      if (signal == 0) {
         level = 0;
      } else if (signal > 0) {
         level = round((double)signal / 10.0);
      } else {
         level = (signal - -90) * (9 - 0) / (-30 - -90) + 0;
      }
   }
   //printf("Wireless signal: %d, level :%d\n", signal, level);
   if (level > 9)
      level = 9;
   if (level < 0)
      level = 0;

   return level;
}


