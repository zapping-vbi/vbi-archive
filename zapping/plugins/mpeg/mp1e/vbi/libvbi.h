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

/* $Id: libvbi.h,v 1.2 2000-11-08 05:54:37 mschimek Exp $ */

#ifndef __LIBVBI_H__
#define __LIBVBI_H__

#include "../common/fifo.h"

extern fifo *		open_vbi_v4l2(char *dev_name);
extern void		close_vbi_v4l2(fifo *f);

extern void		vbi_init(fifo *f);
extern void *		vbi_thread(void *f); // XXX

#endif /* libvbi.h */
