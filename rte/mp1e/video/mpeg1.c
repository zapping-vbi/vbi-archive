/*
 *  MPEG-1 Real Time Encoder
 *
 *  Copyright (C) 1999, 2000, 2001, 2002 Michael H. Schimek
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

/* $Id: mpeg1.c,v 1.32 2002-06-12 03:58:40 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "../site_def.h"

#include <assert.h>
#include <limits.h>
#include <string.h>
#include <ctype.h>

#include "../common/profile.h"
#include "../common/math.h"
#include "../common/types.h"
#include "../common/log.h"
#include "../common/mmx.h"
#include "../common/bstream.h"
#include "../common/fifo.h"
#include "../common/alloc.h"
#include "../systems/mpeg.h"
#include "../systems/systems.h"
#include "vlc.h"
#include "dct.h"
#include "motion.h"
#include "video.h"

/* XXX vlc_mmx.s */
struct bs_rec		video_out;

/* XXX status report */
long long		video_frame_count;
long long		video_frames_dropped;
double 			video_eff_bit_rate;

#ifdef VIDEO_FIFO_TEST
double			in_fifo_load;
double			out_fifo_load;
#endif

/* XXX options */
int			motion;			// current search range

/* XXX motion_mmx.s */
short *			mm_mbrow;		// mm_mbrow[8][mb_width * 16];
int			mm_buf_offs;

/* XXX gui */
unsigned int p_inter_bias = 65536 * 48,
             b_inter_bias = 65536 * 96;
int x_bias = 65536 * 31,
	quant_max = 31;

#define QS 1

#define TEST34 1
#define T34VMC 1500000

#define NO_VBV 0xFFFF

static mpeg1_context *static_context;

mpeg1_context *
mp1e_static_context(void)
{
	return static_context;
}

#define fdct_intra mp1e_mmx_fdct_intra
#define fdct_inter mp1e_mmx_fdct_inter
#define mpeg1_idct_intra mp1e_mmx_mpeg1_idct_intra2
#define mpeg1_idct_inter mp1e_mmx_mpeg1_idct_inter
#define mpeg1_encode_intra mp1e_p6_mpeg1_encode_intra
#define mpeg1_encode_inter mp1e_p6_mpeg1_encode_inter

static const unsigned char
quant_res_intra[32] __attribute__ ((SECTION("video_tables") aligned (CACHE_LINE))) =
{
	1, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
	16, 16, 18, 18, 20, 20, 22, 22,	24, 24, 26, 26, 28, 28, 30, 30
};

extern int		preview;
extern void		packed_preview(unsigned char *buffer,
				       int mb_cols, int mb_rows);

#include "dct_ieee.h"

#ifndef T3RT
#define T3RT 1
#endif
#ifndef T3RI
#define T3RI 1
#endif

#define TEST12 0
#define PBF 2
#define LOWVAR 0
#define LOWVARI 0
#define LOWVARP 0

#define VARQ 65536.0

/*
 *  Picture layer
 */

#define macroblock_address(skipped)					\
do {									\
	/* this isn't really justified, better limit mb_skipped */	\
	for (i = (skipped); __builtin_expect(i >= 33, 0); i -= 33)	\
		bputl(&video_out, 0x008, 11); /* mb addr escape */	\
									\
	code = mp1e_macroblock_address_increment[i].code;		\
	length = mp1e_macroblock_address_increment[i].length; /* 1...11 */ \
} while (0)

#define motion_vector(m, dmv)						\
do {									\
	int l1 = (m)->vlc[dmv[1]].length;				\
	bputl(&video_out, ((m)->vlc[dmv[0]].code << l1)			\
			  + (m)->vlc[dmv[1]].code,			\
	      (m)->vlc[dmv[0]].length + l1);				\
} while (0)

#define MB_HIST(n) (mpeg1->mb_hist[(n) + 1])

/* preliminary */
static inline void
mblock_hp(int n)
{
	int i;

	asm volatile ("\tpxor %mm0,%mm0;\n");

	for (i = 0; i < 6; i++)
		asm volatile (
			"\tmovq %%mm0,0*16+8(%0);\n"
			"\tmovq %%mm0,1*16+4(%0); movq %%mm0,1*16+8(%0);\n"
			"\tmovq %%mm0,2*16+2(%0); movq %%mm0,2*16+8(%0);\n"
			"\tmovq %%mm0,3*16+2(%0); movq %%mm0,3*16+8(%0);\n"
			"\tmovq %%mm0,4*16+0(%0); movq %%mm0,4*16+8(%0);\n"
			"\tmovq %%mm0,5*16+0(%0); movq %%mm0,5*16+8(%0);\n"
			"\tmovq %%mm0,6*16+0(%0); movq %%mm0,6*16+8(%0);\n"
			"\tmovq %%mm0,7*16+0(%0); movq %%mm0,7*16+8(%0);\n"
		:: "r" (&mblock[n][i]) : "memory");
}

static inline void
tmp_slice_i(mpeg1_context *mpeg1, bool motion)
{
	for (mb_col = 0; mb_col < mb_width; mb_col++) {
		unsigned int var;
		int quant;
		struct bs_rec mark;

		pr_start(41, "Filter");
		var = mpeg1->filter_param[0].func(&mpeg1->filter_param[0], mb_col, mb_row);
		pr_end(41);

		if (motion && __builtin_expect(mpeg1->referenced, 1)) {
			pr_start(56, "MB sum");
			mmx_mbsum(newref + mm_buf_offs); // mblock[0]
			pr_end(56);
		}

		emms();

		if (0 && var < LOWVARI) {
			quant = 4;
			fprintf(stderr, "+");
		} else {
			int pq0 = MB_HIST(mb_col - 1).quant; /* left */
			int pq1 = MB_HIST(mb_col + 0).quant; /* up */

			quant = rc_quant(&mpeg1->rc, MB_INTRA,
					 var / VARQ + 1, 0.0,
					 bwritten(&video_out), 0, quant_max);

if (0)
			if (mb_col > 0 && !(TEST12 && motion)) {
		       		if (((quant - pq0) < -2 || (quant - pq1) < -2)) {
					quant = (quant + quant + pq0 + pq1 + 2) >> 2;
				} else if (quant >= 4 && quant > pq0 && (quant - pq0) <= 2) {
					quant = pq0;
				}
			}

			quant = quant_res_intra[quant];
			/*
			 *  quant_res_intra halves the quantization factor resolution above 16 to
			 *  reduce lookup table size (MMX DCT). Quality is already poor at 16,
			 *  so it won't hurt very much.
			 */
		}

		/* Encode macroblock */

		brewind(&mark, &video_out);

		for (;;) {
			pr_start(22, "FDCT intra");
			fdct_intra(quant); // mblock[0] -> mblock[1]
			pr_end(22);

			if (0 && var < LOWVARI)
				mblock_hp(0);

			bepilog(&video_out);

			if (__builtin_expect((mb_col + mb_row) == 0, 0)) {
				bstartq(SLICE_START_CODE);
				bcatq((quant << 3) + 0x3, 8);
				bputq(&video_out, 40);
				/*
				 *  slice header: quantiser_scale_code 'xxxxx', extra_bit_slice '0';
				 *  macroblock_address_increment '1', macroblock_type '1' (I Intra)
				 */
			} else if (MB_HIST(mb_col - 1 /* last */).quant != quant) {
				bputl(&video_out, 0xA0 + quant, 8);
				/*
				 *  macroblock_address_increment '1', macroblock_type '01' (I Intra, Quant),
				 *  quantiser_scale_code 'xxxxx'
				 */
			} else
				bputl(&video_out, 0x3, 2);
				/* macroblock_address_increment '1', macroblock_type '1' (I Intra) */

			pr_start(44, "Encode intra");

			if (__builtin_expect(!mpeg1_encode_intra(), 1)) { // mblock[1]
				pr_end(44);
				break;
			}

			pr_end(44);

			quant++;
			brewind(&video_out, &mark);

			pr_event(42, "I/intra overflow");
		}

		bprolog(&video_out);

		mpeg1->quant_sum += quant;
		MB_HIST(mb_col).quant = quant;

		if (__builtin_expect(mpeg1->referenced, 1)) {
			pr_start(23, "IDCT intra");
			mpeg1_idct_intra(quant);	// mblock[1] -> newref
			pr_end(23);

			if (motion)
				zero_forward_motion();

			mba_col_incr();
		}
#if TEST_PREVIEW
		if (preview > 1) {
			emms();
			packed_preview(newref, mb_width, mb_height);
		}
#endif
	}

	MB_HIST(-1) = MB_HIST(mb_col - 1);

	mba_row_incr();
}

static inline int
tmp_picture_i(mpeg1_context *mpeg1, unsigned char *org, bool motion)
{
	int S;

	printv(3, "Encoding I picture #%lld GOP #%d, ref=%c\n",
		video_frame_count, mpeg1->gop_frame_count, "FT"[mpeg1->referenced]);

	pr_start(21, "Picture I");

	rc_picture_start(&mpeg1->rc, I_TYPE, mb_num);

	mpeg1->quant_sum = 0;

	swap(mpeg1->oldref, newref);

	reset_mba();
	reset_dct_pred();

	{
		struct mblock_hist m = { -100, 0, { 0, 0 }};
		int i;

		for (i = -1; i < mb_width; i++)
			mpeg1->mb_hist[i + 1] = m;
	}

	/* Picture header */

	bepilog(&video_out);

	bputl(&video_out, PICTURE_START_CODE, 32);
	bputl(&video_out, ((mpeg1->gop_frame_count & 1023) << 22)
	      + (I_TYPE << 19) + (NO_VBV << 3) + (0 << 2), 32);
	/*
	 *  temporal_reference [10], picture_coding_type [3], vbv_delay [16];
	 *  extra_bit_picture '0', byte align '00'
	 */

	bprolog(&video_out);

	mpeg1->filter_param[0].src = org;

	for (mb_row = 0; mb_row < mb_height; mb_row++)
		tmp_slice_i(mpeg1, motion);

	emms();

	/* Rate control */

	S = bflush(&video_out);

	rc_picture_end(&mpeg1->rc, I_TYPE, S, mpeg1->quant_sum, mb_num);

	pr_end(21);

#if TEST_PREVIEW
	if (preview)
		packed_preview(newref, mb_width, mb_height);
#endif

	return S >> 3;
}

#define pb_intra_mb(f)							\
do {									\
	brewind(&mark, &video_out);					\
									\
	for (;;) {							\
		pr_start(22, "FDCT intra");				\
		fdct_intra(quant); /* mblock[0] -> mblock[1] */		\
		pr_end(22);						\
									\
		if (0 && var < LOWVARP)					\
			mblock_hp(0);					\
									\
		bepilog(&video_out);					\
									\
		if (f) {						\
			macroblock_address(mb_skipped);			\
									\
			if (prev_quant != quant && prev_quant >= 0) {	\
				bputl(&video_out, (code << 11) + 0x020  \
					+ quant, length + 11);		\
				/* macroblock_type '0000 01' */		\
				/* (P/B Intra, Quant), */		\
				/* quantiser_scale_code 'xxxxx' */	\
			} else	{					\
				bputl(&video_out, (code << 5) + 0x3,	\
					length + 5);			\
				/* macroblock_type '0001 1' */		\
				/* (P/B Intra) */			\
			}						\
		} else {						\
			if (prev_quant != quant) {			\
				bputl(&video_out, 0x820 + quant, 11);	\
				/* macroblock_address_increment '1' */	\
				/* macroblock_type '0000 01' */		\
				/* quantiser_scale_code 'xxxxx' */	\
			} else	{					\
				bputl(&video_out, 0x23, 6);		\
				/* macroblock_address_increment '1' */	\
				/* macroblock_type '0001 1' */		\
			}						\
		}							\
									\
		pr_start(44, "Encode intra");				\
									\
		if (__builtin_expect(!mpeg1_encode_intra(), 1)) {	\
					    /* mblock[1] */		\
			pr_end(44);					\
			break;						\
		}							\
									\
		pr_end(44);						\
									\
		quant++;						\
									\
		brewind(&video_out, &mark);				\
									\
		pr_event(49, "PB/intra overflow");			\
	}								\
									\
	bprolog(&video_out);						\
} while (0)

