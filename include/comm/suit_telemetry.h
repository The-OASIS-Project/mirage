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
 *
 * Suit telemetry republish — re-emits the AURA helmet sensor data (and SPARK
 * armor telemetry) that MIRAGE already parses from serial onto MQTT topics
 * helmet/telemetry / armor/telemetry, so DAWN's suit_service (SAGE proactive-
 * attention PRE phase) can consume it like any component feed.  Publishing
 * happens at the existing parse point (no re-parse), rate-limited per sensor
 * type to keep the streaming Motion feed from flooding the broker.
 */

#ifndef SUIT_TELEMETRY_H
#define SUIT_TELEMETRY_H

#include <stdbool.h>

#include "hardware/devices.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Republish the latest AURA environment sample to helmet/telemetry. */
void suit_telemetry_publish_enviro(const enviro *e);

/** @brief Republish the latest AURA orientation sample to helmet/telemetry. */
void suit_telemetry_publish_motion(const motion *m);

/** @brief Republish the latest AURA GPS sample to helmet/telemetry. */
void suit_telemetry_publish_gps(const gps *g);

/**
 * @brief Republish one SPARK armor piece's telemetry to armor/telemetry.
 *
 * @param piece        Piece identifier (armor element name); required.
 * @param temp         Latest piece temperature (deg C); emitted only if have_temp.
 * @param voltage      Latest piece voltage (V); emitted only if have_voltage.
 * @param have_temp    Whether @p temp is present in this update.
 * @param have_voltage Whether @p voltage is present in this update.
 */
void suit_telemetry_publish_armor(const char *piece,
                                  double temp,
                                  double voltage,
                                  bool have_temp,
                                  bool have_voltage);

#ifdef __cplusplus
}
#endif

#endif /* SUIT_TELEMETRY_H */
