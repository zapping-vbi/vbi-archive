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

/* $Id: copy_image.c,v 1.1 2006-04-12 01:48:16 mschimek Exp $ */

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

#define ERASE(var) memset (&(var), 0xAA, sizeof (var))

tv_bool				fast = 0;

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

static unsigned int
bytes_per_pixel			(const tv_image_format *format)
{
	return format->pixel_format->bits_per_pixel >> 3;
}

static void
compare_images			(const uint8_t *	dst,
				 const tv_image_format *dst_format,
				 const uint8_t *	src,
				 const tv_image_format *src_format)
{
	const tv_pixel_format *pf;
	const uint8_t *d;
	const uint8_t *s;
	unsigned int width;
	unsigned int height;
	unsigned int y;

	pf = dst_format->pixel_format;

	width = dst_format->width * bytes_per_pixel (dst_format);
	height = dst_format->height;

	d = dst + dst_format->offset[0];
	s = src + src_format->offset[0];

	for (y = 0; y < height; ++y) {
		assert (0 == memcmp (d, s, width));
		d += dst_format->bytes_per_line[0];
		s += src_format->bytes_per_line[0];
	}

	if (1 == pf->n_planes)
		return;

	width >>= pf->uv_hshift;
	height >>= pf->uv_vshift;

	d = dst + dst_format->offset[1];
	s = src + src_format->offset[1];

	for (y = 0; y < height; ++y) {
		assert (0 == memcmp (d, s, width));
		d += dst_format->bytes_per_line[1];
		s += src_format->bytes_per_line[1];
	}

	if (2 == pf->n_planes)
		return;

	d = dst + dst_format->offset[2];
	s = src + src_format->offset[2];

	for (y = 0; y < height; ++y) {
		assert (0 == memcmp (d, s, width));
		d += dst_format->bytes_per_line[2];
		s += src_format->bytes_per_line[2];
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

	if (erase_dst) {
		memset (dst_buffer, 0xAA, dst_buffer_end - dst_buffer);
		erase_dst = 0;
	}

	switch (dst_format.pixel_format->pixfmt) {
	case TV_PIXFMT_SBGGR:
		hshift = 1;
		vshift = 1;
		break;

	case TV_PIXFMT_NV12:
	case TV_PIXFMT_YUYV:
	case TV_PIXFMT_UYVY:
	case TV_PIXFMT_YVYU:
	case TV_PIXFMT_VYUY:
		hshift = 1;
		break;

	default:
		break;
	}

	switch (src_format.pixel_format->pixfmt) {
	case TV_PIXFMT_SBGGR:
		hshift = 1;
		vshift = 1;
		break;

	case TV_PIXFMT_NV12:
	case TV_PIXFMT_YUYV:
	case TV_PIXFMT_UYVY:
	case TV_PIXFMT_YVYU:
	case TV_PIXFMT_VYUY:
		hshift = 1;
		break;

	default:
		break;
	}

	success = tv_copy_image (dst, &dst_format,
				 src, &src_format);

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

	erase_dst = 1;

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

	if (fast)
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

			if (fast)
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
				src_format.width = widths[j];

			dst_format.width = src_format.width;
			dst_format.height = src_format.height;

			if (planar)
				unaligned_planar ();
			else
				unaligned_packed ();

			if (fast)
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
	unsigned int i;

	for (i = 0; i < TV_MAX_PIXFMTS; ++i) {
		if (0 == (TV_PIXFMT_SET (i) & TV_PIXFMT_SET_ALL))
			continue;

		dst_format.pixel_format = tv_pixel_format_from_pixfmt (i);
		src_format = dst_format;

		fprintf (stderr, "%s ",
			 dst_format.pixel_format->name);

		all_sizes ();

		fputc ('\n', stderr);
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

	return EXIT_SUCCESS;
}