static inline int
tmp_picture_p(mpeg1_context *mpeg1, unsigned char *org,
	      bool motion, int dist, int forward_motion)
{
	int S, quant, prev_quant, quant1 = 1;
	struct bs_rec mark;
	struct motion M[1];
	unsigned char *q1p;
	unsigned int var, vmc;
	int mb_skipped, mb_count;
	int intra_count = 0;

	if (motion)
		motion_init(mpeg1, &M[0], (dist * forward_motion) >> 8);
	else {
		M[0].f_code = 1;
		M[0].src_range = 0;
	}

	printv(3, "Encoding P picture #%lld GOP #%d, ref=%c, d=%d, f_code=%d (%d)\n",
		video_frame_count, mpeg1->gop_frame_count, "FT"[mpeg1->referenced],
		dist, M[0].f_code, M[0].src_range);

	pr_start(24, "Picture P");

	/* Initialize rate control parameters */

	swap(mpeg1->oldref, newref);

	reset_mba();

#if TEST_PREVIEW
	if (preview > 1)
		memcpy(newref, mpeg1->oldref, 64 * 6 * mb_num);
#endif

	rc_picture_start(&mpeg1->rc, P_TYPE, mb_num);

	mpeg1->quant_sum = 0;

	reset_dct_pred();
	
	prev_quant = -100;
	mb_count = 1;
	mb_skipped = 0;

	/* Picture header */

	bepilog(&video_out);

	bputl(&video_out, PICTURE_START_CODE, 32);

	bputl(&video_out, ((mpeg1->gop_frame_count & 1023) << 19)
	      + (P_TYPE << 16) + NO_VBV, 29);
	bputl(&video_out, (0 << 10) + (M[0].f_code << 7) + (0 << 6), 11);
	/*
	 *  temporal_reference [10], picture_coding_type [3], vbv_delay [16];
	 *  full_pel_forward_vector '0', forward_f_code,
	 *  extra_bit_picture '0', byte align [6]
	 */

	bputl(&video_out, SLICE_START_CODE + 0, 32);
	q1p = (unsigned char *) video_out.p + (video_out.n >> 3);
	bputl(&video_out, (1 << 1) + 0, 6);
	/* slice header: quantiser_scale_code 'xxxxx', extra_bit_slice '0' */

	bprolog(&video_out);

	mpeg1->filter_param[0].src = org;

	for (mb_row = 0; mb_row < mb_height; mb_row++) {
		if (1 && __builtin_expect(mb_row == mpeg1->mb_cx_row &&
		    intra_count >= mpeg1->mb_cx_thresh, 0)) {
			emms();
			swap(mpeg1->oldref, newref);
			pr_event(43, "P/cx trap");
			return 0;
		}

		for (mb_col = 0; mb_col < mb_width; mb_col++) {
			unsigned int cbp;
			int dmv[1][2];

			/* Read macroblock (MMX state) */

			pr_start(41, "Filter");
			var = mpeg1->filter_param[0].func(&mpeg1->filter_param[0], mb_col, mb_row);
			pr_end(41);

			if (motion) {
				if (__builtin_expect(mpeg1->referenced, 1)) {
					pr_start(56, "MB sum");
					mmx_mbsum(newref + mm_buf_offs); // mblock[0]
					pr_end(56);
				}

				pr_start(51, "Predict forward");
				vmc = predict_forward_motion(&M[0], mpeg1->oldref, dist);
				pr_end(51);
			} else {
				pr_start(51, "Predict forward");
				vmc = mpeg1->predict_forward(mpeg1->oldref + mb_address.block[0].offset);
				pr_end(51);
			}

			emms();

			/* Encode macroblock */

#if TEST34
			//fprintf(stderr, "%2d,%2d %8d %8d\n", mb_row, mb_col, var, vmc);

			while (motion && vmc < T34VMC && (mb_row | mb_col) > 0) {
				unsigned int x, s2 = 0;
				int n;

				// fprintf(stderr, "*");

				/* chroma var */

				for (x = 4 * 64; x < 6 * 64; x++) {
					n = mblock[1][0][0][x];
					s2 += n * n;
				}

				if (s2 * 256 * 2 >= T34VMC) {
					// fprintf(stderr, "\\");
					break;
				}

				quant = prev_quant;

				if (motion) {
					dmv[0][0] = (M[0].MV[0] - M[0].PMV[0]) & M[0].f_mask;
					dmv[0][1] = (M[0].MV[1] - M[0].PMV[1]) & M[0].f_mask;

					M[0].PMV[0] = M[0].MV[0];
					M[0].PMV[1] = M[0].MV[1];
				}

				brewind(&mark, &video_out);

				vmc = 0;
				cbp = 0;
				goto l1;
			}
#endif
			if (T3RI
			    && ((TEST12
				 && motion
				 && (var < (LOWVAR / 6)))
				|| vmc > p_inter_bias))
			{
				double act = var / VARQ + 1;
				unsigned int code;
				int length, i;

				/* Calculate quantization factor */

				if (0 && var < LOWVARP) {
					quant = 3;
	    			        fprintf(stderr, "/");
				} else {
					quant = rc_quant(&mpeg1->rc, MB_FORWARD,
							 act, act,
							 bwritten(&video_out), QS, quant_max);
					quant = quant_res_intra[quant];

//				if (quant >= 4 && nbabs(quant - prev_quant) <= 2)
//					quant = prev_quant;
				}

				intra_count++;

				pb_intra_mb(TRUE);

				if (prev_quant < 0)
					quant1 = quant;

				prev_quant = quant;
				mpeg1->quant_sum += quant;

				mb_skipped = 0;

				if (motion) {
					M[0].PMV[0] = 0;
					M[0].PMV[1] = 0;
				}

				if (__builtin_expect(mpeg1->referenced, 1)) {
					pr_start(23, "IDCT intra");
					mpeg1_idct_intra(quant); // mblock[0] -> new
					pr_end(23);

					if (motion)
						zero_forward_motion();
				}
			} else {
				unsigned int code;
				int len, length, i;

				/* Calculate quantization factor */

				quant = rc_quant(&mpeg1->rc, MB_FORWARD,
						 var / VARQ + 1, vmc / VARQ + 1,
						 bwritten(&video_out), 0, quant_max);
if (!T3RT) quant = 2;

//				if (quant >= 4 && nbabs(quant - prev_quant) <= 2)
//					quant = prev_quant;

				if (motion) {
					dmv[0][0] = (M[0].MV[0] - M[0].PMV[0]) & M[0].f_mask;
					dmv[0][1] = (M[0].MV[1] - M[0].PMV[1]) & M[0].f_mask;

					M[0].PMV[0] = M[0].MV[0];
					M[0].PMV[1] = M[0].MV[1];
				}

				brewind(&mark, &video_out);

				for (;;) {
					pr_start(26, "FDCT inter");
					cbp = fdct_inter(mblock[1], quant); // mblock[1] -> mblock[0]
					pr_end(26);

#if TEST34
				l1:
#endif
					i = mb_skipped;

					if (cbp == 0 && mb_count > 1 && mb_count < mb_num &&
						(!motion || (M[0].MV[0] | M[0].MV[1]) == 0)) {
						mp1e_mmx_copy_refblock();
						i++;
						break;
					} else {
						bepilog(&video_out);

						macroblock_address(i); // length = 1..11

						i = 0;

						if (cbp == 0) {
							if (motion) {
								bputl(&video_out, (code << 3) + 1, length + 3);
								motion_vector(&M[0], dmv[0]);
								/* macroblock_type '001' (P MC, Not Coded) */
							} else {
								bputl(&video_out, (code << 5) + 7, length + 5);
								/* macroblock_type '001' (P MC, Not Coded), '11' MV(0, 0) */
							}

							bprolog(&video_out);
							mp1e_mmx_copy_refblock();
							break;
						} else {
							if (motion
							    && (M[0].MV[0] | M[0].MV[1])) {
								if (prev_quant != quant) {
									bputl(&video_out, (code << 10) + (2 << 5) + quant, length + 10);
									/* macroblock_type '0001 0' (P MC, Coded, Quant), quantiser_scale_code 'xxxxx' */
								} else {
									bputl(&video_out, (code << 1) + 1, length + 1);
									/* macroblock_type '1' (P MC, Coded) */
								}

								motion_vector(&M[0], dmv[0]);
							
								bputl(&video_out, mp1e_coded_block_pattern[cbp].code,
								      mp1e_coded_block_pattern[cbp].length); // 3..9
							} else {
								if (prev_quant != quant) {
									code = (code << 10) + (1 << 5) + quant;
									length += 10;
									/* macroblock_type '0000 1' (P No MC, Coded, Quant), quantiser_scale_code 'xxxxx' */
								} else {
									code = (code << 2) + 1;
									length += 2;
									/* macroblock_type '01' (P No MC, Coded) */
								}

								len = mp1e_coded_block_pattern[cbp].length; // 3..9
								code = (code << len) + mp1e_coded_block_pattern[cbp].code;
								bputl(&video_out, code, length + len);
							}

							pr_start(46, "Encode inter");

							if (!mpeg1_encode_inter(mblock[0], cbp)) {
								pr_end(46);

								bprolog(&video_out);

								if (prev_quant < 0)
									quant1 = quant;
								prev_quant = quant;
				
								if (__builtin_expect(mpeg1->referenced, 1)) {
						    			pr_start(27, "IDCT inter");
									mpeg1_idct_inter(quant, cbp); // [0] & [3]
									pr_end(27);
								}

								break;
							}

							pr_end(46);

							quant++;
							brewind(&video_out, &mark);

							pr_event(45, "P/inter overflow");
						
						} // if coded macroblock
					} // if not skipped
				} // retry

				mpeg1->quant_sum += quant;
				mb_skipped = i;

				reset_dct_pred();
			}

			mba_col_incr();
			mb_count++;
#if TEST_PREVIEW
			if (preview > 1) {
				emms();
				packed_preview(newref, mb_width, mb_height);
			}
#endif
		}

		mba_row_incr();
	}

	emms();

	if (motion)
		t7(M[0].src_range, dist);

	/* Rate control */

	S = bflush(&video_out);
	
	*q1p |= (quant1 << 3);

	rc_picture_end(&mpeg1->rc, P_TYPE, S, mpeg1->quant_sum, mb_num);

	pr_end(24);

#if TEST_PREVIEW
	if (preview == 1)
		packed_preview(newref, mb_width, mb_height);
#endif

	return S >> 3;
}

