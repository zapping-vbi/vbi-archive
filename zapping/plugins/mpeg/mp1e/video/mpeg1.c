/*
 *  MPEG-1 Real Time Encoder
 *
 *  Copyright (C) 1999-2001 Michael H. Schimek
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

/* $Id: mpeg1.c,v 1.28 2001-05-09 22:33:21 garetxe Exp $ */

#include <assert.h>
#include <limits.h>
#include "../common/profile.h"
#include "../options.h"
#include "../common/math.h"
#include "../common/types.h"
#include "../common/log.h"
#include "../common/mmx.h"
#include "../common/bstream.h"
#include "../common/fifo.h"
#include "../common/alloc.h"
#include "../common/remote.h"
#include "vlc.h"
#include "dct.h"
#include "predict.h"
#include "mpeg.h"
#include "video.h"
#include "../systems/mpeg.h"
#include "../systems/systems.h"

/**/


enum {
	MB_INTRA,
	MB_FORWARD,
	MB_BACKWARD,
	MB_INTERP
};

#define MAX_P_SUCC 3	// Max. successive P pictures when overriding gop_sequence (error accumulation)
#define MAX_B_SUCC 31	// Max. successive B pictures we can stack up

struct bs_rec		video_out;

 
int			frame_rate_code;

int			video_frames_dropped;

int			video_frame_count;		// frames encoded (coding order)

static int		gop_frame_count,		// .. in current GOP (display order)
			seq_frame_count;		// .. since last sequence header

static double		skip_rate_acc,
			drop_timeout,
			time_per_frame,
			frames_per_sec;

static bool		insert_gop_header = FALSE;

static bool		closed_gop;			// random access point, no forward ref
static bool		referenced;			// by other P or B pictures

static int		ni, np, nb, ob;			// picture types per GOP
static int		Ei, Ep, Eb;
static int		gop_count;
static double		ei, ep, eb;
static int		G0, Gn;				// estimated target bits per GOP
static double		G4;
static int		Tavg;				// estimated avg. bits per frame
static int		Tmin;				// minimum target bits per frame
static int		R;				// remaining bits in GOP
volatile double		Ti, Tmb;

static double		Xi, Xp, Xb;			// global complexity measure
static double		d0i, d0p, d0b;			// virtual buffer fullness
static double		r31;				// reaction parameter
static double		avg_acti;			// average spatial activity
static double		avg_actp;
static int		p_succ, p_dist;

static unsigned char 	seq_header_template[32] __attribute__ ((aligned (CACHE_LINE)));
							// precompressed sequence header
static char *		banner;
static unsigned char *	zerop_template;			// precompressed empty P picture
static int		Sz;				// .. size in bytes

double video_eff_bit_rate;

static int mb_cx_row, mb_cx_thresh;

/* main.c */
extern int		frames_per_seqhdr;
extern int		video_num_frames;
extern double		video_stop_time;

#define new_inter_quant mmx_new_inter_quant
#define fdct_intra mmx_fdct_intra
#define fdct_inter mmx_fdct_inter
#define mpeg1_idct_intra mmx_mpeg1_idct_intra2
#define mpeg1_idct_inter mmx_mpeg1_idct_inter

#define mpeg1_encode_intra p6_mpeg1_encode_intra
#define mpeg1_encode_inter p6_mpeg1_encode_inter

extern int p6_predict_forward_packed(unsigned char *) reg(1);
extern int p6_predict_forward_planar(unsigned char *) reg(1);

#define PACKED 1

#if PACKED
#define predict_forward		p6_predict_forward_packed
#define predict_backward	p6_predict_forward_packed
#define predict_bidirectional	mmx_predict_bidirectional_packed
#else
#define predict_forward		p6_predict_forward_planar
#define predict_backward	p6_predict_forward_planar
#define predict_bidirectional	predict_bidirectional_planar
#endif

static const unsigned char
quant_res_intra[32] __attribute__ ((SECTION("video_tables") aligned (CACHE_LINE))) =
{
	1, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
	16, 16, 18, 18, 20, 20, 22, 22,	24, 24, 26, 26, 28, 28, 30, 30
};

extern bool		temporal_interpolation;

extern int		preview;
extern void		packed_preview(unsigned char *buffer, int mb_cols, int mb_rows);

int p_inter_bias = 65536 * 48,
    b_inter_bias = 65536 * 96,
    x_bias = 65536 * 31,
    quant_max = 31;

#define QS 1 // (PACKED)

fifo *			video_fifo;

#include "dct_ieee.h"

/*
 *  Picture layer
 */

#define B_SHARE 1.4

#define macroblock_address(skipped)					\
do {									\
	/* this isn't really justified, better limit mb_skipped */	\
	for (i = (skipped); i >= 33; i -= 33)				\
		bputl(&video_out, 0x008, 11); /* mb addr escape */	\
									\
	bstartq(macroblock_address_increment[i].code);			\
	length = macroblock_address_increment[i].length; /* 1...11 */	\
} while (0)

#define motion_vector(m, dmv)						\
do {									\
	int l0, l1;							\
									\
	l0 = (m)->vlc[dmv[0]].length;					\
	bcatq((m)->vlc[dmv[0]].code, l0);				\
	l1 = (m)->vlc[dmv[1]].length;					\
	bcatq((m)->vlc[dmv[1]].code, l1);				\
	length += l0 + l1; /* 2...26 (f_code == 3) */			\
} while (0)

static void
motion_dmv(struct motion *m, int dmv[2])
{
	dmv[0] = (m->MV[0] - m->PMV[0]) & m->f_mask;
	dmv[1] = (m->MV[1] - m->PMV[1]) & m->f_mask;

	m->PMV[0] = m->MV[0];
	m->PMV[1] = m->MV[1];
}

#if PACKED
#define T3RT 1
#define T3RI 1
#define PSKIP 0
static const int motion = 0;
const int mm_row; // ld dead end
const int mm_mbrow;
#define zero_forward_motion()
#else
static int motion = 8 * 256;
#include "motion.c"
#endif

