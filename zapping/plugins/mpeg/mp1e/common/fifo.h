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

/* $Id: fifo.h,v 1.16 2001-02-22 14:15:51 mschimek Exp $ */

#ifndef FIFO_H
#define FIFO_H

#include <stdlib.h>
#include "list.h"
#include "types.h"
#include "threads.h"

/*
  TODO:
  - Use the occupancy variable
    + Killing dead consumers
    + Stealing buffers from slow consumers
  
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

typedef struct _fifo fifo;

typedef struct {
	list			full;
	int			occupancy; /* of full */
	mucon			consumer;
	fifo			*f;
	int			index; /* in consumers */
} coninfo;

struct _fifo {
	mucon			producer;
	mucon			mbackup;

	/* Consumers */
	coninfo *		consumers;
	pthread_rwlock_t	consumers_rwlock;
	int			num_consumers;
	pthread_key_t		consumer_key;

	/* Producer */
	list			empty;		/* LIFO */
	list			backup;		/* FIFO */

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
};

extern bool	init_buffer(buffer *b, int size);
extern void	uninit_buffer(buffer *b);
extern int	alloc_buffer_vec(buffer **bpp, int num_buffers, int buffer_size);
extern void	free_buffer_vec(buffer *bvec, int num_buffers);

extern int	init_buffered_fifo(fifo *f, char *name, mucon *consumer, int num_buffers, int buffer_size);
extern int	init_callback_fifo(fifo *f, char *name, buffer * (* wait_full)(fifo *), void (* send_empty)(fifo *, buffer *), buffer * (* wait_empty)(fifo *), void (* send_full)(fifo *, buffer *), int num_buffers, int buffer_size);
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

static inline coninfo*
query_consumer(fifo *f)
{
	int i;
	coninfo *current=pthread_getspecific(f->consumer_key);
	buffer *b;

	if (current)
		return current;
	
	/* nonexistant, create a new entry for the current thread */
	/* no recursive rwlocks, this is non-atomic */
	pthread_rwlock_unlock(&f->consumers_rwlock);
	pthread_rwlock_wrlock(&f->consumers_rwlock);
	/*
	  there's the theoretical possibility that num_consumers changes
	  between the unlock and the wrlock.
	*/
	i = f->num_consumers;
	f->consumers = (coninfo*)
		realloc(f->consumers, sizeof(coninfo)*(i+1));
	
	memset(&(f->consumers[i]), 0, sizeof(coninfo));
	f->consumers[i].index = i;
	f->consumers[i].f = f;
	pthread_setspecific(f->consumer_key, &(f->consumers[i]));
	mucon_init(&(f->consumers[i].consumer));
	f->num_consumers++;
	/* Send the kept buffers to this consumer */
	pthread_mutex_lock(&f->mbackup.mutex);
	while ((b = (buffer*) rem_head(&f->backup))) {
		add_tail(&(f->consumers[i].full), &b->node);
		f->consumers[i].occupancy++;
	}
	pthread_mutex_unlock(&f->mbackup.mutex);

	pthread_rwlock_unlock(&f->consumers_rwlock);
	pthread_rwlock_rdlock(&f->consumers_rwlock);

	return &(f->consumers[i]);
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
	coninfo *consumer;

	pthread_rwlock_rdlock(&f->consumers_rwlock);
	consumer = query_consumer(f);
	pthread_mutex_lock(&consumer->consumer.mutex);

	add_head(&consumer->full, &b->node);
	consumer->occupancy++;

	pthread_mutex_unlock(&consumer->consumer.mutex);
	pthread_cond_broadcast(&consumer->consumer.cond);
	pthread_rwlock_unlock(&f->consumers_rwlock);
}

static inline buffer *
wait_full_buffer(fifo *f)
{
	buffer *b;
	coninfo *consumer;

	pthread_rwlock_rdlock(&f->consumers_rwlock);
	consumer = query_consumer(f);

	pthread_mutex_lock(&consumer->consumer.mutex);
	b = (buffer*) rem_head(&consumer->full);
	if (b)
		consumer->occupancy--;
	pthread_mutex_unlock(&consumer->consumer.mutex);

	if ((!b) && (f->wait_full)) {
		/* Send a new buffer to all consumers */
		while (!b)
			b = f->wait_full(f);
		send_full_buffer(f, b);
		b = (buffer*) rem_head(&consumer->full);
		consumer->occupancy--;
	} else if (!b) {
		pthread_mutex_lock(&consumer->consumer.mutex);

		while (!(b = (buffer *) rem_head(&consumer->full)))
			pthread_cond_wait(&consumer->consumer.cond,
					  &consumer->consumer.mutex);
		consumer->occupancy--;
		pthread_mutex_unlock(&consumer->consumer.mutex);
	}

	pthread_rwlock_unlock(&f->consumers_rwlock);

	return b;
}

static inline buffer *
recv_full_buffer(fifo *f)
{
	buffer *b;
	coninfo *consumer;

	pthread_rwlock_rdlock(&f->consumers_rwlock);
	consumer = query_consumer(f);

	pthread_mutex_lock(&consumer->consumer.mutex);
	b = (buffer*) rem_head(&consumer->full);
	if (b)
		consumer->occupancy--;
	pthread_mutex_unlock(&consumer->consumer.mutex);

	if ((!b) && (f->wait_full)) {
		/* Send a new buffer to all consumers */
		b = f->wait_full(f);
		if (b) {
			send_full_buffer(f, b);
			b = (buffer*) rem_head(&consumer->full);
			consumer->occupancy--;
		}
	}

	pthread_rwlock_unlock(&f->consumers_rwlock);

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
