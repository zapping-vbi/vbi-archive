/*
 *  libzvbi - Image definitions
 *
 *  Copyright (C) 2003, 2004 Michael H. Schimek
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

/* $Id: image_format.h,v 1.1.2.2 2007-11-01 00:21:23 mschimek Exp $ */

#ifndef __ZVBI3_IMAGE_FORMAT_H__
#define __ZVBI3_IMAGE_FORMAT_H__

#include <inttypes.h>		/* uint64_t */
#include "macros.h"

VBI3_BEGIN_DECLS

/**
 * @addtogroup Types
 * @{
 */

/** Pixel format identifier. */
typedef enum {
	VBI3_PIXFMT_NONE,			/**< */
	VBI3_PIXFMT_UNKNOWN = VBI3_PIXFMT_NONE,	/**< */

	VBI3_PIXFMT_RESERVED0,

	/**
	 * Planar YUV formats. Letters XYZ describe the contents of the
	 * first to last plane in memory. Numbers follow standard naming
	 * conventions, e. g. YUV422 means YUV 4:2:2.
	 *
	 * Relative plane sizes of VBI3_PIXFMT_YUV444: 4x4 4x4 4x4
	 */
	VBI3_PIXFMT_YUV444,
	VBI3_PIXFMT_YVU444,	/**< 4x4 4x4 4x4 */
	VBI3_PIXFMT_YUV422,	/**< 4x4 2x4 2x4 */
	VBI3_PIXFMT_YVU422,	/**< 4x4 2x4 2x4 */
	VBI3_PIXFMT_YUV411,	/**< 4x4 1x4 1x4 */
	VBI3_PIXFMT_YVU411,	/**< 4x4 1x4 1x4 */
	VBI3_PIXFMT_YUV420,	/**< 4x4 2x2 2x2 */
	VBI3_PIXFMT_YVU420,	/**< 4x4 2x2 2x2 */
	VBI3_PIXFMT_YUV410,	/**< 4x4 1x1 1x1 */
	VBI3_PIXFMT_YVU410,	/**< 4x4 1x1 1x1 */

	/**
	 * Packed YUV formats. Letters WXYZ describe register contents
	 * from last to most significant bit. Numbers are the significant
	 * bits without alpha bits. Appendix _LE means LSB is stored
	 * first in memory, _BE means MSB.
	 *
	 * VBI3_PIXFMT_YUVA24_LE in register: 0xAAVVUUYY, memory: Y U V A
	 */
	VBI3_PIXFMT_YUVA24_LE,
	VBI3_PIXFMT_YUVA24_BE,	/**< 0xAAVVUUYY, A V U Y */
	VBI3_PIXFMT_YVUA24_LE,	/**< 0xAAUUVVYY, Y V U A */
	VBI3_PIXFMT_YVUA24_BE,	/**< 0xAAUUVVYY, A U V Y */

	VBI3_PIXFMT_AVUY24_BE = VBI3_PIXFMT_YUVA24_LE,	/**< */
	VBI3_PIXFMT_AVUY24_LE,	/**< */
	VBI3_PIXFMT_AUVY24_BE,	/**< */
	VBI3_PIXFMT_AUVY24_LE,	/**< */

	VBI3_PIXFMT_YUV24_LE,	/**< 0xVVUUYY, Y U V */
	VBI3_PIXFMT_YUV24_BE,	/**< 0xVVUUYY, V U Y */
	VBI3_PIXFMT_YVU24_LE,	/**< 0xUUVVYY, Y V U */
	VBI3_PIXFMT_YVU24_BE,	/**< 0xUUVVYY, U V Y */

	VBI3_PIXFMT_VUY24_BE = VBI3_PIXFMT_YUV24_LE,	/**< */
	VBI3_PIXFMT_VUY24_LE,	/**< */
	VBI3_PIXFMT_UVY24_BE,	/**< */
	VBI3_PIXFMT_UVY24_LE,	/**< */

	/**
	 * Packed YUV 4:2:2 formats. Letters WXYZ describe color components
	 * stored at ascending memory addresses, 8 bits each.
	 *
	 * VBI3_PIXFMT_YUYV: Y0, (U0 + U1) / 2, Y1, (V0 + V1) / 2 in memory
	 */
	VBI3_PIXFMT_YUYV,
	VBI3_PIXFMT_YVYU,	/**< Y0  V Y1  U */
	VBI3_PIXFMT_UYVY,	/**<  U Y0  V Y1 */
	VBI3_PIXFMT_VYUY,	/**<  V Y0  U Y1 */

	VBI3_PIXFMT_RESERVED1,

	/** Y only, 8 bit per pixel. */
	VBI3_PIXFMT_Y8,

	VBI3_PIXFMT_RESERVED2,
	VBI3_PIXFMT_RESERVED3,

	/**
	 * Packed RGB formats. Letters WXYZ describe register contents
	 * from last to most significant bit. Numbers are the significant
	 * bits without alpha bits. Appendix _LE means LSB is stored
	 * first in memory, _BE means MSB.
	 *
	 * VBI3_PIXFMT_RGBA24_LE in register: 0xAABBGGRR, memory: R G B A
	 */
	VBI3_PIXFMT_RGBA24_LE,
	VBI3_PIXFMT_RGBA24_BE,	/**< 0xAABBGGRR, A B G R */
	VBI3_PIXFMT_BGRA24_LE,	/**< 0xAARRGGBB, B G R A */
	VBI3_PIXFMT_BGRA24_BE,	/**< 0xAARRGGBB, A R G B */

	VBI3_PIXFMT_ABGR24_BE = VBI3_PIXFMT_RGBA24_LE,	/**< */
	VBI3_PIXFMT_ABGR24_LE,	/**< */
	VBI3_PIXFMT_ARGB24_BE,	/**< */
	VBI3_PIXFMT_ARGB24_LE,	/**< */

	VBI3_PIXFMT_RGB24_LE,	/* 0xBBGGRR, R G B */
	VBI3_PIXFMT_RGB24_BE,	/* 0xRRGGBB, B G R */

	VBI3_PIXFMT_BGR24_BE = VBI3_PIXFMT_RGB24_LE,	/**< */
	VBI3_PIXFMT_BGR24_LE,	/**< */

	/** In register: bbbbbggg gggrrrrr (msb to lsb) */
	VBI3_PIXFMT_RGB16_LE,
	VBI3_PIXFMT_RGB16_BE,	/**< */
	VBI3_PIXFMT_BGR16_LE,	/**< rrrrrggg gggbbbbb */
	VBI3_PIXFMT_BGR16_BE,	/**< */

	VBI3_PIXFMT_RGBA15_LE,	/**< abbbbbgg gggrrrrr */
	VBI3_PIXFMT_RGBA15_BE,	/**< */
	VBI3_PIXFMT_BGRA15_LE,	/**< arrrrrgg gggbbbbb */
	VBI3_PIXFMT_BGRA15_BE,	/**< */

	VBI3_PIXFMT_ARGB15_LE,	/**< bbbbbggg ggrrrrra */
	VBI3_PIXFMT_ARGB15_BE,	/**< */
	VBI3_PIXFMT_ABGR15_LE,	/**< rrrrrggg ggbbbbba */
	VBI3_PIXFMT_ABGR15_BE,	/**< */

	VBI3_PIXFMT_RGBA12_LE,	/**< aaaabbbb ggggrrrr */
	VBI3_PIXFMT_RGBA12_BE,	/**< */
	VBI3_PIXFMT_BGRA12_LE,	/**< aaaarrrr ggggbbbb */
	VBI3_PIXFMT_BGRA12_BE,	/**< */
	VBI3_PIXFMT_ARGB12_LE,	/**< bbbbgggg rrrraaaa */
	VBI3_PIXFMT_ARGB12_BE,	/**< */
	VBI3_PIXFMT_ABGR12_LE,	/**< rrrrgggg bbbbaaaa */
	VBI3_PIXFMT_ABGR12_BE,	/**< */

	VBI3_PIXFMT_RGB8,	/**< bbgggrrr */
	VBI3_PIXFMT_BGR8,	/**< rrrgggbb */

	VBI3_PIXFMT_RGBA7,	/**< abbgggrr */
	VBI3_PIXFMT_BGRA7,	/**< arrgggbb */
	VBI3_PIXFMT_ARGB7,	/**< bbgggrra */
	VBI3_PIXFMT_ABGR7	/**< rrgggbba */
} vbi3_pixfmt;

