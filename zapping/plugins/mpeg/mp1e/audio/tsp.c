/*
 *  MPEG Real Time Encoder
 *  Audio File Library Interface
 *
 *  Copyright (C) 1999-2000 Michael H. Schimek
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

/* $Id: tsp.c,v 1.3 2000-08-12 02:14:37 mschimek Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include "../common/log.h"

#ifdef HAVE_LIBTSP 

#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <libtsp.h>
#include <sys/time.h>

#include "mpeg.h"
#include "audio.h"
#include "../common/mmx.h"
#include "../common/math.h"

/*
 *  Audio File Library (libtsp) Interface
 *
 *  AU audio file:
 *	8-bit mu-law, 8-bit A-law, 8-bit integer, 16-bit integer,
 *      24-bit integer, 32-bit integer, 32-bit IEEE floating-point,
 *      and  64-bit IEEE floating-point data formats are supported.
 *  RIFF WAVE:
 *      8-bit mu-law, 8-bit A-law, offset-binary 1-bit to 8-bit integer,
 *      9-bit to 32-bit integer, 32-bit IEEE floating-point, and 64-bit IEEE
 *      floating-point data formats are supported.
 *  AIFF or AIFF-C audio file:
 *      8-bit mu-law, 8-bit A-law, 1-bit to 32-bit integer,
 *      32-bit IEEE floating-point, and 64-bit IEEE floating-point data
 *      formats are supported.
 *
 *  AFsp-v4r3.tar.Z
 *  http://www.TSP.ECE.McGill.CA/software
 *  or ftp://ftp.TSP.ECE.McGill.CA/pub/AFsp
 */

#define BUFFER_SIZE 8192 // bytes

static AFILE *		AFp;
static long		offs;
static float *		Fbuff;
static short *		abuffer;
static int		scan_range;
static int		look_ahead;
static int		samples_per_frame;
static short *		ubuffer;
static buffer		buf;
static fifo		tsp_fifo;

extern char *		pcm_dev;
extern int		sampling_rate;
extern int		audio_mode;

/*
 * Read window: samples_per_frame (1152 * channels) + look_ahead (480 * channels);
 * Subband window size 512 samples, step width 32 samples (32 * 3 * 12 per frame)
 */

// XXX only one at a time, not checked
static buffer *
tsp_wait_full(fifo *f)
{
	static double rtime = 0, utime;
	static int left = 0;
	static short *p;
	static char done = 0;

	if (!rtime) {
		struct timeval tv;

		gettimeofday(&tv, NULL);

		rtime = tv.tv_sec + tv.tv_usec / 1e6;
	} else if (ubuffer) {
		p = ubuffer;
		ubuffer = NULL;
		buf.time = utime;
		buf.data = (unsigned char *) p;
		return &buf;
	}

	utime = rtime + (double) offs / sampling_rate;

	if (left <= 0)
	{
		ssize_t r;
		int n, i;

		if (done)
			return NULL;
/*
		if (offs == 0) {
			p = abuffer;
			n = scan_range + look_ahead;
		} else
*/
		{
			memcpy(abuffer, abuffer + scan_range, look_ahead * sizeof(abuffer[0]));

			p = abuffer + look_ahead;
			n = scan_range;
		}

		left = scan_range - samples_per_frame;

		while (n > 0) {
			r = AFreadData(AFp, offs, Fbuff, n);

			if (r < 0 && errno == EINTR)
				continue;

			if (r == 0) {
				memset(p, 0, n);
				left -= n;
				if (!left)
					return NULL;
				done = 1;
				break;
			}

			ASSERT("read from audio file, %d bytes", r > 0, n);

			// Now this is stupid, but who cares we're fast enough
			for (i = 0; i < r; p++, i++)
				*p = Fbuff[i];

			offs += r;
			n -= r;
		}

		p = abuffer;

		buf.time = utime;
		buf.data = (unsigned char *) p;
		return &buf;
	}

	left -= samples_per_frame;
	p += samples_per_frame;

	buf.time = utime;
	buf.data = (unsigned char *) p;
	return &buf;
}

void
tsp_send_empty(fifo *f, buffer *b)
{
}

void
tsp_init(void)
{
	int buffer_size, stereo;
	long int Nsamp, Nchan;
	float Sfreq;

	/*
	 * Headerless format hint:
	 * 16-bit integer, start offset 0, sampling rate 44.1 kHz,
	 * little-endian, 1 channel, scale factor 1.0.
	 */
	AFsetNHpar("integer16,0,44100,little-endian,1,1.0");

	AFp = AFopenRead(pcm_dev, &Nsamp, &Nchan, &Sfreq,
		(verbose > 1) ? stderr : NULL);

	ASSERT("open audio file %s%s", AFp != NULL, pcm_dev,
		(errno == 0) ? " (unknown format?)" : "");

	printv(2, "Opened file %s\n", pcm_dev);

	if (audio_mode == AUDIO_MODE_MONO && Nchan > 1)
		audio_mode = AUDIO_MODE_STEREO;
	else if (audio_mode != AUDIO_MODE_MONO && Nchan < 2)
		audio_mode = AUDIO_MODE_MONO;

	sampling_rate = Sfreq;
	stereo = (audio_mode != AUDIO_MODE_MONO);

	samples_per_frame = SAMPLES_PER_FRAME << stereo;
	scan_range = MAX(BUFFER_SIZE / sizeof(short) / samples_per_frame, 1) * samples_per_frame;
	look_ahead = (512 - 32) << stereo;
	offs = 0;

	buffer_size = (scan_range + look_ahead)	* sizeof(buffer[0]);

	ASSERT("allocate audio buffer, %d bytes",
		(buffer = calloc_aligned(buffer_size, CACHE_LINE)) != NULL, buffer_size);

	buffer_size = (scan_range + look_ahead)	* sizeof(Fbuff[0]);

	ASSERT("allocate audio bounce buffer, %d bytes",
		(Fbuff = calloc_aligned(buffer_size, 32)) != NULL, buffer_size);

	printv(3, "Allocated audio buffers, %d bytes\n", buffer_size * (sizeof(buffer[0]) + sizeof(Fbuff[0])));

	init_callback_fifo(audio_cap_fifo = &tsp_fifo,
		tsp_wait_full, tsp_send_empty,
		NULL, NULL, 0, 0);
}

#else

void tsp_init(void)
{
	FAIL("Audio compression from file requires libtsp;\n"
	    "AFsp-v4r3.tar.Z can be obtained from\n"
	    "http://www.TSP.ECE.McGill.CA/software\n"
	    "or ftp://ftp.TSP.ECE.McGill.CA/pub/AFsp\n"
	    "free of charge for non-commercial use.");
}

#endif // HAVE_LIBTSP
