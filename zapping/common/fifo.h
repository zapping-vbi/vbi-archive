/*
 *  Copyright (C) 1999-2001 Michael H. Schimek
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
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

/* $Id: fifo.h,v 1.32 2004-12-07 17:30:39 mschimek Exp $ */

#ifndef FIFO_H
#define FIFO_H

#include <stdlib.h>
#include <sys/time.h>

#include "list.h"
#include "threads.h"

typedef struct zf_fifo zf_fifo;
typedef struct zf_buffer zf_buffer;
typedef struct zf_consumer zf_consumer;
typedef struct zf_producer zf_producer;

struct zf_buffer {
	struct node 		node;		/* fifo->full/empty */
	zf_fifo *		fifo;

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

	z_bool			remove;

	/* Consumer read only, see send_full_buffer() */

	int			type;		/* application specific */
	int			offset;
	double			time;

	unsigned char *		data;		/* mandatory */
	ssize_t			used;

	int			error;		/* copy of errno */
	char *			errorstr;	/* gettext()ized, may be NULL */

	/* Owner private */

	struct node		added;		/* fifo->buffers */

	unsigned char *		allocated;
	ssize_t			size;

	void			(* destroy)(zf_buffer *);

	void *			user_data;

	/* XXX? */
	int			rte_flags;	/* rte internal use */
};

struct zf_fifo {
	struct node		node;		/* owner private */

	char			name[64];	/* for debug messages */

	mucon			pro, con;

	list			full;		/* FIFO */
	list			empty;		/* LIFO */

	list			producers;
	list			consumers;

	int			p_reentry;
	int			c_reentry;

	int			eof_count;	/* counts p->eof_sent */

	/* Owner private */

	mucon *			producer;	/* -> pro */
	mucon *			consumer;	/* -> con */

	list			buffers;	/* add/rem_buffer */

	z_bool			unlink_full_buffers; /* default true */

	void			(* wait_empty)(zf_fifo *);
	void			(* send_full)(struct zf_producer *, zf_buffer *);

	void			(* wait_full)(zf_fifo *);
	void			(* send_empty)(struct zf_consumer *, zf_buffer *);

	z_bool			(* start)(zf_fifo *);
	void			(* stop)(zf_fifo *);

	void			(* destroy)(zf_fifo *);

	void *			user_data;

	zf_buffer *		(* alloc_buffer)(ssize_t);

	void			(* buffer_done)(zf_fifo *, zf_buffer *);
};

struct zf_producer {
	struct node		node;		/* fifo->producers */
	zf_fifo *		fifo;
	int			dequeued;	/* bookkeeping */
	z_bool			eof_sent;
};

struct zf_consumer {
	struct node		node;		/* fifo->consumers */
	zf_fifo *		fifo;
	zf_buffer *		next_buffer;	/* virtual pointer */
	int			dequeued;	/* bookkeeping */
};

/**
 * current_time:
 * 
 * Buffer time is usually noted in seconds TOD. Unfortunately too
 * many interfaces provide no interrupt time so we have no choice
 * but to query the system clock.
 *
 * Return value:
 * gettimeofday() in seconds and fractions, double.
 **/
static inline double
zf_current_time(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);

	return tv.tv_sec + tv.tv_usec * (1 / 1e6);
	/* rezp. mult is faster, not auto optimized for accuracy */
}

/**
 * destroy_buffer:
 * @b: buffer *
 * 
 * Free all resources associated with the buffer. This is a
 * low-level function, don't call it for buffers which have
 * been added to a fifo. No op when @b is %NULL.
 **/
static inline void
zf_destroy_buffer(zf_buffer *b)
{
	if (b && b->destroy)
		b->destroy(b);
}

extern zf_buffer *	zf_init_buffer(zf_buffer *, ssize_t);
extern zf_buffer *	zf_alloc_buffer(ssize_t);

/**
 * destroy_fifo:
 * @f: fifo *
 * 
 * Free all resources associated with the fifo, including all
 * buffers. Make sure no threads are using the fifo and no
 * producers or consumers can be added.
 *
 * Removing all producers and consumers before calling this
 * function is not necessary, for example when a consumer
 * aborted before detaching himself from the fifo.
 *
 * No op when @f is %NULL.
 **/