static inline int
tmp_picture_b(mpeg1_context *mpeg1, unsigned char *org,
	      bool motion, int dist, int forward_motion, int backward_motion)
{
	short (* iblock)[6][8][8];
	int S, quant, prev_quant, quant1 = 1;
	struct bs_rec mark;
	unsigned char *q1p;
	unsigned int var, vmc, vmcf, vmcb;
	mb_type macroblock_type, mb_type_last;
	int mb_skipped, mb_count;
	struct motion M[2];

	if (motion) {
		motion_init(mpeg1, &M[0], forward_motion >> 8);
		motion_init(mpeg1, &M[1], backward_motion >> 8);
	} else {
		M[0].f_code = 1; M[1].f_code = 1;
		M[0].src_range = 0; M[1].src_range = 0;
	}

	printv(3, "Encoding B picture #%lld GOP #%d, fwd=%c, d=%d, f_code=%d (%d), %d (%d)\n",
		video_frame_count, mpeg1->gop_frame_count, "FT"[!mpeg1->closed_gop],
		dist, M[0].f_code, M[0].src_range, M[1].f_code, M[1].src_range);

	pr_start(25, "Picture B");

	reset_mba();

	rc_picture_start(&mpeg1->rc, B_TYPE, mb_num);

	mpeg1->quant_sum = 0;

	reset_dct_pred();


	prev_quant = -100;

	mb_count = 1;
	mb_skipped = 0;
	mb_type_last = -1;

	/* Picture header */

	bepilog(&video_out);

	bputl(&video_out, PICTURE_START_CODE, 32);

	bputl(&video_out, ((mpeg1->gop_frame_count & 1023) << 19)
	      + (B_TYPE << 16) + NO_VBV, 29);
	bputl(&video_out, (0 << 10) + (M[0].f_code << 7) + (0 << 6)
			            + (M[1].f_code << 3) + (0 << 2), 11);
	/*
	 *  temporal_reference [10], picture_coding_type [3], vbv_delay [16];
	 *  full_pel_forward_vector '0', forward_f_code,
	 *  full_pel_backward_vector '0', backward_f_code,
	 *  extra_bit_picture '0', byte align [2]
	 */

	bputl(&video_out, SLICE_START_CODE + 0, 32);
	q1p = (unsigned char *) video_out.p + (video_out.n >> 3);
	bputl(&video_out, (1 << 1) + 0, 6);
	/* slice header: quantiser_scale_code 'xxxxx', extra_bit_slice '0' */

	bprolog(&video_out);

	mpeg1->filter_param[0].src = org;

	for (mb_row = 0; mb_row < mb_height; mb_row++) {
		for (mb_col = 0; mb_col < mb_width; mb_col++) {
			unsigned int cbp;
			int dmv[2][2];

			/* Read macroblock (MMX state) */

			pr_start(41, "Filter");
			var = mpeg1->filter_param[0].func(&mpeg1->filter_param[0], mb_col, mb_row);
			pr_end(41);

			/* Choose prediction type */

if (T3RI
    && ((TEST12
         && motion
	 && (var < (unsigned int)(LOWVAR / (6 * B_SHARE)))
	 )))
	goto skip_pred;

			if (__builtin_expect(!mpeg1->closed_gop, 1)) {
				pr_start(52, "Predict bidirectional");
				if (motion)
					vmc = predict_bidirectional_motion(mpeg1, M, &vmcf, &vmcb, dist);
				else
					vmc = mpeg1->predict_bidirectional(
						mpeg1->oldref + mb_address.block[0].offset,
						newref + mb_address.block[0].offset,
						&vmcf, &vmcb);
				pr_end(52);
				macroblock_type = MB_INTERP;
				iblock = &mblock[3];

				if (vmcf <= vmcb && vmcf <= vmc) {
					if (motion) {
						/* -> dmv */
						M[1].MV[0] = M[1].PMV[0];
						M[1].MV[0] = M[1].PMV[0];
					}
					vmc = vmcf;
					macroblock_type = MB_FORWARD;
					iblock = &mblock[1];
				} else if (vmcb <= vmc) {
					if (motion) {
						M[0].MV[0] = M[0].PMV[0];
						M[0].MV[0] = M[0].PMV[0];
					}
					vmc = vmcb;
					macroblock_type = MB_BACKWARD;
					iblock = &mblock[2];
				}
			} else {
				if (motion) {
					M[0].MV[0] = M[0].PMV[0];
					M[0].MV[0] = M[0].PMV[0];
					vmc = predict_forward_motion(&M[1], newref, dist);
				} else
					vmc = mpeg1->predict_forward(newref + mb_address.block[0].offset);

				macroblock_type = MB_BACKWARD;
				iblock = &mblock[1];
			}
skip_pred:
			emms();

			/* Encode macroblock */

#if TEST34
			while (motion && vmc < T34VMC && (mb_row || mb_col) > 0) {
				unsigned int x, s2 = 0;
				int n;

				// fprintf(stderr, "+");

				/* chroma var */

				for (x = 4 * 64; x < 6 * 64; x++) {
					n = (*iblock)[0][0][x];
					s2 += n * n;
				}

				if (s2 * 256 * 2 >= T34VMC) {
					// fprintf(stderr, "\\");
					break;
				}

				quant = prev_quant; 

				if (motion) {
					dmv[0][0] = (M[0].MV[0] - M[0].PMV[0]) & M[0].f_mask;
					dmv[0][1] = (M[0].MV[1] - M[0].PMV[1]) & M[0].f_mask;
					dmv[1][0] = (M[1].MV[0] - M[1].PMV[0]) & M[1].f_mask;
					dmv[1][1] = (M[1].MV[1] - M[1].PMV[1]) & M[1].f_mask;
				}

				brewind(&mark, &video_out);

				vmc = 0;
				cbp = 0;
				goto l2;
			}
#endif
			if (T3RI
			    && ((TEST12
				 && motion
				 && (var < (unsigned int)(LOWVAR / (6 * B_SHARE))))
				|| vmc > p_inter_bias
				))
			{
				double act = var / VARQ + 1;
				unsigned int code;
				int length, i;

				/* Calculate quantization factor */

				quant = rc_quant(&mpeg1->rc, MB_FORWARD,
						 act, act,
						 bwritten(&video_out), QS, quant_max);
				quant = quant_res_intra[quant];

				macroblock_type = MB_INTRA;

				if (0 && var < LOWVARP) {
					quant = 3;
					fprintf(stderr, "-");
				}

				pb_intra_mb(TRUE);

				if (prev_quant < 0)
					quant1 = quant;

				prev_quant = quant;
				mpeg1->quant_sum += quant;

				mb_skipped = 0;

				if (motion) {
					M[0].PMV[0] = 0;
					M[0].PMV[1] = 0;
					M[1].PMV[0] = 0;
					M[1].PMV[1] = 0;
				}
			} else {
				unsigned int code;
				int len, length, i;

				/* Calculate quantization factor */

				quant = rc_quant(&mpeg1->rc, MB_INTERP,
						 var / VARQ + 1, vmc / VARQ + 1,
						 bwritten(&video_out), 0, quant_max);

				if (quant < 1) quant = 1;
				else
//				if (quant > 8 && vmc < 1500000) quant = 6;
//				else
				if (quant > quant_max) quant = quant_max;


if (!T3RT) quant = 2;

				if (motion) {
					dmv[0][0] = (M[0].MV[0] - M[0].PMV[0]) & M[0].f_mask;
					dmv[0][1] = (M[0].MV[1] - M[0].PMV[1]) & M[0].f_mask;
					dmv[1][0] = (M[1].MV[0] - M[1].PMV[0]) & M[1].f_mask;
					dmv[1][1] = (M[1].MV[1] - M[1].PMV[1]) & M[1].f_mask;
				}

				brewind(&mark, &video_out);

				for (;;) {
	pr_start(26, "FDCT inter");
	cbp = fdct_inter(*iblock, quant); // mblock[1|2|3] -> mblock[0]
	pr_end(26);
#if TEST34
l2:
#endif
	i = mb_skipped;

	if (cbp == 0
	    && macroblock_type == mb_type_last /* -> not first of slice */
	    && mb_count < mb_num /* not last of slice */
	    && (!motion
		|| (dmv[0][0] | dmv[0][1] | dmv[1][0] | dmv[1][1]) == 0)) {
		/* don't touch PMV here */
		i++;
		break;
	} else {
		bepilog(&video_out);

		macroblock_address(i); // 1..11

		i = 0;

		if (cbp == 0) {
			if (motion) {
				len = 5 - macroblock_type;
				bputl(&video_out, (code << len) + 2, length + len);
				/* macroblock_type (B Not Coded, see vlc.c) */

				if (macroblock_type & MB_FORWARD)
					motion_vector(&M[0], dmv[0]);
				if (macroblock_type & MB_BACKWARD)
					motion_vector(&M[1], dmv[1]);
			} else {
				len = mp1e_macroblock_type_b_nomc_notc[macroblock_type].length; // 5..6
				bputl(&video_out, (code << len) +
					mp1e_macroblock_type_b_nomc_notc[macroblock_type].code, length + len);
			}

			bprolog(&video_out);
			break;
		} else {
			if (motion) {
				if (prev_quant != quant) {
					len = mp1e_macroblock_type_b_quant[macroblock_type].length; // 10..11
					code = (code << len) + mp1e_macroblock_type_b_quant[macroblock_type].code + quant;
				} else {
					len = 5 - macroblock_type;
					code = (code << len) + 3;
					/* macroblock_type (B Coded, see vlc.c) */
				}

				bputl(&video_out, code, length + len);

				if (macroblock_type & MB_FORWARD)
					motion_vector(&M[0], dmv[0]);
				if (macroblock_type & MB_BACKWARD)
					motion_vector(&M[1], dmv[1]);
			} else {
				if (prev_quant != quant) {
					len = mp1e_macroblock_type_b_nomc_quant[macroblock_type].length; // 13..14
					code = (code << len)
						+ mp1e_macroblock_type_b_nomc_quant[macroblock_type].code
						+ (quant << mp1e_macroblock_type_b_nomc_quant[macroblock_type].mv_length);
				} else {
					len = mp1e_macroblock_type_b_nomc[macroblock_type].length; // 5..7
					code = (code << len) + mp1e_macroblock_type_b_nomc[macroblock_type].code;
				}

				bputl(&video_out, code, length + len);
			}

			bputl(&video_out, mp1e_coded_block_pattern[cbp].code,
			      mp1e_coded_block_pattern[cbp].length);

			pr_start(46, "Encode inter");

			if (__builtin_expect(!mpeg1_encode_inter(mblock[0], cbp), 1)) {
				pr_end(46);

				bprolog(&video_out);

				break;
			}

			pr_end(46);

			quant++;
			
			brewind(&video_out, &mark);

			pr_event(47, "B/inter overflow");
	
		} // coded mb
	} // not skipped
				} // while overflow

				if (prev_quant < 0)
					quant1 = quant;

				prev_quant = quant;

				mpeg1->quant_sum += quant;
				mb_skipped = i;

				if (motion && i == 0) {
					if (macroblock_type & MB_FORWARD) {
						M[0].PMV[0] = M[0].MV[0];
						M[0].PMV[1] = M[0].MV[1];
					}
					if (macroblock_type & MB_BACKWARD) {
						M[1].PMV[0] = M[1].MV[0];
						M[1].PMV[1] = M[1].MV[1];
					}
				}

				reset_dct_pred();
			}

			mba_col_incr();

			mb_count++;
			mb_type_last = macroblock_type;
		}

		mba_row_incr();
	}

	emms();

	/* Rate control */

	S = bflush(&video_out);

	*q1p |= (quant1 << 3);

	rc_picture_end(&mpeg1->rc, B_TYPE, S, mpeg1->quant_sum, mb_num);

	pr_end(25);

	return S >> 3;
}

