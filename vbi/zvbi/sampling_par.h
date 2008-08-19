/*
 *  libzvbi - Raw VBI sampling parameters
 *
 *  Copyright (C) 2000-2008 Michael H. Schimek
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the 
 *  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, 
 *  Boston, MA  02110-1301  USA.
 */

/* $Id: sampling_par.h,v 1.1.2.1 2008-08-19 10:56:06 mschimek Exp $ */

#ifndef __ZVBI_SAMPLING_PAR_H__
#define __ZVBI_SAMPLING_PAR_H__

#include "zvbi/macros.h"

VBI_BEGIN_DECLS

/**
 * @addtogroup Sampling
 * @{
 */

/**
 * Video standard identifier.
 */
typedef enum {
	VBI_VIDEOSTD_PAL_B	= (1 << 0),
	VBI_VIDEOSTD_PAL_B1	= (1 << 1),
	VBI_VIDEOSTD_PAL_G	= (1 << 2),
	VBI_VIDEOSTD_PAL_H	= (1 << 3),
	VBI_VIDEOSTD_PAL_I	= (1 << 4),
	VBI_VIDEOSTD_PAL_D	= (1 << 5),
	VBI_VIDEOSTD_PAL_D1	= (1 << 6),
	VBI_VIDEOSTD_PAL_K	= (1 << 7),

	VBI_VIDEOSTD_PAL_M	= (1 << 8),
	VBI_VIDEOSTD_PAL_N	= (1 << 9),
	VBI_VIDEOSTD_PAL_NC	= (1 << 10),
	VBI_VIDEOSTD_PAL_60	= (1 << 11),

	VBI_VIDEOSTD_NTSC_M	= (1 << 12),
	VBI_VIDEOSTD_NTSC_M_JP	= (1 << 13),
	VBI_VIDEOSTD_NTSC_443	= (1 << 14),
	VBI_VIDEOSTD_NTSC_M_KR	= (1 << 15),

	VBI_VIDEOSTD_SECAM_B	= (1 << 16),
	VBI_VIDEOSTD_SECAM_D	= (1 << 17),
	VBI_VIDEOSTD_SECAM_G	= (1 << 18),
	VBI_VIDEOSTD_SECAM_H	= (1 << 19),
	VBI_VIDEOSTD_SECAM_K	= (1 << 20),
	VBI_VIDEOSTD_SECAM_K1	= (1 << 21),
	VBI_VIDEOSTD_SECAM_L	= (1 << 22),
	VBI_VIDEOSTD_SECAM_LC	= (1 << 23)
} vbi_videostd_set;

#define VBI_VIDEOSTD_SET_EMPTY		0
#define VBI_VIDEOSTD_SET_PAL_BG		(VBI_VIDEOSTD_PAL_B |		\
					 VBI_VIDEOSTD_PAL_B1 |		\
					 VBI_VIDEOSTD_PAL_G)
#define VBI_VIDEOSTD_SET_PAL_DK		(VBI_VIDEOSTD_PAL_D |		\
				 	 VBI_VIDEOSTD_PAL_D1 |		\
					 VBI_VIDEOSTD_PAL_K)
#define VBI_VIDEOSTD_SET_PAL		(VBI_VIDEOSTD_SET_PAL_BG |	\
				 	 VBI_VIDEOSTD_SET_PAL_DK |	\
					 VBI_VIDEOSTD_PAL_H |		\
					 VBI_VIDEOSTD_PAL_I)
#define VBI_VIDEOSTD_SET_NTSC		(VBI_VIDEOSTD_NTSC_M |		\
					 VBI_VIDEOSTD_NTSC_M_JP |	\
					 VBI_VIDEOSTD_NTSC_M_KR)
#define VBI_VIDEOSTD_SET_SECAM_DK	(VBI_VIDEOSTD_SECAM_D |		\
					 VBI_VIDEOSTD_SECAM_K |		\
					 VBI_VIDEOSTD_SECAM_K1)
#define VBI_VIDEOSTD_SET_SECAM		(VBI_VIDEOSTD_SECAM_B |		\
					 VBI_VIDEOSTD_SECAM_G |		\
					 VBI_VIDEOSTD_SECAM_H |		\
					 VBI_VIDEOSTD_SET_SECAM_DK |	\
					 VBI_VIDEOSTD_SECAM_L |		\
					 VBI_VIDEOSTD_SECAM_LC)
#define VBI_VIDEOSTD_SET_525_60		(VBI_VIDEOSTD_PAL_M |		\
					 VBI_VIDEOSTD_PAL_60 |		\
					 VBI_VIDEOSTD_SET_NTSC |	\
					 VBI_VIDEOSTD_SET_NTSC_443)
#define VBI_VIDEOSTD_SET_625_50		(VBI_VIDEOSTD_SET_PAL |		\
					 VBI_VIDEOSTD_PAL_N |		\
					 VBI_VIDEOSTD_PAL_NC |		\
					 VBI_VIDEOSTD_SET_SECAM)
#define VBI_VIDEOSTD_SET_ALL            (VBI_VIDEOSTD_SET_525_60 |	\
				 	 VBI_VIDEOSTD_SET_625_50)

VBI_END_DECLS

#endif /* __ZVBI_SAMPLING_PAR_H__ */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
