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

/* $Id: fifo.c,v 1.40 2004-09-10 04:58:51 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include "fifo.h"
#include "alloc.h"
#include "math.h"

#ifndef CACHE_LINE
#define CACHE_LINE 32
#endif

#ifndef HAVE_PROGRAM_INVOCATION_NAME
extern char *program_invocation_name;
extern char *program_invocation_short_name;
#endif

#define _pthread_mutex_lock pthread_mutex_lock
#define _pthread_mutex_unlock pthread_mutex_unlock

static char *
addr2line(void *addr)
{
	static char buf[256];
	FILE *stream;
	char *d;

	snprintf(buf, 255, "addr2line -Ce \"%s\" 0x%lx",
		program_invocation_name, (long) addr);

	if (!(stream = popen(buf, "r")))
		return NULL;

	fgets(buf, sizeof(buf) - 1, stream);

	pclose(stream);

	if (!(d = strchr(buf, '\n')))
		return NULL;
	*d = 0;

	if (buf[0] == 0 || buf[0] == '?')
		return NULL;

	return buf;
}

void
asserts_fail(const char *assertion, const char *file, unsigned int line,
	     const char *function, void *caller)
{
	char *at = addr2line(caller);

	if (at)
		fprintf(stderr, "%s: %s:%u: %s called from %s: Assertion \"%s\" failed\n",
			program_invocation_short_name, file, line,
			function, at, assertion);
	else
		fprintf(stderr, "%s: %s:%u: %s: Assertion \"%s\" failed\n",
			program_invocation_short_name, file, line,
			function, assertion);
	abort();
}

/*
 *  Buffer
 */

static z_bool
nop(void)
{
	return TRUE;
}

static void
uninit_buffer(zf_buffer *b)
{
	if (b->allocated)
		free_aligned(b->allocated);

	b->allocated = NULL;
	b->size = 0;
}

/**
 * init_buffer:
 * @b: zf_buffer *
 * @size: buffer memory to be allocated, in bytes < SSIZE_MAX.
 * 
 * Initialize a buffer structure erasing all prior
 * contents, and allocate buffer memory if size > 0.
 * destroy_buffer() frees all resources associated
 * with the buffer. You may override b->destroy to
 * add your own cleanup functions.
 * 
 * Return value: 
 * The buffer pointer, or %NULL if the allocation failed.
 **/
zf_buffer *
zf_init_buffer(zf_buffer *b, ssize_t size)
{
	ssize_t page_size = (ssize_t) sysconf(_SC_PAGESIZE);

	memset(b, 0, sizeof(zf_buffer));

	b->destroy = (void (*)(zf_buffer *)) nop;

	if (size > 0) {
		b->data =
		b->allocated = (unsigned char *)
			calloc_aligned((size_t) size,
				       (unsigned int)((size < page_size) ?
						      CACHE_LINE : page_size));

		if (!b->allocated)
			return NULL;

		b->destroy = uninit_buffer;
		b->size = size;
	}

	return b;
}

static void
free_buffer(zf_buffer *b)
{
	uninit_buffer(b);

	free(b);
}

/**
 * alloc_buffer:
 * @size: buffer memory to be allocated, in bytes < %SSIZE_MAX.
 * 
 * Allocate a buffer structure, and buffer memory if
 * size > 0. You must call destroy_buffer() to free all
 * resources associated with the buffer. You may override
 * b->destroy to add your own cleanup functions.
 * 
 * Return value: 
 * Buffer pointer, or %NULL if any allocation failed.
 **/
zf_buffer *
zf_alloc_buffer(ssize_t size)
{
	zf_buffer *b;

	if (!(b = (zf_buffer *) malloc(sizeof(zf_buffer))))
		return NULL;

	if (!zf_init_buffer(b, size)) {
		free(b);
		return NULL;
	}

	if (size > 0)
		b->destroy = free_buffer;
	else
		b->destroy = (void (*)(zf_buffer *)) free;

	return b;
}

/*
 *  Fifo
 */

/*
 *  To avoid a deadlock, two threads holding a resource
 *  the opposite wants to lock.
 */
