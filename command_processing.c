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

#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <json-c/json.h>

/* Serial Port */
#include <termios.h>
#include <sys/select.h>

#include "defines.h"
#include "main.h"
#include "audio.h"
#include "armor.h"
#include "config_manager.h"
#include "command_processing.h"

static char raw_log[LOG_ROWS][LOG_LINE_LENGTH];
static int next_log_row = 0;

char (*get_raw_log(void))[LOG_LINE_LENGTH] {
   return raw_log;
}

/* Parse the JSON "commands" that come over serial/USB or MQTT. */
int parse_json_command(char *command_string)
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
            if (get_inv_compass()) {
               this_motion->heading =
                   360.0 + ((0.0 - 360.0) / (360.0 - 0.0)) * (json_object_get_double(tmpobj) - 0.0);
            } else {
               this_motion->heading = json_object_get_double(tmpobj);
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
            fprintf(stderr, "Unrecognized audio command: %s\n", tmpstr);
         }
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
            printf("Going to enable or disable %s.\n", tmpstr);

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
         if (!strcmp(armor_element->mqtt_device, tmpstr)) {
            //printf("Found the item: %s : %s\n", armo_element->mqtt_device, tmpstr);
            break;
         }
         armor_element = armor_element->next;
      }

      if (armor_element != NULL) {
         json_object_object_get_ex(parsed_json, "temp", &tmpobj);
         if (tmpobj != NULL) {
            armor_element->last_temp = json_object_get_double(tmpobj);
            printf("Setting last_temp = %0.2f on %s.\n", armor_element->last_temp,
                   armor_element->mqtt_device);
         }

         json_object_object_get_ex(parsed_json, "voltage", &tmpobj);
         if (tmpobj != NULL) {
            armor_element->last_voltage = json_object_get_double(tmpobj);
            printf("Setting last_voltage = %0.2f on %s.\n", armor_element->last_voltage,
                   armor_element->mqtt_device);
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

/* It turns out processing this along with the frame rate doesn't cut it.
 * This function processes input from USB/serial.
 *
 * TODO: This was kind of a quick fix. I think I can do better.
 */
void *command_processing_thread(void *arg)
{
   int sfd = 0;                 /* Socket file descriptor for command processing. */
   fd_set set;
   //int sockfd = -1;
   struct timeval timeout;
   char sread_buf[MAX_FILENAME_LENGTH];
   char *usb_port = (char *)arg;
   struct termios SerialPortSettings;

   /* command buffering */
   char command_buffer[MAX_FILENAME_LENGTH];
   int command_length = 0;

   command_buffer[0] = '\0';

   memset(raw_log, '\0', LOG_ROWS * LOG_LINE_LENGTH);

   /* Read from serial input. */
   if (strcmp(usb_port, "") == 0) {
      sfd = fileno(stdin);
   } else {
      /* Open the serial port. */
      sfd = open(usb_port, O_RDWR | O_NOCTTY | O_NDELAY);
      if (sfd == -1) {
         perror("Unable to open serial port: ");

         return NULL;
      } else {
         printf("Serial port opened successfully.\n");
      }

      /* Setup serial port. */
      tcgetattr(sfd, &SerialPortSettings);
      cfsetispeed(&SerialPortSettings, B115200);        /* Set Read  Speed as 115200 */
      cfsetospeed(&SerialPortSettings, B115200);        /* Set Write Speed as 115200 */

      /* 8N1 Mode */
      SerialPortSettings.c_cflag &= ~PARENB;    /* Disables the Parity Enable bit(PARENB),So No Parity */
      SerialPortSettings.c_cflag &= ~CSTOPB;    /* CSTOPB = 2 Stop bits,here it is cleared so 1 Stop bit */
      SerialPortSettings.c_cflag &= ~CSIZE;     /* Clears the mask for setting the data size */
      SerialPortSettings.c_cflag |= CS8;        /* Set the data bits = 8 */

      SerialPortSettings.c_cflag &= ~CRTSCTS;   /* No Hardware flow Control */
      SerialPortSettings.c_cflag |= CREAD | CLOCAL;     /* Enable receiver,Ignore Modem Control lines */

      SerialPortSettings.c_iflag &= ~(IXON | IXOFF | IXANY);    /* Disable XON/XOFF flow control both i/p and o/p */
      SerialPortSettings.c_iflag &= ~(ICANON | ECHO | ECHOE | ISIG);    /* Non Cannonical mode */

      SerialPortSettings.c_oflag &= ~OPOST;     /*No Output Processing */

      /* Setting Time outs */
      SerialPortSettings.c_cc[VMIN] = 10;       /* Read at least 10 characters */
      SerialPortSettings.c_cc[VTIME] = 1;       /* Wait indefinetly   */

      if ((tcsetattr(sfd, TCSANOW, &SerialPortSettings)) != 0) {
         printf("\n  ERROR ! in Setting attributes: ");
      }

      tcflush(sfd, TCIFLUSH);
   }

   while (!checkShutdown()) {
      int retval = 0;
      int max_socket = 0;
      int this_socket = -1;

      /* This should usually be the helmet we're communicating with directly.
       * At this point we should be connected to it. */
      registerArmor("helmet");

      timeout.tv_sec = 1;
      timeout.tv_usec = 0;
      FD_ZERO(&set);
      FD_SET(sfd, &set);
      //FD_SET(sockfd, &set);

      if (sfd > max_socket)
         max_socket = sfd;

      //if (sockfd > max_socket)
      //   max_socket = sockfd;

      retval = select(max_socket + 1, &set, NULL, NULL, &timeout);
      if (retval < 0) {
         printf("Select error.\n");
      } else if (retval == 0) {
         // This print it available for debugging but not necessary.
         //printf("USB/Serial Data Timeout.\n");
      } else {
         if (FD_ISSET(sfd, &set)) {
            this_socket = sfd;
            //} else if (FD_ISSET(sockfd, &set)) {
            //   this_socket = sockfd;
         }
         if (this_socket != -1) {
            retval = read(this_socket, &sread_buf, MAX_FILENAME_LENGTH);
            sread_buf[retval] = '\0';
            //printf("%s", sread_buf);

            //printf( "retval: %d\n", retval );
            for (int j = 0; j < retval; j++) {
               if (sread_buf[j] == '\n') {
                  //command_buffer[command_length] = '\0';
                  log_command(command_buffer);
                  parse_json_command(command_buffer);
                  command_buffer[0] = '\0';
                  command_length = 0;
               } else if (sread_buf[j] == '\r') {
                  // Do nothing.
               } else {
                  command_buffer[command_length++]
                      = sread_buf[j];
               }
            }
            this_socket = -1;
         }
      }
   }

#ifdef DEBUG_SHUTDOWN
   printf("Closing serial input.\n");
#endif
   close(sfd);
#ifdef DEBUG_SHUTDOWN
   printf("Done.\n");
#endif

   return NULL;
}
