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
#include <string.h>
#include <stdlib.h>

#include <vorbis/vorbisfile.h>
#include <alsa/asoundlib.h>

/* POSIX Message Queue */
#include <fcntl.h>
#include <sys/stat.h>

#include "defines.h"
#include "audio.h"
#include "config_manager.h"
#include "logging.h"
#include "mirage.h"

/* This is my old audio threading code. This is a single instance. We start multiple of them.
 * TODO: Read this and see if it's still good.
 */
void *audio_thread(void *arg)
{
   /* PCM output */
   unsigned int pcm, tmp, rate;
   snd_pcm_uframes_t frames;
   snd_pcm_t *pcm_handle = NULL;
   snd_pcm_hw_params_t *params;
   char *buff = NULL;
   int buff_size = 0;
   int err = 0;

   /* Vorbis */
   FILE *input_file = NULL;
   OggVorbis_File vf;
   char **ptr;
   vorbis_info *vi;
   int eof = 0;
   int current_section = 0;
   ogg_int64_t pcm_length;

   thread_info *this_thread = (thread_info *) arg;

   char in_buffer[MSG_BUFFER_SIZE];
   audio_msg *in_data;
   in_data = (audio_msg *) in_buffer;
   int in_size = 0;

   struct timespec queue_timeout = { .tv_sec = 0, .tv_nsec = 0};

   //char out_buffer [MSG_BUFFER_SIZE];
   //audio_msg *out_data;
   //out_data = (audio_msg *)out_buffer;

   while (!checkShutdown()) {
      this_thread->stop = 1;
      //printf("Thread %d waiting on msg...\n", this_thread->thread_id);
      clock_gettime(CLOCK_REALTIME, &queue_timeout);
      queue_timeout.tv_sec++;
      if ((in_size = mq_timedreceive(this_thread->qd_client, in_buffer, MSG_BUFFER_SIZE, NULL, &queue_timeout)) == -1) {
         //perror("Client: mq_receive");
         continue;
      } else if (in_size > 0) {
#ifdef AUDIO_DEBUG
         LOG_INFO("Thread %d Msg received.", this_thread->thread_id);
         LOG_INFO("\tcommand: %d", in_data->command);
         LOG_INFO("\tfilename: \"%s\"", in_data->filename);
         LOG_INFO("\tstart_percent: %f", in_data->start_percent);
#endif
         strcpy(this_thread->filename, in_data->filename);
         this_thread->start_percent = in_data->start_percent;
      } else {
         //printf("Empty msg received.\n");
         continue;
      }
      this_thread->stop = 0;

      /* Process Input */
      input_file = fopen(this_thread->filename, "r");
      if (input_file == NULL) {
         LOG_ERROR("[%d] Unable to open file: %s\n",
                   this_thread->thread_id, this_thread->filename);
         return NULL;
      }

      if (ov_open(input_file, &vf, NULL, 0) < 0) {
         LOG_ERROR("[%d] Input does not appear to be an Ogg bitstream.\n", this_thread->thread_id);
         return NULL;
      }

      /* Setup Output Device */
      /* Open the PCM device in playback mode */
      if ((pcm = snd_pcm_open(&pcm_handle, PCM_DEVICE, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
         printf("[%d] ERROR: Can't open \"%s\" PCM device. %s",
                this_thread->thread_id, PCM_DEVICE, snd_strerror(pcm));
      }

      /* Allocate parameters object and fill it with default values */
      snd_pcm_hw_params_alloca(&params);

      snd_pcm_hw_params_any(pcm_handle, params);

      ptr = ov_comment(&vf, -1)->user_comments;
      vi = ov_info(&vf, -1);
      while (*ptr) {
         ++ptr;
      }
#ifdef AUDIO_DEBUG
      LOG_INFO("[%d] Bitstream is %d channel, %ldHz",
              this_thread->thread_id, vi->channels, vi->rate);
      LOG_INFO("[%d] Decoded length: %ld samples",
              this_thread->thread_id, (long)ov_pcm_total(&vf, -1));
      LOG_INFO("[%d] Encoded by: %s",
              this_thread->thread_id, ov_comment(&vf, -1)->vendor);
#endif
      pcm_length = ov_pcm_total(&vf, -1);

      /* Setup sound output based on input. */
      if ((pcm = snd_pcm_hw_params_set_access(pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED))
          < 0) {
         LOG_ERROR("[%d] ERROR: Can't set interleaved mode. %s",
                this_thread->thread_id, snd_strerror(pcm));
      }

      if ((pcm = snd_pcm_hw_params_set_format(pcm_handle, params, SND_PCM_FORMAT_S16_LE))
          < 0) {
         LOG_ERROR("[%d] ERROR: Can't set format. %s", this_thread->thread_id, snd_strerror(pcm));
      }

      if ((pcm = snd_pcm_hw_params_set_channels(pcm_handle, params, vi->channels)) < 0) {
         LOG_ERROR("[%d] ERROR: Can't set channels number. %s",
                this_thread->thread_id, snd_strerror(pcm));
      }

      rate = vi->rate;
      if ((pcm = snd_pcm_hw_params_set_rate_near(pcm_handle, params, &rate, 0)) < 0) {
         LOG_ERROR("[%d] ERROR: Can't set rate. %s", this_thread->thread_id, snd_strerror(pcm));
      }

      /* Write parameters */
      if ((pcm = snd_pcm_hw_params(pcm_handle, params)) < 0) {
         LOG_ERROR("[%d] ERROR: Can't set harware parameters. %s",
                this_thread->thread_id, snd_strerror(pcm));
      }

      /* Resume information */
#ifdef AUDIO_DEBUG
      LOG_INFO("[%d] PCM name: '%s'", this_thread->thread_id, snd_pcm_name(pcm_handle));

      LOG_INFO("[%d] PCM state: %s", this_thread->thread_id,
             snd_pcm_state_name(snd_pcm_state(pcm_handle)));
#endif
      snd_pcm_hw_params_get_rate(params, &tmp, 0);
#ifdef AUDIO_DEBUG
      LOG_INFO("[%d] rate: %d bps", this_thread->thread_id, tmp);
#endif

      /* Allocate buffer to hold single period */
      snd_pcm_hw_params_get_period_size(params, &frames, 0);

      buff_size = frames * vi->channels * 2 /* 2 -> sample size */ ;
      buff = (char *)malloc(buff_size);

      snd_pcm_hw_params_get_period_time(params, &tmp, NULL);

#ifdef AUDIO_DEBUG
      LOG_INFO("[%d] Buffer size: %d", this_thread->thread_id, buff_size);
#endif

      if ((err = snd_pcm_prepare(pcm_handle)) < 0) {
         LOG_ERROR("[%d] Cannot prepare audio interface for use (%s)",
                 this_thread->thread_id, snd_strerror(err));
         exit(1);
      }

      if (this_thread->start_percent > 0) {
         int ret = 0;
#ifdef AUDIO_DEBUG
         LOG_INFO("Seeking to %ld", (long)((long double)pcm_length * this_thread->start_percent));
#endif
         ret = ov_pcm_seek(&vf, (long)((long double)pcm_length * this_thread->start_percent));
         if (ret != 0) {
            LOG_ERROR("Seek failed.");
         }
      }

      while (!eof) {
         long ret = ov_read(&vf, buff, buff_size, 0, 2, 1,
                            &current_section);
         if (this_thread->stop) {
#ifdef AUDIO_DEBUG
            LOG_INFO("Thread %d received stop.", this_thread->thread_id);
#endif
            break;
         }
         if (ret == 0) {
            /* EOF */
            eof = 1;
         } else if (ret < 0) {
            /* error in the stream.  Not a problem, just reporting it in
               case we (the app) cares.  In this case, we don't. */
            LOG_ERROR("[%d] Error reading OV file.", this_thread->thread_id);
         } else {
            /* we don't bother dealing with sample rate changes, etc, but
               you'll have to */
            if ((pcm = snd_pcm_writei(pcm_handle, buff, ret / vi->channels / 2)) == -EPIPE) {
               snd_pcm_prepare(pcm_handle);
            } else if (pcm < 0) {
               LOG_ERROR("[%d] ERROR. Can't write to PCM device. %s",
                    this_thread->thread_id, snd_strerror(pcm));
            }
         }
      }

      eof = 0;
      current_section = 0;
      free(buff);
      buff = NULL;
      ov_clear(&vf);
#ifdef AUDIO_DEBUG
      LOG_INFO("[%d] PCM state: %s", this_thread->thread_id,
             snd_pcm_state_name(snd_pcm_state(pcm_handle)));
#endif
      snd_pcm_drain(pcm_handle);
#ifdef AUDIO_DEBUG
      LOG_INFO("[%d] PCM state: %s", this_thread->thread_id,
             snd_pcm_state_name(snd_pcm_state(pcm_handle)));
#endif
      snd_pcm_close(pcm_handle);
      pcm_handle = NULL;

      this_thread->filename[0] = '\0';
   }

   return NULL;
}

int process_audio_command(int command, char *file, double start_percent)
{
   int current_thread = 0;

   /* Audio Command Parsing */
   char out_buffer[MSG_BUFFER_SIZE];
   audio_msg *out_data;
   out_data = (audio_msg *) out_buffer;

   switch (command) {
      case SOUND_PLAY:
         /* Find open thread. */
         while (current_thread < NUM_AUDIO_THREADS)
         {
            if (audio_threads[++current_thread].stop == 1)
            {
               break;
            }

            current_thread++;
         }

         if (current_thread < NUM_AUDIO_THREADS)
         {
            out_data->command = SOUND_PLAY;
            snprintf(out_data->filename, MAX_FILENAME_LENGTH, "%s%s", get_sound_path(), file);
            out_data->start_percent = 0;
            LOG_INFO("Submitting sound to thread %d", current_thread);
            if (mq_send(qd_clients[current_thread], out_buffer, sizeof(audio_msg), 0) == -1) {
               perror("Client: Not able to send message to server");
            }
         } else {
            LOG_ERROR("No audio threads available.");
         }
         break;
      case SOUND_STOP:
         for (int i = 0; i < NUM_AUDIO_THREADS; i++) {
            if (strncmp(audio_threads[i].filename, file, MAX_FILENAME_LENGTH) == 0) {
               LOG_INFO("Stopping thread %d.", i);
               audio_threads[i].stop = 1;
               //pthread_join( thread_handles[i], NULL );
               LOG_INFO("Thread [%d] stop.", i);
               break;
            }
         }
         break;
      default:
         break;
   }

   return SUCCESS;
}


