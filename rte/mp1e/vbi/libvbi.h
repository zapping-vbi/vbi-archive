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

/* $Id: libvbi.h,v 1.3 2001-08-22 01:28:09 mschimek Exp $ */

#ifndef __LIBVBI_H__
#define __LIBVBI_H__

#include "../common/fifo.h"
#include "../systems/libsystems.h"
#include "v4lx.h"

extern void		vbi_init(fifo *f, multiplexer *mux);
extern void *		vbi_thread(void *f); // XXX

#endif /* libvbi.h */
