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

/* $Id: fifo.c,v 1.2 2000-08-10 01:18:59 mschimek Exp $ */

#include "fifo.h"
#include "alloc.h"
#include "mmx.h"

void
uninit_buffer(buffer *b)
{
	if (b && b->allocated)
		free(b->allocated);

	b->allocated = NULL;
	b->_size = 0;
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

		b->_size = size;
	}

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

	memset(f, 0, sizeof(fifo));
}

int
init_buffered_fifo(fifo *f, mucon *consumer, int size, int num_buffers)
{
	int i;

	memset(f, 0, sizeof(fifo));

	mucon_init(&f->producer);
	f->consumer = consumer; /* NB the consumer mucon can be shared,
				   cf. video, audio -> mux */

	if (!(f->buffers = calloc(num_buffers, sizeof(buffer))))
		return 0;

	for (i = 0; i < num_buffers; i++)
		f->buffers[i].index = -1;

	for (i = 0; i < num_buffers; i++) {
		if (!init_buffer(&f->buffers[i], size))
			break;

		f->buffers[i].index = i;

		add_tail(&f->empty, &f->buffers[i].node);
	}

	return f->num_buffers = i;
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
