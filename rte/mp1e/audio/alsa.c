/*
 *  MPEG Real Time Encoder
 *  Advanced Linux Sound Architecture interface
 *
 *  Copyright (C) 2000-2001 Michael H. Schimek
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

/* $Id: alsa.c,v 1.7 2001-09-20 23:35:07 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "../common/log.h" 

#include "audio.h"

#define ASSERT_ALSA(what, func, args...)				\
do {									\
	int _err;							\
									\
	if ((_err = func) < 0) {					\
		fprintf(stderr,	"%s:" __FILE__ ":" ISTF1(__LINE__) ": "	\
			"Failed to " what " (%d, %s)\n",		\
			program_invocation_short_name			\
			 ,##args, _err, snd_strerror(_err));		\
		exit(EXIT_FAILURE);					\
	} else if (0) {							\
		fprintf(stderr,	"%s:" __FILE__ ":" ISTF1(__LINE__) ": "	\
			what " - ok\n", program_invocation_short_name	\
			 ,##args);					\
	}								\
} while (0)

#ifdef HAVE_LIBASOUND

#include <string.h>
#include <ctype.h>

#include <sys/asoundlib.h>

#include "../common/math.h" 

#if !defined(SND_LIB_MAJOR) || (SND_LIB_MAJOR == 0 && SND_LIB_MINOR < 9)

/*
 *  ALSA Library (0.5.10b) PCM Device
 */

struct alsa_context {
	struct pcm_context	pcm;

	snd_pcm_t *		handle;
	int			sleep; /* us */
	double			time, buffer_period;
	double			start;
};

static void
wait_full(fifo *f)
{
	struct alsa_context *alsa = f->user_data;
	buffer *b = PARENT(f->buffers.head, buffer, added);
	unsigned char *p;
	ssize_t r, n;
	double now, dt;

	assert(b->data == NULL); /* no queue */

	for (p = b->allocated, n = b->size; n > 0;) {
		r = snd_pcm_plugin_read(alsa->handle, p, n);

		if (r == -EINTR)
			continue;

		if (r == 0 || r == -EAGAIN) {
			usleep(alsa->sleep);
			continue;
		}

		ASSERT_ALSA("read PCM data, %d bytes", r, n);

		p += r;
		n -= r;

		if (r < 200)
			usleep(alsa->sleep);
	}

	now = current_time();
	dt = now - alsa->time;

	if (alsa->time > 0
	    && fabs(dt - alsa->buffer_period) < alsa->buffer_period * 0.1) {
		b->time = alsa->time + alsa->start;
		alsa->buffer_period = (alsa->buffer_period - dt) * 0.9 + dt;
		alsa->time += alsa->buffer_period;
	} else {
		snd_pcm_channel_status_t status;

		status.channel = SND_PCM_CHANNEL_CAPTURE;

		ASSERT_ALSA("query ALSA PCM plugin status",
			snd_pcm_plugin_status(alsa->handle, &status));

		alsa->start = status.stime.tv_sec
			+ status.stime.tv_usec * (1 / 1e6);

		b->time = alsa->start;
		alsa->start -= now;
		alsa->time = now + alsa->buffer_period;
	}

	b->data = b->allocated;

	send_full_buffer(&alsa->pcm.producer, b);
}

static void
send_empty(consumer *c, buffer *b)
{
	// XXX
	unlink_node(&c->fifo->full, &b->node);

	b->data = NULL;
}

static bool
start(fifo *f)
{
	struct alsa_context *alsa = f->user_data;

	ASSERT_ALSA("start PCM capturing",
		snd_pcm_plugin_prepare(alsa->handle,
				       SND_PCM_CHANNEL_CAPTURE));
	return TRUE;
}

