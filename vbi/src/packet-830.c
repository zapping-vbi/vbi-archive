/*
 *  libzvbi - Teletext packet decoder, packet 8/30
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

/* $Id: packet-830.c,v 1.1.2.3 2004-04-05 04:42:27 mschimek Exp $ */

#include "../config.h"

#include <assert.h>

#include "bcd.h"		/* vbi_bcd2dec() */
#include "hamm.h"		/* vbi_rev16p(), vbi_iham8() */
#include "network.h"		/* vbi_nuid_from_cni() */
#include "packet-830.h"

/**
 * @addtogroup Packet830 Teletext Packet 8/30 Decoder 
 * @ingroup LowDec
 */

/**
 * @param cni CNI of type VBI_CNI_TYPE_8301 is stored here.
 * @param buffer Teletext packet (last 42 bytes, i. e. without clock
 *   run-in and framing code), as in struct vbi_sliced.
 *
 * Decodes a Teletext packet 8/30 format 1 according to
 * ETS 300 706 section 9.8.1, returning the 16 bit Country and
 * Network Identifier in @a cni.
 *
 * @returns
 * @c FALSE if the buffer contained incorrectable data.
 */
vbi_bool
vbi_decode_teletext_8301_cni	(unsigned int *		cni,
				 const uint8_t		buffer[42])
{
	assert (NULL != cni);
	assert (NULL != buffer);

	*cni = vbi_rev16p (buffer + 9);

	return TRUE;
}

/**
 * @param time UTC time is stored here.
 * @param gmtoff Local time offset in seconds east of UTC is stored here,
 *   including daylight saving time, as in BSD and GNU struct tm tm_gmtoff.
 *   To get local time add @a gmtoff to @a time.
 * @param buffer Teletext packet (last 42 bytes, i. e. without clock
 *   run-in and framing code), as in struct vbi_sliced.
 * 
 * Decodes a Teletext packet 8/30 format 1 according to
 * ETS 300 706 section 9.8.1, returning the time data.
 *
 * Note a few stations incorrectly transmit local time instead of UTC,
 * with gmtoff zero.
 *
 * @returns
 * @c FALSE if the buffer contained incorrectable data.
 */
vbi_bool
vbi_decode_teletext_8301_local_time
				(time_t *		time,
				 int *			gmtoff,
				 const uint8_t		buffer[42])
{
	unsigned int mjd;
	unsigned int utc;
	int bcd;
	int t;

	assert (NULL != time);
	assert (NULL != gmtoff);
	assert (NULL != buffer);

	bcd = (+ ((buffer[12] & 15) << 16)
	       +  (buffer[13] << 8)
	       +   buffer[14]
	       - 0x11111);

	if (vbi_bcd_vec_greater (bcd, 0x99999))
		return FALSE;

	mjd = vbi_bcd2dec (bcd);

	bcd = (+ (buffer[15] << 16)
	       + (buffer[16] << 8)
	       +  buffer[17]
	       - 0x111111);

	if (vbi_bcd_vec_greater (bcd, 0x295959))
		return FALSE;

	utc  = (bcd & 15)        + ((bcd >> 4) & 15) * 10;
	bcd >>= 8;
	utc += (bcd & 15) * 60   + ((bcd >> 4) & 15) * 600;
	bcd >>= 8;
	utc += (bcd & 15) * 3600 +  (bcd >> 4)       * 36000;

	if (utc >= 24 * 60 * 60)
		return FALSE;

	*time = (mjd - 40587) * 86400 + utc;

	/* Local time offset in seconds east of UTC. */

	t = (buffer[11] & 0x3E) * (15 * 60);

	if (buffer[11] & 0x40)
		t = -t;

	/* 300 706 says 0x81 bits are reserved, some stations set,
	   some clear, no apparent function (no UTC/local flag anyway). */

	*gmtoff = t;

	return TRUE;
}

