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

/* $Id: fifo.h,v 1.9 2001-05-05 23:45:06 garetxe Exp $ */

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
	node			node; /* in f->consumers */

	list			full; /* buffers ready for use */
	int			waiting; /* in full */

	pthread_t		owner; /* creator */

	mucon			consumer;
	fifo			*f; /* owner */

	int			killable; /* defaults to 1 */
	int			zombie; /* this was killed, but it
					   stays with us a bit
					   longer... */
} coninfo;

struct _fifo {
	mucon			producer;
	mucon			mbackup;

	char			name[64];

	/* Consumers */
	list			consumers;
	pthread_rwlock_t	consumers_rwlock; /* wrlock on
						     consumers change */

	list			limbo; /* zombies stay here until they
					definitively die */
	pthread_mutex_t		limbo_mutex;

	pthread_key_t		consumer_key;

	/* Producer */
	list			empty;		/* LIFO */
	list			backup;		/* FIFO */

	buffer *		(* wait_full)(struct _fifo *);
	void			(* send_empty)(struct _fifo *, buffer *);

	bool			(* start)(struct _fifo *);
	void			(* uninit)(struct _fifo *);

	/* owner private */

	buffer *		buffers;
	int			num_buffers;
	void			(* send_full)(struct _fifo *, buffer *);

	void *			user_data;
};

extern bool	init_buffer(buffer *b, int size);
extern void	uninit_buffer(buffer *b);
extern int	alloc_buffer_vec(buffer **bpp, int num_buffers, int buffer_size);
extern void	free_buffer_vec(buffer *bvec, int num_buffers);

extern int	init_buffered_fifo(fifo *f, char *name, int num_buffers, int buffer_size);
extern int	init_callback_fifo(fifo *f, char *name, buffer * (* wait_full)(fifo *), void (* send_empty)(fifo *, buffer *), int num_buffers, int buffer_size);
extern coninfo *create_consumer(fifo *f);
/* If on, and buffers aren't processed, the consumer will be
   automatically removed from the fifo's consumer list */
extern void set_killable(fifo *f, int on);
extern void kill_zombies(fifo *f);

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
	coninfo *current=pthread_getspecific(f->consumer_key);

	if (current && !current->zombie)
		return current;
	else {
		if (current && current->zombie) {
			/* pending destroy sequence */
			pthread_mutex_lock(&f->limbo_mutex);
			unlink_node(&(f->limbo), (node*)current);
			pthread_mutex_unlock(&f->limbo_mutex);
			free(current);
			pthread_setspecific(f->consumer_key, NULL);
		}

		return create_consumer(f);
	}
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
	consumer->waiting++;

	pthread_mutex_unlock(&consumer->consumer.mutex);
	pthread_cond_broadcast(&consumer->consumer.cond);
	pthread_rwlock_unlock(&f->consumers_rwlock);
}

/**
 * f->consumers_rwlock must be locked.
 * consumer is the consumer that got the buffer and wishes to propagate,
 * the buffer isn't put in his full list.
 */
static inline void
propagate_buffer(fifo *f, buffer *b, coninfo *consumer)
{
	coninfo *p = (coninfo*)f->consumers.head;

	while (p) {
		if (p != consumer) {
			pthread_mutex_lock(&(p->consumer.mutex));
			add_tail(&(p->full), &b->node);
			p->waiting++;
			pthread_mutex_unlock(&(p->consumer.mutex));
			pthread_cond_broadcast(&(p->consumer.cond));
			b->refcount++;
		}
		p = (coninfo*)(p->node.next);
	}

	/* FIXME: better mbackup */
	if (empty_list(&f->consumers)) {
		/* store it for later use */
		b->refcount = 1;
		pthread_mutex_lock(&f->mbackup.mutex);
		add_tail(&f->backup, &b->node);
		pthread_mutex_unlock(&f->mbackup.mutex);
	}
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

	if ((b) || (f->wait_full))
		pthread_mutex_unlock(&consumer->consumer.mutex);

	if (b)
		consumer->waiting--;
	else if ((!b) && (f->wait_full)) {
		/* Wait for a new buffer */
		while (!b)
			b = f->wait_full(f);
		b->refcount = 1;
		propagate_buffer(f, b, consumer);
	} else if (!b) {
		while (!(b = (buffer *) rem_head(&consumer->full)))
			pthread_cond_wait(&consumer->consumer.cond,
					  &consumer->consumer.mutex);
		consumer->waiting--;
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
	fprintf(stderr, "query (%p)\n", f->wait_full);
	consumer = query_consumer(f);
	fprintf(stderr, "queried (%p, %p)\n", consumer, f->consumers.head);
	pthread_mutex_lock(&consumer->consumer.mutex);
	fprintf(stderr, "locked\n");
	b = (buffer*) rem_head(&consumer->full);
	fprintf(stderr, "got %p\n", b);
	if (b)
		consumer->waiting--;
	pthread_mutex_unlock(&consumer->consumer.mutex);

	if ((!b) && (f->wait_full)) {
		b = f->wait_full(f);
		if (b) {
			b->refcount = 1;
			propagate_buffer(f, b, consumer);
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

	pthread_rwlock_rdlock(&f->consumers_rwlock);
	if (empty_list(&f->consumers) &&
	    empty_list(&f->empty)) {
		b = (buffer*) rem_head(&f->backup);
		if (b) {
			b->refcount = 0;
			send_empty_buffer(f, b);
		}
	}
	pthread_rwlock_unlock(&f->consumers_rwlock);

	/* FIXME: steal, kill, be bad */
	pthread_mutex_lock(&f->producer.mutex);

	b = (buffer *) rem_head(&f->empty);

	if (!b) {
	  pthread_mutex_unlock(&f->producer.mutex);

	  kill_zombies(f);

	  pthread_mutex_lock(&f->producer.mutex);

	  while (!(b = (buffer *) rem_head(&f->empty)))
	    pthread_cond_wait(&f->producer.cond,
			      &f->producer.mutex);
	}

	pthread_mutex_unlock(&f->producer.mutex);

	return b;
}

static inline buffer *
recv_empty_buffer(fifo *f)
{
	buffer *b;

	pthread_rwlock_rdlock(&f->consumers_rwlock);
	if (empty_list(&f->consumers) &&
	    empty_list(&f->empty)) {
		b = (buffer*) rem_head(&f->backup);
		if (b) {
			b->refcount = 0;
			send_empty_buffer(f, b);
		}
	}
	pthread_rwlock_unlock(&f->consumers_rwlock);

	pthread_mutex_lock(&f->producer.mutex);

	b = (buffer *) rem_head(&f->empty);

	pthread_mutex_unlock(&f->producer.mutex);

	if (!b)
	  kill_zombies(f);

	return b;
}

#endif /* FIFO_H */
