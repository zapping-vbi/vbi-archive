/*
 *  MPEG-1 Real Time Encoder
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

/* $Id: dct.c,v 1.11 2001-05-07 13:06:06 mschimek Exp $ */

#include <assert.h>
#include "../common/math.h"
#include "../common/mmx.h"
#include "dct.h"
#include "dct_ieee.h"
#include "mpeg.h"
#include "video.h"

char sh1[8] = { 15, 14, 13, 13, 12, 12, 12, 11 };
char sh2[8] = { 16, 14, 13, 13, 13, 12, 12, 12 };

/*
 *  ((q > 16) ? q & ~1 : q) == ((ltp[q] * 2 + 1) << lts[q])
 */
char ltp[32] __attribute__ ((aligned (MIN(32, CACHE_LINE)))) = {
    0, 0, 0, 1, 0, 2, 1, 3, 0, 4, 2, 5, 1, 6, 3, 7,
    0, 0, 4, 4, 2, 2, 5, 5, 1, 1, 6, 6, 3, 3, 7, 7,
};
char lts[32] __attribute__ ((aligned (MIN(32, CACHE_LINE)))) = { 
    0, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    4, 4, 1, 1, 2, 2, 1, 1, 3, 3, 1, 1, 2, 2, 1, 1,
};

extern int intra_quant_scale;
extern int inter_quant_scale;

#if __GNUC_MINOR__ < 90
#define align(n)
#else
#define align(n) __attribute__ ((SECTION("video_tables") aligned (n)))
#endif

mmx_t c0 align(8);
mmx_t c1 align(8);
mmx_t c2 align(8);
mmx_t c4 align(8);
mmx_t c1_15 align(8);
mmx_t c1_16 align(8);
mmx_t c1_17 align(8);
mmx_t c1_15w align(8);
mmx_t c128 align(8);
mmx_t c255 align(8);
mmx_t c128_6 align(8);
mmx_t c1b align(8);
mmx_t cC4_14 align(8);
mmx_t cC4_15 align(8);
mmx_t c1C6_13 align(8);
mmx_t cC4C6_14 align(8);
mmx_t cC2C61_13 align(8);
mmx_t cC2626_15 align(8);
mmx_t cC6262_15 align(8);
mmx_t mm8, mm9;

char			mmx_q_fdct_intra_sh[32]			align(MIN(CACHE_LINE,32));
short			mmx_q_fdct_intra_q_lut[8][8][8]		align(CACHE_LINE);
short			mmx_q_fdct_inter_lut[6][8][2]		align(CACHE_LINE);
short			mmx_q_fdct_inter_lut0[8][1]		align(CACHE_LINE);
short			mmx_q_fdct_inter_q[32]			align(CACHE_LINE);
mmx_t			mmx_q_idct_inter_tab[16]		align(CACHE_LINE);
short			mmx_q_idct_intra_q_lut[8][8][8]		align(CACHE_LINE);

mmx_t cfae;
mmx_t csh;
mmx_t crnd;
mmx_t c3a, c5a;
mmx_t c5b;
mmx_t c1x;
mmx_t shift, mask, mask0;

#define R2 sqrt(2.0)

static void init_dct(void) __attribute__ ((constructor));

