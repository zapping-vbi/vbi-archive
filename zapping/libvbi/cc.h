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

/* $Id: cc.h,v 1.6 2001-02-26 15:01:11 mschimek Exp $ */

#ifndef CC_H
#define CC_H

#include <pthread.h>

#include "format.h"
#include "../common/types.h"

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

	attr_char		attr;
	attr_char *		line;

	int			hidden;
	struct fmt_page		pg[2];
} channel;

struct caption {
	unsigned char		last[2];		/* field 1, cc command repetition */

	int			curr_chan;
	attr_char		transp_space[2];	/* caption, text mode */
	channel			channel[8];		/* caption 1-4, text 1-4 */
	pthread_mutex_t		mutex;

	bool			xds;
	xds_sub_packet		sub_packet[4][0x18];
	xds_sub_packet *	curr_sp;

	char			itv_buf[256];
	int			itv_count;
};

struct vbi; /* parent of struct caption */

extern void		vbi_init_caption(struct vbi *vbi);
extern void		vbi_caption_dispatcher(struct vbi *vbi, int line, unsigned char *buf);

#endif /* CC_H */
