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

/* $Id: mblock.c,v 1.3 2000-09-25 17:08:57 mschimek Exp $ */

#include "video.h"
#include "../common/math.h"

int			mb_col, mb_row,			// current
    			mb_width, mb_height,
			mb_last_col, mb_last_row,
			mb_num;

unsigned char *		oldref;				// past reference frame buffer
unsigned char *		newref;				// future reference frame buffer
/*
 *  Packet reference buffer format is
 *  [mb_height]
 *  [mb_width]  - for all macroblocks of a frame
 *  [6]         - Y0, Y2, Y1, Y3, Cb, Cr
 *  [8][8]      - 8 bit unsigned samples, e. g. according to ITU-R Rec. 601
 */

struct mb_addr		mb_address __attribute__ ((aligned (MIN(CACHE_LINE, 64))));

short			mblock[7][6][8][8] __attribute__ ((aligned (4096)));
/*
 *  Buffer for current macroblock
 *  [7]    - intra, forward, backward, interpolated
 *  [6]    - Y0, Y2, Y1, Y3, Cb, Cr
 *  [8][8] - samples, block difference, dct coefficients
 */

void
video_coding_size(int width, int height)
{
	mb_width  = (saturate(width, 1, MAX_WIDTH) + 15) >> 4;
	mb_height = (saturate(height, 1, MAX_HEIGHT) + 15) >> 4;

	mb_last_col = mb_width - 1;
	mb_last_row = mb_height - 1;

	mb_num    = mb_width * mb_height;
}
