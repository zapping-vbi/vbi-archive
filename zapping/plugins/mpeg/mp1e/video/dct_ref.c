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

/* $Id: dct_ref.c,v 1.4 2000-09-29 17:54:33 mschimek Exp $ */

#include "dct.h"
#include "mpeg.h"
#include "video.h"
#include "dct/ieee.h"
#include "../common/math.h"

#define FLOAT float

static FLOAT		aan_fwd_lut[8][8];
static FLOAT		aan_inv_lut[8][8];

int			inter_quant_scale;

static void aan_lut_init(void) __attribute__ ((constructor));

static void
aan_lut_init(void)
{
	int v, u;

	for (v = 0; v < 8; v++)
		for (u = 0; u < 8; u++) {
			double Cu, Cv;

    			Cu = (u == 0) ? 1.0 : (cos(u * M_PI / 16.0) * sqrt(2.0));
			Cv = (v == 0) ? 1.0 : (cos(v * M_PI / 16.0) * sqrt(2.0));

			aan_fwd_lut[v][u] = 1.0 / (Cu * Cv * 8.0);
			aan_inv_lut[v][u] = 1.0 * (Cu * Cv / 8.0);
		}
}

static void
aan_double_1d_fdct(FLOAT *in, FLOAT *out)
{
	FLOAT tmp0, tmp1, tmp2, tmp3;
	FLOAT tmp4, tmp5, tmp6, tmp7;

	/* even */

    	tmp0 = in[0] + in[7];
	tmp1 = in[1] + in[6];
	tmp2 = in[2] + in[5];
	tmp3 = in[3] + in[4];

	tmp4 = tmp0 + tmp3;
        tmp5 = tmp1 + tmp2;

	out[0] = tmp4 + tmp5;
        out[4] = tmp4 - tmp5;

	tmp0 -= tmp3;
	tmp1 -= tmp2;

	tmp1 = (tmp0 + tmp1) * C4;

	out[2] = tmp0 + tmp1;
	out[6] = tmp0 - tmp1;

	/* odd */

	tmp7 = in[0] - in[7];
	tmp6 = in[1] - in[6];
	tmp5 = in[2] - in[5];
	tmp4 = in[3] - in[4];

	tmp4 += tmp5;
	tmp5 += tmp6;
        tmp6 += tmp7;

	tmp3 = (tmp4 - tmp6) * C6;
	tmp4 = (tmp4 * (C2 - C6)) + tmp3;
	tmp6 = (tmp6 * (C2 + C6)) + tmp3;
	tmp5 = tmp5 * C4;

	tmp3 = tmp7 + tmp5;
	tmp7 = tmp7 - tmp5;

	out[5] = tmp7 + tmp4;
        out[3] = tmp7 - tmp4;
        out[1] = tmp3 + tmp6;
        out[7] = tmp3 - tmp6;
}

static void
aan_double_1d_idct(FLOAT *in, FLOAT *out)
{
	FLOAT tmp0, tmp1, tmp2, tmp3;
	FLOAT tmp4, tmp5, tmp6, tmp7, tmp8;

	/* odd */

	tmp5 = in[5] + in[3];
	tmp6 = in[5] - in[3];
	tmp8 = in[1] + in[7];
	tmp4 = in[1] - in[7];

	tmp7 = tmp8 + tmp5;
	tmp8 = tmp8 - tmp5;

	tmp5 = +2.0 * C2 * (tmp4 + tmp6);
	tmp6 = -2.0 * (C2 + C6) * tmp6 + tmp5;
	tmp4 = -2.0 * (C2 - C6) * tmp4 + tmp5;
	tmp5 = +2.0 * C4 * tmp8;

	tmp6 -= tmp7;
	tmp5 -= tmp6;
	tmp4 -= tmp5;

	/* even */

	tmp2 = in[2] + in[6];
	tmp8 = in[2] - in[6];

	tmp8 = tmp8 * 2.0 * C4 - tmp2;

	tmp0 = in[0] + in[4];
	tmp1 = in[0] - in[4];

	tmp3 = tmp0 - tmp2;
	tmp0 = tmp0 + tmp2;
	tmp2 = tmp1 - tmp8;
	tmp1 = tmp1 + tmp8;

	out[0] = tmp0 + tmp7;
	out[1] = tmp1 + tmp6;
	out[2] = tmp2 + tmp5;
	out[3] = tmp3 + tmp4;
	out[4] = tmp3 - tmp4;
	out[5] = tmp2 - tmp5;
	out[6] = tmp1 - tmp6;
	out[7] = tmp0 - tmp7;
}

