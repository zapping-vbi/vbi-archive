/*
 *  MPEG Real Time Encoder
 *  MPEG-1 Audio Layer II Subband Filter Bank
 *
 *  Copyright (C) 1999-2001 Michael H. Schimek
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

/* $Id: filter.c,v 1.3 2001-09-23 19:45:44 mschimek Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include "../common/math.h"
#include "audio.h"

/* Tables */

short mp1e_mp2_fb_window_coeff[512] __attribute__ ((aligned (CACHE_LINE)));
short mp1e_mp2_fb_filter_coeff[352] __attribute__ ((aligned (CACHE_LINE)));

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
 */
static void
filter_test(void)
{
	static const char *nmode[] = { "mono", "left", "right" };
	static const char *nsrc[] = { "random", "dc min", "dc max (overflow)", " sinus" };
	short *s, src[1024];
	int i, j, k, sb2[64], step;
	mmx_t temp[17];
	double sb1[64];
	double d, dmax[32], dsum[32];
	double err_avgs[4], err_maxs[4];
	const int repeat = 10000;
	int mode;

	for (k = 0; k < 4; k++)
		err_avgs[k] = err_maxs[k] = -1;

	for (mode = 0; mode < 3; mode++) {
		temp[0].uq = 0xFFFFFFFF00000000LL;
		temp[1]    = MMXRD(32768L);

		step = (mode == 0) ? 1 : 2;

		srand(0x76543210);

		for (k = 0; k < 4; k++) {
			double err_avg, err_max;

			printv(1, "\n");

			for (i = 0; i < 32; i++)
				dmax[i] = dsum[i] = 0.0;

			for (j = 0; j < repeat; j++) {
				printv(1, "\rAudio filterbank %s test, %s channel ... %u %%",
				       nsrc[k], nmode[mode], j * 100 / repeat);

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
					mp1e_mp2_mmx_window_mono(src, temp);
					break;
				case 1:
					mp1e_mp2_mmx_window_left(src, temp);
					break;
				case 2:
					mp1e_mp2_mmx_window_right(src, temp);
					break;
				}
			
				mp1e_mp2_mmx_filterbank(sb2, temp);

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

			err_avg = dsum[0] / (32.0 * repeat);
			err_max = dmax[0];

			printv(1, " - passed");
			printv(1, ", avg %22.12f^-1 max %22.12f^-1",
			       1.0 / err_avg, 1.0 / err_max);

			if (mode == 0) {
				err_avgs[k] = err_avg;
				err_maxs[k] = err_max;
			} else if (fabs(err_avgs[k] - err_avg) > 1e-12
				   || fabs(err_maxs[k] - err_max) > 1e-12) {
				printv(1, " - failed, mono != left != right\n");
				exit(EXIT_FAILURE);
			}

			if (err_avg > 0.00001 || err_max > 0.00005) {
				printv(1, " - failed, avg %12.2f^-1 max %12.2f^-1\n",
				       1.0 / err_avg, 1.0 / err_max);
				exit(EXIT_FAILURE);
			}

			printv(1, " - passed");
			printv(2, ", avg %12.2f^-1 max %12.2f^-1",
			       1.0 / err_avg, 1.0 / err_max);
		}
	}

	printv(1, "\n");
}

void
mp1e_mp2_subband_filter_init(int test)
{
	int i, j, k, l;

	for (k = 31; k >= 0; k -= 4)
	    for (i = 0; i < 4; i++)
		for (j = 0; j < 8; j += 2)
		    for (l = 0; l < 2; l++) {
			mp1e_mp2_fb_window_coeff[(31 - k) * 16 + i * 2 + j * 4 + l] =
			    lroundn((double)(1 << 19)
				* C[(16 + (k - i)) + 64 * (7 - j - l)]);
			mp1e_mp2_fb_window_coeff[32 + (31 - k) * 16 + i * 2 + j * 4 + l] =
			    lroundn((double)(1 << 19)
				* C[((16 - (k + 1 - (3 - i))) & 63)
				    + 64 * (7 - j - l)]
				* ((k - (3 - i) + 48) > 63 ? -1.0 : +1.0));
		    }

	mp1e_mp2_fb_filter_coeff[0] = 0;
	mp1e_mp2_fb_filter_coeff[1] = +lroundn(32768.0 * sqrt(2.0) / 2.0);
	mp1e_mp2_fb_filter_coeff[2] = 0;
	mp1e_mp2_fb_filter_coeff[3] = -lroundn(32768.0 * sqrt(2.0) / 2.0);

	for (k = 16, l = 4; k > 1; k >>= 1)
	    for (i = 0; i < 32 / k; i++) {
		for (j = 16 + k / 2; j < 48; j += k) {
		    mp1e_mp2_fb_filter_coeff[l++] = lroundn(
			cos((double)((2 * i + 1) * (16 - j)) * M_PI / 64.0) * 32768.0);
	    }
	}

	for (i = 8; i < 24; i += 8)
	    for (j = 2; j < 4; j++) {
		int temp = mp1e_mp2_fb_filter_coeff[i + j];

		mp1e_mp2_fb_filter_coeff[i + j] = mp1e_mp2_fb_filter_coeff[i + j + 2];
		mp1e_mp2_fb_filter_coeff[i + j + 2] = temp;
	    }

	if (test)
		filter_test();
}
