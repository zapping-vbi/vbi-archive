/*
 *  libzvbi - Raw VBI sampling
 *
 *  Copyright (C) 2000-2004 Michael H. Schimek
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

/* $Id: sampling.h,v 1.1.2.5 2004-10-14 07:54:01 mschimek Exp $ */

/* XXX should be SAMPLING_PAR_H */

#ifndef __ZVBI_SAMPLING_H__
#define __ZVBI_SAMPLING_H__

#include <inttypes.h>		/* uint64_t */
#include "macros.h"
#include "graphics.h"		/* vbi_pixfmt */
#include "sliced.h"		/* vbi_service_set */

VBI_BEGIN_DECLS

/**
 * @addtogroup Sampling
 * @{
 */

/** Videostandard identifier. */
typedef enum {
	VBI_VIDEOSTD_NONE = 0,
	VBI_VIDEOSTD_UNKNOWN = VBI_VIDEOSTD_NONE,

     	VBI_VIDEOSTD_PAL_B = 1,
	VBI_VIDEOSTD_PAL_B1,
	VBI_VIDEOSTD_PAL_G,
	VBI_VIDEOSTD_PAL_H,

	VBI_VIDEOSTD_PAL_I,
	VBI_VIDEOSTD_PAL_D,
	VBI_VIDEOSTD_PAL_D1,
	VBI_VIDEOSTD_PAL_K,

	VBI_VIDEOSTD_PAL_M = 9,
	VBI_VIDEOSTD_PAL_N,
	VBI_VIDEOSTD_PAL_NC,

	VBI_VIDEOSTD_NTSC_M = 13,
	VBI_VIDEOSTD_NTSC_M_JP,

	VBI_VIDEOSTD_SECAM_B = 17,
	VBI_VIDEOSTD_SECAM_D,
	VBI_VIDEOSTD_SECAM_G,
	VBI_VIDEOSTD_SECAM_H,

	VBI_VIDEOSTD_SECAM_K,
	VBI_VIDEOSTD_SECAM_K1,
	VBI_VIDEOSTD_SECAM_L,

	/**
	 * Video hardware may support custom videostandards not defined
	 * by libzvbi, for example hybrid standards to play back NTSC video
	 * tapes at 60 Hz on a PAL TV.
	 */
	VBI_VIDEOSTD_CUSTOM_BEGIN = 32,
	VBI_VIDEOSTD_CUSTOM_END = 64
} vbi_videostd;

/**
 * A set of videostandards is used where more than one
 * videostandard may apply. Use VBI_VIDEOSTD_SET macros
 * to build a set.
 */
typedef uint64_t vbi_videostd_set;

#define VBI_VIDEOSTD_SET(videostd) (((vbi_videostd_set) 1) << (videostd))

#define VBI_VIDEOSTD_SET_UNKNOWN 0
#define VBI_VIDEOSTD_SET_EMPTY 0
#define VBI_VIDEOSTD_SET_PAL_BG (VBI_VIDEOSTD_SET (VBI_VIDEOSTD_PAL_B) | \
				 VBI_VIDEOSTD_SET (VBI_VIDEOSTD_PAL_B1)	| \
				 VBI_VIDEOSTD_SET (VBI_VIDEOSTD_PAL_G))
#define VBI_VIDEOSTD_SET_PAL_DK (VBI_VIDEOSTD_SET (VBI_VIDEOSTD_PAL_D) | \
				 VBI_VIDEOSTD_SET (VBI_VIDEOSTD_PAL_D1)	| \
				 VBI_VIDEOSTD_SET (VBI_VIDEOSTD_PAL_K))
#define VBI_VIDEOSTD_SET_PAL    (VBI_VIDEOSTD_SET_PAL_BG |		\
				 VBI_VIDEOSTD_SET_PAL_DK |		\
				 VBI_VIDEOSTD_SET (VBI_VIDEOSTD_PAL_H) | \
				 VBI_VIDEOSTD_SET (VBI_VIDEOSTD_PAL_I))
