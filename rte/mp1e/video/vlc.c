/*
 *  MPEG-1 Real Time Encoder
 *
 *  Copyright (C) 1999-2000 Michael H. Schimek
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 2.
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

/* $Id: vlc.c,v 1.1.1.1 2001-08-07 22:09:55 garetxe Exp $ */

#include <assert.h>
#include <limits.h>
#include "../common/bstream.h"
#include "../common/log.h"
#include "mpeg.h"
#include "vlc.h"

#define vlc_align(n) __attribute__ ((SECTION("VLC_TABLES") aligned (n)))

int			dc_dct_pred[2][3];

/*
 *  Tables
 */

VLC2			coded_block_pattern[64]			vlc_align(CACHE_LINE);
VLC2			macroblock_address_increment[33]	vlc_align(CACHE_LINE);
VLCM			motion_vector_component[480]		vlc_align(CACHE_LINE);

/* ISO/IEC 13818-2 Table B-2  Variable length codes for macroblock_type in I-pictures */

/*
 *  '1' Intra
 *  '01 xxxxx' Intra, Quant
 */

/* ISO/IEC 13818-2 Table B-3  Variable length codes for macroblock_type in P-pictures */

/*
 *  '1' MC, Coded
 *  '01' No MC, Coded
 *  '001' MC, Not Coded
 *  '0001 1' Intra
 *  '0001 0' MC, Coded, Quant
 *  '0000 1' No MC, Coded, Quant
 *  '0000 01' Intra, Quant
 */

/* ISO/IEC 13818-2 Table B-4  Variable length codes for macroblock_type in B-pictures */

VLC4
macroblock_type_b_nomc_quant[4] vlc_align(16) =
{
	{ 0x0020, 11, 0 },	// '0000 01 xxxxx' (Intra, Quant)
	{ 0x0183, 13, 2 },	// '0000 11 xxxxx 11' (Fwd, Coded, Quant, MV (0, 0))
	{ 0x0103, 13, 2 },	// '0000 10 xxxxx 11' (Bwd, Coded, Quant, MV (0, 0))
	{ 0x040F, 14, 4 }	// '0001 0 xxxxx 11 11' (Interp, Coded, Quant, FMV (0, 0), BMV (0, 0))
};

VLC2
macroblock_type_b_nomc[4] vlc_align(8) =
{
	{ 0x03, 5 },		// '0001 1' (Intra)
	{ 0x0F, 6 },		// '0011  11' (Fwd, Coded, MV (0, 0))
	{ 0x0F, 5 },		// '011  11' (Bwd, Coded, MV (0, 0))
	{ 0x3F, 6 },		// '11  11 11' (Interp, Coded, FMV (0, 0), BMV (0, 0))
};

VLC2
macroblock_type_b_nomc_notc[4] vlc_align(8) =
{
	{ 0, 0 },		// Intra always coded
	{ 0x0B, 6 },		// '0010  11' (Fwd, Not Coded, MV (0, 0))
	{ 0x0B, 5 },		// '010  11' (Bwd, Not Coded, MV (0, 0))
	{ 0x2F, 6 },		// '10  11 11' (Interp, Not Coded, FMV (0, 0), BMV (0, 0))
};

VLC2
macroblock_type_b_quant[4] vlc_align(8) =
{
	{ 0x020, 11 },		// '0000 01 xxxxx' (Intra, Quant)
	{ 0x060, 11 },		// '0000 11 xxxxx' (Fwd, Coded, Quant)
	{ 0x040, 11 },		// '0000 10 xxxxx' (Bwd, Coded, Quant)
	{ 0x040, 10 }		// '0001 0 xxxxx' (Interp, Coded, Quant)
};

#if 0

VLC2
macroblock_type_b[4] vlc_align(8) =
{
	{ 0x03, 5 },		// '0001 1' (Intra)
	{ 0x03, 4 },		// '0011' (Fwd, Coded)
	{ 0x03, 3 },		// '011' (Bwd, Coded)
	{ 0x03, 2 },		// '11' (Interp, Coded)
};

VLC2
macroblock_type_b_notc[4] vlc_align(8) =
{
	{ 0, 0 },		// Intra always coded
	{ 0x02, 4 },		// '0010' (Fwd, Not Coded)
	{ 0x02, 3 },		// '010' (Bwd, Not Coded)
	{ 0x02, 2 },		// '10' (Interp, Not Coded)
};

