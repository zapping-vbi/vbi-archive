/*
 *  Copyright (C) 1999-2001 Michael H. Schimek
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

/* $Id: fifo.c,v 1.29 2001-08-17 20:59:08 garetxe Exp $ */
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
extern const char* program_invocation_name;
extern const char* program_invocation_short_name;
#endif

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
asserts_fail(char *assertion, char *file, unsigned int line,
	char *function, void *caller)
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

static bool
nop(void)
{
	return TRUE;
}

static void
uninit_buffer(buffer *b)
{
	if (b->allocated)
		free_aligned(b->allocated);

	b->allocated = NULL;
	b->size = 0;
}

/**
 * init_buffer:
 * @b: buffer *
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
buffer *
init_buffer(buffer *b, ssize_t size)
{
	size_t page_size = (size_t) sysconf(_SC_PAGESIZE);

	memset(b, 0, sizeof(buffer));

	b->destroy = (void (*)(buffer *)) nop;

	if (size > 0) {
		b->data =
		b->allocated =
			calloc_aligned(size, (size < page_size) ?
				CACHE_LINE : page_size);

		if (!b->allocated)
			return NULL;

		b->destroy = uninit_buffer;
		b->size = size;
	}

	return b;
}

static void
free_buffer(buffer *b)
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
buffer *
alloc_buffer(ssize_t size)
{
	buffer *b;

	if (!(b = malloc(sizeof(buffer))))
		return NULL;

	if (!init_buffer(b, size)) {
		free(b);
		return NULL;
	}

	if (size > 0)
		b->destroy = free_buffer;
	else
		b->destroy = (void (*)(buffer *)) free;

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
		pthread_mutex_lock(m1);
		if (pthread_mutex_trylock(m2) == 0)
			break;
		pthread_mutex_unlock(m1);
		swap(m1, m2);
	}
}

static inline void
mutex_co_lock(pthread_mutex_t *m1, pthread_mutex_t *m2)
{
	while (pthread_mutex_trylock(m2) != 0) {
		pthread_mutex_unlock(m1);
		swap(m1, m2);
		pthread_mutex_lock(m1);
	}
}

static void
dead_fifo(fifo *f)
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
dead_producer(producer *p)
{
	fifo *f = p->fifo;
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
dead_consumer(consumer *c)
{
	fifo *f = c->fifo;
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
uninit_fifo(fifo *f)
{
	node *n;

	f->destroy    = (void (*)(fifo *)) dead_fifo;

	f->wait_empty = (void (*)(fifo *)) dead_fifo;
	f->send_full  = (void (*)(producer *, buffer *)) dead_producer;

	f->wait_full  = (void (*)(fifo *)) dead_fifo;
	f->send_empty = (void (*)(consumer *, buffer *)) dead_consumer;

	f->start      = (bool (*)(fifo *)) dead_fifo;
	f->stop       = (void (*)(fifo *)) dead_fifo;

	while ((n = rem_tail(&f->buffers)))
		destroy_buffer(PARENT(n, buffer, added));

	destroy_list(&f->buffers);

	mucon_destroy(&f->pro);
	mucon_destroy(&f->con);

	destroy_list(&f->full);
	destroy_list(&f->empty);
	destroy_list(&f->producers);
	destroy_list(&f->consumers);
}

/**
 * wait_full_buffer:
 * @c: consumer *
 * 
 * Suspends execution of the calling thread until a full buffer becomes
 * available for consumption. Otherwise identical to recv_full_buffer().
 *
 * Return value:
 * Buffer pointer, never %NULL.
 **/
buffer *
wait_full_buffer(consumer *c)
{
	fifo *f = c->fifo;
	buffer *b;

	pthread_mutex_lock(&f->consumer->mutex);

	while (!(b = c->next_buffer)->node.succ) {
		if (f->c_reentry++ || !f->wait_full) {
			/* Free resources which may block other consumers */
			pthread_cleanup_push((void (*)(void *))
				pthread_mutex_unlock, &f->consumer->mutex);
			pthread_cond_wait(&f->consumer->cond, &f->consumer->mutex);
			pthread_cleanup_pop(0);
		} else {
			pthread_mutex_unlock(&f->consumer->mutex);
			f->wait_full(f);
			pthread_mutex_lock(&f->consumer->mutex);
		}

		f->c_reentry--;
	}

	c->next_buffer = (buffer *) b->node.succ;

	b->dequeued++;

	pthread_mutex_unlock(&f->consumer->mutex);

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
static buffer *
unlink_full_buffer(fifo *f)
{
	consumer *c;
	buffer *b;

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
					c->next_buffer = (buffer *) b->node.succ;

			b->consumers = 0;
			b->dequeued = 0;

			b = PARENT(unlink_node(&f->full, &b->node), buffer, node);

			return b;
		}
	}

	return NULL;
}

static void
wait_empty_buffer_cleanup(mucon *m)
{
	rem_head(&m->list);
	pthread_mutex_unlock(&m->mutex);
}

