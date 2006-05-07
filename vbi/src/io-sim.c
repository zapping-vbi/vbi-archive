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

/* $Id: io-sim.c,v 1.1.2.9 2006-05-07 20:51:36 mschimek Exp $ */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>		/* sin() */
#include <errno.h>
#include <ctype.h>		/* isspace() */
#include <limits.h>		/* INT_MAX */

#include "misc.h"
#include "sliced.h"
#include "sampling_par.h"
#include "vps.h"
#include "wss.h"

#include "io-sim.h"

#define PI 3.1415926535897932384626433832795029

#define PULSE(zero_level)						\
do {									\
	if (0 == seq) {							\
		raw[i] = zero_level;					\
	} else if (3 == seq) {						\
		raw[i] = zero_level + (int) signal_amp;			\
	} else if ((seq ^ bit) & 1) { /* down */			\
		double r = sin (q * tr - (PI / 2.0));			\
		raw[i] = zero_level + (int)(r * r * signal_amp);	\
	} else { /* up */						\
		double r = sin (q * tr);				\
		raw[i] = zero_level + (int)(r * r * signal_amp);	\
	}								\
} while (0)

#define PULSE_SEQ(zero_level)						\
do {									\
	double tr;							\
	unsigned int bit;						\
	unsigned int byte;						\
	unsigned int seq;						\
									\
	tr = t - t1;							\
	bit = tr * bit_rate;						\
	byte = bit >> 3;						\
	bit &= 7;							\
	seq = (buf[byte] >> 7) + buf[byte + 1] * 2;			\
	seq = (seq >> bit) & 3;						\
	PULSE (zero_level);						\
} while (0)

static void
signal_teletext			(const vbi3_sampling_par *sp,
				 unsigned int		black_level,
				 double			signal_amp,
				 double			bit_rate,
				 unsigned int		frc,
				 unsigned int		payload,
				 uint8_t *		raw,
				 const vbi3_sliced *	sliced)
{
	double bit_period = 1.0 / bit_rate;
	double t1 = 10.3e-6 - .5 * bit_period;
	double t2 = t1 + (payload * 8 + 24 + 1) * bit_period;
	double q = (PI / 2.0) * bit_rate;
	double sample_period = 1.0 / sp->sampling_rate;
	uint8_t buf[64];
	unsigned int i;
	double t;

	buf[0] = 0x00;
	buf[1] = 0x55; /* clock run-in */
	buf[2] = 0x55;
	buf[3] = frc;

	memcpy (buf + 4, sliced->data, payload);

	buf[payload + 4] = 0x00;

	t = sp->offset / (double) sp->sampling_rate;

	for (i = 0; i < sp->samples_per_line; ++i) {
		if (t >= t1 && t < t2)
			PULSE_SEQ (black_level);

		t += sample_period;
	}
}

static void
signal_vps			(const vbi3_sampling_par *sp,
				 unsigned int		black_level,
				 unsigned int		white_level,
				 uint8_t *		raw,
				 const vbi3_sliced *	sliced)
{
	static const uint8_t biphase [] = {
		0x55, 0x56, 0x59, 0x5A,
		0x65, 0x66, 0x69, 0x6A,
		0x95, 0x96, 0x99, 0x9A,
		0xA5, 0xA6, 0xA9, 0xAA
	};
	double bit_rate = 15625 * 160;
	double t1 = 12.5e-6 - .5 / bit_rate;
	double t4 = t1 + (13 * 8) / bit_rate;
	double q = (PI / 2.0) * bit_rate;
	double sample_period = 1.0 / sp->sampling_rate;
	double signal_amp = white_level - black_level;
	uint8_t buf[32];
	unsigned int i;
	double t;

	CLEAR (buf);

	buf[1] = 0x55; /* 0101 0101 */
	buf[2] = 0x55; /* 0101 0101 */
	buf[3] = 0x51; /* 0101 0001 */
	buf[4] = 0x99; /* 1001 1001 */

	for (i = 0; i < 13; ++i) {
		buf[5 + i * 2] = biphase[sliced->data[i] >> 4];
		buf[6 + i * 2] = biphase[sliced->data[i] & 15];
	}

	t = sp->offset / (double) sp->sampling_rate;

	for (i = 0; i < sp->samples_per_line; ++i) {
		if (t >= t1 && t < t4)
			PULSE_SEQ (black_level);

		t += sample_period;
	}
}

static void
wss_biphase			(uint8_t		buf[32],
				 const vbi3_sliced *	sliced)
{
	unsigned int bit;
	unsigned int data;
	unsigned int i;

	/* 29 bit run-in and 24 bit start code, lsb first. */

	buf[0] = 0x00;
	buf[1] = 0x1F; /* 0001 1111 */
	buf[2] = 0xC7; /* 1100 0111 */
	buf[3] = 0x71; /* 0111 0001 */
	buf[4] = 0x1C; /* 000 | 1 1100 */
	buf[5] = 0x8F; /* 1000 1111 */
	buf[6] = 0x07; /* 0000 0111 */
	buf[7] = 0x1F; /*    1 1111 */

	bit = 8 + 29 + 24;
	data = sliced->data[0] + sliced->data[1] * 256;

	for (i = 0; i < 14; ++i) {
		static const unsigned int biphase [] = { 0x38, 0x07 };
		unsigned int byte;
		unsigned int shift;
		unsigned int seq;

		byte = bit >> 3;
		shift = bit & 7;
		bit += 6;

		seq = biphase[data & 1] << shift;
		data >>= 1;

		assert (byte < 31);

		buf[byte] |= seq;
		buf[byte + 1] = seq >> 8;
	}
}

static void
signal_wss_625			(const vbi3_sampling_par *sp,
				 unsigned int		black_level,
				 unsigned int		white_level,
				 uint8_t *		raw,
				 const vbi3_sliced *	sliced)
{
	double bit_rate = 15625 * 320;
	double t1 = 11.0e-6 - .5 / bit_rate;
	double t4 = t1 + (29 + 24 + 14 * 6) / bit_rate;
	double q = (PI / 2.0) * bit_rate;
	double sample_period = 1.0 / sp->sampling_rate;
	double signal_amp = white_level - black_level;
	uint8_t buf[32];
	unsigned int i;
	double t;

	wss_biphase (buf, sliced);

	t = sp->offset / (double) sp->sampling_rate;

	for (i = 0; i < sp->samples_per_line; ++i) {
		if (t >= t1 && t < t4)
			PULSE_SEQ (black_level);

		t += sample_period;
	}
}

static void
signal_closed_caption		(const vbi3_sampling_par *sp,
				 unsigned int		blank_level,
				 unsigned int		white_level,
				 double			bit_rate,
				 uint8_t *		raw,
				 const vbi3_sliced *	sliced)
{
	double bit_period = 1.0 / bit_rate;
	double t1 = 10.5e-6 - .25 * bit_period;
	double t2 = t1 + 7 * bit_period; /* CRI 7 cycles */
	double t3 = t2 + 1.25 * bit_period;
	double t4 = t3 + 18 * bit_period; /* 17 bits + raise and fall time */
	double q1 = PI * bit_rate;
	double q = q1 * .5;
	double sample_period = 1.0 / sp->sampling_rate;
	double signal_amp = (white_level - blank_level) * .5;
	unsigned int data;
	unsigned int i;
	double t;

	/* Twice 7 data + odd parity, start bit 0 -> 1 */

	data = (sliced->data[1] << 10) + (sliced->data[0] << 2) + 2;

	t = sp->offset / (double) sp->sampling_rate;

	for (i = 0; i < sp->samples_per_line; ++i) {
		if (t >= t1 && t < t2) {
			double r;

			r = sin (q1 * (t - t2));
			raw[i] = blank_level + (int)(r * r * signal_amp);
		} else if (t >= t3 && t < t4) {
			double tr;
			unsigned int bit;
			unsigned int seq;

			tr = t - t3;
			bit = tr * bit_rate;
			seq = (data >> bit) & 3;

			PULSE (blank_level);
		}

		t += sample_period;
	}
}

static void
clear_image			(uint8_t *		p,
				 unsigned int		value,
				 unsigned int		width,
				 unsigned int		height,
				 unsigned int		bytes_per_line)
{
	if (width == bytes_per_line) {
		memset (p, value, height * bytes_per_line);
	} else {
		while (height-- > 0) {
			memset (p, value, width);
			p += bytes_per_line;
		}
	}
}

static vbi3_bool
signal_u8			(const vbi3_sampling_par *sp,
				 unsigned int		blank_level,
				 unsigned int		black_level,
				 unsigned int		white_level,
				 uint8_t *		raw,
				 const vbi3_sliced *	sliced,
				 unsigned int		sliced_lines,
				 const char *		caller)
{
	unsigned int scan_lines;

	caller = caller;

	scan_lines = sp->count[0] + sp->count[1];

	clear_image (raw, blank_level,
		     sp->samples_per_line, scan_lines,
		     sp->bytes_per_line);

	for (; sliced_lines-- > 0; ++sliced) {
		unsigned int row;
		uint8_t *raw1;

		if (0 == sliced->line) {
			goto bounds;
		} else if (sliced->line >= sp->start[1]) {
			row = sliced->line - sp->start[1];

			if (row >= sp->count[1])
				goto bounds;

			if (sp->interlaced)
				row = row * 2 + 1;
			else
				row += sp->count[0];
		} else if (sliced->line >= sp->start[0]) {
			row = sliced->line - sp->start[0];

			if (row >= sp->count[0])
				goto bounds;

			if (sp->interlaced)
				row *= 2;
		} else {
		bounds:
//			debug ("Sliced line %u out of bounds\n",
//			       sliced->line);
			return FALSE;
		}

		raw1 = raw + row * sp->bytes_per_line;

		switch (sliced->id) {
		case VBI3_SLICED_TELETEXT_A:
			signal_teletext (sp, black_level,
					 .7 * (white_level
					       - black_level), /* ? */
					 15625 * 397, 0xE7, 37,
					 raw1, sliced);
			break;

		case VBI3_SLICED_TELETEXT_B_L10_625:
		case VBI3_SLICED_TELETEXT_B_L25_625:
		case VBI3_SLICED_TELETEXT_B:
			signal_teletext (sp, black_level,
					 .66 * (white_level - black_level),
					 15625 * 444, 0x27, 42,
					 raw1, sliced);
			break;

		case VBI3_SLICED_TELETEXT_C_625:
			signal_teletext (sp, black_level,
					 .7 * (white_level - black_level),
					 15625 * 367, 0xE7, 33,
					 raw1, sliced);
			break;

		case VBI3_SLICED_TELETEXT_D_625:
			signal_teletext (sp, black_level,
					 .7 * (white_level - black_level),
					 5642787, 0xA7, 34,
					 raw1, sliced);
			break;

		case VBI3_SLICED_CAPTION_625_F1:
		case VBI3_SLICED_CAPTION_625_F2:
		case VBI3_SLICED_CAPTION_625:
			signal_closed_caption (sp, blank_level, white_level,
					       15625 * 32, raw1, sliced);
			break;

		case VBI3_SLICED_VPS:
		case VBI3_SLICED_VPS_F2:
			signal_vps (sp, black_level, white_level,
				    raw1, sliced);
			break;

		case VBI3_SLICED_WSS_625:
			signal_wss_625 (sp, black_level, white_level,
					raw1, sliced);
			break;

		case VBI3_SLICED_TELETEXT_B_525:
			signal_teletext (sp, black_level,
					 .7 * (white_level - black_level),
					 5727272, 0x27, 34,
					 raw1, sliced);
			break;

		case VBI3_SLICED_TELETEXT_C_525:
			signal_teletext (sp, black_level,
					 .7 * (white_level - black_level),
					 5727272, 0xE7, 33,
					 raw1, sliced);
			break;

		case VBI3_SLICED_TELETEXT_D_525:
			signal_teletext (sp, black_level,
					 .7 * (white_level - black_level),
					 5727272, 0xA7, 34,
					 raw1, sliced);
			break;

		case VBI3_SLICED_CAPTION_525_F1:
		case VBI3_SLICED_CAPTION_525_F2:
		case VBI3_SLICED_CAPTION_525:
			signal_closed_caption (sp, blank_level, white_level,
					       15734 * 32, raw1, sliced);
			break;

		default:
//			debug ("Service 0x%08x (%s) not supported\n",
//			       sliced->id, vbi3_sliced_name (sliced->id));
			return FALSE;
		}
	}

	return TRUE;
}

