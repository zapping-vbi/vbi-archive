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

/* $Id: filter.c,v 1.11 2002-10-02 02:13:48 mschimek Exp $ */

#include "../common/log.h"
#include "../common/mmx.h"
#include "../common/math.h"
#include "video.h"

const char		cbp_order[6] = { 5, 4, 3, 1, 2, 0 };

const char *
filter_labels[] = {
	"invalid",
	"YUV 4:2:0 fastest",
	"YUYV 4:2:2 fastest",
	"YUYV 4:2:2 w/vertical decimation",
	"YUYV 4:2:2 w/temporal interpolation", /* REMOVED */
	"YUYV 4:2:2 w/vertical interpolation",
	"YUYV 4:2:2 field progressive 50/60 Hz",
	"YUYV 4:2:2 50/60 Hz w/temporal interpolation", /* REMOVED */
	"YUV 4:2:0 w/vertical decimation",
	"YUYV 4:2:2 w/horizontal decimation",
	"YUYV 4:2:2 with 4:1 decimation",
	"YVU 4:2:0 fastest",
};

extern filter_fn	pmmx_YUV420_0;
extern filter_fn	pmmx_YUV420_2;
extern filter_fn	sse_YUV420_0;
extern filter_fn	sse_YUV420_2;
extern filter_fn	pmmx_YUYV_0;
extern filter_fn	pmmx_YUYV_1;
extern filter_fn	pmmx_YUYV_2;
extern filter_fn	pmmx_YUYV_3;
extern filter_fn	pmmx_YUYV_6;
extern filter_fn	sse_YUYV_0;
extern filter_fn	sse_YUYV_2;
extern filter_fn	sse_YUYV_6;

#if 0 /* rewrite */

static int (* color_pred)(unsigned char *, unsigned char *);

static int
color_trap(unsigned char *buffer1, unsigned char *buffer2)
{
	int r = color_pred(buffer1, buffer2);

	asm volatile (
		"\t movq c128,%%mm0;\n"
		"\t movq %%mm0,(%0);	movq %%mm0,1*8(%0);\n"
		"\t movq %%mm0,2*8(%0);	movq %%mm0,3*8(%0);\n"
		"\t movq %%mm0,4*8(%0);	movq %%mm0,5*8(%0);\n"
		"\t movq %%mm0,6*8(%0);	movq %%mm0,7*8(%0);\n"
		"\t movq %%mm0,8*8(%0);	movq %%mm0,9*8(%0);\n"
		"\t movq %%mm0,10*8(%0); movq %%mm0,11*8(%0);\n"
		"\t movq %%mm0,12*8(%0); movq %%mm0,13*8(%0);\n"
		"\t movq %%mm0,14*8(%0); movq %%mm0,15*8(%0);\n"
		"\t movq %%mm0,16*8(%0); movq %%mm0,17*8(%0);\n"
		"\t movq %%mm0,18*8(%0); movq %%mm0,19*8(%0);\n"
		"\t movq %%mm0,20*8(%0); movq %%mm0,21*8(%0);\n"
		"\t movq %%mm0,22*8(%0); movq %%mm0,23*8(%0);\n"
		"\t movq %%mm0,24*8(%0); movq %%mm0,25*8(%0);\n"
		"\t movq %%mm0,26*8(%0); movq %%mm0,27*8(%0);\n"
		"\t movq %%mm0,28*8(%0); movq %%mm0,29*8(%0);\n"
		"\t movq %%mm0,30*8(%0); movq %%mm0,31*8(%0);\n"
	:: "D" (&mblock[0][4][0][0]) : "cc", "memory" FPU_REGS);

	return r;
}

#endif

#ifndef BACKEND_MP1E

#include "../options.h"

/*
 *  Legacy mp1e code, not used with rte
 *
 *  Input:
 *  par->width, par->height (grab size, pixels)
 *  [encoded image] width, height (pixels)
 *  pitch (line distance, Y or YUYV, bytes)
 *
 *  Output:
 *  width, height (pixels)
 *  filter initialized
 */
