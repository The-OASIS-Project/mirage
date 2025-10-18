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

#ifndef SIM_DATA_H
#define SIM_DATA_H

/**
 * @file sim_data.h
 * @brief Simulated data sources for testing gauges and HUD elements
 *
 * Provides various simulated data patterns for testing and demonstration
 * when real sensors are not available.
 */

/**
 * @brief Initialize simulated data system
 *
 * Should be called once at startup.
 */
void init_sim_data(void);

/**
 * @brief Update all simulated data sources
 *
 * Should be called once per frame to update time-based simulations.
 */
void update_sim_data(void);

/**
 * @brief Get smooth linear oscillating value (0-100)
 *
 * Oscillates smoothly between 0 and 100 over ~10 seconds.
 * Good for testing smooth gauges.
 *
 * @return Current simulated value (0.0-100.0)
 */
float get_sim_linear(void);

/**
 * @brief Get fast linear oscillating value (0-100)
 *
 * Oscillates smoothly between 0 and 100 over ~3 seconds.
 * Good for testing rapid updates.
 *
 * @return Current simulated value (0.0-100.0)
 */
float get_sim_linear_fast(void);

/**
 * @brief Get semi-random jumping value (0-100)
 *
 * Jumps to random values with some smoothing.
 * Good for testing noisy sensor data.
 *
 * @return Current simulated value (0.0-100.0)
 */
float get_sim_random(void);

/**
 * @brief Get smooth sine wave oscillation
 *
 * Pure sine wave oscillation, good for testing smooth interpolation.
 *
 * @param min Minimum value
 * @param max Maximum value
 * @param period_seconds How long for one complete cycle
 * @return Current value between min and max
 */
float get_sim_sine(float min, float max, float period_seconds);

/**
 * @brief Get simulated RPM (automotive)
 *
 * Simulates engine RPM with realistic patterns:
 * - Idle around 800 RPM
 * - Occasional revs to 3000-6000 RPM
 * - Random variation
 *
 * @return RPM value (700-7000)
 */
float get_sim_rpm(void);

/**
 * @brief Get simulated vehicle speed (automotive)
 *
 * Simulates vehicle speed with realistic acceleration/deceleration.
 *
 * @return Speed in MPH (0-120)
 */
float get_sim_speed(void);

/**
 * @brief Get simulated engine temperature (automotive)
 *
 * Simulates engine warming up from cold to operating temp.
 *
 * @return Temperature in Fahrenheit (60-220)
 */
float get_sim_engine_temp(void);

/**
 * @brief Get simulated fuel level (automotive)
 *
 * Slowly decreases over time, resets when low.
 *
 * @return Fuel percentage (0-100)
 */
float get_sim_fuel(void);

/**
 * @brief Get simulated boost/vacuum pressure
 *
 * Simulates turbo boost and manifold vacuum.
 *
 * @return Pressure in PSI (-15 to +20)
 */
float get_sim_boost(void);

/**
 * @brief Get simulated throttle position
 *
 * Simulates throttle opening/closing.
 *
 * @return Throttle percentage (0-100)
 */
float get_sim_throttle(void);

#endif /* SIM_DATA_H */

