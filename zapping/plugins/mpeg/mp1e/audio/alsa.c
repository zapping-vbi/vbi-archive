/*
 *  MPEG Real Time Encoder
 *  ALSA Interface (draft)
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

/* $Id: alsa.c,v 1.3 2000-10-22 05:24:50 mschimek Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <asm/types.h>

#include "../common/log.h" 
#include "../common/mmx.h" 
#include "../common/math.h" 
#include "audio.h"
#include "mpeg.h"

#ifdef HAVE_LIBASOUND

#include <sys/asoundlib.h>

/*
 *  PCM Device, ALSA library
 */

#define BUFFER_SIZE 8192 // bytes per read(), appx.

static snd_pcm_t *	handle;
static short *		abuffer;
static int		scan_range;
static int		look_ahead;
static int		samples_per_frame;
static double		frame_time;
static short *		ubuffer;
static buffer		buf;
static fifo		pcm_fifo;

extern char *		pcm_dev;
extern int		sampling_rate;
extern int		stereo;

/*
 *  Read window: samples_per_frame (1152 * channels) + look_ahead (480 * channels);
 *  Subband window size 512 samples, step width 32 samples (32 * 3 * 12 total)
 *
 *  Clock drift not detected (how?)
 */

// XXX only one at a time, not checked
static buffer *
alsa_pcm_wait_full(fifo *f)
{
	static double rtime = -1.0, utime;
	static int left = 0;
	static short *p;

	if (ubuffer) {
		p = ubuffer;
		ubuffer = NULL;
		buf.time = utime;
		buf.data = (unsigned char *) p;
		return &buf;
	}

	if (left <= 0)
	{
		ssize_t r;
		int n, m;

		memcpy(abuffer, abuffer + scan_range, look_ahead * sizeof(abuffer[0]));

		p = abuffer + look_ahead;
		m = scan_range * sizeof(abuffer[0]);

		for (n = m; n > 0;) {
			r = snd_pcm_plugin_read(handle, p, n);

			if (r == -EINTR)
				continue;

			if (r == 0 || r == -EAGAIN) {
				usleep(frame_time * 800000);
				continue;
			}

			ASSERT("read PCM data, %d bytes", r > 0, n);

			(char *) p += r;
			n -= r;

			if (r < 200)
				usleep(frame_time * 800000);
		}

		if (rtime < 0.0) {
			snd_pcm_channel_status_t status;
			int err;

			status.channel = SND_PCM_CHANNEL_CAPTURE;

			if ((err = snd_pcm_plugin_status(handle, &status)) < 0)
				FAIL("Failed to query ALSA PCM plugin status (%d, %s)", err, snd_strerror(err));

			rtime = status.stime.tv_sec + status.stime.tv_usec / 1e6;
		} else
			rtime += frame_time;

		left = scan_range - samples_per_frame;
		p = abuffer;

		buf.time = rtime;
		buf.data = (unsigned char *) p;
		return &buf;
	}

	utime = rtime + ((p - abuffer) >> stereo) / (double) sampling_rate;
	left -= samples_per_frame;

	p += samples_per_frame;

	buf.time = utime;
	buf.data = (unsigned char *) p;
	return &buf;
}

static void
alsa_pcm_send_empty(fifo *f, buffer *b)
{
}

void
alsa_pcm_init(void)
{
	snd_pcm_channel_info_t info;
	snd_pcm_channel_params_t params;
	snd_pcm_channel_setup_t setup;
	int buffer_size;
	int err;

	samples_per_frame = SAMPLES_PER_FRAME << stereo;
	scan_range = MAX(BUFFER_SIZE / sizeof(short) / samples_per_frame, 1) * samples_per_frame;
	frame_time = (scan_range >> stereo) / (double) sampling_rate;
	look_ahead = (512 - 32) << stereo;

	buffer_size = (scan_range + look_ahead)	* sizeof(abuffer[0]);

	ASSERT("allocate PCM buffer, %d bytes",
		(abuffer = calloc_aligned(buffer_size, CACHE_LINE)) != NULL, buffer_size);

	printv(3, "Allocated PCM buffer, %d bytes\n", buffer_size);

	if ((err = snd_pcm_open(&handle, 0, 0, SND_PCM_OPEN_CAPTURE)) < 0)
		FAIL("Cannot open ALSA device 0,0 (%d, %s)", err, snd_strerror(err));
	// XXX 0,0

	info.channel = SND_PCM_CHANNEL_CAPTURE;

	if ((err = snd_pcm_plugin_info(handle, &info)) < 0)
		FAIL("Cannot obtain ALSA device info (%d, %s)", err, snd_strerror(err));

	printv(2, "Opened ALSA PCM plugin device\n");

	memset(&params, 0, sizeof(params));

	params.channel = SND_PCM_CHANNEL_CAPTURE;
	params.mode = SND_PCM_MODE_STREAM;
	params.format.interleave = stereo;
	params.format.format = SND_PCM_SFMT_S16_LE;
	params.format.rate = sampling_rate;
	params.format.voices = stereo + 1;
	params.start_mode = SND_PCM_START_DATA;
	params.stop_mode = SND_PCM_STOP_STOP;
	params.time = 1;
	params.buf.stream.queue_size = BUFFER_SIZE * 2;
	params.buf.stream.fill = SND_PCM_FILL_NONE;

	if ((err = snd_pcm_plugin_params(handle, &params)) < 0)
		FAIL("Failed to set ALSA PCM plugin parameters (%d, %s)", err, snd_strerror(err));

	setup.channel = SND_PCM_CHANNEL_CAPTURE;

	if ((err = snd_pcm_plugin_setup(handle, &setup)) < 0)
		FAIL("Failed to query ALSA PCM plugin parameters (%d, %s)", err, snd_strerror(err));

	printv(3, "ALSA setup: ilv %d, format %d, rate %d, voices %d, queue %d\n",
		setup.format.interleave, setup.format.format,
		setup.format.rate, setup.format.voices,
		setup.buf.stream.queue_size);

	if ((err = snd_pcm_plugin_prepare(handle, SND_PCM_CHANNEL_CAPTURE)) < 0)
		FAIL("Failed to prepare ALSA PCM plugin for recording (%d, %s)", err, snd_strerror(err));

	init_callback_fifo(audio_cap_fifo = &pcm_fifo,
		alsa_pcm_wait_full, alsa_pcm_send_empty,
		NULL, NULL, 0, 0);
}

#else // HAVE_LIBASOUND

void
alsa_pcm_init(void)
{
	FAIL("Not compiled with ALSA interface.\n"
	     "More about ALSA at http://www.alsa-project.org\n");
}

#endif // !HAVE_LIBASOUND
