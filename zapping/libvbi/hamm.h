/*
 *  Zapzilla/libvbi - Error correction functions
 *
 *  Copyright (C) 2001 Michael H. Schimek
 *
 *  Based on code from AleVT 1.5.1
 *  Copyright (C) 1998,1999 Edgar Toernig (froese@gmx.de)
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

/* $Id: hamm.h,v 1.8 2001-08-14 16:36:48 mschimek Exp $ */

#ifndef HAMM_H
#define HAMM_H

extern const unsigned char	bit_reverse[256];
extern const char		hamm8val[256];
extern const char               hamm24par[3][256];

/**
 * parity:
 * @c: Unsigned byte. 
 * 
 * Return value:
 * If the byte has odd parity (sum of bits mod 2 is 1) the
 * byte AND 127, otherwise -1.
 **/
static inline int
parity(int c)
{
	if (hamm24par[0][c] & 32)
		return c & 0x7F;
	else
		return -1;
}

static inline int
chk_parity(unsigned char *p, int n)
{
	unsigned int c;
	int err;

	for (err = 0; n--; p++)
		if (hamm24par[0][c = *p] & 32)
			*p = c & 0x7F;
		else
			*p = 0x20, err++;

	return err == 0;
}

static inline void
set_parity(unsigned char *p, int n)
{
	unsigned int c;

	for (; n--; p++)
		if (!(hamm24par[0][c = *p] & 32))
			*p = c | 0x80;
}

/**
 * hamm8:
 * @c: A Hamming 8/4 protected byte, lsb first
 *   transmitted.
 * 
 * This function decodes a Hamming 8/4 protected byte
 * as specified in ETS 300 706 8.2.
 * 
 * Return value: 
 * Data bits (D4 [msb] ... D1 [lsb])
 * or -1 if the byte contained incorrectable errors.
 **/
#define hamm8(c) hamm8val[(unsigned char)(c)]

/**
 * hamm16:
 * @p: Pointer to a Hamming 8/4 protected byte pair,
 *   bytes in transmission order, lsb first transmitted.
 * 
 * This function decodes a Hamming 8/4 protected byte pair
 * as specified in ETS 300 706 8.2.
 * 
 * Return value: 
 * Data bits D4 [msb] ... D1 of byte 1, D4 ... D1 [lsb] of byte 2
 * or a negative value if the pair contained incorrectable errors.
 **/
static inline int
hamm16(unsigned char *p)
{
	return hamm8val[p[0]] | (hamm8val[p[1]] << 4);
}

extern int hamm24(unsigned char *p);

#endif /* HAMM_H */
























