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

/* $Id: sync.h,v 1.4 2001-09-20 23:35:07 mschimek Exp $ */

#ifndef SYNC_H
#define SYNC_H

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
	double			elapsed;

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
extern int	mp1e_sync_break(synchr_stream *str, double cap_time, double cap_period, double *drift);

#endif /* SYNC_H */
