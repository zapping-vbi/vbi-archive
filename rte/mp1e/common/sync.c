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

/* $Id: sync.c,v 1.4 2001-09-20 23:35:07 mschimek Exp $ */

#include "../common/log.h"
#include "sync.h"
#include "math.h"

struct synchr synchr;

void
mp1e_sync_init(unsigned int modules, unsigned int time_base)
{
	mucon_init(&synchr.mucon);

	synchr.start_time = DBL_MAX;
	synchr.stop_time = DBL_MAX;

	synchr.front_time = -1;

	synchr.modules = modules;
	synchr.vote = 0;

	assert(popcnt(time_base) <= 1);

	synchr.time_base = time_base;
}

/**
 * mp1e_sync_start:
 * @time: 
 * 
 * Trigger initial synchronization at @time.
 * 
 * Return value: 
 **/
bool
mp1e_sync_start(double time)
{
	pthread_mutex_lock(&synchr.mucon.mutex);

	if (synchr.modules == synchr.vote) {
		pthread_mutex_unlock(&synchr.mucon.mutex);
		return FALSE;
	}

	synchr.start_time = time;

	pthread_cond_broadcast(&synchr.mucon.cond);

	pthread_mutex_unlock(&synchr.mucon.mutex);

	return TRUE;
}

/**
 * mp1e_sync_stop:
 * @time: 
 * 
 * Stop encoding at @time.
 * 
 * Return value: 
 **/
bool
mp1e_sync_stop(double time)
{
	pthread_mutex_lock(&synchr.mucon.mutex);

	if (synchr.modules != synchr.vote ||
	    synchr.stop_time < DBL_MAX) {
		pthread_mutex_unlock(&synchr.mucon.mutex);
		return FALSE;
	}

	synchr.stop_time = MAX(time, synchr.front_time);

	printv(4, "sync_stop at %f\n", synchr.stop_time);

	pthread_mutex_unlock(&synchr.mucon.mutex);

	return TRUE;
}

/**
 * mp1e_sync_run_in:
 * @str: 
 * @c: 
 * @frame_frac: 
 * 
 * Initial synchronization for *encoding modules*.
 * 
 * Return value: 
 **/
bool
mp1e_sync_run_in(synchr_stream *str, consumer *c, int *frame_frac)
{
	double first_time, last_time = -1;
	double frame_period;
	buffer *b;

	c->fifo->start(c->fifo);

	b = wait_full_buffer(c);

	if (b->used <= 0)
		FAIL("Premature end of file (%s)", c->fifo->name);

	pthread_mutex_lock(&synchr.mucon.mutex);

	first_time = b->time;

	if (frame_frac)
		*frame_frac = 0;

	for (;;) {
		if (b->time == 0.0) { // offline
			if (synchr.start_time < DBL_MAX) {
				printv(4, "SRI %02x: accept start_time %f for %f, voted %02x/%02x\n",
					str->this_module, synchr.start_time, b->time,
					synchr.vote, synchr.modules);
				if ((synchr.vote |= str->this_module) == synchr.modules)
					break;
				pthread_cond_broadcast(&synchr.mucon.cond);
			}
			pthread_cond_wait(&synchr.mucon.cond, &synchr.mucon.mutex);
			continue;
		}

		// XXX return FALSE
		if (0) // because rte sends in duplicate b->time, but that's ok.
		if (b->time <= last_time)
			FAIL("Invalid timestamps from %s: ..., %f, %f\n",
				c->fifo->name, last_time, b->time);
		if ((b->time - first_time) > 2.0)
			FAIL("Unable to sync %s after %f secs\n",
				c->fifo->name, b->time - first_time);

		last_time = b->time;

		if (synchr.start_time < b->time) {
			printv(4, "SRI %02x: propose start_time %f, was %f\n",
				str->this_module, b->time, synchr.start_time);
			synchr.start_time = b->time;
			synchr.vote = str->this_module;
			if (str->this_module == synchr.modules)
				break;
			pthread_cond_broadcast(&synchr.mucon.cond);
			pthread_cond_wait(&synchr.mucon.cond, &synchr.mucon.mutex);
			continue;
		}

		frame_period = str->frame_period + b->used * str->byte_period;

		if (synchr.start_time < b->time + frame_period) {
			printv(4, "SRI %02x: accept start_time %f for %f, voted %02x/%02x\n",
				str->this_module, synchr.start_time, b->time,
				synchr.vote, synchr.modules);
			if ((synchr.vote |= str->this_module) == synchr.modules) {
				if (frame_frac && str->byte_period > 0.0) {
					double tfrac = synchr.start_time - b->time;
					int sfrac = tfrac / str->byte_period;

					// if (tfrac > 1 / 10.0)
					*frame_frac = saturate(sfrac & -str->bytes_per_sample, 0, b->used - 1);
				}

				break;
			}
		}

		printv(4, "SRI %02x: disagree start_time %f, discard %f\n",
			str->this_module, synchr.start_time, b->time);

		synchr.vote &= ~str->this_module;

		pthread_mutex_unlock(&synchr.mucon.mutex);

		send_empty_buffer(c, b);

		b = wait_full_buffer(c);

		if (b->used <= 0)
			FAIL("Capture failure");

		pthread_mutex_lock(&synchr.mucon.mutex);
	}

	str->start_ref = b->time;
	str->elapsed = 0;

	pthread_mutex_unlock(&synchr.mucon.mutex);
	pthread_cond_broadcast(&synchr.mucon.cond);

	unget_full_buffer(c, b);

	return TRUE;
}

/**
 * mp1e_sync_break:
 * @str: 
 * @time: 
 * @bytes: 
 * @driftp: 
 * 
 * Runtime and terminal synchronization for *encoding modules*.
 * 
 * Return value: 
 **/
int
mp1e_sync_break(synchr_stream *str, double cap_time, double cap_period, double *drift)
{
	pthread_mutex_lock(&synchr.mucon.mutex);

	if (cap_time >= synchr.stop_time) {
		pthread_mutex_unlock(&synchr.mucon.mutex);

		printv(4, "sync_break %08x, %f, stop_time %f\n",
		       str->this_module, cap_time, synchr.stop_time);

		return TRUE;
	}

	if (cap_time > synchr.front_time)
		synchr.front_time = cap_time;

	printv(4, "SB%02d %f:%f i%f o%f d%f\n", str->this_module,
	       cap_time, cap_period,
	       (cap_time - str->start_ref), str->elapsed,
	       (cap_time - str->start_ref) - str->elapsed);

	if (str->this_module == synchr.time_base) {
		// XXX improve
		synchr.ref_warp = (cap_time - str->start_ref) - str->elapsed;

		if (drift) *drift = 0.0;

		printv(4, "SB%02d ref_warp %f s, %f ppm\n",
		       str->this_module, synchr.ref_warp,
		       str->elapsed * 1e6 / (cap_time - str->start_ref) - 1e6);
	} else if (drift) {
		double ref_time = cap_time + synchr.ref_warp - str->start_ref;

		*drift = str->elapsed - ref_time;

		printv(4, "SB%02d ref %f, drift %f s, %f ppm, %f units\n",
		       str->this_module, ref_time, *drift,
		       str->elapsed * 1e6 / ref_time - 1e6,
		       *drift / (str->frame_period + str->byte_period));
	}

	str->elapsed += cap_period;

	pthread_mutex_unlock(&synchr.mucon.mutex);

	return FALSE;
}