static void
init_dct(void)
{
	double mmx_inter_lut[8][8];
	int q, v, u, sh, max;
        double Cu, Cv, m;
	int shq[8];

	/* Constants used throughout the video encoder */

	c0		= MMXRW(0);
	c1		= MMXRW(1);
	c2		= MMXRW(2);
	c4		= MMXRW(4);

	c1_15		= MMXRD(1 << 15);
	c1_16		= MMXRD(1 << 16);
	c1_17		= MMXRD(1 << 17);
	c1_15w		= MMXRW(0x8000);

	c128		= MMXRW(128);
	c255		= MMXRW(255);
	c128_6		= MMXRW(128 << 6);
	c1b		= MMXRB(1);

	cC4_14		= MMXRW(lroundn(C4 * S14));
	cC4_15		= MMXRW(lroundn(C4 * S15));

	c1C6_13		= MMXRW(lroundn((1.0 / C6) * S13));
	cC4C6_14	= MMXRW(lroundn((C4 / C6) * S14));

	cC2C61_13	= MMXRW(lroundn((C2 / C6 - 1) * S13));
	cC2626_15	= MMXW(lroundn(C2 * S15), -lroundn(C6 * S15), lroundn(C2 * S15), -lroundn(C6 * S15));
	cC6262_15	= MMXW(lroundn(C6 * S15), +lroundn(C2 * S15), lroundn(C6 * S15), +lroundn(C2 * S15));

	c3a = MMXRW(0);
	c5a = MMXRW(128 * 32 + 16);
	c5b = MMXRW(16);

	mmx_q_idct_inter_tab[0]	 = MMXW(+lroundn(S15*C1/R2), +lroundn(S15*C7/R2), +lroundn(S15*C1/R2), +lroundn(S15*C7/R2));
	mmx_q_idct_inter_tab[1]	 = MMXW(+lroundn(S15*C7/R2), -lroundn(S15*C1/R2), +lroundn(S15*C7/R2), -lroundn(S15*C1/R2));
	mmx_q_idct_inter_tab[2]	 = MMXW(+lroundn(S15*C3/R2), +lroundn(S15*C5/R2), +lroundn(S15*C3/R2), +lroundn(S15*C5/R2));
	mmx_q_idct_inter_tab[3]	 = MMXW(-lroundn(S15*C5/R2), +lroundn(S15*C3/R2), -lroundn(S15*C5/R2), +lroundn(S15*C3/R2));
	mmx_q_idct_inter_tab[4]	 = MMXRW(lroundn(S15*C1/R2));
	mmx_q_idct_inter_tab[5]	 = MMXRW(lroundn(S15*(C7+C1)/R2));
	mmx_q_idct_inter_tab[6]	 = MMXRW(lroundn(S15*(C7-C1)/R2));
	mmx_q_idct_inter_tab[7]	 = MMXRW(lroundn(S15*C5/R2));
	mmx_q_idct_inter_tab[8]	 = MMXRW(lroundn(S15*(C3+C5)/R2));
	mmx_q_idct_inter_tab[9]	 = MMXRW(lroundn(S15*(C3-C5)/R2));
	mmx_q_idct_inter_tab[10] = MMXRD(1024);
	mmx_q_idct_inter_tab[11] = MMXRW((8 << 2) + 2);
	mmx_q_idct_inter_tab[12] = MMXRW(lroundn(S15*C2/R2));
	mmx_q_idct_inter_tab[13] = MMXRW(lroundn(S15*(C6+C2)/R2));
	mmx_q_idct_inter_tab[14] = MMXRW(lroundn(S15*(C6-C2)/R2));
	mmx_q_idct_inter_tab[15] = MMXRW(lroundn(S16*(C6-C2)/R2));

	for (q = 0; q < 8; q++) {
		for (sh = max = 0; max < 16384; sh++)
			for (v = max = 0; v < 8; v++)
				for (u = 0; u < 8; u++) {
					Cu = (u == 0) ? 1 : (cos(u * M_PI / 16.0) * sqrt(2.0));
					Cv = (v == 0) ? 1 : (cos(v * M_PI / 16.0) * sqrt(2.0));

					m = 1.0 / (Cu * Cv * 8.0);

					if (u == 0 || u == 4) m *= 0.125;
					if (u == 2 || u == 6) m *= 0.25;
					if (u & 1) m *= C6 * 0.5;
					if (v == 0 || v == 4) m *= 2;
					if (v == 2 || v == 6) m *= 4;
					if (v & 1) m *= C6 * 8;
					if (u == 0 && v == 0) m = 0;

					mmx_q_fdct_intra_q_lut[q][u][v] = lroundn(
						m
						* 8
						/ default_intra_quant_matrix[v][u]
						/ (2 * q + 1)
						* (double)(1 << sh));

					if (mmx_q_fdct_intra_q_lut[q][v][u] > max)
						max = mmx_q_fdct_intra_q_lut[q][v][u];

					mmx_q_fdct_intra_q_lut[q][0][0] = 0;
				}
		shq[q] = sh;
	}

	for (q = 1; q < 32; q++) {
		int ltsi = lts[q], ltpi = ltp[q];
		
		mmx_q_fdct_intra_sh[q] = shq[ltpi] + ltsi - 17;
	}

	for (v = 0; v < 8; v++) {
		for (u = 0; u < 8; u++) {
    			Cu = (u == 0) ? 1.0 : (cos(u * M_PI / 16.0) * sqrt(2.0));
			Cv = (v == 0) ? 1.0 : (cos(v * M_PI / 16.0) * sqrt(2.0));

			if (v == 2 || v == 6) Cv = 1.0;

			m = 1.0 / (Cu * Cv * 8.0);

			if (u & 1) m *= C6;
			if (u == 0 || u == 4 || u == 7) m /= 4.0;
			if (u == 2 || u == 5) m /= 2.0;
			if (u == 6) m /= 8.0;

			mmx_inter_lut[v][u] = m;
		}
	}

	for (u = 0; u < 8; u++) {
		mmx_q_fdct_inter_lut0[u][0] = lroundn(mmx_inter_lut[0][u] * S19);
		mmx_q_fdct_inter_lut[1][u][0] = lroundn(mmx_inter_lut[0][u] * +(C2 + C6) * S18);
		mmx_q_fdct_inter_lut[1][u][1] = lroundn(mmx_inter_lut[0][u] * +(C2 - C6) * S18);
		mmx_q_fdct_inter_lut[4][u][0] = lroundn(mmx_inter_lut[0][u] * +(C2 - C6) * S18);
		mmx_q_fdct_inter_lut[4][u][1] = lroundn(mmx_inter_lut[0][u] * -(C2 + C6) * S18);
		mmx_q_fdct_inter_lut[0][u][1] = +(mmx_q_fdct_inter_lut[0][u][0] = lroundn(mmx_inter_lut[1][u] * S19));
		mmx_q_fdct_inter_lut[2][u][1] = -(mmx_q_fdct_inter_lut[2][u][0] = lroundn(mmx_inter_lut[3][u] * S19));
		mmx_q_fdct_inter_lut[3][u][1] = +(mmx_q_fdct_inter_lut[3][u][0] = lroundn(mmx_inter_lut[5][u] * S19));
		mmx_q_fdct_inter_lut[5][u][1] = -(mmx_q_fdct_inter_lut[5][u][0] = lroundn(mmx_inter_lut[7][u] * S17));
	}

	for (q = 0; q < 32; q++)
		mmx_q_fdct_inter_q[q] = lroundn(S15 / q / 2.0);

	for (q = 0; q < 8; q++) {
		for (v = 0; v < 8; v++)
			for (u = 0; u < 8; u++)
				if (u + v == 0)
					mmx_q_idct_intra_q_lut[q][v][u] = 0;
				else
					mmx_q_idct_intra_q_lut[q][v][u] = 
						4 * default_intra_quant_matrix[v][u] * (q * 2 + 1);

//		dump(mmx_q_idct_intra_q_lut[q]);
	}

	c1x = MMXRW(((8 + 128 * 16) << 2) + 2);
}

/* XXX move */
void
mmx_new_inter_quant(int quant_scale)
{
	unsigned int n;

	inter_quant_scale = quant_scale;

	// fdct_inter
	n = mmx_q_fdct_inter_q[quant_scale];
	n |= -32767 << 16;
	cfae.ud[0] = n;
	cfae.ud[1] = n;
}
