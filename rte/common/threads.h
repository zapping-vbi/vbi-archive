/*
 *  Copyright (C) 1999-2000 Michael H. Schimek
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

/* $Id: threads.h,v 1.1 2002-05-09 21:05:49 mschimek Exp $ */

#ifndef THREADS_H
#define THREADS_H

#undef _GNU_SOURCE
#define _GNU_SOURCE 1
/* XXX for rwlocks, but the parent file
   may have included pthread.h already */

#include <pthread.h>
#include "list.h"

typedef struct {
	pthread_mutex_t		mutex;		/* attn: fast mutex */
	pthread_cond_t		cond;
	list			list;
} mucon;

static inline void
mucon_init(mucon *m)
{
	pthread_mutex_init(&m->mutex, NULL);
	pthread_cond_init(&m->cond, NULL);
	init_list(&m->list);
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
