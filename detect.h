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


#ifdef __cplusplus
extern "C" {
#endif

typedef struct _detect {
   int active;
   char description[256];
   double confidence;
   double left;
   double top;
   double width;
   double height;
} detect;

typedef struct _detect_net {
   void *detectNet_net;
   void* detections;

   void *d_image;
   int l_width;
   int l_height;

   sem_t *v_mutex;
} detect_net;

int init_detect(detect_net *new_detect, int argc, char **argv, int width, int height);
int detect_image(detect_net *new_detect, void *image, detect *my_detects, int max_detections);
void free_detect(detect_net *new_detect);

#ifdef __cplusplus
}
#endif
