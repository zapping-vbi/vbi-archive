/*
 *  libzvbi - BCD arithmetic for Teletext page numbers
 *
 *  Copyright (C) 2001-2003 Michael H. Schimek
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

/* $Id: bcd.c,v 1.1.2.2 2004-01-30 00:37:57 mschimek Exp $ */

#include "bcd.h"

/**
 * @addtogroup BCD BCD arithmetic for Teletext page numbers
 * @ingroup Service
 *
 * Teletext page numbers are expressed as packed binary coded decimal
 * numbers in range 0x100 to 0x8FF. The packed bcd format encodes one
 * decimal digit in every hex nibble (four bits) of the number. Page
 * numbers containing digits 0xA to 0xF are reserved for various
 * system purposes and not intended for display.
 *
 * BCD numbers are stored in int types. Negative BCDs are expressed
 * as ten's complement, for example -1 as 0xF999&nbsp;9999, assumed
 * sizeof(int) is four. Their precision as signed packed bcd value
 * is VBI_BCD_MIN .. VBI_BCD_MAX, as two's complement
 * binary VBI_BCD_DEC_MIN ... VBI_BCD_DEC_MAX. That is -10 ** n ...
 * (10 ** n) - 1, where n = 2 * sizeof(int) - 1.
 */

/**
 * @ingroup BCD
 * @param dec Binary number.
 * 
 * Converts a two's complement binary to a signed packed bcd value.
 * The argument @a dec must be in range VBI_BCD_DEC_MIN ...
 * VBI_BCD_DEC_MAX. Other values yield an undefined result.
 * 
 * @return
 * BCD number.
 */
int
vbi_dec2bcd			(int			dec)
{
	int t = 0;

	/* XXX should try x87 bcd for large values. */

	/* Unlikely, Teletext page numbers are unsigned. */
	if (__builtin_expect (dec < 0, 0)) {
		t |= VBI_BCD_MIN;
		dec += -VBI_BCD_DEC_MIN;
	}

	/* Most common case 2-4 digits, as in Teletext
	   page and subpage numbers. */

	t += (dec % 10) << 0; dec /= 10;
	t += (dec % 10) << 4; dec /= 10;
	t += (dec % 10) << 8; dec /= 10;
	t += (dec % 10) << 12;

	if (__builtin_expect (dec >= 10, 0)) {
		unsigned int i;

		for (i = 16; i < sizeof (int) * 8; i += 4) {
			dec /= 10;
			t += (dec % 10) << i;
		}
	}

	return t;
}

/**
 * @ingroup BCD
 * @param bcd BCD number.
 * 
 * Converts a signed packed bcd to a two's complement binary value.
 * 
 * @return
 * Binary number. The result is undefined when the bcd number
 * contains hex digits 0xA ... 0xF, except for the sign nibble.
 */
int
vbi_bcd2dec			(int			bcd)
{
	int s;
	int t;

	s = bcd;

	/* Unlikely, Teletext page numbers are unsigned. */
	if (__builtin_expect (bcd < 0, 0)) {
		/* Cannot negate minimum. */
		if (__builtin_expect (VBI_BCD_MIN == bcd, 0))
			return VBI_BCD_DEC_MIN;

		bcd = vbi_neg_bcd (bcd);
	}

	/* Most common case 2-4 digits, as in Teletext
	   page and subpage numbers. */

	t  = (bcd & 15) * 1; bcd >>= 4;
	t += (bcd & 15) * 10; bcd >>= 4;
	t += (bcd & 15) * 100; bcd >>= 4;
	t += (bcd & 15) * 1000;

	if (__builtin_expect (bcd & -16, 0)) {
		unsigned int u;
		unsigned int i;

		u = (bcd >> (sizeof (int) * 8 - 5 * 4)) & 15;

		for (i = sizeof (int) * 8 - 6 * 4; i >= 4; i -= 4)
			u = u * 10 + ((bcd >> i) & 15);

		t += u * 10000;
	}

	/* Unlikely, Teletext page numbers are unsigned. */
	if (__builtin_expect (s < 0, 0))
		t = -t;

	return t;
}
