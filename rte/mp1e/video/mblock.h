/*
 *  MPEG-1 Real Time Encoder
 *
 *  Copyright (C) 1999-2000 Michael H. Schimek
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 2.
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

/* $Id: mblock.h,v 1.1.1.1 2001-08-07 22:10:09 garetxe Exp $ */

#ifndef MBLOCK_H
#define MBLOCK_H

#include "../common/bstream.h"
#include "../common/fifo.h"

extern int		mb_col, mb_row,
			mb_width, mb_height,
			mb_last_col, mb_last_row,
			mb_num;

extern unsigned char *	oldref;
extern unsigned char *	newref;

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

#define reset_mba()						\
do {								\
	mb_address.block[0].offset = 0;				\
	mb_address.block[4].offset = mb_address.chrom_0;	\
} while (0)

#define mba_col()						\
do {								\
	mb_address.block[0].offset += mb_address.col.lum;	\
	mb_address.block[4].offset += mb_address.col.chrom;	\
} while (0)

#define mba_row()						\
do {								\
	mb_address.block[0].offset += mb_address.row.lum;	\
	mb_address.block[4].offset += mb_address.row.chrom;	\
} while (0)

#endif // MBLOCK_H
