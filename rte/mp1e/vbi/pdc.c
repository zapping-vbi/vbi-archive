/*
 *  MPEG-1 Real Time Encoder
 *
 *  Copyright (C) 1999-2001 Michael H. Schimek
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

/* $Id: pdc.c,v 1.3 2001-08-22 01:28:09 mschimek Exp $ */

#include "../common/log.h"
#include "vbi.h"
#include "hamm.h"

/*
 *  ETS 300 231 -- Specification of the domestic video
 *  Programme Delivery Control system (PDC)
 */

#define PIL(day, mon, hour, min) \
	(((day) << 15) + ((mon) << 11) + ((hour) << 6) + ((min) << 0))

void
pdc_update(struct pdc_rec *r)
{
	printv(4, "PDC update cni=%04x pil=%08x lci=%d luf=%d prf=%d mi=%d\n",
		r->cni, r->pil, r->lci, r->luf, r->prf, r->mi);
}

void
decode_vps(unsigned char *buf)
{
	struct pdc_rec pdc;

	pdc.lci = 0;
	pdc.luf = 0;
	pdc.prf = 0;
	pdc.mi = 1;

	pdc.cni = + ((buf[10] & 3) << 10)
		  + ((buf[11] & 0xC0) << 2)
		  + ((buf[8] & 0xC0) << 0)
		  + (buf[11] & 0x3F);

	pdc.pil = ((buf[8] & 0x3F) << 14) + (buf[9] << 6) + (buf[10] >> 2);

	pdc_update(&pdc);
}

void
decode_pdc(unsigned char *buf)
{
	struct pdc_rec pdc;
	int i, t, err;

	/* ttx packet 8/30/2 */

	for (err = i = 0; i < 5; i++) {
		err |= t = hamm16(buf + i * 2 + 8);
		buf[i] = bit_reverse[t];
	}

	if (err < 0)
		return; /* hamming error */

	pdc.lci = (buf[0] >> 2) & 3;
	pdc.luf = !!(buf[0] & 2);
	pdc.prf = buf[0] & 1;
	pdc.mi = !!(buf[1] & 0x20);

	pdc.cni = + ((buf[4] & 0x03) << 10)
		  + ((buf[5] & 0xC0) << 2)
		  + (buf[2] & 0xC0)
		  + (buf[5] & 0x3F)
		  + ((buf[1] & 0x0F) << 12);

	if (pdc.cni == 0x0DC3)
		pdc.cni = (buf[2] & 0x10) ? 0x0DC2 : 0x0DC1;

	pdc.pil = ((buf[2] & 0x3F) << 14) 
	          + (buf[3] << 6) + (buf[4] >> 2);

	pdc_update(&pdc);
}
