/*
 *  MPEG-1 Real Time Encoder
 *  ESD [Enlightened Sound Daemon] interface
 *
 *  Copyright (C) 2000 I�aki G.E.
 *  Modified 2001-09 Michael H. Schimek
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

/* $Id: esd.c,v 1.7 2001-09-26 10:44:48 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "../common/log.h"

#include "audio.h"

#ifdef HAVE_ESD

#include <unistd.h>
#include <math.h>

#include <esd.h>

struct esd_context {
	struct pcm_context	pcm;

	int			socket;
	double			time, buffer_period;
};

static void
wait_full(fifo *f)
{
	struct esd_context *esd = f->user_data;
	buffer *b = PARENT(f->buffers.head, buffer, added);
	struct timeval tv;
	unsigned char *p;
	ssize_t r, n;
	double dt, now;

	assert(b->data == NULL); /* no queue */

	for (p = b->allocated, n = b->size; n > 0;) {
		fd_set rdset;

		FD_ZERO(&rdset);
		FD_SET(esd->socket, &rdset);
		tv.tv_sec = 2;
		tv.tv_usec = 0;
		r = select(esd->socket+1, &rdset,
			     NULL, NULL, &tv);

		if (r == 0)
			FAIL("ESD read timeout");
		else if (r < 0)
			FAIL("ESD select error (%d, %s)",
				errno, strerror(errno));

		r = read(esd->socket, p, n);

		if (r < 0) {
			ASSERT("read PCM data, %d bytes", errno == EINTR, n);
			continue;
		}

		p += r;
		n -= r;
	}

	now = current_time();
	dt = now - esd->time;

	if (esd->time > 0
	    && fabs(dt - esd->buffer_period) < esd->buffer_period * 0.1) {
		b->time = esd->time;
		esd->buffer_period = (esd->buffer_period - dt) * 0.999 + dt;
		esd->time += esd->buffer_period;
	} else {
		esd->time = now;
		b->time = now - esd->buffer_period;
	}

	b->data = b->allocated;

	send_full_buffer(&esd->pcm.producer, b);
}

static void
send_empty(consumer *c, buffer *b)
{
	// XXX
	unlink_node(&c->fifo->full, &b->node);

	b->data = NULL;
}

fifo *
open_pcm_esd(char *unused, int sampling_rate, bool stereo)
{
	struct esd_context *esd;
	esd_format_t format;
	int buffer_size;
	buffer *b;

	ASSERT("allocate pcm context",
		(esd = calloc(1, sizeof(struct esd_context))));

	esd->pcm.format = RTE_SNDFMT_S16LE;
	esd->pcm.sampling_rate = sampling_rate;
	esd->pcm.stereo = stereo;

	buffer_size = 1 << (10 + (sampling_rate > 24000));

	esd->time = 0;
	esd->buffer_period = buffer_size / (double) sampling_rate;

	buffer_size <<= stereo + 1;

	format = ESD_STREAM | ESD_RECORD | ESD_BITS16
		| (stereo ? ESD_STEREO : ESD_MONO);

	esd->socket =
		esd_record_stream_fallback(format, sampling_rate, NULL, NULL);

	ASSERT("open ESD socket", esd->socket > 0);

	printv(2, "Opened ESD socket\n");
	printv(3, "ESD format 0x%08x, %d Hz %s, buffer %d bytes = %f s\n",
		format, sampling_rate, stereo ? "stereo" : "mono",
		buffer_size, esd->buffer_period);

	ASSERT("init esd fifo",	init_callback_fifo(
		&esd->pcm.fifo, "audio-esd",
		NULL, NULL, wait_full, send_empty,
		1, buffer_size));

	ASSERT("init esd producer",
		add_producer(&esd->pcm.fifo, &esd->pcm.producer));

	esd->pcm.fifo.user_data = esd;

	b = PARENT(esd->pcm.fifo.buffers.head, buffer, added);

	b->data = NULL;
	b->used = b->size;
	b->offset = 0;

	return &esd->pcm.fifo;
}

#else /* !HAVE_ESD */

fifo *
open_pcm_esd(char *dev_name, int sampling_rate, bool stereo)
{
	FAIL("Not compiled with ESD interface.\n"
	     "For more info about ESD visit http://www.tux.org/~ricdude/EsounD.html\n");
}

#endif /* !HAVE_ESD */
