/*
 *  libzvbi - Raw VBI sampling
 *
 *  Copyright (C) 2000, 2001, 2002, 2003, 2004 Michael H. Schimek
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

/* $Id: sampling_par.h,v 1.1.2.3 2006-05-14 14:14:12 mschimek Exp $ */

#ifndef __ZVBI3_SAMPLING_PAR_H__
#define __ZVBI3_SAMPLING_PAR_H__

#include <inttypes.h>		/* uint64_t */
#include "macros.h"
#include "misc.h"
#include "image_format.h"	/* vbi3_pixfmt */
#include "sliced.h"		/* vbi3_service_set */

VBI3_BEGIN_DECLS

/**
 * @addtogroup Sampling
 * @{
 */

/**
 * Video standard identifier.
 */
typedef enum {
	VBI3_VIDEOSTD_NONE = 0,
	VBI3_VIDEOSTD_UNKNOWN = VBI3_VIDEOSTD_NONE,

     	VBI3_VIDEOSTD_PAL_B = 1,
	VBI3_VIDEOSTD_PAL_B1,
	VBI3_VIDEOSTD_PAL_G,
	VBI3_VIDEOSTD_PAL_H,

	VBI3_VIDEOSTD_PAL_I,
	VBI3_VIDEOSTD_PAL_D,
	VBI3_VIDEOSTD_PAL_D1,
	VBI3_VIDEOSTD_PAL_K,

	VBI3_VIDEOSTD_PAL_M = 9,
	VBI3_VIDEOSTD_PAL_N,
	VBI3_VIDEOSTD_PAL_NC,
	VBI3_VIDEOSTD_PAL_60,

	VBI3_VIDEOSTD_NTSC_M = 13,
	VBI3_VIDEOSTD_NTSC_M_JP,
	VBI3_VIDEOSTD_NTSC_443,
	VBI3_VIDEOSTD_NTSC_M_KR,

	VBI3_VIDEOSTD_SECAM_B = 17,
	VBI3_VIDEOSTD_SECAM_D,
	VBI3_VIDEOSTD_SECAM_G,
	VBI3_VIDEOSTD_SECAM_H,
	VBI3_VIDEOSTD_SECAM_K,
	VBI3_VIDEOSTD_SECAM_K1,
	VBI3_VIDEOSTD_SECAM_L,
	VBI3_VIDEOSTD_SECAM_LC,
} vbi3_videostd;

/**
 * A set of video standards is used where more than one
 * video standard may apply. Use @c VBI3_VIDEOSTD_SET macros
 * to build a set.
 *
 * Note these are the same values as in the V4L2 API, except shifted
 * left one bit so we have room for @c VBI3_VIDEOSTD_UNKNOWN.
 */
typedef unsigned int vbi3_videostd_set;

#define VBI3_VIDEOSTD_SET(videostd) (((vbi3_videostd_set) 1) << (videostd))

#define VBI3_VIDEOSTD_SET_EMPTY 0
#define VBI3_VIDEOSTD_SET_UNKNOWN	 VBI3_VIDEOSTD_SET (VBI3_VIDEOSTD_UNKNOWN)
#define VBI3_VIDEOSTD_SET_PAL_BG		(VBI3_VIDEOSTD_SET (VBI3_VIDEOSTD_PAL_B)	    | \
					 VBI3_VIDEOSTD_SET (VBI3_VIDEOSTD_PAL_B1)	    | \
					 VBI3_VIDEOSTD_SET (VBI3_VIDEOSTD_PAL_G))
#define VBI3_VIDEOSTD_SET_PAL_DK		(VBI3_VIDEOSTD_SET (VBI3_VIDEOSTD_PAL_D)	    | \
					 VBI3_VIDEOSTD_SET (VBI3_VIDEOSTD_PAL_D1)	    | \
					 VBI3_VIDEOSTD_SET (VBI3_VIDEOSTD_PAL_K))
#define VBI3_VIDEOSTD_SET_PAL		(VBI3_VIDEOSTD_SET_PAL_BG		    | \
					 VBI3_VIDEOSTD_SET_PAL_DK		    | \
					 VBI3_VIDEOSTD_SET (VBI3_VIDEOSTD_PAL_H)	    | \
					 VBI3_VIDEOSTD_SET (VBI3_VIDEOSTD_PAL_I))
#define VBI3_VIDEOSTD_SET_NTSC		(VBI3_VIDEOSTD_SET (VBI3_VIDEOSTD_NTSC_M)	    | \
					 VBI3_VIDEOSTD_SET (VBI3_VIDEOSTD_NTSC_M_JP)  | \
					 VBI3_VIDEOSTD_SET (VBI3_VIDEOSTD_NTSC_M_KR))
#define VBI3_VIDEOSTD_SET_SECAM_DK	(VBI3_VIDEOSTD_SET (VBI3_VIDEOSTD_SECAM_D)    | \
					 VBI3_VIDEOSTD_SET (VBI3_VIDEOSTD_SECAM_K)    | \
					 VBI3_VIDEOSTD_SET (VBI3_VIDEOSTD_SECAM_K1))
