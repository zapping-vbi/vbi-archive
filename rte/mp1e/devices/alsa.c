/*
 *  MPEG Real Time Encoder
 *  Advanced Linux Sound Architecture interface
 *
 *  Copyright (C) 2000-2003 Michael H. Schimek
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
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

/* $Id: alsa.c,v 1.7 2003-01-03 05:33:45 mschimek Exp $ */

#include "../common/log.h" 
#include "../audio/audio.h"

#ifdef HAVE_ALSA

#include <string.h>
#include <ctype.h>

#define ALSA_PCM_NEW_HW_PARAMS_API 1
#define ALSA_PCM_NEW_SW_PARAMS_API 1

#include <alsa/asoundlib.h>

#include "../common/math.h" 

#define ASSERT_ALSA(what, func, args...)				\
do {									\
	int _err = func;						\
									\
	if (_err < 0) {							\
		fprintf (stderr, "%s:" __FILE__ ":" ISTF1(__LINE__)	\
			 ": Failed to " what " (%d, %s)\n",		\
			 program_invocation_short_name			\
			 ,##args, _err, snd_strerror (_err));		\
		exit (EXIT_FAILURE);					\
	} else if (verbose >= 3) {					\
		fprintf (stderr, "%s:" __FILE__ ":" ISTF1(__LINE__)	\
			 ": " what " - ok\n",				\
			 program_invocation_short_name			\
			 ,##args);					\
	}								\
} while (0)

#define ALSA_DROP_TEST(x) /* x */
#define ALSA_TIME_LOG(x) /* x */

typedef struct alsa_context alsa_context;

struct alsa_context {
	struct pcm_context	pcm;

	snd_pcm_t *		handle;
	snd_pcm_status_t *	status;

	snd_pcm_sframes_t	period_size;
	snd_pcm_uframes_t	offset;
	snd_pcm_uframes_t	frames;

	unsigned int		chunk_bytes;
	unsigned int		frame_bytes;
	double			chunk_time;
	double			frame_time;

	unsigned int		left;

	int			first;
};

extern int test_mode;

static void
recover				(alsa_context *		alsa,
				 int			err)
{
	switch (err) {
	case -EPIPE:
		if (0 && snd_pcm_status (alsa->handle, alsa->status) == 0) {
			struct timeval now, diff, tstamp;

			gettimeofday (&now, 0);
			snd_pcm_status_get_trigger_tstamp (alsa->status, &tstamp);
			timersub (&now, &tstamp, &diff);
			fprintf (stderr, "%.3f ms overrun\n",
				 diff.tv_sec * 1000 + diff.tv_usec / 1000.0);
		}

		ASSERT_ALSA ("recover from overrun",
			     snd_pcm_prepare (alsa->handle));
		break;

	case -ESTRPIPE: /* XXX Suspended (power management) */
	        while ((err = snd_pcm_resume (alsa->handle)) == -EAGAIN);
			sleep (1);

		ASSERT_ALSA ("recover from suspend",
			     snd_pcm_prepare (alsa->handle));
		break;

	default:
		FAIL ("Unexpected ALSA error %d\n", err);
	}
}

static void
wait_full			(fifo *			f)
{
	struct alsa_context *alsa = f->user_data;
	buffer *b = PARENT (f->buffers.head, buffer, added);
	int err;

	if (alsa->left > 0) {
		b->data += alsa->chunk_bytes;
		b->used  = alsa->chunk_bytes;

		b->time += alsa->chunk_time;

		send_full_buffer (&alsa->pcm.producer, b);

		alsa->left -= alsa->chunk_bytes;

		return;
	}

	for (;;) {
		snd_pcm_sframes_t avail;

		switch (snd_pcm_state (alsa->handle)) {
		case SND_PCM_STATE_XRUN:
			recover (alsa, -EPIPE);
			alsa->first = 1;
			continue;

		case SND_PCM_STATE_SUSPENDED:
			recover (alsa, -ESTRPIPE);
			alsa->first = 1;
			continue;

		default:
			break;
		}

		avail = snd_pcm_avail_update (alsa->handle);

		if (avail < 0) {
			recover (alsa, avail);
			alsa->first = 1;
			continue;
		}

		if (avail < alsa->period_size) {
			if (alsa->first) {
				alsa->first = 0;
			
				ASSERT_ALSA ("start alsa capturing",
					     snd_pcm_start (alsa->handle));
			} else {
				err = snd_pcm_wait (alsa->handle, 2000 /* ms */);

				if (err == 0) {
					FAIL ("ALSA timeout\n");
				} else if (err < 0) {
					recover (alsa, err);
					alsa->first = 1;
				}
			}

			continue;
		}

		{
			const snd_pcm_channel_area_t *my_areas;

			alsa->frames = alsa->period_size;

			err = snd_pcm_mmap_begin (alsa->handle, &my_areas,
						  &alsa->offset, &alsa->frames);
			if (err < 0) {
				recover (alsa, err);
				alsa->first = 1;
				continue;
			}
	
			b->data = ((char *) my_areas[0].addr) + (my_areas[0].first >> 3)
				+ alsa->offset * alsa->frame_bytes;
	
			b->used = alsa->chunk_bytes;

			alsa->left = alsa->frames * alsa->frame_bytes - alsa->chunk_bytes;
		}

		{
			snd_timestamp_t tstamp; /* struct timeval */
			snd_pcm_sframes_t delay;
	
			snd_pcm_status (alsa->handle, alsa->status);
	
			/* Silly. */
			snd_pcm_status_get_tstamp (alsa->status, &tstamp);
			delay = snd_pcm_status_get_delay (alsa->status);
	
			b->time = tstamp.tv_sec + tstamp.tv_usec * (1 / 1e6)
				  + delay * alsa->frame_time;
		}

		break;
	}
		
	send_full_buffer (&alsa->pcm.producer, b);
}

static void
send_empty			(consumer *		c,
				 buffer *		b)
{
	struct alsa_context *alsa = b->fifo->user_data;

	// XXX
	unlink_node(&c->fifo->full, &b->node);

	if (alsa->left == 0) {
		snd_pcm_sframes_t actual;

		actual = snd_pcm_mmap_commit (alsa->handle,
				              alsa->offset, alsa->frames);

		if (actual < 0 || actual != alsa->frames) {
			recover (alsa, actual);
			alsa->first = 1;
		}

		b->data = NULL;
	}
}

static rte_bool
start				(fifo *			f)
{
	struct alsa_context *alsa = f->user_data;

	ASSERT_ALSA ("start PCM capturing",
		     snd_pcm_prepare (alsa->handle));

	return TRUE;
}

static inline void
select_sample_format		(alsa_context *		alsa,
				 snd_pcm_hw_params_t *	params,
				 rte_bool		stereo)
{
	static const int
	preference [][3] = {
		{ SND_PCM_FORMAT_S16_LE, RTE_SNDFMT_S16_LE, 2 },
		{ SND_PCM_FORMAT_U16_LE, RTE_SNDFMT_U16_LE, 2 },
		{ SND_PCM_FORMAT_U8,     RTE_SNDFMT_U8,     1 },
		{ SND_PCM_FORMAT_S8,     RTE_SNDFMT_S8,     1 }
	};
	static const int num_formats = 4; 
	unsigned int channels;
	unsigned int i;

	for (i = 0; i < num_formats; i++)
		if (snd_pcm_hw_params_set_format
	            (alsa->handle, params, preference[i][0]) == 0)
			break;

	ASSERT ("set suitable PCM sample format",
	        i < num_formats);

	alsa->pcm.format = preference[i][1];

	stereo = !!stereo;

	ASSERT_ALSA ("set PCM number of channels to %d",
		     snd_pcm_hw_params_set_channels
		     (alsa->handle, params, 1 + stereo), 1 + stereo);

	channels = snd_pcm_hw_params_get_channels (params);

	if (channels != (1 + stereo))
		FAIL ("Unable to set PCM number of channels to %d\n", 1 + stereo);

	alsa->frame_bytes = preference[i][2] << stereo;
}

void
open_pcm_alsa			(const char *		dev_name,
				 unsigned int		sampling_rate,
				 rte_bool		stereo,
				 fifo **		f)
{
	alsa_context *alsa;
	snd_pcm_info_t *info;
	snd_pcm_hw_params_t *params;
	snd_pcm_sw_params_t *swparams;
	unsigned int buffer_size;
	snd_output_t *logs;
	int err;
	buffer *b;

	snd_pcm_info_alloca (&info);
	snd_pcm_hw_params_alloca (&params);
	snd_pcm_sw_params_alloca (&swparams);

	ASSERT ("allocate pcm context",
	    	(alsa = calloc (1, sizeof (*alsa))));

	alsa->pcm.format = RTE_SNDFMT_S16_LE;
	alsa->pcm.sampling_rate = sampling_rate;
	alsa->pcm.stereo = stereo;

	if (snd_output_stdio_attach (&logs, stderr, 0) < 0)
		logs = NULL;

	{
		/* Just an arbitrary prefix selecting this interface */
		if (0 == strncasecmp (dev_name, "alsa", 4))
			dev_name += 4;

		/* Separator */
		while (*dev_name && !isalnum (*dev_name))
			dev_name++;

		if (!*dev_name)
			dev_name = "default";
	}

	ASSERT_ALSA ("snd_pcm_status_malloc",
		     snd_pcm_status_malloc (&alsa->status));

	ASSERT_ALSA ("open ALSA device '%s'",
		     snd_pcm_open (&alsa->handle, dev_name,
			           SND_PCM_STREAM_CAPTURE,
				   SND_PCM_NONBLOCK),
		     dev_name);

	ASSERT_ALSA ("obtain snd_pcm_info",
		     snd_pcm_info (alsa->handle, info));

	printv (2, "Opened ALSA 0.9 PCM device '%s' (%s)\n",
	        dev_name, snd_pcm_info_get_name (info));

	ASSERT_ALSA ("obtain PCM hw configuration",
		     snd_pcm_hw_params_any (alsa->handle, params));

	ASSERT_ALSA ("set PCM access mode",
		     snd_pcm_hw_params_set_access
		     (alsa->handle, params, SND_PCM_ACCESS_MMAP_INTERLEAVED));

	select_sample_format (alsa, params, stereo);

	ASSERT_ALSA ("set PCM sampling rate %d Hz",
		     snd_pcm_hw_params_set_rate_near
		     (alsa->handle, params, sampling_rate, NULL), sampling_rate);

	snd_pcm_hw_params_get_rate (params, &sampling_rate);

	alsa->pcm.sampling_rate = sampling_rate;

	alsa->frame_time = 1.0 / sampling_rate;

#if 0
/* Why can't I get period_size < buffer_time/2 ? */

	buffer_time = 500000; /* us */

	ASSERT_ALSA ("set PCM DMA buffer size %f s",
		     snd_pcm_hw_params_set_buffer_time_near
		     (alsa->handle, params, buffer_time, NULL), buffer_time / 1e6);
#endif
	alsa->period_size = 8192; /* frames (= channels * bytes_per_sample) / interrupt */

	ASSERT_ALSA ("set PCM DMA period size",
		     snd_pcm_hw_params_set_period_size_near
		     (alsa->handle, params, alsa->period_size, 0));

	err = snd_pcm_hw_params (alsa->handle, params);

	if (err < 0) {
		fprintf (stderr, "%s:" __FILE__ ":" ISTF1(__LINE__) ": "
			 "Failed to install PCM hw parameters (%d, %s)",
			 program_invocation_short_name,
			 err, snd_strerror (err));

		if (logs)
			snd_pcm_hw_params_dump (params, logs);

		exit (EXIT_FAILURE);
	}

	/* FIXME this must determine the minimum number of coded video and
	   audio buffers to prevent overflow in the mux */
	alsa->period_size = snd_pcm_hw_params_get_period_size (params, 0);
	buffer_size = snd_pcm_hw_params_get_buffer_size (params);

	if (buffer_size < (alsa->period_size * 2))
		FAIL ("Bad buffer_size/period_size: %d/%d", buffer_size, (int) alsa->period_size);

	for (alsa->chunk_bytes = alsa->period_size;
	     alsa->chunk_bytes >= 4096;
	     alsa->chunk_bytes >>= 1)
	         ;

	alsa->chunk_bytes *= alsa->frame_bytes;
	alsa->chunk_time = alsa->chunk_bytes * alsa->frame_time / alsa->frame_bytes;

	printv (2, "Bounds buffer = %u fr; period = %u fr; chunk = %u B, %f s\n",
		buffer_size, (int) alsa->period_size,
		alsa->chunk_bytes, alsa->chunk_time);

	ASSERT_ALSA ("obtain PCM sw configuration",
		     snd_pcm_sw_params_current (alsa->handle, swparams));

	ASSERT_ALSA ("set PCM sw sleep min 0",
		     snd_pcm_sw_params_set_sleep_min
		     (alsa->handle, swparams, 0));

	ASSERT_ALSA ("set PCM sw avail min %d",
		     snd_pcm_sw_params_set_avail_min
		     (alsa->handle, swparams, alsa->period_size), (int) alsa->period_size);

	ASSERT_ALSA ("set PCM xfer align %d",
		     snd_pcm_sw_params_set_xfer_align
		     (alsa->handle, swparams, alsa->period_size), (int) alsa->period_size);

	ASSERT_ALSA ("set PCM tstamp mode",
		     snd_pcm_sw_params_set_tstamp_mode
		     (alsa->handle, swparams, SND_PCM_TSTAMP_MMAP));

	err = snd_pcm_sw_params (alsa->handle, swparams);

	if (err < 0) {
		fprintf (stderr, "%s:" __FILE__ ":" ISTF1(__LINE__) ": "
			 "Failed to install PCM sw parameters (%d, %s)",
			 program_invocation_short_name,
			 err, snd_strerror (err));

		if (logs)
			snd_pcm_sw_params_dump (swparams, logs);

		exit (EXIT_FAILURE);
	}

	alsa->first = 1;

	if (verbose >= 3 && logs)
		snd_pcm_dump (alsa->handle, logs);

	*f = &alsa->pcm.fifo;

	ASSERT ("init alsa fifo",
	        init_callback_fifo (*f, "audio-alsa",
				    NULL, NULL, wait_full, send_empty,
				    1, buffer_size));

	ASSERT ("init alsa producer",
		add_producer (*f, &alsa->pcm.producer));

	(*f)->user_data = alsa;
	(*f)->start = start;

	b = PARENT ((*f)->buffers.head, buffer, added);

	b->data = NULL;
	b->used = b->size;
	b->offset = 0;
}

#else /* !HAVE_ALSA */

void
open_pcm_alsa			(const char *		dev_name,
			         unsigned int		sampling_rate,
				 rte_bool		stereo,
				 fifo **		f)
{
	FAIL ("Not compiled with ALSA interface.\n"
	      "For more info about ALSA visit http://www.alsa-project.org\n");
}

#endif /* !HAVE_ALSA */