/**
 * @internal
 * @param raw A raw VBI image will be stored here. The buffer
 *   must be large enough for @a sp->count[0] + count[1] lines
 *   of bytes_per_line each, with samples_per_line bytes
 *   actually written.
 * @param sp Describes the raw VBI data generated. sampling_format
 *   must be VBI3_PIXFMT_Y8.
 * @param sliced Pointer to an array of vbi3_sliced containing the
 *   VBI data to be encoded.
 * @param sliced_lines Number of elements in the vbi3_sliced array.
 *
 * Generates a raw VBI image similar to those you get from VBI
 * sampling hardware. The following data services are
 * currently supported: All Teletext services, WSS 625, all Closed
 * Caption services except 2xCC. Sliced data is encoded as is,
 * without verification, except for buffer overflow checks.
 *
 * @return
 * @c FALSE on error.
 */
vbi3_bool
_vbi3_test_image_vbi		(uint8_t *		raw,
				 unsigned int		raw_size,
				 const vbi3_sampling_par *sp,
				 const vbi3_sliced *	sliced,
				 unsigned int		sliced_lines)
{
	unsigned int scan_lines;
	unsigned int blank_level;
	unsigned int black_level;
	unsigned int white_level;

	if (!_vbi3_sampling_par_verify (sp))
		return FALSE;

	scan_lines = sp->count[0] + sp->count[1];
	if (scan_lines * sp->bytes_per_line > raw_size)
		return FALSE;

	if (VBI3_VIDEOSTD_SET_525_60 & sp->videostd_set) {
		const double IRE = 200 / 140.0;

		blank_level = (int)(40   * IRE);
		black_level = (int)(47.5 * IRE);
		white_level = (int)(140  * IRE);
	} else {
		const double IRE = 200 / 143.0;

		blank_level = (int)(43   * IRE);
		black_level = blank_level;
		white_level = (int)(143  * IRE);
	}

	return signal_u8 (sp, blank_level, black_level, white_level,
			  raw, sliced, sliced_lines, __FUNCTION__);
}

#define RGBA_TO_RGB16(value)						\
	(+(((value) & 0xF8) >> (3 - 0))					\
	 +(((value) & 0xFC00) >> (10 - 5))				\
	 +(((value) & 0xF80000) >> (19 - 11)))

#define RGBA_TO_RGBA15(value)						\
	(+(((value) & 0xF8) >> (3 - 0))					\
	 +(((value) & 0xF800) >> (11 - 5))				\
	 +(((value) & 0xF80000) >> (19 - 10))				\
	 +(((value) & 0x80000000) >> (31 - 15)))

#define RGBA_TO_ARGB15(value)						\
	(+(((value) & 0xF8) >> (3 - 1))					\
	 +(((value) & 0xF800) >> (11 - 6))				\
	 +(((value) & 0xF80000) >> (19 - 11))				\
	 +(((value) & 0x80000000) >> (31 - 0)))

#define RGBA_TO_RGBA12(value)						\
	(+(((value) & 0xF0) >> (4 - 0))					\
	 +(((value) & 0xF000) >> (12 - 4))				\
	 +(((value) & 0xF00000) >> (20 - 8))				\
	 +(((value) & 0xF0000000) >> (28 - 12)))

#define RGBA_TO_ARGB12(value)						\
	(+(((value) & 0xF0) << -(4 - 12))				\
	 +(((value) & 0xF000) >> (12 - 8))				\
	 +(((value) & 0xF00000) >> (20 - 4))				\
	 +(((value) & 0xF0000000) >> (28 - 0)))

#define RGBA_TO_RGB8(value)						\
	(+(((value) & 0xE0) >> (5 - 0))					\
	 +(((value) & 0xE000) >> (13 - 3))				\
	 +(((value) & 0xC00000) >> (22 - 6)))

#define RGBA_TO_BGR8(value)						\
	(+(((value) & 0xE0) >> (5 - 5))					\
	 +(((value) & 0xE000) >> (13 - 2))				\
	 +(((value) & 0xC00000) >> (22 - 0)))

#define RGBA_TO_RGBA7(value)						\
	(+(((value) & 0xC0) >> (6 - 0))					\
	 +(((value) & 0xE000) >> (13 - 2))				\
	 +(((value) & 0xC00000) >> (22 - 5))				\
	 +(((value) & 0x80000000) >> (31 - 7)))

#define RGBA_TO_ARGB7(value)						\
	(+(((value) & 0xC0) >> (6 - 6))					\
	 +(((value) & 0xE000) >> (13 - 3))				\
	 +(((value) & 0xC00000) >> (22 - 1))				\
	 +(((value) & 0x80000000) >> (31 - 0)))

#define MST1(d, val, mask) (d) = ((d) & ~(mask)) | ((val) & (mask))
#define MST2(d, val, mask) (d) = ((d) & (mask)) | (val)

#define SCAN_LINE_TO_N(conv, n)						\
do {									\
	for (i = 0; i < sp->samples_per_line; ++i) {			\
		uint8_t *dd = d + i * (n);				\
		unsigned int value = s[i] * 0x01010101;			\
		unsigned int mask = ~pixel_mask;			\
									\
		value = conv (value) & pixel_mask;			\
		MST2 (dd[0], value >> 0, mask >> 0);			\
		if (n >= 2)						\
			MST2 (dd[1], value >> 8, mask >> 8);		\
		if (n >= 3)						\
			MST2 (dd[2], value >> 16, mask >> 16);		\
		if (n >= 4)						\
			MST2 (dd[3], value >> 24, mask >> 24);		\
	}								\
} while (0)

#define SCAN_LINE_TO_RGB2(conv, endian)					\
do {									\
	for (i = 0; i < sp->samples_per_line; ++i) {			\
		uint8_t *dd = d + i * 2;				\
		unsigned int value = s[i] * 0x01010101;			\
		unsigned int mask;					\
									\
		value = conv (value) & pixel_mask;			\
		mask = ~pixel_mask;		       			\
		MST2 (dd[0 + endian], value >> 0, mask >> 0);		\
		MST2 (dd[1 - endian], value >> 8, mask >> 8);		\
	}								\
} while (0)

/**
 * @internal
 * @param raw A raw VBI image will be stored here. The buffer
 *   must be large enough for @a sp->count[0] + count[1] lines
 *   of bytes_per_line each, with samples_per_line actually written.
 * @param sp Describes the raw VBI data to generate. When the
 *   sampling_format is a planar YUV format the function writes
 *   the Y plane only.
 * @param pixel_mask This mask selects which color or alpha channel
 *   shall contain VBI data. Depending on @a sp->sampling_format it is
 *   interpreted as 0xAABBGGRR or 0xAAVVUUYY. A value of 0x000000FF
 *   for example writes data in "red bits", not changing other
 *   bits in the @a raw buffer.
 * @param sliced Pointer to an array of vbi3_sliced containing the
 *   VBI data to be encoded.
 * @param sliced_lines Number of elements in the vbi3_sliced array.
 *
 * Generates a raw VBI image similar to those you get from video
 * capture hardware. Otherwise identical to vbi3_test_image_vbi().
 *
 * @return
 * TRUE on success.
 */
