/*
 *  MPEG-1 Real Time Encoder
 *
 *  Copyright (C) 1999-2000 Michael H. Schimek
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

/* $Id: video.h,v 1.20 2002-08-22 22:03:49 mschimek Exp $ */

#ifndef VIDEO_H
#define VIDEO_H

#include <stdint.h>

#include "../common/bstream.h"
#include "../common/fifo.h"
#include "../common/log.h"
#include "../common/math.h"
#include "../common/sync.h"
#include "mblock.h"

#include "libvideo.h"
#include "mpeg.h"

#include "../b_mp1e.h"

#define MAX_WIDTH 1024			/* 1 ... 4096 */
#define MAX_HEIGHT 1024			/* 1 ... 2800 */

#define reg(n) __attribute__ ((regparm (n)))
#define elements(array) (sizeof(array) / sizeof(array[0]))

struct rc {
	int		ni, np, nb, ob;		/* picture types per GOP */
	long long	Ei, Ep, Eb;
	long long	gop_count;
	double		ei, ep, eb;
	int		G0, Gn;			/* estimated target bits per GOP */
	double		G4;
	int		Tavg;			/* estimated avg. bits per frame */
	int		Tmin;			/* minimum target bits per frame */
	int		R;			/* remaining bits in GOP */

	double		Xi, Xp, Xb;		/* global complexity measure */
	double		d0i, d0p, d0b;		/* virtual buffer fullness */
	double		r31;			/* reaction parameter */

	double		avg_acti, avg_actp;	/* avg spatial activity, intra/inter */

	/* auto */

	double		act_sumi, act_sump;	/* sum spatial activity, intra/inter */
	double		Ti, Tmb;
	int		T;
};

/*
 *  Max. successive P pictures when overriding gop_sequence
 *  (error accumulation) and max. successive B pictures we can stack up
 */
#define MAX_P_SUCC 3
#define MAX_B_SUCC 31

#define B_SHARE 1.4

static inline void
rc_picture_start(struct rc *rc, picture_type type, int mb_num)
{
	switch (type) {
	case I_TYPE:
		/*
		 *  T = lroundn(R / (+ (ni) * Xi / (Xi * 1.0)
		 *		     + (np) * Xp / (Xi * 1.0)
		 *		     + (nb) * Xb / (Xi * 1.4)));
		 */
		rc->T = lroundn(rc->R / ((rc->ni + rc->ei)
					 + ((rc->np + rc->ep) * rc->Xp
					    + (rc->nb + rc->eb) * rc->Xb * (1 / B_SHARE))
					 / rc->Xi));
		rc->Ti = -rc->d0i;
		break;

	case P_TYPE:
		rc->T = lroundn(rc->R / ((rc->np + rc->ep)
					 + ((rc->ni + rc->ei) * rc->Xi
					    + (rc->nb + rc->eb) * rc->Xb * (1 / B_SHARE))
					 / rc->Xp));
		rc->Ti = -rc->d0p;
		break;

	case B_TYPE:
		/*
		 *  T = lroundn(R / (+ (ni + ei) * Xi * 1.4 / Xb
		 *		     + (np + ep) * Xp * 1.4 / Xb
		 *		     + (nb + eb) * Xb / Xb));
		 */
		rc->T = lroundn(rc->R / (((rc->ni + rc->ei) * rc->Xi
					  + (rc->np + rc->ep) * rc->Xp) * B_SHARE
					 / rc->Xb + (rc->nb + rc->eb)));
		rc->Ti = -rc->d0b;
		break;

	default:
		FAIL("!reached");
	}

	if (rc->T < rc->Tmin)
		rc->T = rc->Tmin;

	rc->Tmb = rc->T / mb_num;

	rc->act_sumi = 0.0;
	rc->act_sump = 0.0;

	if (0)
	fprintf(stderr, "P%d T=%8d Ti=%f Tmin=%8d Tavg=%8d Tmb=%f X=%f,%f,%f\n",
		type, rc->T, rc->Ti, rc->Tmin, rc->Tavg, rc->Tmb, rc->Xi, rc->Xp, rc->Xb);
}

