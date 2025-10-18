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

#ifndef DATA_SOURCES_H
#define DATA_SOURCES_H

/**
 * @file data_sources.h
 * @brief Dynamic data source resolution for HUD elements
 *
 * Provides unified access to system sensors, metrics, and live data for both
 * text elements and gauge widgets. Resolves special tokens like *BATTERY*,
 * *FPS*, *PITCH*, etc. to their corresponding sensor values.
 */

/**
 * @brief Resolve a data source string to a floating-point value
 *
 * Checks if the source is a dynamic data token (starts/ends with asterisks).
 * If dynamic, retrieves the current value from the appropriate sensor.
 * If static, parses as a numeric string.
 *
 * Supported dynamic sources:
 *   *FPS*              - Current frame rate
 *   *BATTERY*          - Battery percentage (0-100)
 *   *BATTERY_LEVEL*    - Battery percentage (0-100, same as *BATTERY*)
 *   *BATTERY_VOLTAGE*  - Battery voltage in volts
 *   *BATTERY_CURRENT*  - Battery current in amps
 *   *BATTERY_POWER*    - Battery power consumption in watts
 *   *BATTERY_TEMP*     - Battery temperature in Celsius
 *   *CPU*              - CPU usage percentage (0-100)
 *   *MEM*              - Memory usage percentage (0-100)
 *   *SYSTEM_TEMP*      - System temperature in Celsius
 *   *FAN*              - Fan speed percentage (0-100)
 *   *PITCH*            - Pitch angle in degrees
 *   *ROLL*             - Roll angle in degrees
 *   *HEADING*          - Heading angle in degrees (0-360)
 *   *ALTITUDE*         - GPS altitude in meters
 *   *SPEED*            - GPS speed in m/s
 *   *LATITUDE*         - GPS latitude in degrees
 *   *LONGITUDE*        - GPS longitude in degrees
 *   *HELMTEMP*         - Environmental temperature in Celsius
 *   *HELMTEMP_F*       - Environmental temperature in Fahrenheit
 *   *HELMHUM*          - Environmental humidity percentage
 *   *AIRQUALITY*       - Air quality index (0-100)
 *   *TVOC*             - Total VOC in ppb
 *   *ECO2*             - Equivalent CO2 in ppm
 *   *CO2*              - CO2 level in ppm
 *   *HEATINDEX_C*      - Heat index in Celsius
 *   *DEWPOINT*         - Dew point in Celsius
 *
 * @param source The source string to resolve (e.g., "*FPS*" or "75.5")
 * @return The resolved floating-point value, or 0.0 if source is unknown
 */
float resolve_data_source_float(const char *source);

/**
 * @brief Check if a source string is a dynamic data source
 *
 * Dynamic sources are identified by asterisks at start and end.
 *
 * @param source The source string to check
 * @return 1 if dynamic, 0 if static
 */
int is_dynamic_source(const char *source);

#endif /* DATA_SOURCES_H */
