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

/* $Id: sampling.h,v 1.1.2.1 2004-01-30 00:40:27 mschimek Exp $ */

#ifndef SAMPLING_H
#define SAMPLING_H

#include "sliced.h"

/* Public */

#include <stdio.h>		/* FILE* */
#include <inttypes.h>		/* uint64_t */

/* Private */
/* This code comes from zapping/tveng, which is in turn based on
   V4L2 (new), except numbers have been shifted up to make room
   for VBI_VIDEOSTD_NONE. */
/* Public */

/**
 * @ingroup Sampling
 * Videostandard identifier.
 */
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
 * @ingroup Sampling
 * A set of videostandards is used where more than one
 * videostandard may apply. Use VBI_VIDEOSTD_SET macros
 * to build a set.
 */
typedef uint64_t vbi_videostd_set;

/**
 * @addtogroup Sampling
 * @{
 */
#define VBI_VIDEOSTD_SET(videostd) (((vbi_videostd_set) 1) << (videostd))

#define VBI_VIDEOSTD_SET_UNKNOWN 0
#define VBI_VIDEOSTD_SET_EMPTY 0
#define VBI_VIDEOSTD_SET_PAL_BG (+ VBI_VIDEOSTD_SET (VBI_VIDEOSTD_PAL_B)	\
				 + VBI_VIDEOSTD_SET (VBI_VIDEOSTD_PAL_B1)	\
				 + VBI_VIDEOSTD_SET (VBI_VIDEOSTD_PAL_G))
#define VBI_VIDEOSTD_SET_PAL_DK (+ VBI_VIDEOSTD_SET (VBI_VIDEOSTD_PAL_D)	\
				 + VBI_VIDEOSTD_SET (VBI_VIDEOSTD_PAL_D1)	\
				 + VBI_VIDEOSTD_SET (VBI_VIDEOSTD_PAL_K))
#define VBI_VIDEOSTD_SET_PAL    (+ VBI_VIDEOSTD_SET_PAL_BG			\
				 + VBI_VIDEOSTD_SET_PAL_DK			\
				 + VBI_VIDEOSTD_SET (VBI_VIDEOSTD_PAL_H)	\
				 + VBI_VIDEOSTD_SET (VBI_VIDEOSTD_PAL_I))
#define VBI_VIDEOSTD_SET_NTSC   (+ VBI_VIDEOSTD_SET (VBI_VIDEOSTD_NTSC_M)	\
				 + VBI_VIDEOSTD_SET (VBI_VIDEOSTD_NTSC_M_JP))
#define VBI_VIDEOSTD_SET_SECAM  (+ VBI_VIDEOSTD_SET (VBI_VIDEOSTD_SECAM_B)	\
				 + VBI_VIDEOSTD_SET (VBI_VIDEOSTD_SECAM_D)	\
				 + VBI_VIDEOSTD_SET (VBI_VIDEOSTD_SECAM_G)	\
				 + VBI_VIDEOSTD_SET (VBI_VIDEOSTD_SECAM_H)	\
				 + VBI_VIDEOSTD_SET (VBI_VIDEOSTD_SECAM_K)	\
				 + VBI_VIDEOSTD_SET (VBI_VIDEOSTD_SECAM_K1)	\
				 + VBI_VIDEOSTD_SET (VBI_VIDEOSTD_SECAM_L))
#define VBI_VIDEOSTD_SET_525_60 (+ VBI_VIDEOSTD_SET (VBI_VIDEOSTD_PAL_M)	\
				 + VBI_VIDEOSTD_SET_NTSC)
#define VBI_VIDEOSTD_SET_625_50 (+ VBI_VIDEOSTD_SET_PAL				\
				 + VBI_VIDEOSTD_SET (VBI_VIDEOSTD_PAL_N)	\
				 + VBI_VIDEOSTD_SET (VBI_VIDEOSTD_PAL_NC)	\
				 + VBI_VIDEOSTD_SET_SECAM)
/**
 * All defined videostandards, no custom standards.
 */
