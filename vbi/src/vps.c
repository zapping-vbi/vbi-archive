/*
 *  libzvbi - Video Programming System
 *
 *  Copyright (C) 2000-2004 Michael H. Schimek
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

/* $Id: vps.c,v 1.1.2.2 2004-02-25 17:28:11 mschimek Exp $ */

#include "../config.h"

#include <stdio.h>

#include "hamm.h"
#include "vps.h"

#include "vt.h" /* vbi_decoder */

/**
 * @addtogroup VPS Video Programm System Decoder
 * @ingroup LowDec
 */

/**
 * @param cni CNI of type VBI_CNI_TYPE_VPS is stored here.
 * @param buffer Last 13 bytes of VPS datagram, as
 *   in struct vbi_sliced.
 * 
 * Decodes a VPS datagram according to
 * ETS 300 231, returning the 12 bit Country and
 * Network Identifier in @a cni.
 *
 * @returns
 * Always @c TRUE, no error checking possible.
 */
vbi_bool
vbi_decode_vps_cni		(unsigned int *		cni,
				 const uint8_t		buffer[13])
{
	unsigned int cni_value;

	assert (NULL != cni);
	assert (NULL != buffer);

	cni_value = (+ ((buffer[10] & 0x03) << 10)
		     + ((buffer[11] & 0xC0) << 2)
		     +  (buffer[ 8] & 0xC0)
		     +  (buffer[11] & 0x3F));

	/* From TR 101 231: "NOTE: As this code is used for a time
	   in two networks a distinction for automatic tuning systems
	   is given in data line 16 [VPS]: bit 3 of byte 5 = 1 for
	   the ARD network / = 0 for the ZDF network."
	
	   This note refers to the common morning program of ARD/ZDF.
           Also, being a union of regional public broadcasters ARD seems
           to send the CNI of the contributor of a program (such as
	   WDR). A more reliable method of station identification might
	   be to examine the Teletext header. */

	if (0x0DC3 == cni_value)
		cni_value = (buffer[2] & 0x10) ?
			0x0DC2 /* ZDF */ : 0x0DC1 /* ARD */;

	*cni = cni_value;

	return TRUE;
}

/**
 * @param pi PDC data is stored here.
 * @param buffer Last 13 bytes of VPS datagram, as
 *   in struct vbi_sliced.
 * 
 * Decodes a VPS datagram according to ETS 300 231,
 * storing PDC recording-control data in @a pi.
 *
 * @returns
 * Always @c TRUE, no error checking possible.
 */
vbi_bool
vbi_decode_vps_pdc		(vbi_program_id *	pi,
				 const uint8_t		buffer[13])
{
	unsigned int cni;

	assert (NULL != pi);
	assert (NULL != buffer);

	vbi_decode_vps_cni (&cni, buffer);

	pi->nuid	= vbi_nuid_from_cni (VBI_CNI_TYPE_VPS, cni);

	pi->pil		= (+ ((buffer[ 8] & 0x3F) << 14)
			   +  (buffer[ 9] << 6)
			   +  (buffer[10] >> 2));

	pi->month	= VBI_PIL_MONTH (pi->pil) - 1; 
	pi->day		= VBI_PIL_DAY (pi->pil) - 1; 
	pi->hour	= VBI_PIL_HOUR (pi->pil); 
	pi->minute	= VBI_PIL_MINUTE (pi->pil); 

	pi->length	= 0; /* unknown */

	pi->lci		= 0; /* just one label channel */
	pi->luf		= FALSE; /* no update, just pil */
	pi->mi		= FALSE; /* label is not 30 s early */
	pi->prf		= FALSE; /* prepare to record unknown */

	pi->pcs_audio	= buffer[ 2] >> 6;
	pi->pty		= buffer[12];

	pi->tape_delayed = FALSE;

	return TRUE;
}

/**
 * @internal
 * @param vbi Initialized vbi decoding context.
 * @param buf 13 bytes.
 * 
 * Decode a VPS datagram (13 bytes) according to
 * ETS 300 231 and update decoder state. This may
 * send a @a VBI_EVENT_NETWORK.
 */
extern void
vbi_decode_vps			(vbi_decoder *		vbi,
				 const uint8_t		buffer[13])
{
	vbi_network *n = &vbi->network.ev.network;
	const char *country, *name;
	int cni;

	cni = + ((buffer[10] & 3) << 10)
	      + ((buffer[11] & 0xC0) << 2)
	      + ((buffer[8] & 0xC0) << 0)
	      + (buffer[11] & 0x3F);

	if (cni == 0x0DC3)
		cni = (buffer[2] & 0x10) ? 0x0DC2 : 0x0DC1;

	if (cni != n->cni_vps) {
		n->cni_vps = cni;
		n->cycle = 1;
	} else if (n->cycle == 1) {
		vbi_nuid nuid;

		nuid = vbi_nuid_from_cni (VBI_CNI_TYPE_VPS, cni);

		if (nuid != n->nuid) {
#if 0 // FIXME
			if (n->nuid != 0) {
				vbi_chsw_reset(vbi, nuid);
			}

			vbi_network_from_nuid (n, nuid);
			n->cycle = 1;

			vbi->network.type = VBI_EVENT_NETWORK;
			vbi_send_event(vbi, &vbi->network);
#endif
		}

		n->cycle = 2;
	}

#define BSDATA_TEST 0
	if (BSDATA_TEST && 0) {
		static char pr_label[20];
		static char label[20];
		static int l = 0;
		int cni, pcs, pty, pil;
		int c, j;

		printf("\nVPS:\n");

		printf(" 3-10: %02x %02x %02x %02x %02x %02x %02x %02x\n",
		       buffer[0], buffer[1], buffer[2], buffer[3],
		       buffer[4], buffer[5], buffer[6], buffer[7]);
		vbi_decode_vps_label (buffer);

		cni = + ((buffer[10] & 3) << 10)
		      + ((buffer[11] & 0xC0) << 2)
		      + ((buffer[8] & 0xC0) << 0)
		      + (buffer[11] & 0x3F);
#warning
#if 0
		if (cni)
			for (j = 0; vbi_cni_table[j].name; j++)
				if (vbi_cni_table[j].cni4 == cni) {
					printf(" Country: %s\n Station: %s%s\n",
						vbi_country_names_en[vbi_cni_table[j].country],
						vbi_cni_table[j].name,
						(cni == 0x0DC3) ? ((buffer[2] & 0x10) ? " (ZDF)" : " (ARD)") : "");
					break;
				}
#endif
		pcs = buffer[2] >> 6;
		pil = ((buffer[8] & 0x3F) << 14) + (buffer[9] << 6) + (buffer[10] >> 2);
		pty = buffer[12];

		/* if (!cni || !vbi_cni_table[j].name) */
			printf(" CNI: %04x\n", cni);
#if BSDATA_TEST
		printf(" Analog audio: %s\n", pcs_names[pcs]);

		dump_pil(pil);
		dump_pty(pty);
#endif
	}
}