/* brightness, contrast parameter? */
vbi3_bool
_vbi3_test_image_video		(uint8_t *		raw,
				 unsigned long		raw_size,
				 const vbi3_sampling_par *sp,
				 unsigned int		pixel_mask,
				 const vbi3_sliced *	sliced,
				 unsigned int		sliced_lines)
{
	unsigned int blank_level;
	unsigned int black_level;
	unsigned int white_level;
	vbi3_sampling_par sp8;
	unsigned int scan_lines;
	unsigned int size;
	uint8_t *buf;
	uint8_t *s;
	uint8_t *d;

	if (!_vbi3_sampling_par_verify (sp))
		return FALSE;

	scan_lines = sp->count[0] + sp->count[1];

	if (scan_lines * sp->bytes_per_line > raw_size) {
/*
		debug ("scan_lines %u * bytes_per_line %lu > raw_size %lu\n",
		       scan_lines, sp->bytes_per_line, raw_size);
*/
		return FALSE;
	}

	switch (sp->sampling_format) {
	case VBI3_PIXFMT_YVUA24_LE:	/* 0xAAUUVVYY */
	case VBI3_PIXFMT_YVU24_LE:	/* 0x00UUVVYY */
	case VBI3_PIXFMT_YVYU:
	case VBI3_PIXFMT_VYUY:		/* 0xAAUUVVYY */
		pixel_mask = (+ ((pixel_mask & 0xFF00) << 8)
			      + ((pixel_mask & 0xFF0000) >> 8)
			      + ((pixel_mask & 0xFF0000FF)));
		break;

	case VBI3_PIXFMT_YUVA24_BE:	/* 0xYYUUVVAA */
	case VBI3_PIXFMT_RGBA24_BE:	/* 0xRRGGBBAA */
		pixel_mask = (+ ((pixel_mask & 0xFF) << 24)
			      + ((pixel_mask & 0xFF00) << 8)
			      + ((pixel_mask & 0xFF0000) >> 8)
			      + ((pixel_mask & 0xFF000000) >> 24));
		break;

	case VBI3_PIXFMT_YVUA24_BE:	/* 0xYYVVUUAA */
		pixel_mask = (+ ((pixel_mask & 0xFF) << 24)
			      + ((pixel_mask & 0xFFFF00))
			      + ((pixel_mask & 0xFF000000) >> 24));
		break;

	case VBI3_PIXFMT_YUV24_BE:	/* 0xAAYYUUVV */
	case VBI3_PIXFMT_ARGB24_BE:	/* 0xAARRGGBB */
	case VBI3_PIXFMT_RGB24_BE:
	case VBI3_PIXFMT_BGRA15_LE:
	case VBI3_PIXFMT_BGRA15_BE:
	case VBI3_PIXFMT_ABGR15_LE:
	case VBI3_PIXFMT_ABGR15_BE:
	case VBI3_PIXFMT_BGRA12_LE:
	case VBI3_PIXFMT_BGRA12_BE:
	case VBI3_PIXFMT_ABGR12_LE:
	case VBI3_PIXFMT_ABGR12_BE:
	case VBI3_PIXFMT_BGRA7:
	case VBI3_PIXFMT_ABGR7:
		pixel_mask = (+ ((pixel_mask & 0xFF) << 16)
			      + ((pixel_mask & 0xFF0000) >> 16)
			      + ((pixel_mask & 0xFF00FF00)));
		break;

	case VBI3_PIXFMT_YVU24_BE:	/* 0x00YYVVUU */
		pixel_mask = (+ ((pixel_mask & 0xFF) << 16)
			      + ((pixel_mask & 0xFFFF00) >> 8));
		break;


	case VBI3_PIXFMT_BGRA24_BE:	/* 0xBBGGRRAA */
		pixel_mask = (+ ((pixel_mask & 0xFFFFFF) << 8)
			      + ((pixel_mask & 0xFF000000) >> 24));
		break;

	default:
		break;
	}

	switch (sp->sampling_format) {
	case VBI3_PIXFMT_RGB16_LE:
	case VBI3_PIXFMT_RGB16_BE:
	case VBI3_PIXFMT_BGR16_LE:
	case VBI3_PIXFMT_BGR16_BE:
		pixel_mask = RGBA_TO_RGB16 (pixel_mask);
		break;

	case VBI3_PIXFMT_RGBA15_LE:
	case VBI3_PIXFMT_RGBA15_BE:
	case VBI3_PIXFMT_BGRA15_LE:
	case VBI3_PIXFMT_BGRA15_BE:
		pixel_mask = RGBA_TO_RGBA15 (pixel_mask);
		break;

	case VBI3_PIXFMT_ARGB15_LE:
	case VBI3_PIXFMT_ARGB15_BE:
	case VBI3_PIXFMT_ABGR15_LE:
	case VBI3_PIXFMT_ABGR15_BE:
		pixel_mask = RGBA_TO_ARGB15 (pixel_mask);
		break;

	case VBI3_PIXFMT_RGBA12_LE:
	case VBI3_PIXFMT_RGBA12_BE:
	case VBI3_PIXFMT_BGRA12_LE:
	case VBI3_PIXFMT_BGRA12_BE:
		pixel_mask = RGBA_TO_RGBA12 (pixel_mask);
		break;

	case VBI3_PIXFMT_ARGB12_LE:
	case VBI3_PIXFMT_ARGB12_BE:
	case VBI3_PIXFMT_ABGR12_LE:
	case VBI3_PIXFMT_ABGR12_BE:
		pixel_mask = RGBA_TO_ARGB12 (pixel_mask);
		break;

	case VBI3_PIXFMT_RGB8:
		pixel_mask = RGBA_TO_RGB8 (pixel_mask);
		break;

	case VBI3_PIXFMT_BGR8:
		pixel_mask = RGBA_TO_BGR8 (pixel_mask);
		break;

	case VBI3_PIXFMT_RGBA7:
	case VBI3_PIXFMT_BGRA7:
		pixel_mask = RGBA_TO_RGBA7 (pixel_mask);
		break;

	case VBI3_PIXFMT_ARGB7:
	case VBI3_PIXFMT_ABGR7:
		pixel_mask = RGBA_TO_ARGB7 (pixel_mask);
		break;

	default:
		break;
	}

	if (0 == pixel_mask) {
		/* Done! :-) */
		return TRUE;
	}

	/* ITU-R Rec BT.601 sampling assumed. */

	if (VBI3_VIDEOSTD_SET_525_60 & sp->videostd_set) {
		blank_level = MAX (0, 16 - 40 * 220 / 100);
		black_level = 16;
		white_level = 16 + 219;
	} else {
		blank_level = MAX (0, 16 - 43 * 220 / 100);
		black_level = 16;
		white_level = 16 + 219;
	}

	sp8 = *sp;

	sp8.sampling_format = VBI3_PIXFMT_Y8;
	sp8.bytes_per_line = sp->samples_per_line;

	size = scan_lines * sp->samples_per_line;
	if (!(buf = vbi3_malloc (size))) {
//		error ("Out of memory (%u bytes)\n", size);
		errno = ENOMEM;
		return FALSE;
	}

	if (!signal_u8 (&sp8, blank_level, black_level, white_level,
			buf, sliced, sliced_lines, __FUNCTION__)) {
		vbi3_free (buf);
		return FALSE;
	}

	s = buf;
	d = raw;

	while (scan_lines-- > 0) {
		unsigned int i;

		switch (sp->sampling_format) {
		case VBI3_PIXFMT_NONE:
		case VBI3_PIXFMT_RESERVED0:
		case VBI3_PIXFMT_RESERVED1:
		case VBI3_PIXFMT_RESERVED2:
		case VBI3_PIXFMT_RESERVED3:
			break;

		case VBI3_PIXFMT_YUV444:
		case VBI3_PIXFMT_YVU444:
		case VBI3_PIXFMT_YUV422:
		case VBI3_PIXFMT_YVU422:
		case VBI3_PIXFMT_YUV411:
		case VBI3_PIXFMT_YVU411:
		case VBI3_PIXFMT_YUV420:
		case VBI3_PIXFMT_YVU420:
		case VBI3_PIXFMT_YUV410:
		case VBI3_PIXFMT_YVU410:
		case VBI3_PIXFMT_Y8:
			for (i = 0; i < sp->samples_per_line; ++i)
				MST1 (d[i], s[i], pixel_mask);
			break;

		case VBI3_PIXFMT_YUVA24_LE:
		case VBI3_PIXFMT_YVUA24_LE:
		case VBI3_PIXFMT_YUVA24_BE:
		case VBI3_PIXFMT_YVUA24_BE:
		case VBI3_PIXFMT_RGBA24_LE:
		case VBI3_PIXFMT_RGBA24_BE:
		case VBI3_PIXFMT_BGRA24_LE:
		case VBI3_PIXFMT_BGRA24_BE:
			SCAN_LINE_TO_N (+, 4);
			break;

		case VBI3_PIXFMT_YUV24_LE:
		case VBI3_PIXFMT_YUV24_BE:
		case VBI3_PIXFMT_YVU24_LE:
		case VBI3_PIXFMT_YVU24_BE:
		case VBI3_PIXFMT_RGB24_LE:
		case VBI3_PIXFMT_RGB24_BE:
			SCAN_LINE_TO_N (+, 3);
			break;

		case VBI3_PIXFMT_YUYV:
		case VBI3_PIXFMT_YVYU:
			for (i = 0; i < sp->samples_per_line; i += 2) {
				uint8_t *dd = d + i * 2;
				unsigned int uv = (s[i] + s[i + 1] + 1) >> 1;

				MST1 (dd[0], s[i], pixel_mask);
				MST1 (dd[1], uv, pixel_mask >> 8);
				MST1 (dd[2], s[i + 1], pixel_mask);
				MST1 (dd[3], uv, pixel_mask >> 16);
			}
			break;

		case VBI3_PIXFMT_UYVY:
		case VBI3_PIXFMT_VYUY:
			for (i = 0; i < sp->samples_per_line; i += 2) {
				uint8_t *dd = d + i * 2;
				unsigned int uv = (s[i] + s[i + 1] + 1) >> 1;

				MST1 (dd[0], uv, pixel_mask >> 8);
				MST1 (dd[1], s[i], pixel_mask);
				MST1 (dd[2], uv, pixel_mask >> 16);
				MST1 (dd[3], s[i + 1], pixel_mask);
			}
			break;

		case VBI3_PIXFMT_RGB16_LE:
		case VBI3_PIXFMT_BGR16_LE:
			SCAN_LINE_TO_RGB2 (RGBA_TO_RGB16, 0);
			break;

		case VBI3_PIXFMT_RGB16_BE:
		case VBI3_PIXFMT_BGR16_BE:
			SCAN_LINE_TO_RGB2 (RGBA_TO_RGB16, 1);
			break;

		case VBI3_PIXFMT_RGBA15_LE:
		case VBI3_PIXFMT_BGRA15_LE:
			SCAN_LINE_TO_RGB2 (RGBA_TO_RGBA15, 0);
			break;

		case VBI3_PIXFMT_RGBA15_BE:
		case VBI3_PIXFMT_BGRA15_BE:
			SCAN_LINE_TO_RGB2 (RGBA_TO_RGBA15, 1);
			break;

		case VBI3_PIXFMT_ARGB15_LE:
		case VBI3_PIXFMT_ABGR15_LE:
			SCAN_LINE_TO_RGB2 (RGBA_TO_ARGB15, 0);
			break;

		case VBI3_PIXFMT_ARGB15_BE:
		case VBI3_PIXFMT_ABGR15_BE:
			SCAN_LINE_TO_RGB2 (RGBA_TO_ARGB15, 1);
			break;

		case VBI3_PIXFMT_RGBA12_LE:
		case VBI3_PIXFMT_BGRA12_LE:
			SCAN_LINE_TO_RGB2 (RGBA_TO_RGBA12, 0);
			break;

		case VBI3_PIXFMT_RGBA12_BE:
		case VBI3_PIXFMT_BGRA12_BE:
			SCAN_LINE_TO_RGB2 (RGBA_TO_RGBA12, 1);
			break;

		case VBI3_PIXFMT_ARGB12_LE:
		case VBI3_PIXFMT_ABGR12_LE:
			SCAN_LINE_TO_RGB2 (RGBA_TO_ARGB12, 0);
			break;

		case VBI3_PIXFMT_ARGB12_BE:
		case VBI3_PIXFMT_ABGR12_BE:
			SCAN_LINE_TO_RGB2 (RGBA_TO_ARGB12, 1);
			break;

		case VBI3_PIXFMT_RGB8:
			SCAN_LINE_TO_N (RGBA_TO_RGB8, 1);
			break;

		case VBI3_PIXFMT_BGR8:
			SCAN_LINE_TO_N (RGBA_TO_BGR8, 1);
			break;

		case VBI3_PIXFMT_RGBA7:
		case VBI3_PIXFMT_BGRA7:
			SCAN_LINE_TO_N (RGBA_TO_RGBA7, 1);
			break;

		case VBI3_PIXFMT_ARGB7:
		case VBI3_PIXFMT_ABGR7:
			SCAN_LINE_TO_N (RGBA_TO_ARGB7, 1);
			break;
		}

		s += sp8.bytes_per_line;
		d += sp->bytes_per_line;
	}

	vbi3_free (buf);

	return TRUE;
}

/*
	Capture interface
*/

#include "io-priv.h"
#include "hamm.h"

typedef struct {
	vbi3_capture		cap;

	vbi3_sampling_par	sp;

	vbi3_raw_decoder *	rd;

	vbi3_capture_buffer	raw_buffer;
	size_t			raw_f1_size;
	size_t			raw_f2_size;

	uint8_t *		desync_buffer[2];
	unsigned int		desync_i;

	double			capture_time;
	int64_t			stream_time;

	vbi3_capture_buffer	sliced_buffer;
	vbi3_sliced		sliced[50];

	unsigned int		teletext_page;
	unsigned int		teletext_row;

	uint8_t			caption_buf[4];
	unsigned int		caption_pause;
	unsigned int		caption_ch;
	unsigned int		caption_i;

	vbi3_program_id		vps_pid;

	vbi3_aspect_ratio	wss_aspect;
} vbi3_capture_sim;

