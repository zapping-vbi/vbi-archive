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

/* $Id: wss.c,v 1.2.2.5 2006-05-18 16:49:20 mschimek Exp $ */

#include "../site_def.h"

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "misc.h"
#include "hamm.h"		/* vbi3_par8() */
#include "wss.h"

/**
 * @addtogroup AspectRatio Wide Screen Signalling Decoder
 * @ingroup LowDec
 */

/**
 * @param subtitles Caption / subtitle information.
 *
 * Returns the name of a subtitle mode, for example VBI3_SUBTITLES_ACTIVE ->
 * "ACTIVE". This is mainly intended for debugging.
 * 
 * @return
 * Static ASCII string, NULL if @a subtitles is invalid.
 */
const char *
vbi3_subtitles_name		(vbi3_subtitles		subtitles)
{
	switch (subtitles) {
#undef CASE
#define CASE(s) case VBI3_SUBTITLES_##s : return #s ;
	CASE (NONE)
	CASE (UNKNOWN)
	CASE (ACTIVE)
	CASE (MATTE)
	CASE (CLOSED)
	}

	return NULL;
}

/**
 * @param ar vbi3_aspect_ratio structure initialized with
 *   vbi3_aspect_ratio_init().
 *
 * Frees all resources associated with @a ar, except the structure itself.
 */
void
vbi3_aspect_ratio_destroy	(vbi3_aspect_ratio *	ar)
{
	assert (NULL != ar);

	CLEAR (*ar);
}

/**
 * @param ar vbi3_aspect_ratio structure to initialize.
 * @param videostd_set This video standard (can be a set if more than
 *   one may apply) determines the active video bounds.
 *
 * Initializes a vbi3_aspect_ratio structure.
 */
