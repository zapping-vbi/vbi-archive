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

/* $Id: bcd.h,v 1.6.2.14 2008-02-25 20:58:47 mschimek Exp $ */

#ifndef __ZVBI3_BCD_H__
#define __ZVBI3_BCD_H__

#include "macros.h"

VBI3_BEGIN_DECLS

#ifndef DOXYGEN_SHOULD_IGNORE_THIS

/* We need 32 or 64 bit constants depending on the size of int,
   but we cannot cast to long long or use the LL suffix for ISO C89
   compatibility, and GCC warns about 32 bit shifts if ints are
   32 bits wide */
#define _VBI3_BCD_10 ((int)((0x22222222 << 31) | 0x11111110))
#define _VBI3_BCD_06 (((int)((0xCCCCCCCC << 31) | 0x66666666)) >> 4)

#endif

/**
 * @addtogroup BCD
 * @{
 */

#define VBI3_BCD_MIN ((int)(0xF << (sizeof (int) * 8 - 4)))
#define VBI3_BCD_MAX ((int)(VBI3_BCD_MIN ^ ~_VBI3_BCD_06))

/* We cannot use the LL suffix for ISO C89 compatibility. */
#define VBI3_BCD_BIN_MAX						\
	((int)(((sizeof (int) > 4) ? 9999999 : 0) * 10000000 + 9999999))

#define VBI3_BCD_BIN_MIN ((int)((-VBI3_BCD_BIN_MAX) - 1))

/** Older name. */
#define vbi3_dec2bcd vbi3_bin2bcd
/** Older name. */
#define vbi3_bcd2dec vbi3_bcd2bin

extern int
vbi3_bin2bcd			(int			bin)
  _vbi3_const;
extern int
vbi3_bcd2bin			(int			bcd)
  _vbi3_const;

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
_vbi3_inline int
vbi3_add_bcd			(int			a,
				 int			b)
{
	int t = a + (b += _VBI3_BCD_06);

	a  = ((~(b ^ a ^ t) & _VBI3_BCD_10) >> 3) * 3;

	return t - a;
}

/**
 * @param bcd BCD number.
 * 
 * Calculates the ten's complement of a signed packed bcd. The most
 * significant nibble is the sign, e.g. 0xF9999999 = vbi3_neg_bcd
 * (0x000000001), assumed sizeof(int) is 4.
 *
 * @return
 * BCD number. The result is undefined when any of the arguments
 * contain hex digits 0xA ... 0xF, except for the sign nibble.
 *
 * Note the ten's complement of VBI3_BCD_MIN is not representable
 * as signed packed bcd, this function will return VBI3_BCD_MAX + 1
 * (0x10000000) instead.
 */
_vbi3_inline int
vbi3_neg_bcd			(int			bcd)
{
	int t = -bcd;

	return t - (((bcd ^ t) & _VBI3_BCD_10) >> 3) * 3;
}

/**
 * @param a BCD number.
 * @param b BCD number.
 * 
 * Subtracts two signed packed bcd numbers, returning a - b. The result
 * may be negative (ten's complement), see vbi3_neg_bcd().
 * 
 * @return
 * BCD number. The result is undefined when any of the arguments
 * contain hex digits 0xA ... 0xF, except for the sign nibble.
 */
