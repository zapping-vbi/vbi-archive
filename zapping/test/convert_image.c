/*
 *  Copyright (C) 2006 Michael H. Schimek
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

/* $Id: convert_image.c,v 1.8 2006-06-13 13:13:02 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#define _GNU_SOURCE 1
#undef NDEBUG

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <math.h>		/* lrint() */
#include "libtv/cpu.h"
#include "libtv/rgb2rgb.h"
#include "libtv/yuv2rgb.h"
#include "libtv/yuv2yuv.h"
#include "libtv/misc.h"		/* FFS() */
#include "guard.h"

#ifndef HAVE_LRINT

static long
lrint				(double			x)
{
	if (x < 0)
		return (long)(x - 0.5);
	else
		return (long)(x + 0.5);
}

#endif

#define ERASE(var) memset (&(var), 0xAA, sizeof (var))

tv_bool				fast_check = TRUE;

unsigned int			buffer_size;

/* buffer_size bytes,
   where data before and after is inaccessible. */
uint8_t *			src_buffer;
uint8_t *			src_buffer_end;

/* Ditto. */
uint8_t *			dst_buffer;
uint8_t *			dst_buffer_end;

unsigned int			scatter_width;
unsigned int			scatter_height;

/* scatter_width * scatter_height bytes,
   where all odd rows and row -1 are inaccessible. */
uint8_t *			src_scatter_buffer;
uint8_t *			src_scatter_buffer_end;

uint8_t *			dst_scatter_buffer;
uint8_t *			dst_scatter_buffer_end;

tv_image_format			src_format;
tv_image_format			dst_format;

tv_bool				erase_dst = 1;

