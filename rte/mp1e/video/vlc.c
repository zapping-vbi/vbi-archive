/*
 *  MPEG-1 Real Time Encoder
 *
 *  Copyright (C) 1999-2002 Michael H. Schimek
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

/* $Id: vlc.c,v 1.6 2002-08-22 22:04:22 mschimek Exp $ */

#include <assert.h>
#include <limits.h>
#include "../common/bstream.h"
#include "../common/log.h"
#include "mpeg.h"
#include "vlc.h"

#define align(n) __attribute__ ((SECTION("vlc_tables") aligned (n)))

// XXX
int			dc_dct_pred[2][3];

#include "vlc_tables.h"

// XXX
extern short		mblock[7][6][8][8];
extern struct bs_rec	video_out;

/* Reference */

#if 0

static inline int
escape (int run, int slevel, int ulevel, const int mpeg2)
{
	int len;

	if (mpeg2) {
		if (ulevel > 255 /* XXX */) {
			return 1;
		} else {
			/* %000001 escape, 6 bit run, 12 bit slevel */
			slevel = (1 << 18) | (run << 12) | (slevel & 0xFFF);
			len = 24;
		}
	} else {
		if (ulevel > 255) {
			return 1;
		} else if (ulevel > 127) {
			/* %000001 escape, 6 bit run, %s0000000, slevel (sic) & 0xFF */
			slevel = (1 << 22) | (run << 16) | (slevel & 0x80FF);
			len = 28;
		} else {
			/* %000001 escape, 6 bit run, slevel & 0xFF */
			slevel = (1 << 14) | (run << 8) | (slevel & 0xFF);
			len = 20;
		}
	}

	bputl (&video_out, slevel, len);

	return 0;
}

static inline int
intra_block (short block[8][8], int *dc_pred, const VLC8 *dc_vlc,
	     const int mpeg2, const int B15)
{
	const VLC2 *ac_vlc = /* B15 ? mp1e_ac_vlc_one : */ mp1e_ac_vlc_zero;

	/* DC coefficient (8 bit) */

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
			: "=&r" (size) : "r" (abs (val)));

		if (val < 0) {
			val--;
			val ^= (-1 << size);
		}

		bputl (&video_out, dc_vlc[size].code | val, dc_vlc[size].length);

		*dc_pred = block[0][0];
	}

	/* AC coefficients */

	{
		const VLC2 *q = ac_vlc;
		int i;

		for (i = 1; i < 64; i++) {
			int ulevel, slevel = block[0][mp1e_iscan[0][ (i - 1) & 63]];

			if (slevel) {
				ulevel = abs (slevel);

				if (ulevel < (int) q->length) {
					q += q->code + ulevel;
					bputl (&video_out, q->code | ((slevel >> 31) & 1), q->length);
				} else {
					int run = q - ac_vlc;

					if (escape (run, slevel, ulevel, mpeg2))
						return 1;
				}

				q = ac_vlc; /* run = 0 */
			} else {
				q++; /* run++ */
			}
		}
	}

	return 0;
}

static inline int
intra_mblock (const int mpeg2, const int B15)
{
	int v;

	dc_dct_pred[1][0] = dc_dct_pred[0][0];
	dc_dct_pred[1][1] = dc_dct_pred[0][1];
	dc_dct_pred[1][2] = dc_dct_pred[0][2];

	v  = intra_block (mblock[1][0], &dc_dct_pred[0][0], mp1e_dc_vlc_intra[0], mpeg2, B15);
	v |= intra_block (mblock[1][2], &dc_dct_pred[0][0], mp1e_dc_vlc_intra[B15 ? 3 : 1], mpeg2, B15);
	v |= intra_block (mblock[1][1], &dc_dct_pred[0][0], mp1e_dc_vlc_intra[B15 ? 3 : 1], mpeg2, B15);
	v |= intra_block (mblock[1][3], &dc_dct_pred[0][0], mp1e_dc_vlc_intra[B15 ? 3 : 1], mpeg2, B15);
	v |= intra_block (mblock[1][4], &dc_dct_pred[0][1], mp1e_dc_vlc_intra[B15 ? 4 : 2], mpeg2, B15);
	v |= intra_block (mblock[1][5], &dc_dct_pred[0][2], mp1e_dc_vlc_intra[B15 ? 4 : 2], mpeg2, B15);

	if (B15)
		bputl (&video_out, 0x6, 4); /* EOB '0110' (ISO 13818-2 table B-15) */
	else
		bputl (&video_out, 0x2, 2); /* EOB '10' (ISO 13818-2 table B-14) */

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
mp1e_mpeg1_encode_intra (void)
{
	return intra_mblock (0, 0);
}

int
mp1e_mpeg2_encode_intra_14 (void)
{
	return intra_mblock (1, 0);
}

int
mp1e_mpeg2_encode_intra_15 (void)
{
	return intra_mblock (1, 1);
}

static inline int
inter_block (short block[8][8], const int mpeg2)
{
	const VLC2 *p = mp1e_ac_vlc_zero; /* ISO 13818-2 table B-14 */
	int i = 1, ulevel, slevel;

	/* AC coefficient 0 */

	ulevel = abs (slevel = block[0][0]);

	if (ulevel == 1) {
		bputl (&video_out, 0x2 | ((slevel >> 31) & 1), 2);
	} else {
		i = 0;
	}

	/* AC coefficients */

	while (i < 64) {
		if ((slevel = block[0][mp1e_iscan[0][(i - 1) & 63]])) {
			ulevel = abs (slevel);

	    		if (ulevel < (int) p->length) {
				p += p->code + ulevel;
				bputl (&video_out, p->code | ((slevel >> 31) & 1), p->length);
			} else {
				int run = p - mp1e_ac_vlc_zero;

				if (escape (run, slevel, ulevel, mpeg2))
					return 1;
			}

			p = mp1e_ac_vlc_zero; /* run = 0 */
		} else {
		        p++; /* run++ */
		}

		i++;
	}

	bputl (&video_out, 0x2, 2); /* EOB '10' (ISO 13818-2 table B-14) */

	return 0;
}

static inline int
inter_mblock (short iblock[6][8][8], unsigned int cbp, const int mpeg2)
{
	int v = 0;

	/**** watch cbp_order ****/

	if (cbp & (1 << 5)) v  = inter_block (iblock[0], mpeg2);
	if (cbp & (1 << 3)) v |= inter_block (iblock[2], mpeg2);
	if (cbp & (1 << 4)) v |= inter_block (iblock[1], mpeg2);
	if (cbp & (1 << 2)) v |= inter_block (iblock[3], mpeg2);
	if (cbp & (1 << 1)) v |= inter_block (iblock[4], mpeg2);
	if (cbp & (1 << 0)) v |= inter_block (iblock[5], mpeg2);

	return v;
}

int
mp1e_mpeg1_encode_inter (short iblock[6][8][8], unsigned int cbp)
{
	return inter_mblock (iblock, cbp, 0);
}

int
mp1e_mpeg2_encode_inter (short iblock[6][8][8], unsigned int cbp)
{
	return inter_mblock (iblock, cbp, 1);
}

#endif