void
filter_init(rte_video_stream_params *par, struct filter_param *fp)
{
	int padded_width, padded_height;
	int y_bpp = 2, scale_x = 1, scale_y = 1;
	int off_x, off_y;
	int uv_size = 0;
	int u = 4, v = 5;
	int sse;

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

	par->stride = par->width * 2;

	switch (filter_mode) {
	case CM_YVU:
		u = 5; v = 4;
	case CM_YUV:
		fp->func = sse ? sse_YUV420_0 : pmmx_YUV420_0;
		assert((par->width % 2) == 0);
		par->stride = par->width;
		par->uv_stride = par->width >> 1;
		uv_size = par->uv_stride * par->height / 2;
		y_bpp = 1;
		break;

	case CM_YUV_VERTICAL_DECIMATION:
		fp->func = sse ? sse_YUV420_2 : pmmx_YUV420_2;
		scale_y = 2;
		assert((par->width % 2) == 0);
		par->stride = par->width;
		par->uv_stride = par->width >> 1;
		uv_size = par->uv_stride * par->height / 2;
		y_bpp = 1;
		break;

	case CM_YUYV:
	case CM_YUYV_PROGRESSIVE:
		fp->func = sse ? sse_YUYV_0 : pmmx_YUYV_0;
		break;

/* removed	case CM_YUYV_EXP2: */
/* removed	case CM_YUYV_EXP_VERTICAL_DECIMATION: */
		FAIL("Sorry, the selected filter mode was experimental and is no longer available.\n");
		break;

	case CM_YUYV_VERTICAL_DECIMATION:
		fp->func = sse ? sse_YUYV_2 : pmmx_YUYV_2;
		scale_y = 2;
		break;

	case CM_YUYV_HORIZONTAL_DECIMATION:
		fp->func = pmmx_YUYV_1;
		scale_x = 2;
		break;

	case CM_YUYV_QUAD_DECIMATION:
		fp->func = pmmx_YUYV_3;
		scale_x = 2;
		scale_y = 2;
		break;

	case CM_YUYV_VERTICAL_INTERPOLATION:
		fp->func = sse ? sse_YUYV_6 : pmmx_YUYV_6;
		break;

	case CM_YUYV_TEMPORAL_INTERPOLATION:
	case CM_YUYV_PROGRESSIVE_TEMPORAL:
		FAIL("Sorry, the selected filter mode (temporal interpolation) is no longer available.\n");
		break;
	
	default:
		FAIL("Filter '%s' out of order",
			filter_labels[filter_mode]);
	}

	padded_width = ((width + 15) & -16) * scale_x;
	padded_height = ((height + 15) & -16) * scale_y;

	if (padded_width > par->width) {
		width = (par->width / scale_x) & -16;
		padded_width = width * scale_x;
	}
	if (padded_height > par->height) {
		height = (par->height / scale_y) & -16;
		padded_height = height * scale_y;
	}

	/* Center the encoding window */
	off_x = (par->width - width * scale_x + 1) >> 1;
	off_y = (par->height - height * scale_y + 1) >> 1;

	if (off_x + padded_width > par->width)
		off_x = par->width - padded_width;
	if (off_y + padded_height > par->height)
		off_y = par->height - padded_height;

	fp->stride	= par->stride;
	fp->uv_stride	= fp->stride >> 1;

	fp->offset	= par->stride * off_y + off_x * y_bpp;
	fp->u_offset    = uv_size * u + (fp->uv_stride * (off_y >> 1) + (off_x >> 1));
	fp->v_offset    = uv_size * v + (fp->uv_stride * (off_y >> 1) + (off_x >> 1));

	printv(2, "Filter '%s'\n", filter_labels[filter_mode]);
 /*
	if (luma_only) {
		color_pred = filter;
		filter = color_trap;
	}
 */
}

#endif /* !BACKEND_MP1E */
