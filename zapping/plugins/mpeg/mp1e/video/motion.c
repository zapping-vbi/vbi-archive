/*
 *  MPEG-1 Real Time Encoder
 *  Motion compensation V3.1.38
 *
 *  Copyright (C) 2001 Michael H. Schimek
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

/* $Id: motion.c,v 1.8 2001-06-07 17:43:51 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "site_def.h"

#include <stdio.h>
#include <assert.h>
#include "common/mmx.h"
#include "common/math.h"
#include "common/profile.h"
#include "mblock.h"
#include "motion.h"

#define AUTOR 1		/* search range estimation (P frames only) */

#ifndef T3RT
#define T3RT 1
#endif

/*
 *  16 x 16 signed/unsigned bytes, no padding
 *  Attention this is aliased on mblock written by search():
 *  mblock[1], [3] in forward; [1], [3] then [2], [4] in bidi,
 *  thus only valid in search() before predict().
 */
#define tbuf ((void *) &mblock[2])

mmx_t bbmin, bbdxy, crdxy, crdy0;

/*
 *  vvv                           |||
 *  a B B B B b b b b C C C C c c c c D
 *   . . . . + + + + . . . . + + + + .
 *   ^^^                           |||
 */

// XXX alias mblock
unsigned char temp22[20][16] __attribute__ ((aligned (32)));
unsigned char temp2h[20][16] __attribute__ ((aligned (32)));
unsigned char temp2v[20][16] __attribute__ ((aligned (32)));
unsigned char temp11[20][16] __attribute__ ((aligned (32)));

