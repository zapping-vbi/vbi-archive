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

/* $Id: bcd.h,v 1.1.2.1 2008-08-19 10:56:02 mschimek Exp $ */

#ifndef __ZVBI_BCD_H__
#define __ZVBI_BCD_H__

#include "zvbi/macros.h"

VBI_BEGIN_DECLS

#ifndef DOXYGEN_SHOULD_IGNORE_THIS

/* We need 32 or 64 bit constants depending on the size of int,
   but we cannot cast to long long or use the LL suffix for ISO C89
   compatibility, and GCC warns about 32 bit shifts if ints are
   32 bits wide */
#define _VBI_BCD_10 ((int)((0x22222222 << 31) | 0x11111110))
#define _VBI_BCD_06 (((int)((0xCCCCCCCC << 31) | 0x66666666)) >> 4)

#endif

/**
 * @addtogroup BCD
 * @{
 */

#define VBI_BCD_MIN ((int)(0xF << (sizeof (int) * 8 - 4)))
#define VBI_BCD_MAX ((int)(VBI_BCD_MIN ^ ~_VBI_BCD_06))

/* We cannot use the LL suffix for ISO C89 compatibility. */
#define VBI_BCD_BIN_MAX						\
	((int)(((sizeof (int) > 4) ? 9999999 : 0) * 10000000 + 9999999))

#define VBI_BCD_BIN_MIN ((int)((-VBI_BCD_BIN_MAX) - 1))

/** Older name. */
#define vbi_dec2bcd vbi_bin2bcd
/** Older name. */
#define vbi_bcd2dec vbi_bcd2bin

extern int
vbi_bin2bcd			(int			bin)
  _vbi_const;
extern int
vbi_bcd2bin			(int			bcd)
  _vbi_const;

/**
 * @param a BCD number.
 * @param b BCD number.
 * 
 * Adds two signed packed BCD numbers, returning a signed packed BCD
 * sum.
 * 
 * @returns
 * BCD number. The result is undefined when any of the arguments
 * contain hex digits 0xA ... 0xF, except for the sign nibble.
 * 
 * @since 0.3.1
 */
_vbi_inline int
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
 * Calculates the ten's complement of a signed packed BCD number. The
 * most significant nibble is the sign, e.g. 0xF9999999 = vbi_neg_bcd
 * (0x000000001) when sizeof(int) is 4.
 * 
 * @returns
 * BCD number. The result is undefined when any of the arguments
 * contain hex digits 0xA ... 0xF, except for the sign nibble.
 * 
 * Note the ten's complement of VBI_BCD_MIN is not representable
 * as signed packed BCD number, this function will return
 * VBI_BCD_MAX + 1 (0x10000000 if sizeof(int) is 4) instead.
 * 
 * @since 0.3.1
 */
_vbi_inline int
vbi_neg_bcd			(int			bcd)
{
	int t = -bcd;

	return t - (((bcd ^ t) & _VBI_BCD_10) >> 3) * 3;
}

/**
 * @param a BCD number.
 * @param b BCD number.
 * 
 * Subtracts two signed packed BCD numbers, returning a - b. The
 * result may be negative (ten's complement), see vbi_neg_bcd().
 * 
 * @returns
 * BCD number. The result is undefined when any of the arguments
 * contain hex digits 0xA ... 0xF, except for the sign nibble.
 * 
 * @since 0.3.1
 */
_vbi_inline int
vbi_sub_bcd			(int			a,
				 int			b)
{
	return vbi_add_bcd (a, vbi_neg_bcd (b));
}

/**
 * @param bcd BCD number.
 * 
 * Tests if @a bcd forms a valid signed packed BCD number.
 *
 * @returns
 * @c FALSE if @a bcd contains hex digits 0xA ... 0xF, except
 * for the sign nibble.
 * 
 * @since 0.3.1
 */
_vbi_inline vbi_bool
vbi_is_bcd			(int			bcd)
{
	bcd &= ~VBI_BCD_MIN;

	return 0 == (((bcd + _VBI_BCD_06) ^ bcd ^ _VBI_BCD_06)
		     & _VBI_BCD_10);
}

/**
 * @param bcd Unsigned BCD number.
 * @param maximum Unsigned maximum value.
 *
 * Compares an unsigned packed BCD number digit-wise against a maximum
 * value, for example 0x295959. @a maximum can contain digits 0x0
 * ... 0xF.
 *
 * @returns
 * @c TRUE if any digit of @a bcd is greater than the
 * corresponding digit of @a maximum.
 * 
 * @since 0.3.1
 */
_vbi_inline vbi_bool
vbi_bcd_digits_greater		(unsigned int		bcd,
				 unsigned int		maximum)
{
	maximum ^= ~0;

	return 0 != (((bcd + maximum) ^ bcd ^ maximum) & _VBI_BCD_10);
}

/** @} */

/**
 * @addtogroup Service
 * @{
 */

/**
 * Teletext or Closed Caption page number. For Teletext pages
 * this is a BCD number in range 0x100 ... 0x8FF. Page numbers
 * containing digits 0xA to 0xF are reserved for various system
 * purposes, these pages are not intended for display.
 *
 * Closed Caption "page numbers" between 1 ... 8 correspond
 * to the four Caption and Text channels CC1 ... CC4 and T1 ... T4.
 */
typedef int vbi_pgno;

/** Primary synchronous caption service (English). */
#define VBI_CAPTION_CC1 1

/**
 * Special non-synchronous data that is intended to
 * augment information carried in the program.
 */
#define VBI_CAPTION_CC2 2

/**
 * Secondary synchronous caption service, usually
 * a second language.
 */
#define VBI_CAPTION_CC3 3

/** Special non-synchronous data similar to CC2. */
#define VBI_CAPTION_CC4 4

/** First text service, data usually not program related. */
#define VBI_CAPTION_T1 5

/**
 * Second text service, additional data usually not
 * program related (ITV data).
 */
#define VBI_CAPTION_T2 6

/** Additional text channel. */
#define VBI_CAPTION_T3 7

/** Additional text channel. */
#define VBI_CAPTION_T4 8

/**
 * This is the subpage number only applicable to Teletext pages,
 * a BCD number in range 0x01 ... 0x79. If a page has no subpages
 * its subpage number is 0x00.
 *
 * On special "clock" pages (for example listing the current time
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

#endif /* __ZVBI_BCD_H__ */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
