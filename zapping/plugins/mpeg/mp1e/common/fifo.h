/*
 *  MPEG-1 Real Time Encoder
 *
 *  Copyright (C) 1999-2000 Michael H. Schimek
 *
 *  Modified by Iñaki G.E.
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

/* $Id: fifo.h,v 1.8 2000-10-22 05:24:50 mschimek Exp $ */

#ifndef FIFO_H
#define FIFO_H

#include "list.h"
#include "types.h"
#include "threads.h"
#include "log.h"
#include "alloc.h"

typedef struct {
	node 			node;
	int			index;

	/* Prod. r/w, Cons. r/o */

	int			type;
	int			offset;
	double			time;

	unsigned char *		data;
	long			used;		// bytes

	/* Owner r/o */

	unsigned char *		allocated;	// by init_fifo
	long			size;		// bytes

} buffer;

typedef struct _fifo {
	mucon			producer;
	mucon *			consumer;

	list			full;		// FIFO
	list			empty;		// LIFO

	buffer *		(* wait_full)(struct _fifo *);
	void			(* send_empty)(struct _fifo *, buffer *);
	buffer *		(* wait_empty)(struct _fifo *);
	void			(* send_full)(struct _fifo *, buffer *);

	bool			(* start)(struct _fifo *);

	/* Owner private */

	buffer *		buffers;
	int			num_buffers;

	void *			user_data; // Useful for callbacks
} fifo;

extern bool	init_buffer(buffer *b, int size);
extern void	uninit_buffer(buffer *b);
extern int	init_buffered_fifo(fifo *f, mucon *consumer, int size, int num_buffers);
extern int	init_callback_fifo(fifo *f, buffer * (* wait_full)(fifo *), void (* send_empty)(fifo *, buffer *), buffer * (* wait_empty)(fifo *), void (* send_full)(fifo *, buffer *), int size, int num_buffers);
extern void	uninit_fifo(fifo *f);
extern int	buffers_queued(fifo *f);

#define VALID_BUFFER(f, b) \
	((b)->index < (f)->num_buffers && (f)->buffers + (b)->index == (b))

/*
 *     send_full_buffer, +->-+ wait_full_buffer,
 *  bcast consumer mucon |   | wait consumer mucon
 *                       |   |
 *            (producer) |   | (consumer)
 *                       |   |
 *    wait_empty_buffer, +-<-+ send_empty_buffer,
 *   wait producer mucon       bcast producer mucon
 */

static inline void
send_full_buffer(fifo *f, buffer *b)
{
	if (f->send_full) {
		f->send_full(f, b);
		return;
	}

	pthread_mutex_lock(&f->consumer->mutex);

	add_tail(&f->full, &b->node);

	pthread_mutex_unlock(&f->consumer->mutex);
	pthread_cond_broadcast(&f->consumer->cond);
}

static inline void
unget_full_buffer(fifo *f, buffer *b)
{
	if (f->wait_full) {
		add_head(&f->full, &b->node);
		return;
	}

	pthread_mutex_lock(&f->consumer->mutex);

	add_head(&f->full, &b->node);

	pthread_mutex_unlock(&f->consumer->mutex);
	pthread_cond_broadcast(&f->consumer->cond);
}

static inline buffer *
wait_full_buffer(fifo *f)
{
	buffer *b;

	if (f->wait_full)
		return (b = (buffer *) rem_head(&f->full)) ? b : f->wait_full(f);

	pthread_mutex_lock(&f->consumer->mutex);

	while (!(b = (buffer *) rem_head(&f->full)))
		pthread_cond_wait(&f->consumer->cond, &f->consumer->mutex);

	pthread_mutex_unlock(&f->consumer->mutex);

	return b;
}

static inline buffer *
recv_full_buffer(fifo *f)
{
	buffer *b;

	if (f->wait_full)
		return (b = (buffer *) rem_head(&f->full)) ? b : f->wait_full(f);

	pthread_mutex_lock(&f->consumer->mutex);
	b = (buffer *) rem_head(&f->full);
	pthread_mutex_unlock(&f->consumer->mutex);

	return b;
}

static inline void
send_empty_buffer(fifo *f, buffer *b)
{
	if (f->send_empty) {
		f->send_empty(f, b);
		return;
	}

	pthread_mutex_lock(&f->producer.mutex);

	add_head(&f->empty, &b->node);

	pthread_mutex_unlock(&f->producer.mutex);
	pthread_cond_broadcast(&f->producer.cond);
}

static inline buffer *
wait_empty_buffer(fifo *f)
{
	buffer *b;

	if (f->wait_empty)
		return (b = (buffer *) rem_head(&f->empty)) ? b : f->wait_empty(f);

	pthread_mutex_lock(&f->producer.mutex);

	while (!(b = (buffer *) rem_head(&f->empty)))
		pthread_cond_wait(&f->producer.cond, &f->producer.mutex);

	pthread_mutex_unlock(&f->producer.mutex);

	return b;
}

static inline buffer *
recv_empty_buffer(fifo *f)
{
	buffer *b;

	if (f->wait_empty)
		return (b = (buffer *) rem_head(&f->empty)) ? b : f->wait_empty(f);

	pthread_mutex_lock(&f->producer.mutex);
	b = (buffer *) rem_head(&f->empty);
	pthread_mutex_unlock(&f->producer.mutex);

	return b;
}

#endif // FIFO_H