/* 0xAABBGGRR or 0xAAVVUUYY, lsbs filled up with zero bits. */
static unsigned int
get_pixel			(const uint8_t *	src,
				 const tv_image_format *format,
				 unsigned int		x,
				 unsigned int		y)
{
	const tv_pixel_format *pf;
	const uint8_t *s0;
	const uint8_t *s1;
	const uint8_t *s2;
	unsigned int block;
	unsigned int byte;
	unsigned int x2;
	unsigned int y2;
	unsigned int t;

	pf = format->pixel_format;
	s0 = src + y * format->bytes_per_line[0] + format->offset[0];
	byte = (x * pf->bits_per_pixel) >> 3;

	switch (format->pixel_format->pixfmt) {
	case TV_PIXFMT_NV12:
		x2 = x & ~1;
		y2 = y >> pf->uv_vshift;
		s1 = src + y2 * format->bytes_per_line[1] + format->offset[1];
		return (+ s0[x]
			+ (s1[x2 + 0] << 8)
			+ (s1[x2 + 1] << 16)
			+ 0xFF000000);

	case TV_PIXFMT_HM12:
		block = y / (format->bytes_per_line[0] * 16) + (x / 16);
		s0 = src + format->offset[0]
			+ block * (16 * 16) + (y % 16) * 16;

		y2 = y >> 1;
		block = y2 / (format->bytes_per_line[0] * 16) + (x / 16);
		s1 = src + format->offset[1]
			+ block * (16 * 16) + (y2 % 16) * 16;

		x2 = x & 14;
		return (+ s0[x % 16]
			+ (s1[x2 + 0] << 8)
			+ (s1[x2 + 1] << 16)
			+ 0xFF000000);

	case TV_PIXFMT_YUV444:
	case TV_PIXFMT_YUV422:
	case TV_PIXFMT_YUV411:
	case TV_PIXFMT_YUV420:
	case TV_PIXFMT_YUV410:
		x2 = x >> pf->uv_hshift;
		y2 = y >> pf->uv_vshift;
		s1 = src + y2 * format->bytes_per_line[1] + format->offset[1];
		s2 = src + y2 * format->bytes_per_line[2] + format->offset[2];
		return s0[x] + (s1[x2] << 8) + (s2[x2] << 16) + 0xFF000000;

	case TV_PIXFMT_YVU444:
	case TV_PIXFMT_YVU422:
	case TV_PIXFMT_YVU411:
	case TV_PIXFMT_YVU420:
	case TV_PIXFMT_YVU410:
		x2 = x >> pf->uv_hshift;
		y2 = y >> pf->uv_vshift;
		s1 = src + y2 * format->bytes_per_line[1] + format->offset[1];
		s2 = src + y2 * format->bytes_per_line[2] + format->offset[2];
		return s0[x] + (s2[x2] << 8) + (s1[x2] << 16) + 0xFF000000;

#define YUYV(y0, u)							\
	(x2 = (x & ~1) * 2,						\
	 + (s0[x * 2 + y0] << 0)					\
	 + (s0[x2 + u] << 8)						\
	 + (s0[x2 + (u ^ 2)] << 16)					\
	 + 0xFF000000)
	case TV_PIXFMT_YUYV: return YUYV (0, 1);
	case TV_PIXFMT_UYVY: return YUYV (1, 0);
	case TV_PIXFMT_YVYU: return YUYV (0, 3);
	case TV_PIXFMT_VYUY: return YUYV (1, 2);		

#define RGB32(r, g, b, a)						\
	(+ (s0[byte + r] << 0)						\
	 + (s0[byte + g] << 8)						\
	 + (s0[byte + b] << 16)						\
	 + (s0[byte + a] << 24))
	case TV_PIXFMT_RGBA32_LE: return RGB32 (0, 1, 2, 3);
	case TV_PIXFMT_RGBA32_BE: return RGB32 (3, 2, 1, 0);
	case TV_PIXFMT_BGRA32_LE: return RGB32 (2, 1, 0, 3);
	case TV_PIXFMT_BGRA32_BE: return RGB32 (1, 2, 3, 0);

#define RGB24(r, g, b)							\
	(+ (s0[byte + r] << 0)						\
	 + (s0[byte + g] << 8)						\
	 + (s0[byte + b] << 16)						\
	 + 0xFF000000)
	case TV_PIXFMT_RGB24_LE: return RGB24 (0, 1, 2);
	case TV_PIXFMT_BGR24_LE: return RGB24 (2, 1, 0);

#define RGB16(r, g, b, e)						\
	(t = s0[byte + 0 + e] + s0[byte + 1 - e] * 256,			\
	 + MASKED_SHIFT (t, r, 7)					\
	 + MASKED_SHIFT (t, g, 15)					\
	 + MASKED_SHIFT (t, b, 23)					\
	 + 0xFF000000)
	case TV_PIXFMT_RGB16_LE: return RGB16 (0x001F, 0x07E0, 0xF800, 0);
	case TV_PIXFMT_RGB16_BE: return RGB16 (0x001F, 0x07E0, 0xF800, 1);
	case TV_PIXFMT_BGR16_LE: return RGB16 (0xF800, 0x07E0, 0x001F, 0);
	case TV_PIXFMT_BGR16_BE: return RGB16 (0xF800, 0x07E0, 0x001F, 1);

#define RGBA16(r, g, b, a, e)						\
	(t = s0[byte + 0 + e] + s0[byte + 1 - e] * 256,			\
	 + MASKED_SHIFT (t, r, 7)					\
	 + MASKED_SHIFT (t, g, 15)					\
	 + MASKED_SHIFT (t, b, 23)					\
	 + MASKED_SHIFT (t, a, 31))
#define CASE_RGB16(fmt, r, g, b, a)					\
	case TV_PIXFMT_ ## fmt ## _LE: return RGBA16 (r, g, b, a, 0);	\
	case TV_PIXFMT_ ## fmt ## _BE: return RGBA16 (r, g, b, a, 1);

	CASE_RGB16 (RGBA16, 0x001F, 0x03E0, 0x7C00, 0x8000);
	CASE_RGB16 (BGRA16, 0x7C00, 0x03E0, 0x001F, 0x8000);
	CASE_RGB16 (ARGB16, 0x003E, 0x07C0, 0xF800, 0x0001);
	CASE_RGB16 (ABGR16, 0xF800, 0x07C0, 0x003E, 0x0001);

	CASE_RGB16 (RGBA12, 0x000F, 0x00F0, 0x0F00, 0xF000);
	CASE_RGB16 (BGRA12, 0x0F00, 0x00F0, 0x000F, 0xF000);
	CASE_RGB16 (ARGB12, 0x00F0, 0x0F00, 0xF000, 0x000F);
	CASE_RGB16 (ABGR12, 0xF000, 0x0F00, 0x00F0, 0x000F);

#define RGB8(r, g, b)							\
	(+ MASKED_SHIFT (s0[byte], r, 7)				\
	 + MASKED_SHIFT (s0[byte], g, 15)				\
	 + MASKED_SHIFT (s0[byte], b, 23)				\
	 + 0xFF000000)
	case TV_PIXFMT_RGB8: return RGB8 (0x07, 0x38, 0xC0);
	case TV_PIXFMT_BGR8: return RGB8 (0xE0, 0x1C, 0x03);

#define RGBA8(r, g, b, a)						\
	(+ MASKED_SHIFT (s0[byte], r, 7)				\
	 + MASKED_SHIFT (s0[byte], g, 15)				\
	 + MASKED_SHIFT (s0[byte], b, 23)				\
	 + MASKED_SHIFT (s0[byte], a, 31))
	case TV_PIXFMT_RGBA8: return RGBA8 (0x03, 0x1C, 0x60, 0x80);
	case TV_PIXFMT_BGRA8: return RGBA8 (0x60, 0x1C, 0x03, 0x80);
	case TV_PIXFMT_ARGB8: return RGBA8 (0x06, 0x38, 0xC0, 0x01);
	case TV_PIXFMT_ABGR8: return RGBA8 (0xC0, 0x38, 0x06, 0x01);

	case TV_PIXFMT_SBGGR:
	{
		unsigned int bpl = format->bytes_per_line[0];
		const uint8_t *u, *d;
		unsigned int l, r, h, v;

		t = s0[x];
		--s0;

		l = (x <= 0) * 2;
		r = (x < format->width - 1) * 2;

		h = s0[x + l] + s0[x + r] + 1;

		u = (y <= 0) ? s0 + bpl : s0 - bpl;
		d = (y < format->height - 1) ? s0 + bpl : s0 - bpl;

		v = u[x + 1] + d[x + 1] + 1;

		if ((x ^ y) & 1) {
			h >>= 1;
			v >>= 1;

			if (y & 1)
				return h + (t << 8) + (v << 16) + 0xFF000000;
			else
				return v + (t << 8) + (h << 16) + 0xFF000000;
		} else {
			/* Should average three pixels in some border
			   cases but this is easier and faster in SIMD. */
			h = (h + v) >> 2;
			v = (u[x + l] + d[x + l] +
			     u[x + r] + d[x + r] + 2) >> 2;

			if (x & 1)
				return t + (h << 8) + (v << 16) + 0xFF000000;
			else
				return v + (h << 8) + (t << 16) + 0xFF000000;
		}
	}

	default:
		assert (0);
		return 0;
	}
}