static inline void
mutex_bi_lock(pthread_mutex_t *m1, pthread_mutex_t *m2)
{
	for (;;) {
		_pthread_mutex_lock(m1);
		if (pthread_mutex_trylock(m2) == 0)
			break;
		_pthread_mutex_unlock(m1);
		swap(m1, m2);
	}
}

static inline void
mutex_co_lock(pthread_mutex_t *m1, pthread_mutex_t *m2)
{
	while (pthread_mutex_trylock(m2) != 0) {
		_pthread_mutex_unlock(m1);
		swap(m1, m2);
		_pthread_mutex_lock(m1);
	}
}

static void
dead_fifo(zf_fifo *f)
{
	char *at = addr2line(__builtin_return_address(0));

	if (at)
		fprintf(stderr, "Invalid fifo %p (%s) called at %s\n", f, f->name, at);
	else
		fprintf(stderr, "Invalid fifo %p (%s) called at %p\n",
			f, f->name, __builtin_return_address(0));

	signal(SIGABRT, SIG_DFL);

	abort();
}

static void
dead_producer(zf_producer *p)
{
	zf_fifo *f = p->fifo;
	char *at = addr2line(__builtin_return_address(0));

	if (at)
		fprintf(stderr, "Invalid fifo %p (%s) called by producer %p at %s\n",
			f, f->name, p, at);
	else
		fprintf(stderr, "Invalid fifo %p (%s) called by producer %p at %p\n",
			f, f->name, p, __builtin_return_address(0));

	signal(SIGABRT, SIG_DFL);

	abort();
}

static void
dead_consumer(zf_consumer *c)
{
	zf_fifo *f = c->fifo;
	char *at = addr2line(__builtin_return_address(0));

	if (at)
		fprintf(stderr, "Invalid fifo %p (%s) called by consumer "
			"%p at %s, terminating.\n", f, f->name, c, at);
	else
		fprintf(stderr, "Invalid fifo %p (%s) called by consumer "
			"%p at %p, terminating.\n", f, f->name, c,
			__builtin_return_address(0));

	/*
	 *  A sane consumer should never hold a mutex
	 *  when calling fifo functions, so we won't block
	 *  joining other consumers or producers.
	 */

	pthread_exit(0);
}

static void
uninit_fifo(zf_fifo *f)
{
	node *n;

	f->destroy    = (void (*)(zf_fifo *)) dead_fifo;

	f->wait_empty = (void (*)(zf_fifo *)) dead_fifo;
	f->send_full  = (void (*)(zf_producer *, zf_buffer *)) dead_producer;

	f->wait_full  = (void (*)(zf_fifo *)) dead_fifo;
	f->send_empty = (void (*)(zf_consumer *, zf_buffer *)) dead_consumer;

	f->start      = (z_bool (*)(zf_fifo *)) dead_fifo;
	f->stop       = (void (*)(zf_fifo *)) dead_fifo;

	f->alloc_buffer = NULL;

	while ((n = rem_tail(&f->buffers)))
		zf_destroy_buffer(PARENT(n, zf_buffer, added));

	/* destroy_list(&f->buffers); */

	mucon_destroy(&f->pro);
	mucon_destroy(&f->con);
	/*
	destroy_list(&f->full);
	destroy_list(&f->empty);
	destroy_list(&f->producers);
	destroy_list(&f->consumers);
	*/
}

/**
 * wait_full_buffer:
 * @c: zf_consumer *
 * 
 * Suspends execution of the calling thread until a full buffer becomes
 * available for consumption. Otherwise identical to recv_full_buffer().
 *
 * Return value:
 * Buffer pointer, never %NULL.
 **/
zf_buffer *
zf_wait_full_buffer(zf_consumer *c)
{
	zf_fifo *f = c->fifo;
	zf_buffer *b;

	_pthread_mutex_lock(&f->consumer->mutex);

	while (!(b = c->next_buffer)->node.succ) {
		if (f->c_reentry++ || !f->wait_full) {
			/* Free resources which may block other consumers */
			pthread_cleanup_push((void (*)(void *))
				_pthread_mutex_unlock, &f->consumer->mutex);
			pthread_cond_wait(&f->consumer->cond, &f->consumer->mutex);
			pthread_cleanup_pop(0);
		} else {
			_pthread_mutex_unlock(&f->consumer->mutex);
			f->wait_full(f);
			_pthread_mutex_lock(&f->consumer->mutex);
		}

		f->c_reentry--;
	}

	c->next_buffer = PARENT(b->node.succ, zf_buffer, node);

	b->dequeued++;

	_pthread_mutex_unlock(&f->consumer->mutex);

	c->dequeued++;

//	fprintf(stderr, "WFB %s b=%p dq=%d\n", f->name, b, c->dequeued);

	return b;
}

