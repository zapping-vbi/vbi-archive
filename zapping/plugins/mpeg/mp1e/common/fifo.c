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

/* $Id: fifo.c,v 1.16 2001-05-05 23:35:09 garetxe Exp $ */

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

coninfo*
create_consumer(fifo *f)
{
	buffer *b;
	coninfo *new_consumer;

	/* no recursive rwlocks, this is non-atomic */
	pthread_rwlock_unlock(&f->consumers_rwlock);
	pthread_rwlock_wrlock(&f->consumers_rwlock);

	new_consumer = calloc(sizeof(coninfo), 1);
	mucon_init(&(new_consumer->consumer));
	init_list(&(new_consumer->full));
	new_consumer->f = f;
//	new_consumer->killable = 1;
	new_consumer->owner = pthread_self();

	add_tail(&f->consumers, (node*)new_consumer);

	pthread_setspecific(f->consumer_key, new_consumer);

	/* Send the kept buffers to this consumer */
	pthread_mutex_lock(&f->mbackup.mutex);
	while ((b = (buffer*) rem_head(&f->backup))) {
		add_tail(&(new_consumer->full), &b->node);
		new_consumer->waiting++;
	}
	pthread_mutex_unlock(&f->mbackup.mutex);

	pthread_rwlock_unlock(&f->consumers_rwlock);
	pthread_rwlock_rdlock(&f->consumers_rwlock);

	return new_consumer;
}

static inline void
dealloc_consumer_info(fifo *f, coninfo *consumer)
{
	buffer *b;
	list * full=&(consumer->full);
	mucon *cm=&(consumer->consumer);

	pthread_mutex_lock(&(cm->mutex));
	while ((b = (buffer*) rem_head(full))) {
		send_empty_buffer(f, b);
		consumer->waiting--;
	}
	if (consumer->waiting) {
		fprintf(stderr, "FIFO INTERNAL ERROR: waiting: %d [%s]\n",
			consumer->waiting, f->name);
		assert(consumer->waiting == 0);
	}
	pthread_mutex_unlock(&(cm->mutex));
	mucon_destroy(cm);

	unlink_node(&(f->consumers), (node*)consumer);

	if (pthread_equal(consumer->owner, pthread_self())) {
		/* done with this one */
		free(consumer);
		pthread_setspecific(f->consumer_key, NULL);

	} else {
		/* wait till it comes again and completes the
		   destruction */
		pthread_mutex_lock(&f->limbo_mutex);
		add_tail(&f->limbo, (node*)consumer);
		consumer->zombie = 1;
		pthread_mutex_unlock(&f->limbo_mutex);
	}
}

static void
uninit(fifo * f)
{
	pthread_rwlock_wrlock(&f->consumers_rwlock);
	while (!empty_list(&f->consumers))
		dealloc_consumer_info(f, (coninfo*)(f->consumers.head));
	pthread_rwlock_unlock(&f->consumers_rwlock);

	pthread_mutex_lock(&f->limbo_mutex);
	while (!empty_list(&f->limbo))
		free(rem_head(&f->limbo));
	pthread_mutex_unlock(&f->limbo_mutex);

	pthread_rwlock_destroy(&f->consumers_rwlock);
	pthread_key_delete(f->consumer_key);

	pthread_mutex_destroy(&f->limbo_mutex);

	if (f->buffers)
		free_buffer_vec(f->buffers, f->num_buffers);

	mucon_destroy(&f->producer);
	mucon_destroy(&f->mbackup);

	memset(f, 0, sizeof(fifo));

	f->wait_full  = (buffer * (*)(fifo *))		 dead_end;
	f->send_empty = (void     (*)(fifo *, buffer *)) dead_end;
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
	coninfo *consumer;

	b->refcount = 0;

	pthread_rwlock_rdlock(&f->consumers_rwlock);

	consumer = (coninfo*)f->consumers.head;

	if (consumer) {
		pthread_mutex_lock(&(consumer->consumer.mutex));
		add_tail(&(consumer->full), (node*)b);
		consumer->waiting ++;
		pthread_mutex_unlock(&(consumer->consumer.mutex));
		pthread_cond_broadcast(&(consumer->consumer.cond));
		b->refcount ++;
	}

	propagate_buffer(f, b, consumer);
	
	pthread_rwlock_unlock(&f->consumers_rwlock);
}

/* A consumer thread has disappeared without unregistering as a consumer */
static void
key_destroy_callback(void *param)
{
	coninfo *consumer = (coninfo*) param;
	fifo *f = consumer->f;

	pthread_rwlock_wrlock(&f->consumers_rwlock);
	dealloc_consumer_info(f, consumer);
	pthread_rwlock_unlock(&f->consumers_rwlock);
}

void
set_killable(fifo *f, int on)
{
	coninfo *consumer;

	pthread_rwlock_wrlock(&f->consumers_rwlock);
	consumer = query_consumer(f);

	consumer->killable = on;

	pthread_rwlock_unlock(&f->consumers_rwlock);	
}

/* Remove all stalled consumers */
void
kill_zombies(fifo *f)
{
	int zombies = 0;
	coninfo *p;

	/* first step: look for killable zombie wanabees */
	pthread_rwlock_rdlock(&f->consumers_rwlock);

	p = (coninfo*)f->consumers.head;
	while (p) {
		if (p->waiting == f->num_buffers &&
		    p->killable)
			zombies++;
		p = (coninfo*)(p->node.next);
	}
	pthread_rwlock_unlock(&f->consumers_rwlock);

	/* second step: send them to the limbo */
	pthread_rwlock_wrlock(&f->consumers_rwlock);

 kill_zombies_restart:
	p = (coninfo*)f->consumers.head;
	while (p) {
		if (p->waiting == f->num_buffers &&
		    p->killable) {
			dealloc_consumer_info(f, p);
			goto kill_zombies_restart;
		}
		p = (coninfo*)(p->node.next);
	}
	pthread_rwlock_unlock(&f->consumers_rwlock);
}

int
init_callback_fifo(fifo *f, char *name,
	buffer * (* custom_wait_full)(fifo *),
	void     (* custom_send_empty)(fifo *, buffer *),
	int num_buffers, int buffer_size)
{
	int i;

	memset(f, 0, sizeof(fifo));
	snprintf(f->name, 63, name);

	if (num_buffers > 0) {
		f->num_buffers = alloc_buffer_vec(&f->buffers,
			num_buffers, buffer_size);

		if (f->num_buffers == 0)
			return 0;

		for (i = 0; i < f->num_buffers; i++)
			add_tail(&f->empty, &f->buffers[i].node);
	}

	init_list(&f->consumers);

	pthread_rwlock_init(&f->consumers_rwlock, NULL);
	pthread_key_create(&f->consumer_key, key_destroy_callback);

	init_list(&f->limbo);
	pthread_mutex_init(&f->limbo_mutex, NULL);

	mucon_init(&f->producer);
	mucon_init(&f->mbackup);

	f->wait_full  = custom_wait_full  ? custom_wait_full  : NULL;
	f->send_empty = custom_send_empty ? custom_send_empty : send_empty;
	f->send_full  = send_full;

	/*
	    The caller may want to know what the defaults were
	    when overriding these.
	 */
	f->start = start;
	f->uninit = uninit;

	return f->num_buffers;
}

int
init_buffered_fifo(fifo *f, char *name, int num_buffers, int buffer_size)
{
	init_callback_fifo(f, name, NULL, NULL,
		num_buffers, buffer_size);

	if (num_buffers > 0 && f->num_buffers <= 0)
		return 0;

	return f->num_buffers;
}