#define VBI_VIDEOSTD_SET_NTSC   (VBI_VIDEOSTD_SET (VBI_VIDEOSTD_NTSC_M)	| \
				 VBI_VIDEOSTD_SET (VBI_VIDEOSTD_NTSC_M_JP))
#define VBI_VIDEOSTD_SET_SECAM  (VBI_VIDEOSTD_SET (VBI_VIDEOSTD_SECAM_B) | \
				 VBI_VIDEOSTD_SET (VBI_VIDEOSTD_SECAM_D) | \
				 VBI_VIDEOSTD_SET (VBI_VIDEOSTD_SECAM_G) | \
				 VBI_VIDEOSTD_SET (VBI_VIDEOSTD_SECAM_H) | \
				 VBI_VIDEOSTD_SET (VBI_VIDEOSTD_SECAM_K) | \
				 VBI_VIDEOSTD_SET (VBI_VIDEOSTD_SECAM_K1) | \
				 VBI_VIDEOSTD_SET (VBI_VIDEOSTD_SECAM_L))
#define VBI_VIDEOSTD_SET_525_60 (VBI_VIDEOSTD_SET (VBI_VIDEOSTD_PAL_M) | \
				 VBI_VIDEOSTD_SET_NTSC)
#define VBI_VIDEOSTD_SET_625_50 (VBI_VIDEOSTD_SET_PAL |			\
				 VBI_VIDEOSTD_SET (VBI_VIDEOSTD_PAL_N) | \
				 VBI_VIDEOSTD_SET (VBI_VIDEOSTD_PAL_NC)	| \
				 VBI_VIDEOSTD_SET_SECAM)
/** All defined videostandards without custom standards. */
#define VBI_VIDEOSTD_SET_ALL    (VBI_VIDEOSTD_SET_525_60 |		\
				 VBI_VIDEOSTD_SET_625_50)
/** All custrom videostandards. */
#define VBI_VIDEOSTD_SET_CUSTOM						\
	((~VBI_VIDEOSTD_SET_EMPTY) << VBI_VIDEOSTD_CUSTOM_BEGIN)

extern const char *
_vbi_videostd_name		(vbi_videostd		videostd) vbi_const;

/**
 * @brief Raw VBI sampling parameters.
 */
typedef struct {
	/**
	 * Defines the system all line numbers refer to. Can be more
	 * than one standard if not exactly known, provided there
	 * is no ambiguity.
	 */
	vbi_videostd_set	videostd_set;
	/** Format of the raw vbi data. */
	vbi_pixfmt		sampling_format;
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
	unsigned int		bytes_per_line;
	/**
	 * The distance between 0H (leading edge of horizontal sync pulse,
	 * half amplitude point) and the first sample (pixel) captured,
	 * in samples (pixels).
	 */
	int			offset;
	/**
	 * First scan line to be captured, of the first and second field
	 * respectively, according to the ITU-R line numbering scheme
	 * (see vbi_sliced). Set to zero if the exact line number isn't
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
	vbi_bool		interlaced;
	/**
	 * Fields must be stored in temporal order, as the lines have
	 * been captured. It is assumed that the first field is also
	 * stored first in memory, however if the hardware cannot reliable
	 * distinguish between fields this flag shall be cleared, which
	 * disables decoding of data services depending on the field
	 * number.
	 */
	vbi_bool		synchronous;
} vbi_sampling_par;

extern vbi_service_set
vbi_sampling_par_from_services	(vbi_sampling_par *	sp,
				 unsigned int *		max_rate,
				 vbi_videostd_set	videostd_set,
				 vbi_service_set	services);
extern vbi_service_set
vbi_sampling_par_check_services	(const vbi_sampling_par *sp,
				 vbi_service_set	services,
				 unsigned int		strict) vbi_pure;
/** @} */

/* Private */

extern vbi_bool
_vbi_sampling_par_check_service	(const vbi_sampling_par *sp,
				 const vbi_service_par *par,
				 unsigned int		strict)	vbi_pure;
extern vbi_bool
_vbi_sampling_par_verify	(const vbi_sampling_par *sp) vbi_pure;

VBI_END_DECLS

#endif /* __ZVBI_SAMPLING_H__ */
