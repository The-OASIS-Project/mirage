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

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <json-c/json.h>

/* Serial Port */
#include <sys/ioctl.h>
#include <termios.h>
#include <sys/select.h>

#include "defines.h"
#include "armor.h"
#include "audio.h"
#include "command_processing.h"
#include "config_manager.h"
#include "logging.h"
#include "mirage.h"

#include <arpa/inet.h>

#define SERVER_TIMEOUT 10

static char raw_log[LOG_ROWS][LOG_LINE_LENGTH];
static int next_log_row = 0;

char (*get_raw_log(void))[LOG_LINE_LENGTH] {
   return raw_log;
}

// Serial port state management structure
typedef struct {
   int fd;                  // File descriptor for the serial port
   int enabled;             // Whether serial is enabled
   char port[24];           // Port name
   pthread_mutex_t mutex;   // Mutex for thread-safe access
} serial_state_t;

// Global instance with proper initialization
static serial_state_t serial_state = {
   .fd = -1,
   .enabled = 0,
   .port = "",
   .mutex = PTHREAD_MUTEX_INITIALIZER
};

/**
 * Checks if serial communication is enabled
 * @return 1 if enabled, 0 otherwise
 */
int serial_is_enabled(void) {
   int enabled = 0;
   pthread_mutex_lock(&serial_state.mutex);
   enabled = serial_state.enabled;
   pthread_mutex_unlock(&serial_state.mutex);
   return enabled;
}

/**
 * Gets the current serial file descriptor
 * @return File descriptor or -1 if not available
 */
int serial_get_fd(void) {
   int fd;
   pthread_mutex_lock(&serial_state.mutex);
   fd = serial_state.fd;
   pthread_mutex_unlock(&serial_state.mutex);
   return fd;
}

/**
 * Sets the serial state
 * @param enabled Whether serial is enabled (or -1 to leave unchanged)
 * @param port Port name (or NULL to leave unchanged)
 * @param fd File descriptor (or -1 to leave unchanged)
 */
void serial_set_state(int enabled, const char *port, int fd) {
   pthread_mutex_lock(&serial_state.mutex);

   if (enabled >= 0) {
       serial_state.enabled = enabled;
   }

   if (port != NULL) {
       strncpy(serial_state.port, port, sizeof(serial_state.port) - 1);
       serial_state.port[sizeof(serial_state.port) - 1] = '\0';
   }

   if (fd >= 0 || fd == -1) {  // Allow setting to -1 to indicate invalid
       serial_state.fd = fd;
   }

   pthread_mutex_unlock(&serial_state.mutex);
}

/**
 * Gets a copy of the current serial port name
 * @param buffer Buffer to store the port name
 * @param size Size of the buffer
 */
void serial_get_port(char *buffer, size_t size) {
   pthread_mutex_lock(&serial_state.mutex);
   strncpy(buffer, serial_state.port, size - 1);
   buffer[size - 1] = '\0';
   pthread_mutex_unlock(&serial_state.mutex);
}

