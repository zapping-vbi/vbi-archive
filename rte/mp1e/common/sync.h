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

/* $Id: sync.h,v 1.9 2002-02-25 06:22:19 mschimek Exp $ */

#ifndef SYNC_H
#define SYNC_H

#include <math.h>

#include "threads.h"
#include "fifo.h"
#include "log.h"

typedef unsigned int sync_set;

typedef struct {
	mucon			mucon;

	double			start_time;
	double			stop_time;
	double			front_time;

	sync_set		modules;
	sync_set		vote;

	double			ref_warp;
	sync_set		time_base;
} sync_main;

typedef struct {
	sync_main *		main;
	sync_set		this_module;

	double			start_ref;

	double			byte_period;
	double			frame_period;

	int			bytes_per_sample;	/* power of 2 */
} sync_stream;

extern void mp1e_sync_init(sync_main *, sync_set modules, sync_set time_base);
extern bool mp1e_sync_start(sync_main *, double time);
extern bool mp1e_sync_stop(sync_main *, double time);
extern bool mp1e_sync_run_in(sync_main *, sync_stream *, consumer *, int *frame_frac);

static inline int
mp1e_sync_break(sync_stream *str, double time)
{
	sync_main *mn = str->main;

	pthread_mutex_lock(&mn->mucon.mutex);

	if (time >= str->main->stop_time) {
		pthread_mutex_unlock(&mn->mucon.mutex);

		printv(4, "sync_break %08x, %f, stop_time %f\n",
		       str->this_module, time, mn->stop_time);

		return TRUE;
	}

	if (time > mn->front_time)
		mn->front_time = time;

	pthread_mutex_unlock(&mn->mucon.mutex);

	return FALSE;
}

static inline double
mp1e_sync_drift(sync_stream *str, double time, double elapsed)
{
	sync_main *mn = str->main;
	double drift;

	pthread_mutex_lock(&mn->mucon.mutex);

	printv(4, "SD%02d %f i%f o%f d%f\n", str->this_module,
	       time, (time - str->start_ref), elapsed,
	       (time - str->start_ref) - elapsed);

	if (str->this_module == mn->time_base) {
		if (elapsed >= 0.5) {
			double warp = (time - str->start_ref) / elapsed;
			mn->ref_warp += (warp - mn->ref_warp) * 0.1;
		}

		drift = 0.0;

		printv(4, "SD%02d ref_warp %f, %f s\n",
		       str->this_module, mn->ref_warp,
		       (time - str->start_ref) - elapsed);
	} else {
		double ref_time = (time - str->start_ref) * mn->ref_warp;

		drift = elapsed - ref_time;

		printv(4, "SD%02d ref %f o%f, drift %+9.6f s, "
		       "%+9.6f ppm, %+9.6f units\n",
		       str->this_module, ref_time, elapsed, drift,
		       elapsed * 1e6 / ref_time - 1e6,
		       drift / (str->frame_period + str->byte_period));
	}

	pthread_mutex_unlock(&mn->mucon.mutex);

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
	fprintf(stderr, "tf dt %+f err %+f vel %+f acc %+f ref %f\n",
		dt, err, vel, acc, m->ref);
#endif
	return m->ref;
}

#endif /* SYNC_H */
