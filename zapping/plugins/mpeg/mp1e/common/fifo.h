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

/* $Id: fifo.h,v 1.4 2000-08-28 21:13:22 garetxe Exp $ */

#ifndef FIFO_H
#define FIFO_H

#include "list.h"
#include "types.h"
#include "threads.h"

/*
 *  Old stuff to be removed
 */

#include "log.h"
#include "alloc.h"

typedef struct {
	node 			node;
	unsigned char *		data;
	unsigned char *		buffer;
	int			size;
	double			time;
	int                     index; /* index in fifo.buffer */
} _buffer;

typedef struct {
	pthread_mutex_t		mutex;
	pthread_cond_t		cond;

	list			full;
	list			empty;

	_buffer			buffer[64];
	int                     num_buffers;

	/* MUX parameters */	

	int			in, out, max;
	_buffer *		buf;

	unsigned char *		ptr;
	int			left;
	double			time, tpb;
	double			dts, tick;
	double			rtime, rtick;
} _fifo;

static inline int
_init_fifo(_fifo *f, char *purpose, int size, int buffers)
{
	int i;

	memset(f, 0, sizeof(_fifo));

	for (i = 0; i < 64 && i < buffers; i++) {
		ASSERT("allocate %s buffer #%d",
			(f->buffer[i].data =
			 f->buffer[i].buffer =
			 calloc_aligned(size, 32)) != NULL, purpose, i);

		f->buffer[i].index = i;
		f->buffer[i].size = size;

		add_tail(&f->empty, &f->buffer[i].node);
	}

	pthread_mutex_init(&f->mutex, NULL);
	pthread_cond_init(&f->cond, NULL);

	f->num_buffers = i;

	return i;
}

static inline void
_free_fifo(_fifo * f)
{
	int i;

	for (i = 0; i < f->num_buffers; i++)
		free(f->buffer[i].data);

	pthread_mutex_destroy(&f->mutex);
	pthread_cond_destroy(&f->cond);

	memset(f, 0, sizeof(_fifo));
}

static inline _buffer *
_new_buffer(_fifo *f)
{
	_buffer *b;

	pthread_mutex_lock(&f->mutex);

	while (!(b = (_buffer *) rem_head(&f->empty)))
		pthread_cond_wait(&f->cond, &f->mutex);

	pthread_mutex_unlock(&f->mutex);

	return b;
}

static inline void
_send_buffer(_fifo *f, _buffer *b)
{
	FAIL("Obsolete");
}

/*
  send_buffer uses mux_mutex and mux_cond, and that isn't what we want
*/
static inline void
_send_out_buffer(_fifo *f, _buffer *b)
{
	pthread_mutex_lock(&f->mutex);

	add_tail(&f->full, &b->node);

	pthread_mutex_unlock(&f->mutex);

	pthread_cond_broadcast(&f->cond);
}

static inline void
_empty_buffer(_fifo *f, _buffer *b)
{
	pthread_mutex_lock(&f->mutex);

	add_tail(&f->empty, &b->node);

	pthread_mutex_unlock(&f->mutex);

	pthread_cond_broadcast(&f->cond);
}



/*
 *  New stuff(tm)
 */

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
	long			_size;		// bytes
	// buffer->size != _buffer->size
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

	buffer *		buffers;
	int			num_buffers;
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
__recv_full_buffer(fifo *f)
// recv != recv in 1.1 local
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
__recv_empty_buffer(fifo *f)
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
