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

/* $Id: remote.c,v 1.1.1.1 2001-08-07 22:10:11 garetxe Exp $ */

#include "../common/log.h"
#include "remote.h"
#include "math.h"

struct remote remote;

void
remote_init(unsigned int modules)
{
	mucon_init(&remote.mucon);

	remote.start_time = DBL_MAX;
	remote.stop_time = DBL_MAX;

	remote.front_time = -1;

	remote.modules = modules;
	remote.vote = 0;	
}

bool
remote_start(double time)
{
	pthread_mutex_lock(&remote.mucon.mutex);

	if (remote.modules == remote.vote) {
		pthread_mutex_unlock(&remote.mucon.mutex);
		return FALSE;
	}

	remote.start_time = time;

	pthread_cond_broadcast(&remote.mucon.cond);

	pthread_mutex_unlock(&remote.mucon.mutex);

	return TRUE;
}

bool
remote_stop(double time)
{
	pthread_mutex_lock(&remote.mucon.mutex);

	if (remote.modules != remote.vote ||
	    remote.stop_time < DBL_MAX) {
		pthread_mutex_unlock(&remote.mucon.mutex);
		return FALSE;
	}

	remote.stop_time = MAX(time, remote.front_time);

	printv(4, "remote_stop at %f\n", remote.stop_time);

	pthread_mutex_unlock(&remote.mucon.mutex);

	return TRUE;
}

bool
remote_sync(consumer *c, unsigned int this_module, double frame_period)
{
	double last_time = -1;
	buffer2 *b;

	c->fifo->start(c->fifo);

	b = wait_full_buffer2(c);

	if (b->used <= 0)
		FAIL("Premature end of file (%s)", c->fifo->name);

	pthread_mutex_lock(&remote.mucon.mutex);

	for (;;) { // XXX add timeout
		if (b->time == 0.0) { // offline
			if (remote.start_time < DBL_MAX) {
				printv(4, "RS %02x: accept start_time %f for %f, voted %02x/%02x\n",
					this_module, remote.start_time, b->time,
					remote.vote, remote.modules);
				if ((remote.vote |= this_module) == remote.modules)
					break;
				pthread_cond_broadcast(&remote.mucon.cond);
			}
			pthread_cond_wait(&remote.mucon.cond, &remote.mucon.mutex);
			continue;
		}
if (0)
		if (b->time <= last_time)
			FAIL("Invalid timestamps from %s: ..., %f, %f\n",
				c->fifo->name, last_time, b->time);

		last_time = b->time;

		if (remote.start_time < b->time) {
			printv(4, "RS %02x: propose start_time %f, was %f\n",
				this_module, b->time, remote.start_time);
			remote.start_time = b->time;
			remote.vote = this_module;
			if (this_module == remote.modules)
				break;
			pthread_cond_broadcast(&remote.mucon.cond);
			pthread_cond_wait(&remote.mucon.cond, &remote.mucon.mutex);
			continue;
		}

		if (remote.start_time < b->time + frame_period) {
			printv(4, "RS %02x: accept start_time %f for %f, voted %02x/%02x\n",
				this_module, remote.start_time, b->time,
				remote.vote, remote.modules);
			if ((remote.vote |= this_module) == remote.modules)
				break;
		}

		printv(4, "RS %02x: disagree start_time %f, discard %f\n",
			this_module, remote.start_time, b->time);

		remote.vote &= ~this_module;

		pthread_mutex_unlock(&remote.mucon.mutex);

		send_empty_buffer2(c, b);

		b = wait_full_buffer2(c);

		if (b->used <= 0)
			FAIL("Capture failure");

		pthread_mutex_lock(&remote.mucon.mutex);
	}

	pthread_mutex_unlock(&remote.mucon.mutex);
	pthread_cond_broadcast(&remote.mucon.cond);

	unget_full_buffer2(c, b);

	return TRUE;
}
