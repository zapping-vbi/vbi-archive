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

/* $Id: sync.c,v 1.3 2001-09-03 05:26:07 mschimek Exp $ */

#include "../common/log.h"
#include "sync.h"
#include "math.h"

struct synchr synchr;

void
sync_init(unsigned int modules)
{
	mucon_init(&synchr.mucon);

	synchr.start_time = DBL_MAX;
	synchr.stop_time = DBL_MAX;

	synchr.front_time = -1;

	synchr.modules = modules;
	synchr.vote = 0;	
}

bool
sync_start(double time)
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

bool
sync_stop(double time)
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

bool
sync_sync(consumer *c, unsigned int this_module, double sample_period, int bytes_per_sample)
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

	for (;;) {
		if (b->time == 0.0) { // offline
			if (synchr.start_time < DBL_MAX) {
				printv(4, "SS %02x: accept start_time %f for %f, voted %02x/%02x\n",
					this_module, synchr.start_time, b->time,
					synchr.vote, synchr.modules);
				if ((synchr.vote |= this_module) == synchr.modules)
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
			printv(4, "SS %02x: propose start_time %f, was %f\n",
				this_module, b->time, synchr.start_time);
			synchr.start_time = b->time;
			synchr.vote = this_module;
			if (this_module == synchr.modules)
				break;
			pthread_cond_broadcast(&synchr.mucon.cond);
			pthread_cond_wait(&synchr.mucon.cond, &synchr.mucon.mutex);
			continue;
		}

		if (bytes_per_sample == 0)
			frame_period = sample_period;
		else
			frame_period = b->used * sample_period / bytes_per_sample;

		// XXX should return lag in samples for more accurate syncing
		if (synchr.start_time < b->time + frame_period) {
			printv(4, "SS %02x: accept start_time %f for %f, voted %02x/%02x\n",
				this_module, synchr.start_time, b->time,
				synchr.vote, synchr.modules);
			if ((synchr.vote |= this_module) == synchr.modules)
				break;
		}

		printv(4, "SS %02x: disagree start_time %f, discard %f\n",
			this_module, synchr.start_time, b->time);

		synchr.vote &= ~this_module;

		pthread_mutex_unlock(&synchr.mucon.mutex);

		send_empty_buffer(c, b);

		b = wait_full_buffer(c);

		if (b->used <= 0)
			FAIL("Capture failure");

		pthread_mutex_lock(&synchr.mucon.mutex);
	}

	pthread_mutex_unlock(&synchr.mucon.mutex);
	pthread_cond_broadcast(&synchr.mucon.cond);

	unget_full_buffer(c, b);

	return TRUE;
}
