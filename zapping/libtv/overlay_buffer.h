/*
 *  Copyright (C) 2001-2004 Michael H. Schimek
 *  Copyright (C) 2000-2003 Iñaki García Etxebarria
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

/* $Id: overlay_buffer.h,v 1.1 2004-09-10 04:55:55 mschimek Exp $ */

#ifndef __ZTV_OVERLAY_BUFFER_H__
#define __ZTV_OVERLAY_BUFFER_H__

#include "image_format.h"

TV_BEGIN_DECLS

/* This is the target of DMA overlay, a continuous chunk of physical memory.
   Usually it describes the visible portion of the graphics card's video
   memory. */
typedef struct {
  	/* Memory address as seen by the video capture device, without
	   virtual address translation by the CPU. Actually this assumes
           graphic card and capture device share an address space, which is
	   not necessarily true if the devices connect to different busses,
	   but I'm not aware of any driver APIs considering this either. */
	unsigned long		base;

	/* Base need not align with point 0, 0, e.g. Xinerama. */
	unsigned int		x;
	unsigned int		y;

	tv_image_format		format;
} tv_overlay_buffer;

TV_END_DECLS

#endif /* __ZTV_OVERLAY_BUFFER_H__ */