#define VBI3_MAX_PIXFMTS 64

/**
 * A set of pixel formats is used where more than one
 * format may apply. Use VBI3_PIXFMT_SET macros
 * to build a set.
 */
typedef uint64_t vbi3_pixfmt_set;

#define VBI3_PIXFMT_SET(pixfmt) (((vbi3_pixfmt_set) 1) << (pixfmt))

#define VBI3_PIXFMT_SET_UNKNOWN 0
#define VBI3_PIXFMT_SET_EMPTY 0
#define VBI3_PIXFMT_SET_YUV_PLANAR (VBI3_PIXFMT_SET (VBI3_PIXFMT_YUV444) |	\
				   VBI3_PIXFMT_SET (VBI3_PIXFMT_YVU444) |	\
				   VBI3_PIXFMT_SET (VBI3_PIXFMT_YUV422) |	\
				   VBI3_PIXFMT_SET (VBI3_PIXFMT_YVU422) |	\
				   VBI3_PIXFMT_SET (VBI3_PIXFMT_YUV411) |	\
				   VBI3_PIXFMT_SET (VBI3_PIXFMT_YVU411) |	\
				   VBI3_PIXFMT_SET (VBI3_PIXFMT_YUV420) |	\
				   VBI3_PIXFMT_SET (VBI3_PIXFMT_YVU420) |	\
				   VBI3_PIXFMT_SET (VBI3_PIXFMT_YUV410) |	\
				   VBI3_PIXFMT_SET (VBI3_PIXFMT_YVU410))