static unsigned int
get_avg_pixel			(const uint8_t *	src,
				 const tv_image_format *format,
				 unsigned int		x,
				 unsigned int		y,
				 unsigned int		hshift,
				 unsigned int		vshift)
{
	unsigned int n;
	unsigned int sum0;
	unsigned int sum1;
	unsigned int mask;
	unsigned int i;
	unsigned int j;

	if (0 == (hshift | vshift))
		return get_pixel (src, format, x, y);

	sum0 = 0;
	sum1 = 0;
	mask = 0xFF00FF;

	if (TV_PIXFMT_IS_YUV (format->pixel_format->pixfmt)) {
		n = get_pixel (src, format, x, y);
		sum0 = (n & 0xFF) << (hshift + vshift);
		mask = 0xFF0000;
	}

	x &= ~((1 << hshift) - 1);
	y &= ~((1 << vshift) - 1);

	for (j = 0; j < 1U << vshift; ++j) {
		for (i = 0; i < 1U << hshift; ++i) {
			n = get_pixel (src, format, x + i, y + j);
			sum0 += n & mask;
			sum1 += (n >> 8) & 0xFF00FF;
		}
	}

	n = (0x010001 << (hshift + vshift)) >> 1;
	sum0 = (sum0 + n) >> (hshift + vshift);
	sum1 = (sum1 + n) >> (hshift + vshift);

	return (sum0 & 0xFF00FF) | ((sum1 & 0xFF00FF) << 8);
}

static void
compare_images_straight		(const uint8_t *	dst,
				 const tv_image_format *dst_format,
				 const uint8_t *	src,
				 const tv_image_format *src_format,
				 unsigned int		and_mask,
				 unsigned int		d_mask,
				 unsigned int		s_mask)
{
	const tv_pixel_format *dst_pf;
	const tv_pixel_format *src_pf;
	unsigned int x;
	unsigned int y;

	dst_pf = dst_format->pixel_format;
	src_pf = src_format->pixel_format;

	for (y = 0; y < dst_format->height; ++y) {
		for (x = 0; x < dst_format->width; ++x) {
			unsigned int d, s;

			d = get_avg_pixel (dst, dst_format, x, y,
					   src_pf->uv_hshift,
					   src_pf->uv_vshift);
			s = get_avg_pixel (src, src_format, x, y,
					   dst_pf->uv_hshift,
					   dst_pf->uv_vshift);

			if ((d | d_mask) != ((s & and_mask) | s_mask)) {
				fprintf (stderr,
					 "x=%u y=%u d=%08x s=%08x "
					 "%08x %08x %08x\n",
					 x, y, d, s,
					 d_mask, s_mask, and_mask);
				assert (0);
			}
		}
	}

}

static void
compare_images_yuv2rgb		(const uint8_t *	dst,
				 const tv_image_format *dst_format,
				 const uint8_t *	src,
				 const tv_image_format *src_format,
				 unsigned int		and_mask,
				 unsigned int		d_mask,
				 unsigned int		s_mask)
{
	const tv_pixel_format *dst_pf;
	const tv_pixel_format *src_pf;
	unsigned int x;
	unsigned int y;

	dst_pf = dst_format->pixel_format;
	src_pf = src_format->pixel_format;

	for (y = 0; y < dst_format->height; ++y) {
		for (x = 0; x < dst_format->width; ++x) {
			unsigned int d, s;
			double Y, U, V;
			long int r, g, b;
			long int r_min, g_min, b_min;
			long int r_max, g_max, b_max;

			d = get_pixel (dst, dst_format, x, y);
			s = get_pixel (src, src_format, x, y);

			Y = ((int)((s >> 0) & 0xFF) - 16) * 255 / 219.0;
			U = ((int)((s >> 8) & 0xFF) - 128) * 255 / 224.0;
			V = ((int)((s >> 16) & 0xFF) - 128) * 255 / 224.0;

			r = lrint (Y             + 1.402 * V);
			g = lrint (Y - 0.344 * U - 0.714 * V);
			b = lrint (Y + 1.772 * U);

			r_min = SATURATE (r - 2, 0L, 255L) & (and_mask >> 0);
			r_max = SATURATE (r + 2, 0L, 255L) & (and_mask >> 0);

			g_min = SATURATE (g - 3, 0L, 255L) & (and_mask >> 8);
			g_max = SATURATE (g + 3, 0L, 255L) & (and_mask >> 8);

			b_min = SATURATE (b - 2, 0L, 255L) & (and_mask >> 16);
			b_max = SATURATE (b + 2, 0L, 255L) & (and_mask >> 16);

			if (0 != (0xFF000000 & (d ^ and_mask))
			    || (long int)((d >> 0) & 0xFF) < r_min
			    || (long int)((d >> 0) & 0xFF) > r_max
			    || (long int)((d >> 8) & 0xFF) < g_min
			    || (long int)((d >> 8) & 0xFF) > g_max
			    || (long int)((d >> 16) & 0xFF) < b_min
			    || (long int)((d >> 16) & 0xFF) > b_max) {
				fprintf (stderr,
					 "x=%u y=%u d=%08x s=%08x "
					 "%02lx-%02lx %02lx-%02lx %02lx-%02lx "
					 "%08x %08x %08x\n",
					 x, y, d, s,
					 r_min, r_max,
					 g_min, g_max,
					 b_min, b_max,
					 d_mask, s_mask, and_mask);
				assert (0);
			}
		}
	}
}

