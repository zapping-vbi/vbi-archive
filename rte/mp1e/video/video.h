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

/* $Id: video.h,v 1.6 2001-10-07 10:55:51 mschimek Exp $ */

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

#define MAX_WIDTH 1024			/* 1 ... 4096 */
#define MAX_HEIGHT 1024			/* 1 ... 2800 */

#define reg(n) __attribute__ ((regparm (n)))
#define elements(array) (sizeof(array) / sizeof(array[0]))

struct rc {
	int		ni, np, nb, ob;		/* picture types per GOP */
	int		Ei, Ep, Eb;
	int		gop_count;
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
					    + (rc->nb + rc->eb) * rc->Xb / B_SHARE)
					 / rc->Xi));
		rc->Ti = -rc->d0i;
		break;

	case P_TYPE:
		rc->T = lroundn(rc->R / ((rc->np + rc->ep)
					 + ((rc->ni + rc->ei) * rc->Xi
					    + (rc->nb + rc->eb) * rc->Xb / B_SHARE)
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

static inline void
rc_picture_end(struct rc *rc, picture_type type,
	       int S, int quant_sum, int mb_num)
{
	switch (type) {
	case I_TYPE:
		rc->avg_acti = rc->act_sumi / mb_num;
		rc->Xi = lroundn(S * (double) quant_sum / mb_num);
		rc->d0i += S - rc->T; /* bits encoded - estimated bits */
		break;

	case P_TYPE:
		rc->avg_acti = rc->act_sumi / mb_num;
		rc->avg_actp = rc->act_sump / mb_num;
		rc->Xp = lroundn(S * (double) quant_sum / mb_num);
		rc->d0p += S - rc->T;
		break;

	case B_TYPE:
		rc->avg_acti = rc->act_sumi / mb_num;
		rc->avg_actp = rc->act_sump / mb_num;
		rc->Xb = lroundn(S * (double) quant_sum / mb_num);
		rc->d0b += S - rc->T;
		break;

	default:
		FAIL("!reached");
	}
}

typedef struct stacked_frame {
	uint8_t *	org;
	buffer *	buffer;
	double		time;
} stacked_frame;

typedef struct video_context {
	uint8_t		seq_header_template[32];

	uint8_t *	zerop_template;		/* empty P picture */
	int		Sz;			/* .. size in bytes */

	int		(* picture_i)(uint8_t *org);
	int		(* picture_p)(uint8_t *org,
				      int dist, int forward_motion);
	int		(* picture_b)(uint8_t *org,
				      int dist, int forward_motion,
				      int backward_motion);

	unsigned int	(* predict_forward)(uint8_t *from) reg(1);
	unsigned int	(* predict_bidirectional)(uint8_t *from1, uint8_t *from2,
						  unsigned int *vmc1,
						  unsigned int *vmc2);

	stacked_frame	stack[MAX_B_SUCC];
	stacked_frame	last;

						/* frames encoded (coding order) */
	int		gop_frame_count;	/* .. in current GOP (display order) */
	int		seq_frame_count;	/* .. since last sequence header */

	double		skip_rate_acc;
	double		drop_timeout;
	double		time_per_frame;
	double		frames_per_sec;

	uint8_t *	oldref;			/* past reference frame buffer */

	bool		insert_gop_header;
	bool		closed_gop;		/* random access point, no fwd ref */
	bool		referenced;		/* by other P or B pictures */

	struct rc	rc;

	int		p_succ, p_dist;

	uint8_t *	banner;

	consumer	cons;

	int		mb_cx_row;
	int		mb_cx_thresh;

	int		motion_min;
	int		motion_max;

	int		coded_width;
	int		coded_height;

	int		frames_per_seqhdr;

	/* input */

	synchr_stream	sstr;
	double		coded_elapsed;

	/* Output */

	fifo *		fifo;
	producer	prod;
	double		coded_time;
	double		coded_frame_period;

	/* Options */

	rte_codec	codec;

	double		bit_rate;
	int		frame_rate_code;
	double		virtual_frame_rate;
	char *		gop_sequence;
	int		skip_method;
	bool		motion_compensation;
	bool		monochrome;
	char *		anno;

} video_context;

extern video_context vseg;

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
extern int		(* filter)(unsigned char *, unsigned char *);
extern const char *	filter_labels[];

extern int		video_frame_count;
extern int		video_frames_dropped;

// extern fifo *		video_fifo;
// extern pthread_t	video_thread_id;

extern void *		mpeg1_video_ipb(void *capture_fifo);

extern void		conv_init(int);
extern fifo *		v4l_init(void);
extern fifo *		v4l2_init(void);
extern fifo *		file_init(void);
extern void		filter_init(int pitch);
extern void		video_coding_size(int width, int height);
extern int		video_look_ahead(char *gop_sequence);

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
	CM_YUYV_EXP,
	CM_YUYV_EXP_VERTICAL_DECIMATION,
	CM_YUYV_EXP2,
	CM_YVU,
	CM_NUM_MODES
};

#endif /* VIDEO_H */
