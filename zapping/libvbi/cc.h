/*
 *  Closed Caption decoder
 *
 *  Copyright (C) 2000-2001 Michael H. Schimek
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

/* $Id: cc.h,v 1.1 2001-02-19 07:23:02 mschimek Exp $ */

#ifndef CC_H
#define CC_H

#include "format.h"

typedef struct {
	int			count;
	int			chksum;
	char			buffer[32];
} xds_sub_packet;

#define CODE_PAGE		(8 * 256)
#define CC_PAGE_BASE		1

typedef enum {
	MODE_NONE,
	MODE_POP_ON,
	MODE_PAINT_ON,
	MODE_ROLL_UP,
	MODE_TEXT
} mode;

typedef struct {
	mode			mode;

	int			col, col1;
	int			row, row1;
	int			roll;

	int			nul_ct;
// XXX should be 'silence count'

	bool			redraw_all;
	bool			italic;

	attr_char		attr;
	attr_char *		line;

	struct fmt_page		pg;
} channel;

struct caption {
	unsigned char		last[2];		/* field 1, cc command repetition */

	int			curr_chan;
	attr_char		transp_space[2];	/* caption, text mode */
	channel			channel[8];		/* caption 1-4, text 1-4 */

	bool			xds;
	xds_sub_packet		sub_packet[4][0x18];
	xds_sub_packet *	curr_sp;

	char			itv_buf[256];
	int			itv_count;
};

extern void		vbi_init_caption(struct caption *cc);

#endif /* CC_H */