#endif

unsigned char		iscan[8][8]				vlc_align(CACHE_LINE);

VLC8			dc_vlc_intra[5][12]			vlc_align(CACHE_LINE);
VLC2			ac_vlc_zero[176]			vlc_align(CACHE_LINE);
VLC2			ac_vlc_one[176]				vlc_align(CACHE_LINE);


extern short		mblock[7][6][8][8];
extern struct bs_rec	video_out;
extern const char 	cbp_order[6];

void
vlc_init(void)
{
	int i, j;
	unsigned int code;
	int dct_dc_size;
	int run, level, length;
	int f_code;

	/* Variable length codes for macroblock address increment */

	for (i = 0; i < 33; i++) {
		macroblock_address_increment[i].length = vlc(macroblock_address_increment_vlc[i], &code);
		macroblock_address_increment[i].code = code;
		assert(code <= UCHAR_MAX);
	}

	/* Variable length codes for coded block pattern */

	for (i = 0; i < 64; i++) {
		int j, k;

		for (j = k = 0; k < 6; k++)
			if (i & (1 << k))
				j |= 0x20 >> cbp_order[k]; // (5 - k)

		coded_block_pattern[i].length = vlc(coded_block_pattern_vlc[j], &code);
		coded_block_pattern[i].code = code;
		assert(code <= UCHAR_MAX);
	}

	/* Variable length codes for motion vector component */

	for (f_code = F_CODE_MIN; f_code <= F_CODE_MAX; f_code++) {
		int r_size = f_code - 1;
		int f1 = (1 << r_size) - 1;

		for (i = 0; i < 16 << f_code; i++) {
			int motion_code, motion_residual;
			int delta = (i < (16 << r_size)) ? i : i - (16 << f_code);

			motion_code = (abs(delta) + f1) >> r_size;
			motion_residual = (abs(delta) + f1) & f1;

			length = vlc(motion_code_vlc[motion_code], &code);

			if (motion_code != 0) {
				code = code * 2 + (delta < 0); /* sign */
				length++;
			}

			if (f_code > 1 && motion_code != 0) {
				code = (code << r_size) + motion_residual;
				length += r_size;
			}

			assert(code < (1 << 12) && length < 16);

			motion_vector_component[f1 * 32 + i].code = code;
			motion_vector_component[f1 * 32 + i].length = length;
#if 0
			fprintf(stderr, "MV %02x %-2d ", i, delta);

			for (j = length - 1; j >= 0; j--)
				fprintf(stderr, "%d", (code & (1 << j)) > 0);

			fprintf(stderr, "\n");
#endif
		}
	}

	/* Variable length codes for intra DC coefficient */

	for (dct_dc_size = 0; dct_dc_size < 12; dct_dc_size++) {
		// Intra DC luma VLC
		dc_vlc_intra[0][dct_dc_size].length = vlc(dct_dc_size_luma_vlc[dct_dc_size], &code) + dct_dc_size;
		dc_vlc_intra[0][dct_dc_size].code = code << dct_dc_size;

		// Intra DC luma VLC with EOB ('10' table B-14) of previous block
		dc_vlc_intra[1][dct_dc_size].length = vlc(dct_dc_size_luma_vlc[dct_dc_size], &code) + dct_dc_size + 2;
		dc_vlc_intra[1][dct_dc_size].code = ((0x2 << vlc(dct_dc_size_luma_vlc[dct_dc_size], &code)) | code) << dct_dc_size;

		// Intra DC chroma VLC with EOB of previous block
		dc_vlc_intra[2][dct_dc_size].length = vlc(dct_dc_size_chroma_vlc[dct_dc_size], &code) + dct_dc_size + 2;
		dc_vlc_intra[2][dct_dc_size].code = ((0x2 << vlc(dct_dc_size_chroma_vlc[dct_dc_size], &code)) | code) << dct_dc_size;

		// Intra DC luma VLC with EOB ('0110' table B-15) of previous block
		dc_vlc_intra[3][dct_dc_size].length = vlc(dct_dc_size_luma_vlc[dct_dc_size], &code) + dct_dc_size + 4;
		dc_vlc_intra[3][dct_dc_size].code = ((0x6 << vlc(dct_dc_size_luma_vlc[dct_dc_size], &code)) | code) << dct_dc_size;

		// Intra DC chroma VLC with EOB of previous block
		dc_vlc_intra[4][dct_dc_size].length = vlc(dct_dc_size_chroma_vlc[dct_dc_size], &code) + dct_dc_size + 4;
		dc_vlc_intra[4][dct_dc_size].code = ((0x6 << vlc(dct_dc_size_chroma_vlc[dct_dc_size], &code)) | code) << dct_dc_size;
	}

	/* Variable length codes for AC coefficients (table B-14) */

	for (i = run = 0; run < 64; run++) {
		assert(i <= sizeof(ac_vlc_zero) / sizeof(ac_vlc_zero[0]));

		ac_vlc_zero[j = i++].code = run;

		for (level = 1; (length = dct_coeff_vlc(0, run, level, &code)) > 0; level++, i++) {
			assert(i < sizeof(ac_vlc_zero) / sizeof(ac_vlc_zero[0]));
			assert((code << 1) <= UCHAR_MAX);

			ac_vlc_zero[i].length = length + 1;
			ac_vlc_zero[i].code = code << 1; // sign 0
		}

		ac_vlc_zero[j].length = i - j;
	}

	/* Variable length codes for AC coefficients (table B-15) */

	for (i = run = 0; run < 64; run++) {
		assert(i <= sizeof(ac_vlc_one) / sizeof(ac_vlc_one[0]));

		ac_vlc_one[j = i++].code = run;

		for (level = 1; (length = dct_coeff_vlc(1, run, level, &code)) > 0; level++, i++) {
			assert(i < sizeof(ac_vlc_one) / sizeof(ac_vlc_one[0]));
			assert((code << 0) <= UCHAR_MAX);

			ac_vlc_one[i].length = length + 1;
			ac_vlc_one[i].code = code << 0; // no sign (would need 9 bits)
		}

		ac_vlc_zero[j].length = i - j;
	}

	/*
	 *  Forward zig-zag scanning pattern
	 */
	for (i = 0; i < 64; i++) {
//		iscan[0][63 - scan[0][0][i]] = (i & 7) * 8 + (i >> 3);
		iscan[0][(scan[0][0][i] - 1) & 63] = (i & 7) * 8 + (i >> 3);
	}
}

