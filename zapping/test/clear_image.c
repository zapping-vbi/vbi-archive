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

/* $Id: clear_image.c,v 1.1 2004-12-11 11:46:24 mschimek Exp $ */

#undef NDEBUG

#include "pixel.h"
#include "guard.h"

#include "src/cpu.c"

static uint8_t *		buffer;
static uint8_t *		buffer_end;

static tv_image_format		format;

static unsigned int
format_packed_size		(void)
{
	if (0 == format.width || 0 == format.height)
		return 0;

	return format.offset
		+ format.bytes_per_line	* (format.height - 1)
		+ format.width * tv_pixfmt_bytes_per_pixel (format.pixfmt);
}

static void
test_packed			(uint8_t *		d,
				 uint8_t *		end)
{
	unsigned int x;
	unsigned int y;
	unsigned int bpp;
	uint8_t *s;
	tv_bool r;

	if (0) {
		_tv_image_format_dump (&format, stderr);
		fputc ('\n', stderr);
	}

	s = d + format.offset;

	if (end > s)
		memset (s, 0xAA, end - s);

	r = tv_clear_image (d, &format);

	if ((TV_PIXFMT_SET (format.pixfmt) & TV_PIXFMT_SET_YUV16)
	    && (format.width & 1)) {
		/* Permutations of YUYV. */
		assert (!r);
		return;
	} else {
		assert (r);
	}

	bpp = tv_pixfmt_bytes_per_pixel (format.pixfmt);

	for (y = 0; y < format.height; ++y) {
		if (TV_PIXFMT_YUYV == format.pixfmt ||
		    TV_PIXFMT_YVYU == format.pixfmt) {
			for (x = 0; x < format.width; ++x) {
				assert (0x00 == *s++);
				assert (0x80 == *s++);
			}
		} else if (TV_PIXFMT_UYVY == format.pixfmt ||
			   TV_PIXFMT_VYUY == format.pixfmt) {
			for (x = 0; x < format.width; ++x) {
				assert (0x80 == *s++);
				assert (0x00 == *s++);
			}
		} else if (TV_PIXFMT_IS_YUV (format.pixfmt)) {
			for (x = 0; x < format.width; ++x, s += bpp)
				assert (0x00808000 == get_packed_pixel
					(s, format.pixfmt));
		} else {
			for (x = 0; x < format.width; ++x, s += bpp)
				assert (0x00000000 == get_packed_pixel
					(s, format.pixfmt));
		}

		if (s < end)
			for (x = format.width * bpp;
			     x < format.bytes_per_line; ++x)
				assert (0xAA == *s++);
	}
}

static void
test_planar			(void)
{
}

static void
test_packed1			(void)
{
	if (1) {
		fputc ('.', stderr);
		fflush (stderr);
	}

	for (format.pixfmt = 0;
	     format.pixfmt < TV_MAX_PIXFMTS;
	     ++format.pixfmt) {
		unsigned int bpp;

		if (!TV_PIXFMT_IS_PACKED (format.pixfmt))
			continue;

		if (0)
			fprintf (stderr, "%s\n",
				 tv_pixfmt_name (format.pixfmt));

		bpp = tv_pixfmt_bytes_per_pixel (format.pixfmt);

		format.bytes_per_line = format.width * bpp;

		for (format.offset = 0; format.offset < 17; ++format.offset) {
			format.size = format_packed_size ();
			test_packed (buffer - format.offset,
				     buffer - format.offset + format.size);
		}

		format.offset = 0;

		for (format.bytes_per_line = format.width * bpp;
		     format.bytes_per_line < (format.width + 33) * bpp;
		     ++format.bytes_per_line) {
			format.size = format_packed_size ();
			test_packed (buffer_end - format.size, buffer_end);
		}
	}
}

static void
test_planar1			(void)
{
	/* to do */

	test_planar ();
}

static void
test				(const char *		name)
{
	fprintf (stderr, "%s ", name);

	CLEAR (format);

	test_packed1 ();
	test_planar1 ();

	format.width = 160;
	format.height = 0;

	test_packed1 ();
	test_planar1 ();

	format.width = 0;
	format.height = 160;

	test_packed1 ();
	test_planar1 ();

	format.height = 12;

	for (format.width = 0; format.width < 17; ++format.width) {
		test_packed1 ();
		test_planar1 ();
	}

	for (format.width = 160; format.width < 177; ++format.width) {
		test_packed1 ();
		test_planar1 ();
	}

	fputc ('\n', stderr);
}

int
main				(int			argc,
				 char **		argv)
{
	unsigned int buffer_size;
	cpu_feature_set features;

	(void) argc;
	(void) argv;

	buffer_size = 4 << 20;
	buffer = guard_alloc (buffer_size);
	buffer_end = buffer + buffer_size;

	features = cpu_detection ();

	cpu_features = 0;
	test ("clear_image generic");

	if (features & CPU_FEATURE_MMX) {
		cpu_features = CPU_FEATURE_MMX;
		test ("clear_image mmx");
	}

	if (features & CPU_FEATURE_SSE) {
		cpu_features = CPU_FEATURE_SSE;
		test ("clear_image sse");
	}

	return EXIT_SUCCESS;
}
