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

/* $Id: mpeg1.c,v 1.5 2000-08-10 01:18:59 mschimek Exp $ */

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
#include "vlc.h"
#include "dct.h"
#include "predict.h"
#include "mpeg.h"
#include "video.h"
#include "../systems/mpeg.h"
#include "../systems/systems.h"




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
static double		avg_act;			// average spatial activity
static int		p_succ;

static unsigned char 	seq_header_template[32] __attribute__ ((aligned (CACHE_LINE)));
							// precompressed sequence header
static char *		banner;
static unsigned char *	zerop_template;			// precompressed empty P picture
static int		Sz;				// .. size in bytes

static unsigned char *	oldref;				// past reference frame buffer
static unsigned char *	newref;				// future reference frame buffer
/*
 *  Reference buffer format is
 *  [mb_height]
 *  [mb_width]  - for all macroblocks of a frame
 *  [6]         - Y0, Y2, Y1, Y3, Cb, Cr
 *  [8][8]      - 8 bit unsigned samples, e. g. according to ITU-R Rec. 601
 */

struct {
	int		offset;
	int		pitch;
} mb_address[6];

static int		warn[2];

static double bits = 0;
double gbits = 0;
static double counts = 0;
static int mb_cx_row, mb_cx_thresh;

/* main.c */
extern int		frames_per_seqhdr;
extern int		video_num_frames;
extern double		video_stop_time;

#define new_intra_quant mmx_new_intra_quant
#define new_inter_quant mmx_new_inter_quant
#define fdct_intra mmx_fdct_intra
#define fdct_inter mmx_fdct_inter
#define mpeg1_idct_intra mmx_mpeg1_idct_intra
#define mpeg1_idct_inter mmx_mpeg1_idct_inter

#define mpeg1_encode_intra mmx_mpeg1_encode_intra
#define mpeg1_encode_inter mmx_mpeg1_encode_inter

#define predict_forward mmx_predict_forward
#define predict_backward mmx_predict_backward

static const unsigned char
quant_res_inter[32] __attribute__ ((SECTION("video_tables") aligned (CACHE_LINE))) =
{
	1, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
	16, 16, 18, 18, 20, 20, 22, 22,	24, 24, 26, 26, 28, 28, 30, 30
};

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
    quant_max = 31;

fifo *			video_fifo;

#include "dct/ieee.h"



/*
 *  Picture layer
 */

#define B_SHARE 1.4

#define macroblock_address(skipped)					\
do {									\
	for (i = (skipped); i >= 33; i -= 33)				\
		bputl(&video_out, 0x008, 11); /* mb addr escape */	\
									\
	bstartq(macroblock_address_increment[i].code);			\
	length = macroblock_address_increment[i].length; /* 1...11 */	\
} while (0)

#define motion_vector(dmv)						\
do {									\
	int l0 = motion_vector_component[dmv[0]].length;		\
	int l1 = motion_vector_component[dmv[1]].length;		\
									\
	bcatq(motion_vector_component[dmv[0]].code, l0);		\
	bcatq(motion_vector_component[dmv[1]].code, l1);		\
	length += l0 + l1; /* 2...26 (f_code == 3) */			\
} while (0)

