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

/* $Id: sync.c,v 1.8 2002-12-14 00:43:44 mschimek Exp $ */

#include "../common/log.h"
#include "sync.h"
#include "math.h"

void
mp1e_sync_init(sync_main *mn, sync_set modules, sync_set time_base)
{
	mucon_init(&mn->mucon);

	mn->start_time = DBL_MAX;
	mn->stop_time = DBL_MAX;

	mn->front_time = -1;

	mn->modules = modules;
	mn->vote = 0;

	assert(popcnt(time_base) <= 1);

	mn->time_base = time_base;

	mn->ref_warp = 1.0;
}

/**
 * mp1e_sync_start:
 * @time: 
 * 
 * Trigger initial synchronization at @time.
 * 
 * Return value: 
 **/
rte_bool
mp1e_sync_start(sync_main *mn, double time)
{
	pthread_mutex_lock(&mn->mucon.mutex);

	if (mn->modules == mn->vote) {
		pthread_mutex_unlock(&mn->mucon.mutex);
		return FALSE;
	}

	mn->start_time = time;

	pthread_cond_broadcast(&mn->mucon.cond);
	pthread_mutex_unlock(&mn->mucon.mutex);

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
rte_bool
mp1e_sync_stop(sync_main *mn, double time)
{
	pthread_mutex_lock(&mn->mucon.mutex);

	if (mn->modules != mn->vote ||
	    mn->stop_time < DBL_MAX) {
		pthread_mutex_unlock(&mn->mucon.mutex);
		return FALSE;
	}

	mn->stop_time = MAX(time, mn->front_time);

	printv(4, "sync_stop at %f\n", mn->stop_time);

	pthread_mutex_unlock(&mn->mucon.mutex);

	return TRUE;
}

#define VT(d) (((d) > DBL_MAX / 2.0) ? -1 : (d))

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
rte_bool
mp1e_sync_run_in(sync_main *mn, sync_stream *str, consumer *c, int *frame_frac)
{
	double first_time, last_time = -1;
	double frame_period;
	buffer *b;

	str->main = mn;

	c->fifo->start(c->fifo);

	b = wait_full_buffer(c);

	if (b->used <= 0)
		FAIL("Premature end of file (%s)", c->fifo->name);

	pthread_mutex_lock(&mn->mucon.mutex);

	printv(4, "SRI0 %02x: %f %f %f %d\n",
	       str->this_module, str->start_ref,
	       str->byte_period, str->frame_period,
	       str->bytes_per_sample);

	first_time = b->time;

	if (frame_frac)
		*frame_frac = 0;

	for (;;) {
		if (b->time == 0.0) { // offline
			if (mn->start_time < DBL_MAX) {
				printv(4, "SRI %02x: accept start_time %f for %f, voted %02x/%02x\n",
				       str->this_module, VT(mn->start_time), b->time, mn->vote, mn->modules);
				if ((mn->vote |= str->this_module) == mn->modules)
					break;
				pthread_cond_broadcast(&mn->mucon.cond);
			}
			pthread_cond_wait(&mn->mucon.cond, &mn->mucon.mutex);
			continue;
		}

		/* FIXME must not FAIL w/rte */
		if (0) // because rte sends in duplicate b->time, but that's ok.
		if (b->time <= last_time)
			FAIL("Invalid timestamps from %s: ..., %f, %f\n",
				c->fifo->name, last_time, b->time);
		if ((b->time - first_time) > 2.0) {
			FAIL("Unable to sync %s after %f secs\n",
				c->fifo->name, VT(b->time - first_time));
		}

		last_time = b->time;

		if (mn->start_time < b->time) {
			printv(4, "SRI %02x: propose start_time %f, was %f\n",
				str->this_module, b->time, VT(mn->start_time));
			mn->start_time = b->time;
			mn->vote = str->this_module;
			if (str->this_module == mn->modules)
				break;
			pthread_cond_broadcast(&mn->mucon.cond);
			pthread_cond_wait(&mn->mucon.cond, &mn->mucon.mutex);
			continue;
		}

		frame_period = str->frame_period + b->used * str->byte_period;

		if (mn->start_time < b->time + frame_period) {
			printv(4, "SRI %02x: accept start_time %f for %f, voted %02x/%02x\n",
				str->this_module, VT(mn->start_time), b->time, mn->vote, mn->modules);
			if ((mn->vote |= str->this_module) == mn->modules) {
				if (frame_frac && str->byte_period > 0.0) {
					double tfrac = mn->start_time - b->time;
					int sfrac = tfrac / str->byte_period;

					*frame_frac = saturate(sfrac & -str->bytes_per_sample, 0, b->used - 1);
				}

				break;
			}
		}

		printv(4, "SRI %02x: disagree start_time %f, discard %f\n",
			str->this_module, VT(mn->start_time), b->time);

		mn->vote &= ~str->this_module;

		pthread_mutex_unlock(&mn->mucon.mutex);

		send_empty_buffer(c, b);

		b = wait_full_buffer(c);

		if (b->used <= 0)
			FAIL("Capture failure");

		pthread_mutex_lock(&mn->mucon.mutex);
	}

	str->start_ref = b->time;

	pthread_mutex_unlock(&mn->mucon.mutex);
	pthread_cond_broadcast(&mn->mucon.cond);

	unget_full_buffer(c, b);

	return TRUE;
}