static inline void
zf_destroy_fifo(zf_fifo *f)
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
 * Buffer pointer, or %NULL if the full queue is empty.
 **/
static inline zf_buffer *
zf_recv_full_buffer(zf_consumer *c)
{
	zf_fifo *f = c->fifo;
	zf_buffer *b;

	pthread_mutex_lock(&f->consumer->mutex);

	if ((b = c->next_buffer)->node.succ) {
		c->next_buffer = (zf_buffer *) b->node.succ;
		b->dequeued++;
		c->dequeued++;
	} else
		b = NULL;

	pthread_mutex_unlock(&f->consumer->mutex);

	return b;
}

extern zf_buffer *	zf_wait_full_buffer(zf_consumer *c);
extern zf_buffer *	zf_wait_full_buffer_timeout(zf_consumer *c, struct
						 timespec *timeout);

/**
 * unget_full_buffer:
 * @c: consumer *
 * @b: buffer *
 * 
 * Put buffer @b, dequeued with wait_full_buffer() or recv_full_buffer()
 * and not yet returned with send_empty_buffer(), back on the full queue
 * of its fifo, to be dequeued again later. You can unget more than one
 * buffer, in reverse order of dequeuing, starting with the most
 * recently dequeued.
 **/
static inline void
zf_unget_full_buffer(zf_consumer *c, zf_buffer *b)
{
	zf_fifo *f = c->fifo;

	/* Migration prohibited */
	assert(c->fifo == b->fifo);

	c->dequeued--;

	pthread_mutex_lock(&f->consumer->mutex);

	assert(c->next_buffer == (zf_buffer *) b->node.succ);

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
zf_send_empty_buffer(zf_consumer *c, zf_buffer *b)
{
	/* Migration prohibited */
	assert(c->fifo == b->fifo);

	c->dequeued--;

	c->fifo->send_empty(c, b);
}

/* XXX rethink */
extern void			zf_send_empty_buffered(zf_consumer *c, zf_buffer *b);

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
 * Buffer pointer, or %NULL if the empty queue is empty.
 **/
static inline zf_buffer *
zf_recv_empty_buffer(zf_producer *p)
{
	zf_fifo *f = p->fifo;
	zf_buffer *b;

	pthread_mutex_lock(&f->producer->mutex);

	b = PARENT(rem_head(&f->empty), zf_buffer, node);

	pthread_mutex_unlock(&f->producer->mutex);

	if (b) {
		b->dequeued = 1;
		p->dequeued++;
	}

	return b;
}

extern zf_buffer *		zf_wait_empty_buffer(zf_producer *p);

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
zf_unget_empty_buffer(zf_producer *p, zf_buffer *b)
{
	zf_fifo *f = p->fifo;

	/* Migration prohibited, don't use this to add buffers to the fifo */
	assert(p->fifo == b->fifo && b->dequeued == 1);

	p->dequeued--;
	b->dequeued = 0;

	pthread_mutex_lock(&f->producer->mutex);

	add_tail(&f->empty, &b->node);

	pthread_mutex_unlock(&f->producer->mutex);
}

extern void			zf_send_full_buffer(zf_producer *p, zf_buffer *b);

extern void			zf_rem_buffer(zf_buffer *b);
extern z_bool			zf_add_buffer(zf_fifo *f, zf_buffer *b);

extern int			zf_init_buffered_fifo(zf_fifo *f, const char *name, int num_buffers, ssize_t buffer_size);
extern int			zf_init_callback_fifo(zf_fifo *f, const char *name, void (* custom_wait_empty)(zf_fifo *), void (* custom_send_full)(zf_producer *, zf_buffer *), void (* custom_wait_full)(zf_fifo *), void (* custom_send_empty)(zf_consumer *, zf_buffer *), int num_buffers, ssize_t buffer_size);

extern void			zf_rem_producer(zf_producer *p);
extern zf_producer *		zf_add_producer(zf_fifo *f, zf_producer *p);

extern void			zf_rem_consumer(zf_consumer *c);
extern zf_consumer *		zf_add_consumer(zf_fifo *f, zf_consumer *c);

/* XXX TBD */
/* start only *after* adding a consumer? */
/* mp-fifos? */
static inline z_bool
zf_start_fifo(zf_fifo *f)
{
	return f->start(f);
}

#endif /* FIFO_H */