/* Parse the JSON "commands" that come over serial/USB or MQTT. */
int parse_json_command(char *command_string, char *topic)
{
   armor_settings *this_as = get_armor_settings();
   element *armor_element = this_as->armor_elements;
   element *this_element = get_first_element();

   struct json_object *parsed_json = NULL;
   struct json_object *tmpobj = NULL;
   struct json_object *tmpobj2 = NULL;
   const char *tmpstr = NULL;
   const char *tmpstr2 = NULL;

   int enabled = -1;

   char text[2048] = "";
   int alreadySpoke = 0;

   /* For audio processing. */
   int command = 0;
   char filename[MAX_FILENAME_LENGTH];
   double start_percent = 0.0;

   motion *this_motion = get_motion_dev();
   enviro *this_enviro = get_enviro_dev();
   gps *this_gps = get_gps_dev();

   parsed_json = json_tokener_parse(command_string);
   json_object_object_get_ex(parsed_json, "device", &tmpobj);
   //printf("%s", command_string);
   /* Is this a device? */
   if (tmpobj != NULL) {
      tmpstr = json_object_get_string(tmpobj);
      //printf("device: %s\n", tmpstr);

      if (strcmp("Motion", tmpstr) == 0) {
         /* Motion */
         json_object_object_get_ex(parsed_json, "format", &tmpobj);
         tmpstr = json_object_get_string(tmpobj);
         /* Only look for "Orientation" style for now. */
         if (strcmp("Orientation", tmpstr) == 0) {
            json_object_object_get_ex(parsed_json, "heading", &tmpobj);

            // Get the sensor heading value
            double raw_heading = json_object_get_double(tmpobj);

            // If we get a negative value, convert from -180 to +180 range to 0 to 360 range
            if (raw_heading < 0) {
               raw_heading += 360.0;
            }

            // Apply inversion if needed
            if (get_inv_compass()) {
               this_motion->heading = 360.0 - raw_heading;
            } else {
               this_motion->heading = raw_heading;
            }

            json_object_object_get_ex(parsed_json, "pitch", &tmpobj);
            this_motion->pitch = json_object_get_double(tmpobj);

            json_object_object_get_ex(parsed_json, "roll", &tmpobj);
            this_motion->roll = json_object_get_double(tmpobj);
         }
         //printf("Motion: heading: %f, pitch, %f, roll: %f\n",
         //       this_motion->heading, this_motion->pitch, this_motion->roll);
      } else if (strcmp("Enviro", tmpstr) == 0) {
         /* Enviro */
         json_object_object_get_ex(parsed_json, "temp", &tmpobj);
         this_enviro->temp = json_object_get_double(tmpobj);

         json_object_object_get_ex(parsed_json, "humidity", &tmpobj);
         if (tmpobj != NULL) {
            this_enviro->humidity = json_object_get_double(tmpobj);
         } else {
            this_enviro->humidity = 0.0;
         }

         // Need to add for new environmental screen: tvoc_ppb, eco2_ppm, co2_ppm, co2_quality, co2_eco2_diff,
         //                                           co2_source_analysis, air_quality, air_quality_description, dew_point
         // Example: {"device":"Enviro","temp":22.41512,"humidity":34.46708,"tvoc_ppb":67,"eco2_ppm":491,
         //           "co2_ppm":530,"co2_quality":"Excellent","co2_eco2_diff":39,"co2_source_analysis":"Mixed sources",
         //           "air_quality":80,"air_quality_description":"Good","dew_point":5.969073}

         //printf("Enviro: temp: %f, humidity: %f\n", this_enviro->temp, this_enviro->humidity);
      } else if (strcmp("GPS", tmpstr) == 0) {
         /* GPS */
         json_object_object_get_ex(parsed_json, "time", &tmpobj);
         tmpstr = json_object_get_string(tmpobj);
         if (tmpstr != NULL) {
            strcpy(this_gps->time, tmpstr);
         }

         json_object_object_get_ex(parsed_json, "date", &tmpobj);
         tmpstr = json_object_get_string(tmpobj);
         if (tmpstr != NULL) {
            strcpy(this_gps->date, tmpstr);
         }

         json_object_object_get_ex(parsed_json, "fix", &tmpobj);
         this_gps->fix = json_object_get_int(tmpobj);

         if (this_gps->fix) {
            json_object_object_get_ex(parsed_json, "quality", &tmpobj);
            this_gps->quality = json_object_get_int(tmpobj);

            json_object_object_get_ex(parsed_json, "latitude", &tmpobj);
            this_gps->latitude = json_object_get_double(tmpobj);

            json_object_object_get_ex(parsed_json, "lat", &tmpobj);
            tmpstr = json_object_get_string(tmpobj);
            if (tmpstr != NULL) {
               strcpy(this_gps->lat, tmpstr);
            }

            json_object_object_get_ex(parsed_json, "latitudeDegrees", &tmpobj);
            this_gps->latitudeDegrees = json_object_get_double(tmpobj);

            json_object_object_get_ex(parsed_json, "longitude", &tmpobj);
            this_gps->longitude = json_object_get_double(tmpobj);

            json_object_object_get_ex(parsed_json, "lon", &tmpobj);
            tmpstr = json_object_get_string(tmpobj);
            if (tmpstr != NULL) {
               strcpy(this_gps->lon, tmpstr);
            }

            json_object_object_get_ex(parsed_json, "longitudeDegrees", &tmpobj);
            this_gps->longitudeDegrees = json_object_get_double(tmpobj);

            json_object_object_get_ex(parsed_json, "speed", &tmpobj);
            this_gps->speed = json_object_get_double(tmpobj);

            json_object_object_get_ex(parsed_json, "angle", &tmpobj);
            this_gps->angle = json_object_get_double(tmpobj);

            json_object_object_get_ex(parsed_json, "altitude", &tmpobj);
            this_gps->altitude = json_object_get_double(tmpobj);

            json_object_object_get_ex(parsed_json, "satellites", &tmpobj);
            this_gps->satellites = json_object_get_int(tmpobj);
         }
         //printf("GPS: time: %s, date: %s, fix: %d, quality: %d, latitude: %f, lat: %s, latitudeDegrees: %f, longitude: %f, lon: %s, longitudeDegrees: %f, speed: %f, angle: %f, altitude: %f, satellites: %d\n", this_gps->time, this_gps->date, this_gps->fix, this_gps->quality, this_gps->latitude, this_gps->lat, this_gps->latitudeDegrees, this_gps->longitude, this_gps->lon, this_gps->longitudeDegrees, this_gps->speed, this_gps->angle, this_gps->altitude, this_gps->satellites);
      } else if (strcmp("audio", tmpstr) == 0) {
         /* Audio */
         command = 0;
         filename[0] = '\0';
         start_percent = 0.0;

         json_object_object_get_ex(parsed_json, "command", &tmpobj);
         tmpstr = json_object_get_string(tmpobj);
         if (strcmp(tmpstr, "play") == 0) {
            command = SOUND_PLAY;
            json_object_object_get_ex(parsed_json, "arg1", &tmpobj);
            tmpstr = json_object_get_string(tmpobj);
            if (tmpstr != NULL) {
               strncpy(filename, tmpstr, MAX_FILENAME_LENGTH);
            }
            json_object_object_get_ex(parsed_json, "arg2", &tmpobj);
            start_percent = json_object_get_double(tmpobj);

            process_audio_command(command, filename, start_percent);
         } else if (strcmp(tmpstr, "stop") == 0) {
            command = SOUND_STOP;
            json_object_object_get_ex(parsed_json, "arg1", &tmpobj);
            tmpstr = json_object_get_string(tmpobj);
            if (tmpstr != NULL) {
               strncpy(filename, tmpstr, MAX_FILENAME_LENGTH);
            }

            process_audio_command(command, filename, start_percent);
         } else {
            LOG_WARNING("Unrecognized audio command: %s", tmpstr);
         }
      } else if (strcmp("viewing", tmpstr) == 0) {
         json_object_object_get_ex(parsed_json, "datetime", &tmpobj);
         tmpstr = json_object_get_string(tmpobj);
         trigger_snapshot(tmpstr);
      } else if (strcmp("ai", tmpstr) == 0) {
         const char *aiName = NULL;
         const char *aiState = NULL;

         json_object_object_get_ex(parsed_json, "name", &tmpobj);
         aiName = json_object_get_string(tmpobj);

         json_object_object_get_ex(parsed_json, "state", &tmpobj);
         aiState = json_object_get_string(tmpobj);

         process_ai_state(aiName, aiState);
      }

      json_object_object_get_ex(parsed_json, "action", &tmpobj2);
      if (tmpobj2 != NULL) {
         tmpstr2 = json_object_get_string(tmpobj2);

         if (strcmp(tmpstr2, "enable") == 0) {
            enabled = 1;
         } else if (strcmp(tmpstr2, "disable") == 0) {
            enabled = 0;
         }

         if (enabled > -1) {
            LOG_INFO("Going to enable or disable %s.", tmpstr);

            /* Recording/Streaming */
            if (!strcmp(tmpstr, "record")) {
               if (enabled) {
                  set_recording_state(RECORD);
               } else {
                  set_recording_state(DISABLED);
               }
            } else if (!strcmp(tmpstr, "stream")) {
               if (enabled) {
                  set_recording_state(STREAM);
               } else {
                  set_recording_state(DISABLED);
               }
            } else if (!strcmp(tmpstr, "record and stream")) {
               if (enabled) {
                  set_recording_state(RECORD_STREAM);
               } else {
                  set_recording_state(DISABLED);
               }
            }

            /* Armor is a special case. */
            if (!strcmp(tmpstr, "armor")) {
               //printf("Setting armor enabled to: %d\n", enabled);
               if (!alreadySpoke) {
                  snprintf(text, 2048, "%s armor display.",
                           enabled ? "Enabling" : "Disabling");
                  mqttTextToSpeech(text);
                  alreadySpoke++;
               }
               setArmorEnabled(enabled);
            }

            /* Let's find this device(s). */
            while (this_element != NULL) {
               if (!strcmp(this_element->name, tmpstr)) {
                  if (!alreadySpoke) {
                     snprintf(text, 2048, "%s %s display.",
                              enabled ? "Enabling" : "Disabling",
                              this_element->name);
                     mqttTextToSpeech(text);
                     alreadySpoke++;
                  }
                  //printf("Found the item: %s: %s\n", this_element->name,
                  //       enabled ? "enable" : "disable");
                  this_element->enabled = enabled;
               }
               this_element = this_element->next;
            }
         }
      }

      /* Let's see if this device is from armor. */
      while (armor_element != NULL) {
         if (!strcmp(armor_element->mqtt_device, topic)) {
            //printf("Found the item: %s : %s\n", armor_element->mqtt_device, topic);
            break;
         }
         armor_element = armor_element->next;
      }

      if (armor_element != NULL) {
         json_object_object_get_ex(parsed_json, "temp", &tmpobj);
         if (tmpobj != NULL) {
            armor_element->last_temp = json_object_get_double(tmpobj);
            //LOG_INFO("Setting last_temp = %0.2f on %s.", armor_element->last_temp,
            //       armor_element->mqtt_device);
         }

         json_object_object_get_ex(parsed_json, "voltage", &tmpobj);
         if (tmpobj != NULL) {
            armor_element->last_voltage = json_object_get_double(tmpobj);
            //LOG_INFO("Setting last_voltage = %0.2f on %s.", armor_element->last_voltage,
            //       armor_element->mqtt_device);
         }
      }
   }

   json_object_put(parsed_json);

   return SUCCESS;
}