static const char
caption_test_stream [] =
/* both fields not supported yet
	"<erase_display ch=3><roll_up rows=4><pac row=14>"
	"LIBZVBI CAPTION TEST STREAM.<cr>"
	"This is channel C4.<cr>"
	"Nothing to see here.<cr>"

	"<erase_display ch=2><roll_up rows=4><pac row=14>"
	"LIBZVBI CAPTION TEST STREAM.<cr>"
	"This is channel C3.<cr>"
	"Nothing to see here.<cr>"
*/
	"<erase-display ch=1><roll-up rows=4><pac row=14>"
	"LIBZVBI CAPTION TEST STREAM.<cr>"
	"This is channel C2.<cr>"
	"Nothing to see here.<cr>"

	"<erase-display ch=0>"
	"<roll-up rows=4>"
	"<pac row=14>"

      /* 12345678901234567890123456789012 */

	"LIBZVBI CAPTION TEST STREAM.<cr>"
	"This is roll-up caption on<cr>"
	"channel C1. The cursor stays<cr>"
	"on row 15 of 15.<pause><cr>"

	"We use a four row window, two<cr>"
	"rows just rolled out of view.<pause><cr>"

	"There are 32 columns. The cursor<cr>"
	"sticks at the end of a row.<cr>"
	"12345678901234567890123456789012<cr>"
	"This sentence is slightly too long.<pause><cr>"
	"The row above ends with \"l.\".<pause><cr>"

	"To assure correct reception we<cr>"
	"may repeat control codes. The<cr>"
	"decoder must ignore them.<cr>"
	"One music note: <special code=7><pause><cr>"
	"One music note (1 sent): "
	"<special code=7><special code=7><pause><cr>"
	"One music note (2 sent): "
	"<special code=7><special code=7><special code=7><pause><cr>"
	"One music note (4 sent): "
	"<special code=7><special code=7>"
	"<special code=7><special code=7><pause frames=120><cr>"
	"Two music notes by sending two<cr>"
	"non-consecutive codes: <special code=7><cmd code=0>"
	"<special code=7><pause frames=120><cr>"

	"We can produce an empty<cr>"
	"(transparent) row:<cr>"
	/* transmit a NUL to break control code repetition */
	"<cmd code=0><cr>"
	"by sending two CR commands.<pause><cr>"

	"We now change to a three row<cr>"
	"window. The top row disappears<cr>"
	"and the cursor stays on this row<pause>"
	"<roll-up rows=3><pause>"
	"<pac row=14>" /* back to column 1 */
	"overwriting it.<pause><cr>"

	"This is a three row window, and<cr>"
	"this is still row 15.<pause><cr>"
	"We now change to a two row<cr>"
	"window, the top row disappears.<pause>"
	"<roll-up rows=2><pause><cr>"

	"This is a two row window, and<cr>"
	"this is still row 15.<pause><cr>"

	"We change back to four rows,<cr>"
	"that won't restore the top two.<pause>"
	"<roll-up rows=4><pause><cr>"

	"These are three rows of text<pause><cr>"
	"and now four.<pause>"

      /* 12345678901234567890123456789012 */
	             " We move the<cr>"
	"window up one row, the cursor<cr>"
	"moves with it and overwrites<cr>"
	"this line.<pause>"
	"<pac row=13><pause>"

	"Squash.<pause><cr>"

	"You see four rows and the<cr>"
	"cursor is now in row 14. Row 15<cr>"
	"is empty (transparent).<pause><cr>"

	"Let's try some other rows:<cr>"
	"Now on 14.<pause>"

	"<pac row=12>Now on 13.<pause>"
	"<pac row=11>Now on 12.<pause>"
	"<pac row=10>Now on 11.<pause>"
	"<pac row=9>Now on 10.<pause>"
	"<pac row=8>Now on 9.<pause>"
	"<pac row=7>Now on 8.<pause>"
	"<pac row=6>"
	"For the next three moves we<cr>"
	"use indent codes instead of<cr>"
	"PAC, looks the same as before.<cr>"
	"Now on 7.<pause>"
	"<indent row=5>Now on 6.<pause>"
	"<indent row=4>Now on 5.<pause>"
	"<indent row=3>Now on 4.<pause><cr>"

      /* 12345678901234567890123456789012 */

	"On row 3 and above you can no<cr>"
	"longer see all four rows of<cr>"
	"the text window.<pause frames=120><cr>"
	"<pac row=2>Now on 3.<pause>"
	"<pac row=1>Now on 2.<pause><cr>"

	"But roll-up still works as<cr>"
	"before.<pause><cr>"

	"<pac row=0>"
	"Now on 1.<pause><cr>"

	"Still rolls.<pause><cr>"

	"Let's move back to row 6.<pause>"
	"<pac row=5><pause><cr>"

	"Now you see two rows of text.<pause><cr>"
	"But the text window is<cr>"
	"still four rows high.<pause><cr>"

	"We switch to a two row window<cr>"
	"and move to row 3.<pause>"
	"<roll-up rows=2><pac row=2><pause><cr>"

      /* 12345678901234567890123456789012 */

	"What happens when we switch to<cr>"
	"a four row window up here?<pause>"
	"<roll-up rows=4><pause><cr>"

	"Works fine, except you can<cr>"
	"see only three rows.<pause><cr>"

	"Testing erase display command.<pause><cr>"
	"<erase-display>"
	"You see one row of text.<pause><cr>"
	"Erasing the non-displayed buffer<cr>"
	"<erase-hidden>"
	"has no visible effect.<pause frames=120><cr>"

	"<erase-display>"
	"<resume-direct>"

                  /* 12345678901234567890123456789012 */

	"<pac row=12>LIBZVBI CAPTION TEST STREAM."
	"<pac row=13>This is paint-on caption on"
	"<pac row=14>channel C1.<pause frames=120>"

	"<pac row=11>Like roll-up caption the text<pause>"
	"<pac row=12>appears immediately. It does<pause>"
	"<pac row=13>not roll but prints top-down<pause>"
	"<pac row=14>overwriting old text.<pause frames=120>"

	"<erase-display>"
	"<pac row=12>From now on we erase the dis-"
	"<pac row=13>play before printing new text."
	"<pac row=14>You see three rows of text.<pause frames=120>"

	"<erase-display>"
	"<pac row=11>We can print on any four rows"
	"<pac row=12>(any number of rows with the"
	"<pac row=13>libzvbi caption decoder), in"
	"<pac row=14>any order.<pause frames=120>"

	"<erase-display>"
	"<pac row=0>Row 1.<pause>"
	"<pac row=3>Row 4.<pause>"
	"<pac row=10>Row 11.<pause>"
	"<pac row=7>Row 8.<pause>"
	"<pac row=12>Row 13.<pause>"
	"<pac row=1>Row 2.<pause>"
	"<pac row=6>Row 7.<pause>"
	"<pac row=9>Row 10.<pause>"
	"<pac row=5>Row 6.<pause>"
	"<pac row=13>Row 14.<pause>"
	"<pac row=14>Row 13.<pause>"
	"<pac row=2>Row 3.<pause>"
	"<pac row=4>Row 5.<pause>"
	"<pac row=8>Row 9.<pause>"
	"<pac row=11>Row 12.<pause frames=120>"

	"<erase-display>"
	"<indent row=0>Also with indent codes.<pause>"
	"<indent row=3>Row 4.<pause>"
	"<indent row=7>Row 8.<pause>"
	"<indent row=12>Row 13.<pause>"
	"<indent row=1>Row 2. Stop.<pause frames=120>"

	"<erase-display>"
	"<pac row=11>There are 32 columns. The cursor"
	"<pac row=12>sticks at the end of a row."
	"<pac row=13>12345678901234567890123456789012"
	"<pac row=14>This sentence is slightly too long.<pause frames=120>"

	"<erase-display>"
	"<pac row=13>The CR code has no effect, the"
	"<pac row=14>cursor does not move<pause>"
	"<cr>at all.<pause frames=120>"

	"<roll-up rows=4>"
	"<pac row=14>"
	"This is roll-up caption.<pause><cr>" 
	"The text rolls.<pause>"
	"<resume-direct>"
	"<pac row=10>Switching to paint-on mode"
	"<pac row=11>did not erase the display.<pause frames=120>"

	          /* 12345678901234567890123456789012 */

	"<erase-display>"
	"<pac row=10>Erasing the non-displayed buffer<pause>"
	"<erase-hidden>"
	"<pac row=11>has no visible effect.<pause frames=120>"

	"<erase-display>"
	"<erase-hidden>"
	"<resume-loading>"

	          /* 12345678901234567890123456789012 */

	"<pac row=12>LIBZVBI CAPTION TEST STREAM."
	"<pac row=13>This is pop-on caption on"
	"<pac row=14>channel C1."
	"<end-of-caption><pause frames=120>"

	"<pac row=11>Text is written into a non-"
	"<pac row=12>displayed buffer and appears"
	"<pac row=13>at once upon receipt of an"
	"<pac row=14>end-of-caption code."
	"<end-of-caption><pause frames=120>"

	"<erase-hidden>"
	"<pac row=12>Testing if we erased the non-"
	"<pac row=13>displayed buffer. You see three"
	"<pac row=14>rows of text."
	"<end-of-caption><pause frames=120>"

	"<erase-hidden>"
	"<pac row=11>The end-of-caption code does"
	"<pac row=12>not erase buffers, it merely"
	"<pac row=13>swaps them. This is buffer 1."
	"<end-of-caption><pause frames=120>"

	"<erase-hidden>"
	"<pac row=13>This is buffer 2."
	"<end-of-caption><pause frames=120>"
	"<end-of-caption>" /* repeated control code, ignored */
	"<end-of-caption><pause frames=120>"
	"<end-of-caption>" /* repeated control code, ignored */
	"<end-of-caption><pause frames=120>"
	"<pac row=14>Wrote row 14 in buffer 1."
	"<end-of-caption><pause frames=120>"
	"<pac row=12>Wrote row 12 in buffer 2."
	"<end-of-caption><pause frames=120>"
	"<end-of-caption>" /* repeated control code, ignored */
	"<end-of-caption><pause frames=120>"
	"<end-of-caption>" /* repeated control code, ignored */
	"<end-of-caption><pause frames=120>"

	"<erase-hidden>"
	"<pac row=11>We can print on any four rows"
	"<pac row=12>(any number of rows with the"
	"<pac row=13>libzvbi caption decoder), in"
	"<pac row=14>any order."
	"<end-of-caption><pause frames=120>"

	"<erase-hidden>"
	"<pac row=0>Row 1."
	"<pac row=3>Row 4."
	"<pac row=10>Row 11."
	"<pac row=7>Row 8."
	"<pac row=12>Row 13."
	"<pac row=1>Row 2."
	"<pac row=6>Row 7."
	"<pac row=9>Row 10."
	"<pac row=5>Row 6."
	"<pac row=13>Row 14."
	"<pac row=14>Row 15."
	"<pac row=2>Row 3."
	"<pac row=4>Row 5."
	"<pac row=8>Row 9."
	"<pac row=11>Row 12."
	"<end-of-caption><pause frames=120>"

	"<erase-hidden>"
	"<indent row=0>Also with indent codes."
	"<indent row=3>Row 4."
	"<indent row=10>Row 11."
	"<indent row=7>Row 8. You see four rows."
	"<end-of-caption><pause frames=120>"

	"<erase-hidden>"
	"<pac row=11>There are 32 columns. The cursor"
	"<pac row=12>sticks at the end of a row."
	"<pac row=13>12345678901234567890123456789012"
	"<pac row=14>This sentence is slightly too long."
	"<end-of-caption><pause frames=120>"

	"<erase-hidden>"
	"<pac row=13>The CR code has no effect, the"
	"<pac row=14>cursor does not move &gt;<cr>&lt; at all."
	"<end-of-caption><pause frames=120>"

	"<erase-display>"
	"<roll-up rows=4>"
	"<pac row=14>"
	"This is roll-up caption.<pause><cr>" 
	"The text rolls.<pause>"
	"<resume-loading>"
	"<erase-hidden>"
	"<pac row=14>Now in pop-on mode."
	"<end-of-caption><pause>"
	"<pac row=10>Switching to pop-on mode did"
	"<pac row=11>not erase the roll-up text."
	"<end-of-caption><pause>"

	"<erase-hidden>"
	"<pac row=12>We can erase the display"
	"<pac row=13>at any time without side"
	"<pac row=14>effects."
	"<end-of-caption><pause>"

	"<erase-hidden>"
	"<pac row=11>Writing into the non-displayed"
	"<pac row=12>buffer, we erase the display."
	"<erase-display><pause>"
	"<pac row=13>Done. Now we swap buffers,"
	"<pac row=14>and you see four rows."
	"<end-of-caption><pause frames=120>"

	"<erase-display>"
	"<roll-up rows=4>"
	"<pac row=14>"

      /* 12345678901234567890123456789012 */

	"LIBZVBI CAPTION TEST STREAM.<cr>"
	"Testing horizontal cursor<cr>"
	"motion in roll-up mode.<pause><cr><cr>"

	"<tab cols=2>"
	"<cmd code=0>" /* transmit a NUL to break control code repetition */
	"<tab cols=2>"
	"This row is indented by<cr>"
	"four columns. We sent three<cr>"
	"tab-2 commands, the decoder<cr>"
	"must ignore the second one.<pause frames=120><cr>"

	"Let's try all columns.<cr>"
	/* Note indent does not transmit the 2 lsb of cols. */
	"<cr><indent row=14 cols=0><tab cols=0>Blah.<pause>"
	"<cr><indent row=14 cols=1><tab cols=1>Blah.<pause>"
	"<cr><indent row=14 cols=2><tab cols=2>Blah.<pause>"
	"<cr><indent row=14 cols=3><tab cols=3>Blah.<pause>"
	"<cr><indent row=14 cols=4><tab cols=0>Blah.<pause>"
	"<cr><indent row=14 cols=5><tab cols=1>Blah.<pause>"
	"<cr><indent row=14 cols=6><tab cols=2>Blah.<pause>"
	"<cr><indent row=14 cols=7><tab cols=3>Blah.<pause>"
	"<cr><indent row=14 cols=8><tab cols=0>Blah.<pause>"
	"<cr><indent row=14 cols=9><tab cols=1>Blah.<pause>"
	"<cr><indent row=14 cols=10><tab cols=2>Blah.<pause>"
	"<cr><indent row=14 cols=11><tab cols=3>Blah.<pause>"
	"<cr><indent row=14 cols=12><tab cols=0>Blah.<pause>"
	"<cr><indent row=14 cols=13><tab cols=1>Blah.<pause>"
	"<cr><indent row=14 cols=14><tab cols=2>Blah.<pause>"
	"<cr><indent row=14 cols=15><tab cols=3>Blah.<pause>"
	"<cr><indent row=14 cols=16><tab cols=0>Blah.<pause>"
	"<cr><indent row=14 cols=17><tab cols=1>Blah.<pause>"
	"<cr><indent row=14 cols=18><tab cols=2>Blah.<pause>"
	"<cr><indent row=14 cols=19><tab cols=3>Blah.<pause>"
	"<cr><indent row=14 cols=20><tab cols=0>Blah.<pause>"
	"<cr><indent row=14 cols=21><tab cols=1>Blah.<pause>"
	"<cr><indent row=14 cols=22><tab cols=2>Blah.<pause>"
	"<cr><indent row=14 cols=23><tab cols=3>Blah.<pause>"
	"<cr><indent row=14 cols=24><tab cols=0>Blah.<pause>"
	"<cr><indent row=14 cols=25><tab cols=1>Blah.<pause>"
	"<cr><indent row=14 cols=26><tab cols=2>Blah.<pause>"
	"<cr><indent row=14 cols=27><tab cols=3>Blah.<pause>"
	"<cr><indent row=14 cols=28><tab cols=0>Blah.<pause>"
	"<cr><indent row=14 cols=29><tab cols=1>Blah.<pause>"
	"<cr><indent row=14 cols=30><tab cols=2>Blah.<pause>"
	"<cr><indent row=14 cols=31><tab cols=3>Blah.<pause>"
	"<cr>"

	"The previous two rows contain<cr>"
	"just \"B.\" and \".\".<pause><cr>"

	"We can move the cursor<cr>"
	"to any column.<cr>"
	"12345678901234567890123456789012<cr>"
	"<indent row=14 cols=28>29<pause>"
	"<indent row=14 cols=12>13<pause>"
	"<indent row=14 cols=16><tab cols=3>20<pause><cr>"

	"A CR here &gt; &lt; does not split"
	"<indent row=14 cols=8><tab cols=3><cr><pause>"
	"the row.<pause><cr>"

	"<erase-display>"
                         /* 1234567890123 */
	"<indent row=3 cols=16>This text<cr>"
	"<indent row=3 cols=16>appears<cr>"
	"<indent row=3 cols=16>in the upper<cr>"
	"<indent row=3 cols=16>right corner.<pause><cr>"

	"<erase-display>"
	"<pac row=14>"

	"These rows do not start or end<cr>"
	"with a space but the decoder may<cr>"
	"add one for readability.<pause><cr>"

	" This row starts with a space.<cr>"
	"This row ends with a space. <cr>"
	"The decoder must not add<cr>"
	"another opaque space.<pause><cr>"

	"This &gt;<special code=9>&lt; is a transparent space.<cr>"
	"Two &gt;"
	"<special code=9>"
	"<cmd code=0>" /* transmit a NUL to break control code repetition */
	"<special code=9>"
	"&lt; transparent spaces.<cr>"
	"The decoder may replace<cr>"
	"them by opaque spaces.<pause frames=120><cr>" 

	"Three &gt;"
	"<special code=9>"
	"<cmd code=0>" /* transmit a NUL to break control code repetition */
	"<special code=9>"
	"<cmd code=0>" /* transmit a NUL to break control code repetition */
	"<special code=9>"
 	"&lt; transparent spaces.<cr>"
	"The decoder must not replace the<cr>"
	"second one by an opaque space.<pause frames=120><cr>"

      /* 12345678901234567890123456789012 */

	"This &gt;<tab cols=1>&lt; looks like the<cr>"
	"transparent space above but<cr>"
	"we just advanced the cursor.<pause frames=120><cr>"
	"Again &gt;<tab cols=1>&lt; here.<pause><cr>"
	"Again  &gt;<tab cols=1>&lt; here.<pause><cr>"
	"Two columns &gt;<tab cols=2>&lt; here.<pause><cr>"
	"Three columns &gt;<tab cols=3>&lt; here.<pause><cr><cr>"

	"Backspace test<cr>"
	"This row is correcXX<pause>"
	"<bs>"
	"<cmd code=0>" /* transmit a NUL to break control code repetition */
	"<bs>t.<pause><cr>"

	"How does it work in the middle<cr>"
	"of a row &gt;here<?<pause>"
	"<indent row=14 cols=12><tab cols=2>"
	"<bs>"
	"<cmd code=0>" /* transmit a NUL to break control code repetition */
	"<bs>"
	"<cmd code=0>" /* transmit a NUL to break control code repetition */
	"<bs>"
	"<cmd code=0>" /* transmit a NUL to break control code repetition */
	"<bs>"
	"<cr>"
	"The text was replaced by<cr>"
	"transparent spaces.<pause><cr>"

	"What happens when we<cr>"
	"backspace in column 1?<cr>"
	"<bs>"
	"Nothing.<pause><cr>"

	"Delete-to-end-of-row test<cr>"
	"here &gt;This text disappears.<pause>"
	"<indent row=14 cols=4><tab cols=2><delete-eol><cr>"
	"The text after &gt; was<cr>"
	"replaced by transparent spaces.<pause><cr>"

	"In column 1:<cr>"
	"This text disappears.<pause>"
	"<indent row=14><delete-eol><cr>"
	"The row above is empty<cr>"
	"(transparent).<pause><cr>"

	"In column 32:<cr>"
	"12345678901234567890123456789012<pause>"
	"<indent row=14 cols=28><tab cols=3><delete-eol><cr>"
	"The row now ends with a 1.<pause><cr>"
	"<cr>"

	"<erase-display>"
	"<roll-up rows=4>"
	"<pac row=14>"

      /* 12345678901234567890123456789012 */

	"LIBZVBI CAPTION TEST STREAM<cr>"
	"Testing text attribute commands<cr>"
	"in roll-up mode.<pause><cr><cr>"

	"Testing preamble address codes.<cr><cr>"

	"<pac row=14>White text<pause><cr>"
	"<pac row=14 fg=1>Green text<pause><cr>"
	"<pac row=14 fg=2>Blue text<pause><cr>"
	"<pac row=14 fg=3>Cyan text<pause><cr>"
	"<pac row=14 fg=4>Red text<pause><cr>"
	"<pac row=14 fg=5>Yellow text<pause><cr>"
	"<pac row=14 fg=6>Magenta text<pause><cr>"
	"<pac row=14 fg=7>White italic text<pause><cr>"

	"<pac row=14 u=1>White underlined<pause><cr>"
	"<pac row=14 fg=1 u=1>Green underlined<pause><cr>"
	"<pac row=14 fg=2 u=1>Blue underlined<pause><cr>"
	"<pac row=14 fg=3 u=1>Cyan underlined<pause><cr>"
	"<pac row=14 fg=4 u=1>Red underlined<pause><cr>"
	"<pac row=14 fg=5 u=1>Yellow underlined<pause><cr>"
	"<pac row=14 fg=6 u=1>Magenta underlined<pause><cr>"
	"<pac row=14 fg=7 u=1>White italic underlined<pause><cr>"
	"Still white italic underlined.<pause><cr>"
	"<pac row=14><cr>"

	"Testing indent codes.<pause><cr><cr>"

	"<indent row=14>White text<pause><cr>"
	"<indent row=14 u=1>White underlined text<pause><cr>"
	"<pac row=14>Indent &gt;"
	"<indent row=14 cols=8 u=1>White underl. here<pause><cr>"
	"Still white underlined.<pause><cr>"
	"<pac row=14><cr>"

	"Testing midrow codes. One opaque<cr>"
	"space appears between &gt;&lt;.<pause><cr><cr>"

	"<pac row=14 fg=7 u=1>White ital. underl. &gt;"
	"<mr>&lt; white<pause><cr>"
	"<pac row=14 fg=7 u=1>White ital. underl. &gt;"
	"<mr fg=1>&lt; green<pause><cr>"
	"<pac row=14 fg=7 u=1>White ital. underl. &gt;"
	"<mr fg=2>&lt; blue<pause><cr>"
	"<pac row=14 fg=7 u=1>White ital. underl. &gt;"
	"<mr fg=3>&lt; cyan<pause><cr>"
	"<pac row=14 fg=7 u=1>White ital. underl. &gt;"
	"<mr fg=4>&lt; red<pause><cr>"
	"<pac row=14 fg=7 u=1>White ital. underl. &gt;"
	"<mr fg=5>&lt; yellow<pause><cr>"
	"<pac row=14 fg=7 u=1>White ital. underl. &gt;"
	"<mr fg=6>&lt; magenta<pause><cr>"
	"Still magenta &gt;"
	"<mr fg=7>&lt; mag. ital.<pause><cr>"

	"<pac row=14 fg=7>White ital. &gt;"
	"<mr u=1>&lt; white underl.<pause><cr>"
	"<pac row=14 fg=7>White ital. &gt;"
	"<mr fg=1 u=1>&lt; green underl.<pause><cr>"
	"<pac row=14 fg=7>White ital. &gt;"
	"<mr fg=2 u=1>&lt; blue underl.<pause><cr>"
	"<pac row=14 fg=7>White ital. &gt;"
	"<mr fg=3 u=1>&lt; cyan underl.<pause><cr>"
	"<pac row=14 fg=7>White ital. &gt;"
	"<mr fg=4 u=1>&lt; red underl.<pause><cr>"
	"<pac row=14 fg=7>White ital. &gt;"
	"<mr fg=5 u=1>&lt; yellow underl.<pause><cr>"
	"<pac row=14 fg=7>White ital. &gt;"
	"<mr fg=6 u=1>&lt; magenta underl.<pause><cr>"

	"<pac row=14 u=1>White underl. &gt;"
	"<mr u=1>&lt; still underl.<pause><cr>"
	"<pac row=14>"
	"The &gt; &lt; above is underlined.<pause><cr>"
	"<cr>"

	"Testing flash-on code.<pause><cr><cr>"

	"Solid &gt;<flash-on>&lt; flashing text.<pause><cr>"
	"Flashing (CR) &gt;<mr>&lt; solid (MR).<pause><cr>"
	"<flash-on><bs>Flashing.<cr>"
	"<pac row=14>"	"Still flashing (PAC).<pause><cr>"
	"<indent row=14>Still flashing (indent).<pause><cr>"
	"<mr fg=7 u=1><bs>Solid italic underl. (MR).<pause><cr>"
	"<indent row=14><cr>"

	"Testing optional attributes.<pause><cr><cr>"

	"Text with black background.<pause><cr>"
	/* For compatibility with older decoders which ignore these
	   commands we must print a space, and the command will
	   backspace and replace it by a spacing attribute. */
	"Black &gt; <transp-bg>&lt; transparent.<pause><cr>"
	"Still transparent background.<pause><cr>"
	"Still transp. &gt;<mr>&lt; still (MR).<pause><cr>"
	"<pac row=14>Text with opaque background.<pause><cr>"

	/* Should the background change at or after the
	   spacing attribute? Have no docs. */

	"<pac row=14 fg=1>Green on black text.<pause><cr>"
	"Green/black &gt; <bg bg=0>&lt; green/white.<pause><cr>"
	"Green/white &gt; <black>&lt; black/white.<pause><cr>"
	"Black/white &gt; <black u=1>&lt; black underl.<pause><cr>"
	"<pac row=14>White/black &gt; <bg bg=1>"
	"&lt; on green.<pause><cr>"
	"<pac row=14>Black &gt; <bg bg=2>&lt; blue.<pause><cr>"
	"<pac row=14>Black &gt; <bg bg=3>&lt; cyan.<pause><cr>"
	"<pac row=14>Black &gt; <bg bg=4>&lt; red.<pause><cr>"
	"Still red (CR).<pause><cr>"
	"Red &gt;<mr>&lt; still red (MR).<pause><cr>"
	"<pac row=14>Black &gt; <bg bg=5>&lt; yellow.<pause><cr>"
	"<pac row=14>Black &gt; <bg bg=6>&lt; magenta.<pause><cr>"
	"<pac row=14>Black &gt; <bg bg=7>&lt; black.<pause><cr>"
	"<cr>"

	"Testing translucent background.<pause><cr><cr>"

	"<pac row=14 fg=1>Green/black &gt; <bg bg=0 t=1>"
	"&lt; white transl.<pause><cr>"
	"<pac row=14>White/black &gt; <bg bg=1 t=1>"
	"&lt; green transl.<pause><cr>"
	"<pac row=14>Black &gt; <bg bg=2 t=1>&lt; blue transl.<pause><cr>"
	"<pac row=14>Black &gt; <bg bg=3 t=1>&lt; cyan transl.<pause><cr>"
	"<pac row=14>Black &gt; <bg bg=4 t=1>&lt; red transl.<pause><cr>"
	"Still red translucent (CR).<pause><cr>"
	"Red transl. &gt;<mr>&lt; still (MR).<pause><cr>"
	"<pac row=14>Black &gt; <bg bg=5 t=1>&lt; yellow transl.<pause><cr>"
	"<pac row=14>Black &gt; <bg bg=6 t=1>&lt; magenta transl.<pause><cr>"
	"<pac row=14>Black &gt; <bg bg=7 t=1>&lt; black transl.<pause>"
	"<pause><cr>"

	"<erase-display>"
	"<roll-up rows=4>"
	"<pac row=14>"

	"LIBZVBI CAPTION TEST STREAM.<cr>"
	"Character repertoir.<pause><cr><cr>"

	" !\"#$%&amp;\'().+,-./0123456789:;&lt;=&gt;?<pause><cr>"
	"@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_<pause><cr>"
	"`abcdefghijklmnopqrstuvwxyz{|}~\177<pause><cr>"

	"<mr fg=7><bs>" /* italic */

	" !\"#$%&amp;\'().+,-./0123456789:;&lt;=&gt;?<pause><cr>"
	"@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_<pause><cr>"
	"`abcdefghijklmnopqrstuvwxyz{|}~\177<pause><cr>"

	"<mr u=1><bs>" /* underlined */

	" !\"#$%&amp;\'().+,-./0123456789:;&lt;=&gt;?<pause><cr>"
	"@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_<pause><cr>"
	"`abcdefghijklmnopqrstuvwxyz{|}~\177<pause><cr>"

	"<mr fg=7 u=1><bs>" /* italic underlined */

	" !\"#$%&amp;\'().+,-./0123456789:;&lt;=&gt;?<pause><cr>"
	"@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_<pause><cr>"
	"`abcdefghijklmnopqrstuvwxyz{|}~\177<pause><cr>"
	"<pac row=14><cr>"
	
	"30 Registered mark: <special code=0><pause><cr>"
	"31 Degree sign:     <special code=1><pause><cr>"
	"32 1/2:             <special code=2><pause><cr>"
	"33 Inverse quest.:  <special code=3><pause><cr>"
	"34 Trademark:       <special code=4><pause><cr>"
	"35 Cents sign:      <special code=5><pause><cr>"
	"36 Sterling sign:   <special code=6><pause><cr>"
	"37 Music note:      <special code=7><pause><cr>"
	"38 a grave:         <special code=8><pause><cr>"
	"39 Transp. space:   <special code=9><pause><cr>"
	"3A e grave:         <special code=10><pause><cr>"
	"3B a circumflex:    <special code=11><pause><cr>"
	"3C e circumflex:    <special code=12><pause><cr>"
	"3D i circumflex:    <special code=13><pause><cr>"
	"3E o circumflex:    <special code=14><pause><cr>"
	"3F u circumflex:    <special code=15><pause frames=120><cr>"

	"<erase-display>"
	"<roll-up rows=4>"
	"<pac row=14>"

      /* 12345678901234567890123456789012 */

	"LIBZVBI CAPTION TEST STREAM.<cr>"
	"Text mode test. You must switch<cr>"
	"to channel T1 to see it.<pause frames=120>"

	"<text-restart>"
	"<erase-display>"

	"This is text mode on channel T1<cr>"
	"I have next to no documentation<cr>"
	"about this mode so we just test<cr>"
	"the libzvbi implementation.<pause frames=120><cr><cr><cr>"

	"The text window has a size of<cr>"
	"32 x 15 characters. The default<cr>"
	"background is opaque black, not<cr>"
	"transparent.<pause frames=120><cr><cr><cr>"

	"<roll-up ch=0>End of test stream.<cr>"
	"<roll-up ch=1>End of test stream.<cr>"