static int
picture_i_nomc(mpeg1_context *mpeg1, unsigned char *org)
{
	return tmp_picture_i(mpeg1, org, FALSE);
}

static int
picture_p_nomc(mpeg1_context *mpeg1, unsigned char *org,
	       int dist, int forward_motion)
{
	return tmp_picture_p(mpeg1, org, FALSE, 0, 0);
}

static int
picture_b_nomc(mpeg1_context *mpeg1, unsigned char *org, int dist,
	  int forward_motion, int backward_motion)
{
	return tmp_picture_b(mpeg1, org, FALSE, 0, 0, 0);
}

static int
picture_i_mc(mpeg1_context *mpeg1, unsigned char *org)
{
	return tmp_picture_i(mpeg1, org, TRUE);
}

static int
picture_p_mc(mpeg1_context *mpeg1, unsigned char *org,
	     int dist, int forward_motion)
{
	return tmp_picture_p(mpeg1, org, TRUE, dist, forward_motion);
}

static int
picture_b_mc(mpeg1_context *mpeg1, unsigned char *org, int dist,
	  int forward_motion, int backward_motion)
{
	return tmp_picture_b(mpeg1, org, TRUE, dist,
			     forward_motion, backward_motion);
}

static int
picture_zero(mpeg1_context *mpeg1)
{
	unsigned int code;
	int length, i;

	printv(3, "Encoding 0 picture\n");

	bepilog(&video_out);

	bputl(&video_out, PICTURE_START_CODE, 32);
	
	bputl(&video_out, ((mpeg1->gop_frame_count & 1023) << 19)
	      + (P_TYPE << 16) + NO_VBV, 29);
	bputl(&video_out, (0 << 10) + (1 << 7) + (0 << 6), 11);
	/*
	 *  temporal_reference [10], picture_coding_type [3], vbv_delay [16];
	 *  full_pel_forward_vector '0', forward_f_code '001',
	 *  extra_bit_picture '0', byte align [6]
	 */

	bputl(&video_out, SLICE_START_CODE + 0, 32);
	bputl(&video_out, (1 << 7) + (0 << 6) + (1 << 5) + 0x07, 12);
	/*
	 *  slice header: quantiser_scale_code '00001', extra_bit_slice '0';
	 *  macroblock_address_increment '1' (+1);
	 *  macroblock_type '001' (P MC, Not Coded), '11' MV(0, 0)
	 */

	for (i = mb_num - 2; i >= 33; i -= 33)
		bputl(&video_out, 0x008, 11); /* mb addr escape */

	code = mp1e_macroblock_address_increment[i].code;
	length = mp1e_macroblock_address_increment[i].length; /* 1...11 */

	bputl(&video_out, (code << 5) + 7, length + 5);
	/* macroblock_type '001' (P MC, Not Coded), '11' MV(0, 0) */

	bprolog(&video_out);
	emms();

	return bflush(&video_out) >> 3;
}

/*
 *  Sequence layer
 */

enum {
	SKIP_METHOD_ZERO_P,
	SKIP_METHOD_MUX,
	SKIP_METHOD_FAKE
};

static int
sequence_header(mpeg1_context *mpeg1)
{
	int bit_rate_value;

	printv(3, "Encoding sequence header\n");

	if (!mpeg1->aspect_ratio_code) // XXX mp1e frontend
		switch (mpeg1->frame_rate_code) {
		case 1: // square pixels 23.97 Hz
		case 2: // square pixels 24 Hz
			mpeg1->aspect_ratio_code = 1;
			break;

		case 3: // CCIR 601, 625 line 25 Hz
		case 6: // CCIR 601, 625 line 50 Hz
			mpeg1->aspect_ratio_code = 8;
			break;

		case 4: // CCIR 601, 525 line 29.97 Hz
		case 5: // CCIR 601, 525 line 30 Hz
		case 7: // CCIR 601, 525 line 59.94 Hz
		case 8: // CCIR 601, 525 line 60 Hz
			mpeg1->aspect_ratio_code = 9;
			break;

		default:
			FAIL("Invalid frame_rate_code %d", mpeg1->frame_rate_code);
		}

	bit_rate_value = ceil(mpeg1->bit_rate / 400.0);
	assert(bit_rate_value >= 1 && bit_rate_value <= 0x3FFFF);

	bepilog(&video_out);

	bputl(&video_out, SEQUENCE_HEADER_CODE, 32);

	bputl(&video_out, mpeg1->coded_width & 0xFFF, 12);	/* horizontal_size_value */
	bputl(&video_out, mpeg1->coded_height & 0xFFF, 12);	/* vertical_size_value */

	bputl(&video_out, mpeg1->aspect_ratio_code, 4);		/* aspect_ratio_information */
	bputl(&video_out, mpeg1->frame_rate_code, 4);		/* frame_rate_code */
	bputl(&video_out, bit_rate_value & 0x3FFFF, 18);	/* bit_rate_value */
	bputl(&video_out, 1, 1);				/* marker_bit */
	bputl(&video_out, 0 & 0x3FF, 10);			/* vbv_buffer_size_value */
	bputl(&video_out, 0, 1);				/* constrained_parameters_flag */
	bputl(&video_out, 0, 1);				/* load_intra_quantizer_matrix */
	bputl(&video_out, 0, 1);				/* load_non_intra_quantizer_matrix */

	bprolog(&video_out);

	emms();

	return bflush(&video_out) >> 3;
}

static void
user_data(char *s)
{
	int n = strlen(s);

	if (n > 0) {
		bputl(&video_out, USER_DATA_START_CODE, 32);
		while (n--)
			bputl(&video_out, *s++, 8);
	}
}

#define Rvbr (1.0 / 16)

extern int test_mode;

static inline void
_send_full_buffer(mpeg1_context *mpeg1, buffer *b)
{
        video_eff_bit_rate +=
		((b->used * 8) * mpeg1->coded_frame_rate
		 - video_eff_bit_rate) * Rvbr;

      	if (test_mode & 16)
		printv(0, "vebr %f \r", (double) video_eff_bit_rate);

	pthread_mutex_lock(&mpeg1->codec.codec.mutex);
	mpeg1->codec.status.frames_out++;
	mpeg1->codec.status.coded_time = b->time;
	mpeg1->codec.status.bytes_out += b->used;
	pthread_mutex_unlock(&mpeg1->codec.codec.mutex);

	send_full_buffer(&mpeg1->prod, b);
}

/*
  012345678901234567890123456789
  IBBPBBPBBPBBPBBIBBPBBPBBPBBPBB	gop
   ^    ^    ^ ^ ^ ^  ^^^^^ ^^^		skipped/dropped

  012345678901234567890123456789
  I0BPBp0BBPp0P0p0p0Pp00000p000B	replace
  I-BPBB-BBPB-P-B-B-PB-----B---B	(discarded)

  0 1234 5678 9 0 1 23     4   5
  I0BBPp0BPBp0P0p0p0Pp00000p000I	extend
  I-BBPB-BPBB-P-B-B-PB-----B---I
*/

/*
 *  Encode replacements for the pictures skipped
 *  between this and the previously coded picture.
 *  Can't create NULL frames on the B-stack due to
 *  finite size, hence this->skipped.
 *
 *  NB. IB-B-P -> IPB-B-, I-P -> IP-, ie. skipped
 *  pictures are always coded in order like B frames. 
 */
static void
encode_skipped_frames(mpeg1_context *mpeg1, stacked_frame *this)
{
	buffer *obuf;

	for (; this->skipped > 0; this->skipped--) {
		switch (mpeg1->skip_method) {
		case SKIP_METHOD_FAKE:
			/*
			 *  We encode a placeholder for the
			 *  missing frame (beyond MPEG-1). The mux picture
			 *  type is B, coded in order with normal time stamping.
			 */

			printv(3, "Encoding fake picture #%lld GOP #%d\n",
			       video_frame_count, mpeg1->gop_frame_count);

			obuf = wait_empty_buffer(&mpeg1->prod);

			obuf->type = B_TYPE;
			obuf->offset = 0;
			obuf->used = 12;
			obuf->time = this->time;

			((uint32_t *) obuf->data)[0] =
				swab32(PICTURE_START_CODE);
			((uint32_t *) obuf->data)[1] =
				swab32(((mpeg1->gop_frame_count & 1023) << 22)
				       + (7 << 19) + (NO_VBV << 3) + (0 << 2));
			((uint32_t *) obuf->data)[2] =
				swab32((1 << 31) + (0 << 30) + (1 << 27)
				       + (0 << 26) + 0);
			/*
			 *  temporal_reference [10],
			 *  picture_coding_type [3] $$ 7 $$, vbv_delay [16],
			 *  full_pel_forward_vector '0', forward_f_code [3],
			 *  full_pel_backward_vector '0', backward_f_code [3],
			 *  extra_bit_picture '0', byte align [2]
			 */

			_send_full_buffer(mpeg1, obuf);

			mpeg1->gop_frame_count++;

			break;

		case SKIP_METHOD_MUX:
			/*
			 *  We encode no frame, the player has to slow down
			 *  playback according to the DTS/PTS of coded pictures
			 *  (beyond MPEG-1). Note we still send the mux a
			 *  dummy picture for proper a/v sync. used = 0 is
			 *  not possible because that's an EOF sign. The picture
			 *  type is 'none' to suppress the PTS stamp.
			 */

			printv(3, "Skipping picture (not counted)\n");

			obuf = wait_empty_buffer(&mpeg1->prod);

			obuf->type = 0;
			obuf->offset = 0;
			obuf->used = 4;
			obuf->time = this->time;

			memset(obuf->data, 0, 4);

			_send_full_buffer(mpeg1, obuf);

			/* MPEG does not permit gaps */
			/* mpeg1->gop_frame_count++; */

			break;
		}
	}
}