void
vbi3_aspect_ratio_init		(vbi3_aspect_ratio *	ar,
				 vbi3_videostd_set	videostd_set)
{
	assert (NULL != ar);

	CLEAR (*ar);

	if (videostd_set & VBI3_VIDEOSTD_SET_525_60) {
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

	ar->ratio = 1.0;
}

/**
 * @internal
 * @param ar vbi3_aspect_ratio structure to print.
 * @param fp Destination stream.
 *
 * Prints the contents of a vbi3_aspect_ratio structure as a
 * string without trailing newline. This is intended for debugging.
 */
void
_vbi3_aspect_ratio_dump		(const vbi3_aspect_ratio *ar,
				 FILE *			fp)
{
	fprintf (fp, "active_video=%u-%u,%u-%u ratio=%f subt=%s,%s "
		 "film=%u macp=%u helper=%u surround=%u copyright=%u "
		 "copying_restricted=%u",
		 ar->start[0], ar->start[0] + ar->count[0] - 1,
		 ar->start[1], ar->start[1] + ar->count[1] - 1,
		 ar->ratio,
		 vbi3_subtitles_name (ar->open_subtitles),
		 vbi3_subtitles_name (ar->closed_subtitles),
		 ar->film_mode, ar->palplus_macp, ar->palplus_helper,
		 ar->surround_sound, ar->copyright_asserted,
		 ar->copying_restricted);
}

/**
 * @param ar Results are stored in this structure.
 * @param buffer A WSS packet (14 bits) as defined for @c VBI3_SLICED_WSS_625.
 *
 * Decodes a PAL/SECAM Wide Screen Signalling (EN 300 294) packet
 * and stores the information in a vbi3_aspect_ratio structure.
 *
 * @returns
 * @c FALSE if the packet contains errors, in this case @a ar
 * remains unchanged. Note the error protection is rather weak. It may be
 * prudent to wait until two identical packets have been received.
 */
vbi3_bool
vbi3_decode_wss_625		(vbi3_aspect_ratio *	ar,
				 const uint8_t		buffer[2])
{
	int parity;

	assert (NULL != ar);
	assert (NULL != buffer);

	parity = buffer[0] & 15;
	parity ^= parity >> 2;
	parity ^= parity >> 1;

	if (unlikely (0 == (parity & 1)))
		return FALSE;

	ar->ratio = 1.0;
	ar->wss625_aspect = buffer[0] & 7;

	switch (ar->wss625_aspect) {
	case VBI3_WSS625_ASPECT_4_3:
	case VBI3_WSS625_ASPECT_14_9_SOFT_MATTE:
		ar->start[0] = 23;
		ar->start[1] = 336;
		ar->count[0] = 288; 
		ar->count[1] = 288; 
		break;

	case VBI3_WSS625_ASPECT_14_9:
		ar->start[0] = 41;
		ar->start[1] = 354;
		ar->count[0] = 252; 
		ar->count[1] = 252; 
		break;

	case VBI3_WSS625_ASPECT_14_9_TOP:
		ar->start[0] = 23;
		ar->start[1] = 336;
		ar->count[0] = 252;
		ar->count[1] = 252;
		break;

	case VBI3_WSS625_ASPECT_16_9:
	case VBI3_WSS625_ASPECT_16_9_PLUS:
		ar->start[0] = 59;
		ar->start[1] = 372;
		ar->count[0] = 216;
		ar->count[1] = 216;
		break;

	case VBI3_WSS625_ASPECT_16_9_TOP:
		ar->start[0] = 23;
		ar->start[1] = 336;
		ar->count[0] = 216;
		ar->count[1] = 216;
		break;

	case VBI3_WSS625_ASPECT_16_9_ANAMORPHIC:
		ar->start[0] = 23;
		ar->start[1] = 336;
		ar->count[0] = 288; 
		ar->count[1] = 288; 

		ar->ratio = 3.0 / 4.0;

		break;
	}

	ar->film_mode = ((buffer[0] & 0x10) > 0);
	ar->palplus_macp = ((buffer[0] & 0x20) > 0);
	ar->palplus_helper = ((buffer[0] & 0x40) > 0);

	switch (buffer[1] & 1) {
	case 0:
		ar->closed_subtitles = VBI3_SUBTITLES_NONE;
		break;
	case 1:
		ar->closed_subtitles = VBI3_SUBTITLES_CLOSED;
		break;
	}

	switch ((buffer[1] >> 1) & 3) {
	case 0:
		ar->open_subtitles = VBI3_SUBTITLES_NONE;
		break;
	case 1:
		ar->open_subtitles = VBI3_SUBTITLES_ACTIVE;
		break;
	case 2:
		ar->open_subtitles = VBI3_SUBTITLES_MATTE;
		break;
	case 3:
		ar->open_subtitles = VBI3_SUBTITLES_UNKNOWN;
		break;
	}

	ar->surround_sound = ((buffer[1] & 0x08) > 0);
	ar->copyright_asserted = ((buffer[1] & 0x10) > 0);
	ar->copying_restricted = ((buffer[1] & 0x20) > 0);

	return TRUE;
}

/**
 * @param buffer A WSS packet (14 bits) as defined for @c VBI3_SLICED_WSS_625.
 * @param ar Aspect ratio information.
 *
 * Stores aspect ratio, subtitle and PAL Plus information (specifically
 * wss625_aspect, film_mode, palplus_macp, palplus_helper, surround_sound,
 * copyright_asserted, copying_restriced, open_subtitles, closed_subtitles)
 * in a PAL/SECAM Wide Screen Signalling (EN 300 294) packet.
 *
 * @returns
 * @c FALSE if any of the parameters to encode are invalid; in this
 * case @a buffer remains unmodified.
 */
vbi3_bool
vbi3_encode_wss_625		(uint8_t		buffer[2],
				 const vbi3_aspect_ratio *ar)
{
	unsigned int b0, b1;

	assert (NULL != buffer);
	assert (NULL != ar);

 	if (unlikely ((unsigned int) ar->wss625_aspect > 7))
		return FALSE;

	b0 = vbi3_par8 (ar->wss625_aspect << 4) >> 4;
	b0 |= (!!ar->film_mode) << 4;
	b0 |= (!!ar->palplus_macp) << 5;
	b0 |= (!!ar->palplus_helper) << 6;

	b1 = 0;

	switch (ar->closed_subtitles) {
	case VBI3_SUBTITLES_UNKNOWN:
	case VBI3_SUBTITLES_NONE:
		break;

	case VBI3_SUBTITLES_CLOSED:
		b1 |= 1 << 0;
		break;

	default:
		return FALSE;
	}

	switch (ar->open_subtitles) {
	case VBI3_SUBTITLES_NONE:
		break;

	case VBI3_SUBTITLES_ACTIVE:
		b1 |= 1 << 1;
		break;

	case VBI3_SUBTITLES_MATTE:
		b1 |= 2 << 1;
		break;

	case VBI3_SUBTITLES_UNKNOWN:
		b1 |= 3 << 1;
		break;

	default:
		return FALSE;
	}

	b1 |= (!!ar->surround_sound) << 3;
	b1 |= (!!ar->copyright_asserted) << 4;
	b1 |= (!!ar->copying_restricted) << 5;

	buffer[0] = b0;
	buffer[1] = b1;

	return TRUE;
}

/**
 * @param ar Results are stored in this structure.
 * @param buffer A WSS packet (19 bits) as defined for
 *   @c VBI3_SLICED_WSS_CPR1204.
 *
 * Decodes an NTSC-JP Wide Screen Signalling (EIA-J CPR-1204) packet
 * and stores the information in a vbi3_aspect_ratio structure.
 *
 * @returns
 * Always @c TRUE, no error checking possible.
 */
vbi3_bool
vbi3_decode_wss_cpr1204		(vbi3_aspect_ratio *	ar,
				 const uint8_t		buffer[3])
{
	int b0 = buffer[0] & 0x80;
	int b1 = buffer[0] & 0x40;

	assert (NULL != ar);
	assert (NULL != buffer);

	CLEAR (*ar);

	if (b1) {
		ar->start[0] = 52;
		ar->start[1] = 315;
		ar->count[0] = 180;
		ar->count[1] = 180;

		ar->wss625_aspect = VBI3_WSS625_ASPECT_16_9;
	} else {
		ar->start[0] = 22;
		ar->start[1] = 285;
		ar->count[0] = 240;
		ar->count[1] = 240;
	}

	if (b0) {
		ar->ratio = 3.0 / 4.0;

		ar->wss625_aspect = VBI3_WSS625_ASPECT_16_9_ANAMORPHIC;
	} else {
		ar->ratio = 1.0;
	}

	return TRUE;
}