/* Append a command we received into raw log buffer. */
void log_command(char *command)
{
   strncpy(raw_log[next_log_row], command, LOG_LINE_LENGTH);
   next_log_row++;
   if (next_log_row >= LOG_ROWS) {
      next_log_row = 0;
   }
}

/**
 * Opens and configures a serial port for communication
 *
 * @param port_name The name of the serial port to open
 * @param baud_rate The baud rate to configure
 * @param fd Pointer to store the resulting file descriptor
 * @return 0 on success, -1 on failure
 */
int serial_port_connect(const char *port_name, speed_t baud_rate, int *fd) {
    struct termios SerialPortSettings;

    if (strcmp(port_name, "") == 0) {
        *fd = fileno(stdin);
        LOG_INFO("Using stdin for commands instead of serial port");
        return 0;
    }

    // Open the serial port
    *fd = open(port_name, O_RDWR | O_NOCTTY);
    if (*fd == -1) {
        LOG_ERROR("Unable to open serial port %s: %s", port_name, strerror(errno));
        return -1;
    }

    LOG_INFO("Serial port %s opened successfully.", port_name);

    // Clear all settings
    memset(&SerialPortSettings, 0, sizeof(SerialPortSettings));

    // Get current settings
    if (tcgetattr(*fd, &SerialPortSettings) != 0) {
        LOG_ERROR("Failed to get port attributes: %s", strerror(errno));
        LOG_ERROR("Closing port.");
        close(*fd);
        return -1;
    }

    // Set input/output baud rate
    cfsetispeed(&SerialPortSettings, baud_rate);
    cfsetospeed(&SerialPortSettings, baud_rate);

    // 8N1 Mode
    SerialPortSettings.c_cflag &= ~PARENB;    // Disable parity
    SerialPortSettings.c_cflag &= ~CSTOPB;    // 1 stop bit
    SerialPortSettings.c_cflag &= ~CSIZE;     // Clear size bits
    SerialPortSettings.c_cflag |= CS8;        // 8 data bits

    // Flow control and modem settings
    SerialPortSettings.c_cflag |= CRTSCTS;    // Enable hardware flow Control
    SerialPortSettings.c_cflag |= CREAD | CLOCAL; // Enable receiver, ignore modem control lines
    SerialPortSettings.c_cflag &= ~HUPCL;     // Disable hangup on close

    // Raw input mode
    SerialPortSettings.c_lflag &= ~(ISIG | ICANON | IEXTEN | ECHO | ECHOE | ECHOK);
    SerialPortSettings.c_iflag = IGNBRK;

    // Raw output mode
    SerialPortSettings.c_oflag &= ~(OPOST | ONLCR);

    // Read timeouts
    SerialPortSettings.c_cc[VMIN] = 0;       // Return immediately with what's available
    SerialPortSettings.c_cc[VTIME] = 1;       // 0.1 seconds timeout between bytes

    // Apply settings
    if (tcsetattr(*fd, TCSANOW, &SerialPortSettings) != 0) {
        LOG_ERROR("ERROR in setting attributes: %s", strerror(errno));
        LOG_ERROR("Closing port.");
        close(*fd);
        return -1;
    }

    tcflush(*fd, TCIOFLUSH);

    // Short delay to let settings take effect
    usleep(100000);

    return 0;
}