/* both fields not supported yet
	"<roll-up ch=2>End of test stream.<cr>"
	"<roll-up ch=3>End of test stream.<cr>"
*/
	"<text-restart ch=0>End of test stream.<cr>"
	"<text-restart ch=1>End of test stream.<cr>"
/*
	"<text-restart ch=2>End of test stream.<cr>"
	"<text-restart ch=3>End of test stream.<cr>"
*/
	"<pause frames=300>"

	/* TODO: more on text mode, other channels,
	   multiplexing, XDS, ITV. */

	/* ROLL_UP, ROLL_UP, ROLL_UP changes cursor position? */

	/* TODO: regression test for repeated control code bug:
	   <control code field 1>
	   <zero field 2>
	   <repeated control code field 1, to be ignored> */ 
;

static unsigned int
get_attr			(const char *		s,
				 const char *		name,
				 unsigned int		default_value,
				 unsigned int		minimum,
				 unsigned int		maximum)
{
	unsigned long value;
	unsigned int len;

	value = default_value;

	len = strlen (name);

	for (; 0 != *s && '>' != *s; ++s) {
		if (0 == strncmp (s, name, len)) {
			s += len;

			while (isspace (*s++))
				;

			if ('=' == s[-1]) {
				value = strtoul (s, NULL, 10);
				value = SATURATE (value,
						  (unsigned long) minimum,
						  (unsigned long) maximum);
			}

			break;
		}
	}

	return (unsigned int) value;
}