static inline void
encode_bframes(mpeg1_context *mpeg1, int stacked_bframes)
{
	buffer *obuf, *last = NULL;
	int dist = 0, i;

	mpeg1->referenced = FALSE;

	for (i = 0; i < stacked_bframes; i++) {
		stacked_frame *this = mpeg1->stack + i;

		if (this->skipped > 0) {
			dist += this->skipped;
			encode_skipped_frames(mpeg1, this);
		}

		if (!this->org) {
			/*
			 *  We've lost this frame and couldn't "skip" it,
			 *  but the preceeding B frame has been coded, so
			 *  let's duplicate it.
			 */

			assert(last != NULL);

			obuf = wait_empty_buffer(&mpeg1->prod);

			printv(3, "Dupl'ing B picture #%lld GOP #%d\n",
			       video_frame_count, mpeg1->gop_frame_count);

			memcpy(obuf->data, last->data, last->used);
			
			((uint32_t *) obuf->data)[1] =
				swab32((swab32(((uint32_t *) obuf->data)[1]) & ~(1023 << 22)) |
				       ((mpeg1->gop_frame_count & 1023) << 22));

			obuf->type = B_TYPE;
			obuf->offset = 0;
			obuf->used = last->used;
			obuf->time = this->time;

			_send_full_buffer(mpeg1, last);

			mpeg1->gop_frame_count++;
		} else {
			int forward, backward;

			obuf = wait_empty_buffer(&mpeg1->prod);
			bstart(&video_out, obuf->data);

			/*
			 *  dist: number 0++ of B frame
			 *    + skipped frames since last keyframe
			 *  skipped_zero: last keyframe did not update
			 *    decoder (eg. IBP00B: dist=0, sz=2 -> fwd = 3)
			 *  skipped_fake: skipped frames between last
			 *    and next keyframe
			 */
			forward = mpeg1->skipped_zero + dist + 1;
			backward = mpeg1->skipped_fake + stacked_bframes - dist;

			obuf->used = mpeg1->picture_b(mpeg1, this->org,
						      forward, forward * motion,
						      backward * motion);
			if (this->buffer)
				send_empty_buffer(&mpeg1->cons, this->buffer);

			obuf->type = B_TYPE;
			obuf->offset = 0;
			obuf->time = this->time;

			if (last)
				_send_full_buffer(mpeg1, last);

			mpeg1->gop_frame_count++;
		}

		last = obuf;

		dist++;

		video_frame_count++;
		mpeg1->seq_frame_count++;
		mpeg1->coded_frames_countd -= 1.0;
	}

	if (last)
		_send_full_buffer(mpeg1, last);
}

static inline bool
encode_keyframe(mpeg1_context *mpeg1, stacked_frame *this,
		int stacked_bframes, int key_offset,
		buffer *obuf, bool pframe)
{
	mpeg1->gop_frame_count += key_offset;

	obuf->used = 0;

	if (pframe && mpeg1->p_succ < MAX_P_SUCC) {
		struct bs_rec mark;
		int forward;

		brewind(&mark, &video_out);

		forward = mpeg1->skipped_zero /* I/P but didn't update decoder */
			+ mpeg1->skipped_fake /* skipped since last I/P frame */
			+ stacked_bframes;

		obuf->used = mpeg1->picture_p(mpeg1, this->org,
					      forward + 1, motion);
		if (obuf->used) {
			obuf->type = P_TYPE;
			mpeg1->p_succ++;
		} else /* scene cut, promote this P to I picture */
			brewind(&video_out, &mark);
	}

	if (!obuf->used) {
		obuf->used = mpeg1->picture_i(mpeg1, this->org);
		obuf->type = I_TYPE;
		pframe = FALSE;
		mpeg1->p_succ = 0;
	}

	mpeg1->gop_frame_count -= key_offset;

	/* Skipped frames don't add to GOP, but DTS/PTS */
	if (mpeg1->skip_method == SKIP_METHOD_MUX)
		key_offset += mpeg1->skipped_fake;

	obuf->offset = key_offset;
	obuf->time = this->time;

	if (this->buffer)
		send_empty_buffer(&mpeg1->cons, this->buffer);

	_send_full_buffer(mpeg1, obuf);

	video_frame_count++;
	mpeg1->seq_frame_count++;
	mpeg1->coded_frames_countd -= 1.0;

	return pframe;
}

static bool
encode_stacked_frames(mpeg1_context *mpeg1, buffer *obuf, int stacked, bool pframe)
{
	int stacked_bframes = stacked - 1;
	int key_offset = stacked_bframes;
	stacked_frame *this;

	if (mpeg1->skip_method == SKIP_METHOD_FAKE)
		key_offset += mpeg1->skipped_fake;

	/* Encode I or P frame */

	this = mpeg1->stack + stacked - 1;

	pframe = encode_keyframe(mpeg1, this, stacked_bframes,
				 key_offset, obuf, pframe);

	/* Encode B frames */

	encode_bframes(mpeg1, stacked_bframes);

	if (this->skipped > 0)
		encode_skipped_frames(mpeg1, this);

	mpeg1->gop_frame_count++; /* the keyframe */

	mpeg1->skipped_fake = 0;

	return pframe;
}

static void
skip_zerop(mpeg1_context *mpeg1, stacked_frame *this, int sp)
{
	buffer *obuf;

	if (this->buffer && this->org)
		send_empty_buffer(&mpeg1->cons, this->buffer);

	if (sp < 1)
		return;

	if (sp >= 2) {
		int b = sp - 1;

		obuf = wait_empty_buffer(&mpeg1->prod);
		bstart(&video_out, obuf->data);

		mpeg1->referenced = TRUE;

		if (encode_stacked_frames(mpeg1, obuf, b, TRUE))
			mpeg1->rc.Ep += b;
		else
			mpeg1->rc.Ei += b;

		mpeg1->rc.Eb -= b;
	}

	printv(3, "Encoding 0 picture #%lld GOP #%d\n",
	       video_frame_count, mpeg1->gop_frame_count);

	obuf = wait_empty_buffer(&mpeg1->prod);

	memcpy(obuf->data, mpeg1->zerop_template, mpeg1->Sz);

	((uint32_t *) obuf->data)[1] =
		swab32((swab32(((uint32_t *) obuf->data)[1])
			& ~(1023 << 22))
		       | ((mpeg1->gop_frame_count & 1023) << 22));

	obuf->type = P_TYPE;
	obuf->offset = 0;
	obuf->used = mpeg1->Sz;
	obuf->time = this->time;

	_send_full_buffer(mpeg1, obuf);

	video_frame_count++;
	mpeg1->gop_frame_count++;
	mpeg1->coded_frames_countd -= 1.0;
	mpeg1->skipped_zero++; /* did not update decoder */
}

int video_do_reset = FALSE;

/* FIXME 0P insertion and skip/fake can overflow GOP count */

static rte_bool
thread_body(mpeg1_context *mpeg1)
{
	rte_bool eof = FALSE;
	rte_bool done = FALSE;
	char *seq = "";
	buffer *obuf;

	while (!done) {
		stacked_frame *this = NULL;
		int sp = 0;

		for (;;) {
			this = mpeg1->stack + sp;

			if (mpeg1->last.org) {
				this->buffer = mpeg1->last.buffer;
				this->org = mpeg1->last.org;
				this->time = mpeg1->last.buffer->time;
				mpeg1->last.org = NULL;
			} else {
				buffer *b;

				this->buffer = b = wait_full_buffer(&mpeg1->cons);

				if (b->used > 0) {
					if (test_mode) {
						if ((test_mode & 8)
						    && (rand() % 100) < 20) {
							printv(3, "Forced drop #%lld\n", video_frame_count + sp);
							send_empty_buffer(&mpeg1->cons, this->buffer);

							continue;
						}
						
						if (test_mode & 0x200) {
							int i = 0;
							
							for (i = 0; i < b->used; i++) {
								((uint8_t *) b->data)[i] =
									(((i >> 4) ^ (i >> 10))
									 & 1) - 1;
							}		
						}
					}

					this->org = b->data;
					this->time = b->time;

#ifdef VIDEO_FIFO_TEST
					{
						double now = current_time();

						in_fifo_load = mpeg1->nominal_frame_rate
							* (now - b->time);
						out_fifo_load += (mpeg1->prod.fifo->full.members
							- out_fifo_load) * 0.1;
					}
#endif
				} else {
					this->buffer = NULL;
					this->org = NULL;

					send_empty_buffer(&mpeg1->cons, b);
				}
			}

			if (this->buffer && mpeg1->last.time
			    && this->time > (mpeg1->last.time + mpeg1->nominal_frame_period * 1.5)) {
				/* Count dropped frames we would skip anyway */
				/* video_frames_dropped++; */
				mpeg1->last.buffer = this->buffer;
				mpeg1->last.org = this->org;
				this->org = NULL;
				this->time = mpeg1->last.time += mpeg1->nominal_frame_period;
			} else {
				mpeg1->last.time = this->time;
			}

			sp++;

			mp1e_sync_drift(&mpeg1->codec.sstr, this->time, mpeg1->coded_elapsed);

			mpeg1->coded_elapsed += mpeg1->coded_frame_period;

			if (!this->buffer /* eof */
			    || mp1e_sync_break(&mpeg1->codec.sstr, this->time))
				eof = TRUE;

			if (eof || (mpeg1->coded_frames_countd - sp) < 0.0) {
				printv(2, "Video: End of file\n");

				if (this->buffer && this->org)
					send_empty_buffer(&mpeg1->cons, this->buffer);

				if (mpeg1->last.org) {
					send_empty_buffer(&mpeg1->cons, mpeg1->last.buffer);
					mpeg1->last.org = NULL;
				}

				while (*seq == 'B')
					seq++;

				done = TRUE;
				this--;
				sp--;

				if (sp <= 0)
					goto finish;

				goto next_frame; /* finish with B*[PI] */
			}

			mpeg1->skip_rate_acc += mpeg1->virtual_frame_rate;

			if (!this->org) { /* missed */
				if (mpeg1->skip_rate_acc >= mpeg1->nominal_frame_rate) {
					video_frames_dropped++;

					if (0 && sp >= 2 && *seq == 'B') {
						/*
						 *  We have BB, duplicate last B instead of
						 *  promoting to z (but only if the next I or P
						 *  isn't missing as well). NB this will advance
						 *  the GOP seq pointer, skipping doesn't.
					       	 *
						 *  Temporarily disabled: BBXBbX !-> Bp0Bp0
						 */
						goto next_frame;
					} else /* skip this */
						mpeg1->skip_rate_acc = 0;
				} /* else we skip it anyway */
			}

			/* XXX */
			this->time = mpeg1->coded_time_elapsed;
			mpeg1->coded_time_elapsed += mpeg1->coded_frame_period;

			if (mpeg1->skip_rate_acc < mpeg1->nominal_frame_rate) {
				switch (mpeg1->skip_method) {
				case SKIP_METHOD_MUX:
				case SKIP_METHOD_FAKE:
					if (this->buffer && this->org)
						send_empty_buffer(&mpeg1->cons, this->buffer);

					mpeg1->skipped_fake++;
					this->skipped++;
					sp--;

					continue;

				case SKIP_METHOD_ZERO_P:
					skip_zerop(mpeg1, this, sp);

					sp = 0;

					continue;
				}
			}
next_frame:
			mpeg1->skip_rate_acc -= mpeg1->nominal_frame_rate;

			if (*seq != 'B')
				break;

			/* This is a B picture, stack it up */

			seq++;
		}

		/* Encode P or I picture plus sequence or GOP headers */

		obuf = wait_empty_buffer(&mpeg1->prod);
		bstart(&video_out, obuf->data);

		if (!*seq) {
			/* End of GOP sequence */
#if TEST_PREVIEW
if (video_do_reset) {
	static void video_reset(mpeg1_context *mpeg1);

	video_reset(mpeg1);
	video_do_reset = FALSE;

	bstart(&video_out, obuf->data);
}
#endif

			bepilog(&video_out);

			/* Sequence header */

			if (mpeg1->seq_frame_count >= mpeg1->frames_per_seqhdr) {
				printv(3, "[Sequence header]\n");

				memcpy(video_out.p, mpeg1->seq_header_template, 16);
				video_out.p += 16 * 8 / 64;

				if (video_frame_count == 0 && mpeg1->banner)
					user_data(mpeg1->banner);

				mpeg1->seq_frame_count = 0;
				mpeg1->closed_gop = TRUE;
			}

			/* GOP header */

			if (mpeg1->insert_gop_header) {
				if (sp == 1)
					mpeg1->closed_gop = TRUE;

				printv(3, "[GOP header, closed=%c]\n", "FT"[mpeg1->closed_gop]);

				bputl(&video_out, GROUP_START_CODE, 32);
				bputl(&video_out, (mpeg1->closed_gop << 19) + (0 << 6), 32);
				/*
				 *  time_code [25 w/marker_bit] omitted, closed_gop,
				 *  broken_link '0', byte align [5]
				 */

				mpeg1->gop_frame_count = 0;
			}

			bprolog(&video_out);
			emms();

			seq = mpeg1->gop_sequence;

// XXX
if (mpeg1->rc.gop_count > 0) {
mpeg1->rc.ei = mpeg1->rc.Ei / mpeg1->rc.gop_count;
mpeg1->rc.ep = mpeg1->rc.Ep / mpeg1->rc.gop_count;
mpeg1->rc.eb = mpeg1->rc.Eb / mpeg1->rc.gop_count;
}
mpeg1->rc.gop_count++;
//printv(0, "Eit=%f Ept=%f Ebt=%f \n", ei, ep, eb);

			printv(4, "Rewind sequence R=%d\n", mpeg1->rc.R);

			mpeg1->rc.G0 = mpeg1->rc.Gn;
			mpeg1->rc.ob = 0;
		}

		mpeg1->referenced = (seq[1] == 'P') || (seq[1] == 'B')
			|| (sp > 1) || preview;

		encode_stacked_frames(mpeg1, obuf, sp, *seq == 'P');

		mpeg1->closed_gop = FALSE;

		seq++;

		sp = 0;
	}

finish:
	/* Stream end code */

	if (video_frame_count > 0) {
		obuf = wait_empty_buffer(&mpeg1->prod);
		((unsigned int *) obuf->data)[0] = swab32(SEQUENCE_END_CODE);
		obuf->type = 0; /* no picture, no timestamp */
		obuf->offset = 0;
		obuf->used = 4;
		obuf->time = mpeg1->last.time +=
			mpeg1->nominal_frame_period; /* not used */
		_send_full_buffer(mpeg1, obuf);
	}

	return eof;
}

