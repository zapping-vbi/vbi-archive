/*
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

/* $Id: fifo.h,v 1.12 2000-12-15 00:14:19 garetxe Exp $ */

#ifndef FIFO_H
#define FIFO_H

#include "list.h"
#include "types.h"
#include "log.h"
#include "threads.h"

/*
  TODO:
  - pthread_* rewrite:
    + Figure out some way to avoid blocking all consumers when accesing
      one's full list [rwlock, i believe, but no man page]
    + Use thread-specific data [pthread_key]
  - Store some buffers in fifos without consumers (producer-side).
  - Killing dead consumers
  - Stealing buffers from slow consumers
  - Interface changes:
    + Callbacks: send_full not allowed
    + Consumer mucons no longer pertinent.
    + We should remove wait_full callback in favour of something like
	fill_buffer, and do the fifo managing ourselves, not the producer.
*/

/*
  Refcount of the buffer:
  INC() -> The buffer has been appended to a consumer's full list
  DEC() -> The buffer has been send_empty'ed.
  == 0  -> The buffer is sent empty
*/
typedef struct {
	node 			node;
	int			index;
	int			refcount; /* 0: last unref */

	/* prod. r/w, cons. r/o */

	int			type;
	int			offset;
	double			time;

	unsigned char *		data;
	long			used;		/* bytes */

	/* owner private */

	unsigned char *		allocated;	/* by init_fifo etc */
	long			size;		/* bytes */

	void *			user_data;
} buffer;

typedef struct {
	pthread_t		owner;
	list			full;
	mucon			consumer;
} coninfo;

typedef struct _fifo {
	mucon			producer;

	coninfo *		consumers;
	int			num_consumers;
	pthread_mutex_t		consumers_mutex;
	list			empty;		/* LIFO */

	buffer *		(* wait_full)(struct _fifo *);
	void			(* send_empty)(struct _fifo *, buffer *);
	buffer *		(* wait_empty)(struct _fifo *);
	void			(* send_full)(struct _fifo *, buffer *);

	bool			(* start)(struct _fifo *);
	void			(* uninit)(struct _fifo *);

	/* owner private */

	buffer *		buffers;
	int			num_buffers;

	void *			user_data;
} fifo;

extern bool	init_buffer(buffer *b, int size);
extern void	uninit_buffer(buffer *b);
extern int	alloc_buffer_vec(buffer **bpp, int num_buffers, int buffer_size);
extern void	free_buffer_vec(buffer *bvec, int num_buffers);

extern int	init_buffered_fifo(fifo *f, mucon *consumer, int num_buffers, int buffer_size);
extern int	init_callback_fifo(fifo *f, buffer * (* wait_full)(fifo *), void (* send_empty)(fifo *, buffer *), buffer * (* wait_empty)(fifo *), void (* send_full)(fifo *, buffer *), int num_buffers, int buffer_size);
extern int	num_buffers_queued(fifo *f);
extern void	remove_consumer(fifo *f);

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
query_consumer(fifo *f, list **full, mucon **consumer)
{
	pthread_t current_thread = pthread_self();
	int i;

	for (i=0; i<f->num_consumers;i++)
		if (pthread_equal(f->consumers[i].owner,
				  current_thread)) {
			if (full)
				*full = &(f->consumers[i].full);
			if (consumer)
				*consumer = &(f->consumers[i].consumer);
			return;
		}
	
	/* nonexistant, create a new entry for the current thread */
	f->consumers = (coninfo*)
		realloc(f->consumers, sizeof(coninfo)*(i+1));
	
	memset(&(f->consumers[i]), 0, sizeof(coninfo));
	f->consumers[i].owner = current_thread;
	mucon_init(&(f->consumers[i].consumer));
	if (full)
		*full = &(f->consumers[i].full);
	if (consumer)
		*consumer = &(f->consumers[i].consumer);
	f->num_consumers++;
}

static inline bool
start_fifo(fifo *f)
{
	return f->start(f);
}

/*
    Stop and destroy fifo.
 */
static inline void
uninit_fifo(fifo *f)
{
	f->uninit(f);
}

static inline void
send_full_buffer(fifo *f, buffer *b)
{
	/* FIXME: A callback doesn't make sense for send_full now */
	f->send_full(f, b);
}

static inline void
unget_full_buffer(fifo *f, buffer *b)
{
	list *full; mucon *consumer;

	pthread_mutex_lock(&f->consumers_mutex);
	query_consumer(f, &full, &consumer);
	pthread_mutex_lock(&consumer->mutex);

	add_head(full, &b->node);

	pthread_mutex_unlock(&consumer->mutex);
	pthread_cond_broadcast(&consumer->cond);
	pthread_mutex_unlock(&f->consumers_mutex);
}

static inline buffer *
wait_full_buffer(fifo *f)
{
	buffer *b;
	list *full; mucon *consumer;

	pthread_mutex_lock(&f->consumers_mutex);
	query_consumer(f, &full, &consumer);

	pthread_mutex_lock(&consumer->mutex);
	b = (buffer*) rem_head(full);
	pthread_mutex_unlock(&consumer->mutex);

	if ((!b) && (f->wait_full)) {
		/* Send a new buffer to all consumers */
		while (!b)
			b = f->wait_full(f);
		send_full_buffer(f, b);
		b = (buffer*) rem_head(full); /* Should always work */
	} else if (!b) {
		pthread_mutex_unlock(&f->consumers_mutex);
		pthread_mutex_lock(&consumer->mutex);

		while (!(b = (buffer *) rem_head(full)))
			pthread_cond_wait(&consumer->cond,
					  &consumer->mutex);
		
		pthread_mutex_unlock(&consumer->mutex);
		pthread_mutex_lock(&f->consumers_mutex);
	}

	pthread_mutex_unlock(&f->consumers_mutex);

	return b;
}

static inline buffer *
recv_full_buffer(fifo *f)
{
	buffer *b;
	list *full; mucon *consumer;

	pthread_mutex_lock(&f->consumers_mutex);
	query_consumer(f, &full, &consumer);

	pthread_mutex_lock(&consumer->mutex);
	b = (buffer*) rem_head(full);
	pthread_mutex_unlock(&consumer->mutex);

	if ((!b) && (f->wait_full)) {
		/* Send a new buffer to all consumers */
		send_full_buffer(f, f->wait_full(f));
		b = (buffer*) rem_head(full); /* Should always work */
	} else if (!b) {
		pthread_mutex_lock(&consumer->mutex);

		b = (buffer *) rem_head(full);
		
		pthread_mutex_unlock(&consumer->mutex);
	}

	pthread_mutex_unlock(&f->consumers_mutex);

	return b;
}

static inline void
send_empty_buffer(fifo *f, buffer *b)
{
	if ((--(b->refcount)) <= 0 )
		f->send_empty(f, b);
}

static inline buffer *
wait_empty_buffer(fifo *f)
{
	buffer *b;

	if (f->wait_empty)
		return (b = (buffer *) rem_head(&f->empty)) ?
			b : f->wait_empty(f);

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
		return (b = (buffer *) rem_head(&f->empty)) ?
			b : f->wait_empty(f);

	pthread_mutex_lock(&f->producer.mutex);

	b = (buffer *) rem_head(&f->empty);

	pthread_mutex_unlock(&f->producer.mutex);

	return b;
}

#endif /* FIFO_H */