/**
 * Main thread function for processing serial communication
 * Uses non-blocking I/O with select() timeouts for reliable operation
 *
 * @param arg Pointer to USB port name string
 * @return NULL on thread completion
 */
void *serial_command_processing_thread(void *arg) {
    int sfd = -1;
    fd_set read_fds;
    struct timeval timeout, read_timeout;
    char sread_buf[MAX_SERIAL_BUFFER_LENGTH];
    char *usb_port = (char *)arg;

    /* Command buffering */
    char command_buffer[MAX_SERIAL_BUFFER_LENGTH];
    int command_length = 0;

    /* Watchdog and reconnection settings */
    time_t last_successful_read = 0;
    int watchdog_timeout_sec = 10; // Consider device dead after 10 sec without data
    int reconnect_attempts = 0;
    int max_reconnect_delay = 30; // Max seconds between reconnection attempts
    speed_t serial_speed = B115200;

    /* Initialize command buffer */
    command_buffer[0] = '\0';
    memset(raw_log, '\0', LOG_ROWS * LOG_LINE_LENGTH);

    /* Initial connection */
    if (serial_port_connect(usb_port, serial_speed, &sfd) != 0) {
        if (strcmp(usb_port, "") != 0) {
            LOG_ERROR("Initial connection to serial port %s failed, will retry", usb_port);
            // Continue anyway - main loop will handle reconnection
        } else {
            LOG_ERROR("Failed to open stdin, exiting thread");
            return NULL;
        }
    } else {
        last_successful_read = time(NULL);
        serial_set_state(-1, NULL, sfd); // Update just the file descriptor
    }

    while (!checkShutdown()) {
        int select_result, bytes_available = 0;
        int read_result = 0;
        int flags;

        /* Check if we have a valid file descriptor */
        if (sfd < 0) {
            /* If we don't have a valid file descriptor, try to reconnect */
            if (strcmp(usb_port, "") != 0) {
                /* Calculate backoff delay - increases with failed attempts but caps at maximum */
                int backoff_delay = (reconnect_attempts > max_reconnect_delay) ?
                                    max_reconnect_delay : reconnect_attempts;

                LOG_INFO("Attempting to reconnect to %s (attempt %d, delay %d sec)",
                        usb_port, reconnect_attempts + 1, backoff_delay);

                sleep(backoff_delay); // Delay before reconnection attempt

                if (serial_port_connect(usb_port, serial_speed, &sfd) == 0) {
                    LOG_INFO("Successfully reconnected to %s", usb_port);
                    reconnect_attempts = 0;
                    last_successful_read = time(NULL);
                    serial_set_state(-1, NULL, sfd); // Update just the file descriptor
                } else {
                    LOG_WARNING("Failed to reconnect to %s", usb_port);
                    reconnect_attempts++;
                    continue;
                }
            } else {
                LOG_ERROR("Invalid file descriptor and not using serial port, exiting thread");
                break;
            }
        }

        /* Check watchdog timer */
        time_t current_time = time(NULL);
        if (strcmp(usb_port, "") != 0 && last_successful_read > 0 &&
            (current_time - last_successful_read) > watchdog_timeout_sec) {
            LOG_WARNING("No data received for %ld seconds, attempting reconnection",
                        current_time - last_successful_read);

            /* Close and invalidate the file descriptor */
            close(sfd);
            sfd = -1;
            continue; // Will trigger reconnection on next iteration
        }

        /* Wait for data with timeout using select() */
        FD_ZERO(&read_fds);
        FD_SET(sfd, &read_fds);
        timeout.tv_sec = 1;  // 1 second timeout for main loop
        timeout.tv_usec = 0;

        select_result = select(sfd + 1, &read_fds, NULL, NULL, &timeout);

        if (select_result < 0) {
            if (errno == EINTR) {
                /* Interrupted by signal, not an error */
                continue;
            }

            LOG_ERROR("Select error: %s", strerror(errno));

            /* Handle serious errors by reconnecting */
            if (strcmp(usb_port, "") != 0) {
                close(sfd);
                sfd = -1;
                continue; // Will trigger reconnection on next iteration
            } else {
                LOG_ERROR("Select error on stdin, exiting thread");
                break;
            }
        } else if (select_result == 0) {
            /* Timeout, no data available - this is normal */
            continue;
        }

        /* At this point, data should be available */
        if (!FD_ISSET(sfd, &read_fds)) {
            /* This shouldn't happen but check anyway */
            continue;
        }

        /* Check bytes available */
        if (ioctl(sfd, FIONREAD, &bytes_available) == -1) {
            LOG_ERROR("ioctl error: %s", strerror(errno));
            if (strcmp(usb_port, "") != 0) {
                close(sfd);
                sfd = -1;
                continue; // Will trigger reconnection on next iteration
            } else {
                LOG_ERROR("ioctl error on stdin, exiting thread");
                break;
            }
        }

        if (bytes_available == 0) {
            if (strcmp(usb_port, "") != 0) {
                LOG_WARNING("Zero bytes available but select indicated ready - possible device issue");
                /* Test the connection by doing a non-blocking zero-length write */
                flags = fcntl(sfd, F_GETFL, 0);
                fcntl(sfd, F_SETFL, flags | O_NONBLOCK);

                if (write(sfd, NULL, 0) < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                    LOG_WARNING("Connection test failed: %s", strerror(errno));
                    close(sfd);
                    sfd = -1;
                    continue; // Will trigger reconnection on next iteration
                }

                /* Restore original flags */
                fcntl(sfd, F_SETFL, flags);
            }
            continue;
        }

        /* Set non-blocking mode for read */
        flags = fcntl(sfd, F_GETFL, 0);
        fcntl(sfd, F_SETFL, flags | O_NONBLOCK);

        /* Read with select() timeout */
        FD_ZERO(&read_fds);
        FD_SET(sfd, &read_fds);
        read_timeout.tv_sec = 2;  // 2 second timeout for read operation
        read_timeout.tv_usec = 0;

        select_result = select(sfd + 1, &read_fds, NULL, NULL, &read_timeout);

        if (select_result <= 0) {
            if (select_result < 0) {
                LOG_ERROR("Read select error: %s", strerror(errno));
            } else {
                LOG_WARNING("Read operation timed out after 2 seconds");
            }

            /* Handle timeout or error */
            if (strcmp(usb_port, "") != 0) {
                LOG_WARNING("Possible device hang, attempting to recover connection");
                /* Restore original flags before closing */
                fcntl(sfd, F_SETFL, flags);
                close(sfd);
                sfd = -1;
                continue; // Will trigger reconnection on next iteration
            }

            /* Restore original flags */
            fcntl(sfd, F_SETFL, flags);
            continue;
        }

        /* Actually read the data */
        read_result = read(sfd, sread_buf,
                          bytes_available < (MAX_SERIAL_BUFFER_LENGTH - 1) ?
                          bytes_available : (MAX_SERIAL_BUFFER_LENGTH - 1));

        /* Restore original flags */
        fcntl(sfd, F_SETFL, flags);

        if (read_result < 0) {
            LOG_ERROR("Read error: %s", strerror(errno));

            /* Handle serious I/O errors */
            if (errno == EIO || errno == ENXIO || errno == ENODEV || errno == EBADF) {
                LOG_WARNING("Serious I/O error, reconnecting");
                if (strcmp(usb_port, "") != 0) {
                    close(sfd);
                    sfd = -1;
                    continue; // Will trigger reconnection on next iteration
                } else {
                    LOG_ERROR("Cannot recover from read error on stdin");
                    break;
                }
            }
            continue;
        } else if (read_result == 0) {
            LOG_WARNING("Zero bytes read despite having bytes available - possible disconnection");
            if (strcmp(usb_port, "") != 0) {
                close(sfd);
                sfd = -1;
                continue; // Will trigger reconnection on next iteration
            }
            continue;
        }

        /* Update watchdog timer and reset reconnection attempts on successful read */
        last_successful_read = time(NULL);
        reconnect_attempts = 0;

        /* Process the received data */
        sread_buf[read_result] = '\0';

        for (int j = 0; j < read_result; j++) {
            if (sread_buf[j] == '\n') {
                command_buffer[command_length] = '\0';
                if (command_length > 0) {
                    log_command(command_buffer);
                    registerArmor("helmet");
                    parse_json_command(command_buffer, "helmet");
                }
                command_buffer[0] = '\0';
                command_length = 0;
            } else if (sread_buf[j] == '\r') {
                /* Ignore carriage returns */
            } else if (command_length < MAX_SERIAL_BUFFER_LENGTH - 2) {
                command_buffer[command_length++] = sread_buf[j];
            } else {
                LOG_WARNING("Command buffer overflow, discarding data");
                command_buffer[0] = '\0';
                command_length = 0;
            }
        }
    }

    /* Clean up on thread exit */
    if (sfd >= 0) {
        close(sfd);
        serial_set_state(-1, NULL, -1); // Mark the fd as invalid
    }

    LOG_INFO("Serial command processing thread exiting");
    return NULL;
}

