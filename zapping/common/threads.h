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

/* $Id: threads.h,v 1.1 2000-12-11 04:12:53 mschimek Exp $ */

#ifndef THREADS_H
#define THREADS_H

#include <pthread.h>

typedef struct {
	pthread_mutex_t		mutex;		/* attn: fast mutex */
	pthread_cond_t		cond;
} mucon;

static inline void
mucon_init(mucon *m)
{
	pthread_mutex_init(&m->mutex, NULL);
	pthread_cond_init(&m->cond, NULL);
}

static inline void
mucon_destroy(mucon *m)
{
	pthread_mutex_destroy(&m->mutex);
	pthread_cond_destroy(&m->cond);
}

static inline void
wait_mucon(mucon *m)
{
	pthread_mutex_lock(&m->mutex);
	pthread_cond_wait(&m->cond, &m->mutex);
	pthread_mutex_unlock(&m->mutex);
}

#endif /* THREADS_H */
