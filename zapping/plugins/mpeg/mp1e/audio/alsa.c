/*
 *  MPEG Real Time Encoder
 *  Advanced Linux Sound Architecture Interface (draft)
 *
 *  Copyright (C) 2000 Michael H. Schimek
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

/* $Id: alsa.c,v 1.11 2001-03-31 11:10:26 garetxe Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include "../common/log.h" 
#include "../common/fifo.h"

#ifdef HAVE_LIBASOUND

#include <string.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <asm/types.h>
#include <sys/asoundlib.h>
#include "../common/mmx.h" 
#include "../common/math.h" 
#include "audio.h"

/*
 *  ALSA Library PCM Device
 */

#define BUFFER_SIZE 8192 // bytes per read(), appx.

struct alsa_context {
	struct pcm_context	pcm;

	snd_pcm_t *		handle;
	int			scan_range;
	int			look_ahead;
	int			samples_per_frame;
	double			scan_time;
	short *			p;
	int			left;
	double			time;
};

/* XXX Clock drift & overflow detection missing */

static buffer *
wait_full(fifo *f)
{
	struct alsa_context *alsa = f->user_data;
	buffer *b = f->buffers;

	if (b->data)
		return NULL; // no queue

	if (alsa->left <= 0) {
		ssize_t r, n, m;
		unsigned char *p;

		memcpy(b->allocated, (short *) b->allocated + alsa->scan_range,
			alsa->look_ahead * sizeof(short));

		p = b->allocated + alsa->look_ahead * sizeof(short);
		m = alsa->scan_range * sizeof(short);

		for (n = m; n > 0;) {
			r = snd_pcm_plugin_read(alsa->handle, p, n);

			if (r == -EINTR)
				continue;

			if (r == 0 || r == -EAGAIN) {
				usleep(alsa->scan_time * 800000);
				continue;
			}

			ASSERT("read PCM data, %d bytes", r > 0, n);

			p += r;
			n -= r;

			if (r < 200)
				usleep(alsa->scan_time * 800000);
		}

		if (alsa->time < 0.0) {
			snd_pcm_channel_status_t status;
			int err;

			status.channel = SND_PCM_CHANNEL_CAPTURE;

			if ((err = snd_pcm_plugin_status(alsa->handle, &status)) < 0)
				FAIL("Failed to query ALSA PCM plugin status (%d, %s)",
					err, snd_strerror(err));

			alsa->time = status.stime.tv_sec + status.stime.tv_usec / 1e6;
		} else
			alsa->time += alsa->scan_time;

		alsa->p = (short *) b->allocated;
		alsa->left = alsa->scan_range - alsa->samples_per_frame;

		b->time = alsa->time;
		b->data = b->allocated;

		return b;
	}

	b->time = alsa->time
		+ ((alsa->p - (short *) b->allocated) >> alsa->pcm.stereo)
			/ (double) alsa->pcm.sampling_rate;

	alsa->p += alsa->samples_per_frame;
	alsa->left -= alsa->samples_per_frame;

	b->data = (unsigned char *) alsa->p;

	return b;
}

static void
send_empty(fifo *f, buffer *b)
{
	b->data = NULL;
}

static bool
start(fifo *f)
{
	struct alsa_context *alsa = f->user_data;
	int err;

	if ((err = snd_pcm_plugin_prepare(alsa->handle, SND_PCM_CHANNEL_CAPTURE)) < 0)
		FAIL("Failed to prepare ALSA PCM plugin for recording (%d, %s)",
			err, snd_strerror(err));

	return TRUE;
}

fifo *
open_pcm_alsa(char *dev_name, int sampling_rate, bool stereo)
{
	struct alsa_context *alsa;
	snd_pcm_channel_info_t info;
	snd_pcm_channel_params_t params;
	snd_pcm_channel_setup_t setup;
	int card = 0, device = 0, buffer_size;
	int err;

	while (*dev_name && !isdigit(*dev_name))
		dev_name++;

	sscanf(dev_name, "%d,%d", &card, &device);

	ASSERT("allocate pcm context",
		(alsa = calloc(1, sizeof(struct alsa_context))));

	alsa->pcm.sampling_rate = sampling_rate;
	alsa->pcm.stereo = stereo;

	alsa->samples_per_frame = SAMPLES_PER_FRAME << stereo;
	alsa->scan_range = MAX(BUFFER_SIZE / sizeof(short) 
		/ alsa->samples_per_frame, 1) * alsa->samples_per_frame;
	alsa->look_ahead = (512 - 32) << stereo;
	alsa->scan_time = (alsa->scan_range >> stereo)
			   / (double) sampling_rate;
	alsa->time = -1.0;

	buffer_size = (alsa->scan_range + alsa->look_ahead) * sizeof(short);

	if ((err = snd_pcm_open(&alsa->handle, card, device, SND_PCM_OPEN_CAPTURE)) < 0)
		FAIL("Cannot open ALSA card %d,%d (%d, %s)",
			card, device, err, snd_strerror(err));

	info.channel = SND_PCM_CHANNEL_CAPTURE;

	if ((err = snd_pcm_plugin_info(alsa->handle, &info)) < 0)
		FAIL("Cannot obtain ALSA device info (%d, %s)", err, snd_strerror(err));

	printv(2, "Opened ALSA PCM plugin, card #%d device #%d\n",
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
	params.buf.stream.queue_size = BUFFER_SIZE * 2;
	params.buf.stream.fill = SND_PCM_FILL_NONE;

	if ((err = snd_pcm_plugin_params(alsa->handle, &params)) < 0)
		FAIL("Failed to set ALSA PCM plugin parameters (%d, %s)", err, snd_strerror(err));

	setup.channel = SND_PCM_CHANNEL_CAPTURE;

	if ((err = snd_pcm_plugin_setup(alsa->handle, &setup)) < 0)
		FAIL("Failed to query ALSA PCM plugin parameters (%d, %s)", err, snd_strerror(err));

	printv(3, "ALSA setup: ilv %d, format %d, rate %d, voices %d, queue %d\n",
		setup.format.interleave, setup.format.format,
		setup.format.rate, setup.format.voices,
		setup.buf.stream.queue_size);

	ASSERT("init pcm/alsa capture fifo", init_callback_fifo(
		audio_cap_fifo = &alsa->pcm.fifo, "audio-alsa",
		wait_full, send_empty, NULL, 1, buffer_size));

	alsa->pcm.fifo.start = start;

	alsa->pcm.fifo.buffers[0].data = NULL;
	alsa->pcm.fifo.buffers[0].used =
		(alsa->samples_per_frame + alsa->look_ahead) * sizeof(short);
	alsa->pcm.fifo.user_data = alsa;

	return &alsa->pcm.fifo;
}

#else // !HAVE_LIBASOUND

fifo *
open_pcm_alsa(char *dev_name, int sampling_rate, bool stereo)
{
	FAIL("Not compiled with ALSA interface.\n"
	     "For more info about ALSA visit http://www.alsa-project.org\n");
}

#endif // !HAVE_LIBASOUND