static unsigned int
caption_command			(vbi3_capture_sim *	sim,
				 const char *		s,
				 unsigned int *		n_frames)
{
	static const struct {
		unsigned int	code;
		const char *	name;
	} elements [] = {
		{ 0x1020, "bg" },
		{ 0x172E, "black" },
		{ 0x1421, "bs" },
		{ 0x0001, "cmd" },
		{ 0x142D, "cr" },
		{ 0x1424, "delete-eol" },
		{ 0x142F, "end-of-caption" },
		{ 0x142C, "erase-display" },
		{ 0x142E, "erase-hidden" },
		{ 0x1428, "flash-on" },
		{ 0x1050, "indent" },
		{ 0x1120, "mr" },
		{ 0x1040, "pac" },
		{ 0x0002, "pause" },
		{ 0x1429, "resume-direct" },
		{ 0x1420, "resume-loading" },
		{ 0x142B, "resume-text" },
		{ 0x1425, "roll-up" },
		{ 0x1130, "special" },
		{ 0x1720, "tab" },
		{ 0x142A, "text-restart" },
		{ 0x172D, "transp-bg" },
	};
	static const uint8_t row_mapping [16] = {
		/* 0 */ 2, 3, 4, 5,
		/* 4 */ 10, 11, 12, 13, 14, 15,
		/* 10 */ 0,
		/* 11 */ 6, 7, 8, 9,
		/* 15 */ -1
	};
	unsigned int len;
	unsigned int cmd;
	unsigned int row;
	unsigned int i;

	for (i = 0; i < N_ELEMENTS (elements); ++i) {
		len = strlen (elements[i].name);
		if (0 == strncmp (s, elements[i].name, len))
			break;
	}

	if (i >= N_ELEMENTS (elements))
		return -1;

	s += len;

	if ('>' != *s && ' ' != *s)
		return -1;

	sim->caption_ch = get_attr (s, "ch", sim->caption_ch, 0, 3);

	cmd = elements[i].code | ((sim->caption_ch & 1) << 11);

	switch (elements[i].code) {
	case 1: /* cmd */
		return get_attr (s, "code", 0, 0, 0xFFFF);

	case 2: /* pause */
		*n_frames = get_attr (s, "frames", 60, 1, INT_MAX);
		return -2;

	case 0x1020: /* bg */
		cmd |= get_attr (s, "bg", 0, 0, 7) << 1; /* backgr color */
		cmd |= get_attr (s, "t", 0, 0, 1);	 /* transparent */
		return cmd;

	case 0x172E: /* black */
		return cmd | get_attr (s, "u", 0, 0, 1); /* underlined */

	case 0x1050: /* indent */
		row = row_mapping[get_attr (s, "row", 14, 0, 14)];
		cmd |= (row & 0xE) << 7;
		cmd |= (row & 0x1) << 5;
		cmd |= (get_attr (s, "cols", 0, 0, 31) / 4) << 1;
		cmd |= get_attr (s, "u", 0, 0, 1);
		return cmd;

	case 0x1120: /* mr */
		cmd |= get_attr (s, "fg", 0, 0, 7) << 1; /* text color */
		cmd |= get_attr (s, "u", 0, 0, 1);
		return cmd;

	case 0x1040: /* pac */
		row = row_mapping[get_attr (s, "row", 14, 0, 14)];
		cmd |= (row & 0xE) << 7;
		cmd |= (row & 0x1) << 5;
		cmd |= get_attr (s, "fg", 0, 0, 7) << 1;
		cmd |= get_attr (s, "u", 0, 0, 1);
		return cmd;

	case 0x1425: /* roll_up */
		row = get_attr (s, "rows", 2, 2, 4);
		cmd |= (sim->caption_ch & 2) << 7;
		cmd += row - 2;
		return cmd;

	case 0x1130: /* special character */
		cmd |= get_attr (s, "code", 0, 0, 15);
		return cmd;

	case 0x1720: /* tab */
		cmd |= ((sim->caption_ch & 2) << 7);
		cmd |= get_attr (s, "cols", 0, 0, 3);
		return cmd;

	case 0x172D: /* transparent_bg */
		return cmd;

	default:
		return cmd | ((sim->caption_ch & 2) << 7);
	}
}

