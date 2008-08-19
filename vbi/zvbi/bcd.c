/*
 *  libzvbi -- BCD arithmetic for Teletext page numbers
 *
 *  Copyright (C) 2001-2003 Michael H. Schimek
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the 
 *  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, 
 *  Boston, MA  02110-1301  USA.
 */

/* $Id: bcd.c,v 1.1.2.1 2008-08-19 10:56:02 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "misc.h"
#include "bcd.h"

/**
 * @addtogroup BCD BCD arithmetic for Teletext page numbers
 * @ingroup Service
 *
 * Teletext page numbers are expressed as packed binary coded decimal
 * numbers in range 0x100 to 0x8FF. The packed BCD format encodes one
 * decimal digit in every hex nibble (four bits) of the number. Page
 * numbers containing digits 0xA to 0xF are reserved for various
 * system purposes, such as page inventories or TOP tables, and are
 * not intended for display.
 *
 * The library stores packed BCD numbers in int types, with the four
 * most significant bits containing the sign. Negative BCDs are
 * expressed as ten's complement, for example -1 as 0xF9999999 if
 * sizeof(int) is 4.
 *
 * The limits of signed packed BCD values in BCD format are
 * VBI_BCD_MIN ... VBI_BCD_MAX, and as two's complement binary
 * VBI_BCD_BIN_MIN ... VBI_BCD_BIN_MAX. That is -10 ** n ...
 * (10 ** n) - 1, where n = 2 * sizeof(int) - 1.
 */

/**
 * @ingroup BCD
 * @param bin Binary number.
 * 
 * Converts a two's complement binary to a signed packed BCD value.
 * The argument @a bin must be in range VBI_BCD_BIN_MIN ...
 * VBI_BCD_BIN_MAX. Other values yield an undefined result.
 * 
 * @returns
 * BCD number.
 * 
 * @since 0.3.1
 */
int
vbi_bin2bcd			(int			bin)
{
	int t = 0;

	/* Teletext page numbers are unsigned. */
	if (unlikely (bin < 0)) {
		t |= VBI_BCD_MIN;
		bin += -VBI_BCD_BIN_MIN;
	}

	/* Most common case 2-4 digits, as in Teletext
	   page and subpage numbers. */

	t += (bin % 10) << 0; bin /= 10;
	t += (bin % 10) << 4; bin /= 10;
	t += (bin % 10) << 8; bin /= 10;
	t += (bin % 10) << 12;

	if (unlikely (bin >= 10)) {
		unsigned int i;

		for (i = 16; i < sizeof (int) * 8; i += 4) {
			bin /= 10;
			t += (bin % 10) << i;
		}
	}

	return t;
}

/**
 * @ingroup BCD
 * @param bcd BCD number.
 * 
 * Converts a signed packed BCD number to a two's complement
 * binary value.
 * 
 * @returns
 * Binary number. The result is undefined when the BCD number
 * contains hex digits 0xA ... 0xF, except for the sign nibble.
 * 
 * @since 0.3.1
 */
int
vbi_bcd2bin			(int			bcd)
{
	int s;
	int t;

	s = bcd;

	/* Teletext page numbers are unsigned. */
	if (unlikely (bcd < 0)) {
		/* Cannot negate minimum. */
		if (unlikely (VBI_BCD_MIN == bcd))
			return VBI_BCD_BIN_MIN;

		bcd = vbi_neg_bcd (bcd);
	}

	/* Most common case 2 ... 4 digits, as in Teletext
	   page and subpage numbers. */

	t  = (bcd & 15) * 1; bcd >>= 4;
	t += (bcd & 15) * 10; bcd >>= 4;
	t += (bcd & 15) * 100; bcd >>= 4;
	t += (bcd & 15) * 1000;

	if (unlikely (bcd & -16)) {
		unsigned int u;
		unsigned int i;

		u = (bcd >> (sizeof (int) * 8 - 5 * 4)) & 15;

		for (i = sizeof (int) * 8 - 6 * 4; i >= 4; i -= 4)
			u = u * 10 + ((bcd >> i) & 15);

		t += u * 10000;
	}

	/* Teletext page numbers are unsigned. */
	if (unlikely (s < 0))
		t = -t;

	return t;
}

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