/**
 * Sends a command string to the connected serial device
 *
 * @param command The JSON command string to send
 * @return 0 on success, -1 on failure
 */
int serial_port_send(const char *command) {
   int fd;

   // Check if serial is enabled
   if (!serial_is_enabled()) {
      return -1;
   }

   // Get the file descriptor
   fd = serial_get_fd();
   if (fd < 0) {
      return -1;
   }

   // Send the command
   size_t cmd_len = strlen(command);
   char buffer[MAX_SERIAL_BUFFER_LENGTH];

   strncpy(buffer, command, MAX_SERIAL_BUFFER_LENGTH - 2);
   buffer[MAX_SERIAL_BUFFER_LENGTH - 2] = '\0'; // Ensure null termination with space for newline

   // Make sure the command ends with a newline
   cmd_len = strlen(buffer);
   if (cmd_len > 0 && buffer[cmd_len - 1] != '\n') {
      buffer[cmd_len] = '\n';
      buffer[cmd_len + 1] = '\0';
      cmd_len++; // Update length to include newline
   }

   // Send the command
   ssize_t bytes_written = write(fd, buffer, cmd_len);
   if (bytes_written < 0) {
      LOG_ERROR("Failed to send command to serial port: %s", strerror(errno));
      return -1;
   }

   return 0;
}

