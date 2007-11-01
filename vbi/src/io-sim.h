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

/* $Id: io-sim.h,v 1.1.2.8 2007-11-01 00:21:23 mschimek Exp $ */

#ifndef __ZVBI3_IO_SIM_H__
#define __ZVBI3_IO_SIM_H__

#include "macros.h"
#include "version.h"
#include "sampling_par.h"
#include "io.h"

#if 3 == VBI_VERSION_MINOR
#  include "aspect_ratio.h"
#  include "pdc.h"
#endif

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
vbi3_raw_add_noise		(uint8_t *		raw,
				 const vbi3_sampling_par *sp,
				 unsigned int		min_freq,
				 unsigned int		max_freq,
				 unsigned int		amplitude,
				 unsigned int		seed);
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
extern void
vbi3_capture_sim_add_noise	(vbi3_capture *		cap,
				 unsigned int		min_freq,
				 unsigned int		max_freq,
				 unsigned int		amplitude);
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

extern unsigned int
_vbi3_capture_sim_get_flags	(vbi3_capture *		cap);
extern void
_vbi3_capture_sim_set_flags	(vbi3_capture *		cap,
				 unsigned int		flags);

#define _VBI3_RAW_SWAP_FIELDS	(1 << 0)
#define _VBI3_RAW_SHIFT_CC_CRI	(1 << 1)
#define _VBI3_RAW_LOW_AMP_CC	(1 << 2)

/* NB. Currently this flag has no effect in _vbi3_raw_*_image().
   Call vbi3_raw_add_noise() instead. */
#define _VBI3_RAW_NOISE_2	(1 << 17)

extern vbi3_bool
_vbi3_raw_video_image		(uint8_t *		raw,
				 unsigned long		raw_size,
				 const vbi3_sampling_par *sp,
				 int			blank_level,
				 int			black_level,
				 int			white_level,
				 unsigned int		pixel_mask,
				 unsigned int		flags,
				 const vbi3_sliced *	sliced,
				 unsigned int		n_sliced_lines);
extern vbi3_bool
_vbi3_raw_vbi_image		(uint8_t *		raw,
				 unsigned long		raw_size,
				 const vbi3_sampling_par *sp,
				 int			blank_level,
				 int			white_level,
				 unsigned int		flags,
				 const vbi3_sliced *	sliced,
				 unsigned int		n_sliced_lines);

VBI3_END_DECLS

#endif /* __ZVBI3_IO_SIM_H__ */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
