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

/* $Id: io-sim.c,v 1.1.2.11 2006-05-18 16:49:19 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

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
	double D = 1.0 / bit_rate;
	double t0 = 10.5e-6; /* CRI start half amplitude */
	double t1 = t0 + .25 * D; /* CRI start, blanking level */
	double t2 = t1 + 7 * D; /* CRI 7 cycles */
	double t3 = t0 + 7 * D; /* first start bit left edge half amplitude */
	double q1 = PI * bit_rate * 2;
	double q2 = PI / .120e-6; /* rise/fall time 240 ns */
	double signal_mean = (white_level - blank_level) * .25; /* 25 IRE */
	double signal_high = blank_level + (white_level - blank_level) * .5;
	double sample_period = 1.0 / sp->sampling_rate;
	double t;
	unsigned int data;
	unsigned int i;

	/* Twice 7 data + odd parity, start bit 0 -> 1 */

	data = (sliced->data[1] << 12) + (sliced->data[0] << 4) + 8;

	t = sp->offset / (double) sp->sampling_rate;

	for (i = 0; i < sp->samples_per_line; ++i) {
		if (t >= t1 && t < t2) {
			raw[i] = blank_level
				+ (1.0 - cos (q1 * (t - t1)))
				* signal_mean;
		} else {
			unsigned int bit;
			unsigned int seq;
			double d;

			d = t - t3;
			bit = d * bit_rate;
			seq = (data >> bit) & 3;

			d -= bit * D;
			if ((1 == seq || 2 == seq)
			    && fabs (d) < .120e-6) {
				if (1 == seq)
					raw[i] = blank_level
						+ (1.0 + cos (q2 * d))
						* signal_mean;
				else
					raw[i] = blank_level
						+ (1.0 - cos (q2 * d))
						* signal_mean;
			} else if (data & (2 << bit)) {
				raw[i] = signal_high;
			} else {
				raw[i] = blank_level;
			}
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
		} else if (0 != sp->start[1]
			   && sliced->line >= sp->start[1]) {
			row = sliced->line - sp->start[1];

			if (row >= sp->count[1])
				goto bounds;

			if (sp->interlaced)
				row = row * 2 + 1;
			else
				row += sp->count[0];
		} else if (0 != sp->start[0]
			   && sliced->line >= sp->start[0]) {
			row = sliced->line - sp->start[0];

			if (row >= sp->count[0])
				goto bounds;

			if (sp->interlaced)
				row *= 2;
		} else {
		bounds:
			warning (NULL, 
				 "Sliced line %u out of bounds.",
				 sliced->line);
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
					       30000 * 525 * 32 / 1001, raw1, sliced);
			break;

		default:
			warning (NULL, "Service 0x%08x (%s) not supported.",
				 sliced->id,
				 vbi3_sliced_name (sliced->id));
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

	if (!_vbi3_sampling_par_valid_log (sp, /* log_hook */ NULL))
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

	if (!_vbi3_sampling_par_valid_log (sp, /* log_hook */ NULL))
		return FALSE;

	scan_lines = sp->count[0] + sp->count[1];

	if (scan_lines * sp->bytes_per_line > raw_size) {
		warning (NULL, 
			 "scan_lines %u * bytes_per_line %lu > raw_size %lu.",
			 scan_lines, sp->bytes_per_line, raw_size);
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
	buf = vbi3_malloc (size);
	if (NULL == buf) {
		error (NULL, "Out of memory.");
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

#define MAGIC 0xd804289c

struct buffer {
	char *			data;
	unsigned int		size;
	unsigned int		capacity;
};

typedef struct {
	vbi3_capture		cap;

	unsigned int		magic;

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

	struct buffer		caption_buffers[2];
	unsigned int		caption_i;

	uint8_t			vps_buffer[13];

	uint8_t			wss_buffer[2];
} vbi3_capture_sim;

static vbi3_bool
extend_buffer			(struct buffer *	b,
				 unsigned int		new_capacity)
{
	char *new_data;

	new_data = vbi3_realloc (b->data, new_capacity);
	if (NULL == new_data)
		return FALSE;

	b->data = new_data;
	b->capacity = new_capacity;

	return TRUE;
}

static const char
caption_default_test_stream [] =
	"<erase-displayed ch=\"0\"/><roll-up rows=\"4\"/><pac row=\"14\"/>"
	"LIBZVBI CAPTION SIMULATION CC1.<cr/>"
	"<erase-displayed ch=\"1\"/><roll-up rows=\"4\"/><pac row=\"14\"/>"
	"LIBZVBI CAPTION SIMULATION CC2.<cr/>"
	"<erase-displayed ch=\"2\"/><roll-up rows=\"4\"/><pac row=\"14\"/>"
	"LIBZVBI CAPTION SIMULATION CC3.<cr/>"
	"<erase-displayed ch=\"3\"/><roll-up rows=\"4\"/><pac row=\"14\"/>"
	"LIBZVBI CAPTION SIMULATION CC4.<cr/>"
;
	/* TODO: regression test for repeated control code bug:
	   <control code field 1>
	   <zero field 2>
	   <repeated control code field 1, to be ignored> */ 

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
		int delta;

		if (!isalpha (*s))
			continue;

		delta = strncmp (s, name, len);
		if (0 == delta) {
			s += len;
		} else {
			while (isalnum (*s))
				++s;
		}

		while (isspace (*s++))
			;

		if ('=' != s[-1] || '"' != *s)
			break;

		if (0 == delta) {
			value = strtoul (s + 1,
					 /* endp */ NULL,
					 /* base */ 0);
			break;
		}

		do ++s;
		while (0 != *s && '"' != *s);
	}

	value = SATURATE (value,
			  (unsigned long) minimum,
			  (unsigned long) maximum);

	return (unsigned int) value;
}

static vbi3_bool
caption_append_zeroes		(vbi3_capture_sim *	sim,
				 unsigned int		channel,
				 unsigned int		n_bytes)
{
	struct buffer *b;

	b = &sim->caption_buffers[(channel >> 1) & 1];

	if (b->size + n_bytes > b->capacity) {
		unsigned int new_capacity;

		new_capacity = b->capacity + ((n_bytes + 255) & ~255);
		if (!extend_buffer (b, new_capacity))
			return FALSE;
	}

	memset (b->data + b->size, 0x80, n_bytes);
	b->size += n_bytes;

	return TRUE;
}

static unsigned int
caption_append_command		(vbi3_capture_sim *	sim,
				 unsigned int *		inout_ch,
				 const char *		s)
{
	static const _vbi3_key_value_pair elements [] = {
		{ "aof", 0x1422 },
		{ "aon", 0x1423 },
		{ "backgr", 0x1020 },
		{ "backgr-transp", 0x172D },
		{ "bao", 0x102E },
		{ "bas", 0x102F },
		{ "bbo", 0x1024 },
		{ "bbs", 0x1025 },
		{ "bco", 0x1026 },
		{ "bcs", 0x1027 },
		{ "bgo", 0x1022 },
		{ "bgs", 0x1023 },
		{ "bmo", 0x102C },
		{ "bms", 0x102D },
		{ "bro", 0x1028 },
		{ "brs", 0x1029 },
		{ "bs", 0x1421 },
		{ "bt", 0x172D },
		{ "bwo", 0x1020 },
		{ "bws", 0x1021 },
		{ "byo", 0x102A },
		{ "bys", 0x102B },
		{ "cmd", 0x0001 },
		{ "cr", 0x142D },
		{ "delete-end-of-row", 0x1424 },
		{ "der", 0x1424 },
		{ "edm", 0x142C },
		{ "end-of-caption", 0x142F },
		{ "enm", 0x142E },
		{ "eoc", 0x142F },
		{ "erase-displayed", 0x142C },
		{ "erase-non-displayed", 0x142E },
		{ "extended2", 0x1200 },
		{ "extended3", 0x1300 },
		{ "fa", 0x172E },
		{ "fau", 0x172F },
		{ "flash-on", 0x1428 },
		{ "fon", 0x1428 },
		{ "foregr-black", 0x172E },
		{ "indent", 0x1050 },
		{ "mr", 0x1120 },
		{ "pac", 0x1040 },
		{ "pause", 0x0002 },
		{ "rcl", 0x1420 },
		{ "rdc", 0x1429 },
		{ "resume-caption", 0x1420 },
		{ "resume-direct", 0x1429 },
		{ "resume-text", 0x142B },
		{ "roll-up", 0x1425 },
		{ "rtd", 0x142B },
		{ "ru2", 0x1425 },
		{ "ru3", 0x1426 },
		{ "ru4", 0x1427 },
		{ "special", 0x1130 },
		{ "sync", 0x0003 },
		{ "tab", 0x1720 },
		{ "text-restart", 0x142A },
		{ "to1", 0x1721 },
		{ "to2", 0x1722 },
		{ "to3", 0x1723 },
		{ "tr", 0x142A },
	};
	static const int row_code [16] = {
		0x1140, 0x1160, 0x1240, 0x1260, 
		0x1540, 0x1560, 0x1640, 0x1660, 
		0x1740, 0x1760, 0x1040, 0x1340, 
		0x1360, 0x1440, 0x1460, -1
	};
	struct buffer *b;
	int value;
	unsigned int cmd;
	unsigned int n_frames;
	int n_padding_bytes;
	unsigned int row;
	unsigned int i;
	vbi3_bool parity;

	if (!_vbi3_keyword_lookup (&value, &s,
				   elements,
				   N_ELEMENTS (elements)))
		return TRUE;

	*inout_ch = get_attr (s, "ch", *inout_ch, 0, 3);

	cmd = value | ((*inout_ch & 1) << 11);

	parity = TRUE;

	switch (value) {
	case 1: /* cmd */
		cmd = get_attr (s, "code", 0, 0, 0xFFFF);
		parity = FALSE;
		break;

	case 2: /* pause */
		n_frames = get_attr (s, "frames", 60, 1, INT_MAX);
		if (n_frames > 120 * 60 * 30)
			return TRUE;

		return caption_append_zeroes (sim, *inout_ch, n_frames * 2);

	case 3: /* sync */
		n_padding_bytes = sim->caption_buffers[0].size
			- sim->caption_buffers[1].size;

		if (0 == n_padding_bytes)
			return TRUE;
		else if (n_padding_bytes < 0)
			return caption_append_zeroes (sim, 0, n_padding_bytes);
		else
			return caption_append_zeroes (sim, 2, n_padding_bytes);

	case 0x1020: /* backgr */
		cmd |= get_attr (s, "color", 0, 0, 7) << 1;
		cmd |= get_attr (s, "t", 0, 0, 1); /* transparent */
		break;

	case 0x1040: /* pac (preamble address code) */
		cmd |= row_code[get_attr (s, "row", 14, 0, 14)];
		cmd |= get_attr (s, "color", 0, 0, 7) << 1;
		cmd |= get_attr (s, "u", 0, 0, 1);
		break;

	case 0x1050: /* indent */
		cmd |= row_code[get_attr (s, "row", 14, 0, 14)];
		cmd |= (get_attr (s, "cols", 0, 0, 31) / 4) << 1;
		cmd |= get_attr (s, "u", 0, 0, 1);
		break;

	case 0x1120: /* mr (midrow code) */
		cmd |= get_attr (s, "color", 0, 0, 7) << 1;
		cmd |= get_attr (s, "u", 0, 0, 1);
		break;

	case 0x1130: /* special character */
		cmd |= get_attr (s, "code", 0, 0, 15);
		break;

	case 0x1200: /* extended character set */
	case 0x1300:
		cmd |= get_attr (s, "code", 32, 32, 63);
		break;

	case 0x1420: /* resume caption loading */
	case 0x1421: /* bs */
	case 0x1422: /* aof */
	case 0x1423: /* aon */
	case 0x1424: /* delete to end of row */
	case 0x1428: /* flash-on */
	case 0x1429: /* resume direct caption */
	case 0x142A: /* text restart */
	case 0x142B: /* resume text display */
	case 0x142C: /* erase displayed memory */
	case 0x142D: /* cr */
	case 0x142E: /* erase non-displayed memory */
	case 0x142F: /* end of caption */
		/* Field bit (EIA 608-B Sec. 8.4, 8.5). */
		cmd |= ((*inout_ch & 2) << 7);

	case 0x1425: /* roll_up */
	case 0x1426:
	case 0x1427:
		row = get_attr (s, "rows", 2, 2, 4);
		cmd += row - 2;
		cmd |= (*inout_ch & 2) << 7; /* field bit */
		break;

	case 0x1720: /* tab */
		cmd |= get_attr (s, "cols", 1, 1, 3);
		break;

	case 0x172E: /* foregr-black */
		cmd |= get_attr (s, "u", 0, 0, 1); /* underlined */
		break;

	default:
		break;
	}

	b = &sim->caption_buffers[(*inout_ch >> 1) & 1];
	i = b->size;

	if (i + 3 > b->capacity) {
		if (!extend_buffer (b, b->capacity + 256))
			return FALSE;
	}

	if (i & 1)
		b->data[i++] = 0x80;

	if (likely (parity)) {
		b->data[i] = vbi3_par8 (cmd >> 8);
		b->data[i + 1] = vbi3_par8 (cmd);
	} else {
		/* To test error checks. */
		b->data[i] = cmd >> 8;
		b->data[i + 1] = cmd;
	}

	b->size = i + 2;

	return TRUE;
}

vbi3_bool
_vbi3_capture_sim_load_caption	(vbi3_capture *		cap,
				 const char *		stream,
				 vbi3_bool		append)
{
	vbi3_capture_sim *sim;
	struct buffer *b;
	unsigned int ch;
	const char *s;

	assert (NULL != cap);

	sim = PARENT (cap, vbi3_capture_sim, cap);
	assert (MAGIC == sim->magic);

	if (!append) {
		free (sim->caption_buffers[0].data);
		free (sim->caption_buffers[1].data);

		CLEAR (sim->caption_buffers);

		sim->caption_i = 0;
	}

	if (NULL == stream)
		return TRUE;

	ch = 0;

	b = &sim->caption_buffers[0];

	for (s = stream;;) {
		int c = *s++;

		if (0 == c) {
			break;
		} else if (c < 0x20) {
			continue;
		} else if ('&' == c) {
			if ('#' == *s) {
				char *end;

				c = strtoul (s + 1, &end, 10);
				s = end;

				if (';' == *s)
					++s;
			} else if (0 == strncmp (s, "amp;", 4)) {
				s += 4;
			} else if (0 == strncmp (s, "lt;", 3)) {
				s += 3;
				c = '<';
			} else if (0 == strncmp (s, "gt;", 3)) {
				s += 3;
				c = '>';
			}
		} else if ('<' == c) {
			int delimiter;

			if (!caption_append_command (sim, &ch, s))
				return FALSE;

			b = &sim->caption_buffers[(ch >> 1) & 1];

			/* Skip until '>', except between quotes. */
			delimiter = '>';
			for (; 0 != *s && delimiter != *s; ++s) {
				if ('"' == *s)
					delimiter ^= '>';
			}

			if (0 != *s)
				++s; /* skip delimiter */

			continue;
		}

		if (b->size >= b->capacity) {
			if (!extend_buffer (b, b->capacity + 256))
				return FALSE;
		}

		b->data[b->size++] = vbi3_par8 (c);
	}

	return TRUE;
}

static void
gen_caption			(vbi3_capture_sim *	sim,
				 vbi3_sliced **		inout_sliced,
				 vbi3_service_set	service_set,
				 unsigned int		line)
{
	vbi3_sliced *s;
	struct buffer *b;
	unsigned int i;

	b = &sim->caption_buffers[(line > 200)];

	i = sim->caption_i;

	if (i + 1 < b->size) {
		s = *inout_sliced;
		*inout_sliced = s + 1;

		s->id = service_set;
		s->line = line;
		s->data[0] = b->data[i];
		s->data[1] = b->data[i + 1];
	}
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

	if (sim->caption_buffers[0].size > 0)
		gen_caption (sim, &s, VBI3_SLICED_CAPTION_525, 21);

	if (sim->caption_buffers[1].size > 0)
		gen_caption (sim, &s, VBI3_SLICED_CAPTION_525, 284);

	sim->caption_i += 2;
	if (sim->caption_i >= sim->caption_buffers[0].size
	    && sim->caption_i >= sim->caption_buffers[1].size)
		sim->caption_i = 0;

	return s - sim->sliced;
}

vbi3_bool
_vbi3_capture_sim_load_vps	(vbi3_capture *		cap,
				 const vbi3_program_id *pid)
{
	vbi3_capture_sim *sim;
	vbi3_program_id pid2;

	assert (NULL != cap);

	sim = PARENT (cap, vbi3_capture_sim, cap);
	assert (MAGIC == sim->magic);

	if (NULL == pid) {
		CLEAR (pid2);

		pid2.cni_type	= VBI3_CNI_TYPE_VPS;
		pid2.channel	= VBI3_PID_CHANNEL_VPS;
		pid2.pil	= VBI3_PIL_TIMER_CONTROL;

		pid = &pid2;
	}

	return vbi3_encode_vps_pdc (sim->vps_buffer, pid);
}

vbi3_bool
_vbi3_capture_sim_load_wss_625	(vbi3_capture *		cap,
				 const vbi3_aspect_ratio *ar)
{
	vbi3_capture_sim *sim;
	vbi3_aspect_ratio ar2;

	assert (NULL != cap);

	sim = PARENT (cap, vbi3_capture_sim, cap);
	assert (MAGIC == sim->magic);

	if (NULL == ar) {
		CLEAR (ar2);
		ar = &ar2;
	}

	return vbi3_encode_wss_625 (sim->wss_buffer, ar);
}

static unsigned int
gen_sliced_625			(vbi3_capture_sim *	sim)
{
	vbi3_sliced *s;
	vbi3_sliced *end;
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
	assert (sizeof (s->data) >= sizeof (sim->vps_buffer));
	memcpy (s->data, sim->vps_buffer, sizeof (sim->vps_buffer));
	++s;

	if (sim->caption_buffers[0].size > 0)
		gen_caption (sim, &s, VBI3_SLICED_CAPTION_625, 22);

	sim->caption_i += 2;
	if (sim->caption_i >= sim->caption_buffers[0].size)
		sim->caption_i = 0;

	s->id = VBI3_SLICED_WSS_625;
	s->line = 23;
	assert (sizeof (s->data) >= sizeof (sim->wss_buffer));
	memcpy (s->data, sim->wss_buffer, sizeof (sim->wss_buffer));
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

#warning
		if (1) {
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

	_vbi3_capture_sim_load_caption (cap,
					/* test_stream */ NULL,
					/* append */ FALSE);

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
	vbi3_bool success;

	sim = calloc (1, sizeof (*sim));
	if (NULL == sim) {
		errno = ENOMEM;
		return NULL;
	}

	sim->magic = MAGIC;

	sim->cap.read		= sim_read;
	sim->cap.parameters	= sim_parameters;
	sim->cap.get_fd		= sim_get_fd;
	sim->cap._delete	= sim_delete;

	sim->capture_time = 0.0;

	videostd_set = _vbi3_videostd_set_from_scanning (scanning);
	assert (VBI3_VIDEOSTD_SET_EMPTY != videostd_set);

	/* Sampling parameters. */

	*services = vbi3_sampling_par_from_services
		(&sim->sp, /* return max_rate */ NULL,
		 videostd_set, *services);
	if (0 == *services) {
		goto failure;
	}

	sim->sp.interlaced = interlaced;
	sim->sp.synchronous = synchronous;

	/* Raw VBI buffer. */

	sim->raw_f1_size = sim->sp.bytes_per_line * sim->sp.count[0];
	sim->raw_f2_size = sim->sp.bytes_per_line * sim->sp.count[1];

	sim->raw_buffer.size = sim->raw_f1_size + sim->raw_f2_size;
	sim->raw_buffer.data = malloc (sim->raw_buffer.size);
	if (NULL == sim->raw_buffer.data) {
		goto failure;
	}

	if (!synchronous) {
		size_t size;

		size = sim->sp.bytes_per_line * sim->sp.count[1];

		sim->desync_buffer[0] = calloc (1, size);
		sim->desync_buffer[1] = calloc (1, size);

		if (NULL == sim->desync_buffer[0]
		    || NULL == sim->desync_buffer[1]) {
			goto failure;
		}
	}

	/* Sliced VBI buffer. */

	sim->sliced_buffer.data = sim->sliced;
	sim->sliced_buffer.size = sizeof (sim->sliced);

	/* Raw VBI decoder. */

	sim->rd = vbi3_raw_decoder_new (&sim->sp);
	if (0 == sim->rd) {
		goto failure;
	}

	vbi3_raw_decoder_add_services (sim->rd, *services, 0);

	/* Signal simulation. */
	
	success = _vbi3_capture_sim_load_vps (&sim->cap, NULL);
	assert (success);

	success = _vbi3_capture_sim_load_wss_625 (&sim->cap, NULL);
	assert (success);

	success = _vbi3_capture_sim_load_caption
		(&sim->cap, caption_default_test_stream,
		 /* append */ FALSE);
	if (!success) {
		goto failure;
	}

	return &sim->cap;

 failure:
	sim_delete (&sim->cap);

	return NULL;
}