fifo *
open_pcm_alsa(char *dev_name, int sampling_rate, bool stereo)
{
	struct alsa_context *alsa;
	snd_pcm_channel_info_t info;
	snd_pcm_channel_params_t params;
	snd_pcm_channel_setup_t setup;
	int buffer_size = 128 << (10 + !!stereo);
	int card = 0, device = 0;
	buffer *b;

	while (*dev_name && !isdigit(*dev_name))
		dev_name++;

	sscanf(dev_name, "%d,%d", &card, &device);

	ASSERT("allocate pcm context",
		(alsa = calloc(1, sizeof(struct alsa_context))));

	alsa->pcm.sampling_rate = sampling_rate;
	alsa->pcm.stereo = stereo;

	ASSERT_ALSA("open ALSA PCM card %d,%d",
		snd_pcm_open(&alsa->handle, card, device, SND_PCM_OPEN_CAPTURE),
		card, device);

	info.channel = SND_PCM_CHANNEL_CAPTURE;

	ASSERT_ALSA("obtain PCM device info",
		snd_pcm_plugin_info(alsa->handle, &info));

	printv(2, "Opened ALSA 0.5 PCM plugin, card #%d device #%d\n",
		card, device);

	memset(&params, 0, sizeof(params));

	params.channel = SND_PCM_CHANNEL_CAPTURE;
	params.mode = SND_PCM_MODE_STREAM;
	params.format.interleave = stereo;
	params.format.format = SND_PCM_SFMT_S16_LE;
	params.format.rate = sampling_rate;
	params.format.voices = stereo + 1;
	params.start_mode = SND_PCM_START_DATA;
	params.stop_mode = SND_PCM_STOP_ROLLOVER;
	params.time = 1;
	params.buf.stream.queue_size = buffer_size;
	params.buf.stream.fill = SND_PCM_FILL_NONE;

	ASSERT_ALSA("set PCM plugin parameters",
		snd_pcm_plugin_params(alsa->handle, &params));

	setup.channel = SND_PCM_CHANNEL_CAPTURE;

	ASSERT_ALSA("query PCM plugin parameters",
		snd_pcm_plugin_setup(alsa->handle, &setup));

	printv(3, "ALSA setup: ilv %d, format %d, rate %d, voices %d, queue %d\n",
		setup.format.interleave, setup.format.format,
		setup.format.rate, setup.format.voices,
		setup.buf.stream.queue_size);

	buffer_size = 1 << (11 + (sampling_rate > 24000));

	alsa->time = 0.0;
	alsa->start = 0.0;
	alsa->buffer_period = buffer_size / (double) sampling_rate;
	alsa->sleep = lroundn(400000.0 * alsa->buffer_period);

	buffer_size <<= stereo + 1;

	ASSERT("init alsa fifo", init_callback_fifo(
		&alsa->pcm.fifo, "audio-alsa1",
		NULL, NULL, wait_full, send_empty,
		1, buffer_size));

	ASSERT("init alsa producer",
		add_producer(&alsa->pcm.fifo, &alsa->pcm.producer));

	alsa->pcm.fifo.user_data = alsa;
	alsa->pcm.fifo.start = start;

	ASSERT("init alsa fifo", init_callback_fifo(
		&alsa->pcm.fifo, "audio-alsa2",
		NULL, NULL, wait_full, send_empty,
		1, buffer_size));

	ASSERT("init alsa producer",
		add_producer(&alsa->pcm.fifo, &alsa->pcm.producer));

	alsa->pcm.fifo.user_data = alsa;
	alsa->pcm.fifo.start = start;

	b = PARENT(alsa->pcm.fifo.buffers.head, buffer, added);

	b->data = NULL;
	b->used = b->size;
	b->offset = 0;

	return &alsa->pcm.fifo;
}

#else /* SND_LIB >= 0.9 */

/*
 *  ALSA Library (0.9.0beta7) PCM Device
 *
 *  This is untested and time stamping chews on the gums,
 *  expect the worst. Try OSS.
 */

struct alsa_context {
	struct pcm_context	pcm;

	snd_pcm_t *		handle;
	int			sleep;
};

static void
wait_full(fifo *f)
{
	struct alsa_context *alsa = f->user_data;
	buffer *b = PARENT(f->buffers.head, buffer, added);
	unsigned char *p;
	ssize_t r, n;

	assert(b->data == NULL); /* no queue */

	for (p = b->allocated, n = b->size; n > 0;) {
		r = snd_pcm_readi(alsa->handle, p, n);

		if (r == -EINTR)
			continue;

		if (r == 0 || r == -EAGAIN) {
			usleep(alsa->sleep);
			continue;
		}

		ASSERT_ALSA("read PCM data, %d bytes", r, n);

		p += r;
		n -= r;

		if (r < 200)
			usleep(alsa->sleep);
	}

	b->time = current_time(); // XXX
	b->data = b->allocated;

	send_full_buffer(&alsa->pcm.producer, b);
}

static void
send_empty(consumer *c, buffer *b)
{
	// XXX
	unlink_node(&c->fifo->full, &b->node);

	b->data = NULL;
}

static bool
start(fifo *f)
{
	struct alsa_context *alsa = f->user_data;

	ASSERT_ALSA("start PCM capturing",
		snd_pcm_prepare(alsa->handle));

	return TRUE;
}