#define pb_intra_mb(f)							\
do {									\
	brewind(&mark, &video_out);					\
									\
	for (;;) {							\
		new_intra_quant(quant);					\
									\
		pr_start(22, "FDCT intra");				\
		fdct_intra(); /* mblock[0] -> mblock[1] */		\
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

#define pb_cx_intra(ref)						\
do {									\
	for (; mb_row < mb_height; mb_row++) {				\
		for (mb_col = 0; mb_col < mb_width; mb_col++) {		\
			int length, i;					\
									\
			pr_start(41, "Filter");				\
			var = (*filter)(org0, org1); /* -> mblock[0] */	\
			pr_end(41);					\
									\
			emms();						\
									\
			act_sum += act = var / 65536.0 + 1;		\
			act = (2.0 * act + avg_act)			\
				/ (act + 2.0 * avg_act);		\
									\
			quant = lroundn((bwritten(&video_out) - Ti)	\
				* r31 * act);				\
			quant = quant_res_intra[saturate(quant >> 1,	\
				1, quant_max)];				\
									\
			Ti += Tmb;					\
									\
			pb_intra_mb(TRUE);				\
									\
			prev_quant = quant;				\
			quant_sum += quant;				\
			mb_skipped = 0;					\
									\
			if (ref) {					\
				pr_start(23, "IDCT intra");		\
				mpeg1_idct_intra(new);			\
				pr_end(23);				\
									\
				new += 6 * 64;				\
			}						\
		}							\
	}								\
} while (0)


static const int motion = 0;
static int MV[2][2] = {
	{ 0, 0 },
	{ 0, 0 }
};

static int
picture_i(unsigned char *org0, unsigned char *org1)
{
	double act, act_sum;
	unsigned char *new;
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

	new = oldref;
	oldref = newref;
	newref = new;

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

			emms();

			/* Calculate quantization factor */

			act = var / 65536.0 + 1;
			act_sum += act;
			act = (2.0 * act + avg_act) / (act + 2.0 * avg_act);
			quant = quant_res_intra[saturate(lroundn((bwritten(&video_out) - Ti) * r31 * act), 1, quant_max)];
			/*
			 *  quant_res_intra halves the quantization factor resolution above 16 to
			 *  reduce lookup table size (MMX DCT). Quality is already poor at 16,
			 *  so it won't hurt very much.
	                 */

			Ti += Tmb;

			if (quant >= 4 && nbabs(quant - prev_quant) <= 2)
				quant = prev_quant;

			/* Encode macroblock */

			brewind(&mark, &video_out);

			for (;;) {
				new_intra_quant(quant);

				pr_start(22, "FDCT intra");
				fdct_intra(); // mblock[0] -> mblock[1]
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
				mpeg1_idct_intra(new); // mblock[1] -> new
				pr_end(23);

				new += 6 * 64;
			}
#if TEST_PREVIEW
			if (preview > 1) {
				emms();
				packed_preview(newref, mb_width, mb_height);
			}
#endif
		}
	}

	emms();
 
	/* Rate control */

	S = bflush(&video_out);

//	printv(4, "I %8.0f T=%d S=%d d0i=%f R=%d \n", gbits, T, S, d0i, R);

	Xi = lroundn(S * (double) quant_sum / mb_num);

	d0i += S - T; // bits encoded - estimated bits

	if (d0i > G4) {
		if (!warn[0])
			warn[0]++, fprintf(stderr, "Bit rate is too low, dropping frames\n");
	} else if (d0i < -G4) {
		if (!warn[1])
			warn[1]++, fprintf(stderr, "Bit rate is too high, may run out of sync\n");
	}

	avg_act = act_sum / mb_num;
			
	pr_end(21);

#if TEST_PREVIEW
	if (preview)
		packed_preview(newref, mb_width, mb_height);
#endif

	return S >> 3;
}

