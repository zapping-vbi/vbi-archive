/*
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

/* $Id: fifo.c,v 1.5 2001-03-20 22:19:50 garetxe Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <assert.h>
#include "fifo.h"
#include "alloc.h"

#ifndef CACHE_LINE
#define CACHE_LINE 32
#endif

void
uninit_buffer(buffer *b)
{
	if (b && b->allocated)
		free_aligned(b->allocated);

	b->allocated = NULL;
	b->size = 0;
}

bool
init_buffer(buffer *b, int size)
{
	size_t page_size = (size_t) sysconf(_SC_PAGESIZE);

	memset(b, 0, sizeof(buffer));

	if (size > 0) {
		b->data =
		b->allocated =
			calloc_aligned(size, (size < page_size) ?
				CACHE_LINE : page_size);

		if (!b->allocated)
			return FALSE;

		b->size = size;
	}

	return TRUE;
}

void
free_buffer_vec(buffer *bvec, int num_buffers)
{
	int i;

	if (bvec) {
		for (i = 0; i < num_buffers; i++)
			uninit_buffer(bvec + i);

		free(bvec);
	}
}

int
alloc_buffer_vec(buffer **bpp, int num_buffers, int buffer_size)
{
	int i;
	buffer *bvec;

	if (num_buffers <= 0) {
		*bpp = NULL;
		return 0;
	}

	if (!(bvec = calloc(num_buffers, sizeof(buffer))))
		return 0;

	for (i = 0; i < num_buffers; i++) {
		if (!init_buffer(bvec + i, buffer_size))
			break;

		bvec[i].index = i;
	}

	if (i == 0) {
		free(bvec);
		bvec = 0;
	}

	*bpp = bvec;

	return i;
}

static void
dead_end(fifo *f)
{
	fprintf(stderr, "Internal error - invalid fifo %p", f);
	exit(EXIT_FAILURE);
}

static bool
start(fifo *f)
{
	return TRUE;
}

static inline void
dealloc_consumer_info(fifo *f, int index)
{
	buffer *b; int i;
	list * full=&(f->consumers[index]->full);
	mucon *consumer=&(f->consumers[index]->consumer);

	pthread_mutex_lock(&(consumer->mutex));
	while ((b = (buffer*) rem_head(full))) {
		send_empty_buffer(f, b);
		f->consumers[index]->occupancy--;
	}
	assert(f->consumers[index]->occupancy == 0);
	pthread_mutex_unlock(&(consumer->mutex));
	mucon_destroy(consumer);

	free(f->consumers[index]);

	if (index<(f->num_consumers-1))
		memcpy(&(f->consumers[index]),
		       &(f->consumers[index+1]),
		       ((f->num_consumers-1)-index)*sizeof(coninfo*));

	f->consumers = realloc(f->consumers,
			       (--(f->num_consumers))*sizeof(coninfo*));

	for (i=0; i<f->num_consumers; i++) /* reindex */
		f->consumers[i]->index = i;

	pthread_setspecific(f->consumer_key, NULL);
}

void
remove_consumer(fifo *f)
{
	coninfo *consumer =
		(coninfo*)pthread_getspecific(f->consumer_key);

	if (consumer) {
		pthread_rwlock_wrlock(&f->consumers_rwlock);
		dealloc_consumer_info(f, consumer->index);
		pthread_rwlock_unlock(&f->consumers_rwlock);
	}
}

static void
uninit(fifo * f)
{
	pthread_rwlock_wrlock(&f->consumers_rwlock);
	while (f->num_consumers)
		dealloc_consumer_info(f, 0);
	pthread_rwlock_unlock(&f->consumers_rwlock);

	pthread_rwlock_destroy(&f->consumers_rwlock);
	pthread_key_delete(f->consumer_key);

	if (f->buffers)
		free_buffer_vec(f->buffers, f->num_buffers);

	mucon_destroy(&f->producer);
	mucon_destroy(&f->mbackup);

	memset(f, 0, sizeof(fifo));

	f->wait_full  = (buffer * (*)(fifo *))		 dead_end;
	f->send_empty = (void     (*)(fifo *, buffer *)) dead_end;
	f->wait_empty = (buffer * (*)(fifo *))		 dead_end;
	f->send_full  = (void     (*)(fifo *, buffer *)) dead_end;
	f->start      = (bool     (*)(fifo *))           dead_end;
	f->uninit     = (void     (*)(fifo *))           dead_end;
}

