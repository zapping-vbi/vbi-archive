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

/* $Id: filter.c,v 1.7 2001-06-23 02:50:44 mschimek Exp $ */

#include "../common/log.h"
#include "../common/mmx.h"
#include "../common/math.h"
#include "../options.h"
#include "video.h"

int			(* filter)(unsigned char *, unsigned char *);
bool			temporal_interpolation;

const char		cbp_order[6] = { 5, 4, 3, 1, 2, 0 };

const char *
filter_labels[] = {
	"invalid",
	"YUV 4:2:0 fastest",
	"YUYV 4:2:2 fastest",
	"YUYV 4:2:2 w/vertical decimation",
	"YUYV 4:2:2 w/temporal interpolation",
	"YUYV 4:2:2 w/vertical interpolation",
	"YUYV 4:2:2 field progressive 50/60 Hz",
	"YUYV 4:2:2 50/60 Hz w/temporal interpolation",
	"YVU 4:2:0 fastest",
	"",
	"",
	"",
};

/* static */ int	filter_y_offs,
			filter_u_offs,
			filter_v_offs,
			filter_y_pitch;

extern int		mmx_YUV_420(unsigned char *, unsigned char *);
extern int		mmx_YUYV_422(unsigned char *, unsigned char *);
extern int		mmx_YUYV_422_2v(unsigned char *, unsigned char *);
extern int		mmx_YUYV_422_ti(unsigned char *, unsigned char *);
extern int		mmx_YUYV_422_vi(unsigned char *, unsigned char *);

/* Reference */

int
YUYV_422(unsigned char *buffer, unsigned char *unused)
{
	int y, x;
	unsigned int n, s = 0, s2 = 0;

	buffer += filter_y_pitch * mb_row * 16 + mb_col * 16 * 2 + filter_y_offs;

	for (y = 0; y < 16; y++)
		for (x = 0; x < 8; x++) {
			// Note block order Y0 Y2 Y1 Y3
			mblock[0][0][y][x] = (short) buffer[y * filter_y_pitch + x * 2 + 0];
			mblock[0][2][y][x] = (short) buffer[y * filter_y_pitch + x * 2 + 16];
		}

	for (y = 0; y < 8; y++)
		for (x = 0; x < 8; x++) {
			mblock[0][4][y][x] = (short) buffer[y * filter_y_pitch * 2 + x * 4 + 1];
			mblock[0][5][y][x] = (short) buffer[y * filter_y_pitch * 2 + x * 4 + 3];
		}

	for (x = 0; x < 4 * 64; x++) {
		n = mblock[0][0][0][x];
		s += n;
		s2 += n * n;
	}

	return s2 * 256 - (s * s); // luma spatial activity
}

static int (* color_pred)(unsigned char *, unsigned char *);

/* Hum. Could add rendered subpictures. */

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





/* Experimental low pass filter */

int
YUYV_422_exp1(unsigned char *buffer, unsigned char *unused)
{
	static const char
	f[5][5] = {
		{ 1,  3,  4,  3, 1 },
		{ 3,  9, 12,  9, 3 },
		{ 4, 12, 16, 12, 4 },
		{ 3,  9, 12,  9, 3 },
		{ 1,  3,  4,  3, 1 },
	};
	unsigned int n, s = 0, s2 = 0;
	int y, x;
	int i, j;

//	if (mb_row <= 0 || mb_row >= mb_last_row)
//		return mmx_YUYV_422(buffer, NULL);

	buffer += filter_y_pitch * mb_row * 16 + mb_col * 16 * 2 + filter_y_offs;

	for (y = 0; y < 16; y++)
		for (x = 0; x < 8; x++) {
			n = 0;
			for (j = 0; j < 5; j++)
				for (i = 0; i < 5; i++)
					n += buffer[(y + j) * filter_y_pitch + (x + i) * 2] * f[j][i];
			mblock[0][0][y][x] = (n + 72) / 144;
			n = 0;
			for (j = 0; j < 5; j++)
				for (i = 0; i < 5; i++)
					n += buffer[(y + j) * filter_y_pitch + (x + i) * 2 + 16] * f[j][i];
			mblock[0][2][y][x] = (n + 72) / 144;
		}

	for (y = 0; y < 8; y++)
		for (x = 0; x < 8; x++) {
			mblock[0][4][y][x] = (short) buffer[y * filter_y_pitch * 2 + x * 4 + 1];
			mblock[0][5][y][x] = (short) buffer[y * filter_y_pitch * 2 + x * 4 + 3];
		}

	for (x = 0; x < 4 * 64; x++) {
		n = mblock[0][0][0][x];
		s += n;
		s2 += n * n;
	}

	return s2 * 256 - (s * s);
}


/* Experimental low pass filter */