static int
picture_p(unsigned char *org0, unsigned char *org1)
{
	double act, act_sum;
	unsigned char *old, *new;
	int quant_sum;
	int S, T, quant, prev_quant, quant1 = 1;
	struct bs_rec mark;
	unsigned char *q1p;
	int var, vmc;
	int mb_skipped, mb_count;
	int rows, intra_count = 0;

	printv(3, "Encoding P picture #%d GOP #%d, ref=%c\n",
		video_frame_count, gop_frame_count, "FT"[referenced]);

	pr_start(24, "Picture P");

	/* Initialize rate control parameters */

	new = oldref;
	old = oldref = newref;
	newref = new;

#if TEST_PREVIEW
	if (preview > 1)
		memcpy(new, old, 64 * 6 * mb_num);
#endif

	T = lroundn(R / ((np + ep) + ((ni + ei) * Xi + (nb + eb) * Xb / B_SHARE) / Xp));

	Ti = -d0p;

	if (T < Tmin)
		T = Tmin;

	Tmb = T / mb_num;

	quant_sum = 0;
	act_sum = 0.0;

	reset_dct_pred();
	
	if (motion) {
		PMV[0][0] = 0;
		PMV[0][1] = 0;
	}

	prev_quant = -100;
	mb_count = 1;
	mb_skipped = 0;

	/* Picture header */

	bepilog(&video_out);

	bputl(&video_out, PICTURE_START_CODE, 32);

	bputl(&video_out, ((gop_frame_count & 1023) << 19) + (P_TYPE << 16) + 0, 29);
	bputl(&video_out, (0 << 10) + (3 << 7) + (0 << 6), 11);
	/*
	 *  temporal_reference [10], picture_coding_type [3], vbv_delay [16];
	 *  full_pel_forward_vector '0', forward_f_code 3,
	 *  extra_bit_picture '0', byte align [6]
	 */

	bputl(&video_out, SLICE_START_CODE + 0, 32);
	q1p = (unsigned char *) video_out.p + (video_out.n >> 3);
	bputl(&video_out, (1 << 1) + 0, 6);
	/* slice header: quantiser_scale_code 'xxxxx', extra_bit_slice '0' */

	bprolog(&video_out);

	for (mb_row = 0, rows = mb_cx_row;; mb_row++) {
		if (mb_row >= rows) {
			if (mb_row >= mb_height)
				break;

			if (1 && intra_count >= mb_cx_thresh) {
				pr_event(43, "P/cx trap");
				pb_cx_intra(referenced);
				break;
			}

			rows = mb_height;
		}

		for (mb_col = 0; mb_col < mb_width; mb_col++) {

			/* Read macroblock (MMX state) */

			pr_start(41, "Filter");
			var = (*filter)(org0, org1); // -> mblock[0]
			pr_end(41);

			pr_start(51, "Predict forward");
			vmc = predict_forward(old);
			pr_end(51);

			emms();

			/* Encode macroblock */

			if (vmc > var || vmc > p_inter_bias) {
				int length, i;

				/* Calculate quantization factor */

				act_sum += act = var / 65536.0 + 1;
				act = (2.0 * act + avg_act) / (act + 2.0 * avg_act);

				quant = 
lroundn((bwritten(&video_out) - Ti) * r31 * act);
quant = quant_res_intra[saturate(quant >> 1, 1, quant_max)];

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

				if (motion) {
					PMV[0][0] = 0;
					PMV[0][1] = 0;
				}

				if (referenced) {
					pr_start(23, "IDCT intra");
					mpeg1_idct_intra(new); // mblock[0] -> new
					pr_end(23);
				}
			} else {
				unsigned int cbp;
				int dmv[2], len, length, i;

				/* Calculate quantization factor */

				act_sum += act = vmc / 65536.0 + 1;
				act = (2.0 * act + avg_act) / (act + 2.0 * avg_act);

quant = quant_res_inter[saturate(
lroundn((bwritten(&video_out) - Ti) * r31 * act), 2, quant_max)];
    				
//				if (quant >= 4 && abs(quant - prev_quant) <= 2)
//					quant = prev_quant;

				Ti += Tmb;

				if (motion) {
					if (mb_row > 0 && mb_row < (mb_height - 1) &&
					    mb_col > 0 && mb_col < (mb_width - 1)) {
						MV[0][0] = (rand() & 3) - 2;
						MV[0][1] = (rand() & 3) - 2;
					} else
						memset(MV, 0, sizeof MV);

					dmv[0] = (MV[0][0] - PMV[0][0]) & 0x7F;
					dmv[1] = (MV[0][1] - PMV[0][1]) & 0x7F;

					PMV[0][0] = MV[0][0];
					PMV[0][1] = MV[0][1];
				}

				brewind(&mark, &video_out);

				for (;;) {
					i = mb_skipped;

					new_inter_quant(quant);

					pr_start(26, "FDCT inter");
					cbp = fdct_inter(mblock[1]); // mblock[1] -> mblock[0]
					pr_end(26);

					if (cbp == 0 && mb_count > 1 && mb_count < mb_num &&
						(!motion || (dmv[0] | dmv[1]) == 0)) {

						__builtin_memcpy(new, old, 6 * 64);

						if (motion) {
							PMV[0][0] = 0;
							PMV[0][1] = 0;
						}

						i++;

						break;
					} else {
						bepilog(&video_out);

						macroblock_address(i); // 1..11

						i = 0;

						if (cbp == 0) {
							if (motion) {
								bcatq(1, 3);
								motion_vector(dmv);
								bputq(&video_out, length + 3);
								/* macroblock_type '001' (P MC, Not Coded) */
							} else {
								bcatq(7, 5);
								bputq(&video_out, length + 5);
								/* macroblock_type '001' (P MC, Not Coded), '11' MV(0, 0) */
							}

							bprolog(&video_out);
							__builtin_memcpy(new, old, 6 * 64);
							break;
						} else {
							if (motion && (MV[0][0] | MV[0][1])) {
								if (prev_quant != quant) {
									bcatq((2 << 5) + quant, 10);
									length += 10;
									/* macroblock_type '0001 0' (P MC, Coded, Quant), quantiser_scale_code 'xxxxx' */
								} else {
									bcatq(1, 1);
									length += 1;
									/* macroblock_type '1' (P MC, Coded) */
								}

								motion_vector(dmv);
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
									mpeg1_idct_inter(new, old, cbp); // [0] & [3]
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

			old += 6 * 64;
			new += 6 * 64;

			mb_count++;

#if TEST_PREVIEW
			if (preview > 1) {
				emms();
				packed_preview(newref, mb_width, mb_height);
			}
#endif
		}
	}

	emms();

	/* Rate control */

	S = bflush(&video_out);
	
	*q1p |= (quant1 << 3);

	avg_act = act_sum / mb_num;

	// printv(4, "P %8.0f T=%d S=%d d0p=%f\n", gbits, T, S, d0p);

	Xp = lroundn(S * (double) quant_sum / mb_num);

	d0p += S - T;

	if (d0p > G4) {
		if (!warn[0])
			warn[0]++, fprintf(stderr, "Bit rate is too low, dropping frames\n");
	} else if (d0p < -G4) {
		if (!warn[1])
			warn[1]++, fprintf(stderr, "Bit rate is too high, may run out of sync\n");
	}

	pr_end(24);

#if TEST_PREVIEW
	if (preview == 1)
		packed_preview(newref, mb_width, mb_height);
#endif

	return S >> 3;
}

static int
picture_b(unsigned char *org0, unsigned char *org1)
{
	double act, act_sum;
	unsigned char *old, *new;
	short (* iblock)[6][8][8];
	int quant_sum;
	int S, T, quant, prev_quant, quant1 = 1;
	struct bs_rec mark;
	unsigned char *q1p;
	int var, vmc, vmcf, vmcb;
	int macroblock_type, mb_type_last;
	int mb_skipped, mb_count;
	int rows, intra_count = 0;

	printv(3, "Encoding B picture #%d GOP #%d, fwd=%c\n",
		video_frame_count, gop_frame_count, "FT"[!closed_gop]);

	pr_start(25, "Picture B");

	/* Initialize rate control parameters */

	old = oldref;
	new = newref;

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

	if (motion) {
		PMV[0][0] = 0;
		PMV[0][1] = 0;
		PMV[1][0] = 0;
		PMV[1][1] = 0;
	}

	prev_quant = -100;

	mb_count = 1;
	mb_skipped = 0;
	mb_type_last = -1;


	/* Picture header */

	bepilog(&video_out);

	bputl(&video_out, PICTURE_START_CODE, 32);

	bputl(&video_out, ((gop_frame_count & 1023) << 19) + (B_TYPE << 16) + 0, 29);
	bputl(&video_out, (0 << 10) + (3 << 7) + (0 << 6) + (3 << 3) + (0 << 2), 11);
	/*
	 *  temporal_reference [10], picture_coding_type [3], vbv_delay [16];
	 *  full_pel_forward_vector '0', forward_f_code 3,
	 *  full_pel_backward_vector '0', backward_f_code 3,
	 *  extra_bit_picture '0', byte align [2]
	 */

	bputl(&video_out, SLICE_START_CODE + 0, 32);
	q1p = (unsigned char *) video_out.p + (video_out.n >> 3);
	bputl(&video_out, (1 << 1) + 0, 6);
	/* slice header: quantiser_scale_code 'xxxxx', extra_bit_slice '0' */

	bprolog(&video_out);

	for (mb_row = 0, rows = mb_cx_row;; mb_row++) {
		if (mb_row >= rows) {
			if (mb_row >= mb_height)
				break;

			if (1 && intra_count >= mb_cx_thresh) {
				pr_event(48, "B/cx trap");
				pb_cx_intra(FALSE);
				break;
			}

			rows = mb_height;
		}

		for (mb_col = 0; mb_col < mb_width; mb_col++) {

			/* Read macroblock (MMX state) */

			pr_start(41, "Filter");
			var = (*filter)(org0, org1); // -> mblock[0]
			pr_end(41);

			/* Choose prediction type */

			if (closed_gop) {
				vmc = predict_backward(new);
				macroblock_type = MB_BACKWARD;
				iblock = &mblock[1];
			} else {
				vmc = mmx_predict_bidirectional(old, new, &vmcf, &vmcb);
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
			}

			emms();

			vmc <<= 8;

			/* Encode macroblock */

			if (vmc > var || vmc > b_inter_bias) {
				int length;
				int i;

				/* Calculate quantization factor */

				act_sum += act = var / 65536.0 + 1;
				act = (2.0 * act + avg_act) / (act + 2.0 * avg_act);
	
	quant = lroundn((bwritten(&video_out) - Ti) * r31 * act);
	quant = quant_res_intra[saturate(quant >> 1, 1, quant_max)];

//				if (quant >= 4 && abs(quant - prev_quant) <= 2)
//					quant = prev_quant;

				Ti += Tmb;

				macroblock_type = MB_INTRA;
				intra_count++;

				pb_intra_mb(TRUE);

				if (prev_quant < 0)
					quant1 = quant;

				prev_quant = quant;
				quant_sum += quant;

				mb_skipped = 0;

				if (motion) {
					PMV[0][0] = 0;
					PMV[0][1] = 0;
					PMV[1][0] = 0;
					PMV[1][1] = 0;
				}
			} else {
				unsigned int cbp;
				int dmv[2][2], len, length, i;

				/* Calculate quantization factor */

				act_sum += act = vmc / 65536.0 + 1;
				act = (2.0 * act + avg_act) / (act + 2.0 * avg_act);

				quant = quant_res_inter[saturate(
			lroundn((bwritten(&video_out) - Ti) * r31 * act), 2, quant_max)];

//				if (quant >= 4 && abs(quant - prev_quant) <= 2)
//					quant = prev_quant;

				Ti += Tmb;

				if (motion) {
					if (mb_row > 0 && mb_row < (mb_height - 1) &&
					    mb_col > 0 && mb_col < (mb_width - 1)) {
						MV[0][0] = (rand() & 3) - 2;
						MV[0][1] = (rand() & 3) - 2;
						MV[1][0] = (rand() & 3) - 2;
						MV[1][1] = (rand() & 3) - 2;
					} else
						memset(MV, 0, sizeof MV);

					if (macroblock_type == MB_FORWARD) {
						dmv[0][0] = (MV[0][0] - PMV[0][0]) & 0x7F;
						dmv[0][1] = (MV[0][1] - PMV[0][1]) & 0x7F;
						dmv[1][0] = 0;
						dmv[1][1] = 0;
						PMV[0][0] = MV[0][0];
						PMV[0][1] = MV[0][1];
					} else
					if (macroblock_type == MB_BACKWARD) {
						dmv[0][0] = (MV[1][0] - PMV[1][0]) & 0x7F;
						dmv[0][1] = (MV[1][1] - PMV[1][1]) & 0x7F;
						dmv[1][0] = 0;
						dmv[1][1] = 0;
						PMV[1][0] = MV[1][0];
						PMV[1][1] = MV[1][1];
					} else
					if (macroblock_type == MB_INTERP) {
						dmv[0][0] = (MV[0][0] - PMV[0][0]) & 0x7F;
						dmv[0][1] = (MV[0][1] - PMV[0][1]) & 0x7F;
						dmv[1][0] = (MV[1][0] - PMV[1][0]) & 0x7F;
						dmv[1][1] = (MV[1][1] - PMV[1][1]) & 0x7F;
						PMV[0][0] = MV[0][0];
						PMV[0][1] = MV[0][1];
						PMV[1][0] = MV[1][0];
						PMV[1][1] = MV[1][1];
					}
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

				motion_vector(dmv[0]);

				if (macroblock_type == MB_INTERP)
					motion_vector(dmv[1]);
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

				motion_vector(dmv[0]);

				if (macroblock_type == MB_INTERP)
					motion_vector(dmv[1]);
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

			old += 6 * 64;
			new += 6 * 64;

			mb_count++;
			mb_type_last = macroblock_type;
		}
	}

	emms();

	/* Rate control */

	S = bflush(&video_out);

	*q1p |= (quant1 << 3);

	avg_act = act_sum / mb_num;

	// printv(4, "B %8.0f T=%d S=%d d0b=%f\n", gbits, T, S, d0b);

	Xb = lroundn(S * (double) quant_sum / mb_num);

	d0b += S - T;

	if (d0b > G4) {
		if (!warn[0])
			warn[0]++, fprintf(stderr, "Bit rate is too low, dropping frames\n");
	} else if (d0b < -G4) {
		if (!warn[1])
			warn[1]++, fprintf(stderr, "Bit rate is too high, may run out of sync\n");
	}

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





static inline void
_send_full_buffer(fifo *f, buffer *b)
{
	bits += b->used * 8;
	counts += 1.0 / 25.0;

//	printv(0, "%8.0f\n", gbits);

	send_full_buffer(f, b);
}

static struct {
	unsigned char * org[2];
	int		index;
	double		time;
} stack[MAX_B_SUCC], last, buddy, *this;

static inline void
promote(int n)
{
	int i;
	buffer *obuf;

	for (i = 0; i < n; i++) {
		printv(3, "Promoting stacked B picture #%d\n", i);

		obuf = wait_empty_buffer(video_fifo);
		bstart(&video_out, obuf->data);
		referenced = TRUE;
		Eb--;

		if (p_succ >= MAX_P_SUCC) {
			p_succ = 0;
			Ei++;
			obuf->used = picture_i(stack[i].org[0], stack[i].org[1]);
			obuf->type = I_TYPE;
		} else {
			p_succ++;
			Ep++;
			obuf->used = picture_p(stack[i].org[0], stack[i].org[1]);
			obuf->type = P_TYPE;
		}

		if (stack[i].index >= 0)
			video_frame_done(stack[i].index);
//			video_frame_done(stack[i].buffer);

		obuf->offset = 1;
		obuf->time = stack[i].time;

		_send_full_buffer(video_fifo, obuf);

		video_frame_count++;
		seq_frame_count++;
		gop_frame_count++;
	}
}

static inline void
resume(int n)
{
	int i;
	buffer *obuf, *last = NULL;

	referenced = FALSE;
	p_succ = 0;

	for (i = 0; i < n; i++) {
		if (!stack[i].org[0]) {
			assert(last != NULL);

			obuf = wait_empty_buffer(video_fifo);

			memcpy(obuf->data, last->data, last->used);
			((unsigned int *) obuf->data)[1] |=
				swab32((gop_frame_count & 1023) << 22);

			obuf->type = B_TYPE;
			obuf->offset = 0;
			obuf->used = last->used;
			obuf->time = stack[i].time;

			_send_full_buffer(video_fifo, last);
		} else {
			obuf = wait_empty_buffer(video_fifo);
			bstart(&video_out, obuf->data);
			obuf->used = picture_b(stack[i].org[0], stack[i].org[1]);

			if (stack[i].index >= 0)
				video_frame_done(stack[i].index);
//				video_frame_done(stack[i].buffer);

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
int d3 = 3;

	printv(3, "Video compression thread\n");

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
                                        int index;
 
                                        this->index = buddy.index;
                                        this->org[1] = buddy.org[0];
                                        this->org[0] = video_wait_frame(&this->time, &index);

                                        if (this->org[0]) {
                                                buddy.index = index;
                                                buddy.org[0] = this->org[0];
                                        }

                                        if (!this->org[1]) {
                                                this->index = -1;
                                                this->org[1] = this->org[0];                                                 
					}
/*
					buffer *b;

					this->buffer = buddy.buffer;
					this->org[1] = buddy.org[0];

					b = video_wait_frame();
					this->time = b->time;
					this->org[0] = b->data; 

					if (this->org[0]) {
						buddy.buffer = b;
						buddy.org[0] = this->org[0];
					}

					if (!this->org[1]) {
						this->buffer = NULL;
						this->org[1] = this->org[0];
					}
*/
				} else {
                        		this->org[0] = video_wait_frame(&this->time, &this->index);
/*
					buffer *b;

					this->buffer = b = video_wait_frame();
					this->time = b->time;
					this->org[0] = b->data;
*/
				}
#if TEST_PREVIEW
				if (this->org[0] && (rand() % 100) < force_drop_rate) {
					printv(3, "Forced drop #%d\n", video_frame_count + sp);
					if (this->index >= 0)
						video_frame_done(this->index);
//					if (this->buffer)
//						video_frame_done(this->buffer);
					continue;
				}
#endif
			}

			sp++;

			if (!this->org[0] || this->time >= video_stop_time ||
			    video_frame_count + sp > video_num_frames) {
				printv(2, "Video: End of file\n");

				if (this->org[0] && this->index >= 0)
					video_frame_done(this->index);
//				if (this->org[0] && this->buffer)
//					video_frame_done(this->buffer);

				if (buddy.org[0])
					video_frame_done(buddy.index);
//					video_frame_done(buddy.buffer);

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
					
					if (sp >= 2 && *seq == 'B') // Have BB, duplicate last B
				    		goto next_frame;
					else // skip this
						skip_rate_acc = 0;
				} // else we skip it anyway
			} else
				last.time = this->time;

			if (skip_rate_acc < frames_per_sec) {
				if (this->org[0] && this->index >= 0)
					video_frame_done(this->index);
//				if (this->org[0] && this->buffer)
//					video_frame_done(this->buffer);

				assert(sp == 1 || gop_frame_count > 0);

				promote(sp - 1);

				assert(gop_frame_count > 0);

				obuf = wait_empty_buffer(video_fifo);

				memcpy(obuf->data, zerop_template, Sz);
				((unsigned int *) obuf->data)[1] |=
					swab32((gop_frame_count & 1023) << 22);

				obuf->type = P_TYPE;
				obuf->offset = 1;
				obuf->used = Sz;
				obuf->time = this->time;

				_send_full_buffer(video_fifo, obuf);

				video_frame_count++;
				gop_frame_count++;

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
gbits = (gbits + (bits / counts)) / 2.0;
bits=0;
counts=0;
if (d3-- <= 0) {
d3 = 3;
}

			printv(4, "Rewind sequence R=%d\n", R);

			G0 = Gn;
			ob = 0;
		}

		/* Encode current or forward P or I picture */

		gop_frame_count += sp - 1;

		referenced = (seq[1] == 'P') || (seq[1] == 'B') || (sp > 1) || preview;

		if (*seq++ == 'I') {
			p_succ = 0;
			obuf->used = picture_i(this->org[0], this->org[1]);
			obuf->type = I_TYPE;
		} else {
			p_succ++;
			obuf->used = picture_p(this->org[0], this->org[1]);
			obuf->type = P_TYPE;
		}

		if (this->index >= 0)
			video_frame_done(this->index);
//		if (this->buffer)
//			video_frame_done(this->buffer);

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

	for (;;) {
		obuf = wait_empty_buffer(video_fifo);
		obuf->type = 0;
		obuf->offset = 1;
		obuf->used = 0; // EOF mark
		obuf->time = last.time += time_per_frame;
		_send_full_buffer(video_fifo, obuf);
	}

	return NULL; // never
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
	avg_act	= 400.0;

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
/*
	if (preview) {
		for (i = 0; i < 4; i++)
			mb_address[0].pitch = lalign(mb_width * 16, CACHE_LINE);
		mb_address[1].offset = 0;
		mb_address[2].offset = 8 - mb_address[0].pitch * 16;
		mb_address[3].offset = 0;
		mb_address[4].pitch =
		mb_address[5].pitch = mb_address[0].pitch >> 1;
		mb_address[5].offset = mb_address[4].pitch * mb_height;
	} else if (motion) {
		for (i = 0; i < 4; i++)
			mb_address[0].pitch = lalign(mb_width * 16, CACHE_LINE);
		mb_address[1].offset = 0;
		mb_address[2].offset = 8 - mb_address[0].pitch * 16;
		mb_address[3].offset = 0;
		mb_address[4].pitch = 8;
		mb_address[5].pitch = 8;
		mb_address[5].offset = 64;
	} else { // not used
		for (i = 0; i < 6; i++) {
			mb_address[i].offset = 64;
			mb_address[i].pitch = 8;
		}
	}
*/
	mb_cx_row = mb_height;
	mb_cx_thresh = 100000;

	if (mb_height >= 10) {
		mb_cx_row /= 3;
		mb_cx_thresh = lroundn(mb_cx_row * mb_width * 0.95);
	}

	video_reset();

	video_fifo = mux_add_input_stream(VIDEO_STREAM,
		mb_num * 384 * 4, vid_buffers,
		frames_per_sec, video_bit_rate);
}
