/*
 *  libzvbi - VBI device simulation
 *
 *  Copyright (C) 2004 Michael H. Schimek
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

/* $Id: io-sim.h,v 1.1.2.5 2006-05-14 14:14:11 mschimek Exp $ */

#ifndef __ZVBI3_IO_SIM_H__
#define __ZVBI3_IO_SIM_H__

#include "macros.h"
#include "sampling_par.h"
#include "io.h"

VBI3_BEGIN_DECLS

extern vbi3_bool
_vbi3_test_image_video		(uint8_t *		raw,
				 unsigned long		raw_size,
				 const vbi3_sampling_par *sp,
				 unsigned int		pixel_mask,
				 const vbi3_sliced *	sliced,
				 unsigned int		sliced_lines);
extern vbi3_bool
_vbi3_test_image_vbi		(uint8_t *		raw,
				 unsigned int		raw_size,
				 const vbi3_sampling_par *sp,
				 const vbi3_sliced *	sliced,
				 unsigned int		sliced_lines);

extern vbi3_bool
_vbi3_capture_sim_load_vps	(vbi3_capture *		cap,
				 const vbi3_program_id *pid);
extern vbi3_bool
_vbi3_capture_sim_load_wss_625	(vbi3_capture *		cap,
				 const vbi3_aspect_ratio *ar);
extern vbi3_bool
_vbi3_capture_sim_load_caption	(vbi3_capture *		cap,
				 const char *		stream,
				 vbi3_bool		append);
extern vbi3_capture *
_vbi3_capture_sim_new		(int			scanning,
				 unsigned int *		services,
				 vbi3_bool		interlaced,
				 vbi3_bool		synchronous);

VBI3_END_DECLS

#endif /* __ZVBI3_IO_SIM_H__ */
