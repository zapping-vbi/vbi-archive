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

/* $Id: video.h,v 1.1 2000-07-05 18:09:34 mschimek Exp $ */

#include "../bstream.h"
#include "../fifo.h"

#define MAX_WIDTH 1024			// 1 ... 4096
#define MAX_HEIGHT 1024			// 1 ... 2800

#define video_align(n) __attribute__ ((aligned (n)))

extern struct bs_rec	video_out video_align(32);

extern int		mb_col, mb_row,
			mb_width, mb_height,
			mb_last_col, mb_last_row,
			mb_num;

extern short		mblock[7][6][8][8] video_align(4096);

extern int		frame_rate_code;
extern int		dropped;
extern int		(* filter)(unsigned char *, unsigned char *);
extern const char *	filter_labels[];

extern int		video_frame_count;
extern int		video_frames_dropped;

extern fifo		vid;
extern pthread_t	video_thread_id;

extern void *		video_compression_thread(void *unused);

extern void		(* video_start)(void);
extern unsigned char *	(* video_wait_frame)(double *, int *);
extern void		(* video_frame_done)(int);
extern void		(* video_unget_frame)(int);

extern void		conv_init(int);
extern void		v4l_init(void);
extern void		v4l2_init(void);
extern void		file_init(void);
extern void		filter_init(int pitch);
extern void		video_coding_size(int width, int height);

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
	CM_NUM_MODES
};
