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

/* $Id: mblock.c,v 1.6 2002-10-02 02:13:48 mschimek Exp $ */

#include "video.h"
#include "../common/math.h"

int			mb_col, mb_row,			// current
    			mb_width, mb_height,
			mb_last_col, mb_last_row,
			mb_num;

uint8_t * newref;			/* future reference frame buffer */

/*
 *  Packed reference buffer format is
 *  [mb_height]
 *  [mb_width]  - for all macroblocks of a frame/field
 *  [6]         - Y0, Y2, Y1, Y3, Cb, Cr
 *  [8][8]      - 8 bit unsigned samples, e. g. according to ITU-R Rec. 601
 */

struct mb_addr		mb_address __attribute__ ((aligned (MIN(CACHE_LINE, 64))));

int16_t			mblock[7][6][8][8] __attribute__ ((aligned (4096)));
/*
 *  Buffer for current macroblock
 *  [7]    - intra, forward, backward, interpolated
 *  [6]    - Y0, Y2, Y1, Y3, Cb, Cr
 *  [8][8] - samples, block difference, dct coefficients
 */

void
video_coding_size(int width, int height, rte_bool field)
{
	mb_width = (saturate(width, 1, MAX_WIDTH) + 15) >> 4;

	if (field)
		mb_height = (saturate(height, 1, MAX_HEIGHT) + 31) >> 5;
	else
		mb_height = (saturate(height, 1, MAX_HEIGHT) + 15) >> 4;

	mb_last_col = mb_width - 1;
	mb_last_row = mb_height - 1;

	mb_num = mb_width * mb_height;
}

/*
 *  B picture: encode & discard; I or P picture must be encoded ahead of
 *  all B pictures forward referencing the I or P picture, ie. we will
 *  stack as many captured pictures as there are B pictures in a row
 *  plus the following I or P. The capture module may add one or two
 *  more for double buffering.
 */
int
video_look_ahead(char *gop_sequence)
{
	int i;
	int max = 0;
	int count = 0;

	for (i = 0; i < 1024; i++)
		switch (gop_sequence[i]) {
		case 'I':
		case 'P':
			max = MAX(count, max);
			count = 0;
			break;

		case 'B':
			count++;
			break;

		default:
			i = 1024;
		}

	return MAX(count, max) + 1;
}

/*
 *  V4l/v4l2 don't tell us about the sample aspect, we have to
 *  guess from the capture dimensions and frame rate.
 *  To complicate things yet more MPEG-1/2 define sample aspect
 *  in terms of display aspect (4:3, 16:9) instead of sampling
 *  frequency. So these values may seem wrong, but are correct
 *  in context of MPEG.
 */
double
video_sampling_aspect 		(double			frame_rate,
				 unsigned int		width,
				 unsigned int		height)
{
	const double DAR = 3 / 4.0;
	double magic = DAR / 1.0; /* crop 100 % */

	if (frame_rate > 29.0) {
		if (                   width <= 160
		    || (width > 176 && width <= 320)
		    || (width > 352 && width <= 640)
		    || width > 720) {
			/* SAR = 1.0; */
		} else { /* MPEG's idea of ITU-R Rec 601 NTSC (not 11/10) */
			/* SAR = 3 / 4 * 720 / 480; */

			if (width <= 704) {
				magic = DAR / (704.0 / 720); /* 98 % */
			}
		}
	} else {
		if (width <= 176
		    || (width > 192 && width <= 352)
		    || (width > 384 && width <= 720)) {
			/* MPEG's idea of ITU-R Rec 601 PAL/SECAM (not 54/59) */
			/* SAR = 3 / 4 * 720 / 576; */

			if (width != 480 && width <= 704) {
				magic = DAR / (704.0 / 720); /* 98 % */
			}
		} else {
			/* SAR = 1.0; */
		}
	}

	/*
	 *  We assume the image is scaled from DAR 4:3 (not eg. 16:9),
	 *  with aspect width / (SAR * crop) * DAR / height. Hence eg.
	 *
	 *  352x288 -> .9375	352x144 -> .9375 * 2
	 *  720x576 -> .9375	720x144 -> .9375 * 4
	 *  384x288 -> 1	768x288 -> 2
	 *  704x480 -> 1.125	352x480 -> 1.125 / 2
	 *  640x240 -> 2	480x480 -> .75
	 *  480x576 -> .625
	 *
	 *  Remember this is the grabbed size, not the encoded size. When
	 *  720x432 from 720x576 -> SAR = .9375, DAR = SAR * h / w = 9:16.
	 *  SARs and DARs will be rounded to the closest encodable in any
	 *  case.
	 */
	return width * magic / height;
}