/**
 * wait_full_buffer_timeout:
 * @c: zf_consumer *
 * @timeout: struct timespec *
 * 
 * Suspends execution of the calling thread until a full buffer becomes
 * available for consumption or the timeout is reached. Identical
 * behaviour to wait_full_buffer otherwise.
 *
 * Return value:
 * Buffer pointer, or %NULL if the timeout was reached.
 **/
zf_buffer *
zf_wait_full_buffer_timeout(zf_consumer *c, struct timespec *timeout)
{
	zf_fifo *f = c->fifo;
	zf_buffer *b;
	int err = 0;

	_pthread_mutex_lock(&f->consumer->mutex);

	while (!(b = c->next_buffer)->node.succ) {
		if (f->c_reentry++ || !f->wait_full) {
			/* Free resources which may block other consumers */
			pthread_cleanup_push((void (*)(void *))
				_pthread_mutex_unlock, &f->consumer->mutex);
			err = pthread_cond_timedwait(&f->consumer->cond,
						     &f->consumer->mutex,
						     timeout);
			pthread_cleanup_pop(0);
		} else {
			_pthread_mutex_unlock(&f->consumer->mutex);
			f->wait_full(f);
			_pthread_mutex_lock(&f->consumer->mutex);
		}

		f->c_reentry--;

		if (err == ETIMEDOUT)
		  {
		    _pthread_mutex_unlock(&f->consumer->mutex);
		    return NULL;
		  }
	}

	c->next_buffer = (zf_buffer *) b->node.succ;

	b->dequeued++;

	_pthread_mutex_unlock(&f->consumer->mutex);

	c->dequeued++;

	return b;
}

/*
 *  This function removes the *oldest* buffer from the full queue
 *  which meets these conditions:
 *  - the buffer has no b->used == 0 (eof) or b->used < 0 (error),
 *    to avoid consumers miss this information
 *  - it is not in use by any consumer (b->enqueued < b->dequeued)
 *  - it has been consumed at least once, to avoid producers
 *    spinloop with unlinked buffers
 *  - it has been consumed by the majority of consumers (rounding
 *    up, ie. 1/2, 2/3, 2/4, 3/5). This way the caller (producer)
 *    is throttled when the majority of consumers (be it just one)
 *    is slow, reducing the CPU load. Otherwise the minority of
 *    slow consumers will loose buffers ("drop frames").
 *
 *  f->consumers->mutex must be locked when calling this.
 */
static zf_buffer *
unlink_full_buffer(zf_fifo *f)
{
	zf_consumer *c;
	zf_buffer *b;

	if (!f->unlink_full_buffers || f->consumers.members < 2)
		return NULL;

	for_all_nodes (b, &f->full, node) {
		if (0)
			printf("Unlink cand %p <%s> en=%d de=%d co=%d us=%d\n",
				b, (char *) b->data,
				b->enqueued, b->dequeued,
				b->consumers, b->used);

		if (b->enqueued >= b->dequeued
		    && (b->enqueued * 2) >= b->consumers
		    && b->used > 0) {
			for_all_nodes (c, &f->consumers, node)
				if (c->next_buffer == b)
					c->next_buffer = (zf_buffer *) b->node.succ;

			b->consumers = 0;
			b->dequeued = 0;

			b = PARENT(unlink_node(&f->full, &b->node), zf_buffer, node);

			return b;
		}
	}

	return NULL;
}

static void
wait_empty_buffer_cleanup(mucon *m)
{
	rem_head(&m->list);
	_pthread_mutex_unlock(&m->mutex);
}

/**
 * wait_empty_buffer:
 * @p: zf_producer *
 * 
 * Suspends execution of the calling thread until an empty buffer becomes
 * available. Otherwise identical to recv_empty_buffer().
 *
 * Return value:
 * Buffer pointer, never %NULL.
 **/