/**
 * @param cni CNI of type VBI_CNI_TYPE_8302 is stored here.
 * @param buffer Teletext packet (last 42 bytes, i. e. without clock
 *   run-in and framing code), as in struct vbi_sliced.
 *
 * Decodes a Teletext packet 8/30 format 2 according to
 * ETS 300 706 section 9.8.2, returning the 16 bit Country and
 * Network Identifier in @a cni.
 *
 * @returns
 * @c FALSE if the buffer contained incorrectable data.
 */
vbi_bool
vbi_decode_teletext_8302_cni	(unsigned int *		cni,
				 const uint8_t		buffer[42])
{
	int b[13];

	assert (NULL != cni);
	assert (NULL != buffer);

	b[ 7] = vbi_iham16p (buffer + 10);
	b[ 8] = vbi_iham16p (buffer + 12);
	b[10] = vbi_iham16p (buffer + 16);
	b[11] = vbi_iham16p (buffer + 18);

	if ((b[7] | b[8] | b[10] | b[11]) < 0)
		return FALSE;

	b[ 7] = vbi_rev8 (b[ 7]);
	b[ 8] = vbi_rev8 (b[ 8]);
	b[10] = vbi_rev8 (b[10]);
	b[11] = vbi_rev8 (b[11]);

	*cni = (+ ((b[ 7] & 0x0F) << 12)
		+ ((b[10] & 0x03) << 10)
		+ ((b[11] & 0xC0) << 2)
		+  (b[ 8] & 0xC0)
		+  (b[11] & 0x3F));

	return TRUE;
}

/**
 * @param pid PDC data is stored here.
 * @param buffer Teletext packet (last 42 bytes, i. e. without clock
 *   run-in and framing code), as in struct vbi_sliced.
 * 
 * Decodes a Teletext packet 8/30 format 2 according to
 * ETS 300 231, storing PDC recording-control data in @a pid.
 *
 * @returns
 * @c FALSE if the buffer contained incorrectable data.
 */
vbi_bool
vbi_decode_teletext_8302_pdc	(vbi_program_id *	pid,
				 const uint8_t		buffer[42])
{
	uint8_t b[13];
	unsigned int i;
	int error;
	unsigned int cni;

	assert (NULL != pid);
	assert (NULL != buffer);

	error = vbi_iham8 (buffer[10]);
	b[ 6] = error;

	for (i = 7; i <= 12; ++i) {
		int t;

		t = vbi_iham16p (buffer + i * 2 - 4);
		error |= t;
		b[i] = vbi_rev8 (t);
	}

	if (error < 0)
		return FALSE;

	cni = (+ ((b[ 7] & 0x0F) << 12)
	       + ((b[10] & 0x03) << 10)
	       + ((b[11] & 0xC0) << 2)
	       +  (b[ 8] & 0xC0)
	       +  (b[11] & 0x3F));

	pid->nuid	= vbi_nuid_from_cni (VBI_CNI_TYPE_8302, cni);

	pid->channel	= VBI_PID_CHANNEL_LCI_0 + ((b[6] >> 2) & 3);

	pid->luf	= !!(b[6] & 2);
	pid->prf	= b[6] & 1;

	pid->pcs_audio	= b[7] >> 6;

	pid->mi		= !!(b[7] & 0x20);

	pid->pil	= (+ ((b[ 8] & 0x3F) << 14)
			   +  (b[ 9] << 6)
			   +  (b[10] >> 2));

	pid->month	= VBI_PIL_MONTH (pid->pil) - 1; 
	pid->day	= VBI_PIL_DAY (pid->pil) - 1; 
	pid->hour	= VBI_PIL_HOUR (pid->pil); 
	pid->minute	= VBI_PIL_MINUTE (pid->pil); 

	pid->length	= 0; /* unknown */

	pid->pty	= b[12];

	pid->tape_delayed = FALSE;

	return TRUE;
}