static void
gen_caption_next		(vbi3_capture_sim *	sim)
{
	const char *s;
	unsigned int i;

	CLEAR (sim->caption_buf);

	s = caption_test_stream + sim->caption_i;

	for (i = 0; i < 2;) {
		int c = *s & 0x7F;

		if (0 == c) {
			s = caption_test_stream;
			continue;
		} else if (c < 0x20) {
			++s;
			continue;
		} else if ('&' == c) {
			if (0 == strncmp (s, "&amp;", 5)) {
				s += 5;
			} else if (0 == strncmp (s, "&lt;", 4)) {
				s += 4;
				c = '<';
			} else if (0 == strncmp (s, "&gt;", 4)) {
				s += 4;
				c = '>';
			} else {
				++s;
			}

			sim->caption_buf[i++] = vbi3_par8 (c);
		} else if ('<' == c) {
			unsigned int cmd;
			unsigned int n_frames;

			if (i & 1)
				sim->caption_buf[i++] = 0x80;

			do ++s;
			while (0x20 == *s);

			cmd = caption_command (sim, s, &n_frames);

			if ((unsigned int) -2 == cmd) {
				sim->caption_pause = n_frames;
				i = 2;
			} else if ((unsigned int) -1 != cmd) {
				sim->caption_buf[i++] = vbi3_par8 (cmd >> 8);
				sim->caption_buf[i++] = vbi3_par8 (cmd & 0xFF);
			}

			do {
				c = *s++;
				if (0 == c) {
					s = caption_test_stream;
					break;
				}
			} while ('>' != c);
		} else {
			sim->caption_buf[i++] = vbi3_par8 (c);
			++s;
		}
	}

	sim->caption_i = s - caption_test_stream;
}

static void
gen_caption			(vbi3_capture_sim *	sim,
				 vbi3_sliced **		inout_sliced,
				 vbi3_service_set	service_set,
				 unsigned int		line)
{
	vbi3_sliced *s;

	s = *inout_sliced;
	*inout_sliced = s + 1;

	s->id = service_set;
	s->line = line;

	while (0 == sim->caption_buf[0]) {
		if (sim->caption_pause > 0) {
			s->data[0] = 0x80;
			s->data[1] = 0x80;

			--sim->caption_pause;

			return;
		}

		gen_caption_next (sim);
	}

	s->data[0] = sim->caption_buf[0];
	s->data[1] = sim->caption_buf[1];

	sim->caption_buf[0] = sim->caption_buf[2];
	sim->caption_buf[1] = sim->caption_buf[3];
	sim->caption_buf[2] = 0;
	sim->caption_buf[3] = 0;
}

static void
gen_teletext_b_row		(vbi3_capture_sim *	sim,
				 uint8_t	 	return_buf[45])
{
	static uint8_t s1[2][10] = {
		{ 0x02, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15 },
		{ 0x02, 0x15, 0x02, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15 }
	};
	static uint8_t s2[32] = "100\2LIBZVBI\7            00:00:00";
	static uint8_t s3[40] = "  LIBZVBI TELETEXT SIMULATION           ";
	static uint8_t s4[40] = "  Page 100                              ";
	static uint8_t s5[10][42] = {
		{ 0x02, 0x2f, 0x97, 0x20, 0x37, 0x23, 0x23, 0x23, 0x23, 0x23,
		  0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23,
		  0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23,
		  0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23,
		  0xb5, 0x20 },
		{ 0xc7, 0x2f, 0x97, 0x0d, 0xb5, 0x04, 0x20, 0x9d, 0x83, 0x8c,
		  0x08, 0x2a, 0x2a, 0x2a, 0x89, 0x20, 0x20, 0x0d, 0x54, 0x45,
		  0xd3, 0x54, 0x20, 0xd0, 0xc1, 0xc7, 0x45, 0x8c, 0x20, 0x20,
		  0x08, 0x2a, 0x2a, 0x2a, 0x89, 0x0d, 0x20, 0x20, 0x1c, 0x97,
		  0xb5, 0x20 },
		{ 0x02, 0xd0, 0x97, 0x20, 0xb5, 0x20, 0x20, 0x20, 0x20, 0x20,
		  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
		  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
		  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
		  0xea, 0x20 },
		{ 0xc7, 0xd0, 0x97, 0x20, 0xb5, 0x20, 0x20, 0x20, 0x20, 0x20,
		  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
		  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
		  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
		  0xb5, 0x20 },
		{ 0x02, 0xc7, 0x97, 0x20, 0xb5, 0x20, 0x20, 0x20, 0x20, 0x20,
		  0x20, 0x15, 0x1a, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c,
		  0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c,
		  0x2c, 0x2c, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x97, 0x19,
		  0xb5, 0x20 },
		{ 0xc7, 0xc7, 0x97, 0x20, 0xb5, 0x20, 0x20, 0x20, 0x20, 0x20,
		  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
		  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
		  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
		  0xb5, 0x20 },
		{ 0x02, 0x8c, 0x97, 0x9e, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x13,
		  0x7f, 0x7f, 0x7f, 0x7f, 0x16, 0x7f, 0x7f, 0x7f, 0x7f, 0x92,
		  0x7f, 0x92, 0x7f, 0x7f, 0x15, 0x7f, 0x7f, 0x15, 0x7f, 0x91,
		  0x91, 0x7f, 0x7f, 0x91, 0x94, 0x7f, 0x94, 0x7f, 0x94, 0x97,
		  0xb5, 0x20 },
		{ 0xc7, 0x8c, 0x97, 0x9e, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x13,
		  0x7f, 0x7f, 0x7f, 0x7f, 0x16, 0x7f, 0x7f, 0x7f, 0x7f, 0x92,
		  0x7f, 0x7f, 0x7f, 0x7f, 0x15, 0x7f, 0x7f, 0x7f, 0x7f, 0x91,
		  0x7f, 0x7f, 0x7f, 0x7f, 0x94, 0x7f, 0x7f, 0x7f, 0x7f, 0x97,
		  0xb5, 0x20 },
		{ 0x02, 0x9b, 0x97, 0x9e, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x13,
		  0x7f, 0x7f, 0x7f, 0x7f, 0x16, 0x7f, 0x7f, 0x7f, 0x7f, 0x92,
		  0x7f, 0x7f, 0x7f, 0x7f, 0x15, 0x7f, 0x7f, 0x7f, 0x7f, 0x91,
		  0x7f, 0x7f, 0x7f, 0x7f, 0x94, 0x7f, 0x7f, 0x7f, 0x7f, 0x97,
		  0xb5, 0x20 },
		{ 0xc7, 0x9b, 0x97, 0x20, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23,
		  0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23,
		  0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23,
		  0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23, 0x23,
		  0xa1, 0x20 }
	};
	unsigned int i;

	return_buf[0] = 0x55;
	return_buf[1] = 0x55;
	return_buf[2] = 0x27;

	if (sim->teletext_row >= 13)
		sim->teletext_row = 0;

	switch (sim->teletext_row) {
	case 0:
		memcpy (return_buf + 3, s1[sim->teletext_page], 10);
		sim->teletext_page ^= 1;
		for (i = 0; i < 32; ++i)
			return_buf[13 + i] = vbi3_par8 (s2[i]);
		break;

	case 1:
		return_buf[3] = 0x02;
		return_buf[4] = 0x02;
		for (i = 0; i < 40; ++i)
			return_buf[5 + i] = vbi3_par8 (s3[i]);
		break;

	case 2:
		return_buf[3] = 0x02;
		return_buf[4] = 0x49;
		for (i = 0; i < 40; ++i)
			return_buf[5 + i] = vbi3_par8 (s4[i]);
		break;

	default:
		memcpy (return_buf + 3, s5[sim->teletext_row - 3], 42);
		break;
	}

	++sim->teletext_row;
}

