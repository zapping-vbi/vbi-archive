/*
 *  libzvbi - Error correction functions
 *
 *  Copyright (C) 2001-2003 Michael H. Schimek
 *
 *  Based on code from AleVT 1.5.1
 *  Copyright (C) 1998, 1999 Edgar Toernig <froese@gmx.de>
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

/* $Id: hamm.h,v 1.4.2.2 2003-06-16 06:03:13 mschimek Exp $ */

#ifndef HAMM_H
#define HAMM_H

#include "misc.h"

/* Public */

#include <inttypes.h>

extern const uint8_t		vbi_bit_reverse [256];
extern const uint8_t		vbi_hamm8_fwd [16];
extern const int8_t		vbi_hamm8_inv [256];
extern const int8_t		vbi_hamm24_inv_par [3][256];

/**
 * @addtogroup Error Error correction functions
 * @ingroup Raw
 * @brief Helper functions to decode sliced VBI data.
 *
 * @{
 */

/**
 * @param c Unsigned byte.
 * 
 * Reverses the bits of the argument.
 * 
 * @return
 * Data bits 0 [msb] ... 7 [lsb].
 */
static_inline unsigned int
vbi_rev8			(uint8_t		c)
{
	return vbi_bit_reverse[c];
}

/**
 * @param p Pointer to a 16 bit word, last significant
 *   byte first.
 * 
 * Reverses the bits of the argument.
 * 
 * @return
 * Data bits 0 [msb] ... 15 [lsb].
 */
static_inline unsigned int
vbi_rev16			(const uint8_t *	p)
{
	return vbi_bit_reverse[p[0]] * 256
		+ vbi_bit_reverse[p[1]];
}

/**
 * @param c Unsigned byte. 
 * 
 * @return
 * If the byte has odd parity (sum of bits modulo 2 is 1) the
 * byte AND 127, otherwise a negative value.
 */
static_inline int
vbi_ipar8			(unsigned int		c)
{
#ifdef __GNUC__
#if #cpu (i686)
	int r = c;

	/* This saves cache flushes and a branch */
	__asm__ (" andl		$127,%0\n"
		 " testb	%1,%1\n"
		 " cmovp	%2,%0\n"
		 : "+&a" (r) : "c" (c), "rm" (-1));
	return r;
#endif
#endif
	if (vbi_hamm24_inv_par[0][(uint8_t) c] & 32)
		return c & 127;
	else
		return -1;
}

extern void
vbi_fpar			(uint8_t *		p,
				 unsigned int		n);
extern int
vbi_ipar			(uint8_t *		p,
				 unsigned int		n);

/**
 * @param c Integer between 0 ... 15.
 * 
 * Encodes a nibble with Hamming 8/4 protection
 * as specified in ETS 300 706, Section 8.2.
 * 
 * @return
 * Hamming encoded unsigned byte, lsb first
 * transmitted.
 */
#define vbi_fham8(c) vbi_hamm8_fwd[(c) & 15]

/**
 * @param c Hamming 8/4 protected byte, lsb first
 *   transmitted.
 * 
 * Decodes a Hamming 8/4 protected byte
 * as specified in ETS 300 706, Section 8.2.
 * 
 * @return
 * Data bits (D4 [msb] ... D1 [lsb]) or a negative
 * value if the byte contained incorrectable errors.
 */
#define vbi_iham8(c) vbi_hamm8_inv[(uint8_t)(c)]

/**
 * @param p Pointer to a Hamming 8/4 protected 16 bit word,
 *   last significant byte first, lsb first transmitted.
 * 
 * Decodes a Hamming 8/4 protected byte pair
 * as specified in ETS 300 706, Section 8.2.
 * 
 * @return
 * Data bits D4 [msb] ... D1 of first byte and D4 ... D1 [lsb]
 * of second byte, or a negative value if any of the bytes
 * contained incorrectable errors.
 */
static_inline int
vbi_iham16			(const uint8_t *	p)
{
	return ((int) vbi_hamm8_inv[p[0]])
	  | (((int) vbi_hamm8_inv[p[1]]) << 4);
}

extern int
vbi_iham24			(const uint8_t *	p);

/** @} */

/* Private */

#endif /* HAMM_H */