static inline void
mmx_load_interp(unsigned char *p, int pitch, int dx, int dy)
{
	unsigned char *p1 = p + dx + dy * pitch;
	int y;

	asm volatile ("
		movq		(%0),%%mm0;
		movq		%%mm0,%%mm1;
		movq		%%mm0,temp11(%1);
		psrlq		$8,%%mm1;
		movq		8(%0),%%mm4;
		movq		%%mm4,%%mm2;
		movq		%%mm4,temp11+8(%1);

		psllq		$56,%%mm2;
		movq		c1b,%%mm7;
		por		%%mm2,%%mm1;
		movq		%%mm0,%%mm2;
		movq		%%mm0,%%mm5;
		por		%%mm1,%%mm2;
		pxor		%%mm1,%%mm5;
		por		%%mm7,%%mm0;
		por		%%mm7,%%mm1;
		psrlq		$1,%%mm0;
		pand		%%mm7,%%mm2;
		psrlq		$1,%%mm1;
		paddb		%%mm0,%%mm1;
		paddb		%%mm2,%%mm1;
		movq		%%mm1,temp2h(%1);

		pushl		%%ecx;
		movzxb		16(%0),%%ecx;
		movd		%%ecx,%%mm2;
		movb		%%cl,temp11+18*16(%2);
		movb		17(%0),%%cl;
		movb		%%cl,temp2h+18*16(%2);
		popl		%%ecx;

		movq		%%mm4,%%mm1;
		psrlq		$8,%%mm1;
		psllq		$56,%%mm2;
		por		%%mm2,%%mm1;
		movq		%%mm4,%%mm2;
		movq		%%mm4,%%mm6;
		por		%%mm1,%%mm2;
		pxor		%%mm1,%%mm6;
		por		%%mm7,%%mm4;
		por		%%mm7,%%mm1;
		psrlq		$1,%%mm4;
		pand		%%mm7,%%mm2;
		psrlq		$1,%%mm1;
		paddb		%%mm4,%%mm1;
		paddb		%%mm2,%%mm1;
		movq		%%mm1,temp2h+8(%1);
	" :: "S" (p1), "c" (0), "r" (0) : "memory");

	for (y = 1; y < 17; y++) {
		asm volatile ("
			movq		(%0),%%mm1;
			movq		%%mm1,temp11(%1);

			movq		temp11-16(%1),%%mm0;
			movq		%%mm0,%%mm2;
			movq		%%mm1,%%mm3;
			por		%%mm1,%%mm2;
			por		%%mm7,%%mm0;
			por		%%mm7,%%mm1;
			psrlq		$1,%%mm0;
			pand		%%mm7,%%mm2;
			psrlq		$1,%%mm1;
			paddb		%%mm0,%%mm1;
			paddb		%%mm2,%%mm1;
			movq		%%mm1,temp2v-16(%1);

			movq		8(%0),%%mm1;
			movq		%%mm1,temp11+8(%1);

			movq		temp11+8-16(%1),%%mm0;
			movq		%%mm0,%%mm2;
			movq		%%mm1,%%mm4;
			por		%%mm1,%%mm2;
			por		%%mm7,%%mm0;
			por		%%mm7,%%mm1;
			psrlq		$1,%%mm0;
			pand		%%mm7,%%mm2;
			psrlq		$1,%%mm1;
			paddb		%%mm0,%%mm1;
			paddb		%%mm2,%%mm1;
			movq		%%mm1,temp2v+8-16(%1);

			movq		%%mm3,%%mm0;
			movq		temp11+1(%1),%%mm1;
			movq		%%mm0,%%mm2;
			movq		%%mm0,%%mm3;
			por		%%mm1,%%mm2;
			pxor		%%mm1,%%mm3;
			por		%%mm7,%%mm0;
			por		%%mm7,%%mm1;
			psrlq		$1,%%mm0;
			pand		%%mm7,%%mm2;
			psrlq		$1,%%mm1;
			paddb		%%mm0,%%mm1;
			paddb		%%mm2,%%mm1;
			movq		%%mm1,temp2h(%1);

			movq		temp2h-16(%1),%%mm0;
			movq		%%mm0,%%mm2;
			pxor		%%mm1,%%mm2;
			por		%%mm3,%%mm5;
			pandn		%%mm2,%%mm5;
			movq		%%mm0,%%mm2;
			pand		%%mm1,%%mm2;
			pxor		%%mm5,%%mm2;
			movq		%%mm3,%%mm5;
			por		%%mm7,%%mm0;
			por		%%mm7,%%mm1;
			psrlq		$1,%%mm0;
			pand		%%mm7,%%mm2;
			psrlq		$1,%%mm1;
			paddb		%%mm1,%%mm0;
			paddb		%%mm2,%%mm0;
			movq		%%mm0,temp22-16(%1);

			pushl		%%ecx;
			movzxb		16(%0),%%ecx;
			movd		%%ecx,%%mm2;
			movb		%%cl,temp11+18*16(%2);
			movb		17(%0),%%cl;
			movb		%%cl,temp2h+18*16(%2);
			popl		%%ecx;

			movq		%%mm4,%%mm1;
			psrlq		$8,%%mm1;
			psllq		$56,%%mm2;
			por		%%mm2,%%mm1;
			movq		%%mm4,%%mm3;
			pxor		%%mm1,%%mm3;
			movq		%%mm4,%%mm2;
			por		%%mm1,%%mm2;
			por		%%mm7,%%mm4;
			por		%%mm7,%%mm1;
			psrlq		$1,%%mm4;
			pand		%%mm7,%%mm2;
			psrlq		$1,%%mm1;
			paddb		%%mm4,%%mm1;
			paddb		%%mm2,%%mm1;
			movq		%%mm1,temp2h+8(%1);

			movq		temp2h+8-16(%1),%%mm0;
			movq		%%mm0,%%mm2;
			pxor		%%mm1,%%mm2;
			por		%%mm3,%%mm6;
			pandn		%%mm2,%%mm6;
			movq		%%mm0,%%mm2;
			pand		%%mm1,%%mm2;
			pxor		%%mm6,%%mm2;
			movq		%%mm3,%%mm6;
			por		%%mm7,%%mm0;
			por		%%mm7,%%mm1;
			psrlq		$1,%%mm0;
			pand		%%mm7,%%mm2;
			psrlq		$1,%%mm1;
			paddb		%%mm0,%%mm1;
			paddb		%%mm2,%%mm1;
			movq		%%mm1,temp22+8-16(%1);
		" :: "S" (p1 + y * pitch), "c" (y * 16), "r" (y) : "memory");
	}

	asm volatile ("
		movq		temp11-16(%1),%%mm0;
		movq		(%0),%%mm1;
		movq		%%mm0,%%mm2;
		movq		%%mm1,%%mm3;
		por		%%mm1,%%mm2;
		por		%%mm7,%%mm0;
		por		%%mm7,%%mm1;
		psrlq		$1,%%mm0;
		pand		%%mm7,%%mm2;
		psrlq		$1,%%mm1;
		paddb		%%mm0,%%mm1;
		paddb		%%mm2,%%mm1;
		movq		%%mm1,temp2v-16(%1);

		movq		temp11+8-16(%1),%%mm0;
		movq		8(%0),%%mm1;
		movq		%%mm0,%%mm2;
		movq		%%mm1,%%mm4;
		por		%%mm1,%%mm2;
		por		%%mm7,%%mm0;
		por		%%mm7,%%mm1;
		psrlq		$1,%%mm0;
		pand		%%mm7,%%mm2;
		psrlq		$1,%%mm1;
		paddb		%%mm0,%%mm1;
		paddb		%%mm2,%%mm1;
		movq		%%mm1,temp2v+8-16(%1);

		movq		%%mm3,%%mm0;
		movq		%%mm3,%%mm1;
		movq		%%mm4,%%mm2;
		psrlq		$8,%%mm1;
		psllq		$56,%%mm2;
		por		%%mm2,%%mm1;
		movq		%%mm0,%%mm2;
		por		%%mm1,%%mm2;
		pxor		%%mm1,%%mm3;
		por		%%mm7,%%mm0;
		por		%%mm7,%%mm1;
		psrlq		$1,%%mm0;
		pand		%%mm7,%%mm2;
		psrlq		$1,%%mm1;
		paddb		%%mm0,%%mm1;
		movq		temp2h-16(%1),%%mm0;
		por		%%mm3,%%mm5;
		paddb		%%mm2,%%mm1;
		movq		%%mm0,%%mm2;
		pxor		%%mm1,%%mm2;
		pandn		%%mm2,%%mm5;
		movq		%%mm0,%%mm2;
		pand		%%mm1,%%mm2;
		pxor		%%mm5,%%mm2;
		por		%%mm7,%%mm0;
		por		%%mm7,%%mm1;
		psrlq		$1,%%mm0;
		pand		%%mm7,%%mm2;
		psrlq		$1,%%mm1;
		paddb		%%mm0,%%mm1;
		paddb		%%mm2,%%mm1;
		movq		%%mm1,temp22-16(%1);

		movq		%%mm4,%%mm0;
		movq		%%mm4,%%mm1;
		psrlq		$8,%%mm1;
		pushl		%%edx;
		movzxb		16(%0),%%edx;
		movd		%%edx,%%mm2;
		popl		%%edx;
		psllq		$56,%%mm2;
		por		%%mm2,%%mm1;
		movq		%%mm0,%%mm2;
		por		%%mm1,%%mm2;
		pxor		%%mm1,%%mm4;
		por		%%mm7,%%mm0;
		por		%%mm7,%%mm1;
		psrlq		$1,%%mm0;
		pand		%%mm7,%%mm2;
		psrlq		$1,%%mm1;
		paddb		%%mm0,%%mm1;
		paddb		%%mm2,%%mm1;
		movq		temp2h+8-16(%1),%%mm0;
		movq		%%mm0,%%mm2;
		pxor		%%mm1,%%mm2;
		por		%%mm4,%%mm6;
		pandn		%%mm2,%%mm6;
		movq		%%mm0,%%mm2;
		pand		%%mm1,%%mm2;
		pxor		%%mm6,%%mm2;
		por		%%mm7,%%mm0;
		por		%%mm7,%%mm1;
		psrlq		$1,%%mm0;
		pand		%%mm7,%%mm2;
		psrlq		$1,%%mm1;
		paddb		%%mm0,%%mm1;
		paddb		%%mm2,%%mm1;
		movq		%%mm1,temp22+8-16(%1);

		/* temp2v 16 */

		movq		temp11+18*16+0,%%mm0;
		movq		%%mm0,%%mm3;
		movq		temp11+18*16+1,%%mm1;
		movq		%%mm0,%%mm2;
		por		%%mm1,%%mm2;
		por		%%mm7,%%mm0;
		por		%%mm7,%%mm1;
		psrlq		$1,%%mm0;
		pand		%%mm7,%%mm2;
		psrlq		$1,%%mm1;
		paddb		%%mm0,%%mm1;
		paddb		%%mm2,%%mm1;
		movq		%%mm1,temp2v+18*16+0;

		movq		temp11+18*16+8,%%mm0;
		movq		%%mm0,%%mm4;
		movq		%%mm0,%%mm1;
		psrlq		$8,%%mm1;
		pushl		%%ecx;
		pushl		%%edx;
		movzxb		temp11+18*16+16,%%ecx;
		movd		%%ecx,%%mm2;
		movzxb		16(%0),%%edx;
		addl		%%edx,%%ecx;
		incl		%%ecx;
		shrl		$1,%%ecx;
		movb		%%cl,temp2v+18*16+16;
		popl		%%edx;
		popl		%%ecx;
		psllq		$56,%%mm2;
		por		%%mm2,%%mm1;
		movq		%%mm0,%%mm2;
		por		%%mm1,%%mm2;
		por		%%mm7,%%mm0;
		por		%%mm7,%%mm1;
		psrlq		$1,%%mm0;
		pand		%%mm7,%%mm2;
		psrlq		$1,%%mm1;
		paddb		%%mm0,%%mm1;
		paddb		%%mm2,%%mm1;
		movq		%%mm1,temp2v+18*16+8;

		/* temp2h 16 */

		movq		temp2h+18*16+0,%%mm0;
		movq		%%mm3,%%mm2;
		movq		%%mm0,%%mm5;
		por		%%mm0,%%mm2;
		pxor		%%mm3,%%mm5;
		por		%%mm7,%%mm3;
		por		%%mm7,%%mm0;
		psrlq		$1,%%mm3;
		pand		%%mm7,%%mm2;
		psrlq		$1,%%mm0;
		paddb		%%mm3,%%mm0;
		paddb		%%mm2,%%mm0;
		movq		%%mm0,temp2h+18*16+0;

		movq		temp2h+18*16+8,%%mm1;
		movq		%%mm4,%%mm2;
		movq		%%mm1,%%mm6;
		por		%%mm1,%%mm2;
		pxor		%%mm4,%%mm6;
		por		%%mm7,%%mm4;
		por		%%mm7,%%mm1;
		psrlq		$1,%%mm4;
		pand		%%mm7,%%mm2;
		psrlq		$1,%%mm1;
		paddb		%%mm4,%%mm1;
		paddb		%%mm2,%%mm1;
		movq		%%mm1,temp2h+18*16+8;

		pushl		%%ecx;
		pushl		%%edx;
		movzxb		temp11+18*16+16,%%ecx;
		movzxb		temp2h+18*16+16,%%edx;
		addl		%%edx,%%ecx;
		movd		%%ecx,%%mm4;
		incl		%%ecx;
		movl		%%ecx,%%edx;
		shrl		$1,%%ecx;
		movb		%%cl,temp2h+18*16+16;
		movzxb		16(%0),%%ecx;
		addl		%%ecx,%%edx;
		movzxb		17(%0),%%ecx;
		addl		%%ecx,%%edx;
		incl		%%edx;
		shrl		$2,%%edx;
		movb		%%dl,temp22+18*16+16;
		popl		%%edx;
		popl		%%ecx;

		/* temp22 16 [1 & ((ab & cd) ^ ((ab ^ cd) & ~((a ^ b) | (c ^ d))))] */

		movq		%%mm5,%%mm2;
		movq		%%mm6,%%mm3;
		psrlq		$8,%%mm2;
		psllq		$56,%%mm3;
		por		%%mm2,%%mm5;
		por		%%mm3,%%mm5;
		movq		temp2h+18*16+1,%%mm3;
		movq		%%mm0,%%mm2;
		pxor		%%mm3,%%mm2;
		pandn		%%mm2,%%mm5;
		movq		%%mm0,%%mm2;
		pand		%%mm3,%%mm2;
		pxor		%%mm2,%%mm5;
		pand		%%mm7,%%mm5;
		por		%%mm7,%%mm3;
		por		%%mm7,%%mm0;
		psrlq		$1,%%mm3;
		psrlq		$1,%%mm0;
		paddb		%%mm3,%%mm0;
		paddb		%%mm5,%%mm0;
		movq		%%mm0,temp22+18*16+0;

		movq		%%mm6,%%mm2;
		psrlq		$8,%%mm2;
		psllq		$56,%%mm4;
		por		%%mm2,%%mm6;
		por		%%mm4,%%mm6;
		movq		temp2h+18*16+9,%%mm4;
		movq		%%mm1,%%mm2;
		pxor		%%mm4,%%mm2;
		pandn		%%mm2,%%mm6;
		movq		%%mm1,%%mm2;
		pand		%%mm4,%%mm2;
		pxor		%%mm2,%%mm6;
		pand		%%mm7,%%mm6;
		por		%%mm7,%%mm4;
		por		%%mm7,%%mm1;
		psrlq		$1,%%mm4;
		psrlq		$1,%%mm1;
		paddb		%%mm4,%%mm1;
		paddb		%%mm6,%%mm1;
		movq		%%mm1,temp22+18*16+8;

	" :: "S" (p1 + y * pitch), "c" (y * 16), "r" (y) : "memory");
}

static unsigned int
mmx_sad2h(unsigned char ref[16][16], typeof(temp22) temp, int hy, int *r2)
{
	unsigned char *s = &temp[hy][0];
	unsigned char *t = &ref[0][0];
	unsigned char *l = &temp[18][hy];
	int y;

	asm volatile ("
		movq		(%0),%%mm0;
		movq		(%1),%%mm1;
		pxor		%%mm5,%%mm5;
		pxor		%%mm6,%%mm6;
		pxor		%%mm7,%%mm7;

	" :: "r" (s), "r" (t), "r" (l));

	for (y = 0; y < 15; y++) {
	asm volatile ("
		movq		1(%0),%%mm3;
		movq		%%mm0,%%mm2;
		movq		%%mm1,%%mm4;
		psubusb		%%mm1,%%mm0;
		psubusb		%%mm2,%%mm1;
		por		%%mm1,%%mm0;
		movq		%%mm0,%%mm1;
		movq		%%mm3,%%mm2;
		punpcklbw	%%mm5,%%mm0;
		paddw		%%mm0,%%mm6;
		movq		8(%0),%%mm0;
		punpckhbw	%%mm5,%%mm1;
		paddw		%%mm1,%%mm6;

		movq		8(%1),%%mm1;
		psubusb		%%mm4,%%mm3;
		psubusb		%%mm2,%%mm4;
		movq		(%2),%%mm2;
		por		%%mm4,%%mm3;
		movq		%%mm3,%%mm4;
		punpcklbw	%%mm5,%%mm3;
		paddw		%%mm3,%%mm7;
		movq		%%mm0,%%mm3;
		punpckhbw	%%mm5,%%mm4;
		paddw		%%mm4,%%mm7;

		movq		%%mm1,%%mm4;
		psubusb		%%mm1,%%mm0;
		psubusb		%%mm3,%%mm1;
		psrlq		$8,%%mm3;
		por		%%mm1,%%mm0;
		movq		%%mm0,%%mm1;
		punpcklbw	%%mm5,%%mm0;
		paddw		%%mm0,%%mm6;
		movq		16(%0),%%mm0;
		punpckhbw	%%mm5,%%mm1;
		paddw		%%mm1,%%mm6;

		movq		16(%1),%%mm1;
		psllq		$56,%%mm2;
		por		%%mm2,%%mm3;
		movq		%%mm3,%%mm2;
		psubusb		%%mm4,%%mm3;
		psubusb		%%mm2,%%mm4;
		por		%%mm4,%%mm3;
		movq		%%mm3,%%mm4;
		punpcklbw	%%mm5,%%mm3;
		paddw		%%mm3,%%mm7;
		punpckhbw	%%mm5,%%mm4;
		paddw		%%mm4,%%mm7;

		" :: "r" (s), "r" (t), "r" (l));

		s += 16;
		t += 16;
		l += 1;
	}

	asm volatile ("
		movq		1(%0),%%mm3;
		movq		%%mm0,%%mm2;
		movq		%%mm1,%%mm4;
		psubusb		%%mm1,%%mm0;
		psubusb		%%mm2,%%mm1;
		por		%%mm1,%%mm0;
		movq		%%mm0,%%mm1;
		punpcklbw	%%mm5,%%mm0;
		paddw		%%mm0,%%mm6;
		movq		8(%0),%%mm0;
		punpckhbw	%%mm5,%%mm1;
		paddw		%%mm1,%%mm6;
		movq		8(%1),%%mm1;

		movq		%%mm3,%%mm2;
		psubusb		%%mm4,%%mm3;
		psubusb		%%mm2,%%mm4;
		movq		(%2),%%mm2;
		por		%%mm4,%%mm3;
		movq		%%mm3,%%mm4;
		punpcklbw	%%mm5,%%mm3;
		paddw		%%mm3,%%mm7;
		movq		%%mm0,%%mm3;
		punpckhbw	%%mm5,%%mm4;
		paddw		%%mm4,%%mm7;

		movq		%%mm1,%%mm4;
		psubusb		%%mm1,%%mm0;
		psubusb		%%mm3,%%mm1;
		por		%%mm1,%%mm0;
		movq		%%mm0,%%mm1;
		punpcklbw	%%mm5,%%mm0;
		paddw		%%mm0,%%mm6;
		punpckhbw	%%mm5,%%mm1;
		paddw		%%mm1,%%mm6;

		psrlq		$8,%%mm3;
		psllq		$56,%%mm2;
		por		%%mm2,%%mm3;
		movq		%%mm3,%%mm2;
		psubusb		%%mm4,%%mm3;
		psubusb		%%mm2,%%mm4;
		por		%%mm4,%%mm3;
		movq		%%mm3,%%mm4;
		punpcklbw	%%mm5,%%mm3;
		paddw		%%mm3,%%mm7;
		punpckhbw	%%mm5,%%mm4;
		paddw		%%mm4,%%mm7;

	" :: "r" (s), "r" (t), "r" (l));

	// XXX this is supposed to sort
	asm volatile ("
		movq		%%mm7,%%mm0;
		punpcklwd	%%mm6,%%mm0;
		punpckhwd	%%mm6,%%mm7;
		paddw		%%mm0,%%mm7;

		movq		%%mm7,%%mm0;
		psrlq		$32,%%mm0;
		paddw		%%mm0,%%mm7;
		movd		%%mm7,%0;
		punpcklwd	%%mm5,%%mm7;
		movd		%%mm7,(%1);
		shrl		$16,%0;
	" : "=&a" (y) : "r" (r2));

	return y; // left, r2 = right
}

static unsigned int
mmx_sad2v(unsigned char ref[16][16], typeof(temp22) temp, int hx, int *r2)
{
	unsigned char *s = &temp[0][0];
	unsigned char *t = &ref[0][0];
	unsigned char *l = &temp[18][0];
	int y;

	if (hx == 0) {
	asm volatile ("
		movq		(%0),%%mm3;	
		movq		8(%0),%%mm4;
		movq		(%1),%%mm1;
		movq		16(%0),%%mm0;
		pxor		%%mm5,%%mm5;
		pxor		%%mm6,%%mm6;
		pxor		%%mm7,%%mm7;

	" :: "r" (s), "r" (t), "r" (l));

		for (y = 0; y < 15; y++) {
	asm volatile ("
		movq		%%mm1,%%mm2;
		psubusb		%%mm3,%%mm1;
		psubusb		%%mm2,%%mm3;
		por		%%mm1,%%mm3;
		movq		%%mm3,%%mm1;
		punpckhbw	%%mm5,%%mm1;
		paddw		%%mm1,%%mm6;
		movq		8(%1),%%mm1;
		punpcklbw	%%mm5,%%mm3;
		paddw		%%mm3,%%mm6;

		movq		%%mm0,%%mm3;
		psubusb		%%mm2,%%mm0;
		psubusb		%%mm3,%%mm2;
		por		%%mm2,%%mm0;
		movq		%%mm0,%%mm2;
		punpcklbw	%%mm5,%%mm0;
		paddw		%%mm0,%%mm7;
		movq		24(%0),%%mm0;
		punpckhbw	%%mm5,%%mm2;
		paddw		%%mm2,%%mm7;

		movq		%%mm1,%%mm2;
		psubusb		%%mm4,%%mm1;
		psubusb		%%mm2,%%mm4;
		por		%%mm1,%%mm4;
		movq		%%mm4,%%mm1;
		punpckhbw	%%mm5,%%mm1;
		paddw		%%mm1,%%mm6;
		movq		16(%1),%%mm1;
		punpcklbw	%%mm5,%%mm4;
		paddw		%%mm4,%%mm6;

		movq		%%mm0,%%mm4;
		psubusb		%%mm2,%%mm0;
		psubusb		%%mm4,%%mm2;
		por		%%mm2,%%mm0;
		movq		%%mm0,%%mm2;
		punpcklbw	%%mm5,%%mm0;
		paddw		%%mm0,%%mm7;
		movq		32(%0),%%mm0;
		punpckhbw	%%mm5,%%mm2;
		paddw		%%mm2,%%mm7;

	" :: "r" (s), "r" (t), "r" (l));

			s += 16;
			t += 16;
		}

	asm volatile ("
		movq		%%mm1,%%mm2;
		psubusb		%%mm3,%%mm1;
		psubusb		%%mm2,%%mm3;
		por		%%mm1,%%mm3;
		movq		%%mm3,%%mm1;
		punpckhbw	%%mm5,%%mm1;
		paddw		%%mm1,%%mm6;
		movq		8(%1),%%mm1;
		punpcklbw	%%mm5,%%mm3;
		paddw		%%mm3,%%mm6;

		movq		%%mm0,%%mm3;
		psubusb		%%mm2,%%mm0;
		psubusb		%%mm3,%%mm2;
		por		%%mm2,%%mm0;
		movq		%%mm0,%%mm2;
		punpcklbw	%%mm5,%%mm0;
		paddw		%%mm0,%%mm7;
		movq		24(%0),%%mm0;

	" :: "r" (s), "r" (t), "r" (l));

	} else {

	asm volatile ("
		movq		8(%0),%%mm4;
		psrlq		$8,%%mm4;
		movq		(%2),%%mm2;
		psllq		$56,%%mm2;
		por		%%mm2,%%mm4;

		movq		1(%0),%%mm3;
		movq		(%1),%%mm1;
		movq		17(%0),%%mm0;

		pxor		%%mm5,%%mm5;
		pxor		%%mm6,%%mm6;
		pxor		%%mm7,%%mm7;

	" :: "r" (s), "r" (t), "r" (l));

		for (y = 0; y < 15; y++) {
	asm volatile ("
		movq		%%mm1,%%mm2;
		psubusb		%%mm3,%%mm1;
		psubusb		%%mm2,%%mm3;
		por		%%mm1,%%mm3;
		movq		%%mm3,%%mm1;
		punpckhbw	%%mm5,%%mm1;
		paddw		%%mm1,%%mm6;
		movq		8(%1),%%mm1;
		punpcklbw	%%mm5,%%mm3;
		paddw		%%mm3,%%mm6;

		movq		%%mm0,%%mm3;
		psubusb		%%mm2,%%mm0;
		psubusb		%%mm3,%%mm2;
		por		%%mm2,%%mm0;
		movq		%%mm0,%%mm2;
		punpcklbw	%%mm5,%%mm0;
		paddw		%%mm0,%%mm7;
		movq		24(%0),%%mm0;
		punpckhbw	%%mm5,%%mm2;
		paddw		%%mm2,%%mm7;

		movq		%%mm1,%%mm2;
		psubusb		%%mm4,%%mm1;
		psubusb		%%mm2,%%mm4;
		psrlq		$8,%%mm0;
		por		%%mm1,%%mm4;
		movq		%%mm4,%%mm1;
		punpckhbw	%%mm5,%%mm1;
		paddw		%%mm1,%%mm6;
		movq		16(%1),%%mm1;
		punpcklbw	%%mm5,%%mm4;
		paddw		%%mm4,%%mm6;

		movq		1(%2),%%mm4;
		psllq		$56,%%mm4;
		por		%%mm4,%%mm0;
		movq		%%mm0,%%mm4;
		psubusb		%%mm2,%%mm0;
		psubusb		%%mm4,%%mm2;
		por		%%mm2,%%mm0;
		movq		%%mm0,%%mm2;
		punpcklbw	%%mm5,%%mm0;
		paddw		%%mm0,%%mm7;
		movq		33(%0),%%mm0;
		punpckhbw	%%mm5,%%mm2;
		paddw		%%mm2,%%mm7;

	" :: "r" (s), "r" (t), "r" (l));

			s += 16;
			t += 16;
			l += 1;
		}

	asm volatile ("
		movq		%%mm1,%%mm2;
		psubusb		%%mm3,%%mm1;
		psubusb		%%mm2,%%mm3;
		por		%%mm1,%%mm3;
		movq		%%mm3,%%mm1;
		punpckhbw	%%mm5,%%mm1;
		paddw		%%mm1,%%mm6;
		movq		8(%1),%%mm1;
		punpcklbw	%%mm5,%%mm3;
		paddw		%%mm3,%%mm6;

		movq		%%mm0,%%mm3;
		psubusb		%%mm2,%%mm0;
		psubusb		%%mm3,%%mm2;
		movq		1(%2),%%mm3;
		por		%%mm2,%%mm0;
		psllq		$56,%%mm3;
		movq		%%mm0,%%mm2;
		punpcklbw	%%mm5,%%mm0;
		paddw		%%mm0,%%mm7;

		movq		24(%0),%%mm0;
		psrlq		$8,%%mm0;
		por		%%mm3,%%mm0;

	" :: "r" (s), "r" (t), "r" (l));

	}

	asm volatile ("
		punpckhbw	%mm5,%mm2;
		paddw		%mm2,%mm7;

		movq		%mm1,%mm2;
		psubusb		%mm4,%mm1;
		psubusb		%mm2,%mm4;
		por		%mm1,%mm4;
		movq		%mm4,%mm1;
		punpcklbw	%mm5,%mm4;
		paddw		%mm4,%mm6;
		punpckhbw	%mm5,%mm1;
		paddw		%mm1,%mm6;

		movq		%mm0,%mm4;
		psubusb		%mm2,%mm0;
		psubusb		%mm4,%mm2;
		por		%mm2,%mm0;
		movq		%mm0,%mm2;
		punpcklbw	%mm5,%mm0;
		paddw		%mm0,%mm7;
		punpckhbw	%mm5,%mm2;
		paddw		%mm2,%mm7;

	// XXX this is supposed to sort

		movq		%mm7,%mm0;
		punpcklwd	%mm6,%mm0;
		punpckhwd	%mm6,%mm7;
		paddw		%mm0,%mm7;
	");

	asm volatile ("
		movq		%%mm7,%%mm0;
		psrlq		$32,%%mm0;
		paddw		%%mm0,%%mm7;
		movd		%%mm7,%0;
		punpcklwd	%%mm5,%%mm7;
		movd		%%mm7,(%1);
		shrl		$16,%0;
	" : "=&a" (y) : "r" (r2));

	return y; // left, r2 = right
}

static inline void
mmx_load_ref(unsigned char t[16][16])
{
	asm volatile ("
		movq		0*16+0(%0),%%mm0;
		packuswb	0*16+8(%0),%%mm0;
		movq		1*16+0(%0),%%mm2;
		packuswb	1*16+8(%0),%%mm2;
		movq		256+0*16+0(%0),%%mm1;
		packuswb	256+0*16+8(%0),%%mm1;
		movq		%%mm0,0*16+0(%1);
		movq		256+1*16+0(%0),%%mm3;
		packuswb	256+1*16+8(%0),%%mm3;
		movq		%%mm1,0*16+8(%1);
		movq		2*16+0(%0),%%mm4;
		packuswb	2*16+8(%0),%%mm4;
		movq		%%mm2,1*16+0(%1);
		movq		3*16+0(%0),%%mm6;
		packuswb	3*16+8(%0),%%mm6;
		movq		%%mm3,1*16+8(%1);
		movq		256+2*16+0(%0),%%mm5;
		packuswb	256+2*16+8(%0),%%mm5;
		movq		%%mm4,2*16+0(%1);
		movq		256+3*16+0(%0),%%mm7;
		packuswb	256+3*16+8(%0),%%mm7;
		movq		%%mm5,2*16+8(%1);
		movq		4*16+0(%0),%%mm0;
		packuswb	4*16+8(%0),%%mm0;
		movq		%%mm6,3*16+0(%1);
		movq		5*16+0(%0),%%mm2;
		packuswb	5*16+8(%0),%%mm2;
		movq		%%mm7,3*16+8(%1);
		movq		256+4*16+0(%0),%%mm1;
		packuswb	256+4*16+8(%0),%%mm1;
		movq		%%mm0,4*16+0(%1);
		movq		256+5*16+0(%0),%%mm3;
		packuswb	256+5*16+8(%0),%%mm3;
		movq		%%mm1,4*16+8(%1);
		movq		6*16+0(%0),%%mm4;
		packuswb	6*16+8(%0),%%mm4;
		movq		%%mm2,5*16+0(%1);
		movq		7*16+0(%0),%%mm6;
		packuswb	7*16+8(%0),%%mm6;
		movq		%%mm3,5*16+8(%1);
		movq		256+6*16+0(%0),%%mm5;
		packuswb	256+6*16+8(%0),%%mm5;
		movq		%%mm4,6*16+0(%1);
		movq		256+7*16+0(%0),%%mm7;
		packuswb	256+7*16+8(%0),%%mm7;
		movq		%%mm5,6*16+8(%1);
		movq		8*16+0(%0),%%mm0;
		packuswb	8*16+8(%0),%%mm0;
		movq		%%mm6,7*16+0(%1);
		movq		9*16+0(%0),%%mm2;
		packuswb	9*16+8(%0),%%mm2;
		movq		%%mm7,7*16+8(%1);
		movq		256+8*16+0(%0),%%mm1;
		packuswb	256+8*16+8(%0),%%mm1;
		movq		%%mm0,8*16+0(%1);
		movq		256+9*16+0(%0),%%mm3;
		packuswb	256+9*16+8(%0),%%mm3;
		movq		%%mm1,8*16+8(%1);
		movq		10*16+0(%0),%%mm4;
		packuswb	10*16+8(%0),%%mm4;
		movq		%%mm2,9*16+0(%1);
		movq		11*16+0(%0),%%mm6;
		packuswb	11*16+8(%0),%%mm6;
		movq		%%mm3,9*16+8(%1);
		movq		256+10*16+0(%0),%%mm5;
		packuswb	256+10*16+8(%0),%%mm5;
		movq		%%mm4,10*16+0(%1);
		movq		256+11*16+0(%0),%%mm7;
		packuswb	256+11*16+8(%0),%%mm7;
		movq		%%mm5,10*16+8(%1);
		movq		12*16+0(%0),%%mm0;
		packuswb	12*16+8(%0),%%mm0;
		movq		%%mm6,11*16+0(%1);
		movq		13*16+0(%0),%%mm2;
		packuswb	13*16+8(%0),%%mm2;
		movq		%%mm7,11*16+8(%1);
		movq		256+12*16+0(%0),%%mm1;
		packuswb	256+12*16+8(%0),%%mm1;
		movq		%%mm0,12*16+0(%1);
		movq		256+13*16+0(%0),%%mm3;
		packuswb	256+13*16+8(%0),%%mm3;
		movq		%%mm1,12*16+8(%1);
		movq		14*16+0(%0),%%mm4;
		packuswb	14*16+8(%0),%%mm4;
		movq		%%mm2,13*16+0(%1);
		movq		15*16+0(%0),%%mm6;
		packuswb	15*16+8(%0),%%mm6;
		movq		%%mm3,13*16+8(%1);
		movq		256+14*16+0(%0),%%mm5;
		packuswb	256+14*16+8(%0),%%mm5;
		movq		%%mm4,14*16+0(%1);
		movq		256+15*16+0(%0),%%mm7;
		packuswb	256+15*16+8(%0),%%mm7;
		movq		%%mm5,14*16+8(%1);
		movq		%%mm6,15*16+0(%1);
		movq		%%mm7,15*16+8(%1);

	" :: "S" (&mblock[0][0][0][0]), "D" (&t[0][0]) : "memory");
}

static inline void
mmx_psse_4(char t[16][16], char *p, int pitch)
{
	asm volatile ("
		movq		(%0),%%mm0;
		movq		(%1),%%mm1;
		movq		8(%0),%%mm4;

		movq		%%mm0,%%mm2;
		movq		%%mm1,%%mm3;
		pcmpgtb		%%mm1,%%mm2;		// 1 < 0
		psubb		%%mm0,%%mm1;		// 1 = 1 - 0
		movq		%%mm1,%%mm6;
		punpcklbw	%%mm2,%%mm6;
		pmullw		%%mm6,%%mm6;
		punpckhbw	%%mm2,%%mm1;
		pmullw		%%mm1,%%mm1;
		paddusw		%%mm1,%%mm6;

		movq		8(%1),%%mm1;

		psrlq		$32,%%mm0;
		movq		%%mm4,%%mm2;
		psllq		$32,%%mm2;
		por		%%mm2,%%mm0;

		movq		(%0,%2),%%mm5;

		movq		%%mm4,%%mm2;
		movq		%%mm1,%%mm3;
		pcmpgtb		%%mm1,%%mm2;
		psubb		%%mm4,%%mm1;
		movq		%%mm1,%%mm0;
		punpcklbw	%%mm2,%%mm0;
		pmullw		%%mm0,%%mm0;
		paddusw		%%mm0,%%mm6;
		movd		16(%0),%%mm0;
		punpckhbw	%%mm2,%%mm1;
		pmullw		%%mm1,%%mm1;
		paddusw		%%mm1,%%mm6;

		movq		16(%1),%%mm1;

		psrlq		$32,%%mm4;
		psllq		$32,%%mm0;
		por		%%mm4,%%mm0;

		movq		8(%0,%2),%%mm0;

		movq		%%mm5,%%mm2;
		movq		%%mm1,%%mm3;
		pcmpgtb		%%mm1,%%mm2;
		psubb		%%mm5,%%mm1;
		movq		%%mm1,%%mm4;
		punpcklbw	%%mm2,%%mm4;
		pmullw		%%mm4,%%mm4;
		paddusw		%%mm4,%%mm6;
		movq		24(%1),%%mm4;
		punpckhbw	%%mm2,%%mm1;
		pmullw		%%mm1,%%mm1;
		paddusw		%%mm1,%%mm6;

		movd		16(%0,%2),%%mm5;

		movq		%%mm0,%%mm2;
		movq		%%mm4,%%mm3;
		pcmpgtb		%%mm4,%%mm2;
		psubb		%%mm0,%%mm4;
		movq		%%mm4,%%mm1;
		punpcklbw	%%mm2,%%mm1;
		pmullw		%%mm1,%%mm1;
		paddusw		%%mm1,%%mm6;
		movq		bbmin,%%mm1;
		punpckhbw	%%mm2,%%mm4;
		movq		bbdxy,%%mm2;
		pmullw		%%mm4,%%mm4;

		psrlq		$32,%%mm0;
		paddusw		%%mm4,%%mm6;
		movq		crdxy,%%mm4;
		psllq		$32,%%mm5;
		psubw		c1_15w,%%mm6;
		por		%%mm5,%%mm0;
		paddb		c4,%%mm4;

		movq		%%mm0,%%mm5;
		pcmpgtb		%%mm3,%%mm5;
		psubb		%%mm0,%%mm3;
		movq		%%mm3,%%mm0;
		punpcklbw	%%mm5,%%mm0;
		pmullw		%%mm0,%%mm0;
		paddusw		%%mm0,%%mm7;
		punpckhbw	%%mm5,%%mm3;
		pmullw		%%mm3,%%mm3;
		paddusw		%%mm3,%%mm7;

		movq		%%mm4,crdxy;

		movq		%%mm4,%%mm5;
		pxor		%%mm3,%%mm3;
		pcmpgtb		%%mm4,%%mm3;
		pxor		%%mm3,%%mm5;
		psubb		%%mm3,%%mm5;

		movq		%%mm5,%%mm3;
		psrlw		$8,%%mm5;
		paddsw		%%mm5,%%mm6;
		pand		c255,%%mm3;
		paddsw		%%mm3,%%mm6;

		movq		%%mm1,%%mm5;
		pcmpgtw		%%mm6,%%mm5;
		movq		%%mm1,%%mm3;
		pxor		%%mm6,%%mm3;
		pand		%%mm5,%%mm3;
		pxor		%%mm3,%%mm6;
		pxor		%%mm3,%%mm1;
		pxor		%%mm4,%%mm2;
		pand		%%mm2,%%mm5;
		pxor		%%mm5,%%mm4;
		pxor		%%mm4,%%mm2;

		movq		%%mm6,%%mm5;
		psllq		$16,%%mm6;
		psrlq		$48,%%mm5;
		por		%%mm5,%%mm6;
		movq		%%mm4,%%mm5;
		psllq		$16,%%mm4;
		psrlq		$48,%%mm5;
		por		%%mm5,%%mm4;

		movq		%%mm1,%%mm5;
		pcmpgtw		%%mm6,%%mm5;
		movq		%%mm1,%%mm3;
		pxor		%%mm6,%%mm3;
		pand		%%mm5,%%mm3;
		pxor		%%mm3,%%mm6;
		pxor		%%mm3,%%mm1;
		pxor		%%mm4,%%mm2;
		pand		%%mm2,%%mm5;
		pxor		%%mm5,%%mm4;
		pxor		%%mm4,%%mm2;

		movq		%%mm6,%%mm5;
		psllq		$16,%%mm6;
		psrlq		$48,%%mm5;
		por		%%mm5,%%mm6;
		movq		%%mm4,%%mm5;
		psllq		$16,%%mm4;
		psrlq		$48,%%mm5;
		por		%%mm5,%%mm4;

		movq		%%mm1,%%mm5;
		pcmpgtw		%%mm6,%%mm5;
		movq		%%mm1,%%mm3;
		pxor		%%mm6,%%mm3;
		pand		%%mm5,%%mm3;
		pxor		%%mm3,%%mm6;
		pxor		%%mm3,%%mm1;
		pxor		%%mm4,%%mm2;
		pand		%%mm2,%%mm5;
		pxor		%%mm5,%%mm4;
		pxor		%%mm4,%%mm2;

		movq		%%mm6,%%mm5;
		psllq		$16,%%mm6;
		psrlq		$48,%%mm5;
		por		%%mm5,%%mm6;
		movq		%%mm4,%%mm5;
		psllq		$16,%%mm4;
		psrlq		$48,%%mm5;
		por		%%mm5,%%mm4;

		movq		%%mm1,%%mm5;
		pcmpgtw		%%mm6,%%mm5;
		movq		%%mm1,%%mm3;
		pxor		%%mm6,%%mm3;
		pand		%%mm5,%%mm3;
		pxor		%%mm3,%%mm6;
		pxor		%%mm3,%%mm1;
		pxor		%%mm4,%%mm2;
		pand		%%mm2,%%mm5;
		pxor		%%mm5,%%mm4;
		pxor		%%mm4,%%mm2;

		movq		%%mm1,bbmin;
		movq		%%mm2,bbdxy;

	" :: "r" (p), "r" (t), "r" (pitch * 8 /* ch */) : "memory");
}

static inline void
mmx_psse_8(char t[16][16], char *p, int pitch)
{
	asm volatile ("
		movq		(%0),%%mm0;
		movq		(%1),%%mm1;
		movq		8(%0),%%mm4;

		movq		%%mm0,%%mm2;
		movq		%%mm1,%%mm3;
		pcmpgtb		%%mm1,%%mm2;		// 1 < 0
		psubb		%%mm0,%%mm1;		// 1 = 1 - 0
		movq		%%mm1,%%mm6;
		punpcklbw	%%mm2,%%mm6;
		pmullw		%%mm6,%%mm6;
		punpckhbw	%%mm2,%%mm1;
		pmullw		%%mm1,%%mm1;
		paddusw		%%mm1,%%mm6;

		movq		8(%1),%%mm1;

		psrlq		$32,%%mm0;
		movq		%%mm4,%%mm2;
		psllq		$32,%%mm2;
		por		%%mm2,%%mm0;

		movq		%%mm0,%%mm2;
		pcmpgtb		%%mm3,%%mm2;
		psubb		%%mm0,%%mm3;
		movq		%%mm3,%%mm7;
		punpcklbw	%%mm2,%%mm7;
		pmullw		%%mm7,%%mm7;
		punpckhbw	%%mm2,%%mm3;
		pmullw		%%mm3,%%mm3;
		movq		(%0,%2),%%mm5;
		paddusw		%%mm3,%%mm7;

		movq		%%mm4,%%mm2;
		movq		%%mm1,%%mm3;
		pcmpgtb		%%mm1,%%mm2;
		psubb		%%mm4,%%mm1;
		movq		%%mm1,%%mm0;
		punpcklbw	%%mm2,%%mm0;
		pmullw		%%mm0,%%mm0;
		paddusw		%%mm0,%%mm6;
		movd		16(%0),%%mm0;
		punpckhbw	%%mm2,%%mm1;
		pmullw		%%mm1,%%mm1;
		paddusw		%%mm1,%%mm6;

		movq		16(%1),%%mm1;

		psrlq		$32,%%mm4;
		psllq		$32,%%mm0;
		por		%%mm4,%%mm0;

		movq		%%mm0,%%mm2;
		pcmpgtb		%%mm3,%%mm2;
		psubb		%%mm0,%%mm3;
		movq		%%mm3,%%mm0;
		punpcklbw	%%mm2,%%mm0;
		pmullw		%%mm0,%%mm0;
		paddusw		%%mm0,%%mm7;
		movq		8(%0,%2),%%mm0;
		punpckhbw	%%mm2,%%mm3;
		pmullw		%%mm3,%%mm3;
		paddusw		%%mm3,%%mm7;

		movq		%%mm5,%%mm2;
		movq		%%mm1,%%mm3;
		pcmpgtb		%%mm1,%%mm2;
		psubb		%%mm5,%%mm1;
		movq		%%mm1,%%mm4;
		punpcklbw	%%mm2,%%mm4;
		pmullw		%%mm4,%%mm4;
		paddusw		%%mm4,%%mm6;
		movq		24(%1),%%mm4;
		punpckhbw	%%mm2,%%mm1;
		pmullw		%%mm1,%%mm1;
		paddusw		%%mm1,%%mm6;

		psrlq		$32,%%mm5;
		movq		%%mm0,%%mm1;
		psllq		$32,%%mm1;
		por		%%mm1,%%mm5;

		movq		%%mm5,%%mm2;
		pcmpgtb		%%mm3,%%mm2;
		psubb		%%mm5,%%mm3;
		movd		16(%0,%2),%%mm5;
		movq		%%mm3,%%mm1;
		punpcklbw	%%mm2,%%mm1;
		pmullw		%%mm1,%%mm1;
		paddusw		%%mm1,%%mm7;
		punpckhbw	%%mm2,%%mm3;
		pmullw		%%mm3,%%mm3;
		paddusw		%%mm3,%%mm7;

		movq		%%mm0,%%mm2;
		movq		%%mm4,%%mm3;
		pcmpgtb		%%mm4,%%mm2;
		psubb		%%mm0,%%mm4;
		movq		%%mm4,%%mm1;
		punpcklbw	%%mm2,%%mm1;
		pmullw		%%mm1,%%mm1;
		paddusw		%%mm1,%%mm6;
		movq		bbmin,%%mm1;
		punpckhbw	%%mm2,%%mm4;
		movq		bbdxy,%%mm2;
		pmullw		%%mm4,%%mm4;

		psrlq		$32,%%mm0;
		paddusw		%%mm4,%%mm6;
		movq		crdxy,%%mm4;
		psllq		$32,%%mm5;
		psubw		c1_15w,%%mm6;
		por		%%mm5,%%mm0;
		paddb		c4,%%mm4;

		movq		%%mm0,%%mm5;
		pcmpgtb		%%mm3,%%mm5;
		psubb		%%mm0,%%mm3;
		movq		%%mm3,%%mm0;
		punpcklbw	%%mm5,%%mm0;
		pmullw		%%mm0,%%mm0;
		paddusw		%%mm0,%%mm7;
		punpckhbw	%%mm5,%%mm3;
		pmullw		%%mm3,%%mm3;
		paddusw		%%mm3,%%mm7;

		movq		%%mm4,%%mm0;


		movq		%%mm4,%%mm5;
		pxor		%%mm3,%%mm3;
		pcmpgtb		%%mm4,%%mm3;
		pxor		%%mm3,%%mm5;
		psubb		%%mm3,%%mm5;

		movq		%%mm5,%%mm3;
		psrlw		$8,%%mm5;
		paddsw		%%mm5,%%mm6;
		pand		c255,%%mm3;
		paddsw		%%mm3,%%mm6;


		movq		%%mm1,%%mm5;
		pcmpgtw		%%mm6,%%mm5;
		movq		%%mm1,%%mm3;
		pxor		%%mm6,%%mm3;
		pand		%%mm5,%%mm3;
		pxor		%%mm3,%%mm6;
		pxor		%%mm3,%%mm1;
		pxor		%%mm4,%%mm2;
		pand		%%mm2,%%mm5;
		pxor		%%mm5,%%mm4;
		pxor		%%mm4,%%mm2;

		movq		%%mm6,%%mm5;
		psllq		$16,%%mm6;
		psrlq		$48,%%mm5;
		por		%%mm5,%%mm6;
		movq		%%mm4,%%mm5;
		psllq		$16,%%mm4;
		psrlq		$48,%%mm5;
		por		%%mm5,%%mm4;

		movq		%%mm1,%%mm5;
		pcmpgtw		%%mm6,%%mm5;
		movq		%%mm1,%%mm3;
		pxor		%%mm6,%%mm3;
		pand		%%mm5,%%mm3;
		pxor		%%mm3,%%mm6;
		pxor		%%mm3,%%mm1;
		pxor		%%mm4,%%mm2;
		pand		%%mm2,%%mm5;
		pxor		%%mm5,%%mm4;
		pxor		%%mm4,%%mm2;

		movq		%%mm6,%%mm5;
		psllq		$16,%%mm6;
		psrlq		$48,%%mm5;
		por		%%mm5,%%mm6;
		movq		%%mm4,%%mm5;
		psllq		$16,%%mm4;
		psrlq		$48,%%mm5;
		por		%%mm5,%%mm4;

		psubw		c1_15w,%%mm7;

		movq		%%mm1,%%mm5;
		pcmpgtw		%%mm6,%%mm5;
		movq		%%mm1,%%mm3;
		pxor		%%mm6,%%mm3;
		pand		%%mm5,%%mm3;
		pxor		%%mm3,%%mm6;
		pxor		%%mm3,%%mm1;
		pxor		%%mm4,%%mm2;
		pand		%%mm2,%%mm5;
		pxor		%%mm5,%%mm4;
		pxor		%%mm4,%%mm2;

		paddb		c4,%%mm0;

		movq		%%mm6,%%mm5;
		psllq		$16,%%mm6;
		psrlq		$48,%%mm5;
		por		%%mm5,%%mm6;
		movq		%%mm4,%%mm5;
		psllq		$16,%%mm4;
		psrlq		$48,%%mm5;
		por		%%mm5,%%mm4;

		    movq		%%mm0,crdxy;

		movq		%%mm1,%%mm5;
		pcmpgtw		%%mm6,%%mm5;
		movq		%%mm1,%%mm3;
		pxor		%%mm6,%%mm3;
		pand		%%mm5,%%mm3;
		pxor		%%mm3,%%mm6;
		pxor		%%mm3,%%mm1;
		pxor		%%mm4,%%mm2;
		pand		%%mm2,%%mm5;
		pxor		%%mm5,%%mm4;
		pxor		%%mm4,%%mm2;


		movq		%%mm0,%%mm5;
		pxor		%%mm3,%%mm3;
		pcmpgtb		%%mm0,%%mm3;
		pxor		%%mm3,%%mm5;
		psubb		%%mm3,%%mm5;

		movq		%%mm5,%%mm3;
		psrlw		$8,%%mm5;
		paddsw		%%mm5,%%mm7;
		pand		c255,%%mm3;
		paddsw		%%mm3,%%mm7;


		movq		%%mm1,%%mm5;
		pcmpgtw		%%mm7,%%mm5;
		movq		%%mm1,%%mm3;
		pxor		%%mm7,%%mm3;
		pand		%%mm5,%%mm3;
		pxor		%%mm3,%%mm7;
		pxor		%%mm3,%%mm1;
		pxor		%%mm0,%%mm2;
		pand		%%mm2,%%mm5;
		pxor		%%mm5,%%mm0;
		pxor		%%mm0,%%mm2;

		movq		%%mm7,%%mm5;
		psllq		$16,%%mm7;
		psrlq		$48,%%mm5;
		por		%%mm5,%%mm7;
		movq		%%mm0,%%mm5;
		psllq		$16,%%mm0;
		psrlq		$48,%%mm5;
		por		%%mm5,%%mm0;

		movq		%%mm1,%%mm5;
		pcmpgtw		%%mm7,%%mm5;
		movq		%%mm1,%%mm3;
		pxor		%%mm7,%%mm3;
		pand		%%mm5,%%mm3;
		pxor		%%mm3,%%mm7;
		pxor		%%mm3,%%mm1;
		pxor		%%mm0,%%mm2;
		pand		%%mm2,%%mm5;
		pxor		%%mm5,%%mm0;
		pxor		%%mm0,%%mm2;

		movq		%%mm7,%%mm5;
		psllq		$16,%%mm7;
		psrlq		$48,%%mm5;
		por		%%mm5,%%mm7;
		movq		%%mm0,%%mm5;
		psllq		$16,%%mm0;
		psrlq		$48,%%mm5;
		por		%%mm5,%%mm0;

		movq		%%mm1,%%mm5;
		pcmpgtw		%%mm7,%%mm5;
		movq		%%mm1,%%mm3;
		pxor		%%mm7,%%mm3;
		pand		%%mm5,%%mm3;
		pxor		%%mm3,%%mm7;
		pxor		%%mm3,%%mm1;
		pxor		%%mm0,%%mm2;
		pand		%%mm2,%%mm5;
		pxor		%%mm5,%%mm0;
		pxor		%%mm0,%%mm2;

		movq		%%mm7,%%mm5;
		psllq		$16,%%mm7;
		psrlq		$48,%%mm5;
		por		%%mm5,%%mm7;
		movq		%%mm0,%%mm5;
		psllq		$16,%%mm0;
		psrlq		$48,%%mm5;
		por		%%mm5,%%mm0;

		movq		%%mm1,%%mm5;
		pcmpgtw		%%mm7,%%mm5;
		movq		%%mm1,%%mm3;
		pxor		%%mm7,%%mm3;
		pand		%%mm5,%%mm3;
		pxor		%%mm3,%%mm7;
		pxor		%%mm3,%%mm1;
		pxor		%%mm0,%%mm2;
		pand		%%mm2,%%mm5;
		pxor		%%mm5,%%mm0;
		pxor		%%mm0,%%mm2;

		movq		%%mm1,bbmin;
		movq		%%mm2,bbdxy;

	" :: "r" (p), "r" (t), "r" (pitch * 8 /* ch */) : "memory");
}

static inline void
mmx_load_pref(char t[16][16])
{
	asm volatile ("
		movq		0*16+0(%0),%%mm0;
		paddw		1*16+0(%0),%%mm0;
		movq		0*16+8(%0),%%mm1;
		paddw		1*16+8(%0),%%mm1;
		movq		c1b,%%mm7;
		paddw		2*16+0(%0),%%mm0;
		movq		%%mm7,%%mm6;
		paddw		2*16+8(%0),%%mm1;
		psllq		$7,%%mm7;
		paddw		3*16+0(%0),%%mm0;
		psrlw		$8-4,%%mm6;
		paddw		3*16+8(%0),%%mm1;
		movq		256+0*16+0(%0),%%mm2;
		paddw		4*16+0(%0),%%mm0;
		movq		256+0*16+8(%0),%%mm3;
		paddw		4*16+8(%0),%%mm1;
		paddw		5*16+0(%0),%%mm0;
		paddw		5*16+8(%0),%%mm1;
		paddw		6*16+0(%0),%%mm0;
		paddw		6*16+8(%0),%%mm1;
		paddw		7*16+0(%0),%%mm0;
		movq		%%mm0,%%mm4;
		paddw		7*16+8(%0),%%mm1;
		punpcklwd	%%mm1,%%mm0;
		paddw		256+1*16+0(%0),%%mm2;
		punpckhwd	%%mm1,%%mm4;
		paddw		256+1*16+8(%0),%%mm3;
		paddw		%%mm4,%%mm0;
		paddw		256+2*16+0(%0),%%mm2;
		paddw		256+2*16+8(%0),%%mm3;
		paddw		256+3*16+0(%0),%%mm2;
		paddw		256+3*16+8(%0),%%mm3;
		paddw		256+4*16+0(%0),%%mm2;
		paddw		256+4*16+8(%0),%%mm3;
		paddw		256+5*16+0(%0),%%mm2;
		paddw		256+5*16+8(%0),%%mm3;
		paddw		256+6*16+0(%0),%%mm2;
		paddw		256+6*16+8(%0),%%mm3;
		paddw		256+7*16+0(%0),%%mm2;
		paddw		256+7*16+8(%0),%%mm3;
		movq		128+0*16+0(%0),%%mm1;
		movq		%%mm2,%%mm5;
		movq		128+0*16+8(%0),%%mm4;
		punpcklwd	%%mm3,%%mm2;
		paddw		128+1*16+0(%0),%%mm1;
		punpckhwd	%%mm3,%%mm5;
		paddw		128+1*16+8(%0),%%mm4;
		paddw		%%mm5,%%mm2;
		paddw		128+2*16+0(%0),%%mm1;
		movq		%%mm0,%%mm5;
		paddw		128+2*16+8(%0),%%mm4;
		punpckldq	%%mm2,%%mm0;
		paddw		128+3*16+0(%0),%%mm1;
		punpckhdq	%%mm2,%%mm5;
		paddw		128+3*16+8(%0),%%mm4;
		paddw		%%mm5,%%mm0;
		paddw		128+4*16+0(%0),%%mm1;
		paddw		%%mm6,%%mm0;
		paddw		128+4*16+8(%0),%%mm4;
		psrlw		$5,%%mm0;
		paddw		128+5*16+0(%0),%%mm1;
		movq		%%mm0,%%mm2;
		paddw		128+5*16+8(%0),%%mm4;
		psllw		$8,%%mm2;
		paddw		128+6*16+0(%0),%%mm1;
		por		%%mm2,%%mm0;
		paddw		128+6*16+8(%0),%%mm4;
		pxor		%%mm7,%%mm0;
		paddw		128+7*16+0(%0),%%mm1;
		movq		%%mm0,%%mm2;
		paddw		128+7*16+8(%0),%%mm4;
		punpcklbw	%%mm0,%%mm0;
		movq		384+0*16+0(%0),%%mm3;
		punpckhbw	%%mm2,%%mm2;
		movq		384+0*16+8(%0),%%mm5;
		movq		%%mm0,0(%1);
		paddw		384+1*16+0(%0),%%mm3;
		movq		%%mm2,8(%1);
		paddw		384+1*16+8(%0),%%mm5;
		movq		%%mm1,%%mm0;
		paddw		384+2*16+0(%0),%%mm3;
		punpcklwd	%%mm4,%%mm1;
		paddw		384+2*16+8(%0),%%mm5;
		punpckhwd	%%mm4,%%mm0;
		paddw		384+3*16+0(%0),%%mm3;
		paddw		%%mm0,%%mm1;
		paddw		384+3*16+8(%0),%%mm5;
		movq		%%mm1,%%mm2;
		paddw		384+4*16+0(%0),%%mm3;
		paddw		384+4*16+8(%0),%%mm5;
		paddw		384+5*16+0(%0),%%mm3;
		paddw		384+5*16+8(%0),%%mm5;
		paddw		384+6*16+0(%0),%%mm3;
		paddw		384+6*16+8(%0),%%mm5;
		paddw		384+7*16+0(%0),%%mm3;
		paddw		384+7*16+8(%0),%%mm5;
		movq		%%mm3,%%mm0;
		punpcklwd	%%mm5,%%mm3;
		punpckhwd	%%mm5,%%mm0;
		paddw		%%mm0,%%mm3;
		punpckldq	%%mm3,%%mm1;
		punpckhdq	%%mm3,%%mm2;
		paddw		%%mm2,%%mm1;
		paddw		%%mm6,%%mm1;
		psrlw		$5,%%mm1;
		movq		%%mm1,%%mm2;
		psllw		$8,%%mm2;
		por		%%mm2,%%mm1;
		pxor		%%mm7,%%mm1;
		movq		%%mm1,%%mm2;
		punpcklbw	%%mm1,%%mm1;
		punpckhbw	%%mm2,%%mm2;
		movq		%%mm1,16(%1);
		movq		%%mm2,24(%1);

	" :: "S" (&mblock[0][0][0][0]), "D" (&t[0][0]) : "memory");
}

static inline int
mmx_predict(unsigned char *from, int d2x, int d2y,
      typeof (temp22) *ibuf, int iright, int idown, short dest[6][8][8])
{
	unsigned char *p, *q;
	int mx, my, hy;
	int i, s;

	pr_start(53, "forward fetch");

	asm volatile ("
		pxor		%mm5,%mm5;
		pxor		%mm6,%mm6;
		pxor		%mm7,%mm7;
	");

	if (iright) {
	for (i = 0; i < 16; i++) {
		asm volatile ("
			movq		1(%0),%%mm0;
			movq		%%mm0,%%mm1;
			punpcklbw	%%mm5,%%mm0;
			punpckhbw	%%mm5,%%mm1;
			movq		%%mm0,2*768+0*128+0(%3);
			movq		%%mm1,2*768+0*128+8(%3);
			movq		0*768+0*128+0(%1),%%mm2;
			movq		0*768+0*128+8(%1),%%mm3;
			psubw		%%mm0,%%mm2;
			psubw		%%mm1,%%mm3;
			movq		%%mm2,0*768+0*128+0(%3);
			movq		%%mm3,0*768+0*128+8(%3);
			paddw		%%mm2,%%mm6;
			paddw		%%mm3,%%mm6;
			pmaddwd		%%mm2,%%mm2;
			pmaddwd		%%mm3,%%mm3;
			paddd		%%mm2,%%mm7;
			paddd		%%mm3,%%mm7;

			movq		8(%0),%%mm0;
			psrlq		$8,%%mm0;
			movq		(%2),%%mm2;
			psllq		$56,%%mm2;
			por		%%mm2,%%mm0;
			movq		%%mm0,%%mm1;
			punpcklbw	%%mm5,%%mm0;
			punpckhbw	%%mm5,%%mm1;
			movq		%%mm0,2*768+2*128+0(%3);
			movq		%%mm1,2*768+2*128+8(%3);
			movq		0*768+2*128+0(%1),%%mm2;
			movq		0*768+2*128+8(%1),%%mm3;
			psubw		%%mm0,%%mm2;
			psubw		%%mm1,%%mm3;
			movq		%%mm2,0*768+2*128+0(%3);
			movq		%%mm3,0*768+2*128+8(%3);
			paddw		%%mm2,%%mm6;
			paddw		%%mm3,%%mm6;
			pmaddwd		%%mm2,%%mm2;
			pmaddwd		%%mm3,%%mm3;
			paddd		%%mm2,%%mm7;
			paddd		%%mm3,%%mm7;

		" :: "r" (&(* ibuf)[i+idown][0]),
		     "r" (&mblock[0][0][i][0]),
		     "r" (&(* ibuf)[18][i+idown]),
		     "r" (&dest[0][i][0]));
	}
	} else {
	for (i = 0; i < 16; i++) {
		asm volatile ("
			movq		(%0),%%mm0;
			movq		%%mm0,%%mm1;
			punpcklbw	%%mm5,%%mm0;
			punpckhbw	%%mm5,%%mm1;
			movq		%%mm0,2*768+0*128+0(%2);
			movq		%%mm1,2*768+0*128+8(%2);
			movq		0*768+0*128+0(%1),%%mm2;
			movq		0*768+0*128+8(%1),%%mm3;
			psubw		%%mm0,%%mm2;
			psubw		%%mm1,%%mm3;
			movq		%%mm2,0*768+0*128+0(%2);
			movq		%%mm3,0*768+0*128+8(%2);
			paddw		%%mm2,%%mm6;
			paddw		%%mm3,%%mm6;
			pmaddwd		%%mm2,%%mm2;
			pmaddwd		%%mm3,%%mm3;
			paddd		%%mm2,%%mm7;
			paddd		%%mm3,%%mm7;

			movq		8(%0),%%mm0;
			movq		%%mm0,%%mm1;
			punpcklbw	%%mm5,%%mm0;
			punpckhbw	%%mm5,%%mm1;
			movq		%%mm0,2*768+2*128+0(%2);
			movq		%%mm1,2*768+2*128+8(%2);
			movq		0*768+2*128+0(%1),%%mm2;
			movq		0*768+2*128+8(%1),%%mm3;
			psubw		%%mm0,%%mm2;
			psubw		%%mm1,%%mm3;
			movq		%%mm2,0*768+2*128+0(%2);
			movq		%%mm3,0*768+2*128+8(%2);
			paddw		%%mm2,%%mm6;
			paddw		%%mm3,%%mm6;
			pmaddwd		%%mm2,%%mm2;
			pmaddwd		%%mm3,%%mm3;
			paddd		%%mm2,%%mm7;
			paddd		%%mm3,%%mm7;

		" :: "r" (&(* ibuf)[i+idown][0]),
		     "r" (&mblock[0][0][i][0]),
		     "r" (&dest[0][i][0]));
	}
	}

	asm volatile ("
		pmaddwd		c1,%%mm6;
		movq		%%mm7,%%mm0;
		psrlq		$32,%%mm7;
		paddd		%%mm0,%%mm7;
		movq		%%mm6,%%mm0;
		psrlq		$32,%%mm6;
		paddd		%%mm0,%%mm6;
		pslld		$8,%%mm7;
		movd		%%mm6,%1;
		imul		%1,%1;
		movd		%%mm7,%0;
		subl		%1,%0;

	" : "=&r" (s) : "r" (0));

	mx = d2x + mb_col * 16 * 2;
	my = d2y + mb_row * 16 * 2;

	p = from + mb_address.chrom_0
		+ 8 * mb_address.block[0].pitch + 8
		+ (mx >> 2) + (my >> 2) * mb_address.block[4].pitch;
	hy = ((my >> 1) & 1) * mb_address.block[4].pitch;

	if (mx & 2) {
		q = p + mb_address.block[5].offset;
	asm volatile ("
		movq		c2,%mm7;
	");
		for (i = 0; i < 8; i++) {
			asm volatile ("
				pushl		%1
				movzxb		(%0),%1;
				movd		%1,%%mm1;
				popl		%1
				movq		1(%0),%%mm2;
				movq		%%mm2,%%mm0;
				psllq		$8,%%mm0;
				por		%%mm1,%%mm0;
				movq		%%mm0,%%mm1;
				punpcklbw	%%mm5,%%mm0;
				punpckhbw	%%mm5,%%mm1;
				movq		%%mm2,%%mm3;
				punpcklbw	%%mm5,%%mm2;
				punpckhbw	%%mm5,%%mm3;
				paddw		%%mm2,%%mm0;
				paddw		%%mm3,%%mm1;
				pushl		%1
				movzxb		(%0,%2),%1;
				movd		%1,%%mm3;
				popl		%1
				movq		1(%0,%2),%%mm2;
				movq		%%mm2,%%mm4;
				psllq		$8,%%mm4;
				por		%%mm3,%%mm4;
				movq		%%mm4,%%mm3;
				punpcklbw	%%mm5,%%mm4;
				punpckhbw	%%mm5,%%mm3;
				paddw		%%mm4,%%mm0;
				paddw		%%mm3,%%mm1;
				movq		%%mm2,%%mm3;
				punpcklbw	%%mm5,%%mm2;
				punpckhbw	%%mm5,%%mm3;
				paddw		%%mm2,%%mm0;
				paddw		%%mm3,%%mm1;
				paddw		%%mm7,%%mm0;
				paddw		%%mm7,%%mm1;
				psrlw		$2,%%mm0;
				psrlw		$2,%%mm1;
				movq		0*768+4*128+0(%4),%%mm2;
				movq		0*768+4*128+8(%4),%%mm3;
				psubw		%%mm0,%%mm2;
				psubw		%%mm1,%%mm3;
				movq		%%mm0,2*768+4*128+0(%1);
				movq		%%mm1,2*768+4*128+8(%1);
				movq		%%mm2,0*768+4*128+0(%1);
				movq		%%mm3,0*768+4*128+8(%1);

				pushl		%1
				movzxb		(%3),%1;
				movd		%1,%%mm1;
				popl		%1
				movq		1(%3),%%mm2;
				movq		%%mm2,%%mm0;
				psllq		$8,%%mm0;
				por		%%mm1,%%mm0;
				movq		%%mm0,%%mm1;
				punpcklbw	%%mm5,%%mm0;
				punpckhbw	%%mm5,%%mm1;
				movq		%%mm2,%%mm3;
				punpcklbw	%%mm5,%%mm2;
				punpckhbw	%%mm5,%%mm3;
				paddw		%%mm2,%%mm0;
				paddw		%%mm3,%%mm1;
				pushl		%1
				movzxb		(%3,%2),%1;
				movd		%1,%%mm3;
				popl		%1
				movq		1(%3,%2),%%mm2;
				movq		%%mm2,%%mm4;
				psllq		$8,%%mm4;
				por		%%mm3,%%mm4;
				movq		%%mm4,%%mm3;
				punpcklbw	%%mm5,%%mm4;
				punpckhbw	%%mm5,%%mm3;
				paddw		%%mm4,%%mm0;
				paddw		%%mm3,%%mm1;
				movq		%%mm2,%%mm3;
				punpcklbw	%%mm5,%%mm2;
				punpckhbw	%%mm5,%%mm3;
				paddw		%%mm2,%%mm0;
				paddw		%%mm3,%%mm1;
				paddw		%%mm7,%%mm0;
				paddw		%%mm7,%%mm1;
				psrlw		$2,%%mm0;
				psrlw		$2,%%mm1;

				movq		0*768+5*128+0(%4),%%mm2;
				movq		0*768+5*128+8(%4),%%mm3;
				psubw		%%mm0,%%mm2;
				psubw		%%mm1,%%mm3;
				movq		%%mm0,2*768+5*128+0(%1);
				movq		%%mm1,2*768+5*128+8(%1);
				movq		%%mm2,0*768+5*128+0(%1);
				movq		%%mm3,0*768+5*128+8(%1);

			" :: "r" (p), "c" (&dest[0][i][0]), "r" (hy), "r" (q),
			    "r" (&mblock[0][0][i][0]));

		p += mb_address.block[4].pitch;
		q += mb_address.block[4].pitch;
	}
	} else {
	asm volatile ("
		movq		(%0),%%mm3;
		movq		(%0,%1),%%mm4;
		movq		c1b,%%mm7;
		pxor		%%mm6,%%mm6;

	" :: "r" (p), "r" (mb_address.block[5].offset), "r" (0));

	p += hy;

	if (hy) // XXX
		asm volatile ("
			pcmpeqb		%mm6,%mm6;
		");

	for (i = 0; i < 8; i++) {
		asm volatile ("
			movq		(%0),%%mm2;
			movq		%%mm3,%%mm1;
			pxor		%%mm2,%%mm1;	
			movq		%%mm2,%%mm0;
			pand		%%mm6,%%mm1;
			pxor		%%mm1,%%mm0;
			pxor		%%mm1,%%mm3;
			movq		%%mm2,%%mm1;
			por		%%mm0,%%mm2;
			por		%%mm7,%%mm0;
			por		%%mm7,%%mm1;
			psrlq		$1,%%mm0;
			pand		%%mm7,%%mm2;
			psrlq		$1,%%mm1;
			paddb		%%mm1,%%mm0;
			paddb		%%mm2,%%mm0;
			movq		0*768+4*128+0(%4),%%mm2;
			movq		%%mm0,%%mm1;
			punpcklbw	%%mm5,%%mm0;
			movq		%%mm0,2*768+4*128+0(%2);
			psubw		%%mm0,%%mm2;
			movq		%%mm2,0*768+4*128+0(%2);
			movq		(%0,%1),%%mm0;
			movq		0*768+4*128+8(%4),%%mm2;
			punpckhbw	%%mm5,%%mm1;
			movq		%%mm1,2*768+4*128+8(%2);
			psubw		%%mm1,%%mm2;
			movq		%%mm2,0*768+4*128+8(%2);
			movq		%%mm4,%%mm1;
			pxor		%%mm0,%%mm1;	
			movq		%%mm0,%%mm2;
			pand		%%mm6,%%mm1;
			pxor		%%mm1,%%mm2;
			pxor		%%mm1,%%mm4;
			movq		%%mm0,%%mm1;
			por		%%mm2,%%mm0;
			por		%%mm7,%%mm2;
			por		%%mm7,%%mm1;
			psrlq		$1,%%mm2;
			pand		%%mm7,%%mm0;
			psrlq		$1,%%mm1;
			paddb		%%mm1,%%mm0;
			paddb		%%mm2,%%mm0;
			movq		0*768+5*128+0(%4),%%mm2;
			movq		%%mm0,%%mm1;
			punpcklbw	%%mm5,%%mm0;
			movq		%%mm0,2*768+5*128+0(%2);
			psubw		%%mm0,%%mm2;
			movq		%%mm2,0*768+5*128+0(%2);
			movq		0*768+5*128+8(%4),%%mm2;
			punpckhbw	%%mm5,%%mm1;
			movq		%%mm1,2*768+5*128+8(%2);
			psubw		%%mm1,%%mm2;
			movq		%%mm2,0*768+5*128+8(%2);

		" :: "r" (p), "r" (mb_address.block[5].offset),
		     "r" (&dest[0][i][0]), "r" (0),
		     "r" (&mblock[0][0][i][0]));

		p += mb_address.block[4].pitch;
	}
	}

	pr_end(53);

	return s;
}

static int
search(int *dhx, int *dhy, unsigned char *from,
       int x, int y, int range, short dest[6][8][8])
{
	typeof (temp22) *pat1, *pat2, *pat3, *pat4;
	int act, act2, min, mini[3][3];
	int i, j, k, dx, dy, ii, jj;
	int hrange, vrange;
	int x0, y0, x1, y1;
	unsigned char *p;
	typeof (temp22) *ibuf;
	int iright, idown;

	hrange = (range + 7) & -8;
	vrange = (range + 3) & -4;

	x0 = x - (hrange >> 1);	y0 = y - (vrange >> 1);
	x1 = x + (hrange >> 1);	y1 = y + (vrange >> 1);

	hrange = (range > 8) ? ((range + 15) & -16) >> 1 : 4; 
	vrange >>= 2;

	if (x0 < 0) {
		x0 = 0;
		x1 = hrange;
	} else if (x1 > mb_last_col * 16) {
		x1 = mb_last_col * 16;
		x0 = x1 - hrange;
	}
				if (y0 < 0) {
					y0 = 0;
					y1 = vrange;
				} else if (y1 > mb_last_row * 16) {
					y1 = mb_last_row * 16;
					y0 = y1 - vrange;
				}

	if (!((x1 - x0) == 4 || ((x1 - x0) & 7) == 0)) {
		fprintf(stderr, "x0=%d x1=%d hrange=%d range=%d\n",
			x0, x1, hrange, range);
	}
	assert((x1 - x0) == 4 || ((x1 - x0) & 7) == 0);

	bbmin = MMXRW(0xFFFE - 0x8000);
	bbdxy = MMXRW(0x0000);

	mmx_load_pref(tbuf);

	p = from + mm_buf_offs + y0 * mb_address.block[0].pitch;

	for (k = 0; k < 4; k++) {
		crdy0.b[k * 2 + 0] = x0 - x + k - 4;
		crdy0.b[k * 2 + 1] = y0 - y - 1;
	}

	if ((x1 - x0) > 4)
		for (j = y0; j < y1; p += mb_address.block[0].pitch, j++) {
			asm volatile ("
				movq		crdy0,%mm0;
				paddb		c256,%mm0;
				movq		%mm0,crdy0;
				movq		%mm0,crdxy;
			");

			for (i = x0; i < x1; i += 8)
				mmx_psse_8(tbuf, p + i, mb_address.block[0].pitch);
		}
	else
		for (j = y0; j < y1; p += mb_address.block[0].pitch, j++) {
			asm volatile ("
				movq		crdy0,%mm0;
				paddb		c256,%mm0;
				movq		%mm0,crdy0;
				movq		%mm0,crdxy;
			");

			mmx_psse_4(tbuf, p, mb_address.block[0].pitch);
		}

	p = from + x + y * mb_address.block[0].pitch;

	mmx_load_ref(tbuf);

	min = mmx_sad(tbuf, p, mb_address.block[0].pitch);
	min -= (min >> 3);
	dx = 0;			dy = 0;

	for (i = 0; i < 4; i++) {
		act = mmx_sad(tbuf,
			p + bbdxy.b[i * 2 + 0] /* x */ 
			+ bbdxy.b[i * 2 + 1] * mb_address.block[0].pitch,
			mb_address.block[0].pitch);

		if (act < min) {
			min = act;
			dx = bbdxy.b[i * 2 + 0];
			dy = bbdxy.b[i * 2 + 1];
		}
	}

	*dhx = dx * 2;		*dhy = dy * 2;

#if TEST11
	if (min < (16 * 256) && !(dx | dy)) {

	x *= 2;			y *= 2;
	dx *= 2;		dy *= 2;
	ii = dx;		jj = dy;

	dx -= ((x + dx) >= (mb_last_col * 16) * 2);
	dy -= ((y + dy) >= (mb_last_row * 16) * 2);

	ii -= dx;		jj -= dy; // default halfs from fine sad >> 3

	/* XXX inefficient */
	mmx_load_interp(from, mb_address.block[0].pitch,
		(x + dx - 1) >> 1, (y + dy - 1) >> 1);

	pat1 = &temp11;
	pat2 = &temp2v;
	pat3 = &temp2h;
	pat4 = &temp22;

	iright = ((dx ^ 1) & 1);
	idown = ((dy ^ 1) & 1);

	if (dx & 1) {
		swap(pat1, pat3);
		swap(pat2, pat4);
	}

	if (dy & 1) {
		swap(pat1, pat2);
		swap(pat3, pat4);
	}
		goto bail_out;
	}
#endif

	/* half sample refinement */

	x *= 2;			y *= 2;
	dx *= 2;		dy *= 2;

	ii = dx;		jj = dy;

	/*
	 *  Full range is eg. -8*2 ... +7*2, MV limit -16 ... +16;
	 *  this becomes -16,-15,-14 FHF and +13,+14,+15 HFH, +16 never used
	 *  Second, at the image boundaries we shift 1/2 sample inwards,
	 *  eg. 0*2 -> 0,1,2 FHF. Used to skip refinement, not good.
	 *  Boundary deltas are often +0,+0; Otherwise FHF occurs rarely.
	 */
	dx -= ((x + dx) >= (mb_last_col * 16) * 2);
	dy -= ((y + dy) >= (mb_last_row * 16) * 2);
	dx += ((x + dx) <= x0 * 2);
	dy += ((y + dy) <= y0 * 2);

	ii -= dx;		jj -= dy; // default halfs from fine sad >> 3

	mini[1][1] = min;

	mmx_load_interp(from, mb_address.block[0].pitch,
		(x + dx - 1) >> 1, (y + dy - 1) >> 1);

	pat1 = &temp11;
	pat2 = &temp2v;
	pat3 = &temp2h;
	pat4 = &temp22;

	iright = ((dx ^ 1) & 1);
	idown = ((dy ^ 1) & 1);

	if (dx & 1) {
		swap(pat1, pat3);
		swap(pat2, pat4);
	}

	if (dy & 1) {
		swap(pat1, pat2);
		swap(pat3, pat4);
	}

	/*
	 *  0,0 redundand WRT min if ((dx & 1) == 0 && (dy & 1) == 0),
	 *  the usual case. We use sad2h, avoiding another sad routine.
	 *  XXX the second result should be used.
	 */
	if ((dx | dy) & 1) {
		// act = sad1(tbuf, *pat1, iright, idown); mini[1][1] = act;
		act = mmx_sad2h(tbuf, *pat1, idown, &act2);
		mini[1][1] = iright ? act2 : act;
	}

	act = mmx_sad2h(tbuf, *pat3, idown, &act2); mini[1][0] = act; mini[1][2] = act2;
	act = mmx_sad2h(tbuf, *pat4, 0, &act2); mini[0][0] = act; mini[0][2] = act2;
	act = mmx_sad2h(tbuf, *pat4, 1, &act2); mini[2][0] = act; mini[2][2] = act2;
	act = mmx_sad2v(tbuf, *pat2, iright, &act2); mini[0][1] = act; mini[2][1] = act2;

	/* XXX optimize */
	for (j = -1; j <= +1; j++) {
		for (i = -1; i <= +1; i++) {
			act = mini[j+1][i+1];

			/* XXX inaccurate */
			if (((dx + i) & (dy + j)) & 1)
				act += (act * 3) >> 4;
			else if (((dx + i) | (dy + j)) & 1)
				act += (act * 4) >> 4;

			if (act < min) {
				min = act;
				*dhx = dx + i;
				*dhy = dy + j;
				ii = i;
				jj = j;
			}
		}
	}

bail_out:
	if (ii == 0) {
		ibuf = pat1;
		if (jj != 0) {
			ibuf = pat2;
			idown = (((unsigned int) jj) >> 31) ^ 1;
		}
	} else {
		ibuf = pat3;
		iright = (((unsigned int) ii) >> 31) ^ 1;
		if (jj != 0) {
			ibuf = pat4;
			idown = (((unsigned int) jj) >> 31) ^ 1;
		}
	}

	return mmx_predict(from, *dhx, *dhy, ibuf, iright, idown, dest);
}

static int
t4_edu(unsigned char *ref, int *dxp, int *dyp, int sx, int sy,
	int src_range, int max_range, short dest[6][8][8])
{
	struct motion M;
	int x, y, xs, ys;
	int x0, y0, x1, y1;
	int hrange, vrange;
	int s;

	pr_start(62, "t4_edu");

	x = mb_col * 16;
	y = mb_row * 16;
	xs = x + sx;
	ys = y + sy;

	hrange = 8 >> 1;
	vrange = src_range >> 1;

	assert(hrange <= max_range && (max_range & 7) == 0);

	x0 = x - (max_range >> 1); if (x0 < 0) x0 = 0;
	y0 = y - (max_range >> 1); if (y0 < 0) y0 = 0;
	x1 = x + (max_range >> 1); if (x1 > mb_last_col * 16) x1 = mb_last_col * 16;
	y1 = y + (max_range >> 1); if (y1 > mb_last_row * 16) y1 = mb_last_row * 16;

	if (xs - hrange < x0) {
		xs = x0 + hrange;
	} else if (xs + hrange > x1) {
		xs = x1 - hrange;
	}

	if (ys - vrange < y0) {
		ys = y0 + vrange;
	} else if (ys + vrange > y1) {
		ys = y1 - vrange;
	}

	s = search(dxp, dyp, ref, xs, ys, src_range, dest);

	*dxp += (xs - x) * 2;
	*dyp += (ys - y) * 2;

	pr_end(62);

	return s;
}

static double qmsum = 0.0;
static int qmcount = 0;

/*
 * XXX modulate by cap_fifo load, ie. reduce
 * search efforts before frames drop.
 */

void
t7(int range, int dist)
{
	double m, q;

	if (qmcount == 0)
		return;

	m = qmsum / qmcount;
	q = 1.5 * m / dist;

	if (range != 0)
		assert(range > 3 && dist > 0);

	if (0)
		fprintf(stderr, "mavg %6.4f pred %6.4f\n", m, q);

	qmsum = 0.0;
	qmcount = 0;

	if (AUTOR)
		motion = q * 256;
}

// XXX experimental, don't care
static int pdx[18][22];
static int pdy[18][22];
static int pdist;

void
zero_forward_motion(void)
{
	pdx[mb_row][mb_col] = 127;
}

int
predict_forward_motion(struct motion *M, unsigned char *from, int dist)
{
	int i, s;
	int *pmx; int *pmy;

	pmx = &M[0].MV[0];
	pmy = &M[0].MV[1];

	s = search(pmx, pmy, from,
		mb_col * 16, mb_row * 16,
		M[0].src_range, mblock[1]); // 1 + 3

	emms();

	pdx[mb_row][mb_col] = *pmx;
	pdy[mb_row][mb_col] = *pmy;
	pdist = dist;

	{
		double qq = sqrt((*pmx) * (*pmx) + (*pmy) * (*pmy)); 

		if (qq > 1) {
			qmsum += qq;
			qmcount++;
		}
	}

	if (!T3RT)
		for (i = 0; i < 6*64; i++)
			mblock[1][0][0][i] = 0;

	return s;
}

int
predict_bidirectional_motion(struct motion *M,
	int *vmc1, int *vmc2, int bdist /* forward */)
{
	int i, j, si, sf, sb;
	int *pmx1; int *pmy1;
	int *pmx2; int *pmy2;
	int fdist = pdist - bdist;

	pmx1 = &M[0].MV[0];
	pmy1 = &M[0].MV[1];
	pmx2 = &M[1].MV[0];
	pmy2 = &M[1].MV[1];

	if (0 && pdx[mb_row][mb_col] < 127) {
		sf = t4_edu(oldref, pmx1, pmy1,
			+pdx[mb_row][mb_col] * bdist / pdist,
			+pdy[mb_row][mb_col] * bdist / pdist,
			MIN(M[0].src_range, 8), M[0].max_range,
			mblock[1]); // 1 + 3
		sb = t4_edu(newref, pmx2, pmy2,
			-pdx[mb_row][mb_col] * fdist / pdist,
			-pdy[mb_row][mb_col] * fdist / pdist,
			MIN(M[1].src_range, 8), M[1].max_range,
			mblock[2]); // 2 + 4
	} else {
		sf = search(pmx1, pmy1, oldref,
			mb_col * 16, mb_row * 16,
			M[0].src_range, mblock[1]); // 1 + 3
		sb = search(pmx2, pmy2, newref,
			mb_col * 16, mb_row * 16,
			M[1].src_range, mblock[2]); // 2 + 4

		if (0)
		fprintf(stderr, "%2d,%2d: Pd%d P%+3d,%+3d, Bd%d Bberr%+3d,%+3d<>%+3d,%+3d, Bferr%+3d,%+3d<>%+3d,%+3d\n",
			mb_col, mb_row,
			pdist,
			pdx[mb_row][mb_col], pdy[mb_row][mb_col],
			bdist,
			+pdx[mb_row][mb_col] * bdist / pdist - *pmx1, +pdy[mb_row][mb_col] * bdist / pdist - *pmy1, *pmx1, *pmy1,
			-pdx[mb_row][mb_col] * fdist / pdist - *pmx2, -pdy[mb_row][mb_col] * fdist / pdist - *pmy2, *pmx2, *pmy2
		);
	}

	asm volatile ("
		movq		c1,%mm5;
		pxor		%mm6,%mm6;	
		pxor		%mm7,%mm7;
	");	

	for (j = i = 0; j < 16 * 16; j += 16, i += 4) {
		asm volatile ("
			movq		3*768+0*128+0(%0),%%mm0;
			paddw		4*768+0*128+0(%0),%%mm0;
			paddw		%%mm5,%%mm0;
			psrlw		$1,%%mm0;
			movq		0*768+0*128+0(%0),%%mm2;
			psubw		%%mm0,%%mm2;
			movq		%%mm2,3*768+0*128+0(%0);
			paddw		%%mm2,%%mm6;
			pmaddwd		%%mm2,%%mm2;
			paddd		%%mm2,%%mm7;

			movq		3*768+0*128+8(%0),%%mm0;
			paddw		4*768+0*128+8(%0),%%mm0;
			paddw		%%mm5,%%mm0;
			psrlw		$1,%%mm0;
			movq		0*768+0*128+8(%0),%%mm2;
			psubw		%%mm0,%%mm2;
			movq		%%mm2,3*768+0*128+8(%0);
			paddw		%%mm2,%%mm6;
			pmaddwd		%%mm2,%%mm2;
			paddd		%%mm2,%%mm7;

			movq		3*768+0*128+16(%0),%%mm0;
			paddw		4*768+0*128+16(%0),%%mm0;
			paddw		%%mm5,%%mm0;
			psrlw		$1,%%mm0;
			movq		0*768+0*128+16(%0),%%mm2;
			psubw		%%mm0,%%mm2;
			movq		%%mm2,3*768+0*128+16(%0);
			paddw		%%mm2,%%mm6;
			pmaddwd		%%mm2,%%mm2;
			paddd		%%mm2,%%mm7;

			movq		3*768+0*128+24(%0),%%mm0;
			paddw		4*768+0*128+24(%0),%%mm0;
			paddw		%%mm5,%%mm0;
			psrlw		$1,%%mm0;
			movq		0*768+0*128+24(%0),%%mm2;
			psubw		%%mm0,%%mm2;
			movq		%%mm2,3*768+0*128+24(%0);
			paddw		%%mm2,%%mm6;
			pmaddwd		%%mm2,%%mm2;
			paddd		%%mm2,%%mm7;

			movq		3*768+4*128+0(%1),%%mm0;
			paddw		4*768+4*128+0(%1),%%mm0;
			paddw		%%mm5,%%mm0;
			psrlw		$1,%%mm0;
			movq		0*768+4*128+0(%1),%%mm2;
			psubw		%%mm0,%%mm2;
			movq		%%mm2,3*768+4*128+0(%1);

			movq		3*768+5*128+0(%1),%%mm0;
			paddw		4*768+5*128+0(%1),%%mm0;
			paddw		%%mm5,%%mm0;
			psrlw		$1,%%mm0;
			movq		0*768+5*128+0(%1),%%mm2;
			psubw		%%mm0,%%mm2;
			movq		%%mm2,3*768+5*128+0(%1);

		" :: "r" (&mblock[0][0][0][j]), "r" (&mblock[0][0][0][i]));
	}

	asm volatile ("
		pmaddwd		%%mm5,%%mm6;
		movq		%%mm7,%%mm0;
		psrlq		$32,%%mm7;
		paddd		%%mm0,%%mm7;
		movq		%%mm6,%%mm0;
		psrlq		$32,%%mm6;
		paddd		%%mm0,%%mm6;
		pslld		$8,%%mm7;
		movd		%%mm6,%1;
		imul		%1,%1;
		movd		%%mm7,%0;
		subl		%1,%0;

	" : "=&r" (si) : "r" (0));

	*vmc1 = sf;
	*vmc2 = sb;

	if (!T3RT)
		for (i = 0; i < 6*64; i++)
			mblock[1][0][0][i] = 
			mblock[2][0][0][i] = 
			mblock[3][0][0][i] = 0;

	return si;
}

/*
 *  Zero motion reference
 */

/*
 *  Forward prediction (P frames only)
 *
 *  mblock[1] = org - old_ref;
 *  mblock[3] = old_ref; 	// for reconstruction by idct_inter
 */
int
predict_forward_packed(unsigned char *from)
{
	int i, n, s = 0, s2 = 0;

	for (i = 0; i < 4 * 64; i++) {
		mblock[1][0][0][i] = n = mblock[0][0][0][i] - from[i];
		mblock[3][0][0][i] = from[i];
		s += n;
		s2 += n * n;
	}

	for (; i < 6 * 64; i++) {
		mblock[1][0][0][i] = mblock[0][0][0][i] - from[i];
		mblock[3][0][0][i] = from[i];
	}

	return s2 * 256 - (s * s);
}

int
predict_forward_planar(unsigned char *from)
{
	int i, j, n, s = 0, s2 = 0;
	unsigned char *p;

	p = from;

	for (i = 0; i < 16; i++) {
		for (j = 0; j < 8; j++) {
			mblock[1][0][i][j] = n = mblock[0][0][i][j] - p[j];
			mblock[3][0][i][j] = p[j];
			s += n;
			s2 += n * n;
			mblock[1][0][i + 16][j] = n = mblock[0][0][i + 16][j] - p[j + 8];
			mblock[3][0][i + 16][j] = p[j + 8];
			s += n;
			s2 += n * n;
		}

		p += mb_address.block[0].pitch;
	}

	p = from + (mb_address.block[0].pitch + 1) * 8 + mb_address.block[4].offset;

	for (i = 0; i < 8; i++) {
		for (j = 0; j < 8; j++) {
			mblock[1][4][i][j] = mblock[0][4][i][j] - p[j];
			mblock[3][4][i][j] = p[j];
			mblock[1][5][i][j] = mblock[0][5][i][j] - p[j + mb_address.block[5].offset];
			mblock[3][5][i][j] = p[j + mb_address.block[5].offset];
		}

		p += mb_address.block[4].pitch;
	}

	return s2 * 256 - (s * s);
}

/*
 *  Backward prediction (B frames only, no reconstruction)
 *
 *  mblock[1] = org - new_ref;
 */
int
predict_backward_packed(unsigned char *from)
{
	int i, n, s = 0;

	for (i = 0; i < 4 * 64; i++) {
		mblock[1][0][0][i] = n = mblock[0][0][0][i] - from[i];
		s += n * n;
	}

	for (; i < 6 * 64; i++)
		mblock[1][0][0][i] = mblock[0][0][0][i] - from[i];

	return s;
}

/*
 *  Bidirectional prediction (B frames only, no reconstruction)
 *
 *  mblock[1] = org - old_ref;
 *  mblock[2] = org - new_ref;
 *  mblock[3] = org - linear_interpolation(old_ref, new_ref);
 */
int
predict_bidirectional_packed(unsigned char *from1, unsigned char *from2, int *vmc1, int *vmc2)
{
	int i, n, si = 0, sf = 0, sb = 0;

	for (i = 0; i < 4 * 64; i++) {
		mblock[1][0][0][i] = n = mblock[0][0][0][i] - from1[i];
		sf += n * n; // forward
		mblock[2][0][0][i] = n = mblock[0][0][0][i] - from2[i];
		sb += n * n; // backward
		mblock[3][0][0][i] = n = mblock[0][0][0][i] - ((from1[i] + from2[i] + 1) >> 1);
		si += n * n; // interpolated	                   unsigned -> pavgb
	}

	for (; i < 6 * 64; i++) {
		mblock[1][0][0][i] = mblock[0][0][0][i] - from1[i];
		mblock[2][0][0][i] = mblock[0][0][0][i] - from2[i];
		mblock[3][0][0][i] = mblock[0][0][0][i] - ((from1[i] + from2[i] + 1) >> 1);
	}

	*vmc1 = sf;
	*vmc2 = sb;

	return si;
}

int
predict_bidirectional_planar(unsigned char *from1, unsigned char *from2, int *vmc1, int *vmc2)
{
	int i, j, n, si = 0, sf = 0, sb = 0;
	unsigned char *p1, *p2;

	p1 = from1;
	p2 = from2;

	for (i = 0; i < 16; i++) {
		for (j = 0; j < 8; j++) {
			mblock[1][0][i][j] = n = mblock[0][0][i][j] - p1[j];
			sf += n * n; // forward
			mblock[2][0][i][j] = n = mblock[0][0][i][j] - p2[j];
			sb += n * n; // backward
			mblock[3][0][i][j] = n = mblock[0][0][i][j] - ((p1[j] + p2[j] + 1) >> 1);
			si += n * n; // interpolated	                   unsigned -> pavgb
			mblock[1][0][i + 16][j] = n = mblock[0][0][i + 16][j] - p1[j + 8];
			sf += n * n; // forward
			mblock[2][0][i + 16][j] = n = mblock[0][0][i + 16][j] - p2[j + 8];
			sb += n * n; // backward
			mblock[3][0][i + 16][j] = n = mblock[0][0][i + 16][j] - ((p1[j + 8] + p2[j + 8] + 1) >> 1);
			si += n * n; // interpolated	                   unsigned -> pavgb
		}

		p1 += mb_address.block[0].pitch;
		p2 += mb_address.block[0].pitch;
	}

	p1 = from1 + (mb_address.block[0].pitch + 1) * 8 + mb_address.block[4].offset;
	p2 = from2 + (mb_address.block[0].pitch + 1) * 8 + mb_address.block[4].offset;

	for (i = 0; i < 8; i++) {
		for (j = 0; j < 8; j++) {
			mblock[1][4][i][j] = mblock[0][4][i][j] - p1[j];
			mblock[2][4][i][j] = mblock[0][4][i][j] - p2[j];
			mblock[3][4][i][j] = mblock[0][4][i][j] - ((p1[j] + p2[j] + 1) >> 1);
			mblock[1][5][i][j] = mblock[0][5][i][j] - p1[j + mb_address.block[5].offset];
			mblock[2][5][i][j] = mblock[0][5][i][j] - p2[j + mb_address.block[5].offset];
			mblock[3][5][i][j] = mblock[0][5][i][j] - ((p1[j + mb_address.block[5].offset] + p2[j + mb_address.block[5].offset] + 1) >> 1);
		}

		p1 += mb_address.block[4].pitch;
		p2 += mb_address.block[4].pitch;
	}

	*vmc1 = sf;
	*vmc2 = sb;

	return si;
}
