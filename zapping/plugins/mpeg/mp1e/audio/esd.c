/*
 *  MPEG Real Time Encoder
 *  ESD [Enlightened Sound Daemon] interface
 *
 *  Copyright (C) 2000 Iñaki G.E.
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

/* $Id: esd.c,v 1.1 2000-10-17 21:57:05 garetxe Exp $ */

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
#include <linux/soundcard.h>

#include "../common/log.h" 
#include "../common/mmx.h" 
#include "../common/math.h" 
#include "audio.h"
#include "mpeg.h"

#ifdef USE_ESD

#include <esd.h>

/*
 *  PCM Device, ESD interface
 */

#define BUFFER_SIZE (ESD_BUF_SIZE*2) // bytes per read(), appx.

static int		esd_recording_socket;
static short *		abuffer;
static int		scan_range;
static int		look_ahead;
static int		samples_per_frame;
static double		frame_time;
static short *		ubuffer;
static buffer		buf;
static fifo		pcm_fifo;

extern int		sampling_rate;
extern int		stereo;

/*
 *  Read window: samples_per_frame (1152 * channels) + look_ahead (480 * channels);
 *  Subband window size 512 samples, step width 32 samples (32 * 3 * 12 total)
 */

// XXX only one at a time, not checked
static buffer *
esd_pcm_wait_full(fifo *f)
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
		int n;
		struct timeval tv;

		memcpy(abuffer, abuffer + scan_range, look_ahead *
		       sizeof(abuffer[0]));

		p = abuffer + look_ahead;
		n = scan_range * sizeof(abuffer[0]);

		while (n > 0) {
			fd_set rdset;
			int err;

/*			FD_ZERO(&rdset);
			FD_SET(esd_recording_socket, &rdset);
			tv.tv_sec = 1;
			tv.tv_usec = 0;
			err = select(esd_recording_socket+1, &rdset,
				   NULL, NULL, &tv);

			if ((err == -1) || (err == 0))
				continue;
*/
			r = read(esd_recording_socket, p, n);
			
			if (r < 0 && errno == EINTR)
				continue;

			if (r == 0) {
				memset(p, 0, n);
				break;
			}

			ASSERT("read PCM data, %d bytes", r > 0, n);

			(char *) p += r;
			n -= r;
		}

		gettimeofday(&tv, NULL);

		rtime = tv.tv_sec + tv.tv_usec / 1e6;
		rtime -= (scan_range - n) / (double) sampling_rate;

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
esd_pcm_send_empty(fifo *f, buffer *b)
{
}

void
esd_pcm_init(void)
{
	esd_format_t format;
	int buffer_size;

	samples_per_frame = SAMPLES_PER_FRAME << stereo;
	scan_range = MAX(BUFFER_SIZE / sizeof(short) / samples_per_frame, 1) * samples_per_frame;
	frame_time = (scan_range >> stereo) / (double) sampling_rate;
	look_ahead = (512 - 32) << stereo;

	buffer_size = (scan_range + look_ahead)	* sizeof(abuffer[0]);

	ASSERT("allocate PCM buffer, %d bytes",
		(abuffer = calloc_aligned(buffer_size, CACHE_LINE)) != NULL, buffer_size);

	printv(3, "Allocated PCM buffer, %d bytes\n", buffer_size);

	format = ESD_STREAM | ESD_RECORD | ESD_BITS16;

	if (stereo)
		format |= ESD_STEREO;
	else
		format |= ESD_MONO;

	esd_recording_socket =
		esd_record_stream_fallback(format, sampling_rate, NULL, NULL);

	if (esd_recording_socket <= 0)
		FAIL("couldn't create esd recording socket");

	init_callback_fifo(audio_cap_fifo = &pcm_fifo,
		esd_pcm_wait_full, esd_pcm_send_empty,
		NULL, NULL, 0, 0);
}

#else // USE_ESD

void
esd_pcm_init(void)
{
	FAIL("Not compiled with ESD interface.\n"
	     "More about ESD at http://www.tux.org/~ricdude/EsounD.html\n");
}

#endif // !USE_ESD
