/*
 *  libzvbi - Wide Screen Signalling
 *
 *  Copyright (C) 2001-2004 Michael H. Schimek
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

/* $Id: wss.c,v 1.2.2.3 2004-07-16 00:08:19 mschimek Exp $ */

#include "../site_def.h"
#include "../config.h"

#include <assert.h>
#include <stdio.h>		/* FILE */
#include "misc.h"		/* CLEAR() */
#include "wss.h"

#ifndef WSS_625_LOG
#define WSS_625_LOG 0
#endif

/**
 * @addtogroup AspectRatio Wide Screen Signalling Decoder
 * @ingroup LowDec
 */

/**
 * @param subtitles Caption / subtitle information.
 *
 * Returns the name of a subtitle mode, for example VBI_SUBTITLES_ACTIVE ->
 * "ACTIVE". This is mainly intended for debugging.
 * 
 * @return
 * Static ASCII string, NULL if @a subtitles is invalid.
 */
const char *
vbi_subtitles_name		(vbi_subtitles		subtitles)
{
	switch (subtitles) {

#undef CASE
#define CASE(s) case VBI_SUBTITLES_##s : return #s ;

	CASE (NONE)
	CASE (UNKNOWN)
	CASE (ACTIVE)
	CASE (MATTE)
	CASE (CLOSED)
	
		/* No default, gcc warns. */
	}

	return NULL;
}

/**
 * @param ar vbi_aspect_ratio structure initialized with
 *   vbi_aspect_ratio_init().
 *
 * Frees all resources associated with @a ar, except the structure itself.
 */
void
vbi_aspect_ratio_destroy	(vbi_aspect_ratio *	ar)
{
	assert (NULL != ar);

	CLEAR (*ar);
}

/**
 * @param ar vbi_aspect_ratio structure to initialize.
 * @param videostd_set This video standard (can be a set if more than
 *   one may apply) determines the active video bounds.
 *
 * Initializes a vbi_aspect_ratio structure.
 */
void
vbi_aspect_ratio_init		(vbi_aspect_ratio *	ar,
				 vbi_videostd_set	videostd_set)
{
	assert (NULL != ar);
	
	if (videostd_set & VBI_VIDEOSTD_SET_525_60) {
		ar->start[0] = 23;
		ar->start[1] = 336;
		ar->count[0] = 288; 
		ar->count[1] = 288; 
	} else {
		ar->start[0] = 22;
		ar->start[1] = 285;
		ar->count[0] = 240;
		ar->count[1] = 240;
	}

	ar->ratio		= 1.0;
	ar->film_mode		= FALSE;
	ar->open_subtitles	= VBI_SUBTITLES_UNKNOWN;
	ar->closed_subtitles	= VBI_SUBTITLES_UNKNOWN;

	CLEAR (ar->reserved1);
}

/**
 * @internal
 * @param ar vbi_aspect_ratio structure to print.
 * @param fp Destination stream.
 *
 * Prints the contents of a vbi_aspect_ratio structure as a
 * string without trailing newline. This is intended for debugging. 
 */
void
_vbi_aspect_ratio_dump		(const vbi_aspect_ratio *ar,
				 FILE *			fp)
{
	fprintf (fp, "active=%u-%u,%u-%u ratio=%f film=%u subt=%s,%s",
		 ar->start[0], ar->start[0] + ar->count[0] - 1,
		 ar->start[1], ar->start[1] + ar->count[1] - 1,
		 ar->ratio, ar->film_mode,
		 vbi_subtitles_name (ar->open_subtitles),
		 vbi_subtitles_name (ar->closed_subtitles));
}

/**
 * @param ar Results are stored in this structure.
 * @param buffer A WSS packet (14 bits) as defined for @c VBI_SLICED_WSS_625.
 *
 * Decodes a PAL/SECAM Wide Screen Signalling (EN 300 294) packet
 * and stores the information in a vbi_aspect_ratio structure.
 *
 * @returns
 * @c FALSE if the packet contains errors, in this case @a ar
 * remains unchanged. Note the error protection is rather weak.
 */
