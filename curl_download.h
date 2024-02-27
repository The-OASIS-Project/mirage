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