static void
compare_images			(const uint8_t *	dst,
				 const tv_image_format *dst_format,
				 const uint8_t *	src,
				 const tv_image_format *src_format)
{
	const tv_pixel_format *dst_pf;
	const tv_pixel_format *src_pf;
	tv_image_format format;
	unsigned int and_mask;
	unsigned int d_mask;
	unsigned int s_mask;

	dst_pf = dst_format->pixel_format;
	src_pf = src_format->pixel_format;

	and_mask = -1;
	CLEAR (format);
	format.pixel_format = dst_format->pixel_format;
	and_mask = get_pixel ((void *) &and_mask, &format, 0, 0);

	d_mask = 0;
	s_mask = 0;

	if ((0 == dst_pf->mask.rgb.a) != (0 == src_pf->mask.rgb.a)) {
		d_mask = 0xFF000000;
		s_mask = 0xFF000000;
	}

	if (TV_PIXFMT_IS_RGB (dst_pf->pixfmt)
	    == TV_PIXFMT_IS_RGB (src_pf->pixfmt)) {
		compare_images_straight (dst, dst_format,
					 src, src_format,
					 and_mask, d_mask, s_mask);
	} else if (TV_PIXFMT_IS_RGB (dst_pf->pixfmt)) {
		compare_images_yuv2rgb (dst, dst_format,
					src, src_format,
					and_mask, d_mask, s_mask);
	} else {
		assert (0);
	}
}

static void
randomize			(uint8_t *		begin,
				 uint8_t *		end)
{
	assert (NULL != begin);
	assert (end >= begin);

	for (; begin < end; ++begin)
		*begin = rand ();
}

static void
assert_is_aa			(const uint8_t *	begin,
				 const uint8_t *	end)
{
	assert (NULL != begin);
	assert (NULL != end);

	if (0 == (((unsigned long) begin |
		   (unsigned long) end) & 3)) {
		/* Faster. */
		for (; begin < end; begin += 4)
			assert (0xAAAAAAAA == * (const uint32_t *) begin);
	} else {
		for (; begin < end; ++begin)
			assert (0xAA == *begin);
	}
}

/* Must not write padding bytes before or after the image
   or between lines. */
static void
check_padding			(const tv_image_format *format)
{
	const tv_pixel_format *pf;
	unsigned int order[3];
	unsigned int bw[3];
	const uint8_t *p;
	const uint8_t *end;
	unsigned int i;

	pf = format->pixel_format;

	bw[0] = (format->width * pf->bits_per_pixel) >> 3;
	bw[1] = format->width >> pf->uv_hshift;
	bw[2] = bw[1];

	if (pf->n_planes > 2) {
		order[0] = (format->offset[1] < format->offset[0]);

		if (format->offset[2] < format->offset[order[0] ^ 1]) {
			order[2] = order[0] ^ 1;
			if (format->offset[2] < format->offset[order[0]]) {
				order[1] = order[0];
				order[0] = 2;
			} else {
				order[1] = 2;
			}
		} else {
			order[1] = order[0] ^ 1;
			order[2] = 2;
		}
	} else if (pf->n_planes > 1) {
		order[0] = (format->offset[1] < format->offset[0]);
		order[1] = order[0] ^ 1;
	} else {
		order[0] = 0;
	}

	p = dst_buffer;

	for (i = 0; i < pf->n_planes; ++i) {
		unsigned int count;
		unsigned int j;

		j = order[i];

		end = dst_buffer + format->offset[j];
		assert_is_aa (p, end);
		p = end;

		for (count = format->height - 1; count > 0; --count) {
			end = p + format->bytes_per_line[j];
			assert_is_aa (p + bw[j], end);
			p = end;
		}

		p += bw[j];
	}

	assert_is_aa (p, dst_buffer_end);
}

static unsigned int
min_size			(tv_image_format *	format)
{
	const tv_pixel_format *pf;
	unsigned long end[3];

	assert (NULL != format);

	if (0 == format->width || 0 == format->height)
		return 0;

	pf = format->pixel_format;

	end[0] = format->offset[0]
		+ format->bytes_per_line[0] * (format->height - 1)
		+ ((format->width * pf->bits_per_pixel) >> 3);

	if (pf->n_planes > 2) {
		unsigned int uv_width;
		unsigned int uv_height_m1;

		uv_width = format->width >> pf->uv_hshift;
		uv_height_m1 = (format->height >> pf->uv_vshift) - 1;

		end[1] = format->offset[1] + uv_width
			+ format->bytes_per_line[1] * uv_height_m1;
		end[2] = format->offset[2] + uv_width
			+ format->bytes_per_line[2] * uv_height_m1;

		return MAX (end[0], MAX (end[1], end[2]));
	} else if (pf->n_planes > 1) {
		unsigned int uv_width;
		unsigned int uv_height_m1;

		uv_width = format->width >> pf->uv_hshift;
		uv_height_m1 = (format->height >> pf->uv_vshift) - 1;

		end[1] = format->offset[1] + uv_width
			+ format->bytes_per_line[1] * uv_height_m1;

		return MAX (end[0], end[1]);
	} else {
		return end[0];
	}
}

static unsigned int
bytes_per_pixel			(const tv_image_format *format)
{
	return format->pixel_format->bits_per_pixel >> 3;
}

static unsigned int
byte_width			(const tv_image_format *format,
				 unsigned int		plane)
{
	const tv_pixel_format *pf;

	pf = format->pixel_format;

	if (0 == plane)
		return (format->width * pf->bits_per_pixel) >> 3;
	else
		return format->width >> pf->uv_hshift;
}

#define RES(sh) ((1U << (sh)) - 1)