#define VBI_VIDEOSTD_SET_ALL    (+ VBI_VIDEOSTD_SET_525_60			\
				 + VBI_VIDEOSTD_SET_625_50)
#define VBI_VIDEOSTD_SET_CUSTOM							\
	((~VBI_VIDEOSTD_SET_EMPTY) << VBI_VIDEOSTD_CUSTOM_BEGIN)

extern const char *
vbi_videostd_name		(vbi_videostd		videostd)
	vbi_attribute_const;
/** @} */

/**
 * @ingroup Sampling
 * Sample format identifier.
 */
typedef enum {
	VBI_PIXFMT_NONE,
	VBI_PIXFMT_UNKNOWN = VBI_PIXFMT_NONE,

	VBI_PIXFMT_RESERVED0,

	/* Planar YUV formats */

	VBI_PIXFMT_YUV444,		/* 4x4 4x4 4x4 */
	VBI_PIXFMT_YVU444,
	VBI_PIXFMT_YUV422,		/* 4x4 2x4 2x4 */
	VBI_PIXFMT_YVU422,
	VBI_PIXFMT_YUV411,		/* 4x4 1x4 1x4 */
	VBI_PIXFMT_YVU411,
	VBI_PIXFMT_YUV420,		/* 4x4 2x2 2x2 */
	VBI_PIXFMT_YVU420,
	VBI_PIXFMT_YUV410,		/* 4x4 1x1 1x1 */
	VBI_PIXFMT_YVU410,

	/* Packed YUV formats */	/* register	memory byte 0..3 */

	VBI_PIXFMT_YUVA24_LE,		/* 0xAAVVUUYY	Y U V A */
	VBI_PIXFMT_YUVA24_BE,
	VBI_PIXFMT_YVUA24_LE,
	VBI_PIXFMT_YVUA24_BE,

	VBI_PIXFMT_AVUY24_BE = VBI_PIXFMT_YUVA24_LE,
	VBI_PIXFMT_AVUY24_LE,
	VBI_PIXFMT_AUVY24_BE,
	VBI_PIXFMT_AUVY24_LE,

	VBI_PIXFMT_YUV24_LE,		/* 0xVVUUYY	Y U V */
	VBI_PIXFMT_YUV24_BE,
	VBI_PIXFMT_YVU24_LE,
	VBI_PIXFMT_YVU24_BE,

	VBI_PIXFMT_VUY24_BE = VBI_PIXFMT_YUV24_LE,
	VBI_PIXFMT_VUY24_LE,
	VBI_PIXFMT_UVY24_BE,
	VBI_PIXFMT_UVY24_LE,

	VBI_PIXFMT_YUYV,		/* Y0 U Y1 V in memory */
	VBI_PIXFMT_YVYU,		/* Y0 V Y1 U */
	VBI_PIXFMT_UYVY,		/* U Y0 V Y1 */
	VBI_PIXFMT_VYUY,		/* V Y0 U Y1 */

	VBI_PIXFMT_RESERVED1,
	VBI_PIXFMT_Y8,			/* Y */

	VBI_PIXFMT_RESERVED2,
	VBI_PIXFMT_RESERVED3,

	/* Packed RGB formats */

	VBI_PIXFMT_RGBA24_LE,		/* 0xAABBGGRR   R G B A */
	VBI_PIXFMT_RGBA24_BE,
	VBI_PIXFMT_BGRA24_LE,
	VBI_PIXFMT_BGRA24_BE,

	VBI_PIXFMT_ABGR24_BE = VBI_PIXFMT_RGBA24_LE,
	VBI_PIXFMT_ABGR24_LE,
	VBI_PIXFMT_ARGB24_BE,
	VBI_PIXFMT_ARGB24_LE,

	VBI_PIXFMT_RGB24_LE,		/* 0xBBGGRR	R G B */
	VBI_PIXFMT_BGR24_LE,

	VBI_PIXFMT_BGR24_BE = VBI_PIXFMT_RGB24_LE,
	VBI_PIXFMT_RGB24_BE,

	VBI_PIXFMT_RGB16_LE,		/* bbbbbggggggrrrrr msb..lsb */
	VBI_PIXFMT_RGB16_BE,
	VBI_PIXFMT_BGR16_LE,		/* rrrrrggggggbbbbb */
	VBI_PIXFMT_BGR16_BE,

	VBI_PIXFMT_RGBA15_LE,		/* abbbbbgggggrrrrr */
	VBI_PIXFMT_RGBA15_BE,
	VBI_PIXFMT_BGRA15_LE,		/* arrrrrgggggbbbbb */
	VBI_PIXFMT_BGRA15_BE,
	VBI_PIXFMT_ARGB15_LE,		/* bbbbbgggggrrrrra */
	VBI_PIXFMT_ARGB15_BE,
	VBI_PIXFMT_ABGR15_LE,		/* rrrrrgggggbbbbba */
	VBI_PIXFMT_ABGR15_BE,

	VBI_PIXFMT_RGBA12_LE,		/* aaaabbbbggggrrrr */
	VBI_PIXFMT_RGBA12_BE,
	VBI_PIXFMT_BGRA12_LE,		/* aaaarrrrggggbbbb */
	VBI_PIXFMT_BGRA12_BE,
	VBI_PIXFMT_ARGB12_LE,		/* bbbbggggrrrraaaa */
	VBI_PIXFMT_ARGB12_BE,
	VBI_PIXFMT_ABGR12_LE,		/* rrrrggggbbbbaaaa */
	VBI_PIXFMT_ABGR12_BE,

	VBI_PIXFMT_RGB8,		/* bbgggrrr */
	VBI_PIXFMT_BGR8,		/* rrrgggbb */

	VBI_PIXFMT_RGBA7,		/* abbgggrr */
	VBI_PIXFMT_BGRA7,		/* arrgggbb */
	VBI_PIXFMT_ARGB7,		/* bbgggrra */
	VBI_PIXFMT_ABGR7		/* rrgggbba */
} vbi_pixfmt;