int
YUYV_422_exp2(unsigned char *buffer, unsigned char *buffer2)
{
	unsigned int n, s = 0, s2 = 0;
	int y, x;

	x = mmx_YUYV_422(buffer, buffer2);
//	x = mmx_YUYV_422_ti(buffer, buffer2);

//	if (mb_row <= 0 || mb_row >= mb_last_row)
//		return x;
	if (x < 65536 * 128)
		return x;

	buffer += filter_y_pitch * mb_row * 16 + mb_col * 16 * 2 + filter_y_offs;
//	buffer2 += filter_y_pitch * mb_row * 16 + mb_col * 16 * 2 + filter_y_offs;

	for (y = 0; y < 16; y++)
		for (x = 0; x < 8; x++) {
			n =	buffer[(y - 1) * filter_y_pitch + (x - 1) * 2] +
				buffer[(y - 1) * filter_y_pitch + (x + 1) * 2] +
				buffer[(y + 1) * filter_y_pitch + (x - 1) * 2] +
				buffer[(y + 1) * filter_y_pitch + (x + 1) * 2];
			n +=   (buffer[(y - 1) * filter_y_pitch + (x + 0) * 2] +
				buffer[(y + 1) * filter_y_pitch + (x + 0) * 2] +
				buffer[(y + 0) * filter_y_pitch + (x - 1) * 2] +
				buffer[(y + 0) * filter_y_pitch + (x + 1) * 2]) * 2;
			n +=	buffer[(y + 0) * filter_y_pitch + (x + 0) * 2] * 4;
			mblock[0][0][y][x] = (n + 8) >> 4;
			n =	buffer[(y - 1) * filter_y_pitch + (x - 1) * 2 + 16] +
				buffer[(y - 1) * filter_y_pitch + (x + 1) * 2 + 16] +
				buffer[(y + 1) * filter_y_pitch + (x - 1) * 2 + 16] +
				buffer[(y + 1) * filter_y_pitch + (x + 1) * 2 + 16];
			n +=   (buffer[(y - 1) * filter_y_pitch + (x + 0) * 2 + 16] +
				buffer[(y + 1) * filter_y_pitch + (x + 0) * 2 + 16] +
				buffer[(y + 0) * filter_y_pitch + (x - 1) * 2 + 16] +
				buffer[(y + 0) * filter_y_pitch + (x + 1) * 2 + 16]) * 2;
			n +=	buffer[(y + 0) * filter_y_pitch + (x + 0) * 2 + 16] * 4;
			mblock[0][2][y][x] = (n + 8) >> 4;
		}

//	mblock[0][0][0][0] = 0;

	for (y = 0; y < 8; y++)
		for (x = 0; x < 8; x++) {
			n =	buffer[(y - 1) * filter_y_pitch * 2 + (x - 1) * 4 + 1] +
				buffer[(y - 1) * filter_y_pitch * 2 + (x + 1) * 4 + 1] +
				buffer[(y + 1) * filter_y_pitch * 2 + (x - 1) * 4 + 1] +
				buffer[(y + 1) * filter_y_pitch * 2 + (x + 1) * 4 + 1];
			n +=   (buffer[(y - 1) * filter_y_pitch * 2 + (x + 0) * 4 + 1] +
				buffer[(y + 1) * filter_y_pitch * 2 + (x + 0) * 4 + 1] +
				buffer[(y + 0) * filter_y_pitch * 2 + (x - 1) * 4 + 1] +
				buffer[(y + 0) * filter_y_pitch * 2 + (x + 1) * 4 + 1]) * 2;
			n +=	buffer[(y + 0) * filter_y_pitch * 2 + (x + 0) * 4 + 1] * 4;
			mblock[0][4][y][x] = (n + 8) >> 4;
			n =	buffer[(y - 1) * filter_y_pitch * 2 + (x - 1) * 4 + 3] +
				buffer[(y - 1) * filter_y_pitch * 2 + (x + 1) * 4 + 3] +
				buffer[(y + 1) * filter_y_pitch * 2 + (x - 1) * 4 + 3] +
				buffer[(y + 1) * filter_y_pitch * 2 + (x + 1) * 4 + 3];
			n +=   (buffer[(y - 1) * filter_y_pitch * 2 + (x + 0) * 4 + 3] +
				buffer[(y + 1) * filter_y_pitch * 2 + (x + 0) * 4 + 3] +
				buffer[(y + 0) * filter_y_pitch * 2 + (x - 1) * 4 + 3] +
				buffer[(y + 0) * filter_y_pitch * 2 + (x + 1) * 4 + 3]) * 2;
			n +=	buffer[(y + 0) * filter_y_pitch * 2 + (x + 0) * 4 + 3] * 4;
			mblock[0][5][y][x] = (n + 8) >> 4;
		}

	for (x = 0; x < 4 * 64; x++) {
		n = mblock[0][0][0][x];
		s += n;
		s2 += n * n;
	}

	return s2 * 256 - (s * s);
}

