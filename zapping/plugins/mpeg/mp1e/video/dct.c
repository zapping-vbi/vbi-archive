/*
 *  MPEG-1 Real Time Encoder
 *  Discrete cerebral transformation routines
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

/* $Id: dct.c,v 1.6 2000-09-29 17:54:33 mschimek Exp $ */

#include <assert.h>
#include "../common/math.h"
#include "../common/mmx.h"
#include "dct.h"
#include "dct/ieee.h"
#include "mpeg.h"
#include "video.h"

static short LUT1[8][8][8];
static short LUT2[8][8][8];

char sh1[8] = { 15, 14, 13, 13, 12, 12, 12, 11 };
char sh2[8] = { 16, 14, 13, 13, 13, 12, 12, 12 };

/*
 *  ((q > 16) ? q & ~1 : q) == ((ltp[q] * 2 + 1) << lts[q])
 */
char ltp[32] = {
    0, 0, 0, 1, 0, 2, 1, 3, 0, 4, 2, 5, 1, 6, 3, 7,
    0, 0, 4, 4, 2, 2, 5, 5, 1, 1, 6, 6, 3, 3, 7, 7,
};
char lts[32] = { 
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
mmx_t c128 align(8);
mmx_t c255 align(8);
mmx_t c128_6 align(8);
mmx_t cC4_14 align(8);
mmx_t cC4_15 align(8);
mmx_t c1C6_13 align(8);
mmx_t cC4C6_14 align(8);
mmx_t cC2C61_13 align(8);
mmx_t cC2626_15 align(8);
mmx_t cC6262_15 align(8);
mmx_t mm8, mm9;

char			mmx_q_fdct_intra_sh[32] align(MIN(CACHE_LINE,32));
short			mmx_q_fdct_intra_q_lut[8][8][8] align(CACHE_LINE);

short mmx_q_fdct_inter_lut[6][8][2] align(CACHE_LINE);
short mmx_q_fdct_inter_lut0[8][1] align(CACHE_LINE);
short mmx_q_fdct_inter_q[32] align(CACHE_LINE);

mmx_t cfae;
mmx_t csh;
mmx_t crnd;
short *qfiql;
int qfiq;
mmx_t c3a, c5a;
mmx_t c3b, c5b;
static short *lut1p, *lut2p;

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

	c128		= MMXRW(128);
	c255		= MMXRW(255);

	c128_6		= MMXRW(128 << 6);

	cC4_14		= MMXRW(lroundn(C4 * S14));
	cC4_15		= MMXRW(lroundn(C4 * S15));

	c1C6_13		= MMXRW(lroundn((1.0 / C6) * S13));
	cC4C6_14	= MMXRW(lroundn((C4 / C6) * S14));

	cC2C61_13	= MMXRW(lroundn((C2 / C6 - 1) * S13));
	cC2626_15	= MMXW(lroundn(C2 * S15), -lroundn(C6 * S15), lroundn(C2 * S15), -lroundn(C6 * S15));
	cC6262_15	= MMXW(lroundn(C6 * S15), +lroundn(C2 * S15), lroundn(C6 * S15), +lroundn(C2 * S15));

	c3a = MMXRW(0);
	c5a = MMXRW(128 * 32 + 16);
	c3b = MMXRW(0);
	c5b = MMXRW(16);

	for (q = 0; q < 8; q++) {
		for (v = 0; v < 8; v++) {
			for (u = 0; u < 8; u++) {
				double Cu, Cv, m;

    				Cu = (u == 0) ? 1.0 : (cos(u * M_PI / 16.0) * sqrt(2.0));
				Cv = (v == 0) ? 1.0 : (cos(v * M_PI / 16.0) * sqrt(2.0));

				/* Inverse */

				m = 1.0 * (Cu * Cv / 8.0);

				if (v & 1)
					m *= C6 * 2.0;
				if (u & 1)
					m *= C6 * 2.0;

				if (v + u == 0) {
					LUT1[q][v][u] = 0;
				} else {
					LUT1[q][v][u] = lroundn(
	        				default_intra_quant_matrix[v][u]
						* (q * 2 + 1)
						/ 8.0
						* m
						* (double)(1 << sh1[q])
						* ((v == 2) ? 0.5 : 1)
					);
				}

				LUT2[q][v][u] = lroundn(
	        			default_inter_quant_matrix[v][u]
					* (q * 2 + 1)
					/ 8.0
					* m
					* (double)(1 << sh2[q])
				);
			}
		}
	}

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
}


void
mmx_new_inter_quant(int quant_scale)
{
	unsigned int n;
	int ltsi = lts[quant_scale];
	int ltpi = ltp[quant_scale];

	inter_quant_scale = quant_scale;

	n = mmx_q_fdct_inter_q[quant_scale];
	n |= -32767 << 16;
	cfae.ud[0] = n;
	cfae.ud[1] = n;

	c3b.uq = 16 /* pmulh */ + 5 /* scale */ + ltsi - sh2[ltpi];
	lut2p = &LUT2[ltpi][0][0];
}

/* preliminary */