static int
picture_i(unsigned char *org0, unsigned char *org1)
{
	double act, act_sum;
	int quant_sum;
	int S, T, prev_quant, quant;
	struct bs_rec mark;
	int var;
	bool slice;

	printv(3, "Encoding I picture #%d GOP #%d, ref=%c\n",
		video_frame_count, gop_frame_count, "FT"[referenced]);

	pr_start(21, "Picture I");

	/* Initialize rate control parameters */

	T = lroundn(R / ((ni + ei) + ((np + ep) * Xp + (nb + eb) * Xb / B_SHARE) / Xi));
	/*
	 *  T = lroundn(R / (+ (ni) * Xi / (Xi * 1.0)
	 *		     + (np) * Xp / (Xi * 1.0)
	 *		     + (nb) * Xb / (Xi * 1.4)));
	 */
 
	if (T < Tmin)
		T = Tmin;

	Tmb = T / mb_num;
	Ti = -d0i;

	quant_sum = 0;
	act_sum = 0.0;

	swap(oldref, newref);
#if TEST3p1
	t0();
#endif

	reset_mba();
	reset_dct_pred();

	slice = FALSE;
	prev_quant = -100;

	/* Picture header */

	bepilog(&video_out);

	bputl(&video_out, PICTURE_START_CODE, 32);
	bputl(&video_out, ((gop_frame_count & 1023) << 22) + (I_TYPE << 19) + (0 << 2), 32);
	/*
	 *  temporal_reference [10], picture_coding_type [3], vbv_delay [16];
	 *  extra_bit_picture '0', byte align '00'
	 */

	bprolog(&video_out);

	for (mb_row = 0; mb_row < mb_height; mb_row++) {
		for (mb_col = 0; mb_col < mb_width; mb_col++) {

			/* Read macroblock (MMX state) */

			pr_start(41, "Filter");
			var = (*filter)(org0, org1); // -> mblock[0]
			pr_end(41);
#if TEST3p1
			t1();
#endif
			emms();

			/* Calculate quantization factor */

			act_sum += act = var / 65536.0 + 1;
			act = (2.0 * act + avg_acti) / (act + 2.0 * avg_acti);
			quant = saturate(lroundn((bwritten(&video_out) - Ti) * r31 * act), 1, quant_max);
			quant = quant_res_intra[quant];
			/*
			 *  quant_res_intra halves the quantization factor resolution above 16 to
			 *  reduce lookup table size (MMX DCT). Quality is already poor at 16,
			 *  so it won't hurt very much.
	                 */

			Ti += Tmb;

			if (quant >= 4 && quant > prev_quant &&
			    nbabs(quant - prev_quant) <= 2)
				quant = prev_quant;

			/* Encode macroblock */

			brewind(&mark, &video_out);

			for (;;) {
				pr_start(22, "FDCT intra");
				fdct_intra(quant); // mblock[0] -> mblock[1]
				pr_end(22);

				bepilog(&video_out);

				if (!slice) {
					bstartq(SLICE_START_CODE);
					bcatq((quant << 3) + 0x3, 8);
					bputq(&video_out, 40);
					/*
					 *  slice header: quantiser_scale_code 'xxxxx', extra_bit_slice '0';
					 *  macroblock_address_increment '1', macroblock_type '1' (I Intra)
					 */
				} else if (prev_quant != quant) {
					bputl(&video_out, 0xA0 + quant, 8);
					/*
					 *  macroblock_address_increment '1', macroblock_type '01' (I Intra, Quant),
					 *  quantiser_scale_code 'xxxxx'
					 */
				} else
					bputl(&video_out, 0x3, 2);
					/* macroblock_address_increment '1', macroblock_type '1' (I Intra) */

				pr_start(44, "Encode intra");

				if (!mpeg1_encode_intra()) { // mblock[1]
					pr_end(44);
					break;
				}

				pr_end(44);

				quant++;
				brewind(&video_out, &mark);

				pr_event(42, "I/intra overflow");
			}

			bprolog(&video_out);

			slice = TRUE;
			quant_sum += quant;
			prev_quant = quant;

			if (referenced) {
				pr_start(23, "IDCT intra");
				mpeg1_idct_intra(quant);	// mblock[1] -> newref
				pr_end(23);

				zero_forward_motion();
				mba_col();
			}
#if TEST_PREVIEW
			if (preview > 1) {
				emms();
				packed_preview(newref, mb_width, mb_height);
			}
#endif
		}

		mba_row();
	}

	emms();

#if TEST3p1
	t2();
#endif

	/* Rate control */

	S = bflush(&video_out);

	avg_acti = act_sum / mb_num;

	Xi = lroundn(S * (double) quant_sum / mb_num);

	d0i += S - T; // bits encoded - estimated bits

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
		bepilog(&video_out);					\
									\
		if (f) {						\
			macroblock_address(mb_skipped);			\
									\
			if (prev_quant != quant && prev_quant >= 0) {	\
			    	bcatq(0x020 + quant, 11);		\
				bputq(&video_out, length + 11);		\
				/* macroblock_type '0000 01' */		\
				/* (P/B Intra, Quant), */		\
				/* quantiser_scale_code 'xxxxx' */	\
			} else	{					\
				bcatq(0x3, 5);				\
				bputq(&video_out, length + 5);		\
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
		if (!mpeg1_encode_intra()) { /* mblock[1] */		\
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

#define HOT_AIR 0

static int
picture_p(unsigned char *org0, unsigned char *org1, int dist, int forward_motion)
{
	double act, act_sumi, act_sump;
	int quant_sum;
	int S, T, quant, prev_quant, quant1 = 1;
	struct bs_rec mark;
	struct motion M;
	unsigned char *q1p;
	int var, vmc;
	int mb_skipped, mb_count;
	int intra_count = 0;

	if (motion)
		motion_init(&M, (dist * forward_motion) >> 8);
	else {
		M.f_code = 1;
		M.src_range = 0;
	}

	printv(3, "Encoding P picture #%d GOP #%d, ref=%c, f_code=%d (%d)\n",
		video_frame_count, gop_frame_count, "FT"[referenced],
		M.f_code, M.src_range);

	pr_start(24, "Picture P");

	/* Initialize rate control parameters */

	swap(oldref, newref);
#if TEST3p1
	t0();
#endif
	reset_mba();

#if TEST_PREVIEW
	if (preview > 1)
		memcpy(newref, oldref, 64 * 6 * mb_num);
#endif

	T = lroundn(R / ((np + ep) + ((ni + ei) * Xi + (nb + eb) * Xb / B_SHARE) / Xp));

	Ti = -d0p;

	if (T < Tmin)
		T = Tmin;

	Tmb = T / mb_num;

	quant_sum = 0;
	act_sump = 0.0;
	act_sumi = 0.0;

	reset_dct_pred();
	

	prev_quant = -100;
	mb_count = 1;
	mb_skipped = 0;

	/* Picture header */

	bepilog(&video_out);

	bputl(&video_out, PICTURE_START_CODE, 32);

	bputl(&video_out, ((gop_frame_count & 1023) << 19) + (P_TYPE << 16) + 0, 29);
	bputl(&video_out, (0 << 10) + (M.f_code << 7) + (0 << 6), 11);
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

	for (mb_row = 0; mb_row < mb_height; mb_row++) {
		if (1 && mb_row == mb_cx_row &&
		    intra_count >= mb_cx_thresh) {
			emms();
			swap(oldref, newref);
			pr_event(43, "P/cx trap");
			return 0;
		}

		for (mb_col = 0; mb_col < mb_width; mb_col++) {

			/* Read macroblock (MMX state) */

			pr_start(41, "Filter");
			var = (*filter)(org0, org1); // -> mblock[0]
			pr_end(41);
#if TEST3p1
			t1();
#endif
			pr_start(51, "Predict forward");

#if TEST3p1
			if (motion)
				vmc = predict_forward_motion(&M, oldref, dist);
			else
#endif
				vmc = predict_forward(oldref + mb_address.block[0].offset);

			pr_end(51);

			emms();

			act_sumi += act = var / 65536.0 + 1;

			/* Encode macroblock */

			if (T3RI && vmc > p_inter_bias) {
//			if (vmc > var || vmc > p_inter_bias) {
				int length, i;

				/* Calculate quantization factor */

				act_sump += act;
				act = (2.0 * act + avg_actp) / (act + 2.0 * avg_actp);

				quant = lroundn((bwritten(&video_out) - Ti) * r31 * act);
				quant = saturate(quant >> QS, 1, quant_max);
				quant = quant_res_intra[quant];

//				if (quant >= 4 && abs(quant - prev_quant) <= 2)
//					quant = prev_quant;

				Ti += Tmb;

				intra_count++;

				pb_intra_mb(TRUE);

				if (prev_quant < 0)
					quant1 = quant;

				prev_quant = quant;
				quant_sum += quant;

				mb_skipped = 0;

				if (motion)
					reset_pmv(&M);

				if (referenced) {
					pr_start(23, "IDCT intra");
					mpeg1_idct_intra(quant); // mblock[0] -> new
					pr_end(23);

					zero_forward_motion();
				}
			} else {
				unsigned int cbp;
				int dmv[2], len, length, i;

				/* Calculate quantization factor */

				act_sump += act = vmc / 65536.0 + 1;
				act = (2.0 * act + avg_actp) / (act + 2.0 * avg_actp);

				quant = saturate(lroundn((bwritten(&video_out) - Ti) * r31 * act), 1, quant_max);

if (!T3RT) quant = 2;

//				if (quant >= 4 && abs(quant - prev_quant) <= 2)
//					quant = prev_quant;

				Ti += Tmb;

				if (motion) {
#if HOT_AIR
 					if (mb_row > 0 && mb_row < (mb_height - 1) &&
 					    mb_col > 0 && mb_col < (mb_width - 1)) {
 						M.MV[0] = (rand() & 3) - 2;
 						M.MV[1] = (rand() & 3) - 2;
 					} else
 						memset(M.MV, 0, sizeof M.MV);
#endif
					motion_dmv(&M, dmv);
				}

				brewind(&mark, &video_out);

				for (;;) {
					i = mb_skipped;

					new_inter_quant(quant);

					pr_start(26, "FDCT inter");
					cbp = fdct_inter(mblock[1]); // mblock[1] -> mblock[0]
					pr_end(26);

					if (cbp == 0 && mb_count > 1 && mb_count < mb_num &&
						(!motion || (M.MV[0] | M.MV[1]) == 0)) {
						mmx_copy_refblock();
						i++;
						break;
					} else {
						bepilog(&video_out);

						macroblock_address(i); // 1..11

						i = 0;

						if (cbp == 0) {
							if (motion) {
								bcatq(1, 3);
								motion_vector(&M, dmv);
								bputq(&video_out, length + 3);
								/* macroblock_type '001' (P MC, Not Coded) */
							} else {
								bcatq(7, 5);
								bputq(&video_out, length + 5);
								/* macroblock_type '001' (P MC, Not Coded), '11' MV(0, 0) */
							}

							bprolog(&video_out);
							mmx_copy_refblock();
							break;
						} else {
							if (motion && (M.MV[0] | M.MV[1])) {
								if (prev_quant != quant) {
									bcatq((2 << 5) + quant, 10);
									length += 10;
									/* macroblock_type '0001 0' (P MC, Coded, Quant), quantiser_scale_code 'xxxxx' */
								} else {
									bcatq(1, 1);
									length += 1;
									/* macroblock_type '1' (P MC, Coded) */
								}

								motion_vector(&M, dmv);
								len = coded_block_pattern[cbp].length; // 3..9
								bcatq(coded_block_pattern[cbp].code, len);
								bputq(&video_out, length + len);
							} else {
								if (prev_quant != quant) {
									bcatq((1 << 5) + quant, 10);
									length += 10;
									/* macroblock_type '0000 1' (P No MC, Coded, Quant), quantiser_scale_code 'xxxxx' */
								} else {
									bcatq(1, 2);
									length += 2;
									/* macroblock_type '01' (P No MC, Coded) */
								}

								len = coded_block_pattern[cbp].length; // 3..9
								bcatq(coded_block_pattern[cbp].code, len);
								bputq(&video_out, length + len);
							}

							pr_start(46, "Encode inter");

							if (!mpeg1_encode_inter(mblock[0], cbp)) {
								pr_end(46);

								bprolog(&video_out);

								if (prev_quant < 0)
									quant1 = quant;
								prev_quant = quant;
				
								if (referenced) {
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

				quant_sum += quant;
				mb_skipped = i;

				reset_dct_pred();
			}

			mba_col();
			mb_count++;
#if TEST_PREVIEW
			if (preview > 1) {
				emms();
				packed_preview(newref, mb_width, mb_height);
			}
#endif
		}

		mba_row();
	}

	emms();

#if TEST3p1
	t2();
	t7(M.src_range, dist);
#endif

	/* Rate control */

	S = bflush(&video_out);
	
	*q1p |= (quant1 << 3);

	avg_actp = act_sump / mb_num;
	avg_acti = act_sumi / mb_num;

	Xp = lroundn(S * (double) quant_sum / mb_num);

	d0p += S - T;

	pr_end(24);

#if TEST_PREVIEW
	if (preview == 1)
		packed_preview(newref, mb_width, mb_height);
#endif

	return S >> 3;
}

static int
picture_b(unsigned char *org0, unsigned char *org1, int dist,
	  int forward_motion, int backward_motion)
{
	double act, act_sum;
	short (* iblock)[6][8][8];
	int quant_sum;
	int S, T, quant, prev_quant, quant1 = 1;
	struct bs_rec mark;
	unsigned char *q1p;
	int var, vmc, vmcf, vmcb;
	int macroblock_type, mb_type_last;
	int mb_skipped, mb_count;
	struct motion M[2];

	if (motion) {
		motion_init(&M[0], forward_motion >> 8);
		motion_init(&M[1], backward_motion >> 8);
	} else {
		M[0].f_code = 1; M[1].f_code = 1;
		M[0].src_range = 0; M[1].src_range = 0;
	}

	printv(3, "Encoding B picture #%d GOP #%d, fwd=%c, f_code=%d (%d), %d (%d)\n",
		video_frame_count, gop_frame_count, "FT"[!closed_gop],
		M[0].f_code, M[0].src_range, M[1].f_code, M[1].src_range);

	pr_start(25, "Picture B");

	/* Initialize rate control parameters */

	reset_mba();

	T = lroundn(R / (((ni + ei) * Xi + (np + ep) * Xp) * B_SHARE / Xb + (nb + eb)));
	/*
	 *  T = lroundn(R / (+ (ni + ei) * Xi * 1.4 / Xb
	 *		     + (np + ep) * Xp * 1.4 / Xb
	 *		     + (nb + eb) * Xb / Xb));
	 */

	Ti = -d0b;

	if (T < Tmin)
		T = Tmin;

	Tmb = T / mb_num;

	quant_sum = 0;
	act_sum = 0.0;

	reset_dct_pred();


	prev_quant = -100;

	mb_count = 1;
	mb_skipped = 0;
	mb_type_last = -1;

	/* Picture header */

	bepilog(&video_out);

	bputl(&video_out, PICTURE_START_CODE, 32);

	bputl(&video_out, ((gop_frame_count & 1023) << 19) + (B_TYPE << 16) + 0, 29);
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

	for (mb_row = 0; mb_row < mb_height; mb_row++) {
		for (mb_col = 0; mb_col < mb_width; mb_col++) {

			/* Read macroblock (MMX state) */

			pr_start(41, "Filter");
			var = (*filter)(org0, org1); // -> mblock[0]
			pr_end(41);

			/* Choose prediction type */

			if (!closed_gop) {
				pr_start(52, "Predict bidirectional");
#if TEST3p1
				if (motion)
					vmc = predict_bidirectional_motion(M, oldref, newref,
						&vmcf, &vmcb, dist);
				else
#endif
					vmc = predict_bidirectional(
						oldref + mb_address.block[0].offset,
						newref + mb_address.block[0].offset,
						&vmcf, &vmcb);
				pr_end(52);

#define TEST3 0
if (TEST3) {
	int i, n, s, s2, j = 4 * 64;

	for (i = s = s2 = 0; i < j; i++) {
		s += n = mblock[0][0][0][i];
		s2 += n * n;
	} var = s2 * j - (s * s);
	for (i = s = s2 = 0; i < j; i++) {
		s += n = mblock[1][0][0][i];
		s2 += n * n;
	} vmcf = s2 * j - (s * s);
	for (i = s = s2 = 0; i < j; i++) {
		s += n = mblock[2][0][0][i];
		s2 += n * n;
	} vmcb = s2 * j - (s * s);
	for (i = s = s2 = 0; i < j; i++) {
		s += n = mblock[3][0][0][i];
		s2 += n * n;
	} vmc = s2 * j - (s * s);
}
				macroblock_type = MB_INTERP;
				iblock = &mblock[3];

				if (vmcf <= vmcb && vmcf <= vmc) {
					vmc = vmcf;
					macroblock_type = MB_FORWARD;
					iblock = &mblock[1];
				} else if (vmcb <= vmc) {
					vmc = vmcb;
					macroblock_type = MB_BACKWARD;
					iblock = &mblock[2];
				}
			} else {
				vmc = predict_backward(newref + mb_address.block[0].offset);
				macroblock_type = MB_BACKWARD;
				iblock = &mblock[1];
			}

			emms();

#if !T3P1_25 // ATTN vmc change
if (!TEST3)
			vmc <<= 8;
#endif
			/* Encode macroblock */

			if (T3RI && vmc > b_inter_bias) {
//			if (vmc > var || vmc > b_inter_bias) {
				int length;
				int i;

				/* Calculate quantization factor */

				act_sum += act = var / 65536.0 + 1;
				act = (2.0 * act + avg_actp) / (act + 2.0 * avg_actp);

				quant = lroundn((bwritten(&video_out) - Ti) * r31 * act);
				quant = saturate(quant >> QS, 1, quant_max);
				quant = quant_res_intra[quant];

				Ti += Tmb;

				macroblock_type = MB_INTRA;

				pb_intra_mb(TRUE);

				if (prev_quant < 0)
					quant1 = quant;

				prev_quant = quant;
				quant_sum += quant;

				mb_skipped = 0;

				if (motion) {
					reset_pmv(&M[0]);
					reset_pmv(&M[1]);
				}
			} else {
				unsigned int cbp;
				int dmv[2][2], len, length, i;

				/* Calculate quantization factor */

				act_sum += act = vmc / 65536.0 + 1;
				act = (2.0 * act + avg_actp) / (act + 2.0 * avg_actp);

    quant = saturate(lroundn((bwritten(&video_out) - Ti) * r31 * act), 1, quant_max);
if (!T3RT) quant = 2;

				Ti += Tmb;

				if (motion) {
#if HOT_AIR				
					if (mb_row > 0 && mb_row < (mb_height - 1) &&
					    mb_col > 0 && mb_col < (mb_width - 1)) {
						M[0].MV[0] = (rand() & 3) - 2;
						M[0].MV[1] = (rand() & 3) - 2;
						M[1].MV[0] = (rand() & 3) - 2;
						M[1].MV[1] = (rand() & 3) - 2;
					} else {
						memset(M[0].MV, 0, sizeof M[0].MV);
						memset(M[1].MV, 0, sizeof M[1].MV);
					}
#endif
					if (macroblock_type & MB_FORWARD)
						motion_dmv(&M[0], dmv[0]);

					if (macroblock_type & MB_BACKWARD)
						motion_dmv(&M[1], dmv[1]);
				}

				brewind(&mark, &video_out);

				for (;;) {
	i = mb_skipped;

	new_inter_quant(quant);

	pr_start(26, "FDCT inter");
	cbp = fdct_inter(*iblock); // mblock[1|2|3] -> mblock[0]
	pr_end(26);

	if (cbp == 0
	    && macroblock_type == mb_type_last
	    /* && not first of slice */
	    && mb_count < mb_num
	    && (!motion || (dmv[0][0] | dmv[0][1] | dmv[1][0] | dmv[1][1]) == 0)) {
		/* not reset PMV here */
		i++;
		break;
	} else {
		bepilog(&video_out);

		macroblock_address(i); // 1..11

		i = 0;

		if (cbp == 0) {
			if (motion) {
				len = 5 - macroblock_type;
				bcatq(2, len);
				/* macroblock_type (B Not Coded, see vlc.c) */

				if (macroblock_type & MB_FORWARD)
					motion_vector(&M[0], dmv[0]);
				if (macroblock_type & MB_BACKWARD)
					motion_vector(&M[1], dmv[1]);
			} else {
				len = macroblock_type_b_nomc_notc[macroblock_type].length; // 5..6
				bcatq(macroblock_type_b_nomc_notc[macroblock_type].code, len);
			}

			bputq(&video_out, length + len);
			bprolog(&video_out);

			break;
		} else {
			if (motion) {
				if (prev_quant != quant) {
					len = macroblock_type_b_quant[macroblock_type].length;
					bcatq(macroblock_type_b_quant[macroblock_type].code
						+ quant, len);
				} else {
					len = 5 - macroblock_type;
					bcatq(3, len);
					/* macroblock_type (B Coded, see vlc.c) */
				}

				length += len;

				if (macroblock_type & MB_FORWARD)
					motion_vector(&M[0], dmv[0]);
				if (macroblock_type & MB_BACKWARD)
					motion_vector(&M[1], dmv[1]);
			} else {
				if (prev_quant != quant) {
					len = macroblock_type_b_nomc_quant[macroblock_type].length; // 13..14
					bcatq(macroblock_type_b_nomc_quant[macroblock_type].code
						+ (quant << macroblock_type_b_nomc_quant[macroblock_type].mv_length), len);
				} else {
					len = macroblock_type_b_nomc[macroblock_type].length; // 5..7
					bcatq(macroblock_type_b_nomc[macroblock_type].code, len);
				}

				length += len;
			}

			bcatq(coded_block_pattern[cbp].code,
			      len = coded_block_pattern[cbp].length);

			bputq(&video_out, length + len);

			pr_start(46, "Encode inter");

			if (!mpeg1_encode_inter(mblock[0], cbp)) {
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

				quant_sum += quant;
				mb_skipped = i;

				reset_dct_pred();
			}

			mba_col();

			mb_count++;
			mb_type_last = macroblock_type;
		}

		mba_row();
	}

	emms();

#if TEST3p1
	t8();
#endif

	/* Rate control */

	S = bflush(&video_out);

	*q1p |= (quant1 << 3);

	avg_actp = act_sum / mb_num;

	Xb = lroundn(S * (double) quant_sum / mb_num);

	d0b += S - T;

	pr_end(25);

	return S >> 3;
}

static int
picture_zero(void)
{
	int i;
	int length;

	printv(3, "Encoding 0 picture\n");

	bepilog(&video_out);

	bputl(&video_out, PICTURE_START_CODE, 32);
	
	bputl(&video_out, ((gop_frame_count & 1023) << 19) + (P_TYPE << 16) + 0, 29);
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

	macroblock_address(mb_num - 2);

	bcatq(0x07, 5);
	bputq(&video_out, length + 5);
	/* macroblock_type '001' (P MC, Not Coded), '11' MV(0, 0) */

	bprolog(&video_out);
	emms();

	return bflush(&video_out) >> 3;
}

/*
 *  Sequence layer
 */

static int
sequence_header(void)
{
	int aspect, bit_rate;

	printv(3, "Encoding sequence header\n");

	switch (frame_rate_code) {
	case 1: // square pixels 23.97 Hz
	case 2: // square pixels 24 Hz
		aspect = 1;
		break;

	case 3: // CCIR 601, 625 line 25 Hz
	case 6: // CCIR 601, 625 line 50 Hz
	// XXX check actual SAR
		aspect = 8;
		break;

	case 4: // CCIR 601, 525 line 29.97 Hz
	case 5: // CCIR 601, 525 line 30 Hz
	case 7: // CCIR 601, 525 line 59.94 Hz
		aspect = 9;
		break;

	default:
		FAIL("Invalid frame_rate_code %d", frame_rate_code);
	}

	bit_rate = ceil(video_bit_rate / 400.0);

	bepilog(&video_out);

	bputl(&video_out, SEQUENCE_HEADER_CODE, 32);

	bputl(&video_out, width & 0xFFF, 12);		// horizontal_size_value
	bputl(&video_out, height & 0xFFF, 12);		// vertical_size_value

	bputl(&video_out, aspect, 4);			// aspect_ratio_information
	bputl(&video_out, frame_rate_code, 4);		// frame_rate_code
	bputl(&video_out, bit_rate & 0x3FFFF, 18);	// bit_rate_value
	bputl(&video_out, 1, 1);			// marker_bit
	bputl(&video_out, 0 & 0x3FF, 10);		// vbv_buffer_size_value
	bputl(&video_out, 0, 1);			// constrained_parameters_flag
	bputl(&video_out, 0, 1);			// load_intra_quantizer_matrix
	bputl(&video_out, 0, 1);			// load_non_intra_quantizer_matrix

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





#define Rvbr (1.0 / 64)

static inline void
_send_full_buffer(fifo *f, buffer *b)
{
	video_eff_bit_rate +=
		((b->used * 8) * frames_per_sec
		 - video_eff_bit_rate) * Rvbr;

	send_full_buffer(f, b);
}


static struct {
	unsigned char * org[2];
	buffer *	buffer;
	double		time;
} stack[MAX_B_SUCC], last, buddy, *this;

static void
promote(int n)
{
	int i;
	buffer *obuf;

	for (i = 0; i < n; i++) {
		printv(3, "Promoting stacked B picture #%d\n", i);

		obuf = wait_empty_buffer(video_fifo);
		referenced = TRUE;
		Ep++; Eb--;

		obuf->used = 0;

		if (p_succ < MAX_P_SUCC) {
			bstart(&video_out, obuf->data);
			if ((obuf->used = picture_p(stack[i].org[0], stack[i].org[1], 1, motion))) {
				obuf->type = P_TYPE;
				p_succ++;
				p_dist = 0;
			}
		}

		if (!obuf->used) {
			Ei++; Ep--;
			bstart(&video_out, obuf->data);
			obuf->used = picture_i(stack[i].org[0], stack[i].org[1]);
			obuf->type = I_TYPE;
			p_succ = 0;
			p_dist = 0;
		}

		if (stack[i].buffer)
			send_empty_buffer(video_cap_fifo, stack[i].buffer);

		obuf->offset = 1;
		obuf->time = stack[i].time;

		_send_full_buffer(video_fifo, obuf);

		video_frame_count++;
		seq_frame_count++;
		gop_frame_count++;
	}
}

static void
resume(int n)
{
	int i;
	buffer *obuf, *last = NULL;

	referenced = FALSE;
	p_succ = 0;
	p_dist = 0;

	for (i = 0; i < n; i++) {
		if (!stack[i].org[0]) {
			assert(last != NULL);

			obuf = wait_empty_buffer(video_fifo);

			printv(3, "Dupl'ing B picture #%d GOP #%d\n",
				video_frame_count, gop_frame_count);

			memcpy(obuf->data, last->data, last->used);
			
			((unsigned int *) obuf->data)[1] =
				swab32((swab32(((unsigned int *) obuf->data)[1]) & ~(1023 << 22)) |
					((gop_frame_count & 1023) << 22));

			obuf->type = B_TYPE;
			obuf->offset = 0;
			obuf->used = last->used;
			obuf->time = stack[i].time;

			_send_full_buffer(video_fifo, last);
		} else {
			obuf = wait_empty_buffer(video_fifo);
			bstart(&video_out, obuf->data);
			obuf->used = picture_b(stack[i].org[0], stack[i].org[1],
				i + 1, (i + 1) * motion, (n - i) * motion);

			if (stack[i].buffer)
				send_empty_buffer(video_cap_fifo, stack[i].buffer);

			obuf->type = B_TYPE;
			obuf->offset = 0;
			obuf->time = stack[i].time;

			if (last)
				_send_full_buffer(video_fifo, last);
		}

		last = obuf;

		video_frame_count++;
		seq_frame_count++;
		gop_frame_count++;
	}

	if (last)
		_send_full_buffer(video_fifo, last);
}

char video_do_reset = FALSE;
int force_drop_rate = 0;

void *
mpeg1_video_ipb(void *unused)
{
	bool done = FALSE;
	char *seq = "";
	buffer *obuf;

	printv(3, "Video compression thread\n");

	remote_sync(video_cap_fifo, MOD_VIDEO, time_per_frame);

	while (!done) {
		int sp = 0;

		for (;;) {
			this = stack + sp;

			if (last.org[0]) {
				// Recover post dropped frame
				*this = last;
				last.org[0] = NULL;
			} else {
				if (temporal_interpolation) {
					buffer *b;

					this->buffer = buddy.buffer;
					this->org[1] = buddy.org[0];

					if ((b = wait_full_buffer(video_cap_fifo))) {
						this->time = b->time;
						this->org[0] = b->data;

						if (b->data) {
							buddy.buffer = b;
							buddy.org[0] = b->data;
						}
					} else
						this->org[0] = NULL;

					if (!this->org[1]) {
						this->buffer = NULL;
						this->org[1] = this->org[0];
					}
				} else {
					buffer *b;

					this->buffer = b = wait_full_buffer(video_cap_fifo);

					if (b) {
						this->time = b->time;
						this->org[0] = b->data;
					} else
						this->org[0] = NULL;
				}
#if TEST_PREVIEW
				if (this->org[0] && (rand() % 100) < force_drop_rate) {
					printv(3, "Forced drop #%d\n", video_frame_count + sp);

					if (this->buffer)
						send_empty_buffer(video_cap_fifo, this->buffer);

					continue;
				}
#endif
			}

			sp++;

			if (!this->org[0] || remote_break(this->time, time_per_frame) ||
			    video_frame_count + sp > video_num_frames) {
				printv(2, "Video: End of file\n");

				if (this->org[0] && this->buffer)
					send_empty_buffer(video_cap_fifo, this->buffer);

				if (buddy.org[0])
					send_empty_buffer(video_cap_fifo, buddy.buffer);

				while (*seq == 'B')
					seq++;

				done = TRUE;
				this--;
				sp--;

				if (sp <= 0)
					goto finish;

				goto next_frame; // finish with B*[PI]
			}

			skip_rate_acc += frame_rate;

			if (last.time && last.time + drop_timeout < this->time) {
				// video_frames_dropped++;

				last = *this;

				this->org[0] = NULL;
				this->time = last.time + time_per_frame;

				if (skip_rate_acc >= frames_per_sec) {
					video_frames_dropped++;
					
					if (sp >= 2 && *seq == 'B') { // Have BB, duplicate last B
				    		goto next_frame;
					} else // skip this
						skip_rate_acc = 0;
				} // else we skip it anyway
			} else
				last.time = this->time;

			if (skip_rate_acc < frames_per_sec) {
				if (this->org[0] && this->buffer)
					send_empty_buffer(video_cap_fifo, this->buffer);

				assert(sp == 1 || gop_frame_count > 0);

				promote(sp - 1);

				assert(gop_frame_count > 0);

				printv(3, "Encoding 0 picture #%d GOP #%d\n",
					video_frame_count, gop_frame_count);

				obuf = wait_empty_buffer(video_fifo);

				memcpy(obuf->data, zerop_template, Sz);

				((unsigned int *) obuf->data)[1] =
					swab32((swab32(((unsigned int *) obuf->data)[1]) & ~(1023 << 22)) |
						((gop_frame_count & 1023) << 22));

				obuf->type = P_TYPE;
				obuf->offset = 1;
				obuf->used = Sz;
				obuf->time = this->time;

				_send_full_buffer(video_fifo, obuf);

				video_frame_count++;
				gop_frame_count++;
				p_dist++;

				sp = 0;
				continue;
			}
next_frame:
			skip_rate_acc -= frames_per_sec;

			if (*seq != 'B')
				break;

			// B picture follows, stack it up

			seq++;
		}

		/* Encode P or I picture plus sequence or GOP headers */

		obuf = wait_empty_buffer(video_fifo);
		bstart(&video_out, obuf->data);

		if (!*seq) {
			/* End of GOP sequence */
#if TEST_PREVIEW
if (video_do_reset) {
	static void video_reset(void);

	video_reset();
	video_do_reset = FALSE;

	bstart(&video_out, obuf->data);
}
#endif

			bepilog(&video_out);

			/* Sequence header */

			if (seq_frame_count >= frames_per_seqhdr) {
				printv(3, "[Sequence header]\n");

				memcpy(video_out.p, seq_header_template, 16);
				video_out.p += 16 * 8 / 64;

				if (video_frame_count == 0 && banner)
					user_data(banner);

				seq_frame_count = 0;
				closed_gop = TRUE;
			}

			/* GOP header */

			if (insert_gop_header) {
				if (sp == 1)
					closed_gop = TRUE;

				printv(3, "[GOP header, closed=%c]\n", "FT"[closed_gop]);

				bputl(&video_out, GROUP_START_CODE, 32);
				bputl(&video_out, (closed_gop << 19) + (0 << 6), 32);
				/*
				 *  time_code [25 w/marker_bit] omitted, closed_gop,
				 *  broken_link '0', byte align [5]
				 */

				gop_frame_count = 0;
			}

			bprolog(&video_out);
			emms();

			seq = gop_sequence;

// XXX
if (gop_count > 0) {
ei = Ei / gop_count;
ep = Ep / gop_count;
eb = Eb / gop_count;
}
gop_count++;
//printv(0, "Eit=%f Ept=%f Ebt=%f \n", ei, ep, eb);

			printv(4, "Rewind sequence R=%d\n", R);

			G0 = Gn;
			ob = 0;
		}

		/* Encode current or forward P or I picture */

		gop_frame_count += sp - 1;

		referenced = (seq[1] == 'P') || (seq[1] == 'B') || (sp > 1) || preview;

		obuf->used = 0;

		if (*seq++ == 'P' && p_succ < MAX_P_SUCC) {
			struct bs_rec mark;

			brewind(&mark, &video_out);

			if ((obuf->used = picture_p(this->org[0], this->org[1], sp + p_dist, motion))) {
				obuf->type = P_TYPE;
				p_succ++;
				p_dist = 0;
			} else {
				brewind(&video_out, &mark); // headers

				obuf->used = picture_i(this->org[0], this->org[1]);
				obuf->type = I_TYPE;
				p_succ = 0;
				p_dist = 0;
			}
		} else {
			obuf->used = picture_i(this->org[0], this->org[1]);
			obuf->type = I_TYPE;
			p_succ = 0;
		}

		if (this->buffer)
			send_empty_buffer(video_cap_fifo, this->buffer);

		obuf->offset = sp;
		obuf->time = this->time;

		_send_full_buffer(video_fifo, obuf);

		video_frame_count++;
		seq_frame_count++;

		/* Encode stacked B pictures */

		gop_frame_count -= sp - 1;

		resume(sp - 1);

		sp = 0;
		gop_frame_count++;

		closed_gop = FALSE;
	}

finish:
	if (video_frame_count > 0) {
		obuf = wait_empty_buffer(video_fifo);
		((unsigned int *) obuf->data)[0] = swab32(SEQUENCE_END_CODE);
		obuf->type = 0;
		obuf->offset = 1;
		obuf->used = 4;
		obuf->time = last.time += time_per_frame;
		_send_full_buffer(video_fifo, obuf);
	}

	{
		extern volatile int mux_thread_done;
		
		while (!mux_thread_done) {
			obuf = wait_empty_buffer(video_fifo);
			obuf->type = 0;
			obuf->offset = 1;
			obuf->used = 0; // EOF mark
			obuf->time = last.time += time_per_frame;
			_send_full_buffer(video_fifo, obuf);
		}
	}

	return NULL;
}

static void
video_reset(void)
{
	double x;

	seq_frame_count = frames_per_seqhdr;
	gop_frame_count = 0;

	frames_per_sec = frame_rate_value[frame_rate_code];
	skip_rate_acc = frames_per_sec - frame_rate + frames_per_sec / 2.0;
	time_per_frame = 1.0 / frames_per_sec;
	drop_timeout = time_per_frame * 1.5;

	R	= 0;
	r31	= (double) quant_max / lroundn(video_bit_rate / frame_rate * 1.0);
	Tmin	= lroundn(video_bit_rate / frame_rate / 8.0);
	avg_acti = 400.0;
	avg_actp = 400.0;

	Xi	= lroundn(160.0 * video_bit_rate / 115.0);
	Xp	= lroundn( 60.0 * video_bit_rate / 115.0); 
	Xb	= lroundn( 42.0 * video_bit_rate / 115.0); 

	d0i	= 10.0 / r31;
	d0p	= 10.0 / r31;
	d0b	= 14.0 / r31;

	x	= frames_per_sec / frame_rate - 1.0;

	G0	= lroundn((ni + np + nb - ob) * video_bit_rate / frame_rate);
	Gn	= lroundn((ni + np + nb) * video_bit_rate / frame_rate);
	
	Gn	-= (ni + np + nb) * x * Sz;

	Tavg	= lroundn(video_bit_rate / frame_rate);
	G4	= Gn * 4;
R = Gn;
G0 = Gn;
//fprintf(stderr, "R0 = %d\n", R);

	Ei = 0, Ep = 0, Eb = 0;
	ei = 0, ep = 0, eb = 0;
	gop_count = 0;

	bstart(&video_out, seq_header_template);
	assert(sequence_header() == 16);

	if (banner)
		free(banner);
	asprintf(&banner,
		anno ? "MP1E " VERSION " Test Stream G%dx%d b%2.2f f%2.1f F%d\nANNO: %s\n" :
		       "MP1E " VERSION " Test Stream G%dx%d b%2.2f f%2.1f F%d\n",
		grab_width, grab_height, (double)(video_bit_rate) / 1E6,
		frame_rate, filter_mode, anno);
}

void
video_init(void)
{
	int i;
	int bmax;

	binit_write(&video_out);

	vlc_init();

	ASSERT("allocate forward reference buffer",
		(oldref = calloc_aligned(mb_num * 6 * 64 * sizeof(unsigned char), 4096)) != NULL);

	ASSERT("allocate backward reference buffer",
		(newref = calloc_aligned(mb_num * 6 * 64 * sizeof(unsigned char), 4096)) != NULL);

	bstart(&video_out, oldref);
	Sz = picture_zero();

	ASSERT("allocate 0P template, %d bytes",
		(zerop_template = calloc_aligned(Sz * sizeof(unsigned char), 32)) != NULL, Sz);

	memcpy(zerop_template, oldref, Sz);

	buddy.org[0] = NULL;
	last.org[0] = NULL;
	last.time = 0;
	p_succ = 0;

	video_frames_dropped = 0;
	video_frame_count = 0;

	/* GOP sequence */

	assert(gop_sequence[0] == 'I');

	if (strchr(gop_sequence, 'P') ||
	    strchr(gop_sequence, 'B'))
		insert_gop_header = TRUE;

	ni =
	np =
	nb = 0;

	bmax = 0;

	for (i = 0; i < 1024; i++)
		switch (gop_sequence[i]) {
		case 'I':
			ni++;
			ob = 0;
			break;

		case 'P':
			np++;
			ob = 0;
			break;

		case 'B':
			nb++;
			ob++;	// GOP overlapping B pictures
			if (ob > bmax)
				bmax = ob;
			break;

		default:
			i = 1024;
		}

	if (bmax >= sizeof(stack) / sizeof(stack[0]))
		FAIL("Too many successive B pictures");


#if PACKED
		for (i = 0; i < 6; i++) {
			mb_address.block[i].offset	= 64;
			mb_address.block[i].pitch	= 8;
		}

		mb_address.col.lum	= 6 * 64;
		mb_address.col.chrom	= 0;
		mb_address.row.lum	= 0;
		mb_address.row.chrom	= 0;
		mb_address.chrom_0	= mb_address.block[4].offset;
#else
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

		motion = (motion_min + motion_max) << 7;

		p_inter_bias *= 2;
		b_inter_bias *= 4;
#endif

	mb_cx_row = mb_height;
	mb_cx_thresh = 100000;

	if (mb_height >= 10) {
		mb_cx_row /= 3;
		mb_cx_thresh = lroundn(mb_cx_row * mb_width * 0.95);
	}

	video_reset();

	video_fifo = mux_add_input_stream(
		VIDEO_STREAM, "video-mpeg1",
		mb_num * 384 * 4, vid_buffers,
		frames_per_sec, video_bit_rate, video_cap_fifo);
}