static inline int
rc_quant(struct rc *rc, mb_type type,
	 double acti, double actp,
	 int bits_out, int qs, int quant_max)
{
	int quant;

	switch (type) {
	case MB_INTRA:
		rc->act_sumi += acti;
		acti = (2.0 * acti + rc->avg_acti) / (acti + 2.0 * rc->avg_acti);
		quant = lroundn((bits_out - rc->Ti) * rc->r31 * acti);
		quant = saturate(quant >> qs, 1, quant_max);
		if (0)
		fprintf(stderr, "<< %f %f %d\n", (double) bits_out, (double) rc->Ti, quant);
		rc->Ti += rc->Tmb;
		break;

	case MB_FORWARD:
	case MB_BACKWARD:
		rc->act_sumi += acti;
		rc->act_sump += actp;
		actp = (2.0 * actp + rc->avg_actp) / (actp + 2.0 * rc->avg_actp);
		quant = lroundn((bits_out - rc->Ti) * rc->r31 * actp);
		quant = saturate(quant >> qs, 1, quant_max);
		rc->Ti += rc->Tmb;
		break;

	case MB_INTERP:
		rc->act_sumi += acti;
		rc->act_sump += actp;
		actp = (2.0 * actp + rc->avg_actp) / (actp + 2.0 * rc->avg_actp);
		quant = lroundn((bits_out - rc->Ti) * rc->r31 * actp);
		/* quant = saturate(quant, 1, quant_max); */
		rc->Ti += rc->Tmb;
		break;

	default:
		FAIL("!reached");
	}

	return quant;
}

// XXX this is no CBR/VBR anymore, but for now better than nothing.
// zap: Wed, 30 Jan 2002 11:07:55 +0100
#define RQ(d0) do { if (d0 < -2 * rc->T) d0 = -2 * rc->T; } while (0)

static inline void
rc_picture_end(struct rc *rc, picture_type type,
	       int S, int quant_sum, int mb_num)
{
	switch (type) {
	case I_TYPE:
		rc->avg_acti = rc->act_sumi / mb_num;
		rc->Xi = lroundn(S * (double) quant_sum / mb_num);
		rc->d0i += S - rc->T; /* bits encoded - estimated bits */
		RQ(rc->d0i);
		break;

	case P_TYPE:
		rc->avg_acti = rc->act_sumi / mb_num;
		rc->avg_actp = rc->act_sump / mb_num;
		rc->Xp = lroundn(S * (double) quant_sum / mb_num);
		rc->d0p += S - rc->T;
		RQ(rc->d0p);
		break;

	case B_TYPE:
		rc->avg_acti = rc->act_sumi / mb_num;
		rc->avg_actp = rc->act_sump / mb_num;
		rc->Xb = lroundn(S * (double) quant_sum / mb_num);
		rc->d0b += S - rc->T;
		RQ(rc->d0b);
		break;

	default:
		FAIL("!reached");
	}
}

typedef struct filter_param __attribute__ ((aligned (64))) filter_param;

typedef int (filter_fn)(filter_param *, int col, int row) __attribute__ ((regparm (3)));

struct filter_param {
	int16_t *		dest;		// 0
	uint8_t *		src;
	filter_fn *		func;		// 8
	int			offset;

	int			u_offset;	// 16
	int			v_offset;
	int			stride;		// 24
	int			uv_stride;

	int			clip_col;	// 32
	int			clip_row;
	int			clip_x;		// 40
	int			clip_y;

	mmx_t			lim;		// 48
        int			tcol;		// 56
        int			trow;

	mmx_t			scratch[8];
} __attribute__ ((aligned (64)));

typedef struct stacked_frame {
	uint8_t *	org;
	buffer *	buffer;
	double		time;
	int		skipped;
} stacked_frame;

typedef struct mblock_hist {
	int8_t			quant;
	int8_t			pad1;
	int8_t			mv[2];
} mblock_hist;

typedef struct mpeg1_context mpeg1_context;

struct mpeg1_context { 
	filter_param	filter_param[2];

	uint8_t		seq_header_template[32];

	uint8_t *	zerop_template;		/* empty P picture */
	int		Sz;			/* .. size in bytes */

	int		(* picture_i)(mpeg1_context *, uint8_t *org);
	int		(* picture_p)(mpeg1_context *, uint8_t *org,
				      int dist, int forward_motion);
	int		(* picture_b)(mpeg1_context *, uint8_t *org,
				      int dist, int forward_motion,
				      int backward_motion);