static void
test				(uint8_t *		dst,
				 const uint8_t *	src)
{
	const tv_pixel_format *dst_pf;
	const tv_pixel_format *src_pf;
	tv_image_format md_format;
	tv_image_format ms_format;
	unsigned int hshift;
	unsigned int vshift;
	tv_bool success;

	dst_pf = dst_format.pixel_format;
	src_pf = src_format.pixel_format;

	hshift = MAX (dst_pf->uv_hshift, src_pf->uv_hshift);
	vshift = MAX (dst_pf->uv_vshift, src_pf->uv_vshift);

	md_format = dst_format;
	md_format.width = MIN (dst_format.width, src_format.width);
	md_format.height = MIN (dst_format.height, src_format.height);
	
	if (0)
		fprintf (stderr, "%d %d %d %d\n",
			 md_format.width, md_format.height,
			 hshift, vshift);

	memset (dst_buffer, 0xAA, dst_buffer_end - dst_buffer);

	switch (src_format.pixel_format->pixfmt) {
	case TV_PIXFMT_HM12:
		hshift = 4;
		vshift = 4;
		switch (dst_format.pixel_format->pixfmt) {
		case TV_PIXFMT_YUV420:
		case TV_PIXFMT_YVU420:
			success = _tv_hm12_to_yuv420 (dst, &dst_format,
						      src, &src_format);
			break;
		default:
			assert (0);
		}
		break;
	case TV_PIXFMT_NV12:
		hshift = 1;
		switch (dst_format.pixel_format->pixfmt) {
		case TV_PIXFMT_YUV420:
		case TV_PIXFMT_YVU420:
			success = _tv_nv_to_yuv420 (dst, &dst_format,
						    src, &src_format);
			break;
		case TV_PIXFMT_YUYV:
		case TV_PIXFMT_UYVY:
		case TV_PIXFMT_YVYU:
		case TV_PIXFMT_VYUY:
			hshift = 1;
			success = _tv_nv_to_yuyv (dst, &dst_format,
						  src, &src_format);
			break;
		case TV_PIXFMT_RGBA32_LE:
		case TV_PIXFMT_RGBA32_BE:
		case TV_PIXFMT_BGRA32_LE:
		case TV_PIXFMT_BGRA32_BE:
		case TV_PIXFMT_RGB24_LE:
		case TV_PIXFMT_RGB24_BE:
		case TV_PIXFMT_BGR16_LE:
		case TV_PIXFMT_BGR16_BE:
		case TV_PIXFMT_BGRA16_LE:
		case TV_PIXFMT_BGRA16_BE:
			success = _tv_nv_to_rgb (dst, &dst_format,
						 src, &src_format);
			break;
		default:
			assert (0);
		}
		break;
	case TV_PIXFMT_YUV420:
	case TV_PIXFMT_YVU420:
		switch (dst_format.pixel_format->pixfmt) {
		case TV_PIXFMT_YUV420:
		case TV_PIXFMT_YVU420:
			success = _tv_yuv420_to_yuv420 (dst, &dst_format,
							src, &src_format);
			break;
		case TV_PIXFMT_YUYV:
		case TV_PIXFMT_UYVY:
		case TV_PIXFMT_YVYU:
		case TV_PIXFMT_VYUY:
			hshift = 1;
			success = _tv_yuv420_to_yuyv (dst, &dst_format,
						      src, &src_format);
			break;
		case TV_PIXFMT_RGBA32_LE:
		case TV_PIXFMT_RGBA32_BE:
		case TV_PIXFMT_BGRA32_LE:
		case TV_PIXFMT_BGRA32_BE:
		case TV_PIXFMT_RGB24_LE:
		case TV_PIXFMT_RGB24_BE:
		case TV_PIXFMT_BGR16_LE:
		case TV_PIXFMT_BGR16_BE:
		case TV_PIXFMT_BGRA16_LE:
		case TV_PIXFMT_BGRA16_BE:
			success = _tv_yuv420_to_rgb (dst, &dst_format,
						     src, &src_format);
			break;
		default:
			assert (0);
		}
		break;
	case TV_PIXFMT_YUYV:
	case TV_PIXFMT_UYVY:
	case TV_PIXFMT_YVYU:
	case TV_PIXFMT_VYUY:
		hshift = 1;
		switch (dst_format.pixel_format->pixfmt) {
		case TV_PIXFMT_YUV420:
		case TV_PIXFMT_YVU420:
			success = _tv_yuyv_to_yuv420 (dst, &dst_format,
						      src, &src_format);
			break;
		case TV_PIXFMT_YUYV:
		case TV_PIXFMT_UYVY:
		case TV_PIXFMT_YVYU:
		case TV_PIXFMT_VYUY:
			success = _tv_yuyv_to_yuyv (dst, &dst_format,
						    src, &src_format);
			break;
		case TV_PIXFMT_RGBA32_LE:
		case TV_PIXFMT_RGBA32_BE:
		case TV_PIXFMT_BGRA32_LE:
		case TV_PIXFMT_BGRA32_BE:
		case TV_PIXFMT_RGB24_LE:
		case TV_PIXFMT_RGB24_BE:
		case TV_PIXFMT_BGR16_LE:
		case TV_PIXFMT_BGR16_BE:
		case TV_PIXFMT_BGRA16_LE:
		case TV_PIXFMT_BGRA16_BE:
			success = _tv_yuyv_to_rgb (dst, &dst_format,
						   src, &src_format);
			break;
		default:
			assert (0);
		}
		break;
	case TV_PIXFMT_RGBA32_LE:
	case TV_PIXFMT_RGBA32_BE:
	case TV_PIXFMT_BGRA32_LE:
	case TV_PIXFMT_BGRA32_BE:
	case TV_PIXFMT_RGB24_LE:
	case TV_PIXFMT_RGB24_BE:
		switch (dst_format.pixel_format->pixfmt) {
		case TV_PIXFMT_RGBA32_LE:
		case TV_PIXFMT_RGBA32_BE:
		case TV_PIXFMT_BGRA32_LE:
		case TV_PIXFMT_BGRA32_BE:
		case TV_PIXFMT_RGB24_LE:
		case TV_PIXFMT_RGB24_BE:
			success = _tv_rgb32_to_rgb32 (dst, &dst_format,
						      src, &src_format);
			break;
		case TV_PIXFMT_BGR16_LE:
		case TV_PIXFMT_BGR16_BE:
		case TV_PIXFMT_BGRA16_LE:
		case TV_PIXFMT_BGRA16_BE:
			success = _tv_rgb32_to_rgb16 (dst, &dst_format,
						      src, &src_format);
			break;
		default:
			assert (0);
		}
		break;
	case TV_PIXFMT_SBGGR:
		hshift = 1;
		vshift = 1;
		success = _tv_sbggr_to_rgb (dst, &dst_format,
					    src, &src_format);
		break;
	default:
		assert (0);
	}

#define FAIL_IF(expr)							\
	if (expr) {							\
		assert (!success);					\
		assert_is_aa (dst_buffer, dst_buffer_end);		\
		return;							\
	}

	FAIL_IF (0 == md_format.width);
	FAIL_IF (0 == md_format.height);

	FAIL_IF (md_format.width & RES (hshift));
	FAIL_IF (md_format.height & RES (vshift));

	FAIL_IF (byte_width (&dst_format, 0) > dst_format.bytes_per_line[0]);
	FAIL_IF (byte_width (&src_format, 0) > src_format.bytes_per_line[0]);

	if (dst_pf->n_planes > 2)
		FAIL_IF (byte_width (&dst_format, 2)
			 > dst_format.bytes_per_line[2]);

	if (dst_pf->n_planes > 1)
		FAIL_IF (byte_width (&dst_format, 1)
			 > dst_format.bytes_per_line[1]);

	if (src_pf->n_planes > 2)
		FAIL_IF (byte_width (&src_format, 2)
			 > src_format.bytes_per_line[2]);

	if (src_pf->n_planes > 1)
		FAIL_IF (byte_width (&src_format, 1)
			 > src_format.bytes_per_line[1]);

	assert (success);

	check_padding (&md_format);

	ms_format = src_format;
	/* Required for proper SBGGR clipping. */
	ms_format.width = md_format.width;
	ms_format.height = md_format.height;

	compare_images (dst, &md_format, src, &ms_format);
}