/* Reference */

#if 1

int
mpeg1_encode_intra(void)
{
	int v;

	int
	encode_block(short block[8][8], int *dc_pred, VLC8 *dc_vlc)
	{
		/* DC coefficient */

		{
			register int val = block[0][0] - *dc_pred, size;
			
			/*
			 *  Find first set bit, starting at msb with 0 -> 0.
			 */
			asm volatile (
				" bsrl		%1,%0\n"
				" jnz		1f\n"
				" movl		$-1,%0\n"
		    	        "1:\n"
				" incl		%0\n"
			: "=&r" (size) : "r" (abs(val)));

			if (val < 0) {
				val--;
				val ^= (-1 << size);
			}

			bputl(&video_out, dc_vlc[size].code | val, dc_vlc[size].length);

			*dc_pred = block[0][0];
		}

		/* AC coefficients */

		{
			VLC2 *p = ac_vlc_zero;
			int i;

			for (i = 1; i < 64; i++) {
	    			int ulevel, slevel = block[0][iscan[0][(i - 1) & 63]];

				if (slevel) {
					ulevel = abs(slevel);

		    			if (ulevel < (int) p->length) {
						p += ulevel;
						bputl(&video_out, p->code | ((slevel >> 31) & 1), p->length);
					} else {
		    				int len;

		    				if (slevel > 127) {
							if (slevel > 255)
								return 1;
							/* %000001 escape, 6 bit run, %00000000, slevel & 0xFF */
							slevel = 0x0400000 | (p->code << 16) | (slevel & 0xFF);
							len = 28;
						} else if (slevel < -127) {
							if (slevel < -255)
								return 1;
							/* %000001 escape, 6 bit run, %10000000, slevel (sic) & 0xFF */
							slevel = 0x0408000 | (p->code << 16) | (slevel & 0xFF);
							len = 28;
						} else {
							/* %000001 escape, 6 bit run, slevel & 0xFF */
							slevel = (1 << 14) | (p->code << 8) | (slevel & 0xFF);
							len = 20;
						}

						bputl(&video_out, slevel, len);
					}

					p = ac_vlc_zero; // run = 0
				} else
					p += p->length; // run++
			}
		}

		return 0;
	}

	dc_dct_pred[1][0] = dc_dct_pred[0][0];
	dc_dct_pred[1][1] = dc_dct_pred[0][1];
	dc_dct_pred[1][2] = dc_dct_pred[0][2];

	v  = encode_block(mblock[1][0], &dc_dct_pred[0][0], dc_vlc_intra[0]);
	v |= encode_block(mblock[1][2], &dc_dct_pred[0][0], dc_vlc_intra[1]);
	v |= encode_block(mblock[1][1], &dc_dct_pred[0][0], dc_vlc_intra[1]);
	v |= encode_block(mblock[1][3], &dc_dct_pred[0][0], dc_vlc_intra[1]);
	v |= encode_block(mblock[1][4], &dc_dct_pred[0][1], dc_vlc_intra[2]);
	v |= encode_block(mblock[1][5], &dc_dct_pred[0][2], dc_vlc_intra[2]);

	bputl(&video_out, 0x2, 2); // EOB '10' (ISO 13818-2 table B-14)

	/*
	 *  Saturation is rarely needed, so the forward quantisation code
	 *  skips the step. This routine detects excursions in uncritical
	 *  path and reports but saturates because saturation often causes
	 *  a visibly annoying reconstruction error.
	 */
	if (v) {
		dc_dct_pred[0][0] = dc_dct_pred[1][0];
		dc_dct_pred[0][1] = dc_dct_pred[1][1];
		dc_dct_pred[0][2] = dc_dct_pred[1][2];
	}

	return v;
}

