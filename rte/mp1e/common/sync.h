/*
 *  MPEG-1 Real Time Encoder
 *
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

/* $Id: sync.h,v 1.3 2001-09-03 05:26:07 mschimek Exp $ */

#ifndef SYNC_H
#define SYNC_H

#include "threads.h"
#include "fifo.h"

struct synchr {
	mucon			mucon;

	double			start_time;
	double			stop_time;
	double			front_time;

	unsigned int		modules;
	unsigned int		vote;
};

extern struct synchr synchr;

extern void	sync_init(unsigned int modules);
extern bool	sync_start(double time);
extern bool	sync_stop(double time);
extern bool	sync_sync(consumer *c, unsigned int this_module, double sample_period, int bytes_per_sample);

static inline int
sync_break(unsigned int this_module, double time, double frame_period)
{
	pthread_mutex_lock(&synchr.mucon.mutex);

	if (time >= synchr.stop_time) {
		pthread_mutex_unlock(&synchr.mucon.mutex);
		printv(4, "sync_break %08x, %f, stop_time %f\n",
			this_module, time, synchr.stop_time);
		return TRUE;
	}

	synchr.front_time = time + frame_period;

	pthread_mutex_unlock(&synchr.mucon.mutex);

	return FALSE;
}

#endif // SYNC_H