#if 1

#  define overflow_packed() test (dst_buffer, src_buffer)
#  define overflow_planar() test (dst_buffer, src_buffer)

#else

static void
overflow_packed			(void)
{
	test (dst_buffer, src_buffer);

	if (fast_check)
		return;

	if (src_format.size < buffer_size)
		test (dst_buffer,
		      src_buffer_end
		      - ((src_format.size + 15) & -16));

	if (dst_format.size < buffer_size)
		test (dst_buffer_end
		      - ((dst_format.size + 15) & -16),
		      src_buffer);

	if (dst_format.size < buffer_size
	    && src_format.size < buffer_size)
		test (dst_buffer_end
		      - ((dst_format.size + 15) & -16),
		      src_buffer_end
		      - ((src_format.size + 15) & -16));
}

#endif

static void
unaligned_packed			(void)
{
	static int unaligned[][2] = {
		{ 0, 0 },
		{ 5, 0 },
		{ 0, 5 },
		{ 5, 5 },
		{ 8, 0 },
		{ 0, 8 },
		{ 16, 0 },
		{ 0, 16 },
	};
	unsigned int dst_bpp;
	unsigned int src_bpp;
	unsigned int i;
	unsigned int j;

	dst_bpp = bytes_per_pixel (&dst_format);
	src_bpp = bytes_per_pixel (&src_format);

	for (i = 0; i < N_ELEMENTS (unaligned); ++i) {
		for (j = 0; j < N_ELEMENTS (unaligned); ++j) {
			ERASE (dst_format.offset);
			ERASE (dst_format.bytes_per_line);
			dst_format.offset[0] = unaligned[j][0];
			dst_format.bytes_per_line[0] =
				dst_format.width * dst_bpp
				+ unaligned[j][1];
			dst_format.size = min_size (&dst_format);

			ERASE (src_format.offset);
			ERASE (src_format.bytes_per_line);
			src_format.offset[0] = unaligned[i][0];
			src_format.bytes_per_line[0] =
				src_format.width * src_bpp
				+ unaligned[i][1];
			src_format.size = min_size (&src_format);

			overflow_packed ();

			if (fast_check)
				return;
		}
	}

	dst_format.offset[0] = 0;
	dst_format.bytes_per_line[0] = dst_format.width * dst_bpp;
	dst_format.size = min_size (&dst_format);

	src_format.offset[0] = 0;
	src_format.bytes_per_line[0] = src_format.width * src_bpp;
	src_format.size = min_size (&src_format);

	if (dst_format.width >= 16) {
		dst_format.bytes_per_line[0] =
			(dst_format.width - 16) * dst_bpp;

		overflow_packed ();
	}

	if (src_format.width >= 16) {
		dst_format.bytes_per_line[0] = dst_format.width * dst_bpp;
		src_format.bytes_per_line[0] =
			(src_format.width - 16) * src_bpp;

		overflow_packed ();
	}

	/* TO DO: padding = inaccessible page. */
}