/* Experimental low pass filter */

int
YUYV_422_exp3(unsigned char *buffer, unsigned char *buffer2)
{
	static unsigned char temp[19 * 40];
	unsigned int n, s = 0, s2 = 0;
	int y, x;

	buffer += filter_y_pitch * (mb_row * 32 - 1) + mb_col * 16 * 2 + filter_y_offs;
	buffer2 += filter_y_pitch * (mb_row * 32 - 1) + mb_col * 16 * 2 + filter_y_offs;

	for (y = 0; y < 19; y++) {
		for (x = 0; x < 40; x++)
			temp[y * 40 + x] = (buffer[x - 4] + buffer2[x - 4] + 1) >> 1;
		buffer += filter_y_pitch * 2;
		buffer2 += filter_y_pitch * 2;
	}

	for (y = 0; y < 16; y++)
		for (x = 0; x < 8; x++) {
			n =	temp[(y + 0) * 40 + (x + 0) * 2] +
				temp[(y + 0) * 40 + (x + 2) * 2] +
				temp[(y + 2) * 40 + (x + 0) * 2] +
				temp[(y + 2) * 40 + (x + 2) * 2];
			n +=   (temp[(y + 0) * 40 + (x + 1) * 2] +
				temp[(y + 2) * 40 + (x + 1) * 2] +
				temp[(y + 1) * 40 + (x + 0) * 2] +
				temp[(y + 1) * 40 + (x + 2) * 2]) * 2;
			n +=	temp[(y + 1) * 40 + (x + 1) * 2] * 4;
			mblock[0][0][y][x] = (n + 8) >> 4;
			n =	temp[(y + 0) * 40 + (x + 0) * 2 + 16] +
				temp[(y + 0) * 40 + (x + 2) * 2 + 16] +
				temp[(y + 2) * 40 + (x + 0) * 2 + 16] +
				temp[(y + 2) * 40 + (x + 2) * 2 + 16];
			n +=   (temp[(y + 0) * 40 + (x + 1) * 2 + 16] +
				temp[(y + 2) * 40 + (x + 1) * 2 + 16] +
				temp[(y + 1) * 40 + (x + 0) * 2 + 16] +
				temp[(y + 1) * 40 + (x + 2) * 2 + 16]) * 2;
			n +=	temp[(y + 1) * 40 + (x + 1) * 2 + 16] * 4;
			mblock[0][2][y][x] = (n + 8) >> 4;
		}

//	mblock[0][0][0][0] = 0;

	for (y = 0; y < 8; y++)
		for (x = 0; x < 8; x++) {
			n =	temp[(y + 0) * 40 * 2 + (x + 0) * 4 + 1] +
				temp[(y + 0) * 40 * 2 + (x + 2) * 4 + 1] +
				temp[(y + 2) * 40 * 2 + (x + 0) * 4 + 1] +
				temp[(y + 2) * 40 * 2 + (x + 2) * 4 + 1];
			n +=   (temp[(y + 0) * 40 * 2 + (x + 1) * 4 + 1] +
				temp[(y + 2) * 40 * 2 + (x + 1) * 4 + 1] +
				temp[(y + 1) * 40 * 2 + (x + 0) * 4 + 1] +
				temp[(y + 1) * 40 * 2 + (x + 2) * 4 + 1]) * 2;
			n +=	temp[(y + 1) * 40 * 2 + (x + 1) * 4 + 1] * 4;
			mblock[0][4][y][x] = (n + 8) >> 4;
			n =	temp[(y + 0) * 40 * 2 + (x + 0) * 4 + 3] +
				temp[(y + 0) * 40 * 2 + (x + 2) * 4 + 3] +
				temp[(y + 2) * 40 * 2 + (x + 0) * 4 + 3] +
				temp[(y + 2) * 40 * 2 + (x + 2) * 4 + 3];
			n +=   (temp[(y + 0) * 40 * 2 + (x + 1) * 4 + 3] +
				temp[(y + 2) * 40 * 2 + (x + 1) * 4 + 3] +
				temp[(y + 1) * 40 * 2 + (x + 0) * 4 + 3] +
				temp[(y + 1) * 40 * 2 + (x + 2) * 4 + 3]) * 2;
			n +=	temp[(y + 1) * 40 * 2 + (x + 1) * 4 + 3] * 4;
			mblock[0][5][y][x] = (n + 8) >> 4;
		}

	for (x = 0; x < 4 * 64; x++) {
		n = mblock[0][0][0][x];
		s += n;
		s2 += n * n;
	}

	return s2 * 256 - (s * s);
}

/* Experimental ??? filter */

