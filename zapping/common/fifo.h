/*
 *  Copyright (C) 1999-2001 Michael H. Schimek
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

/* $Id: fifo.h,v 1.16 2001-07-07 08:46:54 mschimek Exp $ */

#ifndef FIFO_H
#define FIFO_H

#include "list.h"
#include "threads.h"

#include <stdlib.h>

/*
  TODO:
  - Use the waiting variable
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
	int			rte_flags;	/* rte internal use */
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
		if (current) {
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
			b->refcount++;
			pthread_mutex_unlock(&(p->consumer.mutex));
			pthread_cond_broadcast(&(p->consumer.cond));
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

extern buffer * wait_full_buffer(fifo *f);

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

#define inc_buffer_refcount(b) (b->refcount++)

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

	/* FIXME: steal, be bad in a polite way */
	pthread_mutex_lock(&f->producer.mutex);

	b = (buffer *) rem_head(&f->empty);

	if (!b) {
	  pthread_mutex_unlock(&f->producer.mutex);

	  kill_zombies(f);

	  pthread_mutex_lock(&f->producer.mutex);

	  while (!(b = (buffer *) rem_head(&f->empty)))
	    pthread_cond_wait(&f->producer.cond,
			      &f->producer.mutex);
	} else
		/* otherwise producer cannot be cancelled without consumers */
		pthread_testcancel();

	pthread_mutex_unlock(&f->producer.mutex);

	b->refcount = 0;

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
	else
		b->refcount = 0;

	return b;
}

/*
 *  NEW STUFF
 */

typedef struct fifo2 fifo2;
typedef struct buffer2 buffer2;
typedef struct consumer consumer;
typedef struct producer producer;

struct buffer2 {
	node3 			node;		/* fifo->full/empty */
	fifo2 *			fifo;

	/*
	 *  These fields are used for the "virtual full queue".
	 *
	 *  consumer->next points to the next buffer in fifo->full
	 *  to be dequeued, NULL if all buffers have been consumed.
	 *  (b->refcount used to count the number of these
	 *   references but it turned out redundant.)
	 *
	 *  consumers is their number at send_full time, always > 0
	 *  because without consumers the buffer is instantly
	 *  send_emptied. We can't use fifo->consumers.members
	 *  because new consumers shall not dequeue old buffers.
	 *
	 *  dequeued and enqueued count the wait_full and send_empty
	 *  calls, respectively. A buffer is actually send_emptied
	 *  when enqueued >= consumers, and can be "recycled" when
	 *  dequeued == 0. enqueued > dequeued is already implied by
	 *  enqueued > consumers and no steady state.
	 *
	 *  For empty buffers b->refcount is n/a, consumers always 0 and
	 *  dequeued counts the wait_empty calls. enqueued is n/a
	 *  because send_full transfers the buffer immediately to
	 *  the full queue.
	 *
	 *  See rem_buffer for scheduled removal.
	 */
	int			consumers;

	int			dequeued;
	int			enqueued;

	bool			remove;

	/* Consumer read only, see send_full_buffer() */

	int			type;		/* application specific */
	int			offset;
	double			time;

	unsigned char *		data;		/* mandatory */
	ssize_t			used;

	int			error;
	char *			errstr;

	/* Owner private */

	node3			added;		/* fifo->buffers */

	unsigned char *		allocated;
	ssize_t			size;

	void			(* destroy)(buffer2 *);

	void *			user_data;
};

struct fifo2 {
	node3			node;		/* owner private */

	char			name[64];	/* for debug messages */

	mucon			pro, con;

	list3			full;		/* FIFO */
	list3			empty;		/* LIFO */

	list3			producers;
	list3			consumers;

	int			p_reentry;
	int			c_reentry;

	int			eof_count;	/* counts p->eof_sent */

	/* Owner private */

	mucon *			producer;	/* -> pro */
	mucon *			consumer;	/* -> con */

	list3			buffers;	/* add/rem_buffer */

	bool			unlink_full_buffers; /* default true */

	void			(* wait_empty)(fifo2 *);
	void			(* send_full)(producer *, buffer2 *);

	void			(* wait_full)(fifo2 *);
	void			(* send_empty)(consumer *, buffer2 *);

	bool			(* start)(fifo2 *);
	void			(* stop)(fifo2 *);

	void			(* destroy)(fifo2 *);

	void *			user_data;
};

struct producer {
	node3			node;		/* fifo->producers */
	fifo2 *			fifo;

	int			dequeued;	/* bookkeeping */
	bool			eof_sent;
};

struct consumer {
	node3			node;		/* fifo->consumers */
	fifo2 *			fifo;

	buffer2 *		next_buffer;	/* virtual pointer */
	int			dequeued;	/* bookkeeping */
};

/**
 * destroy_buffer:
 * @b: buffer2 *
 * 
 * Free all resources associated with the buffer. This is a
 * low-level function, don't call it for buffers which have
 * been added to a fifo. No op when @b is NULL.
 **/
static inline void
destroy_buffer(buffer2 *b)
{
	if (b && b->destroy)
		b->destroy(b);
}

extern buffer2 *		init_buffer2(buffer2 *, ssize_t);
extern buffer2 *		alloc_buffer(ssize_t);

/**
 * destroy_fifo:
 * @f: fifo2 *
 * 
 * Free all resources associated with the fifo, including all
 * buffers. Make sure no threads are using the fifo and no
 * producers or consumers can be added.
 *
 * Removing all producers and consumers before calling this
 * function is not necessary, for example when a consumer
 * aborted before detaching himself from the fifo.
 *
 * No op when @f is NULL.
 **/
static inline void
destroy_fifo(fifo2 *f)
{
	if (f && f->destroy)
		f->destroy(f);
}

