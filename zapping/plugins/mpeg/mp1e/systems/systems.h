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

/* $Id: systems.h,v 1.1 2000-07-04 17:40:20 garetxe Exp $ */

#ifndef __SYSTEMS_H__
#define __SYSTEMS_H__

#include "output.h"

extern pthread_mutex_t		mux_mutex;
extern pthread_cond_t		mux_cond;
extern int			bytes_out;
extern unsigned char *		mux_buffer;
extern double			system_idle;

extern double			get_idle(void);
extern void			stream_output(fifo *fifo);
extern void			mpeg1_system_run_in(void);
extern void *			mpeg1_system_mux(void *unused);

#endif
