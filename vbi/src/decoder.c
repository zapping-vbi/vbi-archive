/*
 *  libzvbi - Raw vbi decoder
 *
 *  Copyright (C) 2000-2003 Michael H. Schimek
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

/* $Id: decoder.c,v 1.7.2.4 2003-09-24 18:49:56 mschimek Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

#include "misc.h"
#include "decoder.h"

/**
 * @addtogroup Slicer Bit Slicer
 * @ingroup Raw
 * @brief Function converting a single scan line of raw VBI
 *   data to sliced VBI data.
 *
 * This is a low level function most useful if you want to decode
 * data services not covered by libzvbi. Usually you will want to
 * use the Raw VBI decoder, converting several lines of different
 * data services at once.
 */

/* This is time critical, tinker with care. */

#define OVERSAMPLING 4		/* 1, 2, 4, 8 */
#define THRESH_FRAC 9

#ifdef __i386__
/* Common cpu type, little endian, shorts need not align.
   Let's save a few cycles. */
#define GREEN(raw, fmt)							\
	((fmt == VBI_PIXFMT_RGB16_LE) ?					\
	 *(const uint16_t *)(raw) & d->green_mask			\
	 : ((fmt == VBI_PIXFMT_RGB16_BE) ?				\
	    ((raw)[0] * 256 + (raw)[1]) & d->green_mask 		\
	    : (raw)[0]))
#else
#define GREEN(raw, fmt)							\
	((fmt == VBI_PIXFMT_RGB16_LE) ?					\
	 ((raw)[0] + (raw)[1] * 256) & d->green_mask			\
	 : ((fmt == VBI_PIXFMT_RGB16_BE) ?				\
	    ((raw)[0] * 256 + (raw)[1])) & d->green_mask		\
	    : (raw)[0]))
#endif

#define SAMPLE(fmt)							\
	r = raw + (i >> 8) * bpp;					\
	raw0 = GREEN (r + 0, fmt);					\
	raw1 = GREEN (r + bpp, fmt);					\
	raw0 = (int)(raw1 - raw0) * (i & 255) + (raw0 << 8);