/**
 * Forwards helmet faceplate commands from MQTT to the serial port
 *
 * @param command_string The JSON command string
 * @return 0 on success, -1 on failure
 */
int forward_helmet_command_to_serial(char *command_string) {
   // Only forward if serial is enabled
   if (!serial_is_enabled()) {
      LOG_WARNING("Serial not enabled. Not forwarding helmet message.");
      return -1;
   }

   LOG_INFO("Forwarding helmet command to serial: %s", command_string);
   return serial_port_send(command_string);
}

void *socket_command_processing_thread(void *arg)
{
   int server_fd, new_socket;
   struct sockaddr_in address;
   int opt = 1;
   int addrlen = sizeof(address);
   char buffer[MAX_SERIAL_BUFFER_LENGTH] = {0};

   // Creating socket file descriptor
   if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
      LOG_ERROR("Socket creation failed.");
      return NULL;
   }

   // Attaching socket to the port
   if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
      LOG_ERROR("Setsockopt failed.");
      close(server_fd);
      return NULL;
   }

   address.sin_family = AF_INET;
   address.sin_addr.s_addr = INADDR_ANY;
   address.sin_port = htons(HELMET_PORT);

   // Binding the socket to the network address and port
   if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
      LOG_ERROR("Socket bind failed.");
      close(server_fd);
      return NULL;
   }

   // Start listening for incoming connections
   if (listen(server_fd, 3) < 0) {
      LOG_ERROR("Listen failed.");
      close(server_fd);
      return NULL;
   }

   LOG_INFO("Server is listening on port %d\n", HELMET_PORT);

   while (!checkShutdown()) {
      fd_set readfds;
      struct timeval reconnect_timeout;
      int activity;

      FD_ZERO(&readfds);
      FD_SET(server_fd, &readfds);

      // Set timeout
      reconnect_timeout.tv_sec = SERVER_TIMEOUT;
      reconnect_timeout.tv_usec = 0;

      activity = select(server_fd + 1, &readfds, NULL, NULL, &reconnect_timeout);

      if (activity < 0 && errno != EINTR) {
         LOG_ERROR("Select error.");
         continue;
      }

      if (activity > 0 && FD_ISSET(server_fd, &readfds)) {
         struct timeval read_timeout;

         new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
         if (new_socket < 0) {
            LOG_ERROR("Accept failed.");
            continue;
         }

         LOG_INFO("Accepted new connection.");

         // Set timeout
         read_timeout.tv_sec = SERVER_TIMEOUT;
         read_timeout.tv_usec = 0;

         // Set the receive timeout for the new socket
         if (setsockopt(new_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&read_timeout, sizeof(read_timeout)) < 0) {
            LOG_ERROR("Setting socket receive timeout failed.");
            close(new_socket);
            continue;
         }

         while (!checkShutdown()) {
            int bytes_read = read(new_socket, buffer, sizeof(buffer) - 1);
            if (bytes_read > 0) {
               buffer[bytes_read] = '\0'; // Null-terminate the received string
               registerArmor("helmet");
               parse_json_command(buffer, "helmet");
            } else if (bytes_read == 0) {
               LOG_INFO("Client disconnected.");
               break;
            } else if (bytes_read < 0 && errno == EWOULDBLOCK) {
               LOG_WARNING("Socket receive timed out.");
               break; // Timeout occurred
            } else if (bytes_read < 0) {
               LOG_ERROR("Socket read failed with error: %s", strerror(errno));
               break;
            }
         }

         close(new_socket);
         LOG_INFO("Closed connection socket, ready for new connections.");
      }
   }

   close(server_fd);
   LOG_INFO("Server socket closed.");

   return NULL;
}