static void
gen_teletext_b			(vbi3_capture_sim *	sim,
				 vbi3_sliced **		inout_sliced,
				 vbi3_sliced *		sliced_end,
				 unsigned int		field)
{
	static const uint16_t lines [] = {
		9, 10, 11, 12, 13, 14, 15,
		19, 20, 21,
		320, 321, 322, 323, 324, 325, 326, 327, 328,
		332, 333, 334, 335
	};
	vbi3_sliced *s;
	unsigned int i;

	s = *inout_sliced;

	for (i = 0; i < N_ELEMENTS (lines); ++i) {
		uint8_t buf[45];

		if ((0 == field) != (lines[i] < 313))
			continue;

		if (s >= sliced_end)
			break;

		s->id = VBI3_SLICED_TELETEXT_B;
		s->line = lines[i];

		gen_teletext_b_row (sim, buf);

		memcpy (&s->data, buf + 3, 42);

		++s;
	}

	*inout_sliced = s;
}

static unsigned int
gen_sliced_525			(vbi3_capture_sim *	sim)
{
	vbi3_sliced *s;
	unsigned int i;

	s = sim->sliced;

	assert (N_ELEMENTS (sim->sliced) >= 4);

	if (0) {
		for (i = 0; i < N_ELEMENTS (s->data); ++i)
			s->data[i] = rand ();

		s[1] = s[0];
		s[2] = s[0];

		s[0].id = VBI3_SLICED_TELETEXT_B_525;
		s[0].line = 10;
		s[1].id = VBI3_SLICED_TELETEXT_C_525;
		s[1].line = 11;
		s[2].id = VBI3_SLICED_TELETEXT_D_525;
		s[2].line = 12;

		s += 3;
	}

	gen_caption (sim, &s, VBI3_SLICED_CAPTION_525, 21);

	return s - sim->sliced;
}

static unsigned int
gen_sliced_625			(vbi3_capture_sim *	sim)
{
	vbi3_sliced *s;
	vbi3_sliced *end;
	vbi3_bool success;
	unsigned int i;

	s = sim->sliced;
	end = &sim->sliced[N_ELEMENTS (sim->sliced)];

	assert (N_ELEMENTS (sim->sliced) >= 5);

	if (0) {
		for (i = 0; i < N_ELEMENTS (s->data); ++i)
			s->data[i] = rand ();

		s[1] = s[0];
		s[2] = s[0];

		s[0].id = VBI3_SLICED_TELETEXT_A;
		s[0].line = 6;
		s[1].id = VBI3_SLICED_TELETEXT_C_625;
		s[1].line = 7;
		s[2].id = VBI3_SLICED_TELETEXT_D_625;
		s[2].line = 8;
		
		s += 3;
	}

	gen_teletext_b (sim, &s, end - 3, /* field */ 0);

	s->id = VBI3_SLICED_VPS;
	s->line = 16;
	success = vbi3_encode_vps_pdc (s->data, &sim->vps_pid);
	assert (success);
	++s;

	gen_caption (sim, &s, VBI3_SLICED_CAPTION_625, 22);

	s->id = VBI3_SLICED_WSS_625;
	s->line = 23;
	success = vbi3_encode_wss_625 (s->data, &sim->wss_aspect);
	assert (success);
	++s;

	gen_teletext_b (sim, &s, end, /* field */ 1);

	return s - sim->sliced;
}

static void
copy_field			(uint8_t *		dst,
				 const uint8_t *	src,
				 unsigned int		height,
				 unsigned long		bytes_per_line)
{
	while (height-- > 0) {
		memcpy (dst, src, bytes_per_line);

		dst += bytes_per_line;
		src += bytes_per_line * 2;
	}
}

static void
delay_raw_data			(vbi3_capture_sim *	sim,
				 uint8_t *		raw_data)
{
	unsigned int i;

	/* Delay the raw VBI data by one field. */

	i = sim->desync_i;

	if (sim->sp.interlaced) {
		assert (sim->sp.count[0] == sim->sp.count[1]);

		copy_field (sim->desync_buffer[i ^ 1],
			    raw_data + sim->sp.bytes_per_line,
			    sim->sp.count[0], sim->sp.bytes_per_line);
			    
		copy_field (raw_data + sim->sp.bytes_per_line,
			    raw_data,
			    sim->sp.count[0], sim->sp.bytes_per_line);

		copy_field (raw_data, sim->desync_buffer[i],
			    sim->sp.count[0], sim->sp.bytes_per_line);
	} else {
		memcpy (sim->desync_buffer[i ^ 1],
			raw_data + sim->raw_f1_size,
			sim->raw_f2_size);

		memmove (raw_data + sim->raw_f2_size,
			 raw_data,
			 sim->raw_f1_size);

		memcpy (raw_data,
			sim->desync_buffer[i],
			sim->raw_f2_size);
	}

	sim->desync_i = i ^ 1;
}

static vbi3_bool
sim_read			(vbi3_capture *		cap,
				 vbi3_capture_buffer **	raw,
				 vbi3_capture_buffer **	sliced,
				 struct timeval *	timeout)
{
	vbi3_capture_sim *sim = PARENT (cap, vbi3_capture_sim, cap);
	unsigned int n_lines;

	timeout = timeout;

	n_lines = 0;

	if (NULL != raw
	    || NULL != sliced) {
		if (VBI3_VIDEOSTD_SET_525_60 & sim->sp.videostd_set) {
			n_lines = gen_sliced_525 (sim);
		} else {
			n_lines = gen_sliced_625 (sim);
		}
	}

	if (NULL != raw) {
		uint8_t *raw_data;

		if (NULL == *raw) {
			/* Return our buffer. */
			*raw = &sim->raw_buffer;
			raw_data = sim->raw_buffer.data;
		} else {
			/* XXX check max size here, after the API required
			   clients to pass one. */
			raw_data = (*raw)->data;
			(*raw)->size = sim->raw_buffer.size;
		}

		(*raw)->timestamp = sim->capture_time;

		memset (raw_data, 0x80, sim->raw_buffer.size);

		_vbi3_test_image_vbi (raw_data,
				      sim->raw_buffer.size,
				      &sim->sp,
				      sim->sliced, n_lines);

		if (!sim->sp.synchronous)
			delay_raw_data (sim, raw_data);

		if (0) {
			/* Decode the simulated raw VBI data to test our
			   encoder & decoder. */

			memset (sim->sliced, 0xAA, sizeof (sim->sliced));

			n_lines = vbi3_raw_decoder_decode
				(sim->rd,
				 sim->sliced, sizeof (sim->sliced),
				 raw_data);
		}
	}

	if (NULL != sliced) {
		if (NULL == *sliced) {
			/* Return our buffer. */
			*sliced = &sim->sliced_buffer;
		} else {
			/* XXX check max size here, after the API required
			   clients to pass one. */
			memcpy ((*sliced)->data, sim->sliced,
				n_lines * sizeof (sim->sliced[0]));
		}

		(*sliced)->size = n_lines * sizeof (sim->sliced[0]);
		(*sliced)->timestamp = sim->capture_time;
	}

	if (VBI3_VIDEOSTD_SET_525_60 & sim->sp.videostd_set) {
		sim->capture_time += 1001 / 30000.0;
	} else {
		sim->capture_time += 1 / 25.0;
	}

	return TRUE;
}

static vbi3_raw_decoder *
sim_parameters			(vbi3_capture *		cap)
{
	vbi3_capture_sim *sim = PARENT (cap, vbi3_capture_sim, cap);

	return sim->rd;
}

static int
sim_get_fd			(vbi3_capture *		cap)
{
	cap = cap;

	return -1; /* not available */
}

static void
sim_delete			(vbi3_capture *		cap)
{
	vbi3_capture_sim *sim = PARENT (cap, vbi3_capture_sim, cap);

	vbi3_raw_decoder_delete (sim->rd);

	free (sim->desync_buffer[1]);
	free (sim->desync_buffer[0]);

	free (sim->raw_buffer.data);

	CLEAR (*sim);

	free (sim);
}

vbi3_capture *
_vbi3_capture_sim_new		(int			scanning,
				 unsigned int *		services,
				 vbi3_bool		interlaced,
				 vbi3_bool		synchronous)
{
	vbi3_capture_sim *sim;
	vbi3_videostd_set videostd_set;

	sim = calloc (1, sizeof (*sim));
	if (NULL == sim) {
		// _vbi3_asprintf (errstr, _("Out of memory."));
		errno = ENOMEM;
		return NULL;
	}

	sim->cap.read		= sim_read;
	sim->cap.parameters	= sim_parameters;
	sim->cap.get_fd		= sim_get_fd;
	sim->cap._delete	= sim_delete;

	sim->capture_time = 0.0;

        switch (scanning) {
        case 525:
		videostd_set = VBI3_VIDEOSTD_SET_525_60;
                break;
        case 625:
                videostd_set = VBI3_VIDEOSTD_SET_625_50;
                break;
        default:
		assert (0);
        }

	/* Sampling parameters. */

	*services = vbi3_sampling_par_from_services
		(&sim->sp, /* return max_rate */ NULL,
		 videostd_set, *services,
		 /* log_fn */ NULL, /* log_user_data */ NULL);
	if (0 == *services)
		goto failure;

	sim->sp.interlaced = interlaced;
	sim->sp.synchronous = synchronous;

	/* Raw VBI buffer. */

	sim->raw_f1_size = sim->sp.bytes_per_line * sim->sp.count[0];
	sim->raw_f2_size = sim->sp.bytes_per_line * sim->sp.count[1];

	sim->raw_buffer.size = sim->raw_f1_size + sim->raw_f2_size;
	sim->raw_buffer.data = malloc (sim->raw_buffer.size);
	if (NULL == sim->raw_buffer.data)
		goto failure;

	if (!synchronous) {
		size_t size;

		size = sim->sp.bytes_per_line * sim->sp.count[1];

		sim->desync_buffer[0] = calloc (1, size);
		sim->desync_buffer[1] = calloc (1, size);

		if (NULL == sim->desync_buffer[0]
		    || NULL == sim->desync_buffer[1])
			goto failure;
	}

	/* Sliced VBI buffer. */

	sim->sliced_buffer.data = sim->sliced;
	sim->sliced_buffer.size = sizeof (sim->sliced);

	/* Raw VBI decoder. */

	sim->rd = vbi3_raw_decoder_new (&sim->sp);
	if (0 == sim->rd)
		goto failure;

	vbi3_raw_decoder_add_services (sim->rd, *services, 0);

	/* Signal simulation. */
	
	CLEAR (sim->vps_pid);

	sim->vps_pid.cni_type	= VBI3_CNI_TYPE_VPS;
	sim->vps_pid.channel	= VBI3_PID_CHANNEL_VPS;
	sim->vps_pid.pil	= VBI3_PIL_TIMER_CONTROL;

	CLEAR (sim->wss_aspect);

	return &sim->cap;

 failure:
	sim_delete (&sim->cap);

	return NULL;
}