#define VBI3_PIXFMT_SET_YUVA24	(VBI3_PIXFMT_SET (VBI3_PIXFMT_YUVA24_LE) | \
				 VBI3_PIXFMT_SET (VBI3_PIXFMT_YUVA24_BE) | \
				 VBI3_PIXFMT_SET (VBI3_PIXFMT_YVUA24_LE) | \
				 VBI3_PIXFMT_SET (VBI3_PIXFMT_YVUA24_BE))
#define VBI3_PIXFMT_SET_YUV24	(VBI3_PIXFMT_SET (VBI3_PIXFMT_YUV24_LE) |	\
				 VBI3_PIXFMT_SET (VBI3_PIXFMT_YUV24_BE) |	\
				 VBI3_PIXFMT_SET (VBI3_PIXFMT_YVU24_LE) |	\
				 VBI3_PIXFMT_SET (VBI3_PIXFMT_YVU24_BE))
#define VBI3_PIXFMT_SET_YUV16	(VBI3_PIXFMT_SET (VBI3_PIXFMT_YUYV) |	\
				 VBI3_PIXFMT_SET (VBI3_PIXFMT_YVYU) |	\
				 VBI3_PIXFMT_SET (VBI3_PIXFMT_UYVY) |	\
				 VBI3_PIXFMT_SET (VBI3_PIXFMT_VYUY))
#define VBI3_PIXFMT_SET_YUV_PACKED (VBI3_PIXFMT_SET_YUVA24 |		\
				   VBI3_PIXFMT_SET_YUV24 |		\
				   VBI3_PIXFMT_SET_YUV16	|		\
				   VBI3_PIXFMT_SET (VBI3_PIXFMT_Y8))
#define VBI3_PIXFMT_SET_YUV	(VBI3_PIXFMT_SET_YUV_PLANAR |		\
				 VBI3_PIXFMT_SET_YUV_PACKED)
#define VBI3_PIXFMT_SET_RGBA24	(VBI3_PIXFMT_SET (VBI3_PIXFMT_RGBA24_LE) | \
				 VBI3_PIXFMT_SET (VBI3_PIXFMT_RGBA24_BE) | \
				 VBI3_PIXFMT_SET (VBI3_PIXFMT_BGRA24_LE) | \
				 VBI3_PIXFMT_SET (VBI3_PIXFMT_BGRA24_BE))
#define VBI3_PIXFMT_SET_RGB24	(VBI3_PIXFMT_SET (VBI3_PIXFMT_RGB24_LE) |	\
				 VBI3_PIXFMT_SET (VBI3_PIXFMT_BGR24_LE))
#define VBI3_PIXFMT_SET_RGB16	(VBI3_PIXFMT_SET (VBI3_PIXFMT_RGB16_LE) |	\
				 VBI3_PIXFMT_SET (VBI3_PIXFMT_RGB16_BE) |	\
				 VBI3_PIXFMT_SET (VBI3_PIXFMT_BGR16_LE) |	\
				 VBI3_PIXFMT_SET (VBI3_PIXFMT_BGR16_BE))