#define VBI_MAX_PIXFMTS 64

/**
 * @ingroup Sampling
 * A set of pixel formats is used where more than one
 * format may apply. Use VBI_PIXFMT_SET macros
 * to build a set.
 */
typedef uint64_t vbi_pixfmt_set;

/**
 * @addtogroup Sampling
 * @{
 */
#define VBI_PIXFMT_SET(pixfmt) (((vbi_pixfmt_set) 1) << (pixfmt))

#define VBI_PIXFMT_SET_UNKNOWN 0
#define VBI_PIXFMT_SET_EMPTY 0
#define VBI_PIXFMT_SET_YUV_PLANAR (+ VBI_PIXFMT_SET (VBI_PIXFMT_YUV444)		\
				   + VBI_PIXFMT_SET (VBI_PIXFMT_YVU444)		\
				   + VBI_PIXFMT_SET (VBI_PIXFMT_YUV422)		\
				   + VBI_PIXFMT_SET (VBI_PIXFMT_YVU422)		\
				   + VBI_PIXFMT_SET (VBI_PIXFMT_YUV411)		\
				   + VBI_PIXFMT_SET (VBI_PIXFMT_YVU411)		\
				   + VBI_PIXFMT_SET (VBI_PIXFMT_YUV420)		\
				   + VBI_PIXFMT_SET (VBI_PIXFMT_YVU420)		\
				   + VBI_PIXFMT_SET (VBI_PIXFMT_YUV410)		\
				   + VBI_PIXFMT_SET (VBI_PIXFMT_YVU410))
#define VBI_PIXFMT_SET_YUVA24    (+ VBI_PIXFMT_SET (VBI_PIXFMT_YUVA24_LE)	\
				  + VBI_PIXFMT_SET (VBI_PIXFMT_YUVA24_BE)	\
				  + VBI_PIXFMT_SET (VBI_PIXFMT_YVUA24_LE)	\
				  + VBI_PIXFMT_SET (VBI_PIXFMT_YVUA24_BE))
