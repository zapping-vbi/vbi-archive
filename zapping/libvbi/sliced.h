/*
 *  Copyright (C) 2000 Michael H. Schimek
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

/* $Id: sliced.h,v 1.3 2001-06-18 12:33:58 mschimek Exp $ */

/*
    Definition of sliced vbi data (vbi device interface),
    the container is fifo.h/buffer (one frame). vbi_sliced
    repeats buffer.used / sizeof(vbi_sliced) times, with
    increasing line numbers i.e. oldest samples first.
 */

/* Known data services */

#define SLICED_TELETEXT_B_L10_625	(1UL << 0)
#define SLICED_TELETEXT_B_L25_625	(1UL << 1)
#define SLICED_VPS			(1UL << 2)
#define SLICED_CAPTION_625_F1		(1UL << 3)
#define SLICED_CAPTION_625		(1UL << 4)
#define SLICED_CAPTION_525_F1		(1UL << 5)
#define SLICED_CAPTION_525		(1UL << 6)
#define SLICED_2xCAPTION_525		(1UL << 7)
#define SLICED_NABTS			(1UL << 8)
#define SLICED_TELETEXT_BD_525		(1UL << 9)
#define SLICED_WSS_625			(1UL << 10)
#define SLICED_WSS_CPR1204		(1UL << 11)
/* capture catch-all */
#define SLICED_VBI_625			(1UL << 30)
#define SLICED_VBI_525			(1UL << 31)

typedef struct {
	unsigned int		id;		/* set of SLICED_ */
	unsigned int		line;		/* ITU-R line number 1..n, 0: unknown */
	unsigned char		data[56];
} vbi_sliced;