/**
 * wait_empty_buffer:
 * @p: producer *
 * 
 * Suspends execution of the calling thread until an empty buffer becomes
 * available. Otherwise identical to recv_empty_buffer().
 *
 * Return value:
 * Buffer pointer, never %NULL.
 **/
buffer *
wait_empty_buffer(producer *p)
{
	fifo *f = p->fifo;
	buffer *b = NULL;
	node n;

	pthread_mutex_lock(&f->producer->mutex);

	if (!empty_list(&f->producer->list))
		goto wait;

	while (!(b = PARENT(rem_head(&f->empty), buffer, node))) {
		mutex_co_lock(&f->producer->mutex, &f->consumer->mutex);

		if ((b = unlink_full_buffer(f))) {
			pthread_mutex_unlock(&f->consumer->mutex);
			break;
		}

		pthread_mutex_unlock(&f->consumer->mutex);
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
			pthread_mutex_unlock(&f->producer->mutex);	
			f->wait_empty(f);
			pthread_mutex_lock(&f->producer->mutex);
		}

		f->p_reentry--;
	}

	pthread_mutex_unlock(&f->producer->mutex);

	b->dequeued = 1;
	p->dequeued++;

	return b;
}

/*
 *  This is the unbuffered lower half of function send_empty_buffer(),
 *  for callback consumers which lack a virtual full queue.
 */
static inline void
send_empty_unbuffered(consumer *c, buffer *b)
{
	fifo *f = c->fifo;

	b->dequeued = 0;
	b->consumers = 0;

	pthread_mutex_lock(&f->producer->mutex);

	add_head(&f->empty, &b->node);

	pthread_mutex_unlock(&f->producer->mutex);

	pthread_cond_broadcast(&f->producer->cond);
}

/*
 *  This is the buffered lower half of function send_empty_buffer().
 */
