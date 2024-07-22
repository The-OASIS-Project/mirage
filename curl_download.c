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
#include <unistd.h>
#include <stdlib.h>

#include <curl/curl.h>

#include "SDL2/SDL.h"
#include "SDL2/SDL_image.h"

#include "curl_download.h"
#include "logging.h"
#include "mirage.h"

/* CURL write function to save downloads to memory. */
static size_t write_data(void *data, size_t size, size_t nmemb, void *userp)
{
   size_t realsize = size * nmemb;
   struct curl_data *mem = (struct curl_data *)userp;

   char *ptr = realloc(mem->data, mem->size + realsize + 1);
   if (ptr == NULL) {
      LOG_ERROR("Error allocating memory in curl callback.");
      return 0;
   }
   //printf("Downloading %lu bytes of data.\n", realsize);

   mem->data = ptr;
   memcpy(&(mem->data[mem->size]), data, realsize);
   mem->size += realsize;
   mem->data[mem->size] = '\0';

   return realsize;
}

/* This will re-download an image from a given URL at a defined interval if the URL has changed.
 * Currently used for map downloading.
 */
void *image_download_thread(void *arg)
{
   struct curl_data *this_data = (struct curl_data *)arg;
   CURL *curl = NULL;
   CURLcode ret = 0;

   SDL_RWops *rwops = NULL;

   char *last_url = NULL;

   while (!checkShutdown()) {
      if ((last_url == NULL) || (strcmp(this_data->url, last_url) != 0)) {
         // In general we don't want to display this since it has our API key.
         // Left for debug.
         //printf("Downloading image from %s\n", this_data->url);
         curl = curl_easy_init();
         if (curl) {
            curl_easy_setopt(curl, CURLOPT_URL, this_data->url);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, this_data);

            ret = curl_easy_perform(curl);
            if (ret != CURLE_OK) {
               LOG_ERROR("curl_easy_perform() failed: %s, url: \"%s\"", curl_easy_strerror(ret), this_data->url);
            }

            curl_easy_cleanup(curl);
         }

         if (this_data->image != NULL) {
            SDL_FreeSurface(this_data->image);
            this_data->image = NULL;
         }

         rwops = SDL_RWFromMem(this_data->data, this_data->size);
         if (rwops != NULL) {
            this_data->image = IMG_LoadPNG_RW(rwops);
            if (this_data->image == NULL) {
               LOG_ERROR("Unable to convert download to image.");
            } else {
               this_data->updated = 1;
            }

            SDL_RWclose(rwops);
         } else {
            LOG_ERROR("Unable to create SDL_RWFromMem().");
         }

         free(this_data->data);
         this_data->data = NULL;
         this_data->size = 0;

         if (last_url != NULL) {
            free(last_url);
         }

         last_url = strdup(this_data->url);
      }

      if (this_data->updated) {
         sleep(this_data->update_interval_sec);
      }
   }

   if (last_url != NULL) {
      free(last_url);
   }

   return NULL;
}