zf_buffer *
zf_wait_empty_buffer(zf_producer *p)
{
	zf_fifo *f = p->fifo;
	zf_buffer *b = NULL;
	node n;

	_pthread_mutex_lock(&f->producer->mutex);

	if (!empty_list(&f->producer->list))
		goto wait;

	while (!(b = PARENT(rem_head(&f->empty), zf_buffer, node))) {
		mutex_co_lock(&f->producer->mutex, &f->consumer->mutex);

		if ((b = unlink_full_buffer(f))) {
			_pthread_mutex_unlock(&f->consumer->mutex);
			break;
		}

		_pthread_mutex_unlock(&f->consumer->mutex);
 wait:
		if (f->p_reentry++ || !f->wait_empty) {
			/* Free resources which may block other producers */
			pthread_cleanup_push((void (*)(void *))
				wait_empty_buffer_cleanup, f->producer);

			add_tail(&f->producer->list, &n);

			/*
			 *  Assure first come first served
			 *  XXX works, but inefficient
			 */
			do pthread_cond_wait(&f->producer->cond, &f->producer->mutex);
			while (f->producer->list.head != &n);

			pthread_cleanup_pop(0);

		    	rem_head(&f->producer->list);
		} else {
			_pthread_mutex_unlock(&f->producer->mutex);	
			f->wait_empty(f);
			_pthread_mutex_lock(&f->producer->mutex);
		}

		f->p_reentry--;
	}

	_pthread_mutex_unlock(&f->producer->mutex);

	b->dequeued = 1;
	p->dequeued++;

//	fprintf(stderr, "WEB %s b=%p dq=%d\n", f->name, b, p->dequeued);

	return b;
}

/*
 *  This is the unbuffered lower half of function send_empty_buffer(),
 *  for callback consumers which lack a virtual full queue.
 */
static inline void
send_empty_unbuffered(zf_consumer *c, zf_buffer *b)
{
	zf_fifo *f = c->fifo;

	b->dequeued = 0;
	b->consumers = 0;

	_pthread_mutex_lock(&f->producer->mutex);

	add_head(&f->empty, &b->node);

	_pthread_mutex_unlock(&f->producer->mutex);

	pthread_cond_broadcast(&f->producer->cond);
}

/*
 *  This is the buffered lower half of function send_empty_buffer().
 */
void
zf_send_empty_buffered(zf_consumer *c, zf_buffer *b)
{
	zf_fifo *f = c->fifo;

	_pthread_mutex_lock(&f->consumer->mutex);

	if (++b->enqueued >= b->consumers)
		unlink_node(&f->full, &b->node);
	else
		b = NULL;

	_pthread_mutex_unlock(&f->consumer->mutex);

	if (!b)
		return;

	if (b->remove) {
		unlink_node(&f->buffers, &b->added);
		zf_destroy_buffer(b);
		return;
	}

	send_empty_unbuffered(c, b);
}

/*
 *  This is the lower half of function send_full_buffer().
 *
 *  b->dequeued, enqueued are zero.
 */
static void
send_full(zf_producer *p, zf_buffer *b)
{
	zf_fifo *f = p->fifo;

	_pthread_mutex_lock(&f->consumer->mutex);

	if ((b->consumers = f->consumers.members)) {
		zf_consumer *c;

		/*
		 *  c->next_buffer is NULL after the consumer dequeued all
		 *  buffers from the virtual f->full queue.
		 */
		for_all_nodes (c, &f->consumers, node)
			if (!c->next_buffer->node.succ)
				c->next_buffer = b;

		add_tail(&f->full, &b->node);

		_pthread_mutex_unlock(&f->consumer->mutex);

		pthread_cond_broadcast(&f->consumer->cond);
	} else {
		zf_consumer c;

		_pthread_mutex_unlock(&f->consumer->mutex);

		/*
		 *  Nobody is listening, I'm only the loopback.
		 */

		asserts(!f->wait_empty);

		c.fifo = f;

		send_empty_unbuffered(&c, b);
	}
}

