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

/* $Id: mblock.c,v 1.1 2000-07-05 18:09:34 mschimek Exp $ */

#include "video.h"

int			mb_col, mb_row,			// current
    			mb_width, mb_height,
			mb_last_col, mb_last_row,
			mb_num;

short			mblock[7][6][8][8] __attribute__ ((aligned (CACHE_LINE)));
/*
 *  Buffer for current macroblock
 *  [7]    - intra, forward, backward, interpolated
 *  [6]    - Y0, Y2, Y1, Y3, Cb, Cr
 *  [8][8] - 16 bit unsigned samples/dct coefficients
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
