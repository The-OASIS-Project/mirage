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

#ifndef AUDIO_H
#define AUDIO_H

#include <mqueue.h>

/* Audio playback commands. */
/* TODO: Replace text strings with JSON. */
enum commands { SOUND_PLAY, SOUND_STOP };

/* Commands
 * play [string filename]
 *    returns id
 *
 * play [string filename] [string time start/percent start]
 *    returns id
 *
 * stop [id]
 *    returns 0/1
 *
 * non
 *    something to set the command type to when no command is set?
 *
 * quit
 *    remote shutdown command
 */

/* Audio thread type and local threads. */
typedef struct _thread_info {
   int thread_id;
   char filename[MAX_FILENAME_LENGTH];
   double start_percent;
   int stop;

   char client_queue_name[MAX_FILENAME_LENGTH];
   mqd_t qd_server, qd_client;
} thread_info;
thread_info audio_threads[NUM_AUDIO_THREADS];
mqd_t qd_server, qd_clients[NUM_AUDIO_THREADS];

/* MQUEUE messages for audio commands handed to threads. */
typedef struct _audio_msg {
   int command;
   char filename[MAX_FILENAME_LENGTH];
   double start_percent;
} audio_msg;


void *audio_thread(void *arg);
int process_audio_command(int command, char *file, double start_percent);

#endif // AUDIO_H