fifo *
open_pcm_alsa(char *dev_name, int sampling_rate, bool stereo)
{
	struct alsa_context *alsa;
	snd_pcm_info_t *info;
	snd_pcm_hw_params_t *params;
	snd_pcm_sw_params_t *swparams;
	int buffer_time = 500000; /* µs */
	int period_size = 2048; /* frames (channel * bytes_per_sample) */
	int buffer_size, err;
	snd_output_t *log;
	buffer *b;

	snd_pcm_info_alloca(&info);
	snd_pcm_hw_params_alloca(&params);
	snd_pcm_sw_params_alloca(&swparams);

	ASSERT("allocate pcm context",
		(alsa = calloc(1, sizeof(struct alsa_context))));

	alsa->pcm.sampling_rate = sampling_rate;
	alsa->pcm.stereo = stereo;

	if (snd_output_stdio_attach(&log, stderr, 0) < 0)
		log = NULL;

	if (0 == strncasecmp(dev_name, "alsa", 4))
		dev_name += 4;
	while (*dev_name && !isalnum(*dev_name))
		dev_name++;
	if (!*dev_name)
		dev_name = "default";

	ASSERT_ALSA("open ALSA device '%s'",
		snd_pcm_open(&alsa->handle, dev_name, SND_PCM_STREAM_CAPTURE, 0),
		dev_name);
	
	ASSERT_ALSA("obtain snd_pcm_info", snd_pcm_info(alsa->handle, info));

	printv(2, "Opened ALSA 0.9 PCM device '%s' (%s)\n",
		dev_name, snd_pcm_info_get_name(info));

	ASSERT_ALSA("obtain PCM hw configuration",
		snd_pcm_hw_params_any(alsa->handle, params));
	ASSERT_ALSA("set PCM access mode",
		snd_pcm_hw_params_set_access(alsa->handle,
			params, SND_PCM_ACCESS_RW_INTERLEAVED));
	ASSERT_ALSA("set PCM sample format signed 16 bit LE",
		snd_pcm_hw_params_set_format(alsa->handle,
			params, SND_PCM_FORMAT_S16_LE));
	ASSERT_ALSA("set PCM channels to %d",
		snd_pcm_hw_params_set_channels(alsa->handle,
			params, 1 + !!stereo), 1 + !!stereo);
	ASSERT_ALSA("set PCM sampling rate %d Hz",
		snd_pcm_hw_params_set_rate_near(alsa->handle,
			params, sampling_rate, 0), sampling_rate);
	ASSERT_ALSA("set PCM DMA buffer size",
		snd_pcm_hw_params_set_buffer_time_near(alsa->handle,
			params, buffer_time, 0));
	ASSERT_ALSA("set PCM DMA period size",
		snd_pcm_hw_params_set_period_size(alsa->handle,
			params, period_size, 0));
	if ((err = snd_pcm_hw_params(alsa->handle, params)) < 0) {
		fprintf(stderr, "%s:" __FILE__ ":" ISTF1(__LINE__) ": "
			"Failed to install PCM hw parameters (%d, %s)",
			program_invocation_short_name, err, snd_strerror(err));

		if (log) snd_pcm_hw_params_dump(params, log);

		exit(EXIT_FAILURE);
	}

	period_size = snd_pcm_hw_params_get_period_size(params, 0);
	buffer_size = snd_pcm_hw_params_get_buffer_size(params);
	if (period_size == buffer_size) {
		FAIL("Can't use period equal to buffer size %d", period_size);
		exit(EXIT_FAILURE);
	}

	ASSERT_ALSA("obtain PCM sw configuration",
		snd_pcm_sw_params_current(alsa->handle, swparams));
	ASSERT_ALSA("set PCM sw sleep min 0",
		snd_pcm_sw_params_set_sleep_min(alsa->handle, swparams, 0));
	ASSERT_ALSA("set PCM sw avail min %d",
		snd_pcm_sw_params_set_avail_min(alsa->handle, swparams, period_size),
		period_size);
	ASSERT_ALSA("set PCM xfer align %d",
		snd_pcm_sw_params_set_xfer_align(alsa->handle, swparams, period_size),
		period_size);
	if ((err = snd_pcm_sw_params(alsa->handle, swparams)) < 0) {
		fprintf(stderr, "%s:" __FILE__ ":" ISTF1(__LINE__) ": "
			"Failed to install PCM sw parameters (%d, %s)",
			program_invocation_short_name, err, snd_strerror(err));

		if (log) snd_pcm_sw_params_dump(swparams, log);

		exit(EXIT_FAILURE);
	}

	if (verbose >= 3 && log)
		snd_pcm_dump(alsa->handle, log);

	buffer_size = period_size << (stereo + 1);
	alsa->sleep = 400000.0 * period_size / (double) sampling_rate;

	ASSERT("init alsa fifo", init_callback_fifo(
		&alsa->pcm.fifo, "audio-alsa2",
		NULL, NULL, wait_full, send_empty,
		1, buffer_size));

	ASSERT("init alsa producer",
		add_producer(&alsa->pcm.fifo, &alsa->pcm.producer));

	alsa->pcm.fifo.user_data = alsa;
	alsa->pcm.fifo.start = start;

	b = PARENT(alsa->pcm.fifo.buffers.head, buffer, added);

	b->data = NULL;
	b->used = b->size;
	b->offset = 0;

	return &alsa->pcm.fifo;
}

#endif /* SND_LIB >= 0.9 */

#else /* !HAVE_LIBASOUND */

fifo *
open_pcm_alsa(char *dev_name, int sampling_rate, bool stereo)
{
	FAIL("Not compiled with ALSA interface.\n"
	     "For more info about ALSA visit http://www.alsa-project.org\n");
}

#endif /* !HAVE_LIBASOUND */
