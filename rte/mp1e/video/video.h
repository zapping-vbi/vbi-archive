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

/* $Id: video.h,v 1.4 2001-09-23 21:04:25 mschimek Exp $ */

#include "../common/bstream.h"
#include "../common/fifo.h"
#include "mblock.h"

#include "libvideo.h"

#define MAX_WIDTH 1024			/* 1 ... 4096 */
#define MAX_HEIGHT 1024			/* 1 ... 2800 */

typedef struct video_context {

	/* Options */

	rte_codec		codec;

} video_context;












#define video_align(n) __attribute__ ((aligned (n)))

extern struct bs_rec	video_out video_align(32);

extern int		frame_rate_code;
extern int		dropped;
extern int		(* filter)(unsigned char *, unsigned char *);
extern const char *	filter_labels[];

extern int		video_frame_count;
extern int		video_frames_dropped;

extern fifo *		video_fifo;
extern pthread_t	video_thread_id;

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
