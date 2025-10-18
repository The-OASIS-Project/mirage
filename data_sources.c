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
#include <stdlib.h>
#include <string.h>

#include "data_sources.h"
#include "system_metrics.h"
#include "mirage.h"
#include "config_manager.h"
#include "logging.h"
#include "sim_data.h"

/* External reference to FPS counter */
extern double averageFrameRate;

int is_dynamic_source(const char *source) {
   if (!source || strlen(source) < 3) {
      return 0;
   }
   
   size_t len = strlen(source);
   return (source[0] == '*' && source[len - 1] == '*');
}

float resolve_data_source_float(const char *source) {
   if (!source || strlen(source) == 0) {
      LOG_WARNING("resolve_data_source_float: NULL or empty source");
      return 0.0f;
   }

   /* Static value (no asterisks) - parse as number */
   if (!is_dynamic_source(source)) {
      return atof(source);
   }

   /* Get device pointers */
   motion *this_motion = get_motion_dev();
   gps *this_gps = get_gps_dev();
   enviro *this_enviro = get_enviro_dev();
   hud_display_settings *this_hds = get_hud_display_settings();

   /* === PERFORMANCE METRICS === */
   if (strcmp(source, "*FPS*") == 0) {
      return (float)averageFrameRate;
   }

   /* === BATTERY METRICS === */
   if (strcmp(source, "*BATTERY*") == 0 || strcmp(source, "*BATTERY_LEVEL*") == 0) {
      return system_metrics.power_available ? system_metrics.battery_level : 0.0f;
   }
   if (strcmp(source, "*BATTERY_VOLTAGE*") == 0) {
      return system_metrics.power_available ? system_metrics.battery_voltage : 0.0f;
   }
   if (strcmp(source, "*BATTERY_CURRENT*") == 0) {
      return system_metrics.power_available ? system_metrics.battery_current : 0.0f;
   }
   if (strcmp(source, "*BATTERY_POWER*") == 0) {
      return system_metrics.power_available ? system_metrics.battery_consumption : 0.0f;
   }
   if (strcmp(source, "*BATTERY_TEMP*") == 0) {
      return system_metrics.power_available ? system_metrics.battery_temperature : 0.0f;
   }

   /* === SYSTEM METRICS === */
   if (strcmp(source, "*CPU*") == 0) {
      return system_metrics.cpu_available ? system_metrics.cpu_usage : 0.0f;
   }
   if (strcmp(source, "*MEM*") == 0) {
      return system_metrics.memory_available ? system_metrics.memory_usage : 0.0f;
   }
   if (strcmp(source, "*SYSTEM_TEMP*") == 0) {
      return system_metrics.system_temp_available ? system_metrics.system_temperature : 0.0f;
   }
   if (strcmp(source, "*SYSTEM_TEMP_F*") == 0) {
      if (system_metrics.system_temp_available) {
         return system_metrics.system_temperature * 9.0f / 5.0f + 32.0f;
      }
      return 0.0f;
   }
   if (strcmp(source, "*FAN*") == 0) {
      return system_metrics.fan_available ? (float)system_metrics.fan_load : 0.0f;
   }

   /* === MOTION/ORIENTATION === */
   if (strcmp(source, "*PITCH*") == 0) {
      return this_motion->pitch + this_hds->pitch_offset;
   }
   if (strcmp(source, "*ROLL*") == 0) {
      return this_motion->roll;
   }
   if (strcmp(source, "*HEADING*") == 0) {
      return this_motion->heading;
   }

   /* === GPS DATA === */
   if (strcmp(source, "*ALTITUDE*") == 0) {
      return this_gps->altitude;
   }
   if (strcmp(source, "*SPEED*") == 0) {
      return this_gps->speed;
   }
   if (strcmp(source, "*LATITUDE*") == 0) {
      return (this_gps->latitudeDegrees != 0.0) ? 
             this_gps->latitudeDegrees : this_gps->latitude;
   }
   if (strcmp(source, "*LONGITUDE*") == 0) {
      return (this_gps->longitudeDegrees != 0.0) ? 
             this_gps->longitudeDegrees : this_gps->longitude;
   }

   /* === ENVIRONMENTAL SENSORS === */
   if (strcmp(source, "*HELMTEMP*") == 0) {
      return this_enviro->temp;
   }
   if (strcmp(source, "*HELMTEMP_F*") == 0) {
      return this_enviro->temp * 9.0f / 5.0f + 32.0f;
   }
   if (strcmp(source, "*HELMHUM*") == 0) {
      return this_enviro->humidity;
   }
   if (strcmp(source, "*AIRQUALITY*") == 0) {
      return this_enviro->air_quality;
   }
   if (strcmp(source, "*TVOC*") == 0) {
      return this_enviro->tvoc_ppb;
   }
   if (strcmp(source, "*ECO2*") == 0) {
      return this_enviro->eco2_ppm;
   }
   if (strcmp(source, "*CO2*") == 0) {
      return this_enviro->co2_ppm;
   }
   if (strcmp(source, "*HEATINDEX_C*") == 0) {
      return this_enviro->heat_index_c;
   }
   if (strcmp(source, "*DEWPOINT*") == 0) {
      return this_enviro->dew_point;
   }

   /* === SIMULATED DATA SOURCES === */
   if (strcmp(source, "*SIM_LINEAR*") == 0) {
      return get_sim_linear();
   }
   if (strcmp(source, "*SIM_LINEAR_FAST*") == 0) {
      return get_sim_linear_fast();
   }
   if (strcmp(source, "*SIM_RANDOM*") == 0) {
      return get_sim_random();
   }
   if (strcmp(source, "*SIM_RPM*") == 0) {
      return get_sim_rpm();
   }
   if (strcmp(source, "*SIM_SPEED*") == 0) {
      return get_sim_speed();
   }
   if (strcmp(source, "*SIM_ENGINE_TEMP*") == 0) {
      return get_sim_engine_temp();
   }
   if (strcmp(source, "*SIM_FUEL*") == 0) {
      return get_sim_fuel();
   }
   if (strcmp(source, "*SIM_BOOST*") == 0) {
      return get_sim_boost();
   }
   if (strcmp(source, "*SIM_THROTTLE*") == 0) {
      return get_sim_throttle();
   }

   /* Unknown source - log warning and return 0 */
   LOG_WARNING("Unknown dynamic data source: '%s'", source);
   return 0.0f;
}