#define VBI3_PIXFMT_SET_RGB15	(VBI3_PIXFMT_SET (VBI3_PIXFMT_RGBA15_LE) | \
				 VBI3_PIXFMT_SET (VBI3_PIXFMT_RGBA15_BE) | \
				 VBI3_PIXFMT_SET (VBI3_PIXFMT_BGRA15_LE) | \
				 VBI3_PIXFMT_SET (VBI3_PIXFMT_BGRA15_BE) | \
				 VBI3_PIXFMT_SET (VBI3_PIXFMT_ARGB15_LE) | \
				 VBI3_PIXFMT_SET (VBI3_PIXFMT_ARGB15_BE) | \
				 VBI3_PIXFMT_SET (VBI3_PIXFMT_ABGR15_LE) | \
				 VBI3_PIXFMT_SET (VBI3_PIXFMT_ABGR15_BE))
#define VBI3_PIXFMT_SET_RGB12	(VBI3_PIXFMT_SET (VBI3_PIXFMT_RGBA12_LE) | \
				 VBI3_PIXFMT_SET (VBI3_PIXFMT_RGBA12_BE) | \
				 VBI3_PIXFMT_SET (VBI3_PIXFMT_BGRA12_LE) | \
				 VBI3_PIXFMT_SET (VBI3_PIXFMT_BGRA12_BE) | \
				 VBI3_PIXFMT_SET (VBI3_PIXFMT_ARGB12_LE) | \
				 VBI3_PIXFMT_SET (VBI3_PIXFMT_ARGB12_BE) | \
				 VBI3_PIXFMT_SET (VBI3_PIXFMT_ABGR12_LE) | \
				 VBI3_PIXFMT_SET (VBI3_PIXFMT_ABGR12_BE))
#define VBI3_PIXFMT_SET_RGB8	(VBI3_PIXFMT_SET (VBI3_PIXFMT_RGB8) |	\
				 VBI3_PIXFMT_SET (VBI3_PIXFMT_BGR8))
#define VBI3_PIXFMT_SET_RGB7	(VBI3_PIXFMT_SET (VBI3_PIXFMT_RGBA7) |	\
				 VBI3_PIXFMT_SET (VBI3_PIXFMT_BGRA7) |	\
				 VBI3_PIXFMT_SET (VBI3_PIXFMT_ARGB7) |	\
				 VBI3_PIXFMT_SET (VBI3_PIXFMT_ABGR7))
#define VBI3_PIXFMT_SET_RGB_PACKED (VBI3_PIXFMT_SET_RGBA24 |		\
				   VBI3_PIXFMT_SET_RGB24	|		\
				   VBI3_PIXFMT_SET_RGB16	|		\
				   VBI3_PIXFMT_SET_RGB15	|		\
				   VBI3_PIXFMT_SET_RGB12	|		\
				   VBI3_PIXFMT_SET_RGB8 |		\
				   VBI3_PIXFMT_SET_RGB7)
#define VBI3_PIXFMT_SET_RGB	(VBI3_PIXFMT_SET_RGB_PACKED)
#define VBI3_PIXFMT_SET_PLANAR	(VBI3_PIXFMT_SET_YUV_PLANAR)
#define VBI3_PIXFMT_SET_PACKED	(VBI3_PIXFMT_SET_YUV_PACKED |		\
				 VBI3_PIXFMT_SET_RGB_PACKED)
#define VBI3_PIXFMT_SET_ALL	(VBI3_PIXFMT_SET_YUV |			\
				 VBI3_PIXFMT_SET_RGB)

#define VBI3_PIXFMT_IS_YUV(pixfmt)					\
	(0 != (VBI3_PIXFMT_SET (pixfmt) & VBI3_PIXFMT_SET_YUV))
#define VBI3_PIXFMT_IS_RGB(pixfmt)					\
	(0 != (VBI3_PIXFMT_SET (pixfmt) & VBI3_PIXFMT_SET_RGB))
#define VBI3_PIXFMT_IS_PLANAR(pixfmt)					\
	(0 != (VBI3_PIXFMT_SET (pixfmt) & VBI3_PIXFMT_SET_PLANAR))
#define VBI3_PIXFMT_IS_PACKED(pixfmt)					\
	(0 != (VBI3_PIXFMT_SET (pixfmt) & VBI3_PIXFMT_SET_PACKED))

#ifndef DOXYGEN_SHOULD_SKIP_THIS

#define _VBI3_PIXFMT_SET_4	(VBI3_PIXFMT_SET_YUVA24 |		\
				 VBI3_PIXFMT_SET_RGBA24)
