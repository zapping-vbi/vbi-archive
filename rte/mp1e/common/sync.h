/*
 *  MPEG-1 Real Time Encoder
 *
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

/* $Id: sync.h,v 1.6 2002-01-21 07:41:04 mschimek Exp $ */

#ifndef SYNC_H
#define SYNC_H

#include <math.h>

#include "threads.h"
#include "fifo.h"
#include "log.h"

typedef unsigned int sync_module;

struct synchr {
	mucon			mucon;

	double			start_time;
	double			stop_time;
	double			front_time;

	sync_module		modules;
	sync_module		vote;

	double			ref_warp;
	sync_module		time_base;
};

typedef struct synchr_stream {
	sync_module		this_module;

	double			start_ref;

	double			byte_period;
	double			frame_period;

	int			bytes_per_sample;	/* power of 2 */
} synchr_stream;

/* GGG */
extern struct synchr synchr;

extern void	mp1e_sync_init(unsigned int modules, unsigned int time_base);
extern bool	mp1e_sync_start(double time);
extern bool	mp1e_sync_stop(double time);
extern bool	mp1e_sync_run_in(synchr_stream *str, consumer *c, int *frame_frac);

static inline int
mp1e_sync_break(synchr_stream *str, double time)
{
	pthread_mutex_lock(&synchr.mucon.mutex);

	if (time >= synchr.stop_time) {
		pthread_mutex_unlock(&synchr.mucon.mutex);

		printv(4, "sync_break %08x, %f, stop_time %f\n",
		       str->this_module, time, synchr.stop_time);

		return TRUE;
	}

	if (time > synchr.front_time)
		synchr.front_time = time;

	pthread_mutex_unlock(&synchr.mucon.mutex);

	return FALSE;
}

static inline double
mp1e_sync_drift(synchr_stream *str, double time, double elapsed)
{
	double drift;

	pthread_mutex_lock(&synchr.mucon.mutex);

	printv(4, "SD%02d %f i%f o%f d%f\n", str->this_module,
	       time, (time - str->start_ref), elapsed,
	       (time - str->start_ref) - elapsed);

	if (str->this_module == synchr.time_base) {
		if (elapsed >= 0.5) {
			double warp = (time - str->start_ref) / elapsed;
			synchr.ref_warp += (warp - synchr.ref_warp) * 0.1;
		}

		drift = 0.0;

		printv(4, "SD%02d ref_warp %f, %f s\n",
		       str->this_module, synchr.ref_warp,
		       (time - str->start_ref) - elapsed);
	} else {
		double ref_time = (time - str->start_ref) * synchr.ref_warp;

		drift = elapsed - ref_time;

		printv(4, "SD%02d ref %f o%f, drift %+9.6f s, %+9.6f ppm, %+9.6f units\n",
		       str->this_module, ref_time, elapsed, drift,
		       elapsed * 1e6 / ref_time - 1e6,
		       drift / (str->frame_period + str->byte_period));
	}

	pthread_mutex_unlock(&synchr.mucon.mutex);

	return drift;
}

struct tfmem {
	double			ref;
	double			err;
	double			ecr;
	double			acc;
};

static inline void
mp1e_timestamp_init(struct tfmem *m, double ref)
{
	m->ref = ref;
	m->err = 0.0;
	m->ecr = 0.0;
	m->acc = 0.0;
}

static inline double
mp1e_timestamp_filter(struct tfmem *m, double dt,
		      double a, double b, double c)
{
	/* NB a,b,c are const */
	double err, vel, acc;

	/* error */
	err = dt - m->ref;

	/* integrated error */
	err = (err - m->err) * a + m->err;

	/* error change */
	vel = err - m->err;
	m->err = err;

	/* change rate */
	acc = vel - m->acc;

	if (fabs(acc - m->ecr) < b) {
		m->ref += c * vel;
		m->ref += c * c * c * c * err;
		m->acc = acc;
	}

	m->ecr = acc;

#if 0
	fprintf(stderr, "dt %+f err %+f vel %+f acc %+f ref %f\n",
		dt, err, vel, acc, m->ref);
#endif
	return m->ref;
}

#endif /* SYNC_H */