void
mmx_mpeg1_idct_intra(int quant_scale)
{
	unsigned char *new = newref;
	int ltsi = lts[quant_scale];
	int ltpi = ltp[quant_scale];
	int i;

	c3a.uq = 16 /* pmulh */ + 5 /* scale */ + ltsi - sh1[ltpi];
	lut1p = &LUT1[ltpi][0][0];

	for (i = 0; i < 6; i++) {
		new += mb_address.block[i].offset;

asm("
	movq 1*16+0*2(%0),%%mm6; psllw c3a,%%mm6; paddw c4,%%mm6; pmulhw 1*16+0*2(%1),%%mm6;
	movq 3*16+0*2(%0),%%mm7; psllw c3a,%%mm7; paddw c4,%%mm7; pmulhw 3*16+0*2(%1),%%mm7;
	movq 5*16+0*2(%0),%%mm4; psllw c3a,%%mm4; paddw c4,%%mm4; pmulhw 5*16+0*2(%1),%%mm4;
	movq 7*16+0*2(%0),%%mm5; psllw c3a,%%mm5; paddw c4,%%mm5; pmulhw 7*16+0*2(%1),%%mm5;

	movq cC4_14,%%mm3;

	movq %%mm7,%%mm0; paddsw %%mm4,%%mm7; psubsw %%mm0,%%mm4;
	movq %%mm5,%%mm1; paddsw %%mm6,%%mm5; psubsw %%mm1,%%mm6;
	
	movq %%mm7,%%mm2; paddsw %%mm5,%%mm7; psubsw %%mm2,%%mm5;

	psllw $2,%%mm7; pmulhw c1C6_13,%%mm7;
	psllw $2,%%mm5; pmulhw cC4C6_14,%%mm5;

	movq %%mm6,%%mm2; movq %%mm4,%%mm1;
	paddsw %%mm6,%%mm6; paddsw %%mm4,%%mm4;
	paddsw %%mm4,%%mm2; psubsw %%mm6,%%mm1;
	pmulhw %%mm3,%%mm4; pmulhw %%mm3,%%mm6;

	psubsw %%mm7,%%mm6;	
	psubsw %%mm1,%%mm6;
	psubsw %%mm6,%%mm5;	
	paddsw %%mm2,%%mm4;
	psubsw %%mm5,%%mm4;

	movq %%mm4,mm8;

	/* Top 8 x 4 even */

	movq 2*16+0*2(%0),%%mm1; psllw c3a,%%mm1; paddw c4,%%mm1; pmulhw  2*16+0*2(%1),%%mm1;
	movq 6*16+0*2(%0),%%mm0; psllw c3a,%%mm0; paddw c4,%%mm0; pmulhw  6*16+0*2(%1),%%mm0;

	paddsw %%mm1,%%mm1; 
	movq %%mm0,%%mm4; paddsw %%mm1,%%mm0; psubsw %%mm4,%%mm1;
	movq %%mm1,%%mm4; paddsw %%mm4,%%mm4; pmulhw %%mm3,%%mm4;

	paddsw %%mm4,%%mm1;
	psubsw %%mm0,%%mm1;

	movq 0*16+0*2(%0),%%mm2; psllw c3a,%%mm2; paddw c4,%%mm2; pmulhw 0*16+0*2(%1),%%mm2;
	movq 4*16+0*2(%0),%%mm3; psllw c3a,%%mm3; paddw c4,%%mm3; pmulhw 4*16+0*2(%1),%%mm3;

	movq 0*16+0*2(%0),%%mm4;
	psllq $48,%%mm4;
	psrlq $43,%%mm4;
	paddw %%mm4,%%mm2;

	movq %%mm3,%%mm4; paddsw %%mm2,%%mm3; psubsw %%mm4,%%mm2;
	movq %%mm0,%%mm4; paddsw %%mm3,%%mm0; psubsw %%mm4,%%mm3;
	movq %%mm1,%%mm4; paddsw %%mm2,%%mm1; psubsw %%mm4,%%mm2;

	movq mm8,%%mm4;

	paddw %%mm0,%%mm7; paddw %%mm0,%%mm0; psubw %%mm7,%%mm0;
	paddw %%mm1,%%mm6; paddw %%mm1,%%mm1; psubw %%mm6,%%mm1;
	paddw %%mm2,%%mm5; paddw %%mm2,%%mm2; psubw %%mm5,%%mm2;
	paddw %%mm3,%%mm4; paddw %%mm3,%%mm3; psubw %%mm4,%%mm3;

	movq %%mm0,mm8;

	movq %%mm7,%%mm0;
	punpcklwd %%mm6,%%mm7;	// 1b 0b 1a 0a
	punpckhwd %%mm6,%%mm0;	// 1d 0d 1c 0c
	movq %%mm5,%%mm6;
	punpcklwd %%mm4,%%mm5;	// 3b 2b 3a 2a
	punpckhwd %%mm4,%%mm6;	// 3d 2d 3c 2c
	movq %%mm7,%%mm4;
	punpckldq %%mm5,%%mm7;	// 3a 2a 1a 0a
	movq %%mm7, 0*16+0*2(%2);
	punpckhdq %%mm5,%%mm4;	// 3b 2b 1b 0b
	movq %%mm4, 1*16+0*2(%2);
	movq %%mm0,%%mm5;
	punpckldq %%mm6,%%mm5;	// 3c 2c 1c 0c
	movq %%mm5, 2*16+0*2(%2);
	punpckhdq %%mm6,%%mm0;	// 3d 2d 1d 0d
	movq %%mm0, 3*16+0*2(%2);

	movq mm8,%%mm0;

	movq %%mm3,%%mm7;
	punpcklwd %%mm2,%%mm3;	// 1b 0b 1a 0a
	punpckhwd %%mm2,%%mm7;	// 1d 0d 1c 0c
	movq %%mm1,%%mm2;
	punpcklwd %%mm0,%%mm1;	// 3b 2b 3a 2a
	punpckhwd %%mm0,%%mm2;	// 3d 2d 3c 2c
	movq %%mm3,%%mm0;
	punpckldq %%mm1,%%mm3;	// 3a 2a 1a 0a
	movq %%mm3, 0*16+4*2(%2);
	punpckhdq %%mm1,%%mm0;	// 3b 2b 1b 0b
	movq %%mm0, 1*16+4*2(%2);
	movq %%mm7,%%mm1;
	punpckldq %%mm2,%%mm1;	// 3c 2c 1c 0c
	movq %%mm1, 2*16+4*2(%2);
	punpckhdq %%mm2,%%mm7;	// 3d 2d 1d 0d
	movq %%mm7, 3*16+4*2(%2);

	
	/* Bottom 8 x 4 odd */

	movq 1*16+4*2(%0),%%mm6; psllw c3a,%%mm6; paddw c4,%%mm6; pmulhw  1*16+4*2(%1),%%mm6;
	movq 3*16+4*2(%0),%%mm7; psllw c3a,%%mm7; paddw c4,%%mm7; pmulhw  3*16+4*2(%1),%%mm7;
	movq 5*16+4*2(%0),%%mm4; psllw c3a,%%mm4; paddw c4,%%mm4; pmulhw  5*16+4*2(%1),%%mm4;
	movq 7*16+4*2(%0),%%mm5; psllw c3a,%%mm5; paddw c4,%%mm5; pmulhw  7*16+4*2(%1),%%mm5;

	movq %%mm7,%%mm0; paddsw %%mm4,%%mm7; psubsw %%mm0,%%mm4;
	movq %%mm5,%%mm1; paddsw %%mm6,%%mm5; psubsw %%mm1,%%mm6;
	movq %%mm7,%%mm2; paddsw %%mm5,%%mm7; psubsw %%mm2,%%mm5;

	psllw $2,%%mm7; pmulhw c1C6_13,%%mm7;
	psllw $2,%%mm5; pmulhw cC4C6_14,%%mm5;

	movq %%mm6,%%mm0; movq %%mm4,%%mm1;
	paddsw %%mm6,%%mm6; paddsw %%mm4,%%mm4;
	paddsw %%mm4,%%mm0; psubsw %%mm6,%%mm1;

	pmulhw cC4_14,%%mm4;	pmulhw cC4_14,%%mm6;

	psubsw %%mm7,%%mm6;	
	psubsw %%mm1,%%mm6;
	psubsw %%mm6,%%mm5;	
	paddsw %%mm0,%%mm4;
	psubsw %%mm5,%%mm4;

	movq %%mm4,mm8;

	/* Bottom 8 x 4 even */

	movq 2*16+4*2(%0),%%mm1; psllw c3a,%%mm1; paddw c4,%%mm1; pmulhw  2*16+4*2(%1),%%mm1;
	movq 6*16+4*2(%0),%%mm0; psllw c3a,%%mm0; paddw c4,%%mm0; pmulhw  6*16+4*2(%1),%%mm0;

	paddsw %%mm1,%%mm1; 
	movq %%mm0,%%mm3; paddsw %%mm1,%%mm0; psubsw %%mm3,%%mm1;
	movq %%mm1,%%mm4; 
	pmulhw cC4_15,%%mm4;
	
	paddsw %%mm4,%%mm1;
	psubsw %%mm0,%%mm1;

	movq 0*16+4*2(%0),%%mm2; psllw c3a,%%mm2; paddw c4,%%mm2; pmulhw  0*16+4*2(%1),%%mm2;
	movq 4*16+4*2(%0),%%mm3; psllw c3a,%%mm3; paddw c4,%%mm3; pmulhw  4*16+4*2(%1),%%mm3;

	movq %%mm3,%%mm4; paddsw %%mm2,%%mm3; psubsw %%mm4,%%mm2;
	movq %%mm0,%%mm4; paddsw %%mm3,%%mm0; psubsw %%mm4,%%mm3;
	movq %%mm1,%%mm4; paddsw %%mm2,%%mm1; psubsw %%mm4,%%mm2;

	movq mm8,%%mm4;

	paddw %%mm0,%%mm7; paddw %%mm0,%%mm0; psubw %%mm7,%%mm0;
	paddw %%mm1,%%mm6; paddw %%mm1,%%mm1; psubw %%mm6,%%mm1;
	paddw %%mm2,%%mm5; paddw %%mm2,%%mm2; psubw %%mm5,%%mm2;
	paddw %%mm3,%%mm4; paddw %%mm3,%%mm3; psubw %%mm4,%%mm3;

	movq %%mm0,mm8;

	movq %%mm7,%%mm0;
	punpcklwd %%mm6,%%mm7;	// 1b 0b 1a 0a
	punpckhwd %%mm6,%%mm0;	// 1d 0d 1c 0c
	movq %%mm5,%%mm6;
	punpcklwd %%mm4,%%mm5;	// 3b 2b 3a 2a
	punpckhwd %%mm4,%%mm6;	// 3d 2d 3c 2c
	movq %%mm7,%%mm4;
	punpckldq %%mm5,%%mm7;	// 3a 2a 1a 0a
	movq %%mm7, 4*16+0*2(%2);
	punpckhdq %%mm5,%%mm4;	// 3b 2b 1b 0b
	movq %%mm4, 5*16+0*2(%2);
	movq %%mm0,%%mm5;
	punpckldq %%mm6,%%mm0;	// 3c 2c 1c 0c
	movq %%mm0, 6*16+0*2(%2);
	punpckhdq %%mm6,%%mm5;	// 3d 2d 1d 0d
	movq %%mm5, 7*16+0*2(%2);

	movq mm8,%%mm0;

	movq %%mm3,%%mm7;
	punpcklwd %%mm2,%%mm3;	// 1b 0b 1a 0a
	punpckhwd %%mm2,%%mm7;	// 1d 0d 1c 0c
	movq %%mm1,%%mm2;
	punpcklwd %%mm0,%%mm1;	// 3b 2b 3a 2a
	punpckhwd %%mm0,%%mm2;	// 3d 2d 3c 2c
	movq %%mm3,%%mm0;
	punpckldq %%mm1,%%mm3;	// 3a 2a 1a 0a
	movq %%mm3, 4*16+4*2(%2);
	punpckhdq %%mm1,%%mm0;	// 3b 2b 1b 0b
	movq %%mm0, 5*16+4*2(%2);
	movq %%mm7,%%mm1;
	punpckldq %%mm2,%%mm1;	// 3c 2c 1c 0c
	movq %%mm1, 6*16+4*2(%2);
	punpckhdq %%mm2,%%mm7;	// 3d 2d 1d 0d
	movq %%mm7, 7*16+4*2(%2);

	/* Right 4 x 8 odd */

	movq 1*16+4*2(%2),%%mm6;
	movq 3*16+4*2(%2),%%mm7;
	movq 5*16+4*2(%2),%%mm4;
	movq 7*16+4*2(%2),%%mm5;

	movq %%mm7,%%mm0; paddsw %%mm4,%%mm7; psubsw %%mm0,%%mm4;
	movq %%mm5,%%mm1; paddsw %%mm6,%%mm5; psubsw %%mm1,%%mm6;
	movq %%mm7,%%mm2; paddsw %%mm5,%%mm7; psubsw %%mm2,%%mm5;

	psllw $2,%%mm7; 
	psllw $2,%%mm5; 
/**/
	pmulhw c1C6_13,%%mm7; 
	pmulhw cC4C6_14,%%mm5; 

	movq %%mm6,%%mm0; movq %%mm4,%%mm1;
	paddsw %%mm6,%%mm6; paddsw %%mm4,%%mm4;
	paddsw %%mm4,%%mm0; psubsw %%mm6,%%mm1;

	pmulhw cC4_14,%%mm4;	pmulhw cC4_14,%%mm6;

	psubsw %%mm7,%%mm6;	
	psubsw %%mm1,%%mm6;
	psubsw %%mm6,%%mm5;	
	paddsw %%mm0,%%mm4;
	psubsw %%mm5,%%mm4;

	movq %%mm4,mm8;

	/* Right 4 x 8 even */

	movq 2*16+4*2(%2),%%mm1;
	movq 6*16+4*2(%2),%%mm0;

	movq %%mm0,%%mm3; paddsw %%mm1,%%mm0; psubsw %%mm3,%%mm1;
	movq %%mm1,%%mm4; pmulhw cC4_15,%%mm4;
	paddsw %%mm4,%%mm1;
	psubsw %%mm0,%%mm1;

	movq 0*16+4*2(%2),%%mm2;
	movq 4*16+4*2(%2),%%mm3;

	movq %%mm3,%%mm4; paddsw %%mm2,%%mm3; psubsw %%mm4,%%mm2;
	movq %%mm0,%%mm4; paddsw %%mm3,%%mm0; psubsw %%mm4,%%mm3;
	movq %%mm1,%%mm4; paddsw %%mm2,%%mm1; psubsw %%mm4,%%mm2;

	movq mm8,%%mm4;

	paddw c5a,%%mm0;
	paddw c5a,%%mm1;
	paddw c5a,%%mm2;
	paddw c5a,%%mm3;

	paddw %%mm0,%%mm7; paddw %%mm0,%%mm0; psubw %%mm7,%%mm0;
	paddw %%mm1,%%mm6; paddw %%mm1,%%mm1; psubw %%mm6,%%mm1;
	paddw %%mm2,%%mm5; paddw %%mm2,%%mm2; psubw %%mm5,%%mm2;
	paddw %%mm3,%%mm4; paddw %%mm3,%%mm3; psubw %%mm4,%%mm3;

	psraw $5,%%mm0; psraw $5,%%mm7;
	psraw $5,%%mm1; psraw $5,%%mm6;
	psraw $5,%%mm2; psraw $5,%%mm5;
	psraw $5,%%mm3; psraw $5,%%mm4;

	movq %%mm7, 0*16+4*2(%2);
	movq %%mm6, 1*16+4*2(%2);
	movq %%mm5, 2*16+4*2(%2);
	movq %%mm4, 3*16+4*2(%2);
	movq %%mm3, 4*16+4*2(%2);
	movq %%mm2, 5*16+4*2(%2);
	movq %%mm1, 6*16+4*2(%2);
	movq %%mm0, 7*16+4*2(%2);

	/* Left 4 x 8 odd */

	movq 1*16+0*2(%2),%%mm6;
	movq 3*16+0*2(%2),%%mm7;
	movq 5*16+0*2(%2),%%mm4;
	movq 7*16+0*2(%2),%%mm5;

	movq %%mm7,%%mm0; paddsw %%mm4,%%mm7; psubsw %%mm0,%%mm4;
	movq %%mm5,%%mm1; paddsw %%mm6,%%mm5; psubsw %%mm1,%%mm6;
	movq %%mm7,%%mm2; paddsw %%mm5,%%mm7; psubsw %%mm2,%%mm5;

	psllw $2,%%mm7; 
	psllw $2,%%mm5; 
/**/
	pmulhw c1C6_13,%%mm7;	
	pmulhw cC4C6_14,%%mm5;	

	movq %%mm6,%%mm0; movq %%mm4,%%mm1;
	paddsw %%mm6,%%mm6; paddsw %%mm4,%%mm4;
	paddsw %%mm4,%%mm0; psubsw %%mm6,%%mm1;

	pmulhw cC4_14,%%mm4;	pmulhw cC4_14,%%mm6;

	psubsw %%mm7,%%mm6;	
	psubsw %%mm1,%%mm6;
	psubsw %%mm6,%%mm5;	
	paddsw %%mm0,%%mm4;
	psubsw %%mm5,%%mm4;

	movq %%mm4,mm8;
	movq %%mm5,mm9;

	/* Left 4 x 8 even */

	movq 2*16+0*2(%2),%%mm1;
	movq 6*16+0*2(%2),%%mm0;
	movq 0*16+0*2(%2),%%mm2;
	movq 4*16+0*2(%2),%%mm3;

	movq %%mm0,%%mm4; paddsw %%mm1,%%mm0; psubsw %%mm4,%%mm1;
	movq %%mm1,%%mm5; pmulhw cC4_15,%%mm5;

	paddsw %%mm5,%%mm1;
	psubsw %%mm0,%%mm1;

	movq %%mm3,%%mm4; paddsw %%mm2,%%mm3; psubsw %%mm4,%%mm2;
	movq %%mm0,%%mm5; paddsw %%mm3,%%mm0; psubsw %%mm5,%%mm3;
	movq %%mm1,%%mm4; paddsw %%mm2,%%mm1; psubsw %%mm4,%%mm2;

	movq mm9,%%mm5;
	movq mm8,%%mm4;

	paddw c5a,%%mm0; 
	paddw c5a,%%mm1; 
	paddw c5a,%%mm2; 
	paddw c5a,%%mm3; 

	paddw %%mm0,%%mm7; paddw %%mm0,%%mm0; psubw %%mm7,%%mm0;
	paddw %%mm1,%%mm6; paddw %%mm1,%%mm1; psubw %%mm6,%%mm1;
	paddw %%mm2,%%mm5; paddw %%mm2,%%mm2; psubw %%mm5,%%mm2;
	paddw %%mm3,%%mm4; paddw %%mm3,%%mm3; psubw %%mm4,%%mm3;

	psraw $5,%%mm0; psraw $5,%%mm7;
	psraw $5,%%mm1; psraw $5,%%mm6;
	psraw $5,%%mm2; psraw $5,%%mm5;
	psraw $5,%%mm3; psraw $5,%%mm4;

	/* Prediction */

	packuswb  0*16+4*2(%2), %%mm7;
	packuswb  1*16+4*2(%2), %%mm6;
	packuswb  2*16+4*2(%2), %%mm5;
	packuswb  3*16+4*2(%2), %%mm4;
	packuswb  4*16+4*2(%2), %%mm3;
	packuswb  5*16+4*2(%2), %%mm2;
	packuswb  6*16+4*2(%2), %%mm1;
	packuswb  7*16+4*2(%2), %%mm0;

	pushl	%0;
	leal	(%3,%4),%0;
	movq %%mm7, (%3);	// 0
	movq %%mm6, (%3,%4);	// 1
	movq %%mm5, (%3,%4,2);	// 2
	movq %%mm4, (%0,%4,2);	// 3
	leal	(%0,%4,4),%0;
	movq %%mm3, (%3,%4,4);	// 4
	movq %%mm2, (%0);	// 5
	movq %%mm1, (%0,%4);	// 6
	movq %%mm0, (%0,%4,2);	// 7
	popl	%0;
1:

" :: "r" (&mblock[1][i][0][0]), "r" (lut1p), 
  "r" (&mblock[0][0][0][0]), "r" (new), "r" (mb_address.block[i].pitch)
  : "cc", "memory" FPU_REGS);

    }

}

