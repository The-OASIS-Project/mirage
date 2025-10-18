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
#include <math.h>
#include <time.h>
#include <sys/time.h>

#include "sim_data.h"
#include "logging.h"

/* Simulation state */
static double sim_time = 0.0;
static double sim_start_time = 0.0;
static int sim_initialized = 0;

/* Automotive simulation state */
static float sim_rpm_target = 800.0f;
static float sim_rpm_current = 800.0f;
static float sim_speed_target = 0.0f;
static float sim_speed_current = 0.0f;
static float sim_engine_temp = 60.0f;
static float sim_fuel_level = 85.0f;
static float sim_throttle_target = 0.0f;
static float sim_throttle_current = 0.0f;
static float sim_boost_current = -5.0f;

/* Random state for variation */
static unsigned int sim_rand_state = 0;

/**
 * @brief Get current time in seconds (high precision)
 */
static double get_time_seconds(void) {
   struct timeval tv;
   gettimeofday(&tv, NULL);
   return tv.tv_sec + tv.tv_usec / 1000000.0;
}

/**
 * @brief Simple linear interpolation
 */
static float lerp(float a, float b, float t) {
   return a + (b - a) * t;
}

/**
 * @brief Fast pseudo-random float between 0 and 1
 */
static float sim_randf(void) {
   sim_rand_state = sim_rand_state * 1103515245 + 12345;
   return (sim_rand_state / 65536) % 32768 / 32768.0f;
}

void init_sim_data(void) {
   sim_start_time = get_time_seconds();
   sim_time = 0.0;
   sim_initialized = 1;
   
   /* Seed random */
   sim_rand_state = (unsigned int)time(NULL);
   
   LOG_INFO("Simulated data sources initialized");
}

void update_sim_data(void) {
   if (!sim_initialized) {
      init_sim_data();
   }
   
   /* Update simulation time */
   sim_time = get_time_seconds() - sim_start_time;
   
   /* Update automotive simulations */
   
   /* RPM: Idle most of time, occasional revs */
   static double last_rpm_change = 0.0;
   if (sim_time - last_rpm_change > 3.0) {
      last_rpm_change = sim_time;
      if (sim_randf() > 0.7f) {
         /* Rev the engine */
         sim_rpm_target = 3000.0f + sim_randf() * 3000.0f;
      } else {
         /* Back to idle */
         sim_rpm_target = 800.0f + sim_randf() * 200.0f;
      }
   }
   
   /* Smooth RPM changes */
   sim_rpm_current = lerp(sim_rpm_current, sim_rpm_target, 0.05f);
   
   /* Speed: Gradual changes */
   static double last_speed_change = 0.0;
   if (sim_time - last_speed_change > 5.0) {
      last_speed_change = sim_time;
      sim_speed_target = sim_randf() * 80.0f;
   }
   sim_speed_current = lerp(sim_speed_current, sim_speed_target, 0.02f);
   
   /* Engine temp: Warm up then stabilize */
   float target_temp = (sim_time < 60.0) ? 
                       60.0f + (sim_time / 60.0f) * 150.0f : 210.0f;
   sim_engine_temp = lerp(sim_engine_temp, target_temp, 0.01f);
   
   /* Fuel: Slowly decrease */
   sim_fuel_level -= 0.001f;
   if (sim_fuel_level < 10.0f) {
      sim_fuel_level = 95.0f;  /* "Refuel" */
   }
   
   /* Throttle: Correlates with RPM changes */
   sim_throttle_target = (sim_rpm_target > 2000.0f) ? 
                         40.0f + sim_randf() * 60.0f : 
                         sim_randf() * 10.0f;
   sim_throttle_current = lerp(sim_throttle_current, sim_throttle_target, 0.08f);
   
   /* Boost: Follows throttle with lag */
   float boost_target = (sim_throttle_current > 50.0f) ? 
                        (sim_throttle_current - 50.0f) * 0.4f - 5.0f :
                        -5.0f - sim_randf() * 10.0f;
   sim_boost_current = lerp(sim_boost_current, boost_target, 0.06f);
}

float get_sim_linear(void) {
   /* 10 second period sine wave, 0-100 range */
   return 50.0f + 50.0f * sinf(sim_time * 2.0f * M_PI / 10.0f);
}

float get_sim_linear_fast(void) {
   /* 3 second period sine wave, 0-100 range */
   return 50.0f + 50.0f * sinf(sim_time * 2.0f * M_PI / 3.0f);
}

float get_sim_random(void) {
   /* Semi-random with smoothing */
   static float current_value = 50.0f;
   static float target_value = 50.0f;
   static double last_change = 0.0;
   
   /* Change target every 0.5 seconds */
   if (sim_time - last_change > 0.5) {
      last_change = sim_time;
      target_value = sim_randf() * 100.0f;
   }
   
   /* Smooth towards target */
   current_value = lerp(current_value, target_value, 0.2f);
   
   return current_value;
}

float get_sim_sine(float min, float max, float period_seconds) {
   float range = max - min;
   return min + range * 0.5f * (1.0f + sinf(sim_time * 2.0f * M_PI / period_seconds));
}

float get_sim_rpm(void) {
   /* Add small random variation for realism */
   return sim_rpm_current + (sim_randf() - 0.5f) * 50.0f;
}

float get_sim_speed(void) {
   return sim_speed_current + (sim_randf() - 0.5f) * 2.0f;
}

float get_sim_engine_temp(void) {
   return sim_engine_temp + (sim_randf() - 0.5f) * 3.0f;
}

float get_sim_fuel(void) {
   return sim_fuel_level;
}

float get_sim_boost(void) {
   return sim_boost_current + (sim_randf() - 0.5f) * 1.0f;
}

float get_sim_throttle(void) {
   return sim_throttle_current;
}

