/*
 *  MPEG-1 Real Time Encoder
 *  SGI Audio File Library (libaudiofile) interface
 *
 *  Copyright (C) 2000, 2001, 2002 Michael H. Schimek
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

/* $Id: afl.c,v 1.10 2002-02-25 06:22:19 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "../common/log.h"
#include "audio.h"

#if HAVE_LIBAUDIOFILE

/*
 *  SGI Audio File Library
 *  (Linux incarnation by M. Pruett, 2000-10-10)
 *
 *  XXX error handling is missing
 */

#include <audiofile.h>
#include "../common/math.h"

struct afl_context {
	struct pcm_context	pcm;

	AFfilehandle		file;
	double			time, buffer_period;
	bool			eof;
};

static void
wait_full(fifo *f)
{
	struct afl_context *afl = f->user_data;
	buffer *b = PARENT(f->buffers.head, buffer, added);
	ssize_t r;

	assert(b->data == NULL); /* no queue */

	b->time = afl->time;
	b->data = b->allocated;

	if (afl->eof) {
		b->used = 0;
		send_full_buffer(&afl->pcm.producer, b);
		return;
	}

	afl->time += afl->buffer_period;

	r = afReadFrames(afl->file, AF_DEFAULT_TRACK,
		b->allocated, b->size >> (afl->pcm.stereo + 1))
			<< (afl->pcm.stereo + 1);

	if (r < b->size) {
		memset(b->allocated + r, 0, b->size - r);

		afl->eof = TRUE;
		b->used = r;
	}

	send_full_buffer(&afl->pcm.producer, b);
}

static void
send_empty(consumer *c, buffer *b)
{
	// XXX
	rem_node(&c->fifo->full, &b->node);

	b->data = NULL;
}

void
open_pcm_afl(char *name, int ignored1, bool ignored2, fifo **f)
{
	struct afl_context *afl;
	int buffer_size = 8192;
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

	afl->pcm.format = RTE_SNDFMT_S16_LE;
	afl->pcm.sampling_rate = rate;
	afl->pcm.stereo = (channels > 1);

	printv(2, "Sampling rate %d Hz, %s\n",
		afl->pcm.sampling_rate, afl->pcm.stereo ? "stereo" : "mono");

	ASSERT("set virtual sample format (signed 16 bit)",
		0 == afSetVirtualSampleFormat(afl->file,
			AF_DEFAULT_TRACK, AF_SAMPFMT_TWOSCOMP, 16));

	ASSERT("set virtual byte order (little endian)",
		0 == afSetVirtualByteOrder(afl->file,
			AF_DEFAULT_TRACK, AF_BYTEORDER_LITTLEENDIAN));

	/*
	 *  Note a stream starting at time 0.0 is not subject to
	 *  initial synchronization but to clock drift compensation
	 *  unless the stream is chosen as reference clock.
	 */
	afl->time = 0.0;
	afl->buffer_period = buffer_size
		/ (double)(afl->pcm.sampling_rate << (afl->pcm.stereo + 1));

	*f = &afl->pcm.fifo;

	ASSERT("init afl fifo",	init_callback_fifo(*f, "audio-afl",
		NULL, NULL, wait_full, send_empty,
		1, buffer_size));

	ASSERT("init afl producer",
		add_producer(*f, &afl->pcm.producer));

	(*f)->user_data = afl;

	b = PARENT(afl->pcm.fifo.buffers.head, buffer, added);

	b->data = NULL;
	b->used = b->size;
	b->offset = 0;
}

#else /* !HAVE_LIBAUDIOFILE */

void
open_pcm_afl(char *name, int ignored1, bool ignored2, fifo **f)
{
	FAIL("Audio compression from file requires libaudiofile:\n"
		"http://www.68k.org/~michael/audiofile\n");
}

#endif /* !HAVE_LIBAUDIOFILE */