#define BIT_SLICER(fmt)							\
static vbi_bool								\
bit_slicer_##fmt		(vbi_bit_slicer *	d,		\
				 uint8_t *		buf,		\
				 const uint8_t *	raw)		\
{									\
	const unsigned int bpp = VBI_PIXFMT_BPP (VBI_PIXFMT_##fmt);	\
	const uint8_t *r;						\
	unsigned int i, j, k;						\
	unsigned int cl = 0;						\
	unsigned int thresh0, tr;					\
	unsigned int c = 0, t;						\
	unsigned int raw0, raw1;					\
	unsigned char b, b1 = 0;					\
									\
	thresh0 = d->thresh;						\
	raw += d->skip;							\
						                        \
	for (i = d->cri_bytes; i > 0; raw += bpp, --i) {		\
		tr = d->thresh >> THRESH_FRAC;				\
									\
		raw0 = GREEN (raw, VBI_PIXFMT_##fmt);			\
		raw1 = GREEN (raw + bpp, VBI_PIXFMT_##fmt) - raw0;	\
		d->thresh += (int)(raw0 - tr) * (int) ABS ((int) raw1);	\
		t = raw0 * OVERSAMPLING;				\
								        \
		for (j = OVERSAMPLING; j > 0; --j) {			\
			b = ((t + (OVERSAMPLING / 2))			\
			     / OVERSAMPLING) >= tr;			\
									\
    			if (__builtin_expect (b ^ b1, 0)) {		\
				cl = d->oversampling_rate >> 1;		\
			} else {					\
				cl += d->cri_rate;			\
									\
				if (cl >= d->oversampling_rate) {	\
					cl -= d->oversampling_rate;	\
					c = c * 2 + b;			\
					if ((c & d->cri_mask)		\
					    == d->cri)			\
						goto payload;		\
				}					\
			}						\
									\
			b1 = b;						\
									\
			if (OVERSAMPLING > 1)				\
				t += raw1;				\
		}							\
	}								\
									\
	d->thresh = thresh0;						\
									\
	return FALSE;							\
									\
payload:								\
	i = d->phase_shift;						\
	tr *= 256;							\
	c = 0;								\
									\
	for (j = d->frc_bits; j > 0; --j) {				\
		SAMPLE (VBI_PIXFMT_##fmt);				\
		c = c * 2 + (raw0 >= tr);				\
		i += d->step;						\
	}								\
									\
	if (c != d->frc)						\
		return FALSE;						\
									\
	switch (d->endian) {						\
	case 3: /* bitwise, lsb first */				\
		for (j = 0; j < d->payload; ++j) {			\
			SAMPLE (VBI_PIXFMT_##fmt);			\
			c = (c >> 1) + ((raw0 >= tr) << 7);		\
			i += d->step;					\
			if ((j & 7) == 7)				\
				*buf++ = c;				\
		}							\
		*buf = c >> ((8 - d->payload) & 7);			\
		break;							\
									\
	case 2: /* bitwise, msb first */				\
		for (j = 0; j < d->payload; ++j) {			\
			SAMPLE (VBI_PIXFMT_##fmt);			\
			c = c * 2 + (raw0 >= tr);			\
			i += d->step;					\
			if ((j & 7) == 7)				\
				*buf++ = c;				\
		}							\
		*buf = c & ((1 << (d->payload & 7)) - 1);		\
		break;							\
									\
	case 1: /* octets, lsb first */					\
		for (j = d->payload; j > 0; --j) {			\
			for (k = 0, c = 0; k < 8; ++k) {		\
				SAMPLE (VBI_PIXFMT_##fmt);		\
				c += (raw0 >= tr) << k;			\
				i += d->step;				\
			}						\
			*buf++ = c;					\
		}							\
		break;							\
									\
	default: /* octets, msb first */				\
		for (j = d->payload; j > 0; --j) {			\
			for (k = 0; k < 8; ++k) {			\
				SAMPLE (VBI_PIXFMT_##fmt);		\
				c = c * 2 + (raw0 >= tr);		\
				i += d->step;				\
			}						\
			*buf++ = c;					\
		}							\
		break;							\
	}								\
									\
	return TRUE;							\
}

BIT_SLICER (YUV420)
BIT_SLICER (YUYV)
BIT_SLICER (RGBA32_LE)
BIT_SLICER (RGB24)
BIT_SLICER (RGB16_LE)
BIT_SLICER (RGB16_BE)

/**
 * @param slicer Pointer to vbi_bit_slicer object allocated with
 *   vbi_bit_slicer_new().
 * @param buf Output data. The buffer must be large enough to store
 *   the number of bits given as @a payload to vbi_bit_slicer_init().
 * @param raw Input data. At least the number of pixels or samples
 *  given as @a samples_per_line to vbi_bit_slicer_new().
 * 
 * Decodes one scan line of raw vbi data. Note the bit slicer tries
 * to adapt to the average signal amplitude, you should avoid
 * using the same vbi_bit_slicer object for data from different
 * devices.
 *
 * Unlike the higher level decoders, as a matter of speed this function
 * does not lock the @a slicer. When you want to share a vbi_bit_slicer
 * object between multiple threads you must implement your own locking
 * mechanism.
 * 
 * @return
 * @c FALSE if the raw data does not contain the expected
 * information, i. e. the CRI/FRC has not been found. This may also
 * result from a too weak or noisy signal. Error correction must be
 * implemented at a higher layer.
 */
vbi_bool
vbi_bit_slice			(vbi_bit_slicer *	slicer,
				 uint8_t *		buf,
				 const uint8_t *	raw)
{
	return slicer->func (slicer, buf, raw);
}

/**
 * @internal
 *
 * See vbi_bit_slicer_new().
 */
void
vbi_bit_slicer_init		(vbi_bit_slicer *	slicer,
				 vbi_pixfmt		sample_format,
				 unsigned int		sampling_rate,
				 unsigned int		samples_per_line,
				 unsigned int		cri,
				 unsigned int		cri_mask,
				 unsigned int		cri_bits,
				 unsigned int		cri_rate,
				 unsigned int		frc,
				 unsigned int		frc_bits,
				 unsigned int		payload_bits,
				 unsigned int		payload_rate,
				 vbi_modulation		modulation)
{
	unsigned int c_mask;
	unsigned int f_mask;
	unsigned int green_shift;

	assert (cri_bits <= 32);
	assert (frc_bits <= 32);

	c_mask = (cri_bits == 32) ? ~0 : (1 << cri_bits) - 1;
	f_mask = (frc_bits == 32) ? ~0 : (1 << frc_bits) - 1;

	switch (sample_format) {
	case VBI_PIXFMT_RGB24:
	case VBI_PIXFMT_BGR24:
	        slicer->func = bit_slicer_RGB24;
		green_shift = 8;
		break;

	case VBI_PIXFMT_RGBA32_LE:
	case VBI_PIXFMT_BGRA32_LE:
		slicer->func = bit_slicer_RGBA32_LE;
		green_shift = 8;
		break;

	case VBI_PIXFMT_RGBA32_BE:
	case VBI_PIXFMT_BGRA32_BE:
		slicer->func = bit_slicer_RGBA32_LE;
		green_shift = 16;
		break;

	case VBI_PIXFMT_RGB16_LE:
	case VBI_PIXFMT_BGR16_LE:
		slicer->func = bit_slicer_RGB16_LE;
		slicer->green_mask = 0x07E0;
		green_shift = 5;
		break;

	case VBI_PIXFMT_RGBA15_LE:
	case VBI_PIXFMT_BGRA15_LE:
		slicer->func = bit_slicer_RGB16_LE;
		slicer->green_mask = 0x03E0;
		green_shift = 5;
		break;

	case VBI_PIXFMT_ARGB15_LE:
	case VBI_PIXFMT_ABGR15_LE:
		slicer->func = bit_slicer_RGB16_LE;
		slicer->green_mask = 0x07C0;
		green_shift = 6;
		break;

	case VBI_PIXFMT_RGB16_BE:
	case VBI_PIXFMT_BGR16_BE:
		slicer->func = bit_slicer_RGB16_BE;
		slicer->green_mask = 0x07E0;
		green_shift = 5;
		break;

	case VBI_PIXFMT_RGBA15_BE:
	case VBI_PIXFMT_BGRA15_BE:
		slicer->func = bit_slicer_RGB16_BE;
		slicer->green_mask = 0x03E0;
		green_shift = 5;
		break;

	case VBI_PIXFMT_ARGB15_BE:
	case VBI_PIXFMT_ABGR15_BE:
		slicer->func = bit_slicer_RGB16_BE;
		slicer->green_mask = 0x07C0;
		green_shift = 6;
		break;

	case VBI_PIXFMT_YUV420:
		slicer->func = bit_slicer_YUV420;
		green_shift = 0;
		break;

	case VBI_PIXFMT_YUYV:
	case VBI_PIXFMT_YVYU:
		slicer->func = bit_slicer_YUYV;
		green_shift = 0;
		break;

	case VBI_PIXFMT_UYVY:
	case VBI_PIXFMT_VYUY:
		slicer->func = bit_slicer_YUYV;
		green_shift = 8;
		break;

	default:
		fprintf (stderr, "%s:%d:vbi_bit_slicer_init: unknown pixfmt %d\n",
			 __FILE__, __LINE__, sample_format);
		exit (EXIT_FAILURE);
	}

	slicer->skip			= green_shift >> 3;

	slicer->cri_mask		= cri_mask & c_mask;
	slicer->cri		 	= cri & slicer->cri_mask;
	/* We stop searching for CRI when the payload
	   cannot possibly fit anymore. */
	slicer->cri_bytes		= samples_per_line
		- ((sampling_rate * (long long)(payload_bits + frc_bits))
		   / payload_rate);
	slicer->cri_rate		= cri_rate;
	/* Raw vbi data is oversampled to account for low sampling rates. */
	slicer->oversampling_rate	= sampling_rate * OVERSAMPLING;
	/* 0/1 threshold, starting value */
	slicer->thresh			= 105 << (THRESH_FRAC + green_shift);
	slicer->frc			= frc & f_mask;
	slicer->frc_bits		= frc_bits;
	/* Payload bit distance in 1/256 raw samples. */
	slicer->step			= sampling_rate * 256.0 / payload_rate;

	if (payload_bits & 7) {
		slicer->payload	= payload_bits;
		slicer->endian	= 3;
	} else {
		/* Endian 0/1 optimized for octets. */
		slicer->payload	= payload_bits >> 3;
		slicer->endian	= 1;
	}

	switch (modulation) {
	case VBI_MODULATION_NRZ_MSB:
		slicer->endian--;
	case VBI_MODULATION_NRZ_LSB:
		slicer->phase_shift = (int)
			(sampling_rate * 256.0 / cri_rate * .5
			 + sampling_rate * 256.0 / payload_rate * .5 + 128);
		break;

	case VBI_MODULATION_BIPHASE_MSB:
		slicer->endian--;
	case VBI_MODULATION_BIPHASE_LSB:
		/* Phase shift between the NRZ modulated CRI and the
		   biphase modulated rest */
		slicer->phase_shift = (int)
			(sampling_rate * 256.0 / cri_rate * .5
			 + sampling_rate * 256.0 / payload_rate * .25 + 128);
		break;
	}
}

/**
 * @param slicer Pointer to a vbi_bit_slicer object allocated with
 *   vbi_bit_slicer_new().
 *
 * Deletes a vbi_bit_slicer object.
 */
void
vbi_bit_slicer_delete		(vbi_bit_slicer *	slicer)
{
	free (slicer);
}

/**
 * @param sample_format Format of the raw data, see vbi_pixfmt.
 * @param sampling_rate Raw vbi sampling rate in Hz, that is the number
 *   of samples or pixels sampled per second by the hardware.
 * @param samples_per_line Number of samples or pixels in one raw vbi
 *   line later passed to vbi_bit_slice(). This limits the number of
 *   bytes read from the raw data buffer. Do not to confuse the value
 *   with bytes per line.
 * @param cri The Clock Run In is a NRZ modulated sequence of '0'
 *   and '1' bits prepending most data transmissions to synchronize data
 *   acquisition circuits. The bit slicer compares the bits in this
 *   word, lsb last transmitted, against the transmitted CRI. Decoding
 *   of payload starts with the next bit after a CRI and FRC match.
 * @param cri_mask Of the CRI bits in @a cri, only these bits are
 *   significant for a match. For instance it is wise not to rely on the
 *   very first CRI bits transmitted.
 * @param cri_bits Number of CRI bits, must not exceed 32.
 * @param cri_rate CRI bit rate in Hz, the number of CRI bits
 *   transmitted per second.
 * @param frc The FRaming Code usually following the CRI is a bit
 *   sequence identifying the data service. There is no mask parameter,
 *   all bits must match. Unlike CRI we assume FRC has @a modulation
 *   of the payload at @a payload_rate. Often such a distinction is not
 *   necessary and FRC bits should be added to CRI, with @a frc_bits
 *   set to zero.
 * @param frc_bits Number of FRC bits, must not exceed 32.
 * @param payload_bits Number of payload bits. Only this data
 *   will be stored in the vbi_bit_slice() output. If this number
 *   is no multiple of eight, the most significant bits of the
 *   last byte are undefined.
 * @param payload_rate Payload bit rate in Hz, the number of payload
 *   bits transmitted per second.
 * @param modulation Modulation of the payload, see vbi_modulation.
 * 
 * Allocates and initializes a vbi_bit_slicer object for use with
 * vbi_bit_slice(). This is a low level function, see also
 * vbi_raw_decode().
 *
 * @returns
 * NULL when out of memory, otherwise a pointer to an opaque
 * vbi_bit_slicer object, which must be deleted with
 * vbi_bit_slicer_delete() when done.
 */
vbi_bit_slicer *
vbi_bit_slicer_new		(vbi_pixfmt		sample_format,
				 unsigned int		sampling_rate,
				 unsigned int		samples_per_line,
				 unsigned int		cri,
				 unsigned int		cri_mask,
				 unsigned int		cri_bits,
				 unsigned int		cri_rate,
				 unsigned int		frc,
				 unsigned int		frc_bits,
				 unsigned int		payload_bits,
				 unsigned int		payload_rate,
				 vbi_modulation		modulation)
{
	vbi_bit_slicer *slicer;

	if (!(slicer = malloc (sizeof (*slicer))))
		return NULL;

        vbi_bit_slicer_init (slicer,
			     sample_format, sampling_rate, samples_per_line,
			     cri, cri_mask, cri_bits, cri_rate,
			     frc, frc_bits,
			     payload_bits, payload_rate, modulation);
	return slicer;	
}

/* ------------------------------------------------------------------------- */

/**
 * @addtogroup Rawdec Raw VBI decoder
 * @ingroup Raw
 * @brief Functions to convert a raw VBI image to sliced VBI data.
 *
 * Blah.
 */

#define MAX_JOBS		8
#define MAX_WAYS		8

#if 0 // future

typedef struct _vbi_raw_decoder vbi_raw_decoder;
typedef struct _vbi_raw_decoder_job vbi_raw_decoder_job;

typedef struct _vbi_service_par vbi_service_par;

struct _vbi_raw_decoder_job {
	vbi_service_par *	service; // XXX was int id
	unsigned int		offset;		/* samples */
	vbi_bit_slicer		slicer;
};

struct vbi_raw_decoder {
	pthread_mutex_t		mutex;

	vbi_sampling_parameters	param;

	unsigned int		services;
	unsigned int		num_jobs;

	int8_t *		pattern;

	vbi_raw_decoder_job	jobs[MAX_JOBS];
};

/*
 *  Data Service Table
 */

struct vbi_service_par {
	unsigned int		id;		/* VBI_SLICED_ */
	const char *		label;

	/* Most scan lines used by the data service, first and last
	   line of first and second field. ITU-R numbering scheme.
	   Zero if no data from this field, requires field sync. */
	uint16_t		first[2];
        uint16_t		last[2];

	/* Leading edge hsync to leading edge first CRI one bit,
	   half amplitude points, in nanoseconds. */
	unsigned int		offset;	

	unsigned int		cri_rate;	/* Hz */
	unsigned int		bit_rate;	/* Hz */

	/* Scanning system: 525 (FV = 59.94 Hz, FH = 15734 Hz),
			    625 (FV = 50 Hz, FH = 15625 Hz). */
	uint16_t		scanning;

	/* Clock Run In and FRaming Code, LSB last txed bit of FRC. */
	unsigned int		cri_frc;

	/* CRI bits significant for identification. Note this isn't
	   shifted by frc_bits. */
	unsigned int		cri_mask;

	/* cri_bits at cri_rate, frc_bits at bit_rate. */
	uint8_t			cri_bits;
	uint8_t			frc_bits;

	uint8_t			payload;	/* bits */
	uint8_t			modulation;
};

#endif









/*
 * Data Service Table
 */



struct vbi_service_par {
	unsigned int	id;		/* VBI_SLICED_ */
	const char *	label;
	short		first[2];	/* scanning lines (ITU-R), max. distribution; */
        short		last[2];	/*  zero: no data from this field, requires field sync */
	int		offset;		/* leading edge hsync to leading edge first CRI one bit
					    half amplitude points, nanoseconds */
	int		cri_rate;	/* Hz */
	int		bit_rate;	/* Hz */
	int		scanning;	/* scanning system: 525 (FV = 59.94 Hz, FH = 15734 Hz),
							    625 (FV = 50 Hz, FH = 15625 Hz) */
	unsigned int	cri_frc;	/* Clock Run In and FRaming Code, LSB last txed bit of FRC */
	unsigned int	cri_mask;	/* cri bits significant for identification, */
	uint8_t		cri_bits;
	uint8_t		frc_bits;	/* cri_bits at cri_rate, frc_bits at bit_rate */
	uint16_t	payload;	/* in bits */
	uint8_t		modulation;	/* payload modulation */
};

const struct vbi_service_par
vbi_services[] = {
	{
		VBI_SLICED_TELETEXT_B_L10_625, "Teletext System B Level 1.5, 625",
		{ 7, 320 },
		{ 22, 335 },
		10300, 6937500, 6937500, /* 444 x FH */
		625, 0x00AAAAE4, ~0, 10, 6, 42 * 8, VBI_MODULATION_NRZ_LSB
	}, {
		VBI_SLICED_TELETEXT_B, "Teletext System B, 625",
		{ 6, 318 },
		{ 22, 335 },
		10300, 6937500, 6937500, /* 444 x FH */
		625, 0x00AAAAE4, ~0, 10, 6, 42 * 8, VBI_MODULATION_NRZ_LSB
	}, {
		VBI_SLICED_VPS, "Video Programming System",
		{ 16, 0 },
		{ 16, 0 },
		12500, 5000000, 2500000, /* 160 x FH */
		625, 0xAAAA8A99, ~0, 24, 0, 13 * 8, VBI_MODULATION_BIPHASE_MSB
	}, {
		VBI_SLICED_WSS_625, "Wide Screen Signalling 625",
		{ 23, 0 },
		{ 23, 0 },
		11000, 5000000, 833333, /* 160/3 x FH */
		625, 0xC71E3C1F, 0x924C99CE, 32, 0, 14 * 1, VBI_MODULATION_BIPHASE_LSB
	}, {
		VBI_SLICED_CAPTION_625_F1, "Closed Caption 625, field 1",
		{ 22, 0 },
		{ 22, 0 },
		10500, 1000000, 500000, /* 32 x FH */
		625, 0x00005551, ~0, 9, 2, 2 * 8, VBI_MODULATION_NRZ_LSB
	}, {
		VBI_SLICED_CAPTION_625_F2, "Closed Caption 625, field 2",
		{ 0, 335 },
		{ 0, 335 },
		10500, 1000000, 500000, /* 32 x FH */
		625, 0x00005551, ~0, 9, 2, 2 * 8, VBI_MODULATION_NRZ_LSB
	}, {
		VBI_SLICED_VBI_625, "VBI 625", /* Blank VBI */
		{ 6, 318 },
		{ 22, 335 },
		10000, 1510000, 1510000,
		625, 0, 0, 0, 0, 10 * 8, 0 /* 10.0-2 ... 62.9+1 us */
	}, {
		VBI_SLICED_NABTS, "Teletext System C, 525", /* NOT CONFIRMED */
		{ 10, 0 },
		{ 21, 0 },
		10500, 5727272, 5727272,
		525, 0x00AAAAE7, ~0, 10, 6, 33 * 8, VBI_MODULATION_NRZ_LSB /* Tb. */
	}, {
		VBI_SLICED_WSS_CPR1204, "Wide Screen Signalling 525",
		/* NOT CONFIRMED (EIA-J CPR-1204) */
		{ 20, 283 },
		{ 20, 283 },
		11200, 3579545, 447443, /* 1/8 x FSC */
		525, 0x0000FF00, 0x00003C3C, 16, 0, 20 * 1, VBI_MODULATION_NRZ_MSB
		/* No useful FRC, but a six bit CRC */
	}, {
		VBI_SLICED_CAPTION_525_F1, "Closed Caption 525, field 1",
		{ 21, 0 },
		{ 21, 0 },
		10500, 1006976, 503488, /* 32 x FH */
		525, 0x00005551, ~0, 9, 2, 2 * 8, VBI_MODULATION_NRZ_LSB
	}, {
		VBI_SLICED_CAPTION_525_F2, "Closed Caption 525, field 2",
		{ 0, 284 },
		{ 0, 284 },
		10500, 1006976, 503488, /* 32 x FH */
		525, 0x00005551, ~0, 9, 2, 2 * 8, VBI_MODULATION_NRZ_LSB
	}, {
		VBI_SLICED_2xCAPTION_525, "2xCaption 525", /* NOT CONFIRMED */
		{ 10, 0 },
		{ 21, 0 },
		10500, 1006976, 1006976, /* 64 x FH */
		525, 0x000554ED, ~0, 8, 8, 4 * 8, VBI_MODULATION_NRZ_LSB /* Tb. */
	}, {
		VBI_SLICED_TELETEXT_BD_525, "Teletext System B / D (Japan), 525",
		/* NOT CONFIRMED */
		{ 10, 0 },
		{ 21, 0 },
		9600, 5727272, 5727272,
		525, 0x00AAAAE4, ~0, 10, 6, 34 * 8, VBI_MODULATION_NRZ_LSB /* Tb. */
	}, {
		VBI_SLICED_VBI_525, "VBI 525", /* Blank VBI */
		{ 10, 272 },
		{ 21, 284 },
		9500, 1510000, 1510000,
		525, 0, 0, 0, 0, 10 * 8, 0 /* 9.5-1 ... 62.4+1 us */
	},
	{
		0, NULL,
		{ 0, 0 },
		{ 0, 0 },
		0, 0, 0,
		0, 0, 0, 0, 0, 0, 0
	}
};

/**
 * @param service A @ref VBI_SLICED_ symbol, for example from a vbi_sliced structure.
 *
 * @return
 * Name of the @a service, ASCII, or @c NULL if unknown.
 */
const char *
vbi_sliced_name			(unsigned int		service)
{
	int i;

	/* These are ambiguous */
	if (service & VBI_SLICED_TELETEXT_B)
		return "Teletext System B";
	if (service & VBI_SLICED_CAPTION_525)
		return "Closed Caption 525";
	if (service & VBI_SLICED_CAPTION_625)
		return "Closed Caption 625";

	for (i = 0; vbi_services[i].id; i++)
		if (service & vbi_services[i].id)
			return vbi_services[i].label;

	return NULL;
}

#ifndef DECODER_PATTERN_DUMP
#define DECODER_PATTERN_DUMP 0
#endif

#ifndef DECODER_SERVICE_DIAG
#define DECODER_SERVICE_DIAG 0
#endif

#define dsdiag(templ, args...)						\
do { /* Check syntax in any case */					\
	if (DECODER_SERVICE_DIAG)					\
		fprintf (stderr, templ , ##args);			\
} while (0)

static __inline__ int
cpr1204_crc			(const vbi_sliced *	out)
{
	const int poly = (1 << 6) + (1 << 1) + 1;
	int crc, i;

	crc = (out->data[0] << 12) + (out->data[1] << 4) + out->data[2];
	crc |= (((1 << 6) - 1) << (14 + 6));

	for (i = 14 + 6 - 1; i >= 0; i--) {
		if (crc & ((1 << 6) << i))
			crc ^= poly << i;
	}

	return crc;
}

/**
 * @param rd Initialized vbi_raw_decoder structure.
 * @param raw A raw vbi image as defined in the vbi_raw_decoder structure
 *   (rd->sampling_format, rd->bytes_per_line, rd->count[0] + rd->count[1]
 *    scan lines).
 * @param out Buffer to store the decoded vbi_sliced data. Since every
 *   vbi scan line may contain data, this must be an array of vbi_sliced
 *   with the same number of entries as scan lines in the raw image
 *   (rd->count[0] + rd->count[1]).
 * 
 * Decode a raw vbi image, consisting of several scan lines of raw vbi data,
 * into sliced vbi data. The output is sorted by line number.
 * 
 * Note this function attempts to learn which lines carry which data
 * service, or none, to speed up decoding. You should avoid using the same
 * vbi_raw_decoder structure for different sources.
 *
 * @bug This function ignores the sampling_format field in struct
 * vbi_raw_decoder, always assuming VBI_PIXFMT_YUV420.
 *
 * @return
 * The number of lines decoded, i. e. the number of vbi_sliced records
 * written.
 */
/* XXX bit_slicer_1() */
int
vbi_raw_decode(vbi_raw_decoder *rd, uint8_t *raw, vbi_sliced *out)
{
/* FIXME not thread safe */
	static int readj = 1;

	int pitch = rd->bytes_per_line << rd->interlaced;
	int8_t *pat, *pattern = rd->pattern;
	uint8_t *raw1 = raw;
	vbi_sliced *out1 = out;
	struct _vbi_raw_decoder_job *job;
	int i, j;

	pthread_mutex_lock(&rd->mutex);

	if (!rd->services) {
		pthread_mutex_unlock(&rd->mutex);
		return 0;
	}

	for (i = 0; i < rd->count[0] + rd->count[1]; i++) {
		if (rd->interlaced && i == rd->count[0])
			raw = raw1 + rd->bytes_per_line;

		if (DECODER_PATTERN_DUMP) {
			fprintf(stderr, "L%02d ", i);
			for (j = 0; j < MAX_WAYS; j++)
				if (pattern[j] < 1 || pattern[j] > 8)
					fprintf(stderr, "0x%02x       ",
						pattern[j] & 0xFF);
				else
					fprintf(stderr, "0x%08x ",
						rd->jobs[pattern[j] - 1].id);
			fprintf(stderr, "\n");
		}

		for (pat = pattern;; pat++) {
			j = *pat; /* data service n, blank 0, or counter -n */

			if (j > 0) {
				job = rd->jobs + (j - 1);
// XXX
				if (!bit_slicer_YUV420 (&job->slicer,
out->data, raw + job->offset))
					continue; /* no match, try next data service */

				/* This service has no CRI/FRC but a CRC */
				if (job->id == VBI_SLICED_WSS_CPR1204) {
					if (cpr1204_crc (out))
						continue;
				}

				/* Positive match, output decoded line */

				out->id = job->id;

				if (i >= rd->count[0])
					out->line = (rd->start[1] > 0) ?
						rd->start[1] - rd->count[0] + i : 0;
				else
					out->line = (rd->start[0] > 0) ?
						rd->start[0] + i : 0;
				out++;

				/* Predict line as non-blank, force testing for
				   data services in the next 128 frames */
				pattern[MAX_WAYS - 1] = -128;

			} else if (pat == pattern) {
				/* Line was predicted as blank, once in 16
				   frames look for data services */
				if ((readj & 15) == 0) {
					j = pattern[0];
					memmove (&pattern[0], &pattern[1],
						 sizeof (*pattern) * (MAX_WAYS - 1));
					pattern[MAX_WAYS - 1] = j;
				}

				break;
			} else if ((j = pattern[MAX_WAYS - 1]) < 0) {
				/* Increment counter, when zero predict line as
				   blank and stop looking for data services */
				pattern[MAX_WAYS - 1] = j + 1;
    				break;
			} else {
				/* found nothing, j = 0 (blank) */
			}

			/* Try the found data service first next time */
			*pat = pattern[0];
			pattern[0] = j;

			break; /* line done */
		}

		raw += pitch;
		pattern += MAX_WAYS;
	}

	++readj;

	pthread_mutex_unlock(&rd->mutex);

	return out - out1;
}

static void
vbi_raw_decoder_remove_pattern( int job, int8_t * pattern, int pattern_size )
{
        int8_t * pat;
	int ways_left;
        int  i, j;

        for (i = 0; i < pattern_size; i++) {
                pat = pattern + i * MAX_WAYS;
                for (j = 0; j < MAX_WAYS; j++) {
                        if (pat[j] == job + 1) {
                                ways_left = (MAX_WAYS - 1) - j;

				if (0) {
					fprintf (stderr, "row %d: %02X%02X%02X%02X%02X%02X%02X%02X\n",
						 i, pat[0] & 0xff, pat[1] & 0xff,
						 pat[2] & 0xff, pat[3] & 0xff,
						 pat[4] & 0xff, pat[5] & 0xff,
						 pat[6] & 0xff, pat[7] & 0xff);
					fprintf (stderr, "row %d: memmove 0x%lX 0x%lX %d\n",
						 i, (long) &pat[j], (long) &pat[j + 1],
						 ways_left * sizeof (*pattern));
				}
                                if (ways_left > 0)
                                        memmove(&pat[j], &pat[j + 1],
                                                ways_left * sizeof(*pattern));

                                pat[MAX_WAYS - 1] = 0;
				if (0)
					fprintf (stderr, "row %d: %02X%02X%02X%02X%02X%02X%02X%02X\n",
						 i, pat[0] & 0xff,  pat[1] & 0xff,
						 pat[2] & 0xff, pat[3] & 0xff,
						 pat[4] & 0xff, pat[5] & 0xff,
						 pat[6] & 0xff, pat[7] & 0xff);
                        }
                }
        }

        for (i = 0; i < pattern_size; i++) {
                if (pattern[i] >= job + 1)
                        pattern[i] -= 1;
        }
}

/**
 * @param rd Initialized vbi_raw_decoder structure.
 * @param services Set of @ref VBI_SLICED_ symbols.
 * 
 * Removes one or more data services to be decoded from the
 * vbi_raw_decoder structure. This function can be called at any
 * time and does not touch sampling parameters. 
 * 
 * @return 
 * Set of @ref VBI_SLICED_ symbols describing the remaining data
 * services that will be decoded.
 */
unsigned int
vbi_raw_decoder_remove_services(vbi_raw_decoder *rd, unsigned int services)
{
	int i;
	int pattern_size = (rd->count[0] + rd->count[1]) * MAX_WAYS;

	pthread_mutex_lock(&rd->mutex);

	for (i = 0; i < rd->num_jobs;) {
		if (rd->jobs[i].id & services) {
			if (rd->pattern)
                                vbi_raw_decoder_remove_pattern(i, rd->pattern, pattern_size);

                        if (i + 1 < MAX_JOBS)
			        memmove(rd->jobs + i, rd->jobs + (i + 1),
				        (MAX_JOBS - (i + 1)) * sizeof(rd->jobs[0]));

			rd->num_jobs -= 1;
			rd->jobs[rd->num_jobs].id = 0;
		} else
			i++;
	}

	pthread_mutex_unlock(&rd->mutex);

	return rd->services &= ~services;
}

/**
 * @param rd Initialized vbi_raw_decoder structure.
 * @param services Set of @ref VBI_SLICED_ symbols.
 * @param strict A value of 0, 1 or 2 requests loose, reliable or strict
 *  matching of sampling parameters. For example if the data service
 *  requires knowledge of line numbers while they are not known, @c 0
 *  will accept the service (which may work if the scan lines are
 *  populated in a non-confusing way) but @c 1 or @c 2 will not. If the
 *  data service <i>may</i> use more lines than are sampled, @c 1 will
 *  accept but @c 2 will not. If unsure, set to @c 1.
 * 
 * After you initialized the sampling parameters in @a rd (according to
 * the abilities of your raw vbi source), this function adds one or more
 * data services to be decoded. The libzvbi raw vbi decoder can decode up
 * to eight data services in parallel. You can call this function while
 * already decoding, it does not change sampling parameters and you must
 * not change them either after calling this.
 * 
 * @return
 * Set of @ref VBI_SLICED_ symbols describing the data services that actually
 * will be decoded. This excludes those services not decodable given
 * the sampling parameters in @a rd.
 */
unsigned int
vbi_raw_decoder_add_services(vbi_raw_decoder *rd, unsigned int services, int strict)
{
	double off_min = (rd->scanning == 525) ? 7.9e-6 : 8.0e-6;
	int row[2], count[2], way;
	struct _vbi_raw_decoder_job *job;
	int8_t *pattern;
	int i, j, k;

	pthread_mutex_lock(&rd->mutex);

	services &= ~(VBI_SLICED_VBI_525 | VBI_SLICED_VBI_625);

	if (!rd->pattern)
		rd->pattern = (int8_t *) calloc((rd->count[0] + rd->count[1])
						* MAX_WAYS, sizeof(rd->pattern[0]));

	for (i = 0; vbi_services[i].id; i++) {
		double signal;
		int skip = 0;

		if (rd->num_jobs >= (int) MAX_JOBS)
			break;

		if (!(vbi_services[i].id & services))
			continue;

		if (vbi_services[i].scanning != rd->scanning)
			goto eliminate;

		if ((vbi_services[i].id & (VBI_SLICED_CAPTION_525_F1
					   | VBI_SLICED_CAPTION_525))
		    && (rd->start[0] <= 0 || rd->start[1] <= 0)) {
			/*
			 *  The same format is used on other lines
			 *  for non-CC data.
			 */
			goto eliminate;
		}

		signal = vbi_services[i].cri_bits / (double) vbi_services[i].cri_rate
			 + (vbi_services[i].frc_bits + vbi_services[i].payload)
			   / (double) vbi_services[i].bit_rate;

		if (rd->offset > 0 && strict > 0) {
			double offset = rd->offset / (double) rd->sampling_rate;
			double samples_end = (rd->offset + rd->bytes_per_line)
					     / (double) rd->sampling_rate;

			if (offset > (vbi_services[i].offset / 1e9 - 0.5e-6)) {
                                dsdiag ("skipping service 0x%08X: H-Off %d = %f > %f\n",
					vbi_services[i].id, rd->offset, offset,
					vbi_services[i].offset / 1e9 - 0.5e-6);
				goto eliminate;
                        }

			if (samples_end < (vbi_services[i].offset / 1e9
					   + signal + 0.5e-6)) {
                                dsdiag ("skipping service 0x%08X: sampling window too short: "
					"end %f < %f = offset %d *10^-9 + %f\n",
					vbi_services[i].id, samples_end,
					vbi_services[i].offset / 1e9 + signal + 0.5e-6,
					vbi_services[i].offset, signal);
				goto eliminate;
                        }

			if (offset < off_min) /* skip colour burst */
				skip = (int)(off_min * rd->sampling_rate);
		} else {
			double samples = rd->bytes_per_line
				         / (double) rd->sampling_rate;

			if (samples < (signal + 1.0e-6)) {
                                dsdiag ("skipping service 0x%08X: not enough samples\n",
					vbi_services[i].id);
				goto eliminate;
                        }
		}

		for (j = 0, job = rd->jobs; j < rd->num_jobs; job++, j++) {
			unsigned int id = job->id | vbi_services[i].id;

			if ((id & ~(VBI_SLICED_TELETEXT_B_L10_625 | VBI_SLICED_TELETEXT_B_L25_625)) == 0
			    || (id & ~(VBI_SLICED_CAPTION_625_F1 | VBI_SLICED_CAPTION_625)) == 0
			    || (id & ~(VBI_SLICED_CAPTION_525_F1 | VBI_SLICED_CAPTION_525)) == 0)
				break;
			/*
			 *  General form implies the special form. If both are
			 *  available from the device, decode() will set both
			 *  bits in the id field for the respective line. 
			 */
		}

		for (j = 0; j < 2; j++) {
			int start = rd->start[j];
			int end = start + rd->count[j] - 1;

			if (!rd->synchronous) {
                                dsdiag ("skipping service 0x%08X: not sync'ed\n",
					vbi_services[i].id);
				goto eliminate; /* too difficult */
                        }

			if (!(vbi_services[i].first[j] && vbi_services[i].last[j])) {
				count[j] = 0;
				continue;
			}

			if (rd->count[j] == 0) {
                                dsdiag ("skipping service 0x%08X: zero count\n",
					vbi_services[i].id);
				goto eliminate;
                        }

			if (rd->start[j] > 0 && strict > 0) {
				/*
				 *  May succeed if not all scanning lines
				 *  available for the service are actually used.
				 */
				if (strict > 1
				    || (vbi_services[i].first[j] ==
					vbi_services[i].last[j]))
					if (start > vbi_services[i].first[j] ||
					    end < vbi_services[i].last[j]) {
                                                dsdiag ("skipping service 0x%08X: lines not available "
							"have %d-%d, need %d-%d\n",
							vbi_services[i].id, start, end,
							vbi_services[i].first[j],
							vbi_services[i].last[j]);
						goto eliminate;
                                        }

				row[j] = MAX(0, (int) vbi_services[i].first[j] - start);
				count[j] = MIN(end, vbi_services[i].last[j]) - (start + row[j]) + 1;  /* XXX TZ FIX */
			} else {
				row[j] = 0;
				count[j] = rd->count[j];
			}

			row[1] += rd->count[0];

			for (pattern = rd->pattern + row[j] * MAX_WAYS, k = count[j];
			     k > 0; pattern += MAX_WAYS, k--) {
				int free = 0;

				for (way = 0; way < MAX_WAYS; way++)
					free += (pattern[way] <= 0
						 || ((pattern[way] - 1)
						     == job - rd->jobs));

				if (free <= 1) { /* reserve one NULL way */
                                        dsdiag ("skipping service 0x%08X: no more patterns free\n",
						vbi_services[i].id);
					goto eliminate;
                                }
			}
		}

		for (j = 0; j < 2; j++) {
			for (pattern = rd->pattern + row[j] * MAX_WAYS, k = count[j];
			     k > 0; pattern += MAX_WAYS, k--) {
				for (way = 0; pattern[way] > 0
				      && (pattern[way] - 1) != (job - rd->jobs); way++);

                                assert((pattern + MAX_WAYS - rd->pattern) <= (rd->count[0] + rd->count[1]) * MAX_WAYS);

				pattern[way] = (job - rd->jobs) + 1;
				pattern[MAX_WAYS - 1] = -128;
			}
                }

		job->id |= vbi_services[i].id;
		job->offset = skip;

		vbi_bit_slicer_init
			(&job->slicer,
			 rd->sampling_format,
			 rd->sampling_rate,
			 rd->bytes_per_line - skip, /* XXX * bpp? */
			 vbi_services[i].cri_frc >> vbi_services[i].frc_bits,
			 vbi_services[i].cri_mask,
			 vbi_services[i].cri_bits,
			 vbi_services[i].cri_rate,
			 vbi_services[i].cri_frc & ((1 << vbi_services[i].frc_bits) - 1),
			 vbi_services[i].frc_bits,
			 vbi_services[i].payload,
			 vbi_services[i].bit_rate,
			 vbi_services[i].modulation);

		if (job >= rd->jobs + rd->num_jobs)
			rd->num_jobs++;

		rd->services |= vbi_services[i].id;
eliminate:
		;
	}

	pthread_mutex_unlock(&rd->mutex);

	return rd->services;
}

/**
 * @param rd Initialized vbi_raw_decoder structure.
 * @param services Set of VBI_SLICED_ symbols. Here (and only here) you
 *   can add @c VBI_SLICED_VBI_625 or @c VBI_SLICED_VBI_525 to include all
 *   vbi scan lines in the calculated sampling parameters.
 * @param scanning When 525 accept only NTSC services, when 625
 *   only PAL/SECAM services. When scanning is 0, assume the scanning
 *   from the requested services, an ambiguous set will pick
 *   a 525 or 625 line system at random.
 * @param max_rate If given, the highest data bit rate in Hz of all
 *   services requested is stored here. (The sampling rate
 *   should be at least twice as high; rd->sampling_rate will
 *   be set to a more reasonable value of 27 MHz derived
 *   from ITU-R Rec. 601.)
 * 
 * Calculate the sampling parameters in @a rd required to receive and
 * decode the requested data @a services. rd->sampling_format will be
 * @c VBI_PIXFMT_YUV420, rd->bytes_per_line set accordingly to a
 * reasonable minimum. This function can be used to initialize hardware
 * prior to calling vbi_raw_decoder_add_service().
 * 
 * @return
 * Set of @ref VBI_SLICED_ symbols describing the data services covered
 * by the calculated sampling parameters. This excludes services the libzvbi
 * raw decoder cannot decode.
 */
unsigned int
vbi_raw_decoder_parameters(vbi_raw_decoder *rd, unsigned int services,
			   int scanning, int *max_rate)
{
	int i, j;

	pthread_mutex_lock(&rd->mutex);

	rd->scanning = scanning;
	rd->sampling_format = VBI_PIXFMT_YUV420;
	rd->sampling_rate = 27000000;	/* ITU-R Rec. 601 */
	rd->bytes_per_line = 0;
	rd->offset = (int)(1000e-6 * rd->sampling_rate);
	rd->start[0] = 1000;
	rd->count[0] = 0;
	rd->start[1] = 1000;
	rd->count[1] = 0;
	rd->interlaced = FALSE;
	rd->synchronous = TRUE;

	/*
	 *  Will only allocate as many vbi lines as
	 *  we need, eg. two lines per frame for CC 525,
	 *  and set reasonable parameters.
	 */
	for (i = 0; vbi_services[i].id; i++) {
		double left_margin;
		int offset, samples;
		double signal;

		if (!(vbi_services[i].id & services))
			continue;

		if (rd->scanning == 0)
			rd->scanning = vbi_services[i].scanning;

		left_margin = (rd->scanning == 525) ? 1.0e-6 : 2.0e-6;

		if (vbi_services[i].scanning != rd->scanning) {
			services &= ~vbi_services[i].id;
			continue;
		}

		*max_rate = MAX(*max_rate,
			MAX(vbi_services[i].cri_rate,
			    vbi_services[i].bit_rate));

		signal = vbi_services[i].cri_bits / (double) vbi_services[i].cri_rate
			 + (vbi_services[i].frc_bits + vbi_services[i].payload)
			   / (double) vbi_services[i].bit_rate;

		offset = (int)((vbi_services[i].offset / 1e9 - left_margin)
			* rd->sampling_rate + 0.5);
		samples = (int)((signal + left_margin + 1.0e-6)
			  * rd->sampling_rate + 0.5);

		rd->offset = MIN(rd->offset, offset);

		rd->bytes_per_line =
			MIN(rd->bytes_per_line + rd->offset,
			    samples + offset) - rd->offset;

		for (j = 0; j < 2; j++)
			if (vbi_services[i].first[j] &&
			    vbi_services[i].last[j]) {
				rd->start[j] =
					MIN(rd->start[j],
					    vbi_services[i].first[j]);
				rd->count[j] =
					MAX(rd->start[j] + rd->count[j],
					    vbi_services[i].last[j] + 1)
					- rd->start[j];
			}
	}

	if (!rd->count[0])
		rd->start[0] = -1;

	if (!rd->count[1]) {
		rd->start[1] = -1;

		if (!rd->count[0])
			rd->offset = 0;
	}

	pthread_mutex_unlock(&rd->mutex);

	return services;
}

/**
 * @param rd Initialized vbi_raw_decoder structure.
 * 
 * Reset a vbi_raw_decoder structure. This removes
 * all previously added services to be decoded (if any)
 * but does not touch the sampling parameters. You are
 * free to change the sampling parameters after calling this.
 */
void
vbi_raw_decoder_reset(vbi_raw_decoder *rd)
{
	if (!rd)
		return;

	pthread_mutex_lock(&rd->mutex);

	if (rd->pattern)
		free(rd->pattern);

	rd->services = 0;
	rd->num_jobs = 0;

	rd->pattern = NULL;

	memset(rd->jobs, 0, sizeof(rd->jobs));

	pthread_mutex_unlock(&rd->mutex);
}

/**
 * @param rd Pointer to initialized vbi_raw_decoder
 *  structure, can be @c NULL.
 * 
 * Free all resources associated with @a rd.
 */
void
vbi_raw_decoder_destroy(vbi_raw_decoder *rd)
{
	vbi_raw_decoder_reset(rd);

	pthread_mutex_destroy(&rd->mutex);
}

/**
 * @param rd Pointer to a vbi_raw_decoder structure.
 * 
 * Initializes a vbi_raw_decoder structure.
 */
void
vbi_raw_decoder_init(vbi_raw_decoder *rd)
{
	pthread_mutex_init(&rd->mutex, NULL);

	rd->pattern = NULL;

	vbi_raw_decoder_reset(rd);
}