/**
 * send_full_buffer:
 * @p: zf_producer *
 * @b: zf_buffer *
 * 
 * Producers call this function when a previously dequeued empty
 * buffer has been filled and is ready for consumption. Dereferencing
 * the buffer pointer after sending the buffer is not permitted.
 *
 * Take precautions to avoid spinlooping because consumption
 * of buffers may take no time.
 * 
 * These fields must be valid:
 * b->data    Pointer to the buffer payload.
 * b->used    Bytes of buffer payload. Zero indicates the end of a
 *            stream and negative values indicate a non-recoverable
 *            error. Consumers must send_empty_buffer() all received
 *            buffers regardless of the b->used value. Producers
 *            need not send EOF before they are removed from a fifo.
 *            When a fifo has multiple producers the EOF will be
 *            recorded until all producers sent EOF, the buffer
 *            will be recycled. Sending non-zero b->used after EOF
 *            is not permitted.
 * b->error   Copy of errno if appropriate, zero otherwise.
 * b->errstr  Pointer to a gettext()ized error message for display
 *            to the user if appropriate, %NULL otherwise. Shall not
 *            include strerror(errno), but rather hint the attempted
 *            action (e.g. "tried to read foo", errno = [no such file]).
 *
 * These fields have application specific semantics:
 * b->type    e.g. MPEG picture type
 * b->offset  e.g. IPB picture reorder offset
 * b->time    Seconds elapsed since some arbitrary reference point,
 *            usually epoch (gettimeofday()), for example the capture
 *            instant of a picture. b->time may not always increase,
 *            but consumers are guaranteed to receive buffers in
 *            sent order.
 **/
void
zf_send_full_buffer(zf_producer *p, zf_buffer *b)
{
	/* Migration prohibited, don't use this to add buffers to the fifo
	   (n/a to asy callback producers) */
	asserts(p->fifo == b->fifo /* && b->dequeued == 1 */);

	b->consumers = 1;

	b->dequeued = 0;
	b->enqueued = 0;

	p->dequeued--;

	if (b->used > 0) {
		asserts(!p->eof_sent);
	} else {
		zf_fifo *f = p->fifo;

		_pthread_mutex_lock(&f->producer->mutex);

		if (!p->eof_sent)
			f->eof_count++;

		if (f->eof_count < (int) list_members(&f->producers)) {
			b->consumers = 0;
			b->used = -1;
			b->error = EINVAL;
			b->errorstr = NULL;

			add_head(&f->empty, &b->node);

			p->eof_sent = TRUE;

			_pthread_mutex_unlock(&f->producer->mutex);

			pthread_cond_broadcast(&f->producer->cond);

			return;
		}

		p->eof_sent = TRUE;

		_pthread_mutex_unlock(&f->producer->mutex);
	}

	p->fifo->send_full(p, b);
}


/**
 * rem_buffer:
 * @b: zf_buffer *
 * 
 * Remove the buffer which has been previously added to its
 * fifo by add_buffer(), from the fifo and destroy it with
 * destroy_buffer().
 *
 * You can remove any buffer regardless if full, empty,
 * dequeued or enqueued. It will be either destroyed immediately
 * or scheduled for removal as soon as it has been consumed.
 **/
void
zf_rem_buffer(zf_buffer *b)
{
	zf_fifo *f = b->fifo;

	_pthread_mutex_lock(&f->consumer->mutex);

	/*
	 *  We do not remove buffers on the full queue: consumers > 0,
	 *  which have not been consumed yet: dequeued < consumers,
	 *  or are currently in use: enqueued < dequeued.
	 *  But we do remove buffers which reside on the empty queue
	 *  (have been returned by all consumers): consumers == dequeued == 0.
	 *  or have been dequeued from the empty queue: dequeued > 0,
	 */
	if (b->consumers == 0) {
		if (b->dequeued == 0)
			unlink_node(&f->empty, &b->node);

		unlink_node(&f->buffers, &b->added);
		zf_destroy_buffer(b);
	} else
		b->remove = TRUE;

	_pthread_mutex_unlock(&f->consumer->mutex);
}

static zf_buffer *
attach_buffer(zf_fifo *f, zf_buffer *b)
{
	if (!b)
		return NULL;

	b->fifo = f;

	b->consumers = 0;

	b->dequeued = 0;
	b->enqueued = 0;

	b->used = -1;
	b->error = EINVAL;

	add_tail(&f->buffers, &b->added);

	return b;
}