int
YUYV_422_exp4(unsigned char *buffer, unsigned char *unused)
{
	unsigned int n, c, d, r, s = 0, s2 = 0;
	int y, x, i, j;

	buffer += filter_y_pitch * mb_row * 16 + mb_col * 16 * 2 + filter_y_offs;

	for (y = 0; y < 16; y++)
		for (x = 0; x < 8; x++) {
			n = c = 0;
			r = buffer[(y) * filter_y_pitch + (x) * 2];
			for (j = -2; j < +2; j++)
				for (i = -2; i < +2; i++) {
					d = buffer[(y + j) * filter_y_pitch + (x + i) * 2];
					if (40 >= nbabs(d - r)) {
						n += d;
						c++;
					}
				}
			mblock[0][0][y][x] = (n + (c >> 1)) / c;
			n = c = 0;
			r = buffer[(y) * filter_y_pitch + (x) * 2 + 16];
			for (j = -2; j < +2; j++)
				for (i = -2; i < +2; i++) {
					d = buffer[(y + j) * filter_y_pitch + (x + i) * 2 + 16];
					if (40 >= nbabs(d - r)) {
						n += d;
						c++;
					}
				}
			mblock[0][2][y][x] = (n + (c >> 1)) / c;
		}

	for (y = 0; y < 8; y++)
		for (x = 0; x < 8; x++) {
			mblock[0][4][y][x] = (short) buffer[y * filter_y_pitch * 2 + x * 4 + 1];
			mblock[0][5][y][x] = (short) buffer[y * filter_y_pitch * 2 + x * 4 + 3];
		}

	for (x = 0; x < 4 * 64; x++) {
		n = mblock[0][0][0][x];
		s += n;
		s2 += n * n;
	}

	return s2 * 256 - (s * s);
}

void
filter_init(int pitch)
{
	int outer_width, outer_height;
	int y_bpp = 2, scale_y = 1;
	int off_x, off_y;
	int uv_size = 0;
	int u = 4, v = 5;

	temporal_interpolation = FALSE;
	filter_y_pitch = pitch;

	switch (filter_mode) {
	case CM_YVU:
		u = 5; v = 4;
	case CM_YUV:
		filter = mmx_YUV_420;
		uv_size = width * height / 4;
		y_bpp = 1;
		break;
	case CM_YUYV:
	case CM_YUYV_PROGRESSIVE:
		filter = mmx_YUYV_422;
		break;

	case CM_YUYV_EXP:
		filter = YUYV_422_exp2;
		temporal_interpolation = FALSE;
		width = saturate(grab_width, 1, grab_width - 16);
		height = saturate(grab_height, 1, grab_height - 16);
		break;

	case CM_YUYV_EXP2:
		filter = YUYV_422_exp4;
		temporal_interpolation = FALSE;
		width = saturate(grab_width, 1, grab_width - 16);
		height = saturate(grab_height, 1, grab_height - 16);
		break;

	case CM_YUYV_EXP_VERTICAL_DECIMATION:
		filter = YUYV_422_exp3;
		temporal_interpolation = TRUE;
		scale_y = 2;
		width = saturate(grab_width, 1, grab_width - 16);
		height = saturate(grab_height / 2, 1, grab_height / 2 - 16);
		break;

	case CM_YUYV_VERTICAL_DECIMATION:
		filter = mmx_YUYV_422_2v;
		scale_y = 2;
		break;

	case CM_YUYV_VERTICAL_INTERPOLATION:
		filter = mmx_YUYV_422_vi;
		break;

	case CM_YUYV_TEMPORAL_INTERPOLATION:
	case CM_YUYV_PROGRESSIVE_TEMPORAL:
		filter = mmx_YUYV_422_ti;
		temporal_interpolation = TRUE;
		break;
	
	default:
		FAIL("Filter '%s' out of order",
			filter_labels[filter_mode]);
	}

	outer_width = (width + 15) >> 4;
	outer_height = (height + 15) >> 4;

	off_x = (grab_width - width + 1) >> 1;
	off_y = (grab_height - height * scale_y + 1) >> 1;

	/*
	 *  Need a clipping mechanism (or padded buffers?), currently
	 *  all memory accesses as 16 x 16 mblocks. Step #2: clear outside
	 *  blocks to all zero and all outside samples to average of
	 *  inside samples (for prediction and FDCT).
	 */
	if (off_x + outer_width > grab_width)
		off_x = grab_width - outer_width;
	if (off_y + outer_height * scale_y > grab_height)
		off_y = grab_height - outer_height;

	filter_y_offs = filter_y_pitch * off_y + off_x * y_bpp; 
	filter_v_offs = filter_u_offs = filter_y_offs / 4;

	filter_u_offs += uv_size * u;
	filter_v_offs += uv_size * v;

	printv(2, "Filter '%s'\n", filter_labels[filter_mode]);

	if (luma_only) {
		color_pred = filter;
		filter = color_trap;
	}
}
