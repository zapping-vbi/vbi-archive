/*
 *  V4L/V4L2 VBI Interface
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

/* $Id: v4lx.h,v 1.8 2001-08-22 01:26:53 mschimek Exp $ */

#include "../common/fifo.h"

extern fifo *		vbi_open_v4lx(char *dev_name, int given_fd, int buffered, int fifo_depth);
extern void		vbi_close_v4lx(fifo *f);