/**
 * add_buffer:
 * @f: zf_fifo * 
 * @b: zf_buffer *
 * 
 * Add the buffer to the fifo buffers list and make it available
 * for use. No op when @b is %NULL. Be warned havoc may prevail
 * when the caller of this function and the fifo owner disagree
 * about the buffer allocation method.
 * 
 * Return value: 
 * %FALSE when @b is %NULL.
 **/
z_bool
zf_add_buffer(zf_fifo *f, zf_buffer *b)
{
	zf_consumer c;

	if (!attach_buffer(f, b))
		return FALSE;

	c.fifo = f;

	send_empty_unbuffered(&c, b);

	return TRUE;
}

static int
init_fifo(zf_fifo *f, const char *name,
	void (* custom_wait_empty)(zf_fifo *),
	void (* custom_send_full)(zf_producer *, zf_buffer *),
	void (* custom_wait_full)(zf_fifo *),
	void (* custom_send_empty)(zf_consumer *, zf_buffer *),
	int num_buffers, ssize_t buffer_size)
{
	memset(f, 0, sizeof(zf_fifo));

	strncpy(f->name, name, sizeof(f->name) - 1);

	f->unlink_full_buffers = TRUE;

	f->wait_empty = custom_wait_empty;
	f->send_full  = custom_send_full;
	f->wait_full  = custom_wait_full;
	f->send_empty = custom_send_empty;

	f->start = (z_bool (*)(zf_fifo *)) nop;
	f->stop  = (void (*)(zf_fifo *)) nop;

	f->destroy = uninit_fifo;

	f->alloc_buffer = zf_alloc_buffer;

	init_list(&f->full);
	init_list(&f->empty);
	init_list(&f->producers);
	init_list(&f->consumers);

	mucon_init(f->producer = &f->pro);
	mucon_init(f->consumer = &f->con);

	init_list(&f->buffers);

	for (; num_buffers > 0; num_buffers--) {
		zf_buffer *b;

		if (!(b = attach_buffer(f, f->alloc_buffer(buffer_size)))) {
			if (empty_list(&f->buffers)) {
				uninit_fifo(f);
				return 0;
			} else
				break;
		}

		add_tail(&f->empty, &b->node);
	}

	return list_members(&f->buffers);
}

/**
 * init_buffered_fifo:
 * @f: zf_fifo *
 * @name: The fifo name, for debugging purposes. Will be copied.
 * @num_buffers: Number of buffer objects to allocate and add to the fifo as
 * with add_buffer().
 * @buffer_size: If non-zero this is the size in bytes of buffer memory to
 * be allocated for each buffer with alloc_buffer().
 *
 * Initialize a fifo structure erasing all prior contents, and allocate
 * buffer memory as desired. destroy_fifo() frees all resources associated
 * with the fifo. You may override f->destroy to add your own cleanup functions.
 *
 * Return value: 
 * The number of buffers actually allocated.
 **/
int
zf_init_buffered_fifo(zf_fifo *f, const char *name, int num_buffers, ssize_t buffer_size)
{
	return init_fifo(f, name,
		NULL, send_full, NULL, zf_send_empty_buffered,
		num_buffers, buffer_size);
}

