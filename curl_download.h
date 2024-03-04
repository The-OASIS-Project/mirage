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

#ifndef CURL_DOWNLOAD_H
#define CURL_DOWNLOAD_H

#include "defines.h"

/* Curl Image Download Data */
struct curl_data {
   char url[512];
   int update_interval_sec;
   int updated;
   SDL_Surface *image;

   /* curl download area */
   size_t size;
   char *data;
};

void *image_download_thread(void *arg);

#endif // CURL_DOWNLOAD_H