#define VBI_PIXFMT_SET_YUV24	 (+ VBI_PIXFMT_SET (VBI_PIXFMT_YUV24_LE)	\
				  + VBI_PIXFMT_SET (VBI_PIXFMT_YUV24_BE)	\
				  + VBI_PIXFMT_SET (VBI_PIXFMT_YVU24_LE)	\
				  + VBI_PIXFMT_SET (VBI_PIXFMT_YVU24_BE))
#define VBI_PIXFMT_SET_YUV16	 (+ VBI_PIXFMT_SET (VBI_PIXFMT_YUYV)		\
				  + VBI_PIXFMT_SET (VBI_PIXFMT_YVYU)		\
				  + VBI_PIXFMT_SET (VBI_PIXFMT_UYVY)		\
				  + VBI_PIXFMT_SET (VBI_PIXFMT_VYUY))
#define VBI_PIXFMT_SET_YUV_PACKED (+ VBI_PIXFMT_SET_YUVA24			\
				   + VBI_PIXFMT_SET_YUV24			\
				   + VBI_PIXFMT_SET_YUV16			\
				   + VBI_PIXFMT_SET (VBI_PIXFMT_Y8))
#define VBI_PIXFMT_SET_YUV	 (+ VBI_PIXFMT_SET_YUV_PLANAR			\
				  + VBI_PIXFMT_SET_YUV_PACKED)
#define VBI_PIXFMT_SET_RGBA24	 (+ VBI_PIXFMT_SET (VBI_PIXFMT_RGBA24_LE)	\
				  + VBI_PIXFMT_SET (VBI_PIXFMT_RGBA24_BE)	\
				  + VBI_PIXFMT_SET (VBI_PIXFMT_BGRA24_LE)	\
				  + VBI_PIXFMT_SET (VBI_PIXFMT_BGRA24_BE))
#define VBI_PIXFMT_SET_RGB24	 (+ VBI_PIXFMT_SET (VBI_PIXFMT_RGB24_LE)	\
				  + VBI_PIXFMT_SET (VBI_PIXFMT_BGR24_LE))
#define VBI_PIXFMT_SET_RGB16	 (+ VBI_PIXFMT_SET (VBI_PIXFMT_RGB16_LE)	\
				  + VBI_PIXFMT_SET (VBI_PIXFMT_RGB16_BE)	\
				  + VBI_PIXFMT_SET (VBI_PIXFMT_BGR16_LE)	\
				  + VBI_PIXFMT_SET (VBI_PIXFMT_BGR16_BE))
#define VBI_PIXFMT_SET_RGB15	 (+ VBI_PIXFMT_SET (VBI_PIXFMT_RGBA15_LE)	\
				  + VBI_PIXFMT_SET (VBI_PIXFMT_RGBA15_BE)	\
				  + VBI_PIXFMT_SET (VBI_PIXFMT_BGRA15_LE)	\
				  + VBI_PIXFMT_SET (VBI_PIXFMT_BGRA15_BE)	\
				  + VBI_PIXFMT_SET (VBI_PIXFMT_ARGB15_LE)	\
				  + VBI_PIXFMT_SET (VBI_PIXFMT_ARGB15_BE)	\
				  + VBI_PIXFMT_SET (VBI_PIXFMT_ABGR15_LE)	\
				  + VBI_PIXFMT_SET (VBI_PIXFMT_ABGR15_BE))
#define VBI_PIXFMT_SET_RGB12	 (+ VBI_PIXFMT_SET (VBI_PIXFMT_RGBA12_LE)	\
				  + VBI_PIXFMT_SET (VBI_PIXFMT_RGBA12_BE)	\
				  + VBI_PIXFMT_SET (VBI_PIXFMT_BGRA12_LE)	\
				  + VBI_PIXFMT_SET (VBI_PIXFMT_BGRA12_BE)	\
				  + VBI_PIXFMT_SET (VBI_PIXFMT_ARGB12_LE)	\
				  + VBI_PIXFMT_SET (VBI_PIXFMT_ARGB12_BE)	\
				  + VBI_PIXFMT_SET (VBI_PIXFMT_ABGR12_LE)	\
				  + VBI_PIXFMT_SET (VBI_PIXFMT_ABGR12_BE))
