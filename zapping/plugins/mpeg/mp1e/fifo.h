/*
 *  MPEG-1 Real Time Encoder
 *
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

/* $Id: fifo.h,v 1.2 2000-07-06 10:58:47 garetxe Exp $ */

#ifndef FIFO_H
#define FIFO_H

#include <string.h>
#include <pthread.h>
#include "misc.h"
#include "log.h"

typedef struct _node {
	struct _node *		next;
} node;

typedef struct _list {
	struct _node *		head;
	struct _node *		tail;
} list;

static inline void
add_tail(list *l, node *n)
{
	node *p;

	n->next = (node *) 0;

	if ((p = l->tail))
		p->next = n;

	if (!l->head)
		l->head = n;

	l->tail = n;
}

static inline node *
rem_head(list *l)
{
	node *n;

	if ((n = l->head)) {
		if (!n->next)
			l->tail = (node *) 0;

		l->head = n->next;
	}

	return n;
}

typedef struct {
	node 			node;
	unsigned char *		data;
	unsigned char *		buffer;
	int			size;
	double			time;
	int                     index; /* index in fifo.buffer */
} buffer;

typedef struct {
	pthread_mutex_t		mutex;
	pthread_cond_t		cond;

	list			full;
	list			empty;

	buffer			buffer[64];
	int                     num_buffers;

	/* MUX parameters */	

	int			in, out, max;
	buffer *		buf;

	unsigned char *		ptr;
	int			left;
	double			time, tpb;
	double			dts, tick;
	double			rtime, rtick;
} fifo;

static inline int
init_fifo(fifo *f, char *purpose, int size, int buffers)
{
	int i;

	memset(f, 0, sizeof(fifo));

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
free_fifo(fifo * f)
{
	int i;

	for (i = 0; i < f->num_buffers; i++)
		free(f->buffer[i].data);

	pthread_mutex_destroy(&f->mutex);
	pthread_cond_destroy(&f->cond);

	memset(f, 0, sizeof(fifo));
}

static inline buffer *
new_buffer(fifo *f)
{
	buffer *b;

	pthread_mutex_lock(&f->mutex);

	while (!(b = (buffer *) rem_head(&f->empty)))
		pthread_cond_wait(&f->cond, &f->mutex);

	pthread_mutex_unlock(&f->mutex);

	return b;
}

static inline void
send_buffer(fifo *f, buffer *b)
{
	extern pthread_mutex_t mux_mutex;
	extern pthread_cond_t mux_cond;

	pthread_mutex_lock(&mux_mutex);

	add_tail(&f->full, &b->node);

	pthread_mutex_unlock(&mux_mutex);
	pthread_cond_broadcast(&mux_cond);
}

/*
  send_buffer uses mux_mutex and mux_cond, and that isn't what we want
*/
static inline void
send_out_buffer(fifo *f, buffer *b)
{
	pthread_mutex_lock(&f->mutex);

	add_tail(&f->full, &b->node);

	pthread_mutex_unlock(&f->mutex);
	pthread_cond_broadcast(&f->cond);
}

static inline void
empty_buffer(fifo *f, buffer *b)
{
	pthread_mutex_lock(&f->mutex);

	add_tail(&f->empty, &b->node);

	pthread_mutex_unlock(&f->mutex);
	pthread_cond_broadcast(&f->cond);
}

#endif