/**
 * recv_full_buffer:
 * @c: consumer *
 * 
 * Dequeues the next buffer in production order from the consumer's fifo's
 * full queue. Remind callback (unbuffered) fifos do not fill automatically
 * but only when the consumer calls wait_full_buffer(). In this case
 * recv_full_buffer() returns a buffer only after unget_full_buffer() or
 * when the producer enqueued more than one full buffer at the last
 * wait_full_buffer().
 *
 * Buffers must be returned with send_empty_buffer() as soon as possible
 * for re-use by the producer, including those with buffer.used == 0
 * (end of stream) or buffer.used < 0 (error). You can dequeue more buffers
 * before returning and buffers need not be returned in order. All buffer
 * contents are read-only for consumers.
 *
 * None of the fifo functions depend on the identity of the calling thread
 * (thread_t), however we assume the consumer object is not shared. 
 *
 * Return value:
 * Buffer pointer, or NULL if the full queue is empty.
 **/
static inline buffer2 *
recv_full_buffer2(consumer *c)
{
	fifo2 *f = c->fifo;
	buffer2 *b;

	pthread_mutex_lock(&f->consumer->mutex);

	if ((b = c->next_buffer)->node.succ) {
		c->next_buffer = (buffer2 *) b->node.succ;
		b->dequeued++;
	}

	pthread_mutex_unlock(&f->consumer->mutex);

	c->dequeued++;

	return b;
}

extern buffer2 *		wait_full_buffer2(consumer *c);

/**
 * unget_full_buffer:
 * @c: consumer *
 * @b: buffer2 *
 * 
 * Put buffer @b, dequeued with wait_full_buffer() or recv_full_buffer()
 * and not yet returned with send_empty_buffer(), back on the full queue
 * of its fifo, to be dequeued again later. You can unget more than one
 * buffer, in reverse order of dequeuing, starting with the most
 * recently dequeued.
 **/
static inline void
unget_full_buffer2(consumer *c, buffer2 *b)
{
	fifo2 *f = c->fifo;

	/* Migration prohibited */
	assert(c->fifo == b->fifo);

	c->dequeued--;

	pthread_mutex_lock(&f->consumer->mutex);

	assert(c->next_buffer == (buffer2 *) b->node.succ);

	b->dequeued--;

	c->next_buffer = b;

	pthread_mutex_unlock(&f->consumer->mutex);
}

/**
 * send_empty_buffer:
 * @c: consumer *
 * @b: buffer *
 * 
 * Consumers call this function when done with a previously dequeued
 * full buffer. Dereferencing the buffer pointer after sending the
 * buffer is not permitted.
 **/
static inline void
send_empty_buffer2(consumer *c, buffer2 *b)
{
	/* Migration prohibited */
	assert(c->fifo == b->fifo);

	c->dequeued--;

	c->fifo->send_empty(c, b);
}

/**
 * recv_empty_buffer:
 * @p: producer *
 * 
 * Dequeues an empty buffer, no particular order, from the producer's fifo's
 * empty queue. Remind callback (unbuffered) fifos do not fill automatically
 * but only when the producer calls wait_empty_buffer().
 * 
 * Send filled buffers with send_full_buffer(). You can dequeue more buffers
 * before sending and buffers need not be sent in dequeuing order.
 *
 * None of the fifo functions depend on the identity of the calling thread
 * (thread_t), however we assume the producer object is not shared. 
 *
 * Return value:
 * Buffer pointer, or NULL if the empty queue is empty.
 **/
static inline buffer2 *
recv_empty_buffer2(producer *p)
{
	fifo2 *f = p->fifo;
	buffer2 *b;

	pthread_mutex_lock(&f->producer->mutex);

	b = PARENT(rem_head3(&f->empty), buffer2, node);

	pthread_mutex_unlock(&f->producer->mutex);

	b->dequeued = 1;
	p->dequeued++;

	return b;
}

extern buffer2 *		wait_empty_buffer2(producer *p);

/**
 * unget_empty_buffer:
 * @p: producer *
 * @b: buffer *
 * 
 * Put buffer @b, dequeued with wait_empty_buffer() or recv_empty_buffer()
 * and not yet enqueued with send_full_buffer(), back on the empty queue
 * of its fifo, to be dequeued again later. You can unget more than one
 * buffer, in any order, but not the same buffer twice.
 *
 * It may happen another producer grabs the ungot buffer, so a subsequent
 * recv_empty_buffer() will not necessarily succeed.
 **/
static inline void
unget_empty_buffer2(producer *p, buffer2 *b)
{
	fifo2 *f = p->fifo;

	/* Migration prohibited, don't use this to add buffers to the fifo */
	assert(p->fifo == b->fifo && b->dequeued == 1);

	p->dequeued--;
	b->dequeued = 0;

	pthread_mutex_lock(&f->producer->mutex);

	add_tail3(&f->empty, &b->node);

	pthread_mutex_unlock(&f->producer->mutex);
}

extern void			send_full_buffer2(producer *p, buffer2 *b);

extern void			rem_buffer(buffer2 *b);
extern bool			add_buffer(fifo2 *f, buffer2 *b);

extern int			init_buffered_fifo2(fifo2 *f, char *name, int num_buffers, ssize_t buffer_size);
extern int			init_callback_fifo2(fifo2 *f, char *name, void (* custom_wait_empty)(fifo2 *), void (* custom_send_full)(producer *, buffer2 *), void (* custom_wait_full)(fifo2 *), void (* custom_send_empty)(consumer *, buffer2 *), int num_buffers, ssize_t buffer_size);

extern void			rem_producer(producer *p);
extern producer *		add_producer(fifo2 *f, producer *p);

extern void			rem_consumer(consumer *c);
extern consumer	*		add_consumer(fifo2 *f, consumer *c);

#endif /* FIFO_H */
