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

/* $Id: libvideo.h,v 1.2 2001-10-07 10:55:51 mschimek Exp $ */

#include "../rtepriv.h"
#include "../systems/libsystems.h"

extern rte_codec_class	mp1e_mpeg1_video_codec;

extern void		video_init(int cpu_type,
				   int coded_width, int coded_height,
				   int motion_min, int motion_max,
				   unsigned int module, multiplexer *mux);