// #define SATURATE(val, min, max) saturate((val), (min), (max))
#define SATURATE(val, min, max) (val)
/*
 *  Saturation in RL/VLC routines with overflow feedback, see there.
 */

void
fdct_intra(int quant_scale)
{
	int i, j, v, u, val, div;

	emms();

	for (i = 0; i < 6; i++) {
		FLOAT F[8][8], t[8][8];

		for (v = 0; v < 64; v++)
			F[0][v] = mblock[0][i][0][v] - 128;

		for (v = 0; v < 8; v++)
			aan_double_1d_fdct(F[v], t[v]);

		mirror(t);

		for (u = 0; u < 8; u++)
			aan_double_1d_fdct(t[u], F[u]);

		mirror(F);

		val = lroundn(F[0][0] * aan_fwd_lut[0][0]);

		mblock[1][i][0][0] = SATURATE((val + 4 * sign(val)) / 8, -255, +255);

		for (j = 1; j < 64; j++) {
			val = lroundn(F[0][j] * aan_fwd_lut[0][j]);
			div = default_intra_quant_matrix[0][j] * quant_scale;

			mblock[1][i][0][j] = SATURATE((8 * val + sign(val) * (div >> 1)) / div, -255, +255);
		}

		mirror(mblock[1][i]);
	}
}

unsigned int
fdct_inter(short iblock[6][8][8])
{
	int i, j, val, cbp = 0;

	emms();

	for (i = 0; i < 6; i++)	{
		FLOAT F[8][8], t[8][8];

		for (j = 0; j < 64; j++)
			F[0][j] = iblock[i][0][j];

		for (j = 0; j < 8; j++)
			aan_double_1d_fdct(F[j], t[j]);

		mirror(t);

		for (j = 0; j < 8; j++)
			aan_double_1d_fdct(t[j], F[j]);

		mirror(F);

		for (j = 0; j < 64; j++) {
			val = lroundn(F[0][j] * aan_fwd_lut[0][j]);

			if ((mblock[0][i][0][j] = SATURATE(val / (2 * inter_quant_scale), -255, +255)) != 0)
				cbp |= 0x20 >> i;
		}

		mirror(mblock[0][i]);
	}

	return cbp;
}

void
mpeg1_idct_intra(int quant_scale)
{
	int i, j, k, val;
	unsigned char *p, *new = newref;

	emms();

	for (i = 0; i < 6; i++)	{
		FLOAT F[8][8], t[8][8];

		new += mb_address.block[0].offset;

		mirror(mblock[1][i]);

		F[0][0] = mblock[1][i][0][0] * 8 * aan_inv_lut[0][0];

		for (j = 1; j < 64; j++) {
			val = (int)(mblock[1][i][0][j] * 
				default_intra_quant_matrix[0][j] * quant_scale) / 8;

			/* mismatch control */

			if (!(val & 1))
				val -= sign(val);

			F[0][j] = aan_inv_lut[0][j] * saturate(val, -2048, 2047);
		}

		for (j = 0; j < 8; j++)
			aan_double_1d_idct(F[j], t[j]);

		mirror(t);

		for (j = 0; j < 8; j++)
			aan_double_1d_idct(t[j], F[j]);

		mirror(F);

		for (j = 0, p = new; j < 8; j++) {
			for (k = 0; k < 8; k++)
				p[k] = saturate(lroundn(F[j][k]) + 128, 0, 255);
			p += mb_address.block[i].pitch;
		}
	}
}