/* preliminary */

short t1s[8][8] __attribute__ ((aligned (32)));

void
mmx_mpeg1_idct_inter(unsigned int cbp)
{
    int i;
    unsigned char *new = newref;

  for (i = 0; i < 6; i++) {
   new += mb_address.block[i].offset;
    if (cbp & (0x20 >> i)) 
    {
asm("
	movq 1*16+0*2(%0),%%mm6; psllw c3b,%%mm6; paddw c4,%%mm6; pmulhw  1*16+0*2(%1),%%mm6;
	movq 3*16+0*2(%0),%%mm7; psllw c3b,%%mm7; paddw c4,%%mm7; pmulhw  3*16+0*2(%1),%%mm7;
	movq 5*16+0*2(%0),%%mm4; psllw c3b,%%mm4; paddw c4,%%mm4; pmulhw  5*16+0*2(%1),%%mm4;
	movq 7*16+0*2(%0),%%mm5; psllw c3b,%%mm5; paddw c4,%%mm5; pmulhw  7*16+0*2(%1),%%mm5;

	movq cC4_14,%%mm3;

	movq %%mm7,%%mm0; paddsw %%mm4,%%mm7; psubsw %%mm0,%%mm4;
	movq %%mm5,%%mm1; paddsw %%mm6,%%mm5; psubsw %%mm1,%%mm6;
	movq %%mm7,%%mm2; paddsw %%mm5,%%mm7; psubsw %%mm2,%%mm5;

	psllw $2,%%mm7; pmulhw c1C6_13,%%mm7;
	psllw $2,%%mm5; pmulhw cC4C6_14,%%mm5;

	movq %%mm6,%%mm2; movq %%mm4,%%mm1;
	paddsw %%mm6,%%mm6; paddsw %%mm4,%%mm4;
	paddsw %%mm4,%%mm2; psubsw %%mm6,%%mm1;
	pmulhw %%mm3,%%mm4; pmulhw %%mm3,%%mm6;

	psubsw %%mm7,%%mm6;	
	psubsw %%mm1,%%mm6;
	psubsw %%mm6,%%mm5;	
	paddsw %%mm2,%%mm4;
	psubsw %%mm5,%%mm4;

	movq %%mm4,mm8;

	/* Left Even */

	movq 2*16+0*2(%0),%%mm1; psllw c3b,%%mm1; paddw c4,%%mm1; pmulhw  2*16+0*2(%1),%%mm1;
	movq 6*16+0*2(%0),%%mm0; psllw c3b,%%mm0; paddw c4,%%mm0; pmulhw  6*16+0*2(%1),%%mm0;

	movq %%mm0,%%mm4; paddsw %%mm1,%%mm0; psubsw %%mm4,%%mm1;
	movq %%mm1,%%mm4; paddsw %%mm4,%%mm4; pmulhw %%mm3,%%mm4;

	paddsw %%mm4,%%mm1;
	psubsw %%mm0,%%mm1;

	movq 0*16+0*2(%0),%%mm2; psllw c3b,%%mm2; paddw c4,%%mm2; pmulhw  0*16+0*2(%1),%%mm2;
	movq 4*16+0*2(%0),%%mm3; psllw c3b,%%mm3; paddw c4,%%mm3; pmulhw  4*16+0*2(%1),%%mm3;

	movq %%mm3,%%mm4; paddsw %%mm2,%%mm3; psubsw %%mm4,%%mm2;
	movq %%mm0,%%mm4; paddsw %%mm3,%%mm0; psubsw %%mm4,%%mm3;
	movq %%mm1,%%mm4; paddsw %%mm2,%%mm1; psubsw %%mm4,%%mm2;

	movq mm8,%%mm4;

	paddsw %%mm0,%%mm7; paddsw %%mm0,%%mm0; psubsw %%mm7,%%mm0;
	paddsw %%mm1,%%mm6; paddsw %%mm1,%%mm1; psubsw %%mm6,%%mm1;
	paddsw %%mm2,%%mm5; paddsw %%mm2,%%mm2; psubsw %%mm5,%%mm2;
	paddsw %%mm3,%%mm4; paddsw %%mm3,%%mm3; psubsw %%mm4,%%mm3;

	movq %%mm0,mm8;

	movq %%mm7,%%mm0;
	punpcklwd %%mm6,%%mm7;	// 1b 0b 1a 0a
	punpckhwd %%mm6,%%mm0;	// 1d 0d 1c 0c
	movq %%mm5,%%mm6;
	punpcklwd %%mm4,%%mm5;	// 3b 2b 3a 2a
	punpckhwd %%mm4,%%mm6;	// 3d 2d 3c 2c
	movq %%mm7,%%mm4;
	punpckldq %%mm5,%%mm7;	// 3a 2a 1a 0a
	movq %%mm7, 0*16+0*2(%2);
	punpckhdq %%mm5,%%mm4;	// 3b 2b 1b 0b
	movq %%mm4, 1*16+0*2(%2);
	movq %%mm0,%%mm5;
	punpckldq %%mm6,%%mm5;	// 3c 2c 1c 0c
	movq %%mm5, 2*16+0*2(%2);
	punpckhdq %%mm6,%%mm0;	// 3d 2d 1d 0d
	movq %%mm0, 3*16+0*2(%2);

	movq mm8,%%mm0;

	movq %%mm3,%%mm7;
	punpcklwd %%mm2,%%mm3;	// 1b 0b 1a 0a
	punpckhwd %%mm2,%%mm7;	// 1d 0d 1c 0c
	movq %%mm1,%%mm2;
	punpcklwd %%mm0,%%mm1;	// 3b 2b 3a 2a
	punpckhwd %%mm0,%%mm2;	// 3d 2d 3c 2c
	movq %%mm3,%%mm0;
	punpckldq %%mm1,%%mm3;	// 3a 2a 1a 0a
	movq %%mm3, 0*16+4*2(%2);
	punpckhdq %%mm1,%%mm0;	// 3b 2b 1b 0b
	movq %%mm0, 1*16+4*2(%2);
	movq %%mm7,%%mm1;
	punpckldq %%mm2,%%mm1;	// 3c 2c 1c 0c
	movq %%mm1, 2*16+4*2(%2);
	punpckhdq %%mm2,%%mm7;	// 3d 2d 1d 0d
	movq %%mm7, 3*16+4*2(%2);

	/* Right odd */

	movq 1*16+4*2(%0),%%mm6; psllw c3b,%%mm6; paddw c4,%%mm6; pmulhw  1*16+4*2(%1),%%mm6;
	movq 3*16+4*2(%0),%%mm7; psllw c3b,%%mm7; paddw c4,%%mm7; pmulhw  3*16+4*2(%1),%%mm7;
	movq 5*16+4*2(%0),%%mm4; psllw c3b,%%mm4; paddw c4,%%mm4; pmulhw  5*16+4*2(%1),%%mm4;
	movq 7*16+4*2(%0),%%mm5; psllw c3b,%%mm5; paddw c4,%%mm5; pmulhw  7*16+4*2(%1),%%mm5;


	movq %%mm7,%%mm0; paddsw %%mm4,%%mm7; psubsw %%mm0,%%mm4;
	movq %%mm5,%%mm1; paddsw %%mm6,%%mm5; psubsw %%mm1,%%mm6;
	movq %%mm7,%%mm2; paddsw %%mm5,%%mm7; psubsw %%mm2,%%mm5;

	psllw $2,%%mm7; pmulhw c1C6_13,%%mm7;
	psllw $2,%%mm5; pmulhw cC4C6_14,%%mm5;

	movq %%mm6,%%mm0; movq %%mm4,%%mm1;
	paddsw %%mm6,%%mm6; paddsw %%mm4,%%mm4;
	paddsw %%mm4,%%mm0; psubsw %%mm6,%%mm1;

	pmulhw cC4_14,%%mm4;	pmulhw cC4_14,%%mm6;

	psubsw %%mm7,%%mm6;	
	psubsw %%mm1,%%mm6;
	psubsw %%mm6,%%mm5;	
	paddsw %%mm0,%%mm4;
	psubsw %%mm5,%%mm4;

	movq %%mm4,mm8;

	/* Right Even */

	movq 2*16+4*2(%0),%%mm1; psllw c3b,%%mm1; paddw c4,%%mm1; pmulhw  2*16+4*2(%1),%%mm1;
	movq 6*16+4*2(%0),%%mm0; psllw c3b,%%mm0; paddw c4,%%mm0; pmulhw  6*16+4*2(%1),%%mm0;

	movq %%mm0,%%mm3; paddsw %%mm1,%%mm0; psubsw %%mm3,%%mm1;
	movq %%mm1,%%mm4; paddsw %%mm4,%%mm4; pmulhw cC4_14,%%mm4;
	
	paddsw %%mm4,%%mm1;
	psubsw %%mm0,%%mm1;

	movq 0*16+4*2(%0),%%mm2; psllw c3b,%%mm2; paddw c4,%%mm2; pmulhw  0*16+4*2(%1),%%mm2;
	movq 4*16+4*2(%0),%%mm3; psllw c3b,%%mm3; paddw c4,%%mm3; pmulhw  4*16+4*2(%1),%%mm3;

	movq %%mm3,%%mm4; paddsw %%mm2,%%mm3; psubsw %%mm4,%%mm2;
	movq %%mm0,%%mm4; paddsw %%mm3,%%mm0; psubsw %%mm4,%%mm3;
	movq %%mm1,%%mm4; paddsw %%mm2,%%mm1; psubsw %%mm4,%%mm2;

	movq mm8,%%mm4;

	paddsw %%mm0,%%mm7; paddsw %%mm0,%%mm0; psubsw %%mm7,%%mm0;
	paddsw %%mm1,%%mm6; paddsw %%mm1,%%mm1; psubsw %%mm6,%%mm1;
	paddsw %%mm2,%%mm5; paddsw %%mm2,%%mm2; psubsw %%mm5,%%mm2;
	paddsw %%mm3,%%mm4; paddsw %%mm3,%%mm3; psubsw %%mm4,%%mm3;

	movq %%mm0,mm8;

	movq %%mm7,%%mm0;
	punpcklwd %%mm6,%%mm7;	// 1b 0b 1a 0a
	punpckhwd %%mm6,%%mm0;	// 1d 0d 1c 0c
	movq %%mm5,%%mm6;
	punpcklwd %%mm4,%%mm5;	// 3b 2b 3a 2a
	punpckhwd %%mm4,%%mm6;	// 3d 2d 3c 2c
	movq %%mm7,%%mm4;
	punpckldq %%mm5,%%mm7;	// 3a 2a 1a 0a
	movq %%mm7, 4*16+0*2(%2);
	punpckhdq %%mm5,%%mm4;	// 3b 2b 1b 0b
	movq %%mm4, 5*16+0*2(%2);
	movq %%mm0,%%mm5;
	punpckldq %%mm6,%%mm0;	// 3c 2c 1c 0c
	movq %%mm0, 6*16+0*2(%2);
	punpckhdq %%mm6,%%mm5;	// 3d 2d 1d 0d
	movq %%mm5, 7*16+0*2(%2);

	movq mm8,%%mm0;

	movq %%mm3,%%mm7;
	punpcklwd %%mm2,%%mm3;	// 1b 0b 1a 0a
	punpckhwd %%mm2,%%mm7;	// 1d 0d 1c 0c
	movq %%mm1,%%mm2;
	punpcklwd %%mm0,%%mm1;	// 3b 2b 3a 2a
	punpckhwd %%mm0,%%mm2;	// 3d 2d 3c 2c
	movq %%mm3,%%mm0;
	punpckldq %%mm1,%%mm3;	// 3a 2a 1a 0a
	movq %%mm3, 4*16+4*2(%2);
	punpckhdq %%mm1,%%mm0;	// 3b 2b 1b 0b
	movq %%mm0, 5*16+4*2(%2);
	movq %%mm7,%%mm1;
	punpckldq %%mm2,%%mm1;	// 3c 2c 1c 0c
	movq %%mm1, 6*16+4*2(%2);
	punpckhdq %%mm2,%%mm7;	// 3d 2d 1d 0d
	movq %%mm7, 7*16+4*2(%2);

	/* Bottom odd */

	movq 1*16+4*2(%2),%%mm6;
	movq 3*16+4*2(%2),%%mm7;
	movq 5*16+4*2(%2),%%mm4;
	movq 7*16+4*2(%2),%%mm5;

	movq %%mm7,%%mm0; paddsw %%mm4,%%mm7; psubsw %%mm0,%%mm4;
	movq %%mm5,%%mm1; paddsw %%mm6,%%mm5; psubsw %%mm1,%%mm6;
	movq %%mm7,%%mm2; paddsw %%mm5,%%mm7; psubsw %%mm2,%%mm5;

	psllw $2,%%mm7; pmulhw c1C6_13,%%mm7; 
	psllw $2,%%mm5; pmulhw cC4C6_14,%%mm5; 

	movq %%mm6,%%mm0; movq %%mm4,%%mm1;
	paddsw %%mm6,%%mm6; paddsw %%mm4,%%mm4;
	paddsw %%mm4,%%mm0; psubsw %%mm6,%%mm1;

	pmulhw cC4_14,%%mm4;	pmulhw cC4_14,%%mm6;

	psubsw %%mm7,%%mm6;	
	psubsw %%mm1,%%mm6;
	psubsw %%mm6,%%mm5;	
	paddsw %%mm0,%%mm4;
	psubsw %%mm5,%%mm4;

	movq %%mm4,mm8;

	/* Bottom even */

	movq 2*16+4*2(%2),%%mm1;
	movq 6*16+4*2(%2),%%mm0;

	movq %%mm0,%%mm3; paddsw %%mm1,%%mm0; psubsw %%mm3,%%mm1;
	movq %%mm1,%%mm4; paddsw %%mm4,%%mm4; pmulhw cC4_14,%%mm4;
	paddsw %%mm4,%%mm1;
	psubsw %%mm0,%%mm1;

	movq 0*16+4*2(%2),%%mm2;
	movq 4*16+4*2(%2),%%mm3;

	movq %%mm3,%%mm4; paddsw %%mm2,%%mm3; psubsw %%mm4,%%mm2;
	movq %%mm0,%%mm4; paddsw %%mm3,%%mm0; psubsw %%mm4,%%mm3;
	movq %%mm1,%%mm4; paddsw %%mm2,%%mm1; psubsw %%mm4,%%mm2;

	movq mm8,%%mm4;

	paddw c5b,%%mm0; 
	paddw c5b,%%mm1; 
	paddw c5b,%%mm2; 
	paddw c5b,%%mm3; 

	paddsw %%mm0,%%mm7; paddsw %%mm0,%%mm0; psubsw %%mm7,%%mm0;
	paddsw %%mm1,%%mm6; paddsw %%mm1,%%mm1; psubsw %%mm6,%%mm1;
	paddsw %%mm2,%%mm5; paddsw %%mm2,%%mm2; psubsw %%mm5,%%mm2;
	paddsw %%mm3,%%mm4; paddsw %%mm3,%%mm3; psubsw %%mm4,%%mm3;

	psraw $5,%%mm0; psraw $5,%%mm7;
	psraw $5,%%mm1; psraw $5,%%mm6;
	psraw $5,%%mm2; psraw $5,%%mm5;
	psraw $5,%%mm3; psraw $5,%%mm4;

	paddsw  0*16+4*2+3*768(%0),%%mm7;	// old reference
	paddsw  1*16+4*2+3*768(%0),%%mm6;
	paddsw  2*16+4*2+3*768(%0),%%mm5;
	paddsw  3*16+4*2+3*768(%0),%%mm4;
	paddsw  4*16+4*2+3*768(%0),%%mm3;
	paddsw  5*16+4*2+3*768(%0),%%mm2;
	paddsw  6*16+4*2+3*768(%0),%%mm1;
	paddsw  7*16+4*2+3*768(%0),%%mm0;

	movq %%mm7, 0*16+4*2+t1s;
	movq %%mm6, 1*16+4*2+t1s;
	movq %%mm5, 2*16+4*2+t1s;
	movq %%mm4, 3*16+4*2+t1s;
	
	movq %%mm3, 4*16+4*2+t1s;
	movq %%mm2, 5*16+4*2+t1s;
	movq %%mm1, 6*16+4*2+t1s;
	movq %%mm0, 7*16+4*2+t1s;

	/* Top odd */

	movq 1*16+0*2(%2),%%mm6;
	movq 3*16+0*2(%2),%%mm7;
	movq 5*16+0*2(%2),%%mm4;
	movq 7*16+0*2(%2),%%mm5;

	movq %%mm7,%%mm0; paddsw %%mm4,%%mm7; psubsw %%mm0,%%mm4;
	movq %%mm5,%%mm1; paddsw %%mm6,%%mm5; psubsw %%mm1,%%mm6;
	movq %%mm7,%%mm2; paddsw %%mm5,%%mm7; psubsw %%mm2,%%mm5;

	psllw $2,%%mm7; pmulhw c1C6_13,%%mm7;	
	psllw $2,%%mm5; pmulhw cC4C6_14,%%mm5;	

	movq %%mm6,%%mm0; movq %%mm4,%%mm1;
	paddsw %%mm6,%%mm6; paddsw %%mm4,%%mm4;
	paddsw %%mm4,%%mm0; psubsw %%mm6,%%mm1;

	pmulhw cC4_14,%%mm4;	pmulhw cC4_14,%%mm6;

	psubsw %%mm7,%%mm6;	
	psubsw %%mm1,%%mm6;
	psubsw %%mm6,%%mm5;	
	paddsw %%mm0,%%mm4;
	psubsw %%mm5,%%mm4;

	movq %%mm4,mm8;
	movq %%mm5,mm9;

	/* Top Even */

	movq 2*16+0*2(%2),%%mm1;
	movq 6*16+0*2(%2),%%mm0;
	movq 0*16+0*2(%2),%%mm2;
	movq 4*16+0*2(%2),%%mm3;

	movq %%mm0,%%mm4; paddsw %%mm1,%%mm0; psubsw %%mm4,%%mm1;
	movq %%mm1,%%mm5; paddsw %%mm5,%%mm5; pmulhw cC4_14,%%mm5;

	paddsw %%mm5,%%mm1;
	psubsw %%mm0,%%mm1;

	movq %%mm3,%%mm4; paddsw %%mm2,%%mm3; psubsw %%mm4,%%mm2;
	movq %%mm0,%%mm5; paddsw %%mm3,%%mm0; psubsw %%mm5,%%mm3;
	movq %%mm1,%%mm4; paddsw %%mm2,%%mm1; psubsw %%mm4,%%mm2;

	movq mm8,%%mm4;
	movq mm9,%%mm5;

	paddw c5b,%%mm0; 
	paddw c5b,%%mm1; 
	paddw c5b,%%mm2; 
	paddw c5b,%%mm3; 

	paddsw %%mm0,%%mm7; paddsw %%mm0,%%mm0; psubsw %%mm7,%%mm0;
	paddsw %%mm1,%%mm6; paddsw %%mm1,%%mm1; psubsw %%mm6,%%mm1;
	paddsw %%mm2,%%mm5; paddsw %%mm2,%%mm2; psubsw %%mm5,%%mm2;
	paddsw %%mm3,%%mm4; paddsw %%mm3,%%mm3; psubsw %%mm4,%%mm3;

	psraw $5,%%mm0; psraw $5,%%mm7;
	psraw $5,%%mm1; psraw $5,%%mm6;
	psraw $5,%%mm2; psraw $5,%%mm5;
	psraw $5,%%mm3; psraw $5,%%mm4;

	paddsw  0*16+0*2+3*768(%0),%%mm7;
	paddsw  1*16+0*2+3*768(%0),%%mm6;
	paddsw  2*16+0*2+3*768(%0),%%mm5;
	paddsw  3*16+0*2+3*768(%0),%%mm4;
	paddsw  4*16+0*2+3*768(%0),%%mm3;
	paddsw  5*16+0*2+3*768(%0),%%mm2;
	paddsw  6*16+0*2+3*768(%0),%%mm1;
	paddsw  7*16+0*2+3*768(%0),%%mm0;

	packuswb  0*16+4*2+t1s, %%mm7;
	packuswb  1*16+4*2+t1s, %%mm6;
	packuswb  2*16+4*2+t1s, %%mm5;
	packuswb  3*16+4*2+t1s, %%mm4;
	packuswb  4*16+4*2+t1s, %%mm3;
	packuswb  5*16+4*2+t1s, %%mm2;
	packuswb  6*16+4*2+t1s, %%mm1;
	packuswb  7*16+4*2+t1s, %%mm0;
	
	pushl	%0;
	leal	(%4,%3),%0;
	movq %%mm7, (%4);	// 0
	movq %%mm6, (%4,%3);	// 1
	movq %%mm5, (%4,%3,2);	// 2
	movq %%mm4, (%0,%3,2);	// 3
	leal	(%0,%3,4),%0;
	movq %%mm3, (%4,%3,4);	// 4
	movq %%mm2, (%0);	// 5
	movq %%mm1, (%0,%3);	// 6
	movq %%mm0, (%0,%3,2);	// 7
	popl	%0;

" :: "r" (&mblock[0][i][0][0]),	// predicted
     "r" (lut2p),		// AAN/quant table
     "r" (&mblock[1][0][0][0]),	// temporary #1
     "r" (mb_address.block[i].pitch),
     "r" (new)			// new reference
  : "cc", "memory" FPU_REGS);

    } else {
		asm("
			pushl		%2;
			leal		(%1,%3),%2;
			movq		(%0),%%mm0;
			movq		1*8(%0),%%mm1;
			movq		2*8(%0),%%mm2;		packuswb 	%%mm1,%%mm0;
			movq		3*8(%0),%%mm3;		movq 		%%mm0,(%1);		// 0
			movq		4*8(%0),%%mm4;		packuswb	%%mm3,%%mm2;
			movq		5*8(%0),%%mm5;		movq		%%mm2,(%1,%3);		// 1
			movq		6*8(%0),%%mm6;		packuswb 	%%mm5,%%mm4;
			movq		7*8(%0),%%mm7;		movq 		%%mm4,(%1,%3,2);	// 2
			movq		8*8(%0),%%mm0;		packuswb	%%mm7,%%mm6;
			movq		9*8(%0),%%mm1;		movq		%%mm6,(%2,%3,2);	// 3
			leal		(%2,%3,4),%2;
			movq		10*8(%0),%%mm2;		packuswb 	%%mm1,%%mm0;
			movq		11*8(%0),%%mm3;		movq 		%%mm0,(%1,%3,4);	// 4
			movq		12*8(%0),%%mm4;		packuswb	%%mm3,%%mm2;
			movq		13*8(%0),%%mm5;		movq		%%mm2,(%2);		// 5
			movq		14*8(%0),%%mm6;		packuswb 	%%mm5,%%mm4;
			movq		15*8(%0),%%mm7;		movq 		%%mm4,(%2,%3);		// 6
								packuswb	%%mm7,%%mm6;
								movq		%%mm6,(%2,%3,2);	// 7
			popl		%2;
		" :: "r" (mblock[3][i]), "r" (new), "r" (0), "r" (mb_address.block[i].pitch));
    }
  }
}
