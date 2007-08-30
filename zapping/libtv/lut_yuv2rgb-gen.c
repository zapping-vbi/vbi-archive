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

/* $Id: lut_yuv2rgb-gen.c,v 1.3 2007-08-30 14:14:09 mschimek Exp $ */

/*  Generates look-up tables for image format conversion. */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#define _GNU_SOURCE 1
#undef NDEBUG

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>		/* uint16_t */
#include <limits.h>
#include <math.h>		/* lrint() */
#include "misc.h"		/* SWAB16() */

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

/* Convert positive float to fixed point integer.
   pr: 16 if we can multiply by vu16, 15 by v16. */
static unsigned int
shift				(double			co,
				 unsigned int		pr)
{
	return pr - 1 - lrint (floor (log (co) / log (2.0)));
}

static double
fixed				(double			co,
				 unsigned int		sh)
{
	return floor (co * (1 << sh) + .5);
}

static void
simd_coeff			(int			header)
{
	double cy, gu, bu, rv, gv;
	unsigned int cy_sh, gu_bu_sh, rv_gv_sh;
	unsigned int i;

	cy = 255.0 / 219.0;

	gu = 0.344 * 255.0 / 224.0;
	bu = 1.772 * 255.0 / 224.0;

	rv = 1.402 * 255.0 / 224.0;
	gv = 0.714 * 255.0 / 224.0;

	cy_sh = shift (cy, 15);
	cy = fixed (cy, cy_sh);

	gu_bu_sh = shift (MAX (gu, bu), 15);
	gu = fixed (gu, gu_bu_sh);
	bu = fixed (bu, gu_bu_sh);

	rv_gv_sh = shift (MAX (rv, gv), 15);
	rv = fixed (rv, rv_gv_sh);
	gv = fixed (gv, rv_gv_sh);

#define print_v16(name)							\
do {									\
	fputs ("const int16_t __attribute__ ((aligned (16)))\n"		\
	       "_tv_vsplat16_yuv2rgb_" #name " [] =\n\t{ ", stdout);	\
	for (i = 0; i < 8; ++i)						\
		printf ("0x%04x, ", (unsigned int) name);		\
	puts ("};");							\
} while (0)

	if (header) {
		/* Exponents. */
		printf ("#define CY_SH %u\n", cy_sh);
		printf ("#define GU_BU_SH %u\n", gu_bu_sh);
		printf ("#define RV_GV_SH %u\n\n", rv_gv_sh);

		puts ("#if SIMD\n"
		      "extern const v16\t\t_tv_vsplat16_yuv2rgb_cy;\n"
		      "extern const v16\t\t_tv_vsplat16_yuv2rgb_gu;\n"
		      "extern const v16\t\t_tv_vsplat16_yuv2rgb_bu;\n"
		      "extern const v16\t\t_tv_vsplat16_yuv2rgb_rv;\n"
		      "extern const v16\t\t_tv_vsplat16_yuv2rgb_gv;\n"
		      "#endif\n");
	} else {
		print_v16 (cy);
		print_v16 (gu);
		print_v16 (bu);
		print_v16 (rv);
		print_v16 (gv);

		puts ("");
	}
}

/*
   Ok, here's the plan.  This is what we need to do:

   y1 = (y - 16) * 255 / 219;
   u1 = (u - 128) * 255 / 224;
   v1 = (v - 128) * 255 / 224;
   r = sat (y1              + 1.402 * v1, 0, 255);
   g = sat (y1 - 0.344 * u1 - 0.714 * v1, 0, 255);
   b = sat (y1 + 1.772 * u1,              0, 255);

   This is what we actually do:

   r = sat ((y + 1.402 * (v - 128) * 255 / 224 / 255 * 219)
   	    * 255 / 219 - 16 * 255 / 219, 0, 255);
   ->
   ys[x] = sat ((x - 16) * 255 / 219, 0, 255);
   rv[x] = (x - 128) * 255 / 224 / 255 * 219 * 1.402;
   r = ys [y + rv[v]];

   etc.
*/

static int16_t			lut_gu[256];
static int16_t			lut_gv[256];
static int16_t			lut_rv[256];
static int16_t			lut_bu[256];

#define BIAS 256
#define SUM 512

static void
scalar_coeff			(int			header)
{
	int gu_min, gv_min, rv_min, bu_min;
	int gu_max, gv_max, rv_max, bu_max;
	int i;

	if (header) {
		puts ("extern const int16_t\t\t_tv_lut_yuv2rgb_gu [256];\n"
		      "extern const int16_t\t\t_tv_lut_yuv2rgb_gv [256];\n"
		      "extern const int16_t\t\t_tv_lut_yuv2rgb_rv [256];\n"
		      "extern const int16_t\t\t_tv_lut_yuv2rgb_bu [256];\n");
		return;
	}

	gu_min = INT_MAX;
	gu_max = INT_MIN;

	gv_min = INT_MAX;
	gv_max = INT_MIN;

	rv_min = INT_MAX;
	rv_max = INT_MIN;

	bu_min = INT_MAX;
	bu_max = INT_MIN;

	for (i = 0; i < 256; ++i) {
		lut_gu[i] = lrint ((i - 128) * 219 / 224.0 * -0.344) + BIAS;
		lut_gv[i] = lrint ((i - 128) * 219 / 224.0 * -0.714);
		lut_rv[i] = lrint ((i - 128) * 219 / 224.0 * +1.402) + BIAS;
		lut_bu[i] = lrint ((i - 128) * 219 / 224.0 * +1.772) + BIAS;

#define MIN_MAX(array)							\
do {									\
	if (lut_ ## array[i] < array ## _min)				\
		array ## _min = lut_ ## array[i];			\
	if (lut_ ## array[i] > array ## _max)				\
		array ## _max = lut_ ## array[i];			\
} while (0)

		MIN_MAX (gu);
		MIN_MAX (gv);
		MIN_MAX (rv);
		MIN_MAX (bu);
	}

	if (0)
		fprintf (stderr, "coeff min=%d,%d,%d,%d max=%d,%d,%d,%d\n",
			 gu_min, gv_min, rv_min, bu_min,
			 gu_max, gv_max, rv_max, bu_max);

	assert (0 + gu_min + gv_min >= 0);
	assert (0 + rv_min >= 0);
	assert (0 + bu_min >= 0);

	assert (255 + gu_max + gv_max < BIAS + SUM);
	assert (255 + rv_min < BIAS + SUM);
	assert (255 + bu_min < BIAS + SUM);

#define dump(array)							\
do {									\
	unsigned int i;							\
									\
	fputs ("const int16_t\n"					\
	       "_tv_lut_yuv2rgb_" #array " [256] = {", stdout);		\
	for (i = 0; i < 256; ++i) {					\
		printf ("%s0x%04x, ",					\
			(0 == (i % 8)) ? "\n\t" : "",			\
			0xFFFF & lut_ ## array[i]);			\
	}								\
	puts ("\n};\n");						\
} while (0)

	dump (gu);
	dump (gv);
	dump (rv);
	dump (bu);
}

static uint8_t			lut_8[BIAS + SUM];

static uint16_t			lut_rgb16[2][6][BIAS + SUM];

static void
color				(int			header)
{
	int i;

	if (header) {
		printf ("extern const uint8_t\t\t"
			"_tv_lut_yuv2rgb8 [%d + %d];\n"
			"extern const uint16_t\t\t"
			"_tv_lut_yuv2rgb16 [2][6][%d + %d];\n\n",
			BIAS, SUM, BIAS, SUM);
		return;
	}

	for (i = -BIAS; i < SUM; ++i) {
		unsigned int m;
		unsigned int n;

		m = SATURATE (lrint ((i - 16) * 255 / 219.0), 0, 255);

		lut_8[i + BIAS] = m;

		n = (m << 8) & 0xF800;
		lut_rgb16[0][0][i + BIAS] = n; /* BGR red / RGB blue  */
		lut_rgb16[1][0][i + BIAS] = SWAB16 (n);

		n = (m << 7) & 0x7C00;
		lut_rgb16[0][1][i + BIAS] = n; /* BGRA red / ARGB blue */
		lut_rgb16[1][1][i + BIAS] = SWAB16 (n);

		n = (m << 3) & 0x07E0;
		lut_rgb16[0][2][i + BIAS] = n; /* BGR / RGB green */
		lut_rgb16[1][2][i + BIAS] = SWAB16 (n);

		n = (m << 2) & 0x03E0;
		lut_rgb16[0][3][i + BIAS] = n; /* BGRA / RGBA green */
		lut_rgb16[1][3][i + BIAS] = SWAB16 (n);

		n = (m >> 3) & 0x001F;
		lut_rgb16[0][4][i + BIAS] = n; /* blue / red */
		lut_rgb16[1][4][i + BIAS] = SWAB16 (n);

		n = (m << 8) & 0x8000;
		lut_rgb16[0][5][i + BIAS] = n; /* alpha */
		lut_rgb16[1][5][i + BIAS] = SWAB16 (n);
	}

	printf ("const uint8_t\n"
		"_tv_lut_yuv2rgb8 [%d + %d] = {", BIAS, SUM);

	for (i = 0; i < BIAS + SUM; ++i)
		printf ("%s0x%02x, ", (0 == (i % 8)) ? "\n\t" : "", lut_8[i]);

	printf ("\n};\n\n"
		"const uint16_t\n"
		"_tv_lut_yuv2rgb16 [2][6][%d + %d] = {\n\t", BIAS, SUM);

	for (i = 0; i < 2; ++i) {
		unsigned int j;

		fputs ("{\n\t\t", stdout);
		for (j = 0; j < 6; ++j) {
			unsigned int k;

			fputs ("{", stdout);
			for (k = 0; k < BIAS + SUM; k += 8) {
				unsigned int l;

				fputs ("\n\t\t\t", stdout);
				for (l = 0; l < 8; ++l) {
					printf ("0x%04x, ",
						lut_rgb16[i][j][k + l]);
				}
			}

			fputs ("\n\t\t}, ", stdout);
		}

		fputs ("\n\t}, ", stdout);
	}

	puts ("\n};\n");
}

int
main				(int			argc,
				 char **		argv)
{
	int header;

	argv = argv;

	header = (argc > 1);

	puts ("/* Generated file, do not edit! */\n");

	if (header)
		puts ("#ifndef LUT_YUV2RGB_H\n"
		      "#define LUT_YUV2RGB_H\n\n"
		      "#include <inttypes.h>\n"
		      "#include \"simd.h\"\n");
	else
		puts ("#include \"misc.h\"\n"
		      "#include \"lut_yuv2rgb.h\"\n");

	simd_coeff (header);
	scalar_coeff (header);

	color (header);

	if (header)
		puts ("#endif /* LUT_YUV2RGB_H */\n");

	exit (EXIT_SUCCESS);
}

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