/**
 * init_callback_fifo:
 * @f: zf_fifo *
 * @name: The fifo name, for debugging purposes. Will be copied.
 * @custom_wait_empty: Custom functions, see below. NULL to get the default.
 * @custom_send_full: dto.
 * @custom_wait_full: dto.
 * @custom_send_empty: dto.
 * @num_buffers: Number of buffers to allocate and add to the fifo as
 *   with add_buffer().
 * @buffer_size: If non-zero this is the size in bytes of buffer memory to
 *   be allocated for each buffer with alloc_buffer().
 *
 * DEPRECATED. Will be replaced.
 *
 * Some producers or consumers merely transfer data to another fifo,
 * for example a device driver. Running a separate thread for this purpose
 * is just a waste of resources, so fifos can be initialized to use custom
 * functions for i/o. Callback fifos are transparent to the opposite side
 * and permit multiple producers or consumers.
 *
 * Note that custom_wait_* callbacks are protected from reentrancy,
 * custom_send_* callbacks must do their own serialization.
 *
 * Entering callback functions is not enough, the callback producer or
 * consumer must be added to the fifo with add_producer() or add_consumer()
 * to complete the initialization. Apart of this, init_callback_fifo() is
 * identical to init_buffered_fifo().
 *
 * a) Callback producer
 *   Preface:
 *     add_buffer() (or init_callback_fifo does it)
 *   custom_send_full():
 *     %NULL (default)
 *   custom_wait_empty():
 *     %NULL (default). If no buffers are available this will
 *     block until a consumer returns an empty buffer, should
 *     only happen when the fifo has less buffers than consumers
 *     or the producer wants more than one buffer for synchronous i/o.
 *   Consumer:
 *     wait_full_buffer()
 *     send_empty_buffer()
 *   Producer, synchronous i/o:
 *     custom_wait_full():
 *       call wait_empty_buffer(). Initially this will take a
 *         buffer from the empty queue, added by add_buffer().
 *       complete i/o
 *	 call send_full_buffer() and return
 *       Of course you can wait for and send more buffers at once.
 *     custom_send_empty():
 *       %NULL (default). Puts the buffer on the empty queue
 *       for wait_empty_buffer().
 *   Producer, asynchronous i/o:
 *     custom_send_empty():
 *   XXX flawed, rethink: send_empty_buffered?
 *       start i/o and return
 *     custom_wait_full():
 *       wait for i/o completion
 *       call send_full_buffer() and return
 *
 * b) Callback consumer
 *   Preface:
 *     add_buffer() (or init_callback_fifo does it)
 *   custom_send_empty():
 *     %NULL (default)
 *   custom_wait_full():
 *     %NULL (default). If no buffers are available this will
 *     block until a producer provides a full buffer, should
 *     only happen when the fifo has less buffers than producers
 *     or the consumer wants more than one buffer for synchronous i/o.
 *   Producer:
 *     wait_empty_buffer(). Initially this will take a
 *     buffer from the empty queue, added by add_buffer().
 *     send_full_buffer()
 *   Consumer, synchronous i/o:
 *     custom_wait_empty():
 *       call wait_full_buffer()
 *       complete i/o
 *	 call send_empty_buffer() and return
 *     custom_send_full():
 *       NULL (default). Puts the buffer on the full queue
 *       for wait_full_buffer().
 *   Consumer, asynchronous i/o:
 *     custom_send_full():
 *   XXX check
 *       start i/o and return
 *     custom_wait_empty():
 *       wait for i/o completion
 *       call send_empty_buffer() and return
 *
 * c) There is no c), either the producer or consumer must
 *    provide callbacks.
 * 
 * Return value: 
 * The number of buffers actually allocated.
 **/
int
zf_init_callback_fifo(zf_fifo *f, const char *name,
	void (* custom_wait_empty)(zf_fifo *),
	void (* custom_send_full)(zf_producer *, zf_buffer *),
	void (* custom_wait_full)(zf_fifo *),
	void (* custom_send_empty)(zf_consumer *, zf_buffer *),
	int num_buffers, ssize_t buffer_size)
{
//	asserts((!!custom_wait_empty) != (!!custom_wait_full));
//	asserts((!!custom_wait_empty) >= (!!custom_send_full));
//	asserts((!!custom_wait_full) >= (!!custom_send_empty));

	if (!custom_send_full)
		custom_send_full = send_full;

	if (!custom_send_empty) {
		if (custom_wait_empty)
			custom_send_empty = send_empty_unbuffered;
		else
			custom_send_empty = zf_send_empty_buffered;
	}

	return init_fifo(f, name,
		custom_wait_empty, custom_send_full,
		custom_wait_full, custom_send_empty,
		num_buffers, buffer_size);
}

/**
 * rem_producer:
 * @p: zf_producer *
 * 
 * Detach a producer from its fifo. No resource tracking;
 * All previously dequeued buffers must be returned with
 * send_full_buffer() before calling this function or they
 * remain unavailable until the fifo is destroyed.
 *
 * Safe to call after add_producer failed.
 **/