static void
rc_reset(mpeg1_context *mpeg1)
{
	double x;

	mpeg1->rc.R	= 0;
	mpeg1->rc.r31	= (double) quant_max
		/ lroundn(mpeg1->bit_rate / mpeg1->virtual_frame_rate * 1.0);
	mpeg1->rc.Tmin	= lroundn(mpeg1->bit_rate / mpeg1->virtual_frame_rate / 8.0);
	mpeg1->rc.avg_acti = 400.0;
	mpeg1->rc.avg_actp = 400.0;

	mpeg1->rc.Xi	= lroundn(160.0 * mpeg1->bit_rate / 115.0);
	mpeg1->rc.Xp	= lroundn( 60.0 * mpeg1->bit_rate / 115.0); 
	mpeg1->rc.Xb	= lroundn( 42.0 * mpeg1->bit_rate / 115.0); 

	mpeg1->rc.d0i	= 10.0 / mpeg1->rc.r31;
	mpeg1->rc.d0p	= 10.0 / mpeg1->rc.r31;
	mpeg1->rc.d0b	= 14.0 / mpeg1->rc.r31;

	x	= mpeg1->nominal_frame_rate / mpeg1->virtual_frame_rate - 1.0;

	mpeg1->rc.G0	= lroundn((mpeg1->rc.ni + mpeg1->rc.np + mpeg1->rc.nb - mpeg1->rc.ob)
				  * mpeg1->bit_rate / mpeg1->virtual_frame_rate);
	mpeg1->rc.Gn	= lroundn((mpeg1->rc.ni + mpeg1->rc.np + mpeg1->rc.nb)
				  * mpeg1->bit_rate / mpeg1->virtual_frame_rate);
	
	mpeg1->rc.Gn	-= (mpeg1->rc.ni + mpeg1->rc.np + mpeg1->rc.nb) * x * mpeg1->Sz;

	mpeg1->rc.Tavg	= lroundn(mpeg1->bit_rate / mpeg1->virtual_frame_rate);
	mpeg1->rc.G4	= mpeg1->rc.Gn * 4;

	mpeg1->rc.R = mpeg1->rc.G0 = mpeg1->rc.Gn;


	mpeg1->rc.Ei = 0, mpeg1->rc.Ep = 0, mpeg1->rc.Eb = 0;
	mpeg1->rc.ei = 0, mpeg1->rc.ep = 0, mpeg1->rc.eb = 0;
	mpeg1->rc.gop_count = 0;
}

void *
mp1e_mpeg1(void *codec)
{
	mpeg1_context *mpeg1 = PARENT(codec, mpeg1_context, codec);
	buffer *obuf;

	printv(3, "Video compression thread\n");

	assert(mpeg1->codec.codec.state == RTE_STATE_RUNNING);

	/* XXX this function isn't reentrant. yet. */
	assert(static_context == mpeg1);

	if (!add_consumer(mpeg1->codec.input, &mpeg1->cons))
		return (void *) -1;
	// XXX need better exit method

	if (!add_producer(mpeg1->codec.output, &mpeg1->prod)) {
		rem_consumer(&mpeg1->cons);
		return (void *) -1;
	}

	if (!mp1e_sync_run_in(&PARENT(mpeg1->codec.codec.context,
				      mp1e_context, context)->sync,
			      &mpeg1->codec.sstr, &mpeg1->cons, NULL)) {
		rem_consumer(&mpeg1->cons);
		rem_producer(&mpeg1->prod);
		return (void *) -1;
	}

	mpeg1->filter_param[0].dest = (void *) &mblock[0];

	for (;;) {
		extern int split_sequence;

		if (thread_body(mpeg1) || !split_sequence)
			break;

		obuf = wait_empty_buffer(&mpeg1->prod);
		obuf->used = 0;
		obuf->error = -1;
		send_full_buffer(&mpeg1->prod, obuf);

		mpeg1->prod.eof_sent = FALSE;
		mpeg1->prod.fifo->eof_count = 0;

		mpeg1->coded_frames_countd += mpeg1->num_frames;

		mpeg1->seq_frame_count = mpeg1->frames_per_seqhdr;
		mpeg1->gop_frame_count = 0;

		mpeg1->skip_rate_acc = mpeg1->nominal_frame_rate
			- mpeg1->virtual_frame_rate
			+ mpeg1->nominal_frame_rate / 2.0;

		/* XXX */
		mpeg1->coded_time_elapsed = 0.0;

		rc_reset(mpeg1);

		mpeg1->last.org = NULL;
		mpeg1->last.time = 0;
		mpeg1->p_succ = 0;

		mpeg1->mb_cx_row = mb_height;
		mpeg1->mb_cx_thresh = 100000;

		if (mb_height >= 10) {
			mpeg1->mb_cx_row /= 3;
			mpeg1->mb_cx_thresh =
				lroundn(mpeg1->mb_cx_row * mb_width * 0.95);
		}
	}

	/* End of file */

	obuf = wait_empty_buffer(&mpeg1->prod);
	obuf->used = 0;
	obuf->error = 0xE0F;
	send_full_buffer(&mpeg1->prod, obuf);

	rem_consumer(&mpeg1->cons);
	rem_producer(&mpeg1->prod);

	static_context = NULL;

	return NULL;
}

/*
 *  API
 */

#include <stdarg.h>

#undef elements
#define elements(array) (sizeof(array) / sizeof(array[0]))

static void
video_reset(mpeg1_context *mpeg1)
{
	int header_size;

	mpeg1->coded_frame_rate = frame_rate_value[mpeg1->frame_rate_code];
	mpeg1->coded_frame_period = 1.0 / mpeg1->coded_frame_rate;

	if (mpeg1->virtual_frame_rate > mpeg1->coded_frame_rate)
		mpeg1->virtual_frame_rate = mpeg1->coded_frame_rate;

	mpeg1->seq_frame_count = mpeg1->frames_per_seqhdr;
	mpeg1->gop_frame_count = 0;

	if (!mpeg1->nominal_frame_rate) // XXX mp1e frontend
		mpeg1->nominal_frame_rate = frame_rate_value[mpeg1->frame_rate_code];

	mpeg1->nominal_frame_period = 1.0 / mpeg1->nominal_frame_rate;

	mpeg1->skip_rate_acc = mpeg1->nominal_frame_rate
		- mpeg1->virtual_frame_rate + mpeg1->nominal_frame_rate / 2.0;

	mpeg1->drop_timeout = mpeg1->nominal_frame_period * 1.5;
	mpeg1->coded_time_elapsed = 0.0;

	rc_reset(mpeg1);

	bstart(&video_out, mpeg1->seq_header_template);
	header_size = sequence_header(mpeg1);
	assert(header_size == 16);

	if (mpeg1->banner)
		free(mpeg1->banner);

	asprintf((char **) &mpeg1->banner,
		 mpeg1->anno && mpeg1->anno[0] ?
		 "MP1E " VERSION "\nANNO: %s\n" : "MP1E " VERSION "\n",
		 mpeg1->anno);
}

#define GOP_RULE							\
"A valid GOP sequence can consist of the picture types 'I' "		\
"intra coded, 'P' forward predicted and 'B' bidirectionally "		\
"predicted, in any order headed by an 'I' picture."

static bool
gop_validation(mpeg1_context *mpeg1, char *gop_sequence)
{
	int bmax;
	int i;

	if (!gop_sequence
	    || gop_sequence[0] != 'I'
	    || strspn(gop_sequence, "IPB") != strlen(gop_sequence)) {
		rte_error_printf(mpeg1->codec.codec.context,
				 _("Invalid group of pictures sequence: \"%s\".\n"
				   GOP_RULE), gop_sequence);
		return FALSE;
	}

	if (strlen(gop_sequence) > 1024) {
		rte_error_printf(mpeg1->codec.codec.context,
				 _("Invalid group of pictures sequence: \"%s\", length %d.\n"
				   "The number of pictures in a GOP is limited to 1024."),
				 gop_sequence, strlen(gop_sequence));
		return FALSE;
	}

	if (strchr(gop_sequence, 'P') ||
	    strchr(gop_sequence, 'B'))
		mpeg1->insert_gop_header = TRUE;

	/*
	 *  I, P and B in GOP for rate control
	 */
	mpeg1->rc.ni =
	mpeg1->rc.np =
	mpeg1->rc.nb = 0;

	bmax = 0;

	for (i = 0; i < 1024; i++)
		switch (gop_sequence[i]) {
		case 'I':
			mpeg1->rc.ni++;
			mpeg1->rc.ob = 0;
			break;

		case 'P':
			mpeg1->rc.np++;
			mpeg1->rc.ob = 0;
			break;

		case 'B':
			/*
			 *  ob: GOP overlapping B pictures (BB|I)
			 *  bmax: max. successive B pictures
			 */
			mpeg1->rc.nb++;
			mpeg1->rc.ob++;

			if (mpeg1->rc.ob > bmax)
				bmax = mpeg1->rc.ob;

			break;

		default:
			i = 1024; /* abort */
		}

	/*
	 *  One position used by I or P.
	 */
	if (bmax >= elements(mpeg1->stack)) {
		rte_error_printf(mpeg1->codec.codec.context,
				 _("Invalid group of pictures sequence: \"%s\".\n"
				   "The number of successive 'B' bidirectionally predicted "
				   "pictures is limited to %u."),
				 gop_sequence, elements(mpeg1->stack) - 1);
		return FALSE;
	}

	return TRUE;
}