_vbi3_inline int
vbi3_sub_bcd			(int			a,
				 int			b)
{
	return vbi3_add_bcd (a, vbi3_neg_bcd (b));
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
_vbi3_inline vbi3_bool
vbi3_is_bcd			(int			bcd)
{
	bcd &= ~VBI3_BCD_MIN;

	return 0 == (((bcd + _VBI3_BCD_06) ^ bcd ^ _VBI3_BCD_06)
		     & _VBI3_BCD_10);
}

/**
 * @param bcd Unsigned BCD number.
 * @param maximum Unsigned maximum value.
 *
 * Compares an unsigned packed bcd number digit-wise against a maximum
 * value, for example 0x295959. The function takes constant time,
 * about six machine instructions. @a maximum can contain digits 0x0
 * ... 0xF.
 *
 * @return
 * @c TRUE if any digit of @a bcd is greater than the
 * corresponding digit of @a maximum.
 */
_vbi3_inline vbi3_bool
vbi3_bcd_digits_greater		(unsigned int		bcd,
				 unsigned int		maximum)
{
	maximum ^= ~0;

	return 0 != (((bcd + maximum) ^ bcd ^ maximum) & _VBI3_BCD_10);
}

/** @} */

/**
 * @addtogroup Service
 * @{
 */

/**
 * Teletext or Closed Caption page number. For Teletext pages
 * this is a bcd number in range 0x100 ... 0x8FF. Page numbers
 * containing digits 0xA to 0xF are reserved for various system
 * purposes, these pages are not intended for display.
 * 
 * Closed Caption "page numbers" between 1 ... 8 correspond
 * to the four Caption and Text channels CC1 ... CC4 and T1 ... T4.
 */
typedef int vbi3_pgno;

/** Primary synchronous caption service (English). */
#define VBI3_CAPTION_CC1 1

/**
 * Special non-synchronous data that is intended to
 * augment information carried in the program.
 */
#define VBI3_CAPTION_CC2 2

/**
 * Secondary synchronous caption service, usually
 * a second language.
 */
#define VBI3_CAPTION_CC3 3

/** Special non-synchronous data similar to CC2. */
#define VBI3_CAPTION_CC4 4

/** First text service, data usually not program related. */
#define VBI3_CAPTION_T1 5

/**
 * Second text service, additional data usually not
 * program related (ITV data).
 */
#define VBI3_CAPTION_T2 6

/** Additional text channel. */
#define VBI3_CAPTION_T3 7

/** Additional text channel. */
#define VBI3_CAPTION_T4 8

/**
 * This is the subpage number only applicable to Teletext pages,
 * a BCD number in range 0x01 ... 0x79. Pages without subpages
 * have subpage number 0x00.
 *
 * On special "clock" pages (for example listing the current time
 * in different time zones) it can assume values between 0x0000 ...
 * 0x2359 expressing local time (EN 300 706, Section E.2). These are
 * not actually subpages.
 *
 * Finally the non-BCD value 0x3F7F is possible, symbolic
 * @c VBI3_ANY_SUBNO and @c VBI3_NO_SUBNO depending on context.
 */
typedef int vbi3_subno;

/* Magic constants from EN 300 706. */

#define VBI3_ANY_SUBNO ((vbi3_subno) 0x3F7F)
#define VBI3_NO_SUBNO ((vbi3_subno) 0x3F7F)

/** @} */

VBI3_END_DECLS

#ifdef __cplusplus

#include <iostream>
#include <iomanip>

namespace vbi {
  class Bcd {
    int	value;

  public:
    Bcd () {};
    Bcd (int n) : value (n) {};

    operator int () const
      { return value; };

    int bin (void)
      { return vbi3_bcd2bin (value); };
    Bcd& bcd (int n)
      { value = vbi3_bin2bcd (n); return *this; };

    Bcd operator + ()
      { return *this; };
    Bcd operator - ()
      { Bcd t (vbi3_neg_bcd (value)); return t; };

    Bcd& operator += (const Bcd& other)
      { value = vbi3_add_bcd (value, other.value); return *this; };
    Bcd& operator ++ ()
      { value = vbi3_add_bcd (value, 0x1); return *this; };
    Bcd operator ++ (int)
      { Bcd t = *this; ++*this; return t; };

    Bcd& operator -= (const Bcd& other)
      { value = vbi3_sub_bcd (value, other.value); return *this; };
    Bcd& operator -- ()
      { value = vbi3_sub_bcd (value, 0x1); return *this; };
    Bcd operator -- (int)
      { Bcd t = *this; --*this; return t; };

    bool operator == (int n) const
      { return value == n; };
    bool operator == (const Bcd& other) const
      { return value == other.value; };
    bool operator != (int n) const
      { return value != n; };
    bool operator != (const Bcd& other) const
      { return value != other.value; };

    bool valid () const
      { return vbi3_is_bcd (value); };
  };

  static inline Bcd operator +	(Bcd a, const Bcd& b)
    { a += b; return a; }
  static inline Bcd operator -	(Bcd a, const Bcd& b)
    { a -= b; return a; }
  static inline
    std::ostream& operator <<	(std::ostream&		stream,
				 const Bcd&		b)
    {
      std::ios::fmtflags flags = stream.flags ();

      stream << std::hex << (int) b;
      stream.flags (flags);
      return stream;
    }

  typedef Bcd Pgno;
  typedef Bcd Subno;

  static const Bcd any_subno (VBI3_ANY_SUBNO);
  static const Bcd no_subno (VBI3_NO_SUBNO);
}

#endif /* __cplusplus */

#endif /* __ZVBI3_BCD_H__ */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
