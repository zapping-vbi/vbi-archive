/*
 *  libzvbi - Video Program System
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

/* $Id: vps.c,v 1.1.2.4 2004-04-05 04:42:27 mschimek Exp $ */

#include "../config.h"

#include <assert.h>
#include "network.h"		/* vbi_nuid_from_cni() */
#include "vps.h"

/**
 * @addtogroup VPS Video Program System Decoder
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
 * @param pid PDC data is stored here.
 * @param buffer Last 13 bytes of VPS datagram, as
 *   in struct vbi_sliced.
 * 
 * Decodes a VPS datagram according to ETS 300 231,
 * storing PDC recording-control data in @a pid.
 *
 * @returns
 * Always @c TRUE, no error checking possible.
 */
vbi_bool
vbi_decode_vps_pdc		(vbi_program_id *	pid,
				 const uint8_t		buffer[13])
{
	unsigned int cni;

	assert (NULL != pid);
	assert (NULL != buffer);

	vbi_decode_vps_cni (&cni, buffer);

	pid->nuid	= vbi_nuid_from_cni (VBI_CNI_TYPE_VPS, cni);

	pid->channel	= VBI_PID_CHANNEL_VPS;

	pid->pil	= (+ ((buffer[ 8] & 0x3F) << 14)
			   +  (buffer[ 9] << 6)
			   +  (buffer[10] >> 2));

	pid->month	= VBI_PIL_MONTH (pid->pil) - 1; 
	pid->day	= VBI_PIL_DAY (pid->pil) - 1; 
	pid->hour	= VBI_PIL_HOUR (pid->pil); 
	pid->minute	= VBI_PIL_MINUTE (pid->pil); 

	pid->length	= 0; /* unknown */

	pid->luf	= FALSE; /* no update, just pil */
	pid->mi		= FALSE; /* label is not 30 s early */
	pid->prf	= FALSE; /* prepare to record unknown */

	pid->pcs_audio	= buffer[ 2] >> 6;
	pid->pty	= buffer[12];

	pid->tape_delayed = FALSE;

	return TRUE;
}