#define VBI_PIXFMT_SET_RGB8	 (+ VBI_PIXFMT_SET (VBI_PIXFMT_RGB8)	\
				  + VBI_PIXFMT_SET (VBI_PIXFMT_BGR8))
#define VBI_PIXFMT_SET_RGB7	 (+ VBI_PIXFMT_SET (VBI_PIXFMT_RGBA7)	\
				  + VBI_PIXFMT_SET (VBI_PIXFMT_BGRA7)	\
				  + VBI_PIXFMT_SET (VBI_PIXFMT_ARGB7)	\
				  + VBI_PIXFMT_SET (VBI_PIXFMT_ABGR7))
#define VBI_PIXFMT_SET_RGB_PACKED (+ VBI_PIXFMT_SET_RGBA24		\
				   + VBI_PIXFMT_SET_RGB24		\
				   + VBI_PIXFMT_SET_RGB16		\
				   + VBI_PIXFMT_SET_RGB15		\
				   + VBI_PIXFMT_SET_RGB12		\
				   + VBI_PIXFMT_SET_RGB8		\
				   + VBI_PIXFMT_SET_RGB7)
#define VBI_PIXFMT_SET_RGB	    VBI_PIXFMT_SET_RGB_PACKED
#define VBI_PIXFMT_SET_PLANAR	    VBI_PIXFMT_SET_YUV_PLANAR
#define VBI_PIXFMT_SET_PACKED	 (+ VBI_PIXFMT_SET_YUV_PACKED		\
				  + VBI_PIXFMT_SET_RGB_PACKED)
#define VBI_PIXFMT_SET_ALL	 (+ VBI_PIXFMT_SET_YUV			\
				  + VBI_PIXFMT_SET_RGB)

#define VBI_PIXFMT_IS_YUV(pixfmt)					\
	(0 != (VBI_PIXFMT_SET (pixfmt) & VBI_PIXFMT_SET_YUV))
#define VBI_PIXFMT_IS_RGB(pixfmt)					\
	(0 != (VBI_PIXFMT_SET (pixfmt) & VBI_PIXFMT_SET_RGB))
#define VBI_PIXFMT_IS_PLANAR(pixfmt)					\
	(0 != (VBI_PIXFMT_SET (pixfmt) & VBI_PIXFMT_SET_PLANAR))
#define VBI_PIXFMT_IS_PACKED(pixfmt)					\
	(0 != (VBI_PIXFMT_SET (pixfmt) & VBI_PIXFMT_SET_PACKED))

#ifdef __GNUC__
#define VBI_PIXFMT_BYTES_PER_PIXEL(pixfmt)				\
	(!__builtin_constant_p (pixfmt) ?				\
	 vbi_pixfmt_bytes_per_pixel (pixfmt) :				\
	 ((VBI_PIXFMT_SET (pixfmt) & VBI_PIXFMT_SET_YUVA24) ? 4 :	\
	  ((VBI_PIXFMT_SET (pixfmt) & VBI_PIXFMT_SET_RGBA24) ? 4 :	\
	   ((VBI_PIXFMT_SET (pixfmt) & VBI_PIXFMT_SET_YUV24) ? 3 :	\
	    ((VBI_PIXFMT_SET (pixfmt) & VBI_PIXFMT_SET_RGB24) ? 3 :	\
	     ((VBI_PIXFMT_SET (pixfmt) & VBI_PIXFMT_SET_YUV16) ? 2 :	\
	      ((VBI_PIXFMT_SET (pixfmt) & VBI_PIXFMT_SET_RGB16) ? 2 :	\
	       ((VBI_PIXFMT_SET (pixfmt) & VBI_PIXFMT_SET_RGB15) ? 2 :	\
	        ((VBI_PIXFMT_SET (pixfmt) & VBI_PIXFMT_SET_RGB12) ? 2 :	\
		 1)))))))))