#define _VBI3_PIXFMT_SET_3	(VBI3_PIXFMT_SET_YUV24 |			\
	 			 VBI3_PIXFMT_SET_RGB24)
#define _VBI3_PIXFMT_SET_2	(VBI3_PIXFMT_SET_YUV16 |			\
				 VBI3_PIXFMT_SET_RGB16 |			\
				 VBI3_PIXFMT_SET_RGB15 |			\
				 VBI3_PIXFMT_SET_RGB12)
#define _VBI3_PIXFMT_SET_1	(VBI3_PIXFMT_SET_YUV_PLANAR |		\
				 VBI3_PIXFMT_SET (VBI3_PIXFMT_Y8) |	\
				 VBI3_PIXFMT_SET_RGB8 |			\
				 VBI3_PIXFMT_SET_RGB7)
#endif

#ifdef __GNUC__
#define vbi3_pixfmt_bytes_per_pixel(pixfmt)				\
	(!__builtin_constant_p (pixfmt) ?				\
	 _vbi3_pixfmt_bytes_per_pixel (pixfmt) :				\
	  ((VBI3_PIXFMT_SET (pixfmt) & _VBI3_PIXFMT_SET_4) ? 4U :		\
	   ((VBI3_PIXFMT_SET (pixfmt) & _VBI3_PIXFMT_SET_3) ? 3U :	\
	    ((VBI3_PIXFMT_SET (pixfmt) & _VBI3_PIXFMT_SET_2) ? 2U :	\
	     ((VBI3_PIXFMT_SET (pixfmt) & _VBI3_PIXFMT_SET_1) ? 1U :	\
	      0U)))))
#else
/**
 * @param pixfmt Pixel format.
 *
 * Returns the number of bytes per pixel used by a pixel format.
 * For planar YUV formats (which may take a fraction of bytes
 * on average) the result is 1.
 * 
 * @return
 * Number of bytes per pixel, 0 if @a pixfmt is invalid.
 */
#define vbi3_pixfmt_bytes_per_pixel(pixfmt)				\
	(_vbi3_pixfmt_bytes_per_pixel (pixfmt))
#endif

extern const char *
vbi3_pixfmt_name			(vbi3_pixfmt		pixfmt)
  __attribute__ ((const));

#ifndef DOXYGEN_SHOULD_SKIP_THIS

extern unsigned int
_vbi3_pixfmt_bytes_per_pixel	(vbi3_pixfmt		pixfmt)
  __attribute__ ((const));

#endif

/** Color space identifier. No values defined yet. */
typedef enum {
	VBI3_COLOR_SPACE_NONE,				/**< */
	VBI3_COLOR_SPACE_UNKNOWN = VBI3_COLOR_SPACE_NONE,	/**< */
} vbi3_color_space;

/** This structure describes an image buffer. */
typedef struct {
	/**
	 * Image width in pixels, for planar formats this refers to
	 * the Y plane and must be a multiple of vbi3_pixel_format.uv_hscale.
	 */
	unsigned int		width;

	/**
	 * Image height in pixels, for planar formats this refers to
	 * the Y plane and must be a multiple of tv_pixel_format.uv_vscale.
	 */
	unsigned int		height;

	/**
	 * For packed formats bytes_per_line >= (width *
	 * vbi3_pixel_format.bits_per_pixel + 7) / 8. For planar formats
	 * this refers to the Y plane only, with implied y_size =
	 * bytes_per_line * height.
	 */
	unsigned long		bytes_per_line;

	/** For planar formats only, refers to the U and V plane. */
	unsigned long		uv_bytes_per_line;

	/**
	 * For packed formats the image offset in bytes from the buffer
	 * start. For planar formats this refers to the Y plane.
	 */
	unsigned long		offset;

	/**
	 * For planar formats only, the byte offset of the U and V
	 * plane from the start of the buffer.
	 */
	unsigned long		u_offset;
	unsigned long		v_offset;

	/**
	 * Buffer size. For packed formats size >= offset + height *
	 * bytes_per_line. For planar formats size >=
	 * MAX (offset + y_size, u_offset + uv_size, v_offset + uv_size).
	 */
	unsigned long		size;

	/** Pixel format used by the buffer. */
	vbi3_pixfmt		pixfmt;

	/** Color space used by the buffer. */
	vbi3_color_space		color_space;
} vbi3_image_format;

/** @} */

VBI3_END_DECLS

#endif /* __ZVBI3_IMAGE_FORMAT_H__ */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
