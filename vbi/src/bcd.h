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

/* $Id: bcd.h,v 1.6.2.2 2003-05-02 10:44:19 mschimek Exp $ */

#ifndef BCD_H
#define BCD_H

#include "misc.h"

/**
 * @addtogroup BCD BCD arithmetic for Teletext page numbers
 * @ingroup Service
 *
 * Teletext page numbers are expressed as binary coded decimal numbers
 * in range 0x100 to 0x8FF. The bcd format encodes one decimal digit in
 * every hex nibble (four bits) of the number. Page numbers containing
 * digits 0xA to 0xF are reserved for various system purposes and not
 * intended for display.
 */

/* Public */

/**
 * @ingroup BCD
 * @param dec Decimal number.
 * 
 * Converts a two's complement binary to a packed bcd value.
 * Currently the argument @a dec must be in range 0 ... 9999.
 * Other values yield an undefined result.
 *
 * @return
 * Packed bcd number.
 */
static_inline int
vbi_dec2bcd			(int			dec)
{
	int t;

	/* Should try x87 bcd, hence the undefined clause above.
	   Don't forget to add limit constants. */

	t =  (dec % 10) << 0; dec /= 10;
	t += (dec % 10) << 4; dec /= 10;
	t += (dec % 10) << 8; dec /= 10;
	t += (dec % 10) << 12;

	return t;
}

/**
 * @ingroup BCD
 * @param bcd BCD number.
 * 
 * Converts a packed bcd in to a two's complement binary value.
 * Currently the argument @a bcd must be in range 0x0 ... 0x9999.
 * Other values yield an undefined result.
 * 
 * @return
 * Binary number. The result is undefined when the bcd number
 * contains hex digits 0xA ... 0xF.
 */
static_inline int
vbi_bcd2dec			(int			bcd)
{
	int t;

	t  = (bcd & 15); bcd >>= 4;
	t += (bcd & 15) * 10; bcd >>= 4;
	t += (bcd & 15) * 100; bcd >>= 4;
	t += (bcd & 15) * 1000;

	return t;
}

/* Consider 64 bit ints. */

static const int vbi_bcd10 = ((int) 0x1111111111111111LL) << 4;
static const int vbi_bcd06 = ((int) 0x6666666666666666LL) >> 4; 

/**
 * @ingroup BCD
 * @param a BCD number.
 * @param b BCD number.
 * 
 * Adds two packed bcd numbers, returning a packed bcd sum.
 * 
 * @return
 * BCD number. The result is undefined when the bcd number
 * contains hex digits 0xA ... 0xF.
 */
static_inline int
vbi_add_bcd			(int			a,
				 int			b)
{
	int t = a + (b += vbi_bcd06);

	a  = ((~(b ^ a ^ t) & vbi_bcd10) >> 3) * 3;

	return t - a;
}

/**
 * @ingroup BCD
 * @param bcd BCD number.
 * 
 * Calculates the 10's complement of a packed bcd. The most significant
 * nibble is the sign, e.g. 0xF999&nbsp;9999 = vbi_neg_bcd (0x0000&nbsp;00001).
 * 
 * @return
 * BCD number. The result is undefined when the bcd number contains
 * hex digits 0xA ... 0xF.
 */
static_inline int
vbi_neg_bcd			(int			bcd)
{
	int t = -bcd;

	return t - (((bcd ^ t) & vbi_bcd10) >> 3) * 3;
}

/**
 * @ingroup BCD
 * @param a BCD number.
 * @param b BCD number.
 * 
 * Subtracts two packed bcd numbers, returning a - b. The result
 * may be negative (10's complement), see vbi_neg_bcd().
 * 
 * @return
 * BCD number. The result is undefined when the bcd number contains
 * hex digits 0xA ... 0xF.
 */
static_inline int
vbi_sub_bcd			(int			a,
				 int			b)
{
	return vbi_add_bcd (a, vbi_neg_bcd (b));
}

/**
 * @ingroup BCD
 * @param bcd BCD number.
 * 
 * Tests if @a bcd forms a valid packed bcd number.
 * 
 * @return
 * @c FALSE if @a bcd contains hex digits 0xA ... 0xF.
 */
static_inline vbi_bool
vbi_is_bcd			(int			bcd)
{
	if (bcd < 0) bcd ^= 0xF << (8 * sizeof (int) - 4);

	return (((bcd + vbi_bcd06) ^ (bcd ^ vbi_bcd06)) & vbi_bcd10) == 0;
}

/**
 * @ingroup Service
 * 
 * Teletext or Closed Caption page number. For Teletext pages
 * this is a bcd number in range 0x100 ... 0x8FF. Page numbers
 * containing digits 0xA to 0xF are reserved for various system
 * purposes, these pages are not intended for display.
 * 
 * Closed Caption page numbers between 1 ... 8 correspond
 * to the four Caption and Text channels:
 * <table>
 * <tr><td>1</td><td>Caption 1</td><td>
 *   "Primary synchronous caption service [English]"</td></tr>
 * <tr><td>2</td><td>Caption 2</td><td>
 *   "Special non-synchronous data that is intended to
 *   augment information carried in the program"</td></tr>
 * <tr><td>3</td><td>Caption 3</td><td>
 *   "Secondary synchronous caption service, usually
 *    second language [Spanish, French]"</td></tr>
 * <tr><td>4</td><td>Caption 4</td><td>
 *   "Special non-synchronous data similar to Caption 2"</td></tr>
 * <tr><td>5</td><td>Text 1</td><td>
 *   "First text service, data usually not program related"</td></tr>
 * <tr><td>6</td><td>Text 2</td><td>
 *   "Second text service, additional data usually not program related
 *    [ITV data]"</td></tr>
 * <tr><td>7</td><td>Text 3</td><td>
 *   "Additional text channel"</td></tr>
 * <tr><td>8</td><td>Text 4</td><td>
 *   "Additional text channel"</td></tr>
 * </table>
 */
typedef int vbi_pgno;

/**
 * @ingroup Service
 *
 * This is the subpage number only applicable to Teletext pages,
 * a BCD number in range 0x00 ... 0x99. On special 'clock' pages
 * (for example listing the current time in different time zones)
 * it can assume values between 0x0000 ... 0x2359 expressing
 * local time. These are not actually subpages.
 */
typedef int vbi_subno;

/* Magic constants from ETS 300 706. */

/**
 * @ingroup Service
 */
#define VBI_ANY_SUBNO ((vbi_subno) 0x3F7F)

/**
 * @ingroup Service
 */
#define VBI_NO_SUBNO ((vbi_subno) 0x3F7F)

/* Private */

#endif /* BCD_H */