static void
send_empty(fifo *f, buffer *b)
{
	pthread_mutex_lock(&f->producer.mutex);
	
	add_head(&f->empty, &b->node);
	
	pthread_mutex_unlock(&f->producer.mutex);
	pthread_cond_broadcast(&f->producer.cond);
}

/*
    No wait_*, recv_* because we don't bother callback
    producers with unget_full.
 */
static void
send_full(fifo *f, buffer *b)
{
	int i;

	pthread_rwlock_rdlock(&f->consumers_rwlock);

	if (f->num_consumers) {
		b->refcount = f->num_consumers;

		for (i=0; i<f->num_consumers; i++) {
			pthread_mutex_lock(&(f->consumers[i]->consumer.mutex));
			add_tail(&(f->consumers[i]->full), &b->node);
			f->consumers[i]->occupancy++;
			pthread_mutex_unlock(&(f->consumers[i]->consumer.mutex));
			pthread_cond_broadcast(&(f->consumers[i]->consumer.cond));
		}
	} else {
		/* store it for later use */
		b->refcount = 1;
		pthread_mutex_lock(&f->mbackup.mutex);
		add_tail(&f->backup, &b->node);
		pthread_mutex_unlock(&f->mbackup.mutex);
	}
	
	pthread_rwlock_unlock(&f->consumers_rwlock);
}

/* A consumer thread has disappeared without unregistering as a consumer */
static void
key_destroy_callback(void *param)
{
	coninfo *consumer = (coninfo*) param;

	if (consumer) { /* param MUST be != NULL, just paranoid */
		pthread_rwlock_wrlock(&consumer->f->consumers_rwlock);
		dealloc_consumer_info(consumer->f, consumer->index);
		pthread_rwlock_unlock(&consumer->f->consumers_rwlock);
	}	
}

int
init_callback_fifo(fifo *f, char *name,
	buffer * (* custom_wait_full)(fifo *),
	void     (* custom_send_empty)(fifo *, buffer *),
	buffer * (* custom_wait_empty)(fifo *),
	void     (* custom_send_full)(fifo *, buffer *),
	int num_buffers, int buffer_size)
{
	int i;

	memset(f, 0, sizeof(fifo));

	/*	if (custom_send_full)
		FAIL("send_full callback not allowed");*/

	if (num_buffers > 0) {
		f->num_buffers = alloc_buffer_vec(&f->buffers,
			num_buffers, buffer_size);

		if (f->num_buffers == 0)
			return 0;

		for (i = 0; i < f->num_buffers; i++)
			add_tail(&f->empty, &f->buffers[i].node);
	}

	pthread_rwlock_init(&f->consumers_rwlock, NULL);
	pthread_key_create(&f->consumer_key, key_destroy_callback);

	mucon_init(&f->producer);
	mucon_init(&f->mbackup);

	f->wait_full  = custom_wait_full  ? custom_wait_full  : NULL;
	f->send_empty = custom_send_empty ? custom_send_empty : send_empty;
	f->wait_empty = custom_wait_empty ? custom_wait_empty : NULL;
	f->send_full  = custom_send_full  ? custom_send_full  : send_full;

	/*
	    The caller may want to know what the defaults were
	    when overriding these.
	 */
	f->start = start;
	f->uninit = uninit;

	return f->num_buffers;
}

int
init_buffered_fifo(fifo *f, char *name,
	mucon *consumer, int num_buffers, int buffer_size)
{
	init_callback_fifo(f, name, NULL, NULL, NULL, NULL,
		num_buffers, buffer_size);

	if (num_buffers > 0 && f->num_buffers <= 0)
		return 0;

	/* there are no public consumer mucons in this implementation */
	/*	if (consumer)
		FAIL("Custom consumer mucons not allowed");*/
//	f->consumer = consumer;

	return f->num_buffers;
}

/* fixme: What should we do with this? */
int
num_buffers_queued(fifo *f)
{
#if 0
	node *n;
	int i;
	list *full; mucon *consumer;

	pthread_rwlock_rdlock(&f->consumers_rwlock);
	query_consumer(f, &full, &consumer);
	pthread_mutex_lock(&consumer->mutex);

	for (n = full->head, i = 0; n; n = n->next, i++); 

	pthread_mutex_unlock(&consumer->mutex);
	pthread_mutex_unlock(&f->consumers_mutex);

	return i;
#endif
	return 0;
}