static inline void
do_free(void **pp)
{
	if (*pp) {
		free(*pp);
		*pp = NULL;
	}
}

static void
uninit(rte_codec *codec)
{
	mpeg1_context *mpeg1 = PARENT(codec, mpeg1_context, codec);

	/* 0P */
	do_free((void **) &mpeg1->zerop_template);

	/* main buffers */
	do_free((void **) &mm_mbrow);
	do_free((void **) &newref);
	do_free((void **) &mpeg1->oldref);

	do_free((void **) &mpeg1->mb_hist);

	do_free((void **) &mpeg1->banner);

	if (static_context == mpeg1)
		static_context = NULL;
}

static int
dvec_imin(const double *vec, int size, double val)
{
	int i, imin = 0;
	double d, dmin = DBL_MAX;

	assert(size > 0);

	for (i = 0; i < size; i++) {
		d = fabs(val - vec[i]);

		if (d < dmin) {
			dmin = d;
		        imin = i;
		}
	}

	return imin;
}

static rte_bool
parameters_set(rte_codec *codec, rte_stream_parameters *rsp)
{
	extern filter_fn pmmx_YUV420_0, sse_YUV420_0;
	extern filter_fn pmmx_YUYV_6, sse_YUYV_6;
	extern int filter_mode;
	mpeg1_context *mpeg1 = PARENT(codec, mpeg1_context, codec);
	rte_bool packed = TRUE;
	int size;

	switch (codec->state) {
	case RTE_STATE_NEW:
		break;

	case RTE_STATE_PARAM:
		uninit(codec);
		break;

	default:
		assert(!"reached");
	}

	if (rsp->video.width <= 0 || rsp->video.height <= 0)
		goto reject;

	// XXX relax
	rsp->video.width = (saturate(rsp->video.width, 16, MAX_WIDTH) + 8) & -16;
	rsp->video.height = (saturate(rsp->video.height, 16, MAX_HEIGHT) + 8) & -16;

	mpeg1->coded_width = rsp->video.width;
	mpeg1->coded_height = rsp->video.height;

	video_coding_size(mpeg1->coded_width, mpeg1->coded_height);

	if (filter_mode != -1) { // XXX mp1e frontend
		extern int motion_min, motion_max;
		extern int width, height;

		mpeg1->coded_width = width;
		mpeg1->coded_height = height;

		video_coding_size(mpeg1->coded_width, mpeg1->coded_height);

		mpeg1->motion_min = motion_min; // option
		mpeg1->motion_max = motion_max;
	} else {
		extern int cpu_type;
	  	int sse;

	// XXX relax
	rsp->video.width = (saturate(rsp->video.width, 16, MAX_WIDTH) + 8) & -16;
	rsp->video.height = (saturate(rsp->video.height, 16, MAX_HEIGHT) + 8) & -16;

	mpeg1->coded_width = rsp->video.width;
	mpeg1->coded_height = rsp->video.height;

	video_coding_size(mpeg1->coded_width, mpeg1->coded_height);

	switch (cpu_type) {
	case CPU_PENTIUM_III:
	case CPU_PENTIUM_4:
	case CPU_ATHLON:
		sse = 1;
		break;

	default:
		sse = 0;
		break;
	}

	switch (rsp->video.pixfmt) {
	case RTE_PIXFMT_YUV420:
		if (rsp->video.stride == 0)
			rsp->video.stride = rsp->video.width;
		if (rsp->video.uv_stride == 0)
			rsp->video.uv_stride = rsp->video.width >> 1;
		if (rsp->video.u_offset == 0 || rsp->video.v_offset == 0) {
			rsp->video.u_offset = rsp->video.height * rsp->video.stride;
			rsp->video.v_offset += (rsp->video.height >> 1) * rsp->video.uv_stride;
		}

		mpeg1->codec.input_buffer_size =
			MAX(rsp->video.offset + rsp->video.stride * rsp->video.height,
			    MAX(rsp->video.u_offset, rsp->video.v_offset)
			    + rsp->video.uv_stride * (rsp->video.height >> 1));

		mpeg1->filter_param[0].dest = (void *) &mblock[0];
		mpeg1->filter_param[0].offset = rsp->video.offset;
		mpeg1->filter_param[0].u_offset = rsp->video.u_offset;
		mpeg1->filter_param[0].v_offset = rsp->video.v_offset;
		mpeg1->filter_param[0].stride = rsp->video.stride;
		mpeg1->filter_param[0].uv_stride = rsp->video.uv_stride;
		mpeg1->filter_param[0].func = sse ? sse_YUV420_0 : pmmx_YUV420_0;

		break;

	case RTE_PIXFMT_YUYV:
		if (rsp->video.stride == 0)
			rsp->video.stride = rsp->video.width * 2;

		mpeg1->codec.input_buffer_size =
			rsp->video.offset + rsp->video.stride * rsp->video.height;

		mpeg1->filter_param[0].dest = (void *) &mblock[0];
		mpeg1->filter_param[0].offset = rsp->video.offset;
		mpeg1->filter_param[0].stride = rsp->video.stride;
		mpeg1->filter_param[0].func = sse ? sse_YUYV_6 : pmmx_YUYV_6;

		break;

	default:
		goto reject;
	}
	}

	if (rsp->video.frame_rate <= 24.0)
		rsp->video.frame_rate = 24.0;

	mpeg1->nominal_frame_rate = rsp->video.frame_rate;

	if (!rsp->video.pixel_aspect)
		rsp->video.pixel_aspect = 1.0;

	mpeg1->aspect_ratio_code = dvec_imin(&aspect_ratio_value[1], 14,
					     rsp->video.pixel_aspect); 

	rsp->video.pixel_aspect = aspect_ratio_value[mpeg1->aspect_ratio_code];

	memcpy(&codec->params, rsp, sizeof(codec->params));



	/* XXX this codec isn't reentrant */
	if (!static_context)
		static_context = mpeg1;
	assert(static_context == mpeg1);


	memset(&mpeg1->codec.sstr, 0, sizeof(mpeg1->codec.sstr));

	mpeg1->codec.sstr.frame_period = mpeg1->nominal_frame_period;


	assert(gop_validation(mpeg1, mpeg1->gop_sequence));

	mpeg1->frames_per_seqhdr = 12;


	video_frames_dropped = 0;
	video_frame_count = 0;
	mpeg1->coded_frames_countd = mpeg1->num_frames;


	/* Init motion compensation */

	/* XXX */
	if (mpeg1->motion_compensation) {
		mpeg1->motion_min = 4;
		mpeg1->motion_max = 24;
	}

	if (mpeg1->motion_min && mpeg1->motion_max
	    && mpeg1->rc.np > 0 && mpeg1->rc.nb > 0) {
		switch (cpu_detection()) {
		case CPU_K6_2:
		case CPU_CYRIX_III:
			search = _3dn_search;
			break;

		case CPU_PENTIUM_4:
#if USE_SSE2
			search = sse2_search;
			break;
#endif
		case CPU_PENTIUM_III:
		case CPU_ATHLON:
			search = sse_search;
			break;

		default:
			search = mmx_search;
			break;
		}
#if REG_TEST
		motion = (mpeg1->motion_min + mpeg1->motion_max) << 7;
#else
		motion = (mpeg1->motion_min * 3 + mpeg1->motion_max * 1) * 256 / 4;
#endif
		p_inter_bias *= 2;
		b_inter_bias *= PBF;

		packed = FALSE;

		mpeg1->picture_i = picture_i_mc;
		mpeg1->picture_p = picture_p_mc;
		mpeg1->picture_b = picture_b_mc;

		mm_buf_offs = 6 * 64 * mb_num;
		mm_mbrow = calloc_aligned(mb_width * (16 * 8) * sizeof(short), 4096);

		if (!mm_mbrow) {
			rte_error_printf(codec->context, _("Out of memory for mc buffer."));
			goto reject;
		}
	} else {
		motion = 0;

		mpeg1->picture_i = picture_i_nomc;
		mpeg1->picture_p = picture_p_nomc;
		mpeg1->picture_b = picture_b_nomc;
	}

	/* Init buffers */

	mpeg1->mb_hist = calloc_aligned ((mb_width + 1) * sizeof (*mpeg1->mb_hist), 32);

	// XXX too big. Interleave.

	size = (motion ? 10 * 64 : 6 * 64) * mb_num;

	if (!(mpeg1->oldref = calloc_aligned(size, 4096))) {
		rte_error_printf(codec->context, _("Out of memory for video buffer."));
		goto reject;
	}

	if (!(newref = calloc_aligned(size, 4096))) {
		rte_error_printf(codec->context, _("Out of memory for video buffer."));
		goto reject;
	}

	binit_write(&video_out);
	bstart(&video_out, mpeg1->oldref);
	mpeg1->Sz = picture_zero(mpeg1);

	if (!(mpeg1->zerop_template =
	      calloc_aligned(mpeg1->Sz * sizeof(unsigned char), CACHE_LINE))) {
		rte_error_printf(codec->context, _("Out of memory."));
		goto reject;
	}

	memcpy(mpeg1->zerop_template, mpeg1->oldref, mpeg1->Sz);

	if (packed) {
		int i;

		for (i = 0; i < 6; i++) {
			mb_address.block[i].offset	= 64;
			mb_address.block[i].pitch	= 8;
		}

		mb_address.col.lum	= 6 * 64;
		mb_address.col.chrom	= 0;
		mb_address.row.lum	= 0;
		mb_address.row.chrom	= 0;
		mb_address.chrom_0	= mb_address.block[4].offset;

		mpeg1->predict_forward = mmx_predict_forward_packed;
		mpeg1->predict_bidirectional = mmx_predict_bidirectional_packed;
	} else {
		int i;

		for (i = 0; i < 6; i++)
			mb_address.block[i].pitch = mb_width * ((i >= 4) ? 8 : 16);

		mb_address.block[1].offset = mb_width * 16 * 8;
		mb_address.block[2].offset = 8 - mb_width * 16 * 8;
		mb_address.block[3].offset = mb_width * 16 * 8;
		mb_address.block[4].offset = mb_num * 64 * 4 - mb_width * 16 * 8 - 8;
		mb_address.block[5].offset = mb_num * 64;

		mb_address.col.lum	= 16;
		mb_address.col.chrom	= 8 - 16;
		mb_address.row.lum	= mb_width * 16 * 15;
		mb_address.row.chrom	= mb_width * (8 * 7 - 16 * 15);
		mb_address.chrom_0	= mb_address.block[4].offset;

		mpeg1->predict_forward = mmx_predict_forward_planar;
		mpeg1->predict_bidirectional = predict_bidirectional_planar; // no MMX equv
	}

	/* Misc */

	mpeg1->last.org = NULL;
	mpeg1->last.time = 0;
	mpeg1->p_succ = 0;

	mpeg1->mb_cx_row = mb_height;
	mpeg1->mb_cx_thresh = 100000;

	if (mb_height >= 10) {
		mpeg1->mb_cx_row /= 3;
		mpeg1->mb_cx_thresh =
			lroundn(mpeg1->mb_cx_row * mb_width * 0.95);
	}

	video_reset(mpeg1);

	/* I/O */

	mpeg1->codec.sstr.frame_period = 1.0 / mpeg1->nominal_frame_rate;

	mpeg1->codec.io_stack_size = video_look_ahead(mpeg1->gop_sequence);

	mpeg1->codec.output_buffer_size = mb_num * 384 * 4;
	mpeg1->codec.output_bit_rate = mpeg1->bit_rate;
	mpeg1->codec.output_frame_rate = mpeg1->coded_frame_rate;

	memset(&mpeg1->codec.status, 0, sizeof(mpeg1->codec.status));
	mpeg1->codec.status.valid = 0
		+ RTE_STATUS_FRAMES_OUT
		+ RTE_STATUS_BYTES_OUT
		+ RTE_STATUS_CODED_TIME;

	codec->state = RTE_STATE_PARAM;

	return TRUE;

reject:
	uninit(codec);

	codec->state = RTE_STATE_NEW;

	return FALSE;
}

