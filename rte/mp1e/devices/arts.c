/*
 *  MPEG-1 Real Time Encoder
 *  ARTS (KDE sound daemon) interface
 *
 *  Copyright (C) 2000, 2001 Iñaki García Etxebarria
 *  Modified 2002 Michael H. Schimek
 *
 *  (This a near verbatim copy of its Zapping counterpart,
 *   mhs couldn't care less if this works.)
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

/* $Id: arts.c,v 1.1 2002-06-12 04:00:40 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "../common/log.h"
#include "../common/math.h"

#include "../audio/audio.h"

#ifdef HAVE_ARTS

#include <unistd.h>
#include <math.h>

/** artsc, aka "porting to arts for dummies" :-) **/
#include <artsc.h>

struct arts_context {
	struct pcm_context	pcm;
	arts_stream_t		stream;
	double			time;
	struct tfmem		tfmem;
};

#define ARTS_TIME_LOG(x) x /* x */

static void
wait_full(fifo *f)
{
	struct arts_context *arts = f->user_data;
	buffer *b = PARENT(f->buffers.head, buffer, added);
	unsigned char *p;
	ssize_t r, n;
	struct timeval tv;
	double now;

	assert(b->data == NULL); /* no queue */

	for (p = b->allocated, n = b->size; n > 0;) {
		r = arts_read(arts->stream, p, n);

		if (r < 0) {
			g_warning("ARTS: READ ERROR, quitting: %s",
				  arts_error_text(r));
			memset(p, 0, n);
			break;
		}

		p += r;
		n -= r;
	}

	/* Awful. */
	now = current_time();

	if (arts->time > 0) {
		double dt = now - arts->time;

		/* not reliable enough, let's hope arts cares
		   if (dt - arts->tfmem.err > arts->tfmem.ref * 1.98) {
 			arts->time = now;
		   } else */
		{
			arts->time += mp1e_timestamp_filter
				(&arts->tfmem, dt, 0.001, 1e-7, 0.05);
		}

		ARTS_TIME_LOG(printv(0, "now %f dt %+f err %+f t/b %+f\n",
				     now, dt, arts->tfmem.err, arts->tfmem.ref));
	} else
		arts->time = now;

	b->time = arts->time;
	b->data = b->allocated;

	send_full_buffer(&arts->pcm.producer, b);
}

static void
send_empty(consumer *c, buffer *b)
{
	// XXX
	unlink_node(&c->fifo->full, &b->node);

	b->data = NULL;
}

void
open_pcm_arts(char *unused, int sampling_rate, bool stereo, fifo **f)
{
	struct arts_context *arts;
	int errcode;
	int buffer_size;
	buffer *b;

	ASSERT("allocate pcm context",
		(arts = calloc(1, sizeof(struct arts_context))));

	arts->pcm.format = RTE_SNDFMT_S16LE;
	arts->pcm.sampling_rate = sampling_rate;
	arts->pcm.stereo = stereo;

	buffer_size = 1 << (10 + (sampling_rate > 24000));

	arts->time = 0.0;
	mp1e_timestamp_init(&arts->tfmem,
			    buffer_size / (double) sampling_rate);

	buffer_size <<= stereo + 1;

	if ((errcode = arts_init()))
		FAIL("Cannot initialize ARTS: %s",
		     arts_error_text(errcode));

	arts->stream = arts_record_stream(sampling_rate, 16,
					  (!!stereo) + 1, "Zapping");

	/* FIXME: can this really fail? */
	ASSERT("open ARTS recording stream", arts->stream != 0);

	*f = &arts->pcm.fifo;

	ASSERT("init arts fifo", init_callback_fifo(*f, "audio-arts",
		NULL, NULL, wait_full, send_empty,
		1, buffer_size));

	ASSERT("init arts producer",
		add_producer(*f, &arts->pcm.producer));

	(*f)->user_data = arts;

	b = PARENT((*f)->buffers.head, buffer, added);

	b->data = NULL;
	b->used = b->size;
	b->offset = 0;
}

#else /* !HAVE_ARTS */

void
open_pcm_arts(char *dev_name, int sampling_rate, bool stereo, fifo **f)
{
	FAIL("Not compiled with ARTS interface.\n");
}

#endif /* !HAVE_ARTS */