static void
unaligned_planar			(void)
{
	static int unaligned[][6] = {
		{ 0, 0, 0, 0, 0, 0 },
		{ 5, 0, 0, 0, 0, 0 },
		{ 0, 5, 0, 0, 0, 0 },
		{ 5, 5, 0, 0, 0, 0 },
		{ 0, 0, 5, 0, 0, 0 },
		{ 0, 0, 0, 5, 0, 0 },
		{ 0, 0, 0, 0, 5, 0 },
		{ 0, 0, 0, 0, 0, 5 },
		{ 8, 0, 8, 0, 8, 0 },
		{ 0, 8, 0, 8, 0, 8 },
		{ 16, 0, 16, 0, 16, 0 },
		{ 0, 16, 0, 16, 0, 16 },
	};
	unsigned int dst_bpp;
	unsigned int src_bpp;
	unsigned int dst_y_bpl;
	unsigned int dst_uv_bpl;
	unsigned int src_y_bpl;
	unsigned int src_uv_bpl;
	unsigned int short_bpl;
	unsigned int i;
	unsigned int j;

	dst_bpp = bytes_per_pixel (&dst_format);
	dst_y_bpl = dst_format.width * dst_bpp;
	dst_uv_bpl = (dst_format.width * dst_bpp)
		>> dst_format.pixel_format->uv_hshift;

	src_bpp = bytes_per_pixel (&src_format);
	src_y_bpl = src_format.width * src_bpp;
	src_uv_bpl = (src_format.width * src_bpp)
		>> src_format.pixel_format->uv_hshift;

	for (i = 0; i < N_ELEMENTS (unaligned); ++i) {
		for (j = 0; j < N_ELEMENTS (unaligned); ++j) {
			ERASE (dst_format.offset);
			ERASE (dst_format.bytes_per_line);
			dst_format.offset[0] = unaligned[j][0];
			dst_format.bytes_per_line[0] = dst_y_bpl
				+ unaligned[j][1];
			if (dst_format.pixel_format->n_planes > 1) {
				dst_format.offset[1] = (dst_y_bpl * 16)
					* (dst_format.height + 1)
					+ unaligned[j][2];
				dst_format.bytes_per_line[1] = dst_uv_bpl
					+ unaligned[j][3];
				dst_format.offset[2] = (dst_y_bpl * 16)
					* (dst_format.height + 1) * 2
					+ unaligned[j][4];
				dst_format.bytes_per_line[2] = dst_uv_bpl
					+ unaligned[j][5];
			}
			dst_format.size = min_size (&dst_format);

			ERASE (src_format.offset);
			ERASE (src_format.bytes_per_line);
			src_format.offset[0] = unaligned[i][0];
			src_format.bytes_per_line[0] =
				src_y_bpl + unaligned[i][1];
			if (src_format.pixel_format->n_planes > 1) {
				src_format.offset[1] = unaligned[i][2];
				src_format.bytes_per_line[1] =
					src_uv_bpl + unaligned[i][3];
				src_format.offset[2] = unaligned[i][4];
				src_format.bytes_per_line[2] =
					src_uv_bpl + unaligned[i][5];
			}
			src_format.size = min_size (&src_format);

			overflow_planar ();

			if (fast_check)
				return;
		}
	}

	dst_format.offset[0] = 2 * dst_y_bpl * dst_format.height;
	dst_format.bytes_per_line[0] = dst_y_bpl;
	if (dst_format.pixel_format->n_planes > 1) {
		dst_format.offset[1] = 1 * dst_y_bpl * dst_format.height;
		dst_format.bytes_per_line[1] = dst_uv_bpl;
		dst_format.offset[2] = 0 * dst_y_bpl * dst_format.height;
		dst_format.bytes_per_line[2] = dst_uv_bpl;
	}
	dst_format.size = min_size (&dst_format);

	src_format.offset[0] = 0;
	src_format.bytes_per_line[0] = src_y_bpl;
	if (dst_format.pixel_format->n_planes > 1) {
		src_format.offset[1] = 0;
		src_format.bytes_per_line[1] = src_uv_bpl;
		src_format.offset[2] = 0;
		src_format.bytes_per_line[2] = src_uv_bpl;
	}
	src_format.size = min_size (&src_format);

	if (dst_format.width >= 16) {
		dst_format.bytes_per_line[0] =
			(dst_format.width - 16) * dst_bpp;
		overflow_planar ();
		dst_format.bytes_per_line[0] = dst_y_bpl;

		if (dst_format.pixel_format->n_planes > 1) {
			short_bpl = ((dst_format.width - 16) * dst_bpp)
				>> dst_format.pixel_format->uv_hshift;
			dst_format.bytes_per_line[1] = short_bpl;
			overflow_planar ();
			dst_format.bytes_per_line[1] = dst_uv_bpl;
			dst_format.bytes_per_line[2] = short_bpl;
			overflow_planar ();
			dst_format.bytes_per_line[2] = dst_uv_bpl;
		}
	}

	dst_format.bytes_per_line[0] = dst_format.width * dst_bpp;

	if (src_format.width >= 16) {
		src_format.bytes_per_line[0] =
			(src_format.width - 16) * src_bpp;
		overflow_planar ();
		src_format.bytes_per_line[0] = src_y_bpl;

		if (dst_format.pixel_format->n_planes > 1) {
			short_bpl = ((src_format.width - 16) * src_bpp)
				>> src_format.pixel_format->uv_hshift;
			src_format.bytes_per_line[1] = short_bpl;
			overflow_planar ();
			src_format.bytes_per_line[1] = src_uv_bpl;
			src_format.bytes_per_line[2] = short_bpl;
			overflow_planar ();
			src_format.bytes_per_line[2] = short_bpl;
		}
	}

	/* TO DO: padding = inaccessible page. */
}

