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

/* $Id: bcd.h,v 1.6.2.7 2004-03-31 00:41:33 mschimek Exp $ */

#ifndef __ZVBI_BCD_H__
#define __ZVBI_BCD_H__

#include "macros.h"

/* Public */

VBI_BEGIN_DECLS

#ifndef DOXYGEN_SHOULD_IGNORE_THIS

#define _VBI_BCD_10 (((int) 0x1111111111111111LL) << 4)
#define _VBI_BCD_06 (((int) 0x6666666666666666LL) >> 4)

#endif

/**
 * @addtogroup BCD
 * @{
 */

#define VBI_BCD_MIN (0xF << (sizeof (int) * 8 - 4))
#define VBI_BCD_MAX (VBI_BCD_MIN ^ ~_VBI_BCD_06)

#define VBI_BCD_DEC_MAX	   /* FEDCBA9876543210    F6543210 */		\
	((8 == sizeof (int)) ? 999999999999999LL : 9999999)
#define VBI_BCD_DEC_MIN ((-VBI_BCD_DEC_MAX) - 1)

extern int
vbi_dec2bcd			(int			dec) vbi_const;
extern int
vbi_bcd2dec			(int			bcd) vbi_const;

/**
 * @param a BCD number.
 * @param b BCD number.
 * 
 * Adds two signed packed bcd numbers, returning a signed packed bcd sum.
 * 
 * @return
 * BCD number. The result is undefined when any of the arguments
 * contain hex digits 0xA ... 0xF, except for the sign nibble.
 */
vbi_inline int
vbi_add_bcd			(int			a,
				 int			b)
{
	int t = a + (b += _VBI_BCD_06);

	a  = ((~(b ^ a ^ t) & _VBI_BCD_10) >> 3) * 3;

	return t - a;
}

/**
 * @param bcd BCD number.
 * 
 * Calculates the ten's complement of a signed packed bcd. The most
 * significant nibble is the sign, e.g. 0xF999&nbsp;9999 = vbi_neg_bcd
 * (0x0000&nbsp;00001), presumed sizeof(int) is 4.
 *
 * @return
 * BCD number. The result is undefined when any of the arguments
 * contain hex digits 0xA ... 0xF, except for the sign nibble.
 *
 * Note the ten's complement of VBI_BCD_MIN is not representable
 * as signed packed bcd, this function will return VBI_BCD_MAX + 1
 * (0x1000&nbsp;0000) instead.
 */
vbi_inline int
vbi_neg_bcd			(int			bcd)
{
	int t = -bcd;

	return t - (((bcd ^ t) & _VBI_BCD_10) >> 3) * 3;
}

/**
 * @param a BCD number.
 * @param b BCD number.
 * 
 * Subtracts two signed packed bcd numbers, returning a - b. The result
 * may be negative (ten's complement), see vbi_neg_bcd().
 * 
 * @return
 * BCD number. The result is undefined when any of the arguments
 * contain hex digits 0xA ... 0xF, except for the sign nibble.
 */
vbi_inline int
vbi_sub_bcd			(int			a,
				 int			b)
{
	return vbi_add_bcd (a, vbi_neg_bcd (b));
}

/**
 * @param bcd BCD number.
 * 
 * Tests if @a bcd forms a valid signed packed bcd number.
 * 
 * @return
 * @c FALSE if @a bcd contains hex digits 0xA ... 0xF, ignoring
 * the four most significant bits i.e. the sign nibble.
 */
vbi_inline vbi_bool
vbi_is_bcd			(int			bcd)
{
	bcd &= ~VBI_BCD_MIN;

	return 0 == (((bcd + _VBI_BCD_06) ^ bcd ^ _VBI_BCD_06) & _VBI_BCD_10);
}

/**
 * @param bcd Unsigned BCD number.
 * @param maximum Unsigned maximum value.
 *
 * Compares an unsigned packed bcd number digit-wise against a maximum
 * value, for example 0x295959. The function takes about six instructions.
 * @a maximum can contain digits 0x0 ... 0xF.
 *
 * @return
 * @c TRUE if any digit of @a bcd is greater than the
 * corresponding digit of @a maximum.
 */
vbi_inline vbi_bool
vbi_bcd_vec_greater		(unsigned int		bcd,
				 unsigned int		maximum)
{
	maximum ^= ~0;

	return 0 != (((bcd + maximum) ^ bcd ^ maximum) & _VBI_BCD_10);
}

/** @} */

/**
 * @addtogroup Service
 */

/**
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
 * This is the subpage number only applicable to Teletext pages,
 * a BCD number in range 0x01 ... 0x79. Pages without subpages
 * have subpage number 0x00.
 *
 * On special 'clock' pages (for example listing the current time
 * in different time zones) it can assume values between 0x0000 ...
 * 0x2359 expressing local time (EN 300 706, Section E.2). These are
 * not actually subpages.
 *
 * Finally the non-BCD value 0x3F7F is possible, symbolic
 * @c VBI_ANY_SUBNO and @c VBI_NO_SUBNO depending on context.
 */
typedef int vbi_subno;

/* Magic constants from EN 300 706. */

#define VBI_ANY_SUBNO ((vbi_subno) 0x3F7F)
#define VBI_NO_SUBNO ((vbi_subno) 0x3F7F)

/** @} */

VBI_END_DECLS

/* Private */

#endif /* __ZVBI_BCD_H__ */