	unsigned int	(* predict_forward)(uint8_t *from) reg(1);
	unsigned int	(* predict_bidirectional)(uint8_t *from1, uint8_t *from2,
						  unsigned int *vmc1,
						  unsigned int *vmc2);

	stacked_frame	stack[MAX_B_SUCC];
	stacked_frame	last;

	mblock_hist *	mb_hist;		/* attn: base -1 */

						/* frames encoded (coding order) */
	int		gop_frame_count;	/* .. in current GOP (display order) */
	int		seq_frame_count;	/* .. since last sequence header */

	double		skip_rate_acc;
	double		drop_timeout;

	uint8_t *	oldref;			/* past reference frame buffer */

	bool		insert_gop_header;
	bool		closed_gop;		/* random access point, no fwd ref */
	bool		referenced;		/* by other P or B pictures */
	bool		slice;

	int		quant_sum;

	struct rc	rc;

	int		p_succ;
	int		skipped_fake;
	int		skipped_zero;

	uint8_t *	banner;

	consumer	cons;

	int		mb_cx_row;
	int		mb_cx_thresh;

	int		motion_min;
	int		motion_max;

	int		coded_width;
	int		coded_height;

	int		frames_per_seqhdr;
	int		aspect_ratio_code;

	/* input */

//	sync_stream	sstr;
	double		coded_elapsed;
	double		nominal_frame_rate;
	double		nominal_frame_period;

	/* Output */

	fifo *		fifo;
	producer	prod;
	double		coded_time_elapsed;
	double		coded_frame_rate;
	double		coded_frame_period;
	double		coded_frames_countd;

	/* Options */

	mp1e_codec	codec;

	int		bit_rate;
	int		frame_rate_code;
	double		virtual_frame_rate;
	char *		gop_sequence;
	int		skip_method;
	bool		motion_compensation;
	bool		monochrome;
	char *		anno;
	double		num_frames;
};

extern uint8_t * newref;	/* future reference frame buffer */


extern int		mb_col, mb_row,
			mb_width, mb_height,
			mb_last_col, mb_last_row,
			mb_num;

extern short		mblock[7][6][8][8];

extern struct mb_addr {
	struct {
		int		offset;
		int		pitch;
	}		block[6];
	struct {
		int		lum;
		int		chrom;
	}		col, row;
	int		chrom_0;
} mb_address;

#define reset_mba()							\
do {									\
	mb_address.block[0].offset = 0;					\
	mb_address.block[4].offset = mb_address.chrom_0;		\
} while (0)

#define mba_col_incr()							\
do {									\
	mb_address.block[0].offset += mb_address.col.lum;		\
	mb_address.block[4].offset += mb_address.col.chrom;		\
} while (0)

#define mba_row_incr()							\
do {									\
	mb_address.block[0].offset += mb_address.row.lum;		\
	mb_address.block[4].offset += mb_address.row.chrom;		\
} while (0)








#define video_align(n) __attribute__ ((aligned (n)))

extern struct bs_rec	video_out video_align(32);

extern int		dropped;
extern const char *	filter_labels[];

extern long long	video_frame_count;
extern long long	video_frames_dropped;

// extern fifo *		video_fifo;
// extern pthread_t	video_thread_id;

extern void *		mpeg1_video_ipb(void *capture_fifo);

extern void		conv_init(int);
extern void		filter_init(rte_video_stream_params *par,
				    struct filter_param *fp);
extern void		video_coding_size(int width, int height, rte_bool field);
extern int		video_look_ahead(char *gop_sequence);
extern double		video_sampling_aspect(double frame_rate,
				unsigned int width, unsigned int height);

/* don't change order */
/* XXX rethink */
enum {
	CM_INVALID,
	CM_YUV,
	CM_YUYV,
	CM_YUYV_VERTICAL_DECIMATION,
	CM_YUYV_TEMPORAL_INTERPOLATION,
	CM_YUYV_VERTICAL_INTERPOLATION,
	CM_YUYV_PROGRESSIVE,
	CM_YUYV_PROGRESSIVE_TEMPORAL,
	CM_YUV_VERTICAL_DECIMATION,
	CM_YUYV_HORIZONTAL_DECIMATION,
	CM_YUYV_QUAD_DECIMATION,
	CM_YVU,
	CM_NUM_MODES
};

#endif /* VIDEO_H */