static void
all_sizes			(void)
{
	static unsigned int heights[] = {
		0, 1, 2, 11, 12
	};
	unsigned int i;
	tv_bool planar;

	planar = ((dst_format.pixel_format->n_planes
		   | src_format.pixel_format->n_planes) > 1);

	for (i = 0; i < N_ELEMENTS (heights); ++i) {
		static unsigned int widths[] = {
			0, 1, 3, 8, 13, 16, 21, 155, 160, 165
		};
		unsigned int j;

		if (fast_check)
			src_format.height = 12;
		else
			src_format.height = heights[i];

		for (j = 0; j < N_ELEMENTS (widths); ++j) {
			fputc ('.', stderr);
			fflush (stderr);

			if (fast_check)
				src_format.width = 32;
			else
				src_format.width = widths[j];

			dst_format.width = src_format.width;
			dst_format.height = src_format.height;

			if (planar)
				unaligned_planar ();
			else
				unaligned_packed ();

			if (fast_check)
				return;

			if (src_format.width > 5) {
				dst_format.width = src_format.width - 5;
				dst_format.height = src_format.height;

				if (planar)
					unaligned_planar ();
				else
					unaligned_packed ();
			}

			dst_format.width = src_format.width + 5;
			dst_format.height = src_format.height;

			if (planar)
				unaligned_planar ();
			else
				unaligned_packed ();

			if (src_format.height > 5) {
				dst_format.width = src_format.width;
				dst_format.height = src_format.height - 5;

				if (planar)
					unaligned_planar ();
				else
					unaligned_packed ();
			}

			dst_format.width = src_format.width;
			dst_format.height = src_format.height + 5;

			if (planar)
				unaligned_planar ();
			else
				unaligned_packed ();
		}
	}
}

static void
all_formats			(void)
{
	static const tv_pixfmt src_formats[] = {
		TV_PIXFMT_HM12,
		TV_PIXFMT_NV12,
		TV_PIXFMT_YUV420,
		TV_PIXFMT_YVU420,
		TV_PIXFMT_YUYV,
		TV_PIXFMT_UYVY,
		TV_PIXFMT_YVYU,
		TV_PIXFMT_VYUY,
		TV_PIXFMT_RGBA32_LE,
		TV_PIXFMT_RGBA32_BE,
		TV_PIXFMT_BGRA32_LE,
		TV_PIXFMT_BGRA32_BE,
		TV_PIXFMT_RGB24_LE,
		TV_PIXFMT_RGB24_BE,
		TV_PIXFMT_SBGGR,
	};
	static const tv_pixfmt dst_formats[] = {
		TV_PIXFMT_YUV420,
		TV_PIXFMT_YVU420,
		TV_PIXFMT_YUYV,
		TV_PIXFMT_UYVY,
		TV_PIXFMT_YVYU,
		TV_PIXFMT_VYUY,
		TV_PIXFMT_RGBA32_LE,
		TV_PIXFMT_RGBA32_BE,
		TV_PIXFMT_BGRA32_LE,
		TV_PIXFMT_BGRA32_BE,
		TV_PIXFMT_RGB24_LE,
		TV_PIXFMT_RGB24_BE,
		TV_PIXFMT_BGR16_LE,
		TV_PIXFMT_BGR16_BE,
		TV_PIXFMT_BGRA16_LE,
		TV_PIXFMT_BGRA16_BE,
	};
	tv_image_format format;
	unsigned int i;
	unsigned int j;

	CLEAR (format);

	for (i = 0; i < N_ELEMENTS (src_formats); ++i) {
		for (j = 0; j < N_ELEMENTS (dst_formats); ++j) {
			if (TV_PIXFMT_IS_RGB (src_formats[i])
			    && !TV_PIXFMT_IS_RGB (dst_formats[j]))
				continue; /* later */

			if (TV_PIXFMT_HM12 == src_formats[i]
			    && TV_PIXFMT_YUV420 != dst_formats[j]
			    && TV_PIXFMT_YVU420 != dst_formats[j])
				continue; /* later */

			dst_format.pixel_format =
				tv_pixel_format_from_pixfmt (dst_formats[j]);
			src_format.pixel_format =
				tv_pixel_format_from_pixfmt (src_formats[i]);

			if (!fast_check) {
				fprintf (stderr, "%s -> %s ",
					 src_format.pixel_format->name,
					 dst_format.pixel_format->name);
			}

			all_sizes ();

			if (!fast_check)
				fputc ('\n', stderr);
		}
	}
}

int
main				(int			argc,
				 char **		argv)
{
	unsigned int scatter_size;
	unsigned int i;

	buffer_size = 32 * 4096;

	src_buffer = guard_alloc (buffer_size);
	src_buffer_end = src_buffer + buffer_size;

	randomize (src_buffer, src_buffer_end);
	make_unwriteable (src_buffer, buffer_size);

	dst_buffer = guard_alloc (buffer_size);
	dst_buffer_end = dst_buffer + buffer_size;

	scatter_width = getpagesize ();
	scatter_height = 24;
	scatter_size = scatter_width * (scatter_height - 1);

	src_scatter_buffer = guard_alloc (scatter_size);
	src_scatter_buffer_end = src_scatter_buffer + scatter_size;

	for (i = 0; i < scatter_size; i += scatter_width * 2) {
		make_unwriteable (src_scatter_buffer + i, scatter_width);
		make_inaccessible (src_scatter_buffer + i + scatter_width,
				   scatter_width);
	}

	dst_scatter_buffer = guard_alloc (scatter_size);
	dst_scatter_buffer_end = dst_scatter_buffer + scatter_size;

	for (i = 0; i < scatter_size; i += scatter_width * 2) {
		make_inaccessible (src_scatter_buffer + i + scatter_width,
				   scatter_width);
	}

	/* Use generic version. */
	cpu_features = (cpu_feature_set) 0;

	if (argc > 1) {
		/* Use optimized version, if available. */
		cpu_features = cpu_feature_set_from_string (argv[1]);
	}

	all_formats ();

	if (fast_check)
		fputc ('\n', stderr);

	return EXIT_SUCCESS;
}
