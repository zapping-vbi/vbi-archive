/*
 *  MPEG Real Time Encoder
 *  MPEG-1 Audio Layer II Subband Filter Bank
 *
 *  Copyright (C) 1999-2000 Michael H. Schimek
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

/* $Id: filter.c,v 1.3 2000-11-11 02:32:21 mschimek Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include "../common/math.h"
#include "audio.h"

#define TEST 0

/* Tables */

short fb_window_coeff[512] __attribute__ ((aligned (CACHE_LINE)));
short fb_filter_coeff[352] __attribute__ ((aligned (CACHE_LINE)));

#if TEST

static void
reference_filter(short *s, double *d)
{
	double z[512], y[64];
        int i, j;

    	for (i = 0; i < 512; i++)
		z[i] = ((double) s[511 - i]) / 32768.0 * C[i];

	for (i = 0; i < 64; i++)
		for (j = 0, y[i] = 0; j < 8; j++)
			y[i] += z[i + 64 * j];

	for (i = 0; i < 32; i++)
		for (j = 0, d[i] = 0; j < 64; j++)
			d[i] += cos((double)((2 * i + 1) * (16 - j)) * M_PI / 64.0) * y[j];
}

/*
 *  Compare output of the MMX and reference subband filter
 *  mode: 0 = mono, 1 = left, 2 = right (should all give the same result)
 */
static void
filter_test(int mode)
{
	static const char *nmode[] = { "mono", "left", "right" };
	static const char *nsrc[] = { "random", "dc min", "dc max", " sinus" };
	short *s, src[1024];
	int i, j, k, sb2[64], step = (mode == 0) ? 1 : 2;
	mmx_t temp[17];
	double sb1[64];
	double d, dmax[32], dsum[32];
	const int repeat = 10000;

	temp[0].uq = 0xFFFFFFFF00000000LL;
	temp[1]    = MMXRD(32768L);

	srand(0x76543210);

	fprintf(stderr, "filter_test, %d iterations, %s channel\n",
		repeat, nmode[mode]);

	for (k = 0; k < 4; k++) {
		for (i = 0; i < 32; i++)
			dmax[i] = dsum[i] = 0.0;

		for (j = 0; j < repeat; j++) {
			s = src + ((mode == 2) ? 1 : 0);
			for (i = 0; i < 512; s += step, i++)
				switch (k) {
				case 0:
					*s = rand();
					break;
				case 1:
					*s = 1;
					break;
				case 2:
					*s = -32768;
				        break;
				case 3:
					*s = 32767 * sin(M_PI * ((double) i / j));
					break;
				}

			switch (mode) {
			case 0:
				mmx_window_mono(src, temp);
				break;
			case 1:
				mmx_window_left(src, temp);
				break;
			case 2:
				mmx_window_right(src, temp);
				break;
			}
			
			mmx_filterbank(sb2, temp);

			emms();

			if (mode == 1)
				for (i = 0; i < 512; i++)
					src[i] = src[i * 2];
			else if (mode == 2)
				for (i = 0; i < 512; i++)
					src[i] = src[i * 2 + 1];

			reference_filter(src, sb1);

			for (i = 0; i < 32; i++) {
				d = fabs(sb1[i] - ((double) sb2[i]) / (double)(1L << 30));
				if (d > dmax[0])
					dmax[0] = d;
				dsum[0] += d;
			}
		}

		fprintf(stderr, "%s: mean abs diff %12.2f^-1, max abs diff %12.2f^-1\n",
			nsrc[k], (32.0 * repeat) / dsum[0], 1.0 / dmax[0]);
	}
}

#endif // TEST

void
subband_filter_init(struct audio_seg *mp2)
{
	int i, j, k, l;

	for (k = 31; k >= 0; k -= 4)
	    for (i = 0; i < 4; i++)
		for (j = 0; j < 8; j += 2)
		    for (l = 0; l < 2; l++) {
			fb_window_coeff[(31 - k) * 16 + i * 2 + j * 4 + l] = lroundn((double)(1 << 19) * C[(16 + (k - i)) + 64 * (7 - j - l)]);
			fb_window_coeff[32 + (31 - k) * 16 + i * 2 + j * 4 + l] = lroundn((double)(1 << 19) * C[((16 - (k + 1 - (3 - i))) & 63) + 64 * (7 - j - l)] * ((k - (3 - i) + 48) > 63 ? -1.0 : +1.0));
		    }

	fb_filter_coeff[0] = 0;
	fb_filter_coeff[1] = +lroundn(32768.0 * sqrt(2.0) / 2.0);
	fb_filter_coeff[2] = 0;
	fb_filter_coeff[3] = -lroundn(32768.0 * sqrt(2.0) / 2.0);

	for (k = 16, l = 4; k > 1; k >>= 1)
	    for (i = 0; i < 32 / k; i++) {
		for (j = 16 + k / 2; j < 48; j += k) {
		    fb_filter_coeff[l++] = lroundn(cos((double)((2 * i + 1) * (16 - j)) * M_PI / 64.0) * 32768.0);
	    }
	}

	for (i = 8; i < 24; i += 8)
	    for (j = 2; j < 4; j++) {
		int temp = fb_filter_coeff[i + j];

		fb_filter_coeff[i + j] = fb_filter_coeff[i + j + 2];
		fb_filter_coeff[i + j + 2] = temp;
	    }

#if TEST

	filter_test(0);
	filter_test(1);
	filter_test(2);

	exit(EXIT_SUCCESS);
#endif
}