int
mpeg1_encode_inter(short iblock[6][8][8], unsigned int cbp)
{
	int v = 0;

	int
	encode_block(short block[8][8])
	{
		VLC2 *p = ac_vlc_zero; // ISO 13818-2 table B-14
    		int i = 1, len, ulevel, slevel;

		/* DC coefficient */

		ulevel = abs(slevel = block[0][0]);

		if (ulevel == 1) {
			bputl(&video_out, 0x2 | ((slevel >> 31) & 1), 2);
		} else
			i = 0;

		/* AC coefficients */

		while (i < 64) {
	    		if ((slevel = block[0][iscan[0][(i - 1) & 63]])) {
				ulevel = abs(slevel);

		    		if (ulevel < (int) p->length) {
					p += ulevel;
					bputl(&video_out, p->code | ((slevel >> 31) & 1), p->length);
				} else {
		    			if (slevel > 127) {
						if (slevel > 255)
							return 1;
						/* %000001 escape, 6 bit run, %00000000, slevel & 0xFF */
						slevel = 0x0400000 | (p->code << 16) | (slevel & 0xFF);
						len = 28;
					} else if (slevel < -127) {
						if (slevel < -255)
							return 1;
						/* %000001 escape, 6 bit run, %10000000, slevel (sic) & 0xFF */
						slevel = 0x0408000 | (p->code << 16) | (slevel & 0xFF);
						len = 28;
					} else {
						/* %000001 escape, 6 bit run, slevel & 0xFF */
						slevel = (1 << 14) | (p->code << 8) | (slevel & 0xFF);
						len = 20;
					}

					bputl(&video_out, slevel, len);
				}

				p = ac_vlc_zero; // run = 0
			} else
				p += p->length; // run++
			i++;
		}

		bputl(&video_out, 0x2, 2);
		return 0;
	}

	// watch cbp_order
	if (cbp & (1 << 5)) v  = encode_block(iblock[0]);
	if (cbp & (1 << 3)) v |= encode_block(iblock[2]);
	if (cbp & (1 << 4)) v |= encode_block(iblock[1]);
	if (cbp & (1 << 2)) v |= encode_block(iblock[3]);
	if (cbp & (1 << 1)) v |= encode_block(iblock[4]);
	if (cbp & (1 << 0)) v |= encode_block(iblock[5]);

	return v;
}

#endif