/*
 *  NB there are four frame rates:
 *  1) The nominal input frame rate (eg. 25 Hz)
 *  2) The actual input frame rate (eg. 25.001 Hz)
 *  3) The coded output frame rate (eg. 24 Hz)
 *  4) The virtual output frame rate,
 *     after forced frame dropping (eg. 8.5 Hz)
 */

static char *
menu_skip_method[] = {
	N_("Standard compliant"),
	N_("Multiplexer"),
	N_("Fake picture"),
};

static rte_option_info
mpeg1_options[] = {
	/* FILTER omitted, will change, default for now */
	/* FRAMES_PER_SEQ_HEADER omitted, ancient legacy */
	/* FIX vcd_parameters_set when adding w/h options */
	RTE_OPTION_INT_RANGE_INITIALIZER
	  ("bit_rate", N_("Bit rate"),
	   2300000, 30000, 8000000, 1000,
	   N_("Output bit rate")),
	RTE_OPTION_REAL_MENU_INITIALIZER
	  ("coded_frame_rate", NULL,
	   2 /* 25.0 */, (double *) &frame_rate_value[1], 8, NULL),
	RTE_OPTION_REAL_RANGE_INITIALIZER
	  ("virtual_frame_rate", N_("Virtual frame rate"),
	   60.0, 0.0002, 60.0, 1e-3,
	   N_("MPEG-1 allows only a few discrete values for frames/s, "
	      "but this codec can skip frames if you wish. Choose the "
	      "output bit rate accordingly.")),
	RTE_OPTION_MENU_INITIALIZER
	  ("skip_method", N_("Virtual frame rate method"),
	   0, menu_skip_method, elements(menu_skip_method),
	   N_("The standard compliant method has one major drawback: "
	      "it may have to promote some or even all B to P pictures, "
	      "reducing the image quality.")),
	RTE_OPTION_STRING_INITIALIZER
	  ("gop_sequence", N_("GOP sequence"),
	   "IBBPBBPBBPBB", N_(GOP_RULE)),
	RTE_OPTION_BOOL_INITIALIZER
	  ("motion_compensation", N_("Motion compensation"),
	   FALSE, N_("Enable motion compensation to improve the image "
		     "quality. The motion search range is automatically "
		     "adjusted.")),
/*	RTE_OPTION_BOOL_INITIALIZER
	  ("desaturate", N_("Desaturate"), FALSE, (NULL)),
XXX
*/	RTE_OPTION_STRING_INITIALIZER
	  ("anno", N_("Annotation"), "",
	   N_("Add an annotation in the user data field shortly after "
	      "the video stream start. This is an mp1e extension, "
	      "players will ignore it.")),
	RTE_OPTION_REAL_RANGE_INITIALIZER
	  ("num_frames", NULL, DBL_MAX, 1, DBL_MAX, 1, NULL),
};

#define KEYWORD(name) strcmp(keyword, name) == 0

static char *
option_print(rte_codec *codec, const char *keyword, va_list args)
{
	char buf[80];
	rte_context *context = codec->context;

	if (KEYWORD("bit_rate")) {
	        snprintf(buf, sizeof(buf), _("%5.3f Mbit/s"), va_arg(args, int) / 1e6);
	} else if (KEYWORD("coded_frame_rate")) {
		snprintf(buf, sizeof(buf), _("%4.2f frames/s"),
			 frame_rate_value[dvec_imin(
				  &frame_rate_value[1], 8,
				  va_arg(args, double)) + 1]);
	} else if (KEYWORD("virtual_frame_rate")) {
		snprintf(buf, sizeof(buf), _("%5.3f frames/s"), va_arg(args, double));
	} else if (KEYWORD("skip_method")) {
		return rte_strdup(context, NULL, menu_skip_method[
			RTE_OPTION_ARG_MENU(menu_skip_method)]);
	} else if (KEYWORD("gop_sequence") || KEYWORD("anno")) {
		return rte_strdup(context, NULL, va_arg(args, char *));
	} else if (KEYWORD("motion_compensation") || KEYWORD("monochrome")) {
		return rte_strdup(context, NULL, va_arg(args, int) ?
				  _("on") : _("off"));
	} else if (KEYWORD("num_frames")) {
		snprintf(buf, sizeof(buf), _("%f frames"), va_arg(args, double));
	} else {
		rte_unknown_option(context, codec, keyword);
	failed:
		return NULL;
	}

	return rte_strdup(context, NULL, buf);
}

static rte_bool
option_get(rte_codec *codec, const char *keyword, rte_option_value *v)
{
	mpeg1_context *mpeg1 = PARENT(codec, mpeg1_context, codec);
	rte_context *context = codec->context;

	if (KEYWORD("bit_rate")) {
		v->num = mpeg1->bit_rate;
	} else if (KEYWORD("coded_frame_rate")) {
		v->dbl = frame_rate_value[mpeg1->frame_rate_code];
	} else if (KEYWORD("virtual_frame_rate")) {
		v->dbl = mpeg1->virtual_frame_rate;
	} else if (KEYWORD("skip_method")) {
		v->num = mpeg1->skip_method;
	} else if (KEYWORD("gop_sequence")) {
		if (!(v->str = rte_strdup(context, NULL, mpeg1->gop_sequence)))
			return FALSE;
	} else if (KEYWORD("motion_compensation")) {
		v->num = !!mpeg1->motion_compensation;
	} else if (KEYWORD("monochrome")) {
		v->num = !!mpeg1->monochrome;
	} else if (KEYWORD("anno")) {
		if (!(v->str = rte_strdup(context, NULL, mpeg1->anno)))
			return FALSE;
	} else if (KEYWORD("num_frames")) {
		v->dbl = mpeg1->num_frames;
	} else {
		rte_unknown_option(context, codec, keyword);
		return FALSE;
	}

	return TRUE;
}

static rte_bool
option_set(rte_codec *codec, const char *keyword, va_list args)
{
	mpeg1_context *mpeg1 = PARENT(codec, mpeg1_context, codec);
	rte_context *context = codec->context;

	if (0) {
		static char *option_print(rte_codec *, const char *, va_list);
		char *str = option_print(codec, keyword, args);

		printv(0, "mpeg1/option_set(%p, %s, %s)\n", mpeg1, keyword, str);
		free(str);
	}

	/* Preview runtime changes here */

	switch (codec->state) {
	case RTE_STATE_NEW:
		break;

	case RTE_STATE_PARAM:
		uninit(codec);
		break;

	case RTE_STATE_READY:
		assert(!"reached");

	default:
		rte_error_printf(codec->context, "Cannot set %s options, codec is busy.",
				 codec->class->public.keyword);
		return FALSE;
	}

	if (KEYWORD("bit_rate")) {
		mpeg1->bit_rate = RTE_OPTION_ARG(int, 30000, 8000000);
	} else if (KEYWORD("coded_frame_rate")) {
		mpeg1->frame_rate_code =
			dvec_imin(&frame_rate_value[1], 8,
				  va_arg(args, double)) + 1;
	} else if (KEYWORD("virtual_frame_rate")) {
		mpeg1->virtual_frame_rate =
			RTE_OPTION_ARG(double, 1 / 3600.0, 60.0);
	} else if (KEYWORD("skip_method")) {
		mpeg1->skip_method = RTE_OPTION_ARG_MENU(menu_skip_method);
	} else if (KEYWORD("gop_sequence")) {
		char *str;
		int i;

		if (!(str = rte_strdup(context, NULL, va_arg(args, char *))))
			return FALSE;

		for (i = 0; str[i]; i++)
			str[i] = toupper(str[i]);

		if (!gop_validation(mpeg1, str)) {
			free(str);
			return FALSE;
		}

		if (mpeg1->gop_sequence)
			free(mpeg1->gop_sequence);

		mpeg1->gop_sequence = str;
	} else if (KEYWORD("motion_compensation")) {
		mpeg1->motion_compensation = !!va_arg(args, int);
	} else if (KEYWORD("monochrome")) {
		mpeg1->monochrome = !!va_arg(args, int);
	} else if (KEYWORD("anno")) {
		if (!rte_strdup(context, &mpeg1->anno, va_arg(args, char *)))
			return FALSE;
	} else if (KEYWORD("num_frames")) {
		mpeg1->num_frames = va_arg(args, double);
	} else {
		rte_unknown_option(context, codec, keyword);
	failed:
		return FALSE;
	}

	codec->state = RTE_STATE_NEW;

	return TRUE;
}

static rte_option_info *
option_enum(rte_codec *codec, int index)
{
	/* mpeg1_context *mpeg1 = PARENT(codec, mpeg1_context, codec); */

	if (index < 0 || index >= elements(mpeg1_options))
		return NULL;

	return mpeg1_options + index;
}

static void
codec_delete(rte_codec *codec)
{
	mpeg1_context *mpeg1 = PARENT(codec, mpeg1_context, codec);

	switch (codec->state) {
	case RTE_STATE_PARAM:
		uninit(codec);
		break;

	case RTE_STATE_READY:
		assert(!"reached");
		break;

	case RTE_STATE_RUNNING:
	case RTE_STATE_PAUSED:
		fprintf(stderr, "mp1e bug warning: attempt to delete "
			"running mpeg1 codec ignored\n");
		return;

	default:
		break;
	}

	/* Options */

	if (mpeg1->gop_sequence)
		free(mpeg1->gop_sequence);

	if (mpeg1->anno)
		free(mpeg1->anno);

	pthread_mutex_destroy(&codec->mutex);

	free_aligned(mpeg1);
}

static rte_codec *
codec_new(rte_codec_class *cc, char **errstr)
{
	mpeg1_context *mpeg1;
	rte_codec *codec;

	if (!(mpeg1 = calloc_aligned(sizeof(*mpeg1), 8192))) {
		rte_asprintf(errstr, _("Out of memory."));
		return NULL;
	}

	codec = &mpeg1->codec.codec;
	codec->class = cc;

	pthread_mutex_init(&codec->mutex, NULL);

	codec->state = RTE_STATE_NEW;

	return codec;
}

rte_codec_class
mp1e_mpeg1_video_codec = {
	.public = {
		.stream_type = RTE_STREAM_VIDEO,
		.keyword = "mpeg1_video",
		.label = "MPEG-1 Video",
	},

	.new		= codec_new,
	.delete         = codec_delete,

	.option_enum	= option_enum,
	.option_get	= option_get,
	.option_set	= option_set,
	.option_print	= option_print,

	.parameters_set = parameters_set,
};

void
mp1e_mpeg1_module_init(int test)
{
}
