/*
 *  Test program for RTE (real time encoder)
 *
 *  Copyright (C) 2000 Iñaki García Etxebarria
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <string.h>
#include <getopt.h>
#include <math.h>
#include <signal.h>
#include <time.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <asm/types.h>
#include "videodev2.h"
#include "audio/mpeg.h"
#include "video/mpeg.h"
#include "video/video.h"
#include "audio/audio.h"
#include "systems/systems.h"
#include "common/profile.h"
#include "common/math.h"
#include "common/log.h"
#include "common/mmx.h"
#include "common/bstream.h"
#include "options.h"
#include "rtepriv.h"
#include "main.h"

char *			my_name;
int			verbose;

double			video_stop_time = 1e30;
double			audio_stop_time = 1e30;

pthread_t		audio_thread_id;
fifo *			audio_cap_fifo;
int			stereo;

pthread_t		video_thread_id;
fifo *			video_cap_fifo;
void			(* video_start)(void);
int			min_cap_buffers;

pthread_t               output_thread_id;

pthread_t		tk_main_id;
extern void *		tk_main(void *);

extern int		psycho_loops;
extern int		audio_num_frames;
extern int		video_num_frames;

extern void options(int ac, char **av);

extern void preview_init(void);

extern void audio_init(void);
extern void video_init(void);

volatile int program_shutdown = 0;

pthread_t video_emulation_thread_id;
pthread_t audio_emulation_thread_id;
pthread_mutex_t video_device_mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t audio_device_mutex=PTHREAD_MUTEX_INITIALIZER;

fifo *			ye_olde_audio_cap_fifo;
fifo *			ye_olde_video_cap_fifo;

void emulation_data_callback(void * data, double * time, int video,
			     rte_context * context, void * user_data)
{
	int frame;
	void * misc_data;
	buffer *b;

	if (video) {
		pthread_mutex_lock(&video_device_mutex);
		b = wait_full_buffer(ye_olde_video_cap_fifo);
		misc_data = b->data;
		*time = b->time;
		pthread_mutex_unlock(&video_device_mutex);
		memcpy(data, misc_data, context->video_bytes);
		send_empty_buffer(ye_olde_video_cap_fifo, b);
	}
	else {
		pthread_mutex_lock(&audio_device_mutex);
		b = wait_full_buffer(ye_olde_audio_cap_fifo);
		misc_data = b->data;
		*time = b->time;
		pthread_mutex_unlock(&audio_device_mutex);
		memcpy(data, misc_data, context->audio_bytes);
		send_empty_buffer(ye_olde_audio_cap_fifo, b);
	}
}

void * video_emulation_thread (void * ptr)
{
	int frame;
	double timestamp;
	unsigned char * data;
	void * video_data;
	rte_context * context = (rte_context *)ptr;
	buffer *b;

	data = rte_push_video_data(context, NULL, 0);
	for (;data;) {
		pthread_mutex_lock(&video_device_mutex);
		b = wait_full_buffer(ye_olde_video_cap_fifo);
		video_data = b->data;
		timestamp = b->time;
		pthread_mutex_unlock(&video_device_mutex);
		memcpy(data, video_data, context->video_bytes);
		data = rte_push_video_data(context, data, timestamp);
		send_empty_buffer(ye_olde_video_cap_fifo, b);
	}
	fprintf(stderr, "video emulation: %s\n", context->error);

	return NULL;
}

void * audio_emulation_thread (void * ptr)
{
	double timestamp;
	short * data;
	short * audio_data;
	rte_context * context = (rte_context *)ptr;
	buffer *b;

	data = rte_push_audio_data(context, NULL, 0);
	for (;data;) {
		pthread_mutex_lock(&audio_device_mutex);
		b = wait_full_buffer(ye_olde_audio_cap_fifo);
		audio_data = (short *) b->data;
		timestamp = b->time;
		pthread_mutex_unlock(&audio_device_mutex);
		memcpy(data, audio_data, context->audio_bytes);
		data = rte_push_audio_data(context, data, timestamp);
		send_empty_buffer(ye_olde_audio_cap_fifo, b);
	}
	fprintf(stderr, "audio emulation: %s\n", context->error);
	return NULL;
}

static	rte_context * context;

/*
  This is just for a preliminary testing, loads of things need to be
  done before this gets really functional.
  push + callbacks don't work together yet, but separately they
  do. Does anybody really want to use them together?
  The push interface works great, not much more CPU usage and nearly
  no lost frames; the callbacks one needs a bit more CPU, and drops
  some more frames, but works fine too.
*/
int emulation_thread_init ( void )
{
	int do_test = 1; /* 1 == push, 2 == callbacks, 3 == both */
	rteDataCallback callback;
	enum rte_pixformat format;

	ye_olde_audio_cap_fifo = audio_cap_fifo;
	ye_olde_video_cap_fifo = video_cap_fifo;

	if (!rte_init())
		return 0;

	if (do_test & 2)
		callback = RTE_DATA_CALLBACK(emulation_data_callback);
	else
		callback = NULL;

	context = rte_context_new("temp.mpeg", 352, 288, RTE_RATE_3,
				  NULL, callback, (void*)0xdeadbeef);

	if (!context) {
		fprintf(stderr, "%s\n", context->error);
		return 0;
	}

	rte_set_video_parameters(context, RTE_YUV420, context->width,
				 context->height, context->video_rate,
				 context->output_video_bits);

	printv(3, "\nrte_start started its work.\n");
	if (!rte_start(context))
	{
		fprintf(stderr, "%s\n", context->error);
		rte_context_destroy(context);
		return 0;
	}
	printv(3, "\nrte_start finished its work.\n");

	if (do_test & 1) {
		pthread_create(&video_emulation_thread_id, NULL,
			       video_emulation_thread, context);
		pthread_create(&audio_emulation_thread_id, NULL,
			       audio_emulation_thread, context);
	}

	return 1;
}

int
main(int ac, char **av)
{
	sigset_t block_mask;
	struct timeval tv;
	pthread_t mux_thread; /* Multiplexer thread */

	my_name = av[0];
	options(ac, av);

	mix_init();
	pcm_init();

	v4l2_init();
	video_start();

	printv(3, "\nstarting emulation... ");
	if (!emulation_thread_init())
		return 0;
	printv(3, "done (closing).\n");

	sleep(5);

	program_shutdown = 1;

	if (modules & 1) {
		printv(3, "\nvideo thread... ");
		pthread_cancel(video_thread_id);
		pthread_join(video_thread_id, NULL);
		printv(3, "done\n");
	}

	if (modules & 2) {
		printv(3, "\naudio thread... ");
		pthread_cancel(audio_thread_id);
		pthread_join(audio_thread_id, NULL);
		printv(3, "done\n");
	}

	printv(3, "\noutput thread... ");

	rte_context_destroy(context);

	output_end();
	printv(3, "done\n");

	printv(2, "\nCleanup done, bye...\n");

	return EXIT_SUCCESS;
}