void
zf_rem_producer(zf_producer *p)
{
	zf_fifo *f;

	if ((f = p->fifo)) {
		_pthread_mutex_lock(&f->producer->mutex);

		if (rem_node(&f->producers, &p->node)) {
			asserts(p->dequeued == 0);

			/*
			 *  Pretend we didn't attempt an eof, and when
			 *  we really sent eofs remember it.
			 */
			if (f->eof_count > 1)
				if (p->eof_sent)
					f->eof_count--;
		}

		_pthread_mutex_unlock(&f->producer->mutex);
	}

	memset(p, 0, sizeof(*p));
}

/**
 * add_producer:
 * @f: zf_fifo *
 * @p: zf_producer *
 *
 * Initialize the producer object and add the producer to an
 * already initialized fifo. The fifo will recycle old buffers
 * when it runs out of empty buffers, so producers will not
 * starve when the fifo has no consumers.
 *
 * Producers should be removed when done to free resources, and
 * must be removed before destroying the producer object, with
 * the rem_producer() function.
 * 
 * Return value: 
 * The producer pointer, or %NULL if the operation failed.
 **/
zf_producer *
zf_add_producer(zf_fifo *f, zf_producer *p)
{
	p->fifo = f;

	p->dequeued = 0;
	p->eof_sent = FALSE;

	_pthread_mutex_lock(&f->producer->mutex);

	/*
	 *  Callback producers are individuals and finished
	 *  fifos remain finished. (Just in case.)
	 */
	if ((empty_list(&f->producers) || !f->wait_full)
	    && f->eof_count <= (int) list_members(&f->consumers))
		add_tail(&f->producers, &p->node);
	else
		p = NULL;

	_pthread_mutex_unlock(&f->producer->mutex);

	return p;
}

/**
 * rem_consumer:
 * @c: zf_consumer *
 * 
 * Detach a consumer from its fifo. No resource tracking;
 * All previously dequeued buffers must be returned with
 * send_empty_buffer() before calling this function or they
 * remain unavailable until the fifo is destroyed.
 *
 * Safe to call after add_consumer failed.
 **/
void
zf_rem_consumer(zf_consumer *c)
{
	zf_fifo *f;

	if ((f = c->fifo)) {
		_pthread_mutex_lock(&f->consumer->mutex);

		if (rem_node(&f->consumers, &c->node)) {
			zf_buffer *b;

			asserts(c->dequeued == 0);

			if (c->next_buffer) {
				for_all_nodes (b, &f->full, node) {
					b->consumers = f->consumers.members;
				}
			}
		}

		_pthread_mutex_unlock(&f->consumer->mutex);
	}

	memset(c, 0, sizeof(*c));
}

/**
 * add_consumer:
 * @f: zf_fifo *
 * @c: zf_consumer *
 * 
 * Initialize the consumer object and add the consumer to an
 * already initialized fifo. The consumer will not dequeue buffers
 * which have been produced and enqueued prior to this call.
 *
 * Consumers should be removed when done to free resources, and
 * must be removed before destroying the consumer object, with
 * the rem_consumer() function.
 * 
 * Return value: 
 * The consumer pointer, or %NULL if the operation failed.
 **/
zf_consumer *
zf_add_consumer(zf_fifo *f, zf_consumer *c)
{
	c->fifo = f;
	c->next_buffer = (zf_buffer *) &f->full.null;
	c->dequeued = 0;

	mutex_bi_lock(&f->producer->mutex, &f->consumer->mutex);

	/*
	 *  Callback consumers are individuals and finished
	 *  fifos are inhibited for new consumers. (Just in case.)
	 */
	if ((empty_list(&f->consumers) || !f->wait_empty)
	    && f->eof_count <= (int) list_members(&f->consumers))
		add_tail(&f->consumers, &c->node);
	else
		c = NULL;

	_pthread_mutex_unlock(&f->producer->mutex);
	_pthread_mutex_unlock(&f->consumer->mutex);

	return c;
}

/*
    TODO:
    * test callbacks
    * stealing buffers & callbacks ok?

    * add_p/c shall make a fifo callback
    * error ignores mp-fifo, in data direction only
    * add wait timeout (optional)
 */
