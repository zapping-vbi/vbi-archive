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

/* $Id: video.h,v 1.22 2002-09-14 04:20:50 mschimek Exp $ */

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

#include "ratectl.h"

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

	uint8_t		seq_header_template[16];

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

//	int		quant_sum;

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





struct mpeg2_context { 
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

//	int		quant_sum;

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

extern struct vlc_rec	video_out video_align(32);

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