#define VBI3_VIDEOSTD_SET_SECAM		(VBI3_VIDEOSTD_SET (VBI3_VIDEOSTD_SECAM_B)    | \
					 VBI3_VIDEOSTD_SET (VBI3_VIDEOSTD_SECAM_G)    | \
					 VBI3_VIDEOSTD_SET (VBI3_VIDEOSTD_SECAM_H)    | \
					 VBI3_VIDEOSTD_SET_SECAM_DK		    | \
					 VBI3_VIDEOSTD_SET (VBI3_VIDEOSTD_SECAM_L)    | \
					 VBI3_VIDEOSTD_SET (VBI3_VIDEOSTD_SECAM_LC))
#define VBI3_VIDEOSTD_SET_525_60		(VBI3_VIDEOSTD_SET (VBI3_VIDEOSTD_PAL_M)	    | \
					 VBI3_VIDEOSTD_SET (VBI3_VIDEOSTD_PAL_60)	    | \
					 VBI3_VIDEOSTD_SET_NTSC			    | \
					 VBI3_VIDEOSTD_SET (VBI3_VIDEOSTD_NTSC_443))
#define VBI3_VIDEOSTD_SET_625_50		(VBI3_VIDEOSTD_SET_PAL			    | \
					 VBI3_VIDEOSTD_SET (VBI3_VIDEOSTD_PAL_N)	    | \
					 VBI3_VIDEOSTD_SET (VBI3_VIDEOSTD_PAL_NC)	    | \
					 VBI3_VIDEOSTD_SET_SECAM)
#define VBI3_VIDEOSTD_SET_ALL		(VBI3_VIDEOSTD_SET_525_60		    | \
					 VBI3_VIDEOSTD_SET_625_50)

extern const char *
_vbi3_videostd_name		(vbi3_videostd		videostd)
  __attribute__ ((const));

extern vbi3_videostd_set
vbi3_videostd_set_from_scanning	(int			scanning)
  __attribute__ ((const));

/**
 * @brief Raw VBI sampling parameters.
 */
typedef struct {
	/**
	 * Defines the system all line numbers refer to. Can be more
	 * than one standard if not exactly known, provided there
	 * is no ambiguity.
	 */
	vbi3_videostd_set	videostd_set;
	/** Format of the raw vbi data. */
	vbi3_pixfmt		sampling_format;
	/**
	 * Sampling rate in Hz, the number of samples or pixels
	 * captured per second.
	 */
	unsigned int		sampling_rate;
	/**
	 * Number of samples (pixels) captured per scan line. The
	 * decoder will access only these bytes.
	 */
	unsigned int		samples_per_line;
	/**
	 * Distance of the first bytes of two successive scan lines
	 * in memory, in bytes. Must be greater or equal @a
	 * samples_per_line times bytes per sample.
	 */
	unsigned long		bytes_per_line;
	/**
	 * The distance between 0H (leading edge of horizontal sync pulse,
	 * half amplitude point) and the first sample (pixel) captured,
	 * in samples (pixels).
	 */
	int			offset;
	/**
	 * First scan line to be captured, of the first and second field
	 * respectively, according to the ITU-R line numbering scheme
	 * (see vbi3_sliced). Set to zero if the exact line number isn't
	 * known.
	 */
	unsigned int		start[2];
	/**
	 * Number of scan lines captured, of the first and second
	 * field respectively. This can be zero if only data from one
	 * field is required or available. The sum @a count[0] + @a
	 * count[1] determines the raw vbi image height.
	 */
	unsigned int		count[2];
	/**
	 * In the raw vbi image, normally all lines of the second
	 * field are supposed to follow all lines of the first field. When
	 * this flag is set, the scan lines of first and second field
	 * will be interleaved in memory, starting with the first field.
	 * This implies @a count[0] and @a count[1] are equal.
	 */
	vbi3_bool		interlaced;
	/**
	 * Fields must be stored in temporal order, as the lines have
	 * been captured. It is assumed that the first field is also
	 * stored first in memory, however if the hardware cannot reliable
	 * distinguish between fields this flag shall be cleared, which
	 * disables decoding of data services depending on the field
	 * number.
	 */
	vbi3_bool		synchronous;
} vbi3_sampling_par;

#ifndef ZAPPING8

extern vbi3_service_set
vbi3_sampling_par_from_services	(vbi3_sampling_par *	sp,
				 unsigned int *		max_rate,
				 vbi3_videostd_set	videostd_set,
				 vbi3_service_set	services,
				 vbi3_log_fn *		log_fn,
				 void *			log_user_data);
extern vbi3_service_set
vbi3_sampling_par_check_services	(const vbi3_sampling_par *sp,
				 vbi3_service_set	services,
					 unsigned int		strict,
				 vbi3_log_fn *		log_fn,
				 void *			log_user_data)
  __attribute__ ((_vbi3_pure));
/** @} */

/* Private */

extern vbi3_bool
_vbi3_sampling_par_check_service	(const vbi3_sampling_par *sp,
				 const vbi3_service_par *par,
					 unsigned int		strict,
				 vbi3_log_fn *		log_fn,
				 void *			log_user_data)
  __attribute__ ((_vbi3_pure));
extern vbi3_bool
_vbi3_sampling_par_valid	(const vbi3_sampling_par *sp,
				 vbi3_log_fn *		log_fn,
				 void *			log_user_data)
  __attribute__ ((_vbi3_pure));

#endif /* !ZAPPING8 */

VBI3_END_DECLS

#endif /* __ZVBI3_SAMPLING_PAR_H__ */
