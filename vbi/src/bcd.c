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

/* $Id: bcd.c,v 1.1.2.5 2007-11-01 00:21:22 mschimek Exp $ */

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
 * numbers in range 0x100 to 0x8FF. The packed bcd format encodes one
 * decimal digit in every hex nibble (four bits) of the number. Page
 * numbers containing digits 0xA to 0xF are reserved for various
 * system purposes, such as the transmission of page inventories,
 * and not intended for display.
 *
 * The library stores packed BCD numbers in int types, with the four
 * most significant bits containing the sign. Negative BCDs are
 * expressed as ten's complement, for example -1 as 0xF9999999, assumed
 * sizeof(int) is 4. Their limits as signed packed bcd value is
 * VBI3_BCD_MIN .. VBI3_BCD_MAX, as two's complement binary
 * VBI3_BCD_BIN_MIN ... VBI3_BCD_BIN_MAX. That is -10 ** n ...
 * (10 ** n) - 1, where n = 2 * sizeof(int) - 1.
 */

/**
 * @ingroup BCD
 * @param bin Binary number.
 * 
 * Converts a two's complement binary to a signed packed bcd value.
 * The argument @a bin must be in range VBI3_BCD_BIN_MIN ...
 * VBI3_BCD_BIN_MAX. Other values yield an undefined result.
 * 
 * @return
 * BCD number.
 */
int
vbi3_bin2bcd			(int			bin)
{
	int t = 0;

	/* XXX Try x87 bcd for large values? */

	/* Teletext page numbers are unsigned. */
	if (unlikely (bin < 0)) {
		t |= VBI3_BCD_MIN;
		bin += -VBI3_BCD_BIN_MIN;
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
 * Converts a signed packed bcd to a two's complement binary value.
 * 
 * @return
 * Binary number. The result is undefined when the bcd number
 * contains hex digits 0xA ... 0xF, except for the sign nibble.
 */
int
vbi3_bcd2bin			(int			bcd)
{
	int s;
	int t;

	s = bcd;

	/* Teletext page numbers are unsigned. */
	if (unlikely (bcd < 0)) {
		/* Cannot negate minimum. */
		if (unlikely (VBI3_BCD_MIN == bcd))
			return VBI3_BCD_BIN_MIN;

		bcd = vbi3_neg_bcd (bcd);
	}

	/* Most common case 2-4 digits, as in Teletext
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
