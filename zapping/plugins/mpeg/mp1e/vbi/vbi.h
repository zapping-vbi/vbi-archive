/*
 *  MPEG-1 Real Time Encoder
 *
 *  Copyright (C) 1999-2001 Michael H. Schimek
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

/* $Id: vbi.h,v 1.6 2001-07-28 06:55:57 mschimek Exp $ */

#include "../common/types.h"
#include "../common/fifo.h"

struct vbi_context {
	fifo2		fifo;
};

/* pdc.c */

struct pdc_rec {
	int		cni;	/* country and network identifier */
	int		pil;	/* programme identification label */
	int		lci;	/* label channel identifier */
	int		luf;	/* label update flag */
	int		prf;	/* prepare-to-record flag */
	int		mi;	/* mode identifier */
};

extern void		pdc_update(struct pdc_rec *r);
extern void		decode_vps(unsigned char *buf);
extern void		decode_pdc(unsigned char *buf);

/* subtitles.c */

extern unsigned char	stuffing_packet[2][46];

extern int		init_dvb_packet_filter(char *s);
extern int		dvb_teletext_packet_filter(unsigned char *p,
				unsigned char *buf, int line,
				int magazine, int packet);
