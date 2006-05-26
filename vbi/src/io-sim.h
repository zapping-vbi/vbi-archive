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

/* $Id: io-sim.h,v 1.1.2.7 2006-05-26 00:43:05 mschimek Exp $ */

#ifndef __ZVBI3_IO_SIM_H__
#define __ZVBI3_IO_SIM_H__

#include "macros.h"
#include "version.h"
#include "sampling_par.h"
#include "io.h"

VBI3_BEGIN_DECLS

/* Public */

/**
 * @addtogroup Rawenc
 * @{
 */
extern vbi3_bool
vbi3_raw_video_image		(uint8_t *		raw,
				 unsigned long		raw_size,
				 const vbi3_sampling_par *sp,
				 int			blank_level,
				 int			black_level,
				 int			white_level,
				 unsigned int		pixel_mask,
				 vbi3_bool		swap_fields,
				 const vbi3_sliced *	sliced,
				 unsigned int		n_sliced_lines);
extern vbi3_bool
vbi3_raw_vbi_image		(uint8_t *		raw,
				 unsigned long		raw_size,
				 const vbi3_sampling_par *sp,
				 int			blank_level,
				 int			white_level,
				 vbi3_bool		swap_fields,
				 const vbi3_sliced *	sliced,
				 unsigned int		n_sliced_lines);
/** @} */
/**
 * @addtogroup Device
 * @{
 */
#if 3 == VBI_VERSION_MINOR
extern vbi3_bool
vbi3_capture_sim_load_vps	(vbi3_capture *		cap,
				 const vbi3_program_id *pid);
extern vbi3_bool
vbi3_capture_sim_load_wss_625	(vbi3_capture *		cap,
				 const vbi3_aspect_ratio *ar);
extern vbi3_bool
vbi3_capture_sim_load_caption	(vbi3_capture *		cap,
				 const char *		stream,
				 vbi3_bool		append);
#endif
extern void
vbi3_capture_sim_decode_raw	(vbi3_capture *		cap,
				 vbi3_bool		enable);
extern vbi3_capture *
vbi3_capture_sim_new		(int			scanning,
				 unsigned int *		services,
				 vbi3_bool		interlaced,
				 vbi3_bool		synchronous);
/** @} */

/* Private */

VBI3_END_DECLS

#endif /* __ZVBI3_IO_SIM_H__ */