#else
#define VBI_PIXFMT_BYTES_PER_PIXEL(pixfmt)				\
	(vbi_pixfmt_bytes_per_pixel (pixfmt))
#endif

extern const char *
vbi_pixfmt_name			(vbi_pixfmt		pixfmt)
	vbi_attribute_const;
extern unsigned int
vbi_pixfmt_bytes_per_pixel	(vbi_pixfmt		pixfmt)
	vbi_attribute_const;
/** @} */

#if 0
typedef enum {
	VBI_PIXFMT_YUV420 = 1,
	VBI_PIXFMT_YUYV,
	VBI_PIXFMT_YVYU,
	VBI_PIXFMT_UYVY,
	VBI_PIXFMT_VYUY,
	VBI_PIXFMT_RGBA32_LE = 32,
	VBI_PIXFMT_RGBA32_BE,
	VBI_PIXFMT_BGRA32_LE,
	VBI_PIXFMT_BGRA32_BE,
	VBI_PIXFMT_ABGR32_BE = 32, /* synonyms */
	VBI_PIXFMT_ABGR32_LE,
	VBI_PIXFMT_ARGB32_BE,
	VBI_PIXFMT_ARGB32_LE,
	VBI_PIXFMT_RGB24,
	VBI_PIXFMT_BGR24,
	VBI_PIXFMT_RGB16_LE,
	VBI_PIXFMT_RGB16_BE,
	VBI_PIXFMT_BGR16_LE,
	VBI_PIXFMT_BGR16_BE,
	VBI_PIXFMT_RGBA15_LE,
	VBI_PIXFMT_RGBA15_BE,
	VBI_PIXFMT_BGRA15_LE,
	VBI_PIXFMT_BGRA15_BE,
	VBI_PIXFMT_ARGB15_LE,
	VBI_PIXFMT_ARGB15_BE,
	VBI_PIXFMT_ABGR15_LE,
	VBI_PIXFMT_ABGR15_BE
} vbi_pixfmt;

extern const char *
vbi_pixfmt_name			(vbi_pixfmt		pixfmt)
	vbi_attribute_const;

/* Private */

#define VBI_PIXFMT_RGB2(fmt) ((fmt) >= VBI_PIXFMT_RGB16_LE)

#define VBI_PIXFMT_BPP(fmt)						\
	(((fmt) == VBI_PIXFMT_YUV420) ? 1 :				\
	 (((fmt) >= VBI_PIXFMT_RGBA32_LE				\
	   && (fmt) <= VBI_PIXFMT_BGRA32_BE) ? 4 :			\
	  (((fmt) >= VBI_PIXFMT_RGB24					\
	    && (fmt) <= VBI_PIXFMT_BGR24) ? 3 : 2)))

/* Public */
#endif

/**
 * @ingroup Sampling
 * @brief Raw VBI sampling parameters.
 */
typedef struct {
	/**
	 * Defines the system all line numbers refer to. Can be more
	 * than one standard if not exactly known, provided there
	 * is no ambiguity.
	 */
	vbi_videostd_set	videostd_set;
	/**
	 * Format of the raw vbi data.
	 */
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

/**
 * @addtogroup Sampling
 * @{
 */
extern vbi_service_set
vbi_sampling_par_from_services	(vbi_sampling_par *	sp,
				 unsigned int *		max_rate,
				 vbi_videostd_set	videostd_set,
				 vbi_service_set	services);
extern vbi_service_set
vbi_sampling_par_check_services	(const vbi_sampling_par *sp,
				 vbi_service_set	services,
				 unsigned int		strict)
	vbi_attribute_pure;
/** @} */

/* Private */

extern vbi_bool
vbi_sampling_par_check_service	(const vbi_sampling_par *sp,
				 const vbi_service_par *par,
				 unsigned int		strict)
	vbi_attribute_pure;
extern vbi_bool
vbi_sampling_par_verify		(const vbi_sampling_par *sp)
	vbi_attribute_pure;

#endif /* SAMPLING_H */
