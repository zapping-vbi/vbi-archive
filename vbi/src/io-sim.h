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

/* $Id: io-sim.h,v 1.1.2.2 2004-04-08 23:36:25 mschimek Exp $ */

#ifndef __ZVBI_IO_SIM_H__
#define __ZVBI_IO_SIM_H__

#include "macros.h"
#include "sampling.h"

VBI_BEGIN_DECLS

extern vbi_bool
_vbi_test_image_video		(uint8_t *		raw,
				 const vbi_sampling_par *sp,
				 unsigned int		pixel_mask,
				 const vbi_sliced *	sliced,
				 unsigned int		sliced_lines);
extern vbi_bool
_vbi_test_image_vbi		(uint8_t *		raw,
				 const vbi_sampling_par *sp,
				 const vbi_sliced *	sliced,
				 unsigned int		sliced_lines);

VBI_END_DECLS

#endif /* __ZVBI_IO_SIM_H__ */
