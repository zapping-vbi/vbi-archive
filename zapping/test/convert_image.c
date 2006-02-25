/*
 *  Copyright (C) 2004 Michael H. Schimek
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

/* $Id: convert_image.c,v 1.1 2006-02-25 19:49:49 mschimek Exp $ */

#undef NDEBUG

#include <stdio.h>
#include <assert.h>
#include "libtv/cpu.h"
#include "libtv/rgb2rgb.h"
#include "libtv/misc.h"		/* FFS() */
#include "guard.h"

extern tv_bool
_tv_sbggr_to_rgb		(void *			dst_image,
				 const tv_image_format *dst_format,
				 const void *		src_image,
				 const tv_image_format *src_format);

tv_bool				fast = FALSE;

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

unsigned int			and_mask;
unsigned int			or_mask;

/* 0xAABBGGRR or 0xAAVVUUYY, lsbs filled up with zero bits. */
static unsigned int
get_pixel			(const uint8_t *	s,
				 const tv_image_format *format,
				 unsigned int		x,
				 unsigned int		y)
{
	unsigned int t;

	assert (NULL != s);
	assert (NULL != format);

	s += format->offset[0];
	s += y * format->bytes_per_line[0];
	s += (x * format->pixel_format->bits_per_pixel) >> 3;

	switch (format->pixel_format->pixfmt) {
#define RGB32(r, g, b, a)						\
	(+ (s[r] << 0)							\
	 + (s[g] << 8)							\
	 + (s[b] << 16)							\
	 + (s[a] << 24))
	case TV_PIXFMT_RGBA32_LE: return RGB32 (0, 1, 2, 3);
	case TV_PIXFMT_RGBA32_BE: return RGB32 (3, 2, 1, 0);
	case TV_PIXFMT_BGRA32_LE: return RGB32 (2, 1, 0, 3);
	case TV_PIXFMT_BGRA32_BE: return RGB32 (1, 2, 3, 0);

#define RGB24(r, g, b)							\
	(+ (s[r] << 0)							\
	 + (s[g] << 8)							\
	 + (s[b] << 16)							\
	 + 0xFF000000)
	case TV_PIXFMT_RGB24_LE: return RGB24 (0, 1, 2);
	case TV_PIXFMT_BGR24_LE: return RGB24 (2, 1, 0);

#define RGB16(r, g, b, e)						\
	(t = s[0 + e] + s[1 - e] * 256,					\
	 + MASKED_SHIFT (t, r, 7)					\
	 + MASKED_SHIFT (t, g, 15)					\
	 + MASKED_SHIFT (t, b, 23)					\
	 + 0xFF000000)
	case TV_PIXFMT_RGB16_LE: return RGB16 (0x001F, 0x07E0, 0xF800, 0);
	case TV_PIXFMT_RGB16_BE: return RGB16 (0x001F, 0x07E0, 0xF800, 1);
	case TV_PIXFMT_BGR16_LE: return RGB16 (0xF800, 0x07E0, 0x001F, 0);
	case TV_PIXFMT_BGR16_BE: return RGB16 (0xF800, 0x07E0, 0x001F, 1);

#define RGBA16(r, g, b, a, e)						\
	(t = s[0 + e] + s[1 - e] * 256,					\
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
	(+ MASKED_SHIFT (s[0], r, 7)					\
	 + MASKED_SHIFT (s[0], g, 15)					\
	 + MASKED_SHIFT (s[0], b, 23)					\
	 + 0xFF000000)
	case TV_PIXFMT_RGB8: return RGB8 (0x07, 0x38, 0xC0);
	case TV_PIXFMT_BGR8: return RGB8 (0xE0, 0x1C, 0x03);

#define RGBA8(r, g, b, a)						\
	(+ MASKED_SHIFT (s[0], r, 7)					\
	 + MASKED_SHIFT (s[0], g, 15)					\
	 + MASKED_SHIFT (s[0], b, 23)					\
	 + MASKED_SHIFT (s[0], a, 31))
	case TV_PIXFMT_RGBA8: return RGBA8 (0x03, 0x1C, 0x60, 0x80);
	case TV_PIXFMT_BGRA8: return RGBA8 (0x60, 0x1C, 0x03, 0x80);
	case TV_PIXFMT_ARGB8: return RGBA8 (0x06, 0x38, 0xC0, 0x01);
	case TV_PIXFMT_ABGR8: return RGBA8 (0xC0, 0x38, 0x06, 0x01);

	case TV_PIXFMT_SBGGR:
	{
		unsigned int bpl = format->bytes_per_line[0];
		const uint8_t *u, *d;
		unsigned int l, r, h, v;

		t = *s--;

		l = (x <= 0) * 2;
		r = (x < format->width - 1) * 2;

		h = s[l] + s[r] + 1;

		u = (y <= 0) ? s + bpl : s - bpl;
		d = (y < format->height - 1) ? s + bpl : s - bpl;

		v = u[1] + d[1] + 1;

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
			v = (u[l] + d[l] + u[r] + d[r] + 2) >> 2;

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
bytes_per_pixel			(const tv_image_format *format)
{
	assert (NULL != format);
	assert (NULL != format->pixel_format);
	return format->pixel_format->bits_per_pixel >> 3;
}

static unsigned int
byte_width			(const tv_image_format *format)
{
	assert (NULL != format);
	assert (NULL != format->pixel_format);
	return (format->width * format->pixel_format->bits_per_pixel) >> 3;
}

static unsigned int
min_size			(tv_image_format *	format)
{
	assert (NULL != format);

	if (0 == format->width || 0 == format->height)
		return 0;

	return format->offset[0]
		+ format->bytes_per_line[0] * (format->height - 1)
		+ byte_width (format); /* last line */
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
	assert (end >= begin);

	for (; begin < end; ++begin)
		assert (0xAA == *begin);
}

/* Must not write padding bytes before or after the image
   or between lines. */
static void
assert_padding_is_aa		(const uint8_t *	dst,
				 const tv_image_format *format)
{
	const uint8_t *p;
	unsigned int bw;

	p = dst + format->offset[0];

	assert_is_aa (dst_buffer, p);

	bw = byte_width (format);
	p += bw;

	if (format->width > 0) {
		unsigned int padding;
		unsigned int count;

		padding = format->bytes_per_line[0] - bw;

		for (count = 1; count < format->height; ++count) {
			assert_is_aa (p, p + padding);
			p += format->bytes_per_line[0];
		}
	}

	assert_is_aa (p, dst_buffer_end);
}

static void
assert_conversion_ok		(const uint8_t *	dst,
				 const uint8_t *	src,
				 const tv_image_format *dst_format,
				 const tv_image_format *src_format)
{
	unsigned int x;
	unsigned int y;

	for (y = 0; y < dst_format->height; ++y) {
		for (x = 0; x < dst_format->width; ++x) {
			unsigned int a, b;

			a = get_pixel (dst, dst_format, x, y);
			b = get_pixel (src, src_format, x, y);
				
			if (0)
				fprintf (stderr, "x=%3u y=%3u %08x %08x\n",
					 x, y, a, b);

			assert (a == ((b & and_mask) | or_mask));
		}
	}
}

static void
convert_rgb			(uint8_t *		dst,
				 const uint8_t *	src)
{
	tv_image_format md_format;
	tv_image_format ms_format;
	tv_bool success;

	memset (dst_buffer, 0xAA, dst_buffer_end - dst_buffer);

	switch (src_format.pixel_format->pixfmt) {
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
			success = _tv_rgb32_to_rgb32 (dst, &dst_format, src, &src_format);
			break;
		case TV_PIXFMT_BGR16_LE:
		case TV_PIXFMT_BGR16_BE:
		case TV_PIXFMT_BGRA16_LE:
		case TV_PIXFMT_BGRA16_BE:
			success = _tv_rgb32_to_rgb16 (dst, &dst_format, src, &src_format);
			break;
		default:
			assert (0);
		}
		break;
	case TV_PIXFMT_SBGGR:
		success = _tv_sbggr_to_rgb (dst, &dst_format, src, &src_format);
		break;
	default:
		assert (0);
	}

	md_format = dst_format;
	md_format.width = MIN (dst_format.width, src_format.width);
	md_format.height = MIN (dst_format.height, src_format.height);

	if (TV_PIXFMT_SBGGR == src_format.pixel_format->pixfmt) {
		md_format.width &= ~1;
		md_format.height &= ~1;
	}

#define FAIL_IF(expr)							\
	if (expr) {							\
		assert (!success);					\
		assert_is_aa (dst_buffer, dst_buffer_end);		\
		return;							\
	}

	FAIL_IF (0 == md_format.width);
	FAIL_IF (0 == md_format.height);
	FAIL_IF (byte_width (&dst_format) > dst_format.bytes_per_line[0]);
	FAIL_IF (byte_width (&src_format) > src_format.bytes_per_line[0]);

	assert (success);
	assert_padding_is_aa (dst, &md_format);

	ms_format = src_format;
	/* Required for proper SBGGR clipping. */
	ms_format.width = md_format.width;
	ms_format.height = md_format.height;

	assert_conversion_ok (dst, src, &md_format, &ms_format);
}

static void
overflow_rgb			(void)
{
	convert_rgb (dst_buffer, src_buffer);

	if (fast)
		return;

	if (src_format.size < buffer_size)
		convert_rgb (dst_buffer,
			     src_buffer_end - ((src_format.size + 15) & -16));

	if (dst_format.size < buffer_size)
		convert_rgb (dst_buffer_end - ((dst_format.size + 15) & -16),
			     src_buffer);

	if (dst_format.size < buffer_size
	    && src_format.size < buffer_size)
		convert_rgb (dst_buffer_end - ((dst_format.size + 15) & -16),
			     src_buffer_end - ((src_format.size + 15) & -16));
}

static void
unaligned_rgb			(void)
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
			dst_format.offset[0] = unaligned[j][0];
			dst_format.bytes_per_line[0] =
				dst_format.width * dst_bpp
				+ unaligned[j][1];
			dst_format.size = min_size (&dst_format);

			src_format.offset[0] = unaligned[i][0];
			src_format.bytes_per_line[0] =
				src_format.width * src_bpp
				+ unaligned[i][1];
			src_format.size = min_size (&src_format);

			overflow_rgb ();

			if (fast)
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

		overflow_rgb ();
	}

	if (src_format.width >= 16) {
		dst_format.bytes_per_line[0] = dst_format.width * dst_bpp;
		src_format.bytes_per_line[0] =
			(src_format.width - 16) * src_bpp;

		overflow_rgb ();
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

	for (i = 0; i < N_ELEMENTS (heights); ++i) {
		static unsigned int widths[] = {
			0, 1, 3, 8, 13, 16, 21, 155, 160, 165
		};
		unsigned int j;

		if (fast)
			src_format.height = 12;
		else
			src_format.height = heights[i];

		for (j = 0; j < N_ELEMENTS (widths); ++j) {
			fputc ('.', stderr);
			fflush (stderr);

			if (fast)
				src_format.width = 32;
			else
				src_format.width = widths[i];

			dst_format.width = src_format.width;
			dst_format.height = src_format.height;

			unaligned_rgb ();

			if (fast)
				return;

			if (src_format.width > 5) {
				dst_format.width = src_format.width - 5;
				dst_format.height = src_format.height;

				unaligned_rgb ();
			}

			dst_format.width = src_format.width + 5;
			dst_format.height = src_format.height;

			unaligned_rgb ();

			if (src_format.height > 5) {
				dst_format.width = src_format.width;
				dst_format.height = src_format.height - 5;

				unaligned_rgb ();
			}

			dst_format.width = src_format.width;
			dst_format.height = src_format.height + 5;

			unaligned_rgb ();
		}
	}
}

static void
all_formats			(void)
{
	static const tv_pixfmt src_formats[] = {
		TV_PIXFMT_RGBA32_LE,
		TV_PIXFMT_RGBA32_BE,
		TV_PIXFMT_BGRA32_LE,
		TV_PIXFMT_BGRA32_BE,
		TV_PIXFMT_RGB24_LE,
		TV_PIXFMT_RGB24_BE,
		TV_PIXFMT_SBGGR,
	};
	static const tv_pixfmt dst_formats[] = {
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
			format.pixel_format =
				tv_pixel_format_from_pixfmt (dst_formats[j]);
			and_mask = -1;
			and_mask = get_pixel ((void *) &and_mask,
					      &format, 0, 0);

			or_mask = 0;
			if ((TV_PIXFMT_SET (TV_PIXFMT_RGB24_LE) |
			     TV_PIXFMT_SET (TV_PIXFMT_RGB24_BE) |
			     TV_PIXFMT_SET (TV_PIXFMT_BGR16_LE) |
			     TV_PIXFMT_SET (TV_PIXFMT_BGR16_BE))
			    & TV_PIXFMT_SET (dst_formats[j]))
				or_mask = 0xFF000000;

			dst_format.pixel_format =
				tv_pixel_format_from_pixfmt (dst_formats[j]);
			src_format.pixel_format =
				tv_pixel_format_from_pixfmt (src_formats[i]);

			fprintf (stderr, "%s -> %s ",
				 src_format.pixel_format->name,
				 dst_format.pixel_format->name);

			all_sizes ();

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

	buffer_size = 8 * 4096;

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

	CLEAR (dst_format);
	CLEAR (src_format);

	all_formats ();

	return EXIT_SUCCESS;
}