void
send_empty_buffered(consumer *c, buffer *b)
{
	fifo *f = c->fifo;

	pthread_mutex_lock(&f->consumer->mutex);

	if (++b->enqueued >= b->consumers)
		unlink_node(&f->full, &b->node);
	else
		b = NULL;

	pthread_mutex_unlock(&f->consumer->mutex);

	if (!b)
		return;

	if (b->remove) {
		unlink_node(&f->buffers, &b->added);
		destroy_buffer(b);
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
send_full(producer *p, buffer *b)
{
	fifo *f = p->fifo;

	pthread_mutex_lock(&f->consumer->mutex);

	if ((b->consumers = f->consumers.members)) {
		consumer *c;

		/*
		 *  c->next_buffer is NULL after the consumer dequeued all
		 *  buffers from the virtual f->full queue.
		 */
		for_all_nodes (c, &f->consumers, node)
			if (!c->next_buffer->node.succ)
				c->next_buffer = b;

		add_tail(&f->full, &b->node);

		pthread_mutex_unlock(&f->consumer->mutex);

		pthread_cond_broadcast(&f->consumer->cond);
	} else {
		consumer c;

		pthread_mutex_unlock(&f->consumer->mutex);

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
 * @p: producer *
 * @b: buffer *
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
send_full_buffer(producer *p, buffer *b)
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
		fifo *f = p->fifo;

		pthread_mutex_lock(&f->producer->mutex);

		if (!p->eof_sent)
			f->eof_count++;

		if (f->eof_count < list_members(&f->producers)) {
			b->consumers = 0;
			b->used = -1;
			b->error = EINVAL;
			b->errstr = NULL;

			add_head(&f->empty, &b->node);

			p->eof_sent = TRUE;

			pthread_mutex_unlock(&f->producer->mutex);

			pthread_cond_broadcast(&f->producer->cond);

			return;
		}

		p->eof_sent = TRUE;

		pthread_mutex_unlock(&f->producer->mutex);
	}

	p->fifo->send_full(p, b);
}


/**
 * rem_buffer:
 * @b: buffer *
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
rem_buffer(buffer *b)
{
	fifo *f = b->fifo;

	pthread_mutex_lock(&f->consumer->mutex);

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
		destroy_buffer(b);
	} else
		b->remove = TRUE;

	pthread_mutex_unlock(&f->consumer->mutex);
}

static buffer *
attach_buffer(fifo *f, buffer *b)
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
 * @f: fifo * 
 * @b: buffer *
 * 
 * Add the buffer to the fifo buffers list and make it available
 * for use. No op when @b is %NULL. Be warned havoc may prevail
 * when the caller of this function and the fifo owner disagree
 * about the buffer allocation method.
 * 
 * Return value: 
 * %FALSE when @b is %NULL.
 **/
bool
add_buffer(fifo *f, buffer *b)
{
	consumer c;

	if (!attach_buffer(f, b))
		return FALSE;

	c.fifo = f;

	send_empty_unbuffered(&c, b);

	return TRUE;
}

static int
init_fifo(fifo *f, char *name,
	void (* custom_wait_empty)(fifo *),
	void (* custom_send_full)(producer *, buffer *),
	void (* custom_wait_full)(fifo *),
	void (* custom_send_empty)(consumer *, buffer *),
	int num_buffers, ssize_t buffer_size)
{
	memset(f, 0, sizeof(fifo));

	strncpy(f->name, name, sizeof(f->name) - 1);

	f->unlink_full_buffers = TRUE;

	f->wait_empty = custom_wait_empty;
	f->send_full  = custom_send_full;
	f->wait_full  = custom_wait_full;
	f->send_empty = custom_send_empty;

	f->start = (bool (*)(fifo *)) nop;
	f->stop  = (void (*)(fifo *)) nop;

	f->destroy = uninit_fifo;

	init_list(&f->full);
	init_list(&f->empty);
	init_list(&f->producers);
	init_list(&f->consumers);

	mucon_init(f->producer = &f->pro);
	mucon_init(f->consumer = &f->con);

	init_list(&f->buffers);

	for (; num_buffers > 0; num_buffers--) {
		buffer *b;

		if (!(b = attach_buffer(f, alloc_buffer(buffer_size)))) {
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
 * @f: fifo *
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
init_buffered_fifo(fifo *f, char *name, int num_buffers, ssize_t buffer_size)
{
	return init_fifo(f, name,
		NULL, send_full, NULL, send_empty_buffered,
		num_buffers, buffer_size);
}

/**
 * init_callback_fifo:
 * @f: fifo *
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
init_callback_fifo(fifo *f, char *name,
	void (* custom_wait_empty)(fifo *),
	void (* custom_send_full)(producer *, buffer *),
	void (* custom_wait_full)(fifo *),
	void (* custom_send_empty)(consumer *, buffer *),
	int num_buffers, ssize_t buffer_size)
{
	asserts((!!custom_wait_empty) != (!!custom_wait_full));
	asserts((!!custom_wait_empty) >= (!!custom_send_full));
	asserts((!!custom_wait_full) >= (!!custom_send_empty));

	if (!custom_send_full)
		custom_send_full = send_full;

	if (!custom_send_empty) {
		if (custom_wait_empty)
			custom_send_empty = send_empty_unbuffered;
		else
			custom_send_empty = send_empty_buffered;
	}

	return init_fifo(f, name,
		custom_wait_empty, custom_send_full,
		custom_wait_full, custom_send_empty,
		num_buffers, buffer_size);
}

/**
 * rem_producer:
 * @p: producer *
 * 
 * Detach a producer from its fifo. No resource tracking;
 * All previously dequeued buffers must be returned with
 * send_full_buffer() before calling this function or they
 * remain unavailable until the fifo is destroyed.
 *
 * Safe to call after add_producer failed.
 **/
void
rem_producer(producer *p)
{
	fifo *f;

	if ((f = p->fifo)) {
		pthread_mutex_lock(&f->producer->mutex);

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

		pthread_mutex_unlock(&f->producer->mutex);
	}

	memset(p, 0, sizeof(*p));
}

/**
 * add_producer:
 * @f: fifo *
 * @p: producer *
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
producer *
add_producer(fifo *f, producer *p)
{
	p->fifo = f;

	p->dequeued = 0;
	p->eof_sent = FALSE;

	pthread_mutex_lock(&f->producer->mutex);

	/*
	 *  Callback producers are individuals and finished
	 *  fifos remain finished. (Just in case.)
	 */
	if ((empty_list(&f->producers) || !f->wait_full)
	    && f->eof_count <= list_members(&f->consumers))
		add_tail(&f->producers, &p->node);
	else
		p = NULL;

	pthread_mutex_unlock(&f->producer->mutex);

	return p;
}

/**
 * rem_consumer:
 * @c: consumer *
 * 
 * Detach a consumer from its fifo. No resource tracking;
 * All previously dequeued buffers must be returned with
 * send_empty_buffer() before calling this function or they
 * remain unavailable until the fifo is destroyed.
 *
 * Safe to call after add_consumer failed.
 **/
void
rem_consumer(consumer *c)
{
	fifo *f;

	if ((f = c->fifo)) {
		pthread_mutex_lock(&f->consumer->mutex);

		if (rem_node(&f->consumers, &c->node)) {
			asserts(c->dequeued == 0);
		}

		pthread_mutex_unlock(&f->consumer->mutex);
	}

	memset(c, 0, sizeof(*c));
}

/**
 * add_consumer:
 * @f: fifo *
 * @c: consumer *
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
consumer *
add_consumer(fifo *f, consumer *c)
{
	c->fifo = f;
	c->next_buffer = (buffer *) &f->full.null;
	c->dequeued = 0;

	mutex_bi_lock(&f->producer->mutex, &f->consumer->mutex);

	/*
	 *  Callback consumers are individuals and finished
	 *  fifos are inhibited for new consumers. (Just in case.)
	 */
	if ((empty_list(&f->consumers) || !f->wait_empty)
	    && f->eof_count <= list_members(&f->consumers))
		add_tail(&f->consumers, &c->node);
	else
		c = NULL;

	pthread_mutex_unlock(&f->producer->mutex);
	pthread_mutex_unlock(&f->consumer->mutex);

	return c;
}

/*
    TODO:
    * test callbacks
    * stealing buffers & callbacks ok?

    * add_p/c shall make a fifo callback
    * error ignores mp-fifo, in data direction only
 */


