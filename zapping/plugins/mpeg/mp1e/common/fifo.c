/*
 *  MPEG-1 Real Time Encoder
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

/* $Id: fifo.c,v 1.4 2000-10-15 21:24:48 mschimek Exp $ */

#include "fifo.h"
#include "alloc.h"
#include "mmx.h"

void
uninit_buffer(buffer *b)
{
	if (b && b->allocated)
		free_aligned(b->allocated);

	b->allocated = NULL;
	b->size = 0;
}

bool
init_buffer(buffer *b, int size)
{
	if (!b)
		return FALSE;

	memset(b, 0, sizeof(buffer));

	if (size > 0) {
		b->data =
		b->allocated =
			calloc_aligned(size, size < 4096 ? CACHE_LINE : 4096);

		if (!b->allocated)
			return FALSE;

		b->size = size;
	}

	return TRUE;
}

static void
dead_end(fifo *f)
{
	FAIL("Invalid fifo %p", f);
}

static bool
tv_sucks(fifo *f)
{
	return TRUE;
}

void
uninit_fifo(fifo * f)
{
	int i;

	if (f->buffers) {
		for (i = 0; i < f->num_buffers; i++)
			uninit_buffer(&f->buffers[i]);

		free(f->buffers);
	}

	mucon_destroy(&f->producer);

	f->wait_full  = (buffer * (*)(fifo *)) dead_end;
	f->send_empty = (void (*)(fifo *, buffer *)) dead_end;
	f->wait_empty = (buffer * (*)(fifo *)) dead_end;
	f->send_full  = (void (*)(fifo *, buffer *)) dead_end;

	f->start = tv_sucks;

	memset(f, 0, sizeof(fifo));
}

int
init_buffered_fifo(fifo *f, mucon *consumer, int size, int num_buffers)
{
	int i;

	memset(f, 0, sizeof(fifo));

	if (!(f->buffers = calloc(num_buffers, sizeof(buffer))))
		return 0;

	for (i = 0; i < num_buffers; i++)
		f->buffers[i].index = i;

	if (size > 0) {
		for (i = 0; i < num_buffers; i++) {
			if (!init_buffer(&f->buffers[i], size))
				break;

			add_tail(&f->empty, &f->buffers[i].node);
		}

		if (i == 0) {
			free(f->buffers);
			f->buffers = NULL;
			return 0;
		}
	}

	mucon_init(&f->producer);
	f->consumer = consumer; /* NB the consumer mucon can be shared,
				   cf. video, audio -> mux */

	f->start = tv_sucks;

	return f->num_buffers = i; // sic
}

int
init_callback_fifo(fifo *f,
	buffer * (* wait_full)(fifo *),
	void     (* send_empty)(fifo *, buffer *),
	buffer * (* wait_empty)(fifo *),
	void     (* send_full)(fifo *, buffer *),
	int size, int num_buffers)
{
	int i;

	memset(f, 0, sizeof(fifo));

	if (num_buffers > 0) {
		if (!(f->buffers = calloc(num_buffers, sizeof(buffer))))
			return 0;

		for (i = 0; i < num_buffers; i++)
			f->buffers[i].index = i;

		if (size > 0) {
			for (i = 0; i < num_buffers; i++) {
				if (!init_buffer(&f->buffers[i], size))
					break;

				add_tail(&f->empty, &f->buffers[i].node);
			}

			if (i == 0) {
				free(f->buffers);
				f->buffers = NULL;
				return 0;
			}
		}

		f->num_buffers = i;
	}

	mucon_init(&f->producer);

	f->wait_full  = wait_full;
	f->send_empty = send_empty;
	f->wait_empty = wait_empty;
	f->send_full  = send_full;

	f->start = tv_sucks;

	return f->num_buffers;
}

int
buffers_queued(fifo *f)
{
	node *n;
	int i;

	pthread_mutex_lock(&f->consumer->mutex);
	for (n = f->full.head, i = 0; n; n = n->next, i++); 
	pthread_mutex_unlock(&f->consumer->mutex);

	return i;
}