void
mpeg1_idct_inter(unsigned int cbp)
{
	FLOAT F[8][8], t[8][8];
	unsigned char *new = newref;
	int i, j, k, val;

	emms();

	for (i = 0; i < 6; i++) {
		new += mb_address.block[0].offset;

		if (cbp & (0x20 >> i)) {
			unsigned char *p = new;

			mirror(mblock[0][i]);

			for (j = 0; j < 64; j++) {
				val = (2 * mblock[0][i][0][j] + sign(mblock[0][i][0][j])) * inter_quant_scale;

				/* mismatch control */

				if (!(val & 1))
					val -= sign(val);

				F[0][j] = aan_inv_lut[0][j] * saturate(val, -2048, 2047);
			}

			for (j = 0; j < 8; j++)
				aan_double_1d_idct(F[j], t[j]);

			mirror(t);

			for (j = 0; j < 8; j++)
				aan_double_1d_idct(t[j], F[j]);

			mirror(F);

			for (j = 0; j < 8; j++) {
				for (k = 0; k < 8; k++)
#if 1
					p[k] = saturate(lroundn(F[j][k]) + mblock[3][i][j][k], 0, 255);
#else
					p[k] = saturate(saturate(lroundn(F[j][k]), -128, 127) + mblock[3][i][j][k], 0, 255);
#endif
				p += mb_address.block[i].pitch;
			}
		} else {
			unsigned char *p = new;

			for (j = 0; j < 8; j++) {
				for (k = 0; k < 8; k++)
					p[k] = mblock[3][i][j][k];
				p += mb_address.block[i].pitch;
			}
		}
	}
}

void
mpeg2_idct_intra(int quant_scale)
{
	int i, j, k, val, sum;
	unsigned char *p, *new = newref;

	emms();

	for (i = 0; i < 6; i++)	{
		FLOAT F[8][8], t[8][8];

		new += mb_address.block[0].offset;

		mirror(mblock[1][i]);

		F[0][0] = (sum = mblock[1][i][0][0] * 8) * aan_inv_lut[0][0];

		for (j = 1; j < 64; j++) {
			val = (int)(mblock[1][i][0][j] * 
				default_intra_quant_matrix[0][j] * quant_scale) / 8;

			sum += val = saturate(val, -2048, 2047);

			if (j == 63 && !(sum & 1))
				val ^= 1;

			F[0][j] = aan_inv_lut[0][j] * val;
		}

		for (j = 0; j < 8; j++)
			aan_double_1d_idct(F[j], t[j]);

		mirror(t);

		for (j = 0; j < 8; j++)
			aan_double_1d_idct(t[j], F[j]);

		mirror(F);

		for (j = 0, p = new; j < 8; j++) {
			for (k = 0; k < 8; k++)
				p[k] = saturate(lroundn(F[j][k]) + 128, 0, 255);
			p += mb_address.block[i].pitch;
		}
	}
}

void
mpeg2_idct_inter(unsigned int cbp)
{
	FLOAT F[8][8], t[8][8];
	unsigned char *new = newref;
	int i, j, k, val, sum;

	emms();

	for (i = 0; i < 6; i++) {
		new += mb_address.block[0].offset;

		if (cbp & (0x20 >> i)) {
			unsigned char *p = new;

			mirror(mblock[0][i]);

			for (j = 0, sum = 0; j < 64; j++) {
				val = (2 * mblock[0][i][0][j] + sign(mblock[0][i][0][j])) * inter_quant_scale;

				sum += val = saturate(val, -2048, 2047);

				/* mismatch control */

				if (j == 63 && !(sum & 1))
					val ^= 1;

				F[0][j] = aan_inv_lut[0][j] * val;
			}

			for (j = 0; j < 8; j++)
				aan_double_1d_idct(F[j], t[j]);

			mirror(t);

			for (j = 0; j < 8; j++)
				aan_double_1d_idct(t[j], F[j]);

			mirror(F);

			for (j = 0; j < 8; j++) {
				for (k = 0; k < 8; k++)
#if 1
					p[k] = saturate(lroundn(F[j][k]) + mblock[3][i][j][k], 0, 255);
#else
					p[k] = saturate(saturate(lroundn(F[j][k]), -128, 127) + mblock[3][i][j][k], 0, 255);
#endif
				p += mb_address.block[i].pitch;
			}
		} else {
			unsigned char *p = new;

			for (j = 0; j < 8; j++) {
				for (k = 0; k < 8; k++)
					p[k] = mblock[3][i][j][k];
				p += mb_address.block[i].pitch;
			}
		}
	}
}

void
new_inter_quant(int quant_scale)
{
	inter_quant_scale = quant_scale;
}
