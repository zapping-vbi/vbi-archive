/*
 *  MPEG-1 Real Time Encoder
 *  Audio File Library (libaudiofile) Interface
 *
 *  Copyright (C) 2000-2001 Michael H. Schimek
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

/* $Id: afl.c,v 1.3 2001-08-08 23:01:58 garetxe Exp $ */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include "../common/log.h"
#include "../common/fifo.h"

#if HAVE_LIBAUDIOFILE

#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>

#include <audiofile.h>

#include "audio.h"
#include "../common/math.h"
#include "../common/alloc.h"

#define BUFFER_SIZE 8192 // bytes

struct afl_context {
	struct pcm_context	pcm;

	AFfilehandle		file;
	int			scan_range;
	int			look_ahead;
	int			samples_per_frame;
	double			time, frame_period;
	short *			p;
	int			left;
	bool			eof;
};

static void
wait_full(fifo *f)
{
	struct afl_context *afl = f->user_data;
	buffer *b = PARENT(f->buffers.head, buffer, added);

	assert(b->data == NULL); /* no queue */

	if (afl->left <= 0) {
		ssize_t r;

		if (afl->eof) {
			b->used = 0;
			send_full_buffer(&afl->pcm.producer, b);
			return;
		}

		memcpy(b->allocated, (short *) b->allocated + afl->scan_range,
			afl->look_ahead * sizeof(short));

		afl->p = (short *) b->allocated;
		afl->left = afl->scan_range - afl->samples_per_frame;

		r = afReadFrames(afl->file, AF_DEFAULT_TRACK,
			b->allocated + afl->look_ahead * sizeof(short),
			afl->scan_range >> afl->pcm.stereo) << afl->pcm.stereo;

		if (r < afl->scan_range) {
			memset((short *) b->allocated + afl->look_ahead + r,
				0, (afl->scan_range - r) * sizeof(short));

			afl->left -= MAX(r - afl->look_ahead, 0);
			afl->eof = TRUE;

			if (afl->left < afl->samples_per_frame) {
				b->used = 0; /* EOF */
				send_full_buffer(&afl->pcm.producer, b);
			}
		}

		b->data = b->allocated;
	} else {
		afl->p += afl->samples_per_frame;
		afl->left -= afl->samples_per_frame;

		b->data = (unsigned char *) afl->p;
	}

	b->time = afl->time;
	afl->time += afl->frame_period;

	send_full_buffer(&afl->pcm.producer, b);
}

static void
send_empty(consumer *c, buffer *b)
{
	// XXX
	rem_node(&c->fifo->full, &b->node);
	b->data = NULL;
}

fifo *
open_pcm_afl(char *name, int ignored1, bool ignored2)
{
	struct afl_context *afl;
	int buffer_size;
	int version, channels, rate;
	buffer *b;

	ASSERT("allocate pcm context",
		(afl = calloc(1, sizeof(struct afl_context))));

	ASSERT("open audio file %s",
		(afl->file = afOpenFile(name, "r", NULL)), name);

	printv(2, "Opened file %s, ", name);

	switch (afGetFileFormat(afl->file, &version)) {
	case AF_FILE_AIFFC:
		printv(2, "Audio Interchange File Format AIFF-C\n");
		break;

	case AF_FILE_AIFF:
		printv(2, "Audio Interchange File Format\n");
		break;

	case AF_FILE_NEXTSND:
		printv(2, "NeXT .snd/Sun .au format\n");
		break;

	case AF_FILE_WAVE:
		printv(2, "MS RIFF WAVE format\n");
		break;

	default:
		printv(2, "unknown format\n");
	}

	channels = afGetChannels(afl->file, AF_DEFAULT_TRACK);
	rate = lroundn(afGetRate(afl->file, AF_DEFAULT_TRACK));

	if (channels > 2)
		FAIL("Cannot read %d channel file %s\n", channels, name);

	afl->pcm.sampling_rate = rate;
	afl->pcm.stereo = (channels > 1);

	printv(2, "Sampling rate %d Hz, %s\n",
		afl->pcm.sampling_rate, afl->pcm.stereo ? "stereo" : "mono");

	ASSERT("set virtual sample format",
		0 == afSetVirtualSampleFormat(afl->file,
			AF_DEFAULT_TRACK, AF_SAMPFMT_TWOSCOMP, 16));

	ASSERT("set virtual byte order",
		0 == afSetVirtualByteOrder(afl->file,
			AF_DEFAULT_TRACK, AF_BYTEORDER_LITTLEENDIAN));

	afl->samples_per_frame = SAMPLES_PER_FRAME << afl->pcm.stereo;
	afl->scan_range = MAX(BUFFER_SIZE / sizeof(short)
		/ afl->samples_per_frame, 1) * afl->samples_per_frame;
	afl->look_ahead = (512 - 32) << afl->pcm.stereo;

	afl->time = 0.0; // source offline, don't synchronize
	afl->frame_period = SAMPLES_PER_FRAME / (double) afl->pcm.sampling_rate;

	buffer_size = (afl->scan_range + afl->look_ahead) * sizeof(short);

	ASSERT("init afl fifo",	init_callback_fifo(
		&afl->pcm.fifo, "audio-afl",
		NULL, NULL, wait_full, send_empty,
		1, buffer_size));

	ASSERT("init afl producer",
		add_producer(&afl->pcm.fifo, &afl->pcm.producer));

	afl->pcm.fifo.user_data = afl;

	b = PARENT(afl->pcm.fifo.buffers.head, buffer, added);

	b->data = NULL;
	b->used = (afl->samples_per_frame + afl->look_ahead) * sizeof(short);

	return &afl->pcm.fifo;
}

#else // !HAVE_LIBAUDIOFILE

fifo *
open_pcm_afl(char *name, int ignored1, bool ignored2)
{
	FAIL("Audio compression from file requires libaudiofile:\n"
		"http://www.68k.org/~michael/audiofile\n");
}

#endif // !HAVE_LIBAUDIOFILE
