/*
 *  libzvbi - Raw VBI decoder
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

/* $Id: raw_decoder.h,v 1.1.2.1 2004-01-30 00:39:52 mschimek Exp $ */

#ifndef RAW_DECODER_H
#define RAW_DECODER_H

#include "bit_slicer.h"

/* Public */

/**
 * @ingroup RawDecoder
 * @brief Raw VBI decoder context.
 *
 * The contents of this structure are private.
 */
typedef struct _vbi_raw_decoder vbi_raw_decoder;

/* Private */

#define MAX_JOBS 8
#define MAX_WAYS 8

typedef struct {
	vbi_service_set		id;
	vbi_bit_slicer		slicer;
} vbi_raw_decoder_job;

struct _vbi_raw_decoder {
	vbi_sampling_par	sampling;

	vbi_service_set		services;

	unsigned int		n_jobs;

	int			readjust;

	int8_t *		pattern;	/* n scan lines * MAX_WAYS */

	vbi_raw_decoder_job	jobs[MAX_JOBS];
};

/* typedef vbi_service_par in sliced.h */

struct _vbi_service_par {
	vbi_service_set		id;
	const char *		label;

	/* Video standard: 525 (FV = 59.94 Hz, FH = 15734 Hz),
			   625 (FV = 50 Hz, FH = 15625 Hz). */
	vbi_videostd_set	videostd_set;

	/* Most scan lines used by the data service, first and last
	   line of first and second field. ITU-R numbering scheme.
	   Zero if no data from this field, requires field sync. */
	unsigned int		first[2];
        unsigned int		last[2];

	/* Leading edge hsync to leading edge first CRI one bit,
	   half amplitude points, in nanoseconds. */
	unsigned int		offset;

	unsigned int		cri_rate;	/* Hz */
	unsigned int		bit_rate;	/* Hz */

	/* Clock Run In and FRaming Code, LSB last txed bit of FRC. */
	unsigned int		cri_frc;

	/* CRI and FRC bits significant for identification. */
	unsigned int		cri_frc_mask;

	/* Number of significat cri_bits (at cri_rate),
	   frc_bits (at bit_rate). */
	unsigned int		cri_bits;
	unsigned int		frc_bits;

	unsigned int		payload;	/* bits */
	vbi_modulation		modulation;
};

extern const vbi_service_par vbi_service_table [];

extern vbi_bool
vbi_raw_decoder_init		(vbi_raw_decoder *	rd,
				 const vbi_sampling_par *sp);
extern void
vbi_raw_decoder_destroy		(vbi_raw_decoder *	rd);
extern void
vbi_raw_decoder_dump		(const vbi_raw_decoder *rd,
				 FILE *			fp);

/* Public */

/**
 * @addtogroup RawDecoder
 * @{
 */
extern vbi_raw_decoder *
vbi_raw_decoder_new		(const vbi_sampling_par *sp);
extern void
vbi_raw_decoder_delete		(vbi_raw_decoder *	rd);
extern void
vbi_raw_decoder_get_sampling_par
				(const vbi_raw_decoder *rd,
				 vbi_sampling_par *	sp);
extern vbi_service_set
vbi_raw_decoder_set_sampling_par
				(vbi_raw_decoder *	rd,
				 const vbi_sampling_par *sp,
				 unsigned int		strict);
extern vbi_service_set
vbi_raw_decoder_add_services	(vbi_raw_decoder *	rd,
				 vbi_service_set	services,
				 unsigned int		strict);
extern vbi_service_set
vbi_raw_decoder_remove_services	(vbi_raw_decoder *	rd,
				 vbi_service_set	services);
extern void
vbi_raw_decoder_reset		(vbi_raw_decoder *	rd);
extern unsigned int
vbi_raw_decoder_decode		(vbi_raw_decoder *	rd,
				 vbi_sliced *		sliced,
				 unsigned int		sliced_lines,
				 const uint8_t *	raw);
/** @} */

/* Private */

#endif /* RAW_DECODER_H */