vbi_bool
vbi_decode_wss_625		(vbi_aspect_ratio *	ar,
				 const uint8_t		buffer[2])
{
	int parity;

	assert (NULL != ar);
	assert (NULL != buffer);

	parity = buffer[0] & 15;
	parity ^= parity >> 2;
	parity ^= parity >> 1;

	if (!(parity & 1))
		return FALSE;

	ar->ratio = 1.0;

	switch (buffer[0] & 7) {
	case 0: /* 4:3 */
	case 6: /* 14:9 soft matte */
		ar->start[0] = 23;
		ar->start[1] = 336;
		ar->count[0] = 288; 
		ar->count[1] = 288; 
		break;
	case 1: /* 14:9 */
		ar->start[0] = 41;
		ar->start[1] = 354;
		ar->count[0] = 252; 
		ar->count[1] = 252; 
		break;
	case 2: /* 14:9 top */
		ar->start[0] = 23;
		ar->start[1] = 336;
		ar->count[0] = 252;
		ar->count[1] = 252;
		break;
	case 3: /* 16:9 */
	case 5: /* "Letterbox > 16:9" */
		ar->start[0] = 59;
		ar->start[1] = 372;
		ar->count[0] = 216;
		ar->count[1] = 216;
		break;
	case 4: /* 16:9 top */
		ar->start[0] = 23;
		ar->start[1] = 336;
		ar->count[0] = 216;
		ar->count[1] = 216;
		break;
	case 7: /* 16:9 anamorphic */
		ar->start[0] = 23;
		ar->start[1] = 336;
		ar->count[0] = 288; 
		ar->count[1] = 288; 
		ar->ratio = 3.0 / 4.0;
		break;
	}

	ar->film_mode = !!(buffer[0] & 0x10);

	switch (buffer[1] & 1) {
	case 0:
		ar->closed_subtitles = VBI_SUBTITLES_NONE;
		break;
	case 1:
		ar->closed_subtitles = VBI_SUBTITLES_CLOSED;
		break;
	}

	switch ((buffer[1] >> 1) & 3) {
	case 0:
		ar->open_subtitles = VBI_SUBTITLES_NONE;
		break;
	case 1:
		ar->open_subtitles = VBI_SUBTITLES_ACTIVE;
		break;
	case 2:
		ar->open_subtitles = VBI_SUBTITLES_MATTE;
		break;
	case 3:
		ar->open_subtitles = VBI_SUBTITLES_UNKNOWN;
		break;
	}

	CLEAR (ar->reserved1);

	if (WSS_625_LOG) {
		fputs ("WSS: ", stderr);

		_vbi_aspect_ratio_dump (ar, stderr);

		fprintf (stderr,
			 "\nmacp=%u, helper=%u, b7=%u "
			 "surround=%u, copyright=%u, copying=%u\n",
			 !!(buffer[0] & 0x20), /* motion adaptive color plus */
			 !!(buffer[0] & 0x40), /* modulated helper */
			 !!(buffer[0] & 0x80), /* reserved */
			 !!(buffer[1] & 0x08), /* surround sound */
			 !!(buffer[1] & 0x10), /* copyright asserted */
			 !!(buffer[1] & 0x20)); /* copying restricted */
	}

	return TRUE;
}

/**
 * @param ar Results are stored in this structure.
 * @param buffer A WSS packet (19 bits) as defined for
 *   @c VBI_SLICED_WSS_CPR1204.
 *
 * Decodes an NTSC-J Wide Screen Signalling (EIA-J CPR-1204) packet
 * and stores the information in a vbi_aspect_ratio structure.
 *
 * @returns
 * Always @c TRUE, no error checking possible.
 */
vbi_bool
vbi_decode_wss_cpr1204		(vbi_aspect_ratio *	ar,
				 const uint8_t		buffer[3])
{
	int b0 = buffer[0] & 0x80;
	int b1 = buffer[0] & 0x40;

	assert (NULL != ar);
	assert (NULL != buffer);

	if (b1) {
		ar->start[0] = 52;
		ar->start[1] = 315;
		ar->count[0] = 180;
		ar->count[1] = 180;
	} else {
		ar->start[0] = 22;
		ar->start[1] = 285;
		ar->count[0] = 240;
		ar->count[1] = 240;
	}

	ar->ratio = b0 ? 3.0 / 4.0 : 1.0;

	ar->film_mode = FALSE;

	ar->open_subtitles = VBI_SUBTITLES_UNKNOWN;
	ar->closed_subtitles = VBI_SUBTITLES_UNKNOWN;

	CLEAR (ar->reserved1);

	return TRUE;
}
