/*
 *  MPEG-1 Real Time Encoder
 *  Motion compensation V3.1.39
 *
 *  Copyright (C) 2001 Michael H. Schimek
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 2.
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

/* $Id: motion.c,v 1.1.1.1 2001-08-07 22:10:07 garetxe Exp $ */

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

search_fn *search;

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

	asm volatile (
		" movq		(%0),%%mm0;\n"
		" movq		%%mm0,%%mm1;\n"
		" movq		%%mm0,temp11(%1);\n"
		" psrlq		$8,%%mm1;\n"
		" movq		8(%0),%%mm4;\n"
		" movq		%%mm4,%%mm2;\n"
		" movq		%%mm4,temp11+8(%1);\n"

		" psllq		$56,%%mm2;\n"
		" movq		c1b,%%mm7;\n"
		" por		%%mm2,%%mm1;\n"
		" movq		%%mm0,%%mm2;\n"
		" movq		%%mm0,%%mm5;\n"
		" por		%%mm1,%%mm2;\n"
		" pxor		%%mm1,%%mm5;\n"
		" por		%%mm7,%%mm0;\n"
		" por		%%mm7,%%mm1;\n"
		" psrlq		$1,%%mm0;\n"
		" pand		%%mm7,%%mm2;\n"
		" psrlq		$1,%%mm1;\n"
		" paddb		%%mm0,%%mm1;\n"
		" paddb		%%mm2,%%mm1;\n"
		" movq		%%mm1,temp2h(%1);\n"

		" pushl		%%ecx;\n"
		" movzxb	16(%0),%%ecx;\n"
		" movd		%%ecx,%%mm2;\n"
		" movb		%%cl,temp11+18*16(%2);\n"
		" movb		17(%0),%%cl;\n"
		" movb		%%cl,temp2h+18*16(%2);\n"
		" popl		%%ecx;\n"

		" movq		%%mm4,%%mm1;\n"
		" psrlq		$8,%%mm1;\n"
		" psllq		$56,%%mm2;\n"
		" por		%%mm2,%%mm1;\n"
		" movq		%%mm4,%%mm2;\n"
		" movq		%%mm4,%%mm6;\n"
		" por		%%mm1,%%mm2;\n"
		" pxor		%%mm1,%%mm6;\n"
		" por		%%mm7,%%mm4;\n"
		" por		%%mm7,%%mm1;\n"
		" psrlq		$1,%%mm4;\n"
		" pand		%%mm7,%%mm2;\n"
		" psrlq		$1,%%mm1;\n"
		" paddb		%%mm4,%%mm1;\n"
		" paddb		%%mm2,%%mm1;\n"
		" movq		%%mm1,temp2h+8(%1);\n"
	:: "S" (p1), "c" (0), "r" (0) : "memory");

	for (y = 1; y < 17; y++) {
		asm volatile (
			" movq		(%0),%%mm1;\n"
			" movq		%%mm1,temp11(%1);\n"

			" movq		temp11-16(%1),%%mm0;\n"
			" movq		%%mm0,%%mm2;\n"
			" movq		%%mm1,%%mm3;\n"
			" por		%%mm1,%%mm2;\n"
			" por		%%mm7,%%mm0;\n"
			" por		%%mm7,%%mm1;\n"
			" psrlq		$1,%%mm0;\n"
			" pand		%%mm7,%%mm2;\n"
			" psrlq		$1,%%mm1;\n"
			" paddb		%%mm0,%%mm1;\n"
			" paddb		%%mm2,%%mm1;\n"
			" movq		%%mm1,temp2v-16(%1);\n"

			" movq		8(%0),%%mm1;\n"
			" movq		%%mm1,temp11+8(%1);\n"

			" movq		temp11+8-16(%1),%%mm0;\n"
			" movq		%%mm0,%%mm2;\n"
			" movq		%%mm1,%%mm4;\n"
			" por		%%mm1,%%mm2;\n"
			" por		%%mm7,%%mm0;\n"
			" por		%%mm7,%%mm1;\n"
			" psrlq		$1,%%mm0;\n"
			" pand		%%mm7,%%mm2;\n"
			" psrlq		$1,%%mm1;\n"
			" paddb		%%mm0,%%mm1;\n"
			" paddb		%%mm2,%%mm1;\n"
			" movq		%%mm1,temp2v+8-16(%1);\n"

			" movq		%%mm3,%%mm0;\n"
			" movq		temp11+1(%1),%%mm1;\n"
			" movq		%%mm0,%%mm2;\n"
			" movq		%%mm0,%%mm3;\n"
			" por		%%mm1,%%mm2;\n"
			" pxor		%%mm1,%%mm3;\n"
			" por		%%mm7,%%mm0;\n"
			" por		%%mm7,%%mm1;\n"
			" psrlq		$1,%%mm0;\n"
			" pand		%%mm7,%%mm2;\n"
			" psrlq		$1,%%mm1;\n"
			" paddb		%%mm0,%%mm1;\n"
			" paddb		%%mm2,%%mm1;\n"
			" movq		%%mm1,temp2h(%1);\n"

			" movq		temp2h-16(%1),%%mm0;\n"
			" movq		%%mm0,%%mm2;\n"
			" pxor		%%mm1,%%mm2;\n"
			" por		%%mm3,%%mm5;\n"
			" pandn		%%mm2,%%mm5;\n"
			" movq		%%mm0,%%mm2;\n"
			" pand		%%mm1,%%mm2;\n"
			" pxor		%%mm5,%%mm2;\n"
			" movq		%%mm3,%%mm5;\n"
			" por		%%mm7,%%mm0;\n"
			" por		%%mm7,%%mm1;\n"
			" psrlq		$1,%%mm0;\n"
			" pand		%%mm7,%%mm2;\n"
			" psrlq		$1,%%mm1;\n"
			" paddb		%%mm1,%%mm0;\n"
			" paddb		%%mm2,%%mm0;\n"
			" movq		%%mm0,temp22-16(%1);\n"

			" pushl		%%ecx;\n"
			" movzxb		16(%0),%%ecx;\n"
			" movd		%%ecx,%%mm2;\n"
			" movb		%%cl,temp11+18*16(%2);\n"
			" movb		17(%0),%%cl;\n"
			" movb		%%cl,temp2h+18*16(%2);\n"
			" popl		%%ecx;\n"

			" movq		%%mm4,%%mm1;\n"
			" psrlq		$8,%%mm1;\n"
			" psllq		$56,%%mm2;\n"
			" por		%%mm2,%%mm1;\n"
			" movq		%%mm4,%%mm3;\n"
			" pxor		%%mm1,%%mm3;\n"
			" movq		%%mm4,%%mm2;\n"
			" por		%%mm1,%%mm2;\n"
			" por		%%mm7,%%mm4;\n"
			" por		%%mm7,%%mm1;\n"
			" psrlq		$1,%%mm4;\n"
			" pand		%%mm7,%%mm2;\n"
			" psrlq		$1,%%mm1;\n"
			" paddb		%%mm4,%%mm1;\n"
			" paddb		%%mm2,%%mm1;\n"
			" movq		%%mm1,temp2h+8(%1);\n"

			" movq		temp2h+8-16(%1),%%mm0;\n"
			" movq		%%mm0,%%mm2;\n"
			" pxor		%%mm1,%%mm2;\n"
			" por		%%mm3,%%mm6;\n"
			" pandn		%%mm2,%%mm6;\n"
			" movq		%%mm0,%%mm2;\n"
			" pand		%%mm1,%%mm2;\n"
			" pxor		%%mm6,%%mm2;\n"
			" movq		%%mm3,%%mm6;\n"
			" por		%%mm7,%%mm0;\n"
			" por		%%mm7,%%mm1;\n"
			" psrlq		$1,%%mm0;\n"
			" pand		%%mm7,%%mm2;\n"
			" psrlq		$1,%%mm1;\n"
			" paddb		%%mm0,%%mm1;\n"
			" paddb		%%mm2,%%mm1;\n"
			" movq		%%mm1,temp22+8-16(%1);\n"
		:: "S" (p1 + y * pitch), "c" (y * 16), "r" (y) : "memory");
	}

	asm volatile (
		" movq		temp11-16(%1),%%mm0;\n"
		" movq		(%0),%%mm1;\n"
		" movq		%%mm0,%%mm2;\n"
		" movq		%%mm1,%%mm3;\n"
		" por		%%mm1,%%mm2;\n"
		" por		%%mm7,%%mm0;\n"
		" por		%%mm7,%%mm1;\n"
		" psrlq		$1,%%mm0;\n"
		" pand		%%mm7,%%mm2;\n"
		" psrlq		$1,%%mm1;\n"
		" paddb		%%mm0,%%mm1;\n"
		" paddb		%%mm2,%%mm1;\n"
		" movq		%%mm1,temp2v-16(%1);\n"

		" movq		temp11+8-16(%1),%%mm0;\n"
		" movq		8(%0),%%mm1;\n"
		" movq		%%mm0,%%mm2;\n"
		" movq		%%mm1,%%mm4;\n"
		" por		%%mm1,%%mm2;\n"
		" por		%%mm7,%%mm0;\n"
		" por		%%mm7,%%mm1;\n"
		" psrlq		$1,%%mm0;\n"
		" pand		%%mm7,%%mm2;\n"
		" psrlq		$1,%%mm1;\n"
		" paddb		%%mm0,%%mm1;\n"
		" paddb		%%mm2,%%mm1;\n"
		" movq		%%mm1,temp2v+8-16(%1);\n"

		" movq		%%mm3,%%mm0;\n"
		" movq		%%mm3,%%mm1;\n"
		" movq		%%mm4,%%mm2;\n"
		" psrlq		$8,%%mm1;\n"
		" psllq		$56,%%mm2;\n"
		" por		%%mm2,%%mm1;\n"
		" movq		%%mm0,%%mm2;\n"
		" por		%%mm1,%%mm2;\n"
		" pxor		%%mm1,%%mm3;\n"
		" por		%%mm7,%%mm0;\n"
		" por		%%mm7,%%mm1;\n"
		" psrlq		$1,%%mm0;\n"
		" pand		%%mm7,%%mm2;\n"
		" psrlq		$1,%%mm1;\n"
		" paddb		%%mm0,%%mm1;\n"
		" movq		temp2h-16(%1),%%mm0;\n"
		" por		%%mm3,%%mm5;\n"
		" paddb		%%mm2,%%mm1;\n"
		" movq		%%mm0,%%mm2;\n"
		" pxor		%%mm1,%%mm2;\n"
		" pandn		%%mm2,%%mm5;\n"
		" movq		%%mm0,%%mm2;\n"
		" pand		%%mm1,%%mm2;\n"
		" pxor		%%mm5,%%mm2;\n"
		" por		%%mm7,%%mm0;\n"
		" por		%%mm7,%%mm1;\n"
		" psrlq		$1,%%mm0;\n"
		" pand		%%mm7,%%mm2;\n"
		" psrlq		$1,%%mm1;\n"
		" paddb		%%mm0,%%mm1;\n"
		" paddb		%%mm2,%%mm1;\n"
		" movq		%%mm1,temp22-16(%1);\n"

		" movq		%%mm4,%%mm0;\n"
		" movq		%%mm4,%%mm1;\n"
		" psrlq		$8,%%mm1;\n"
		" pushl		%%edx;\n"
		" movzxb	16(%0),%%edx;\n"
		" movd		%%edx,%%mm2;\n"
		" popl		%%edx;\n"
		" psllq		$56,%%mm2;\n"
		" por		%%mm2,%%mm1;\n"
		" movq		%%mm0,%%mm2;\n"
		" por		%%mm1,%%mm2;\n"
		" pxor		%%mm1,%%mm4;\n"
		" por		%%mm7,%%mm0;\n"
		" por		%%mm7,%%mm1;\n"
		" psrlq		$1,%%mm0;\n"
		" pand		%%mm7,%%mm2;\n"
		" psrlq		$1,%%mm1;\n"
		" paddb		%%mm0,%%mm1;\n"
		" paddb		%%mm2,%%mm1;\n"
		" movq		temp2h+8-16(%1),%%mm0;\n"
		" movq		%%mm0,%%mm2;\n"
		" pxor		%%mm1,%%mm2;\n"
		" por		%%mm4,%%mm6;\n"
		" pandn		%%mm2,%%mm6;\n"
		" movq		%%mm0,%%mm2;\n"
		" pand		%%mm1,%%mm2;\n"
		" pxor		%%mm6,%%mm2;\n"
		" por		%%mm7,%%mm0;\n"
		" por		%%mm7,%%mm1;\n"
		" psrlq		$1,%%mm0;\n"
		" pand		%%mm7,%%mm2;\n"
		" psrlq		$1,%%mm1;\n"
		" paddb		%%mm0,%%mm1;\n"
		" paddb		%%mm2,%%mm1;\n"
		" movq		%%mm1,temp22+8-16(%1);\n"

		/* temp2v 16 */

		" movq		temp11+18*16+0,%%mm0;\n"
		" movq		%%mm0,%%mm3;\n"
		" movq		temp11+18*16+1,%%mm1;\n"
		" movq		%%mm0,%%mm2;\n"
		" por		%%mm1,%%mm2;\n"
		" por		%%mm7,%%mm0;\n"
		" por		%%mm7,%%mm1;\n"
		" psrlq		$1,%%mm0;\n"
		" pand		%%mm7,%%mm2;\n"
		" psrlq		$1,%%mm1;\n"
		" paddb		%%mm0,%%mm1;\n"
		" paddb		%%mm2,%%mm1;\n"
		" movq		%%mm1,temp2v+18*16+0;\n"

		" movq		temp11+18*16+8,%%mm0;\n"
		" movq		%%mm0,%%mm4;\n"
		" movq		%%mm0,%%mm1;\n"
		" psrlq		$8,%%mm1;\n"
		" pushl		%%ecx;\n"
		" pushl		%%edx;\n"
		" movzxb	temp11+18*16+16,%%ecx;\n"
		" movd		%%ecx,%%mm2;\n"
		" movzxb	16(%0),%%edx;\n"
		" addl		%%edx,%%ecx;\n"
		" incl		%%ecx;\n"
		" shrl		$1,%%ecx;\n"
		" movb		%%cl,temp2v+18*16+16;\n"
		" popl		%%edx;\n"
		" popl		%%ecx;\n"
		" psllq		$56,%%mm2;\n"
		" por		%%mm2,%%mm1;\n"
		" movq		%%mm0,%%mm2;\n"
		" por		%%mm1,%%mm2;\n"
		" por		%%mm7,%%mm0;\n"
		" por		%%mm7,%%mm1;\n"
		" psrlq		$1,%%mm0;\n"
		" pand		%%mm7,%%mm2;\n"
		" psrlq		$1,%%mm1;\n"
		" paddb		%%mm0,%%mm1;\n"
		" paddb		%%mm2,%%mm1;\n"
		" movq		%%mm1,temp2v+18*16+8;\n"

		/* temp2h 16 */

		" movq		temp2h+18*16+0,%%mm0;\n"
		" movq		%%mm3,%%mm2;\n"
		" movq		%%mm0,%%mm5;\n"
		" por		%%mm0,%%mm2;\n"
		" pxor		%%mm3,%%mm5;\n"
		" por		%%mm7,%%mm3;\n"
		" por		%%mm7,%%mm0;\n"
		" psrlq		$1,%%mm3;\n"
		" pand		%%mm7,%%mm2;\n"
		" psrlq		$1,%%mm0;\n"
		" paddb		%%mm3,%%mm0;\n"
		" paddb		%%mm2,%%mm0;\n"
		" movq		%%mm0,temp2h+18*16+0;\n"

		" movq		temp2h+18*16+8,%%mm1;\n"
		" movq		%%mm4,%%mm2;\n"
		" movq		%%mm1,%%mm6;\n"
		" por		%%mm1,%%mm2;\n"
		" pxor		%%mm4,%%mm6;\n"
		" por		%%mm7,%%mm4;\n"
		" por		%%mm7,%%mm1;\n"
		" psrlq		$1,%%mm4;\n"
		" pand		%%mm7,%%mm2;\n"
		" psrlq		$1,%%mm1;\n"
		" paddb		%%mm4,%%mm1;\n"
		" paddb		%%mm2,%%mm1;\n"
		" movq		%%mm1,temp2h+18*16+8;\n"

		" pushl		%%ecx;\n"
		" pushl		%%edx;\n"
		" movzxb	temp11+18*16+16,%%ecx;\n"
		" movzxb	temp2h+18*16+16,%%edx;\n"
		" addl		%%edx,%%ecx;\n"
		" movd		%%ecx,%%mm4;\n"
		" incl		%%ecx;\n"
		" movl		%%ecx,%%edx;\n"
		" shrl		$1,%%ecx;\n"
		" movb		%%cl,temp2h+18*16+16;\n"
		" movzxb	16(%0),%%ecx;\n"
		" addl		%%ecx,%%edx;\n"
		" movzxb	17(%0),%%ecx;\n"
		" addl		%%ecx,%%edx;\n"
		" incl		%%edx;\n"
		" shrl		$2,%%edx;\n"
		" movb		%%dl,temp22+18*16+16;\n"
		" popl		%%edx;\n"
		" popl		%%ecx;\n"

		/* temp22 16 [1 & ((ab & cd) ^ ((ab ^ cd) & ~((a ^ b) | (c ^ d))))] */

		" movq		%%mm5,%%mm2;\n"
		" movq		%%mm6,%%mm3;\n"
		" psrlq		$8,%%mm2;\n"
		" psllq		$56,%%mm3;\n"
		" por		%%mm2,%%mm5;\n"
		" por		%%mm3,%%mm5;\n"
		" movq		temp2h+18*16+1,%%mm3;\n"
		" movq		%%mm0,%%mm2;\n"
		" pxor		%%mm3,%%mm2;\n"
		" pandn		%%mm2,%%mm5;\n"
		" movq		%%mm0,%%mm2;\n"
		" pand		%%mm3,%%mm2;\n"
		" pxor		%%mm2,%%mm5;\n"
		" pand		%%mm7,%%mm5;\n"
		" por		%%mm7,%%mm3;\n"
		" por		%%mm7,%%mm0;\n"
		" psrlq		$1,%%mm3;\n"
		" psrlq		$1,%%mm0;\n"
		" paddb		%%mm3,%%mm0;\n"
		" paddb		%%mm5,%%mm0;\n"
		" movq		%%mm0,temp22+18*16+0;\n"

		" movq		%%mm6,%%mm2;\n"
		" psrlq		$8,%%mm2;\n"
		" psllq		$56,%%mm4;\n"
		" por		%%mm2,%%mm6;\n"
		" por		%%mm4,%%mm6;\n"
		" movq		temp2h+18*16+9,%%mm4;\n"
		" movq		%%mm1,%%mm2;\n"
		" pxor		%%mm4,%%mm2;\n"
		" pandn		%%mm2,%%mm6;\n"
		" movq		%%mm1,%%mm2;\n"
		" pand		%%mm4,%%mm2;\n"
		" pxor		%%mm2,%%mm6;\n"
		" pand		%%mm7,%%mm6;\n"
		" por		%%mm7,%%mm4;\n"
		" por		%%mm7,%%mm1;\n"
		" psrlq		$1,%%mm4;\n"
		" psrlq		$1,%%mm1;\n"
		" paddb		%%mm4,%%mm1;\n"
		" paddb		%%mm6,%%mm1;\n"
		" movq		%%mm1,temp22+18*16+8;\n"

	:: "S" (p1 + y * pitch), "c" (y * 16), "r" (y) : "memory");
}

static inline void
sse_load_interp(unsigned char *p, int pitch, int dx, int dy)
{
	unsigned char *p1 = p + dx + dy * pitch;
	int y;

	asm volatile (
		" movq		(%0),%%mm0;\n"
		" movq		%%mm0,%%mm1;\n"
		" movq		%%mm0,temp11(%1);\n"
		" psrlq		$8,%%mm1;\n"
		" movq		8(%0),%%mm4;\n"
		" movq		%%mm4,%%mm2;\n"
		" movq		%%mm4,temp11+8(%1);\n"
		" psllq		$56,%%mm2;\n"
		" movq		c1b,%%mm7;\n"
		" por		%%mm2,%%mm1;\n"
		" movq		%%mm1,%%mm5;\n"
		" pavgb		%%mm0,%%mm1;\n"
		" pxor		%%mm0,%%mm5;\n"
		" movq		%%mm1,temp2h(%1);\n"

		" pushl		%%ecx;\n"
		" movzxb	16(%0),%%ecx;\n"
		" movd		%%ecx,%%mm2;\n"
		" movb		%%cl,temp11+18*16(%2);\n"
		" movb		17(%0),%%cl;\n"
		" movb		%%cl,temp2h+18*16(%2);\n"
		" popl		%%ecx;\n"

		" psllq		$56,%%mm2;\n"
		" movq		%%mm4,%%mm1;\n"
		" psrlq		$8,%%mm1;\n"
		" por		%%mm2,%%mm1;\n"
		" movq		%%mm1,%%mm6;\n"
		" pavgb		%%mm4,%%mm1;\n"
		" pxor		%%mm4,%%mm6;\n"
		" movq		%%mm1,temp2h+8(%1);\n"
	:: "S" (p1), "c" (0), "r" (0) : "memory");

	for (y = 1; y < 17; y++) {
		asm volatile (
			" movq		(%0),%%mm1;\n"
			" movq		%%mm1,temp11(%1);\n"
			" movq		%%mm1,%%mm3;\n"
			" pavgb		temp11-16(%1),%%mm1;\n"
			" movq		%%mm1,temp2v-16(%1);\n"

			" movq		8(%0),%%mm1;\n"
			" movq		%%mm1,temp11+8(%1);\n"
			" movq		%%mm1,%%mm4;\n"
			" pavgb		temp11+8-16(%1),%%mm1;\n"
			" movq		%%mm1,temp2v+8-16(%1);\n"

			" movq		%%mm3,%%mm0;\n"
			" movq		temp11+1(%1),%%mm1;\n"
			" pxor		%%mm1,%%mm3;\n"
			" pavgb		%%mm0,%%mm1;\n"
			" movq		%%mm1,temp2h(%1);\n"

			" movq		temp2h-16(%1),%%mm0;\n"
			" movq		%%mm0,%%mm2;\n"
			" pxor		%%mm1,%%mm2;\n"
			" por		%%mm3,%%mm5;\n"
			" pandn		%%mm2,%%mm5;\n"
			" movq		%%mm0,%%mm2;\n"
			" pand		%%mm1,%%mm2;\n"
			" pxor		%%mm5,%%mm2;\n"
			" movq		%%mm3,%%mm5;\n"
			" por		%%mm7,%%mm0;\n"
			" por		%%mm7,%%mm1;\n"
			" psrlq		$1,%%mm0;\n"
			" pand		%%mm7,%%mm2;\n"
			" psrlq		$1,%%mm1;\n"
			" paddb		%%mm1,%%mm0;\n"
			" paddb		%%mm2,%%mm0;\n"
			" movq		%%mm0,temp22-16(%1);\n"

			" pushl		%%ecx;\n"
			" movzxb	16(%0),%%ecx;\n"
			" movd		%%ecx,%%mm2;\n"
			" movb		%%cl,temp11+18*16(%2);\n"
			" movb		17(%0),%%cl;\n"
			" movb		%%cl,temp2h+18*16(%2);\n"
			" popl		%%ecx;\n"

			" movq		%%mm4,%%mm1;\n"
			" psrlq		$8,%%mm1;\n"
			" psllq		$56,%%mm2;\n"
			" por		%%mm2,%%mm1;\n"
			" movq		%%mm1,%%mm3;\n"
			" pavgb		%%mm4,%%mm1;\n"
			" pxor		%%mm4,%%mm3;\n"
			" movq		%%mm1,temp2h+8(%1);\n"

			" movq		temp2h+8-16(%1),%%mm0;\n"
			" movq		%%mm0,%%mm2;\n"
			" pxor		%%mm1,%%mm2;\n"
			" por		%%mm3,%%mm6;\n"
			" pandn		%%mm2,%%mm6;\n"
			" movq		%%mm0,%%mm2;\n"
			" pand		%%mm1,%%mm2;\n"
			" pxor		%%mm6,%%mm2;\n"
			" movq		%%mm3,%%mm6;\n"
			" por		%%mm7,%%mm0;\n"
			" por		%%mm7,%%mm1;\n"
			" psrlq		$1,%%mm0;\n"
			" pand		%%mm7,%%mm2;\n"
			" psrlq		$1,%%mm1;\n"
			" paddb		%%mm0,%%mm1;\n"
			" paddb		%%mm2,%%mm1;\n"
			" movq		%%mm1,temp22+8-16(%1);\n"
		:: "S" (p1 + y * pitch), "c" (y * 16), "r" (y) : "memory");
	}

	asm volatile (
		" movq		(%0),%%mm1;\n"
		" movq		%%mm1,%%mm3;\n"
		" pavgb		temp11-16(%1),%%mm1;\n"
		" movq		%%mm3,%%mm0;\n"
		" movq		%%mm1,temp2v-16(%1);\n"
		" movq		8(%0),%%mm1;\n"
		" movq		%%mm1,%%mm4;\n"
		" pavgb		temp11+8-16(%1),%%mm1;\n"
		" movq		%%mm4,%%mm2;\n"
		" movq		%%mm1,temp2v+8-16(%1);\n"
		" movq		%%mm3,%%mm1;\n"
		" psrlq		$8,%%mm1;\n"
		" psllq		$56,%%mm2;\n"
		" por		%%mm2,%%mm1;\n"
		" movq		%%mm0,%%mm2;\n"
		" por		%%mm1,%%mm2;\n"
		" pxor		%%mm1,%%mm3;\n"
		" por		%%mm7,%%mm0;\n"
		" por		%%mm7,%%mm1;\n"
		" psrlq		$1,%%mm0;\n"
		" pand		%%mm7,%%mm2;\n"
		" psrlq		$1,%%mm1;\n"
		" paddb		%%mm0,%%mm1;\n"
		" movq		temp2h-16(%1),%%mm0;\n"
		" por		%%mm3,%%mm5;\n"
		" paddb		%%mm2,%%mm1;\n"
		" movq		%%mm0,%%mm2;\n"
		" pxor		%%mm1,%%mm2;\n"
		" pandn		%%mm2,%%mm5;\n"
		" movq		%%mm0,%%mm2;\n"
		" pand		%%mm1,%%mm2;\n"
		" pxor		%%mm5,%%mm2;\n"
		" por		%%mm7,%%mm0;\n"
		" por		%%mm7,%%mm1;\n"
		" psrlq		$1,%%mm0;\n"
		" pand		%%mm7,%%mm2;\n"
		" psrlq		$1,%%mm1;\n"
		" paddb		%%mm0,%%mm1;\n"
		" paddb		%%mm2,%%mm1;\n"
		" movq		%%mm1,temp22-16(%1);\n"

		" movq		%%mm4,%%mm0;\n"
		" movq		%%mm4,%%mm1;\n"
		" psrlq		$8,%%mm1;\n"
		" pushl		%%edx;\n"
		" movzxb	16(%0),%%edx;\n"
		" movd		%%edx,%%mm2;\n"
		" popl		%%edx;\n"
		" psllq		$56,%%mm2;\n"
		" por		%%mm2,%%mm1;\n"
		" pxor		%%mm1,%%mm4;\n"
		" pavgb		%%mm0,%%mm1;\n"
		" movq		temp2h+8-16(%1),%%mm0;\n"
		" movq		%%mm0,%%mm2;\n"
		" pxor		%%mm1,%%mm2;\n"
		" por		%%mm4,%%mm6;\n"
		" pandn		%%mm2,%%mm6;\n"
		" movq		%%mm0,%%mm2;\n"
		" pand		%%mm1,%%mm2;\n"
		" pxor		%%mm6,%%mm2;\n"
		" por		%%mm7,%%mm0;\n"
		" por		%%mm7,%%mm1;\n"
		" psrlq		$1,%%mm0;\n"
		" pand		%%mm7,%%mm2;\n"
		" psrlq		$1,%%mm1;\n"
		" paddb		%%mm0,%%mm1;\n"
		" paddb		%%mm2,%%mm1;\n"
		" movq		%%mm1,temp22+8-16(%1);\n"

		/* temp2v 16 */

		" movq		temp11+18*16+0,%%mm0;\n"
		" movq		%%mm0,%%mm3;\n"
		" movq		temp11+18*16+1,%%mm1;\n"
		" pavgb		%%mm0,%%mm1;\n"
		" movq		%%mm1,temp2v+18*16+0;\n"
		" movq		temp11+18*16+8,%%mm0;\n"
		" movq		%%mm0,%%mm4;\n"
		" movq		%%mm0,%%mm1;\n"
		" psrlq		$8,%%mm1;\n"

		" pushl		%%ecx;\n"
		" pushl		%%edx;\n"
		" movzxb	temp11+18*16+16,%%ecx;\n"
		" movd		%%ecx,%%mm2;\n"
		" movzxb	16(%0),%%edx;\n"
		" addl		%%edx,%%ecx;\n"
		" incl		%%ecx;\n"
		" shrl		$1,%%ecx;\n"
		" movb		%%cl,temp2v+18*16+16;\n"
		" popl		%%edx;\n"
		" popl		%%ecx;\n"

		" psllq		$56,%%mm2;\n"
		" por		%%mm2,%%mm1;\n"
		" pavgb		%%mm0,%%mm1;\n"
		" movq		%%mm1,temp2v+18*16+8;\n"

		/* temp2h 16 */

		" movq		temp2h+18*16+0,%%mm0;\n"
		" movq		%%mm0,%%mm5;\n"
		" pavgb		%%mm3,%%mm0;\n"
		" movq		%%mm0,temp2h+18*16+0;\n"
		" pxor		%%mm3,%%mm5;\n"
		" movq		temp2h+18*16+8,%%mm1;\n"
		" movq		%%mm1,%%mm6;\n"
		" pavgb		%%mm4,%%mm1;\n"
		" pxor		%%mm4,%%mm6;\n"
		" movq		%%mm1,temp2h+18*16+8;\n"

		" pushl		%%ecx;\n"
		" pushl		%%edx;\n"
		" movzxb	temp11+18*16+16,%%ecx;\n"
		" movzxb	temp2h+18*16+16,%%edx;\n"
		" addl		%%edx,%%ecx;\n"
		" movd		%%ecx,%%mm4;\n"
		" incl		%%ecx;\n"
		" movl		%%ecx,%%edx;\n"
		" shrl		$1,%%ecx;\n"
		" movb		%%cl,temp2h+18*16+16;\n"
		" movzxb	16(%0),%%ecx;\n"
		" addl		%%ecx,%%edx;\n"
		" movzxb	17(%0),%%ecx;\n"
		" addl		%%ecx,%%edx;\n"
		" incl		%%edx;\n"
		" shrl		$2,%%edx;\n"
		" movb		%%dl,temp22+18*16+16;\n"
		" popl		%%edx;\n"
		" popl		%%ecx;\n"

		/* temp22 16 [1 & ((ab & cd) ^ ((ab ^ cd) & ~((a ^ b) | (c ^ d))))] */

		" movq		%%mm5,%%mm2;\n"
		" movq		%%mm6,%%mm3;\n"
		" psrlq		$8,%%mm2;\n"
		" psllq		$56,%%mm3;\n"
		" por		%%mm2,%%mm5;\n"
		" por		%%mm3,%%mm5;\n"
		" movq		temp2h+18*16+1,%%mm3;\n"
		" movq		%%mm0,%%mm2;\n"
		" pxor		%%mm3,%%mm2;\n"
		" pandn		%%mm2,%%mm5;\n"
		" movq		%%mm0,%%mm2;\n"
		" pand		%%mm3,%%mm2;\n"
		" pxor		%%mm2,%%mm5;\n"
		" pand		%%mm7,%%mm5;\n"
		" por		%%mm7,%%mm3;\n"
		" por		%%mm7,%%mm0;\n"
		" psrlq		$1,%%mm3;\n"
		" psrlq		$1,%%mm0;\n"
		" paddb		%%mm3,%%mm0;\n"
		" paddb		%%mm5,%%mm0;\n"
		" movq		%%mm0,temp22+18*16+0;\n"

		" movq		%%mm6,%%mm2;\n"
		" psrlq		$8,%%mm2;\n"
		" psllq		$56,%%mm4;\n"
		" por		%%mm2,%%mm6;\n"
		" por		%%mm4,%%mm6;\n"
		" movq		temp2h+18*16+9,%%mm4;\n"
		" movq		%%mm1,%%mm2;\n"
		" pxor		%%mm4,%%mm2;\n"
		" pandn		%%mm2,%%mm6;\n"
		" movq		%%mm1,%%mm2;\n"
		" pand		%%mm4,%%mm2;\n"
		" pxor		%%mm2,%%mm6;\n"
		" pand		%%mm7,%%mm6;\n"
		" por		%%mm7,%%mm4;\n"
		" por		%%mm7,%%mm1;\n"
		" psrlq		$1,%%mm4;\n"
		" psrlq		$1,%%mm1;\n"
		" paddb		%%mm4,%%mm1;\n"
		" paddb		%%mm6,%%mm1;\n"
		" movq		%%mm1,temp22+18*16+8;\n"

	:: "S" (p1 + y * pitch), "c" (y * 16), "r" (y) : "memory");
}

static inline void
_3dn_load_interp(unsigned char *p, int pitch, int dx, int dy)
{
	unsigned char *p1 = p + dx + dy * pitch;
	int y;

	asm volatile (
		" movq		(%0),%%mm0;\n"
		" movq		%%mm0,%%mm1;\n"
		" movq		%%mm0,temp11(%1);\n"
		" psrlq		$8,%%mm1;\n"
		" movq		8(%0),%%mm4;\n"
		" movq		%%mm4,%%mm2;\n"
		" movq		%%mm4,temp11+8(%1);\n"
		" psllq		$56,%%mm2;\n"
		" movq		c1b,%%mm7;\n"
		" por		%%mm2,%%mm1;\n"
		" movq		%%mm1,%%mm5;\n"
		" pavgusb	%%mm0,%%mm1;\n"
		" pxor		%%mm0,%%mm5;\n"
		" movq		%%mm1,temp2h(%1);\n"

		" pushl		%%ecx;\n"
		" movzxb	16(%0),%%ecx;\n"
		" movd		%%ecx,%%mm2;\n"
		" movb		%%cl,temp11+18*16(%2);\n"
		" movb		17(%0),%%cl;\n"
		" movb		%%cl,temp2h+18*16(%2);\n"
		" popl		%%ecx;\n"

		" psllq		$56,%%mm2;\n"
		" movq		%%mm4,%%mm1;\n"
		" psrlq		$8,%%mm1;\n"
		" por		%%mm2,%%mm1;\n"
		" movq		%%mm1,%%mm6;\n"
		" pavgusb	%%mm4,%%mm1;\n"
		" pxor		%%mm4,%%mm6;\n"
		" movq		%%mm1,temp2h+8(%1);\n"
	:: "S" (p1), "c" (0), "r" (0) : "memory");

	for (y = 1; y < 17; y++) {
		asm volatile (
			" movq		(%0),%%mm1;\n"
			" movq		%%mm1,temp11(%1);\n"
			" movq		%%mm1,%%mm3;\n"
			" pavgusb	temp11-16(%1),%%mm1;\n"
			" movq		%%mm1,temp2v-16(%1);\n"

			" movq		8(%0),%%mm1;\n"
			" movq		%%mm1,temp11+8(%1);\n"
			" movq		%%mm1,%%mm4;\n"
			" pavgusb	temp11+8-16(%1),%%mm1;\n"
			" movq		%%mm1,temp2v+8-16(%1);\n"

			" movq		%%mm3,%%mm0;\n"
			" movq		temp11+1(%1),%%mm1;\n"
			" pxor		%%mm1,%%mm3;\n"
			" pavgusb	%%mm0,%%mm1;\n"
			" movq		%%mm1,temp2h(%1);\n"

			" movq		temp2h-16(%1),%%mm0;\n"
			" movq		%%mm0,%%mm2;\n"
			" pxor		%%mm1,%%mm2;\n"
			" por		%%mm3,%%mm5;\n"
			" pandn		%%mm2,%%mm5;\n"
			" movq		%%mm0,%%mm2;\n"
			" pand		%%mm1,%%mm2;\n"
			" pxor		%%mm5,%%mm2;\n"
			" movq		%%mm3,%%mm5;\n"
			" por		%%mm7,%%mm0;\n"
			" por		%%mm7,%%mm1;\n"
			" psrlq		$1,%%mm0;\n"
			" pand		%%mm7,%%mm2;\n"
			" psrlq		$1,%%mm1;\n"
			" paddb		%%mm1,%%mm0;\n"
			" paddb		%%mm2,%%mm0;\n"
			" movq		%%mm0,temp22-16(%1);\n"

			" pushl		%%ecx;\n"
			" movzxb	16(%0),%%ecx;\n"
			" movd		%%ecx,%%mm2;\n"
			" movb		%%cl,temp11+18*16(%2);\n"
			" movb		17(%0),%%cl;\n"
			" movb		%%cl,temp2h+18*16(%2);\n"
			" popl		%%ecx;\n"

			" movq		%%mm4,%%mm1;\n"
			" psrlq		$8,%%mm1;\n"
			" psllq		$56,%%mm2;\n"
			" por		%%mm2,%%mm1;\n"
			" movq		%%mm1,%%mm3;\n"
			" pavgusb	%%mm4,%%mm1;\n"
			" pxor		%%mm4,%%mm3;\n"
			" movq		%%mm1,temp2h+8(%1);\n"

			" movq		temp2h+8-16(%1),%%mm0;\n"
			" movq		%%mm0,%%mm2;\n"
			" pxor		%%mm1,%%mm2;\n"
			" por		%%mm3,%%mm6;\n"
			" pandn		%%mm2,%%mm6;\n"
			" movq		%%mm0,%%mm2;\n"
			" pand		%%mm1,%%mm2;\n"
			" pxor		%%mm6,%%mm2;\n"
			" movq		%%mm3,%%mm6;\n"
			" por		%%mm7,%%mm0;\n"
			" por		%%mm7,%%mm1;\n"
			" psrlq		$1,%%mm0;\n"
			" pand		%%mm7,%%mm2;\n"
			" psrlq		$1,%%mm1;\n"
			" paddb		%%mm0,%%mm1;\n"
			" paddb		%%mm2,%%mm1;\n"
			" movq		%%mm1,temp22+8-16(%1);\n"
		:: "S" (p1 + y * pitch), "c" (y * 16), "r" (y) : "memory");
	}

	asm volatile (
		" movq		(%0),%%mm1;\n"
		" movq		%%mm1,%%mm3;\n"
		" pavgusb	temp11-16(%1),%%mm1;\n"
		" movq		%%mm3,%%mm0;\n"
		" movq		%%mm1,temp2v-16(%1);\n"
		" movq		8(%0),%%mm1;\n"
		" movq		%%mm1,%%mm4;\n"
		" pavgusb	temp11+8-16(%1),%%mm1;\n"
		" movq		%%mm4,%%mm2;\n"
		" movq		%%mm1,temp2v+8-16(%1);\n"
		" movq		%%mm3,%%mm1;\n"
		" psrlq		$8,%%mm1;\n"
		" psllq		$56,%%mm2;\n"
		" por		%%mm2,%%mm1;\n"
		" movq		%%mm0,%%mm2;\n"
		" por		%%mm1,%%mm2;\n"
		" pxor		%%mm1,%%mm3;\n"
		" por		%%mm7,%%mm0;\n"
		" por		%%mm7,%%mm1;\n"
		" psrlq		$1,%%mm0;\n"
		" pand		%%mm7,%%mm2;\n"
		" psrlq		$1,%%mm1;\n"
		" paddb		%%mm0,%%mm1;\n"
		" movq		temp2h-16(%1),%%mm0;\n"
		" por		%%mm3,%%mm5;\n"
		" paddb		%%mm2,%%mm1;\n"
		" movq		%%mm0,%%mm2;\n"
		" pxor		%%mm1,%%mm2;\n"
		" pandn		%%mm2,%%mm5;\n"
		" movq		%%mm0,%%mm2;\n"
		" pand		%%mm1,%%mm2;\n"
		" pxor		%%mm5,%%mm2;\n"
		" por		%%mm7,%%mm0;\n"
		" por		%%mm7,%%mm1;\n"
		" psrlq		$1,%%mm0;\n"
		" pand		%%mm7,%%mm2;\n"
		" psrlq		$1,%%mm1;\n"
		" paddb		%%mm0,%%mm1;\n"
		" paddb		%%mm2,%%mm1;\n"
		" movq		%%mm1,temp22-16(%1);\n"

		" movq		%%mm4,%%mm0;\n"
		" movq		%%mm4,%%mm1;\n"
		" psrlq		$8,%%mm1;\n"
		" pushl		%%edx;\n"
		" movzxb	16(%0),%%edx;\n"
		" movd		%%edx,%%mm2;\n"
		" popl		%%edx;\n"
		" psllq		$56,%%mm2;\n"
		" por		%%mm2,%%mm1;\n"
		" pxor		%%mm1,%%mm4;\n"
		" pavgusb	%%mm0,%%mm1;\n"
		" movq		temp2h+8-16(%1),%%mm0;\n"
		" movq		%%mm0,%%mm2;\n"
		" pxor		%%mm1,%%mm2;\n"
		" por		%%mm4,%%mm6;\n"
		" pandn		%%mm2,%%mm6;\n"
		" movq		%%mm0,%%mm2;\n"
		" pand		%%mm1,%%mm2;\n"
		" pxor		%%mm6,%%mm2;\n"
		" por		%%mm7,%%mm0;\n"
		" por		%%mm7,%%mm1;\n"
		" psrlq		$1,%%mm0;\n"
		" pand		%%mm7,%%mm2;\n"
		" psrlq		$1,%%mm1;\n"
		" paddb		%%mm0,%%mm1;\n"
		" paddb		%%mm2,%%mm1;\n"
		" movq		%%mm1,temp22+8-16(%1);\n"

		/* temp2v 16 */

		" movq		temp11+18*16+0,%%mm0;\n"
		" movq		%%mm0,%%mm3;\n"
		" movq		temp11+18*16+1,%%mm1;\n"
		" pavgusb	%%mm0,%%mm1;\n"
		" movq		%%mm1,temp2v+18*16+0;\n"
		" movq		temp11+18*16+8,%%mm0;\n"
		" movq		%%mm0,%%mm4;\n"
		" movq		%%mm0,%%mm1;\n"
		" psrlq		$8,%%mm1;\n"

		" pushl		%%ecx;\n"
		" pushl		%%edx;\n"
		" movzxb	temp11+18*16+16,%%ecx;\n"
		" movd		%%ecx,%%mm2;\n"
		" movzxb	16(%0),%%edx;\n"
		" addl		%%edx,%%ecx;\n"
		" incl		%%ecx;\n"
		" shrl		$1,%%ecx;\n"
		" movb		%%cl,temp2v+18*16+16;\n"
		" popl		%%edx;\n"
		" popl		%%ecx;\n"

		" psllq		$56,%%mm2;\n"
		" por		%%mm2,%%mm1;\n"
		" pavgusb	%%mm0,%%mm1;\n"
		" movq		%%mm1,temp2v+18*16+8;\n"

		/* temp2h 16 */

		" movq		temp2h+18*16+0,%%mm0;\n"
		" movq		%%mm0,%%mm5;\n"
		" pavgusb	%%mm3,%%mm0;\n"
		" movq		%%mm0,temp2h+18*16+0;\n"
		" pxor		%%mm3,%%mm5;\n"
		" movq		temp2h+18*16+8,%%mm1;\n"
		" movq		%%mm1,%%mm6;\n"
		" pavgusb	%%mm4,%%mm1;\n"
		" pxor		%%mm4,%%mm6;\n"
		" movq		%%mm1,temp2h+18*16+8;\n"

		" pushl		%%ecx;\n"
		" pushl		%%edx;\n"
		" movzxb	temp11+18*16+16,%%ecx;\n"
		" movzxb	temp2h+18*16+16,%%edx;\n"
		" addl		%%edx,%%ecx;\n"
		" movd		%%ecx,%%mm4;\n"
		" incl		%%ecx;\n"
		" movl		%%ecx,%%edx;\n"
		" shrl		$1,%%ecx;\n"
		" movb		%%cl,temp2h+18*16+16;\n"
		" movzxb	16(%0),%%ecx;\n"
		" addl		%%ecx,%%edx;\n"
		" movzxb	17(%0),%%ecx;\n"
		" addl		%%ecx,%%edx;\n"
		" incl		%%edx;\n"
		" shrl		$2,%%edx;\n"
		" movb		%%dl,temp22+18*16+16;\n"
		" popl		%%edx;\n"
		" popl		%%ecx;\n"

		/* temp22 16 [1 & ((ab & cd) ^ ((ab ^ cd) & ~((a ^ b) | (c ^ d))))] */

		" movq		%%mm5,%%mm2;\n"
		" movq		%%mm6,%%mm3;\n"
		" psrlq		$8,%%mm2;\n"
		" psllq		$56,%%mm3;\n"
		" por		%%mm2,%%mm5;\n"
		" por		%%mm3,%%mm5;\n"
		" movq		temp2h+18*16+1,%%mm3;\n"
		" movq		%%mm0,%%mm2;\n"
		" pxor		%%mm3,%%mm2;\n"
		" pandn		%%mm2,%%mm5;\n"
		" movq		%%mm0,%%mm2;\n"
		" pand		%%mm3,%%mm2;\n"
		" pxor		%%mm2,%%mm5;\n"
		" pand		%%mm7,%%mm5;\n"
		" por		%%mm7,%%mm3;\n"
		" por		%%mm7,%%mm0;\n"
		" psrlq		$1,%%mm3;\n"
		" psrlq		$1,%%mm0;\n"
		" paddb		%%mm3,%%mm0;\n"
		" paddb		%%mm5,%%mm0;\n"
		" movq		%%mm0,temp22+18*16+0;\n"

		" movq		%%mm6,%%mm2;\n"
		" psrlq		$8,%%mm2;\n"
		" psllq		$56,%%mm4;\n"
		" por		%%mm2,%%mm6;\n"
		" por		%%mm4,%%mm6;\n"
		" movq		temp2h+18*16+9,%%mm4;\n"
		" movq		%%mm1,%%mm2;\n"
		" pxor		%%mm4,%%mm2;\n"
		" pandn		%%mm2,%%mm6;\n"
		" movq		%%mm1,%%mm2;\n"
		" pand		%%mm4,%%mm2;\n"
		" pxor		%%mm2,%%mm6;\n"
		" pand		%%mm7,%%mm6;\n"
		" por		%%mm7,%%mm4;\n"
		" por		%%mm7,%%mm1;\n"
		" psrlq		$1,%%mm4;\n"
		" psrlq		$1,%%mm1;\n"
		" paddb		%%mm4,%%mm1;\n"
		" paddb		%%mm6,%%mm1;\n"
		" movq		%%mm1,temp22+18*16+8;\n"

	:: "S" (p1 + y * pitch), "c" (y * 16), "r" (y) : "memory");
}

static unsigned int
mmx_sad2h(unsigned char ref[16][16], typeof(temp22) temp, int hy, int *r2)
{
	unsigned char *s = &temp[hy][0];
	unsigned char *t = &ref[0][0];
	unsigned char *l = &temp[18][hy];
	int y;

	asm volatile (
		" movq		(%0),%%mm0;\n"
		" movq		(%1),%%mm1;\n"
		" pxor		%%mm5,%%mm5;\n"
		" pxor		%%mm6,%%mm6;\n"
		" pxor		%%mm7,%%mm7;\n"
	:: "r" (s), "r" (t), "r" (l));

	for (y = 0; y < 15; y++) {
	asm volatile (
		" movq		1(%0),%%mm3;\n"
		" movq		%%mm0,%%mm2;\n"
		" movq		%%mm1,%%mm4;\n"
		" psubusb	%%mm1,%%mm0;\n"
		" psubusb	%%mm2,%%mm1;\n"
		" por		%%mm1,%%mm0;\n"
		" movq		%%mm0,%%mm1;\n"
		" movq		%%mm3,%%mm2;\n"
		" punpcklbw	%%mm5,%%mm0;\n"
		" paddw		%%mm0,%%mm6;\n"
		" movq		8(%0),%%mm0;\n"
		" punpckhbw	%%mm5,%%mm1;\n"
		" paddw		%%mm1,%%mm6;\n"

		" movq		8(%1),%%mm1;\n"
		" psubusb	%%mm4,%%mm3;\n"
		" psubusb	%%mm2,%%mm4;\n"
		" movq		(%2),%%mm2;\n"
		" por		%%mm4,%%mm3;\n"
		" movq		%%mm3,%%mm4;\n"
		" punpcklbw	%%mm5,%%mm3;\n"
		" paddw		%%mm3,%%mm7;\n"
		" movq		%%mm0,%%mm3;\n"
		" punpckhbw	%%mm5,%%mm4;\n"
		" paddw		%%mm4,%%mm7;\n"

		" movq		%%mm1,%%mm4;\n"
		" psubusb	%%mm1,%%mm0;\n"
		" psubusb	%%mm3,%%mm1;\n"
		" psrlq		$8,%%mm3;\n"
		" por		%%mm1,%%mm0;\n"
		" movq		%%mm0,%%mm1;\n"
		" punpcklbw	%%mm5,%%mm0;\n"
		" paddw		%%mm0,%%mm6;\n"
		" movq		16(%0),%%mm0;\n"
		" punpckhbw	%%mm5,%%mm1;\n"
		" paddw		%%mm1,%%mm6;\n"

		" movq		16(%1),%%mm1;\n"
		" psllq		$56,%%mm2;\n"
		" por		%%mm2,%%mm3;\n"
		" movq		%%mm3,%%mm2;\n"
		" psubusb	%%mm4,%%mm3;\n"
		" psubusb	%%mm2,%%mm4;\n"
		" por		%%mm4,%%mm3;\n"
		" movq		%%mm3,%%mm4;\n"
		" punpcklbw	%%mm5,%%mm3;\n"
		" paddw		%%mm3,%%mm7;\n"
		" punpckhbw	%%mm5,%%mm4;\n"
		" paddw		%%mm4,%%mm7;\n"

		:: "r" (s), "r" (t), "r" (l));

		s += 16;
		t += 16;
		l += 1;
	}

	asm volatile (
		" movq		1(%0),%%mm3;\n"
		" movq		%%mm0,%%mm2;\n"
		" movq		%%mm1,%%mm4;\n"
		" psubusb	%%mm1,%%mm0;\n"
		" psubusb	%%mm2,%%mm1;\n"
		" por		%%mm1,%%mm0;\n"
		" movq		%%mm0,%%mm1;\n"
		" punpcklbw	%%mm5,%%mm0;\n"
		" paddw		%%mm0,%%mm6;\n"
		" movq		8(%0),%%mm0;\n"
		" punpckhbw	%%mm5,%%mm1;\n"
		" paddw		%%mm1,%%mm6;\n"
		" movq		8(%1),%%mm1;\n"

		" movq		%%mm3,%%mm2;\n"
		" psubusb	%%mm4,%%mm3;\n"
		" psubusb	%%mm2,%%mm4;\n"
		" movq		(%2),%%mm2;\n"
		" por		%%mm4,%%mm3;\n"
		" movq		%%mm3,%%mm4;\n"
		" punpcklbw	%%mm5,%%mm3;\n"
		" paddw		%%mm3,%%mm7;\n"
		" movq		%%mm0,%%mm3;\n"
		" punpckhbw	%%mm5,%%mm4;\n"
		" paddw		%%mm4,%%mm7;\n"

		" movq		%%mm1,%%mm4;\n"
		" psubusb	%%mm1,%%mm0;\n"
		" psubusb	%%mm3,%%mm1;\n"
		" por		%%mm1,%%mm0;\n"
		" movq		%%mm0,%%mm1;\n"
		" punpcklbw	%%mm5,%%mm0;\n"
		" paddw		%%mm0,%%mm6;\n"
		" punpckhbw	%%mm5,%%mm1;\n"
		" paddw		%%mm1,%%mm6;\n"

		" psrlq		$8,%%mm3;\n"
		" psllq		$56,%%mm2;\n"
		" por		%%mm2,%%mm3;\n"
		" movq		%%mm3,%%mm2;\n"
		" psubusb	%%mm4,%%mm3;\n"
		" psubusb	%%mm2,%%mm4;\n"
		" por		%%mm4,%%mm3;\n"
		" movq		%%mm3,%%mm4;\n"
		" punpcklbw	%%mm5,%%mm3;\n"
		" paddw		%%mm3,%%mm7;\n"
		" punpckhbw	%%mm5,%%mm4;\n"
		" paddw		%%mm4,%%mm7;\n"

	:: "r" (s), "r" (t), "r" (l));

	// XXX this is supposed to sort
	asm volatile (
		" movq		%%mm7,%%mm0;\n"
		" punpcklwd	%%mm6,%%mm0;\n"
		" punpckhwd	%%mm6,%%mm7;\n"
		" paddw		%%mm0,%%mm7;\n"

		" movq		%%mm7,%%mm0;\n"
		" psrlq		$32,%%mm0;\n"
		" paddw		%%mm0,%%mm7;\n"
		" movd		%%mm7,%0;\n"
		" punpcklwd	%%mm5,%%mm7;\n"
		" movd		%%mm7,(%1);\n"
		" shrl		$16,%0;\n"
	: "=&a" (y) : "r" (r2));

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
	asm volatile (
		" movq		(%0),%%mm3;\n"
		" movq		8(%0),%%mm4;\n"
		" movq		(%1),%%mm1;\n"
		" movq		16(%0),%%mm0;\n"
		" pxor		%%mm5,%%mm5;\n"
		" pxor		%%mm6,%%mm6;\n"
		" pxor		%%mm7,%%mm7;\n"
	:: "r" (s), "r" (t), "r" (l));

		for (y = 0; y < 15; y++) {
	asm volatile (
		" movq		%%mm1,%%mm2;\n"
		" psubusb	%%mm3,%%mm1;\n"
		" psubusb	%%mm2,%%mm3;\n"
		" por		%%mm1,%%mm3;\n"
		" movq		%%mm3,%%mm1;\n"
		" punpckhbw	%%mm5,%%mm1;\n"
		" paddw		%%mm1,%%mm6;\n"
		" movq		8(%1),%%mm1;\n"
		" punpcklbw	%%mm5,%%mm3;\n"
		" paddw		%%mm3,%%mm6;\n"

		" movq		%%mm0,%%mm3;\n"
		" psubusb	%%mm2,%%mm0;\n"
		" psubusb	%%mm3,%%mm2;\n"
		" por		%%mm2,%%mm0;\n"
		" movq		%%mm0,%%mm2;\n"
		" punpcklbw	%%mm5,%%mm0;\n"
		" paddw		%%mm0,%%mm7;\n"
		" movq		24(%0),%%mm0;\n"
		" punpckhbw	%%mm5,%%mm2;\n"
		" paddw		%%mm2,%%mm7;\n"

		" movq		%%mm1,%%mm2;\n"
		" psubusb	%%mm4,%%mm1;\n"
		" psubusb	%%mm2,%%mm4;\n"
		" por		%%mm1,%%mm4;\n"
		" movq		%%mm4,%%mm1;\n"
		" punpckhbw	%%mm5,%%mm1;\n"
		" paddw		%%mm1,%%mm6;\n"
		" movq		16(%1),%%mm1;\n"
		" punpcklbw	%%mm5,%%mm4;\n"
		" paddw		%%mm4,%%mm6;\n"

		" movq		%%mm0,%%mm4;\n"
		" psubusb	%%mm2,%%mm0;\n"
		" psubusb	%%mm4,%%mm2;\n"
		" por		%%mm2,%%mm0;\n"
		" movq		%%mm0,%%mm2;\n"
		" punpcklbw	%%mm5,%%mm0;\n"
		" paddw		%%mm0,%%mm7;\n"
		" movq		32(%0),%%mm0;\n"
		" punpckhbw	%%mm5,%%mm2;\n"
		" paddw		%%mm2,%%mm7;\n"
	:: "r" (s), "r" (t), "r" (l));

			s += 16;
			t += 16;
		}

	asm volatile (
		" movq		%%mm1,%%mm2;\n"
		" psubusb	%%mm3,%%mm1;\n"
		" psubusb	%%mm2,%%mm3;\n"
		" por		%%mm1,%%mm3;\n"
		" movq		%%mm3,%%mm1;\n"
		" punpckhbw	%%mm5,%%mm1;\n"
		" paddw		%%mm1,%%mm6;\n"
		" movq		8(%1),%%mm1;\n"
		" punpcklbw	%%mm5,%%mm3;\n"
		" paddw		%%mm3,%%mm6;\n"

		" movq		%%mm0,%%mm3;\n"
		" psubusb	%%mm2,%%mm0;\n"
		" psubusb	%%mm3,%%mm2;\n"
		" por		%%mm2,%%mm0;\n"
		" movq		%%mm0,%%mm2;\n"
		" punpcklbw	%%mm5,%%mm0;\n"
		" paddw		%%mm0,%%mm7;\n"
		" movq		24(%0),%%mm0;\n"

	:: "r" (s), "r" (t), "r" (l));

	} else {

	asm volatile (
		" movq		8(%0),%%mm4;\n"
		" psrlq		$8,%%mm4;\n"
		" movq		(%2),%%mm2;\n"
		" psllq		$56,%%mm2;\n"
		" por		%%mm2,%%mm4;\n"

		" movq		1(%0),%%mm3;\n"
		" movq		(%1),%%mm1;\n"
		" movq		17(%0),%%mm0;\n"

		" pxor		%%mm5,%%mm5;\n"
		" pxor		%%mm6,%%mm6;\n"
		" pxor		%%mm7,%%mm7;\n"
	:: "r" (s), "r" (t), "r" (l));

		for (y = 0; y < 15; y++) {
	asm volatile (
		" movq		%%mm1,%%mm2;\n"
		" psubusb	%%mm3,%%mm1;\n"
		" psubusb	%%mm2,%%mm3;\n"
		" por		%%mm1,%%mm3;\n"
		" movq		%%mm3,%%mm1;\n"
		" punpckhbw	%%mm5,%%mm1;\n"
		" paddw		%%mm1,%%mm6;\n"
		" movq		8(%1),%%mm1;\n"
		" punpcklbw	%%mm5,%%mm3;\n"
		" paddw		%%mm3,%%mm6;\n"

		" movq		%%mm0,%%mm3;\n"
		" psubusb	%%mm2,%%mm0;\n"
		" psubusb	%%mm3,%%mm2;\n"
		" por		%%mm2,%%mm0;\n"
		" movq		%%mm0,%%mm2;\n"
		" punpcklbw	%%mm5,%%mm0;\n"
		" paddw		%%mm0,%%mm7;\n"
		" movq		24(%0),%%mm0;\n"
		" punpckhbw	%%mm5,%%mm2;\n"
		" paddw		%%mm2,%%mm7;\n"

		" movq		%%mm1,%%mm2;\n"
		" psubusb	%%mm4,%%mm1;\n"
		" psubusb	%%mm2,%%mm4;\n"
		" psrlq		$8,%%mm0;\n"
		" por		%%mm1,%%mm4;\n"
		" movq		%%mm4,%%mm1;\n"
		" punpckhbw	%%mm5,%%mm1;\n"
		" paddw		%%mm1,%%mm6;\n"
		" movq		16(%1),%%mm1;\n"
		" punpcklbw	%%mm5,%%mm4;\n"
		" paddw		%%mm4,%%mm6;\n"

		" movq		1(%2),%%mm4;\n"
		" psllq		$56,%%mm4;\n"
		" por		%%mm4,%%mm0;\n"
		" movq		%%mm0,%%mm4;\n"
		" psubusb	%%mm2,%%mm0;\n"
		" psubusb	%%mm4,%%mm2;\n"
		" por		%%mm2,%%mm0;\n"
		" movq		%%mm0,%%mm2;\n"
		" punpcklbw	%%mm5,%%mm0;\n"
		" paddw		%%mm0,%%mm7;\n"
		" movq		33(%0),%%mm0;\n"
		" punpckhbw	%%mm5,%%mm2;\n"
		" paddw		%%mm2,%%mm7;\n"
	:: "r" (s), "r" (t), "r" (l));

			s += 16;
			t += 16;
			l += 1;
		}

	asm volatile (
		" movq		%%mm1,%%mm2;\n"
		" psubusb	%%mm3,%%mm1;\n"
		" psubusb	%%mm2,%%mm3;\n"
		" por		%%mm1,%%mm3;\n"
		" movq		%%mm3,%%mm1;\n"
		" punpckhbw	%%mm5,%%mm1;\n"
		" paddw		%%mm1,%%mm6;\n"
		" movq		8(%1),%%mm1;\n"
		" punpcklbw	%%mm5,%%mm3;\n"
		" paddw		%%mm3,%%mm6;\n"

		" movq		%%mm0,%%mm3;\n"
		" psubusb	%%mm2,%%mm0;\n"
		" psubusb	%%mm3,%%mm2;\n"
		" movq		1(%2),%%mm3;\n"
		" por		%%mm2,%%mm0;\n"
		" psllq		$56,%%mm3;\n"
		" movq		%%mm0,%%mm2;\n"
		" punpcklbw	%%mm5,%%mm0;\n"
		" paddw		%%mm0,%%mm7;\n"

		" movq		24(%0),%%mm0;\n"
		" psrlq		$8,%%mm0;\n"
		" por		%%mm3,%%mm0;\n"
	:: "r" (s), "r" (t), "r" (l));

	}

	asm volatile (
		" punpckhbw	%mm5,%mm2;\n"
		" paddw		%mm2,%mm7;\n"

		" movq		%mm1,%mm2;\n"
		" psubusb	%mm4,%mm1;\n"
		" psubusb	%mm2,%mm4;\n"
		" por		%mm1,%mm4;\n"
		" movq		%mm4,%mm1;\n"
		" punpcklbw	%mm5,%mm4;\n"
		" paddw		%mm4,%mm6;\n"
		" punpckhbw	%mm5,%mm1;\n"
		" paddw		%mm1,%mm6;\n"

		" movq		%mm0,%mm4;\n"
		" psubusb	%mm2,%mm0;\n"
		" psubusb	%mm4,%mm2;\n"
		" por		%mm2,%mm0;\n"
		" movq		%mm0,%mm2;\n"
		" punpcklbw	%mm5,%mm0;\n"
		" paddw		%mm0,%mm7;\n"
		" punpckhbw	%mm5,%mm2;\n"
		" paddw		%mm2,%mm7;\n"

	// XXX this is supposed to sort

		" movq		%mm7,%mm0;\n"
		" punpcklwd	%mm6,%mm0;\n"
		" punpckhwd	%mm6,%mm7;\n"
		" paddw		%mm0,%mm7;\n"
	);

	asm volatile (
		" movq		%%mm7,%%mm0;\n"
		" psrlq		$32,%%mm0;\n"
		" paddw		%%mm0,%%mm7;\n"
		" movd		%%mm7,%0;\n"
		" punpcklwd	%%mm5,%%mm7;\n"
		" movd		%%mm7,(%1);\n"
		" shrl		$16,%0;\n"
	: "=&a" (y) : "r" (r2));

	return y; // left, r2 = right
}

static unsigned int
sse_sad2h(unsigned char ref[16][16], typeof(temp22) temp, int hy, int *r2)
{
	unsigned char *s = &temp[hy][0];
	unsigned char *t = &ref[0][0];
	unsigned char *l = &temp[18][hy];
	int y;

	asm volatile (
		" pxor		%%mm6,%%mm6;\n"
		" pxor		%%mm7,%%mm7;\n"
	:: "r" (s), "r" (t), "r" (l));

	for (y = 0; y < 16; y++) {
	asm volatile (
		" movq		(%1),%%mm1;\n"
		" movq		(%0),%%mm0;\n"
		" psadbw	%%mm1,%%mm0;\n"
		" paddw		%%mm0,%%mm6;\n"
		" movq		1(%0),%%mm0;\n"
		" psadbw	%%mm1,%%mm0;\n"
		" paddw		%%mm0,%%mm7;\n"

		" movq		8(%1),%%mm1;\n"
		" movq		8(%0),%%mm0;\n"
		" movq		%%mm0,%%mm3;\n"
		" psadbw	%%mm1,%%mm0;\n"
		" paddw		%%mm0,%%mm6;\n"
		" movq		(%2),%%mm2;\n"
		" psrlq		$8,%%mm3;\n"
		" psllq		$56,%%mm2;\n"
		" por		%%mm2,%%mm3;\n"
		" psadbw	%%mm1,%%mm3;\n"
		" paddw		%%mm3,%%mm7;\n"

		:: "r" (s), "r" (t), "r" (l));

		s += 16;
		t += 16;
		l += 1;
	}

	// XXX this is supposed to sort
	asm volatile (
		" movd		%%mm6,%0;\n"
		" movd		%%mm7,(%1);\n"
	: "=&a" (y) : "r" (r2));

	return y; // left, r2 = right
}

static unsigned int
sse_sad2v(unsigned char ref[16][16], typeof(temp22) temp, int hx, int *r2)
{
	unsigned char *s = &temp[0][0];
	unsigned char *t = &ref[0][0];
	unsigned char *l = &temp[18][0];
	int y;

	if (hx == 0) {
	asm volatile (
		" movq		(%0),%%mm3;\n"
		" movq		8(%0),%%mm4;\n"
		" movq		(%1),%%mm1;\n"
		" movq		16(%0),%%mm0;\n"
		" pxor		%%mm5,%%mm5;\n"
		" pxor		%%mm6,%%mm6;\n"
		" pxor		%%mm7,%%mm7;\n"
	:: "r" (s), "r" (t), "r" (l));

		for (y = 0; y < 5; y++) {
	asm volatile (
		" movq		%%mm1,%%mm2;\n"
		" psadbw	%%mm3,%%mm1;\n"
		" paddw		%%mm1,%%mm6;\n"
		" movq		0*16+8(%1),%%mm1;\n"
		" movq		%%mm0,%%mm3;\n"
		" psadbw	%%mm2,%%mm0;\n"
		" paddw		%%mm0,%%mm7;\n"
		" movq		0*16+24(%0),%%mm0;\n"
		" movq		%%mm1,%%mm2;\n"
		" psadbw	%%mm4,%%mm1;\n"
		" paddw		%%mm1,%%mm6;\n"
		" movq		0*16+16(%1),%%mm1;\n"
		" movq		%%mm0,%%mm4;\n"
		" psadbw	%%mm2,%%mm0;\n"
		" paddw		%%mm0,%%mm7;\n"
		" movq		0*16+32(%0),%%mm0;\n"

		" movq		%%mm1,%%mm2;\n"
		" psadbw	%%mm3,%%mm1;\n"
		" paddw		%%mm1,%%mm6;\n"
		" movq		1*16+8(%1),%%mm1;\n"
		" movq		%%mm0,%%mm3;\n"
		" psadbw	%%mm2,%%mm0;\n"
		" paddw		%%mm0,%%mm7;\n"
		" movq		1*16+24(%0),%%mm0;\n"
		" movq		%%mm1,%%mm2;\n"
		" psadbw	%%mm4,%%mm1;\n"
		" paddw		%%mm1,%%mm6;\n"
		" movq		1*16+16(%1),%%mm1;\n"
		" movq		%%mm0,%%mm4;\n"
		" psadbw	%%mm2,%%mm0;\n"
		" paddw		%%mm0,%%mm7;\n"
		" movq		1*16+32(%0),%%mm0;\n"

		" movq		%%mm1,%%mm2;\n"
		" psadbw	%%mm3,%%mm1;\n"
		" paddw		%%mm1,%%mm6;\n"
		" movq		2*16+8(%1),%%mm1;\n"
		" movq		%%mm0,%%mm3;\n"
		" psadbw	%%mm2,%%mm0;\n"
		" paddw		%%mm0,%%mm7;\n"
		" movq		2*16+24(%0),%%mm0;\n"
		" movq		%%mm1,%%mm2;\n"
		" psadbw	%%mm4,%%mm1;\n"
		" paddw		%%mm1,%%mm6;\n"
		" movq		2*16+16(%1),%%mm1;\n"
		" movq		%%mm0,%%mm4;\n"
		" psadbw	%%mm2,%%mm0;\n"
		" paddw		%%mm0,%%mm7;\n"
		" movq		2*16+32(%0),%%mm0;\n"
	:: "r" (s), "r" (t), "r" (l));

			s += 16 * 3;
			t += 16 * 3;
		}

	asm volatile (
		" movq		%%mm1,%%mm2;\n"
		" psadbw	%%mm3,%%mm1;\n"
		" paddw		%%mm1,%%mm6;\n"
		" movq		8(%1),%%mm1;\n"

		" movq		%%mm0,%%mm3;\n"
		" psadbw	%%mm2,%%mm0;\n"
		" paddw		%%mm0,%%mm7;\n"
		" movq		24(%0),%%mm0;\n"
	:: "r" (s), "r" (t), "r" (l));

	} else {

	asm volatile (
		" movq		8(%0),%%mm4;\n"
		" psrlq		$8,%%mm4;\n"
		" movq		(%2),%%mm2;\n"
		" psllq		$56,%%mm2;\n"
		" por		%%mm2,%%mm4;\n"

		" movq		1(%0),%%mm3;\n"
		" movq		(%1),%%mm1;\n"
		" movq		17(%0),%%mm0;\n"

		" pxor		%%mm5,%%mm5;\n"
		" pxor		%%mm6,%%mm6;\n"
		" pxor		%%mm7,%%mm7;\n"
	:: "r" (s), "r" (t), "r" (l));

		for (y = 0; y < 5; y++) {
	asm volatile (
		" movq		%%mm1,%%mm2;\n"
		" psadbw	%%mm3,%%mm1;\n"
		" paddw		%%mm1,%%mm6;\n"
		" movq		0*16+8(%1),%%mm1;\n"
		" movq		%%mm0,%%mm3;\n"
		" psadbw	%%mm2,%%mm0;\n"
		" paddw		%%mm0,%%mm7;\n"
		" movq		0*16+24(%0),%%mm0;\n"
		" movq		%%mm1,%%mm2;\n"
		" psadbw	%%mm4,%%mm1;\n"
		" movq		1(%2),%%mm4;\n"
		" psrlq		$8,%%mm0;\n"
		" paddw		%%mm1,%%mm6;\n"
		" movq		0*16+16(%1),%%mm1;\n"
		" psllq		$56,%%mm4;\n"
		" por		%%mm4,%%mm0;\n"
		" movq		%%mm0,%%mm4;\n"
		" psadbw	%%mm2,%%mm0;\n"
		" paddw		%%mm0,%%mm7;\n"
		" movq		0*16+33(%0),%%mm0;\n"

		" movq		%%mm1,%%mm2;\n"
		" psadbw	%%mm3,%%mm1;\n"
		" paddw		%%mm1,%%mm6;\n"
		" movq		1*16+8(%1),%%mm1;\n"
		" movq		%%mm0,%%mm3;\n"
		" psadbw	%%mm2,%%mm0;\n"
		" paddw		%%mm0,%%mm7;\n"
		" movq		1*16+24(%0),%%mm0;\n"
		" movq		%%mm1,%%mm2;\n"
		" psadbw	%%mm4,%%mm1;\n"
		" movq		1+1(%2),%%mm4;\n"
		" psrlq		$8,%%mm0;\n"
		" paddw		%%mm1,%%mm6;\n"
		" movq		1*16+16(%1),%%mm1;\n"
		" psllq		$56,%%mm4;\n"
		" por		%%mm4,%%mm0;\n"
		" movq		%%mm0,%%mm4;\n"
		" psadbw	%%mm2,%%mm0;\n"
		" paddw		%%mm0,%%mm7;\n"
		" movq		1*16+33(%0),%%mm0;\n"

		" movq		%%mm1,%%mm2;\n"
		" psadbw	%%mm3,%%mm1;\n"
		" paddw		%%mm1,%%mm6;\n"
		" movq		2*16+8(%1),%%mm1;\n"
		" movq		%%mm0,%%mm3;\n"
		" psadbw	%%mm2,%%mm0;\n"
		" paddw		%%mm0,%%mm7;\n"
		" movq		2*16+24(%0),%%mm0;\n"
		" movq		%%mm1,%%mm2;\n"
		" psadbw	%%mm4,%%mm1;\n"
		" movq		2+1(%2),%%mm4;\n"
		" psrlq		$8,%%mm0;\n"
		" paddw		%%mm1,%%mm6;\n"
		" movq		2*16+16(%1),%%mm1;\n"
		" psllq		$56,%%mm4;\n"
		" por		%%mm4,%%mm0;\n"
		" movq		%%mm0,%%mm4;\n"
		" psadbw	%%mm2,%%mm0;\n"
		" paddw		%%mm0,%%mm7;\n"
		" movq		2*16+33(%0),%%mm0;\n"
	:: "r" (s), "r" (t), "r" (l));
			s += 16 * 3;
			t += 16 * 3;
			l += 1 * 3;
		}

	asm volatile (
		" movq		%%mm1,%%mm2;\n"
		" psadbw	%%mm3,%%mm1;\n"
		" paddw		%%mm1,%%mm6;\n"
		" movq		8(%1),%%mm1;\n"

		" movq		%%mm0,%%mm3;\n"
		" movq		1(%2),%%mm3;\n"
		" psadbw	%%mm2,%%mm0;\n"
		" psllq		$56,%%mm3;\n"
		" paddw		%%mm0,%%mm7;\n"
		" movq		24(%0),%%mm0;\n"
		" psrlq		$8,%%mm0;\n"
		" por		%%mm3,%%mm0;\n"
	:: "r" (s), "r" (t), "r" (l));

	}

	// XXX this is supposed to sort

	asm volatile (
		" movq		%%mm1,%%mm2;\n"
		" psadbw	%%mm4,%%mm1;\n"
		" paddw		%%mm1,%%mm6;\n"
		" psadbw	%%mm2,%%mm0;\n"
		" paddw		%%mm0,%%mm7;\n"

		" movd		%%mm6,%0;\n"
		" movd		%%mm7,(%1);\n"
	: "=&a" (y) : "r" (r2));

	return y; // left, r2 = right
}

static inline void
mmx_load_ref(unsigned char t[16][16])
{
	asm volatile (
		" movq		0*16+0(%0),%%mm0;\n"
		" packuswb	0*16+8(%0),%%mm0;\n"
		" movq		1*16+0(%0),%%mm2;\n"
		" packuswb	1*16+8(%0),%%mm2;\n"
		" movq		256+0*16+0(%0),%%mm1;\n"
		" packuswb	256+0*16+8(%0),%%mm1;\n"
		" movq		%%mm0,0*16+0(%1);\n"
		" movq		256+1*16+0(%0),%%mm3;\n"
		" packuswb	256+1*16+8(%0),%%mm3;\n"
		" movq		%%mm1,0*16+8(%1);\n"
		" movq		2*16+0(%0),%%mm4;\n"
		" packuswb	2*16+8(%0),%%mm4;\n"
		" movq		%%mm2,1*16+0(%1);\n"
		" movq		3*16+0(%0),%%mm6;\n"
		" packuswb	3*16+8(%0),%%mm6;\n"
		" movq		%%mm3,1*16+8(%1);\n"
		" movq		256+2*16+0(%0),%%mm5;\n"
		" packuswb	256+2*16+8(%0),%%mm5;\n"
		" movq		%%mm4,2*16+0(%1);\n"
		" movq		256+3*16+0(%0),%%mm7;\n"
		" packuswb	256+3*16+8(%0),%%mm7;\n"
		" movq		%%mm5,2*16+8(%1);\n"
		" movq		4*16+0(%0),%%mm0;\n"
		" packuswb	4*16+8(%0),%%mm0;\n"
		" movq		%%mm6,3*16+0(%1);\n"
		" movq		5*16+0(%0),%%mm2;\n"
		" packuswb	5*16+8(%0),%%mm2;\n"
		" movq		%%mm7,3*16+8(%1);\n"
		" movq		256+4*16+0(%0),%%mm1;\n"
		" packuswb	256+4*16+8(%0),%%mm1;\n"
		" movq		%%mm0,4*16+0(%1);\n"
		" movq		256+5*16+0(%0),%%mm3;\n"
		" packuswb	256+5*16+8(%0),%%mm3;\n"
		" movq		%%mm1,4*16+8(%1);\n"
		" movq		6*16+0(%0),%%mm4;\n"
		" packuswb	6*16+8(%0),%%mm4;\n"
		" movq		%%mm2,5*16+0(%1);\n"
		" movq		7*16+0(%0),%%mm6;\n"
		" packuswb	7*16+8(%0),%%mm6;\n"
		" movq		%%mm3,5*16+8(%1);\n"
		" movq		256+6*16+0(%0),%%mm5;\n"
		" packuswb	256+6*16+8(%0),%%mm5;\n"
		" movq		%%mm4,6*16+0(%1);\n"
		" movq		256+7*16+0(%0),%%mm7;\n"
		" packuswb	256+7*16+8(%0),%%mm7;\n"
		" movq		%%mm5,6*16+8(%1);\n"
		" movq		8*16+0(%0),%%mm0;\n"
		" packuswb	8*16+8(%0),%%mm0;\n"
		" movq		%%mm6,7*16+0(%1);\n"
		" movq		9*16+0(%0),%%mm2;\n"
		" packuswb	9*16+8(%0),%%mm2;\n"
		" movq		%%mm7,7*16+8(%1);\n"
		" movq		256+8*16+0(%0),%%mm1;\n"
		" packuswb	256+8*16+8(%0),%%mm1;\n"
		" movq		%%mm0,8*16+0(%1);\n"
		" movq		256+9*16+0(%0),%%mm3;\n"
		" packuswb	256+9*16+8(%0),%%mm3;\n"
		" movq		%%mm1,8*16+8(%1);\n"
		" movq		10*16+0(%0),%%mm4;\n"
		" packuswb	10*16+8(%0),%%mm4;\n"
		" movq		%%mm2,9*16+0(%1);\n"
		" movq		11*16+0(%0),%%mm6;\n"
		" packuswb	11*16+8(%0),%%mm6;\n"
		" movq		%%mm3,9*16+8(%1);\n"
		" movq		256+10*16+0(%0),%%mm5;\n"
		" packuswb	256+10*16+8(%0),%%mm5;\n"
		" movq		%%mm4,10*16+0(%1);\n"
		" movq		256+11*16+0(%0),%%mm7;\n"
		" packuswb	256+11*16+8(%0),%%mm7;\n"
		" movq		%%mm5,10*16+8(%1);\n"
		" movq		12*16+0(%0),%%mm0;\n"
		" packuswb	12*16+8(%0),%%mm0;\n"
		" movq		%%mm6,11*16+0(%1);\n"
		" movq		13*16+0(%0),%%mm2;\n"
		" packuswb	13*16+8(%0),%%mm2;\n"
		" movq		%%mm7,11*16+8(%1);\n"
		" movq		256+12*16+0(%0),%%mm1;\n"
		" packuswb	256+12*16+8(%0),%%mm1;\n"
		" movq		%%mm0,12*16+0(%1);\n"
		" movq		256+13*16+0(%0),%%mm3;\n"
		" packuswb	256+13*16+8(%0),%%mm3;\n"
		" movq		%%mm1,12*16+8(%1);\n"
		" movq		14*16+0(%0),%%mm4;\n"
		" packuswb	14*16+8(%0),%%mm4;\n"
		" movq		%%mm2,13*16+0(%1);\n"
		" movq		15*16+0(%0),%%mm6;\n"
		" packuswb	15*16+8(%0),%%mm6;\n"
		" movq		%%mm3,13*16+8(%1);\n"
		" movq		256+14*16+0(%0),%%mm5;\n"
		" packuswb	256+14*16+8(%0),%%mm5;\n"
		" movq		%%mm4,14*16+0(%1);\n"
		" movq		256+15*16+0(%0),%%mm7;\n"
		" packuswb	256+15*16+8(%0),%%mm7;\n"
		" movq		%%mm5,14*16+8(%1);\n"
		" movq		%%mm6,15*16+0(%1);\n"
		" movq		%%mm7,15*16+8(%1);\n"

	:: "S" (&mblock[0][0][0][0]), "D" (&t[0][0]) : "memory");
}

static inline void
mmx_psse_4(char t[16][16], char *p, int pitch)
{
	asm volatile (
		" movq		(%0),%%mm0;\n"
		" movq		(%1),%%mm1;\n"
		" movq		8(%0),%%mm4;\n"

		" movq		%%mm0,%%mm2;\n"
		" movq		%%mm1,%%mm3;\n"
		" pcmpgtb	%%mm1,%%mm2;		// 1 < 0\n"
		" psubb		%%mm0,%%mm1;		// 1 = 1 - 0\n"
		" movq		%%mm1,%%mm6;\n"
		" punpcklbw	%%mm2,%%mm6;\n"
		" pmullw	%%mm6,%%mm6;\n"
		" punpckhbw	%%mm2,%%mm1;\n"
		" pmullw	%%mm1,%%mm1;\n"
		" paddusw	%%mm1,%%mm6;\n"

		" movq		8(%1),%%mm1;\n"

		" psrlq		$32,%%mm0;\n"
		" movq		%%mm4,%%mm2;\n"
		" psllq		$32,%%mm2;\n"
		" por		%%mm2,%%mm0;\n"

		" movq		(%0,%2),%%mm5;\n"

		" movq		%%mm4,%%mm2;\n"
		" movq		%%mm1,%%mm3;\n"
		" pcmpgtb		%%mm1,%%mm2;\n"
		" psubb		%%mm4,%%mm1;\n"
		" movq		%%mm1,%%mm0;\n"
		" punpcklbw	%%mm2,%%mm0;\n"
		" pmullw		%%mm0,%%mm0;\n"
		" paddusw		%%mm0,%%mm6;\n"
		" movd		16(%0),%%mm0;\n"
		" punpckhbw	%%mm2,%%mm1;\n"
		" pmullw		%%mm1,%%mm1;\n"
		" paddusw		%%mm1,%%mm6;\n"

		" movq		16(%1),%%mm1;\n"

		" psrlq		$32,%%mm4;\n"
		" psllq		$32,%%mm0;\n"
		" por		%%mm4,%%mm0;\n"

		" movq		8(%0,%2),%%mm0;\n"

		" movq		%%mm5,%%mm2;\n"
		" movq		%%mm1,%%mm3;\n"
		" pcmpgtb		%%mm1,%%mm2;\n"
		" psubb		%%mm5,%%mm1;\n"
		" movq		%%mm1,%%mm4;\n"
		" punpcklbw	%%mm2,%%mm4;\n"
		" pmullw		%%mm4,%%mm4;\n"
		" paddusw		%%mm4,%%mm6;\n"
		" movq		24(%1),%%mm4;\n"
		" punpckhbw	%%mm2,%%mm1;\n"
		" pmullw		%%mm1,%%mm1;\n"
		" paddusw		%%mm1,%%mm6;\n"

		" movd		16(%0,%2),%%mm5;\n"

		" movq		%%mm0,%%mm2;\n"
		" movq		%%mm4,%%mm3;\n"
		" pcmpgtb		%%mm4,%%mm2;\n"
		" psubb		%%mm0,%%mm4;\n"
		" movq		%%mm4,%%mm1;\n"
		" punpcklbw	%%mm2,%%mm1;\n"
		" pmullw		%%mm1,%%mm1;\n"
		" paddusw		%%mm1,%%mm6;\n"
		" movq		bbmin,%%mm1;\n"
		" punpckhbw	%%mm2,%%mm4;\n"
		" movq		bbdxy,%%mm2;\n"
		" pmullw		%%mm4,%%mm4;\n"

		" psrlq		$32,%%mm0;\n"
		" paddusw		%%mm4,%%mm6;\n"
		" movq		crdxy,%%mm4;\n"
		" psllq		$32,%%mm5;\n"
		" psubw		c1_15w,%%mm6;\n"
		" por		%%mm5,%%mm0;\n"
		" paddb		c4,%%mm4;\n"

		" movq		%%mm0,%%mm5;\n"
		" pcmpgtb		%%mm3,%%mm5;\n"
		" psubb		%%mm0,%%mm3;\n"
		" movq		%%mm3,%%mm0;\n"
		" punpcklbw	%%mm5,%%mm0;\n"
		" pmullw		%%mm0,%%mm0;\n"
		" paddusw		%%mm0,%%mm7;\n"
		" punpckhbw	%%mm5,%%mm3;\n"
		" pmullw		%%mm3,%%mm3;\n"
		" paddusw		%%mm3,%%mm7;\n"

		" movq		%%mm4,crdxy;\n"

		" movq		%%mm4,%%mm5;\n"
		" pxor		%%mm3,%%mm3;\n"
		" pcmpgtb		%%mm4,%%mm3;\n"
		" pxor		%%mm3,%%mm5;\n"
		" psubb		%%mm3,%%mm5;\n"

		" movq		%%mm5,%%mm3;\n"
		" psrlw		$8,%%mm5;\n"
		" paddsw		%%mm5,%%mm6;\n"
		" pand		c255,%%mm3;\n"
		" paddsw		%%mm3,%%mm6;\n"

		" movq		%%mm1,%%mm5;\n"
		" pcmpgtw		%%mm6,%%mm5;\n"
		" movq		%%mm1,%%mm3;\n"
		" pxor		%%mm6,%%mm3;\n"
		" pand		%%mm5,%%mm3;\n"
		" pxor		%%mm3,%%mm6;\n"
		" pxor		%%mm3,%%mm1;\n"
		" pxor		%%mm4,%%mm2;\n"
		" pand		%%mm2,%%mm5;\n"
		" pxor		%%mm5,%%mm4;\n"
		" pxor		%%mm4,%%mm2;\n"

		" movq		%%mm6,%%mm5;\n"
		" psllq		$16,%%mm6;\n"
		" psrlq		$48,%%mm5;\n"
		" por		%%mm5,%%mm6;\n"
		" movq		%%mm4,%%mm5;\n"
		" psllq		$16,%%mm4;\n"
		" psrlq		$48,%%mm5;\n"
		" por		%%mm5,%%mm4;\n"

		" movq		%%mm1,%%mm5;\n"
		" pcmpgtw		%%mm6,%%mm5;\n"
		" movq		%%mm1,%%mm3;\n"
		" pxor		%%mm6,%%mm3;\n"
		" pand		%%mm5,%%mm3;\n"
		" pxor		%%mm3,%%mm6;\n"
		" pxor		%%mm3,%%mm1;\n"
		" pxor		%%mm4,%%mm2;\n"
		" pand		%%mm2,%%mm5;\n"
		" pxor		%%mm5,%%mm4;\n"
		" pxor		%%mm4,%%mm2;\n"

		" movq		%%mm6,%%mm5;\n"
		" psllq		$16,%%mm6;\n"
		" psrlq		$48,%%mm5;\n"
		" por		%%mm5,%%mm6;\n"
		" movq		%%mm4,%%mm5;\n"
		" psllq		$16,%%mm4;\n"
		" psrlq		$48,%%mm5;\n"
		" por		%%mm5,%%mm4;\n"

		" movq		%%mm1,%%mm5;\n"
		" pcmpgtw		%%mm6,%%mm5;\n"
		" movq		%%mm1,%%mm3;\n"
		" pxor		%%mm6,%%mm3;\n"
		" pand		%%mm5,%%mm3;\n"
		" pxor		%%mm3,%%mm6;\n"
		" pxor		%%mm3,%%mm1;\n"
		" pxor		%%mm4,%%mm2;\n"
		" pand		%%mm2,%%mm5;\n"
		" pxor		%%mm5,%%mm4;\n"
		" pxor		%%mm4,%%mm2;\n"

		" movq		%%mm6,%%mm5;\n"
		" psllq		$16,%%mm6;\n"
		" psrlq		$48,%%mm5;\n"
		" por		%%mm5,%%mm6;\n"
		" movq		%%mm4,%%mm5;\n"
		" psllq		$16,%%mm4;\n"
		" psrlq		$48,%%mm5;\n"
		" por		%%mm5,%%mm4;\n"

		" movq		%%mm1,%%mm5;\n"
		" pcmpgtw		%%mm6,%%mm5;\n"
		" movq		%%mm1,%%mm3;\n"
		" pxor		%%mm6,%%mm3;\n"
		" pand		%%mm5,%%mm3;\n"
		" pxor		%%mm3,%%mm6;\n"
		" pxor		%%mm3,%%mm1;\n"
		" pxor		%%mm4,%%mm2;\n"
		" pand		%%mm2,%%mm5;\n"
		" pxor		%%mm5,%%mm4;\n"
		" pxor		%%mm4,%%mm2;\n"

		" movq		%%mm1,bbmin;\n"
		" movq		%%mm2,bbdxy;\n"

	:: "r" (p), "r" (t), "r" (pitch * 8 /* ch */) : "memory");
}

static inline void
mmx_psse_8(char t[16][16], char *p, int pitch)
{
	asm volatile (
		" movq		(%0),%%mm0;\n"
		" movq		(%1),%%mm1;\n"
		" movq		8(%0),%%mm4;\n"

		" movq		%%mm0,%%mm2;\n"
		" movq		%%mm1,%%mm3;\n"
		" pcmpgtb	%%mm1,%%mm2;\n"		// 1 < 0
		" psubb		%%mm0,%%mm1;\n"		// 1 = 1 - 0
		" movq		%%mm1,%%mm6;\n"
		" punpcklbw	%%mm2,%%mm6;\n"
		" pmullw	%%mm6,%%mm6;\n"
		" punpckhbw	%%mm2,%%mm1;\n"
		" pmullw	%%mm1,%%mm1;\n"
		" paddusw	%%mm1,%%mm6;\n"

		" movq		8(%1),%%mm1;\n"

		" psrlq		$32,%%mm0;\n"
		" movq		%%mm4,%%mm2;\n"
		" psllq		$32,%%mm2;\n"
		" por		%%mm2,%%mm0;\n"

		" movq		%%mm0,%%mm2;\n"
		" pcmpgtb	%%mm3,%%mm2;\n"
		" psubb		%%mm0,%%mm3;\n"
		" movq		%%mm3,%%mm7;\n"
		" punpcklbw	%%mm2,%%mm7;\n"
		" pmullw	%%mm7,%%mm7;\n"
		" punpckhbw	%%mm2,%%mm3;\n"
		" pmullw	%%mm3,%%mm3;\n"
		" movq		(%0,%2),%%mm5;\n"
		" paddusw	%%mm3,%%mm7;\n"

		" movq		%%mm4,%%mm2;\n"
		" movq		%%mm1,%%mm3;\n"
		" pcmpgtb	%%mm1,%%mm2;\n"
		" psubb		%%mm4,%%mm1;\n"
		" movq		%%mm1,%%mm0;\n"
		" punpcklbw	%%mm2,%%mm0;\n"
		" pmullw	%%mm0,%%mm0;\n"
		" paddusw	%%mm0,%%mm6;\n"
		" movd		16(%0),%%mm0;\n"
		" punpckhbw	%%mm2,%%mm1;\n"
		" pmullw	%%mm1,%%mm1;\n"
		" paddusw	%%mm1,%%mm6;\n"

		" movq		16(%1),%%mm1;\n"

		" psrlq		$32,%%mm4;\n"
		" psllq		$32,%%mm0;\n"
		" por		%%mm4,%%mm0;\n"

		" movq		%%mm0,%%mm2;\n"
		" pcmpgtb	%%mm3,%%mm2;\n"
		" psubb		%%mm0,%%mm3;\n"
		" movq		%%mm3,%%mm0;\n"
		" punpcklbw	%%mm2,%%mm0;\n"
		" pmullw	%%mm0,%%mm0;\n"
		" paddusw	%%mm0,%%mm7;\n"
		" movq		8(%0,%2),%%mm0;\n"
		" punpckhbw	%%mm2,%%mm3;\n"
		" pmullw	%%mm3,%%mm3;\n"
		" paddusw	%%mm3,%%mm7;\n"

		" movq		%%mm5,%%mm2;\n"
		" movq		%%mm1,%%mm3;\n"
		" pcmpgtb	%%mm1,%%mm2;\n"
		" psubb		%%mm5,%%mm1;\n"
		" movq		%%mm1,%%mm4;\n"
		" punpcklbw	%%mm2,%%mm4;\n"
		" pmullw	%%mm4,%%mm4;\n"
		" paddusw	%%mm4,%%mm6;\n"
		" movq		24(%1),%%mm4;\n"
		" punpckhbw	%%mm2,%%mm1;\n"
		" pmullw	%%mm1,%%mm1;\n"
		" paddusw	%%mm1,%%mm6;\n"

		" psrlq		$32,%%mm5;\n"
		" movq		%%mm0,%%mm1;\n"
		" psllq		$32,%%mm1;\n"
		" por		%%mm1,%%mm5;\n"

		" movq		%%mm5,%%mm2;\n"
		" pcmpgtb	%%mm3,%%mm2;\n"
		" psubb		%%mm5,%%mm3;\n"
		" movd		16(%0,%2),%%mm5;\n"
		" movq		%%mm3,%%mm1;\n"
		" punpcklbw	%%mm2,%%mm1;\n"
		" pmullw	%%mm1,%%mm1;\n"
		" paddusw	%%mm1,%%mm7;\n"
		" punpckhbw	%%mm2,%%mm3;\n"
		" pmullw	%%mm3,%%mm3;\n"
		" paddusw	%%mm3,%%mm7;\n"

		" movq		%%mm0,%%mm2;\n"
		" movq		%%mm4,%%mm3;\n"
		" pcmpgtb	%%mm4,%%mm2;\n"
		" psubb		%%mm0,%%mm4;\n"
		" movq		%%mm4,%%mm1;\n"
		" punpcklbw	%%mm2,%%mm1;\n"
		" pmullw	%%mm1,%%mm1;\n"
		" paddusw	%%mm1,%%mm6;\n"
		" movq		bbmin,%%mm1;\n"
		" punpckhbw	%%mm2,%%mm4;\n"
		" movq		bbdxy,%%mm2;\n"
		" pmullw	%%mm4,%%mm4;\n"

		" psrlq		$32,%%mm0;\n"
		" paddusw	%%mm4,%%mm6;\n"
		" movq		crdxy,%%mm4;\n"
		" psllq		$32,%%mm5;\n"
		" psubw		c1_15w,%%mm6;\n"
		" por		%%mm5,%%mm0;\n"
		" paddb		c4,%%mm4;\n"

		" movq		%%mm0,%%mm5;\n"
		" pcmpgtb	%%mm3,%%mm5;\n"
		" psubb		%%mm0,%%mm3;\n"
		" movq		%%mm3,%%mm0;\n"
		" punpcklbw	%%mm5,%%mm0;\n"
		" pmullw	%%mm0,%%mm0;\n"
		" paddusw	%%mm0,%%mm7;\n"
		" punpckhbw	%%mm5,%%mm3;\n"
		" pmullw	%%mm3,%%mm3;\n"
		" paddusw	%%mm3,%%mm7;\n"

		" movq		%%mm4,%%mm0;\n"


		" movq		%%mm4,%%mm5;\n"
		" pxor		%%mm3,%%mm3;\n"
		" pcmpgtb	%%mm4,%%mm3;\n"
		" pxor		%%mm3,%%mm5;\n"
		" psubb		%%mm3,%%mm5;\n"

		" movq		%%mm5,%%mm3;\n"
		" psrlw		$8,%%mm5;\n"
		" paddsw	%%mm5,%%mm6;\n"
		" pand		c255,%%mm3;\n"
		" paddsw	%%mm3,%%mm6;\n"


		" movq		%%mm1,%%mm5;\n"
		" pcmpgtw	%%mm6,%%mm5;\n"
		" movq		%%mm1,%%mm3;\n"
		" pxor		%%mm6,%%mm3;\n"
		" pand		%%mm5,%%mm3;\n"
		" pxor		%%mm3,%%mm6;\n"
		" pxor		%%mm3,%%mm1;\n"
		" pxor		%%mm4,%%mm2;\n"
		" pand		%%mm2,%%mm5;\n"
		" pxor		%%mm5,%%mm4;\n"
		" pxor		%%mm4,%%mm2;\n"

		" movq		%%mm6,%%mm5;\n"
		" psllq		$16,%%mm6;\n"
		" psrlq		$48,%%mm5;\n"
		" por		%%mm5,%%mm6;\n"
		" movq		%%mm4,%%mm5;\n"
		" psllq		$16,%%mm4;\n"
		" psrlq		$48,%%mm5;\n"
		" por		%%mm5,%%mm4;\n"

		" movq		%%mm1,%%mm5;\n"
		" pcmpgtw	%%mm6,%%mm5;\n"
		" movq		%%mm1,%%mm3;\n"
		" pxor		%%mm6,%%mm3;\n"
		" pand		%%mm5,%%mm3;\n"
		" pxor		%%mm3,%%mm6;\n"
		" pxor		%%mm3,%%mm1;\n"
		" pxor		%%mm4,%%mm2;\n"
		" pand		%%mm2,%%mm5;\n"
		" pxor		%%mm5,%%mm4;\n"
		" pxor		%%mm4,%%mm2;\n"

		" movq		%%mm6,%%mm5;\n"
		" psllq		$16,%%mm6;\n"
		" psrlq		$48,%%mm5;\n"
		" por		%%mm5,%%mm6;\n"
		" movq		%%mm4,%%mm5;\n"
		" psllq		$16,%%mm4;\n"
		" psrlq		$48,%%mm5;\n"
		" por		%%mm5,%%mm4;\n"

		" psubw		c1_15w,%%mm7;\n"

		" movq		%%mm1,%%mm5;\n"
		" pcmpgtw	%%mm6,%%mm5;\n"
		" movq		%%mm1,%%mm3;\n"
		" pxor		%%mm6,%%mm3;\n"
		" pand		%%mm5,%%mm3;\n"
		" pxor		%%mm3,%%mm6;\n"
		" pxor		%%mm3,%%mm1;\n"
		" pxor		%%mm4,%%mm2;\n"
		" pand		%%mm2,%%mm5;\n"
		" pxor		%%mm5,%%mm4;\n"
		" pxor		%%mm4,%%mm2;\n"

		" paddb		c4,%%mm0;\n"

		" movq		%%mm6,%%mm5;\n"
		" psllq		$16,%%mm6;\n"
		" psrlq		$48,%%mm5;\n"
		" por		%%mm5,%%mm6;\n"
		" movq		%%mm4,%%mm5;\n"
		" psllq		$16,%%mm4;\n"
		" psrlq		$48,%%mm5;\n"
		" por		%%mm5,%%mm4;\n"

		    " movq		%%mm0,crdxy;\n"

		" movq		%%mm1,%%mm5;\n"
		" pcmpgtw	%%mm6,%%mm5;\n"
		" movq		%%mm1,%%mm3;\n"
		" pxor		%%mm6,%%mm3;\n"
		" pand		%%mm5,%%mm3;\n"
		" pxor		%%mm3,%%mm6;\n"
		" pxor		%%mm3,%%mm1;\n"
		" pxor		%%mm4,%%mm2;\n"
		" pand		%%mm2,%%mm5;\n"
		" pxor		%%mm5,%%mm4;\n"
		" pxor		%%mm4,%%mm2;\n"


		" movq		%%mm0,%%mm5;\n"
		" pxor		%%mm3,%%mm3;\n"
		" pcmpgtb	%%mm0,%%mm3;\n"
		" pxor		%%mm3,%%mm5;\n"
		" psubb		%%mm3,%%mm5;\n"

		" movq		%%mm5,%%mm3;\n"
		" psrlw		$8,%%mm5;\n"
		" paddsw	%%mm5,%%mm7;\n"
		" pand		c255,%%mm3;\n"
		" paddsw	%%mm3,%%mm7;\n"


		" movq		%%mm1,%%mm5;\n"
		" pcmpgtw	%%mm7,%%mm5;\n"
		" movq		%%mm1,%%mm3;\n"
		" pxor		%%mm7,%%mm3;\n"
		" pand		%%mm5,%%mm3;\n"
		" pxor		%%mm3,%%mm7;\n"
		" pxor		%%mm3,%%mm1;\n"
		" pxor		%%mm0,%%mm2;\n"
		" pand		%%mm2,%%mm5;\n"
		" pxor		%%mm5,%%mm0;\n"
		" pxor		%%mm0,%%mm2;\n"

		" movq		%%mm7,%%mm5;\n"
		" psllq		$16,%%mm7;\n"
		" psrlq		$48,%%mm5;\n"
		" por		%%mm5,%%mm7;\n"
		" movq		%%mm0,%%mm5;\n"
		" psllq		$16,%%mm0;\n"
		" psrlq		$48,%%mm5;\n"
		" por		%%mm5,%%mm0;\n"

		" movq		%%mm1,%%mm5;\n"
		" pcmpgtw	%%mm7,%%mm5;\n"
		" movq		%%mm1,%%mm3;\n"
		" pxor		%%mm7,%%mm3;\n"
		" pand		%%mm5,%%mm3;\n"
		" pxor		%%mm3,%%mm7;\n"
		" pxor		%%mm3,%%mm1;\n"
		" pxor		%%mm0,%%mm2;\n"
		" pand		%%mm2,%%mm5;\n"
		" pxor		%%mm5,%%mm0;\n"
		" pxor		%%mm0,%%mm2;\n"

		" movq		%%mm7,%%mm5;\n"
		" psllq		$16,%%mm7;\n"
		" psrlq		$48,%%mm5;\n"
		" por		%%mm5,%%mm7;\n"
		" movq		%%mm0,%%mm5;\n"
		" psllq		$16,%%mm0;\n"
		" psrlq		$48,%%mm5;\n"
		" por		%%mm5,%%mm0;\n"

		" movq		%%mm1,%%mm5;\n"
		" pcmpgtw	%%mm7,%%mm5;\n"
		" movq		%%mm1,%%mm3;\n"
		" pxor		%%mm7,%%mm3;\n"
		" pand		%%mm5,%%mm3;\n"
		" pxor		%%mm3,%%mm7;\n"
		" pxor		%%mm3,%%mm1;\n"
		" pxor		%%mm0,%%mm2;\n"
		" pand		%%mm2,%%mm5;\n"
		" pxor		%%mm5,%%mm0;\n"
		" pxor		%%mm0,%%mm2;\n"

		" movq		%%mm7,%%mm5;\n"
		" psllq		$16,%%mm7;\n"
		" psrlq		$48,%%mm5;\n"
		" por		%%mm5,%%mm7;\n"
		" movq		%%mm0,%%mm5;\n"
		" psllq		$16,%%mm0;\n"
		" psrlq		$48,%%mm5;\n"
		" por		%%mm5,%%mm0;\n"

		" movq		%%mm1,%%mm5;\n"
		" pcmpgtw	%%mm7,%%mm5;\n"
		" movq		%%mm1,%%mm3;\n"
		" pxor		%%mm7,%%mm3;\n"
		" pand		%%mm5,%%mm3;\n"
		" pxor		%%mm3,%%mm7;\n"
		" pxor		%%mm3,%%mm1;\n"
		" pxor		%%mm0,%%mm2;\n"
		" pand		%%mm2,%%mm5;\n"
		" pxor		%%mm5,%%mm0;\n"
		" pxor		%%mm0,%%mm2;\n"

		" movq		%%mm1,bbmin;\n"
		" movq		%%mm2,bbdxy;\n"

	:: "r" (p), "r" (t), "r" (pitch * 8 /* ch */) : "memory");
}

static inline void
sse_psse_8(char t[16][16], char *p, int pitch)
{
	asm volatile (
		" movq		(%0),%%mm0;\n"
		" movq		(%1),%%mm1;\n"
		" movq		8(%0),%%mm4;\n"

		" movq		%%mm0,%%mm2;\n"
		" movq		%%mm1,%%mm3;\n"
		" pcmpgtb	%%mm1,%%mm2;\n"		// 1 < 0
		" psubb		%%mm0,%%mm1;\n"		// 1 = 1 - 0
		" movq		%%mm1,%%mm6;\n"
		" punpcklbw	%%mm2,%%mm6;\n"
		" pmullw	%%mm6,%%mm6;\n"
		" punpckhbw	%%mm2,%%mm1;\n"
		" pmullw	%%mm1,%%mm1;\n"
		" paddusw	%%mm1,%%mm6;\n"

		" movq		8(%1),%%mm1;\n"

		" psrlq		$32,%%mm0;\n"
		" movq		%%mm4,%%mm2;\n"
		" psllq		$32,%%mm2;\n"
		" por		%%mm2,%%mm0;\n"

		" movq		%%mm0,%%mm2;\n"
		" pcmpgtb	%%mm3,%%mm2;\n"
		" psubb		%%mm0,%%mm3;\n"
		" movq		%%mm3,%%mm7;\n"
		" punpcklbw	%%mm2,%%mm7;\n"
		" pmullw	%%mm7,%%mm7;\n"
		" punpckhbw	%%mm2,%%mm3;\n"
		" pmullw	%%mm3,%%mm3;\n"
		" movq		(%0,%2),%%mm5;\n"
		" paddusw	%%mm3,%%mm7;\n"

		" movq		%%mm4,%%mm2;\n"
		" movq		%%mm1,%%mm3;\n"
		" pcmpgtb	%%mm1,%%mm2;\n"
		" psubb		%%mm4,%%mm1;\n"
		" movq		%%mm1,%%mm0;\n"
		" punpcklbw	%%mm2,%%mm0;\n"
		" pmullw	%%mm0,%%mm0;\n"
		" paddusw	%%mm0,%%mm6;\n"
		" movd		16(%0),%%mm0;\n"
		" punpckhbw	%%mm2,%%mm1;\n"
		" pmullw	%%mm1,%%mm1;\n"
		" paddusw	%%mm1,%%mm6;\n"

		" movq		16(%1),%%mm1;\n"

		" psrlq		$32,%%mm4;\n"
		" psllq		$32,%%mm0;\n"
		" por		%%mm4,%%mm0;\n"

		" movq		%%mm0,%%mm2;\n"
		" pcmpgtb	%%mm3,%%mm2;\n"
		" psubb		%%mm0,%%mm3;\n"
		" movq		%%mm3,%%mm0;\n"
		" punpcklbw	%%mm2,%%mm0;\n"
		" pmullw	%%mm0,%%mm0;\n"
		" paddusw	%%mm0,%%mm7;\n"
		" movq		8(%0,%2),%%mm0;\n"
		" punpckhbw	%%mm2,%%mm3;\n"
		" pmullw	%%mm3,%%mm3;\n"
		" paddusw	%%mm3,%%mm7;\n"

		" movq		%%mm5,%%mm2;\n"
		" movq		%%mm1,%%mm3;\n"
		" pcmpgtb	%%mm1,%%mm2;\n"
		" psubb		%%mm5,%%mm1;\n"
		" movq		%%mm1,%%mm4;\n"
		" punpcklbw	%%mm2,%%mm4;\n"
		" pmullw	%%mm4,%%mm4;\n"
		" paddusw	%%mm4,%%mm6;\n"
		" movq		24(%1),%%mm4;\n"
		" punpckhbw	%%mm2,%%mm1;\n"
		" pmullw	%%mm1,%%mm1;\n"
		" paddusw	%%mm1,%%mm6;\n"

		" psrlq		$32,%%mm5;\n"
		" movq		%%mm0,%%mm1;\n"
		" psllq		$32,%%mm1;\n"
		" por		%%mm1,%%mm5;\n"

		" movq		%%mm5,%%mm2;\n"
		" pcmpgtb	%%mm3,%%mm2;\n"
		" psubb		%%mm5,%%mm3;\n"
		" movd		16(%0,%2),%%mm5;\n"
		" movq		%%mm3,%%mm1;\n"
		" punpcklbw	%%mm2,%%mm1;\n"
		" pmullw	%%mm1,%%mm1;\n"
		" paddusw	%%mm1,%%mm7;\n"
		" punpckhbw	%%mm2,%%mm3;\n"
		" pmullw	%%mm3,%%mm3;\n"
		" paddusw	%%mm3,%%mm7;\n"

		" movq		%%mm0,%%mm2;\n"
		" movq		%%mm4,%%mm3;\n"
		" pcmpgtb	%%mm4,%%mm2;\n"
		" psubb		%%mm0,%%mm4;\n"
		" movq		%%mm4,%%mm1;\n"
		" punpcklbw	%%mm2,%%mm1;\n"
		" pmullw	%%mm1,%%mm1;\n"
		" paddusw	%%mm1,%%mm6;\n"
		" movq		bbmin,%%mm1;\n"
		" punpckhbw	%%mm2,%%mm4;\n"
		" movq		bbdxy,%%mm2;\n"
		" pmullw	%%mm4,%%mm4;\n"

		" psrlq		$32,%%mm0;\n"
		" paddusw	%%mm4,%%mm6;\n"
		" movq		crdxy,%%mm4;\n"
		" psllq		$32,%%mm5;\n"
		" psubw		c1_15w,%%mm6;\n"
		" por		%%mm5,%%mm0;\n"
		" paddb		c4,%%mm4;\n"

		" movq		%%mm0,%%mm5;\n"
		" pcmpgtb	%%mm3,%%mm5;\n"
		" psubb		%%mm0,%%mm3;\n"
		" movq		%%mm3,%%mm0;\n"
		" punpcklbw	%%mm5,%%mm0;\n"
		" pmullw	%%mm0,%%mm0;\n"
		" paddusw	%%mm0,%%mm7;\n"
		" punpckhbw	%%mm5,%%mm3;\n"
		" pmullw	%%mm3,%%mm3;\n"
		" paddusw	%%mm3,%%mm7;\n"

		" movq		%%mm4,%%mm0;\n"


		" movq		%%mm4,%%mm5;\n"
		" pxor		%%mm3,%%mm3;\n"
		" pcmpgtb	%%mm4,%%mm3;\n"
		" pxor		%%mm3,%%mm5;\n"
		" psubb		%%mm3,%%mm5;\n"

		" movq		%%mm5,%%mm3;\n"
		" psrlw		$8,%%mm5;\n"
		" paddsw	%%mm5,%%mm6;\n"
		" pand		c255,%%mm3;\n"
		" paddsw	%%mm3,%%mm6;\n"

		" movq		%%mm1,%%mm5;\n"
		" pcmpgtw	%%mm6,%%mm5;\n"
		" movq		%%mm1,%%mm3;\n"
		" pxor		%%mm6,%%mm3;\n"
		" pand		%%mm5,%%mm3;\n"
		" pxor		%%mm3,%%mm6;\n"
		" pxor		%%mm3,%%mm1;\n"
		" pshufw	$2*64+1*16+0*4+3,%%mm6,%%mm6;\n"
		" pxor		%%mm4,%%mm2;\n"
		" pand		%%mm2,%%mm5;\n"
		" pxor		%%mm5,%%mm4;\n"
		" pxor		%%mm4,%%mm2;\n"
		" pshufw	$2*64+1*16+0*4+3,%%mm4,%%mm4;\n"

		" psubw		c1_15w,%%mm7;\n"

		" movq		%%mm1,%%mm5;\n"
		" pcmpgtw	%%mm6,%%mm5;\n"
		" movq		%%mm1,%%mm3;\n"
		" pxor		%%mm6,%%mm3;\n"
		" pand		%%mm5,%%mm3;\n"
		" pxor		%%mm3,%%mm6;\n"
		" pxor		%%mm3,%%mm1;\n"
		" pshufw	$2*64+1*16+0*4+3,%%mm6,%%mm6;\n"
		" pxor		%%mm4,%%mm2;\n"
		" pand		%%mm2,%%mm5;\n"
		" pxor		%%mm5,%%mm4;\n"
		" pxor		%%mm4,%%mm2;\n"
		" pshufw	$2*64+1*16+0*4+3,%%mm4,%%mm4;\n"

		" paddb		c4,%%mm0;\n"

		" movq		%%mm1,%%mm5;\n"
		" pcmpgtw	%%mm6,%%mm5;\n"
		" movq		%%mm1,%%mm3;\n"
		" pxor		%%mm6,%%mm3;\n"
		" pand		%%mm5,%%mm3;\n"
		" pxor		%%mm3,%%mm6;\n"
		" pxor		%%mm3,%%mm1;\n"
		" pshufw	$2*64+1*16+0*4+3,%%mm6,%%mm6;\n"
		" pxor		%%mm4,%%mm2;\n"
		" pand		%%mm2,%%mm5;\n"
		" pxor		%%mm5,%%mm4;\n"
		" pxor		%%mm4,%%mm2;\n"
		" pshufw	$2*64+1*16+0*4+3,%%mm4,%%mm4;\n"

		    " movq		%%mm0,crdxy;\n"

		" movq		%%mm1,%%mm5;\n"
		" pcmpgtw	%%mm6,%%mm5;\n"
		" movq		%%mm1,%%mm3;\n"
		" pxor		%%mm6,%%mm3;\n"
		" pand		%%mm5,%%mm3;\n"
		" pxor		%%mm3,%%mm6;\n"
		" pxor		%%mm3,%%mm1;\n"
		" pxor		%%mm4,%%mm2;\n"
		" pand		%%mm2,%%mm5;\n"
		" pxor		%%mm5,%%mm4;\n"
		" pxor		%%mm4,%%mm2;\n"


		" movq		%%mm0,%%mm5;\n"
		" pxor		%%mm3,%%mm3;\n"
		" pcmpgtb	%%mm0,%%mm3;\n"
		" pxor		%%mm3,%%mm5;\n"
		" psubb		%%mm3,%%mm5;\n"

		" movq		%%mm5,%%mm3;\n"
		" psrlw		$8,%%mm5;\n"
		" paddsw	%%mm5,%%mm7;\n"
		" pand		c255,%%mm3;\n"
		" paddsw	%%mm3,%%mm7;\n"


		" movq		%%mm1,%%mm5;\n"
		" pcmpgtw	%%mm7,%%mm5;\n"
		" movq		%%mm1,%%mm3;\n"
		" pxor		%%mm7,%%mm3;\n"
		" pand		%%mm5,%%mm3;\n"
		" pxor		%%mm3,%%mm7;\n"
		" pxor		%%mm3,%%mm1;\n"
		" pshufw	$2*64+1*16+0*4+3,%%mm7,%%mm7;\n"
		" pxor		%%mm0,%%mm2;\n"
		" pand		%%mm2,%%mm5;\n"
		" pxor		%%mm5,%%mm0;\n"
		" pxor		%%mm0,%%mm2;\n"
		" pshufw	$2*64+1*16+0*4+3,%%mm0,%%mm0;\n"

		" movq		%%mm1,%%mm5;\n"
		" pcmpgtw	%%mm7,%%mm5;\n"
		" movq		%%mm1,%%mm3;\n"
		" pxor		%%mm7,%%mm3;\n"
		" pand		%%mm5,%%mm3;\n"
		" pxor		%%mm3,%%mm7;\n"
		" pxor		%%mm3,%%mm1;\n"
		" pshufw	$2*64+1*16+0*4+3,%%mm7,%%mm7;\n"
		" pxor		%%mm0,%%mm2;\n"
		" pand		%%mm2,%%mm5;\n"
		" pxor		%%mm5,%%mm0;\n"
		" pxor		%%mm0,%%mm2;\n"
		" pshufw	$2*64+1*16+0*4+3,%%mm0,%%mm0;\n"

		" movq		%%mm1,%%mm5;\n"
		" pcmpgtw	%%mm7,%%mm5;\n"
		" movq		%%mm1,%%mm3;\n"
		" pxor		%%mm7,%%mm3;\n"
		" pand		%%mm5,%%mm3;\n"
		" pxor		%%mm3,%%mm7;\n"
		" pxor		%%mm3,%%mm1;\n"
		" pshufw	$2*64+1*16+0*4+3,%%mm7,%%mm7;\n"
		" pxor		%%mm0,%%mm2;\n"
		" pand		%%mm2,%%mm5;\n"
		" pxor		%%mm5,%%mm0;\n"
		" pxor		%%mm0,%%mm2;\n"
		" pshufw	$2*64+1*16+0*4+3,%%mm0,%%mm0;\n"

		" movq		%%mm1,%%mm5;\n"
		" pcmpgtw	%%mm7,%%mm5;\n"
		" movq		%%mm1,%%mm3;\n"
		" pxor		%%mm7,%%mm3;\n"
		" pand		%%mm5,%%mm3;\n"
		" pxor		%%mm3,%%mm7;\n"
		" pxor		%%mm3,%%mm1;\n"
		" pxor		%%mm0,%%mm2;\n"
		" movq		%%mm1,bbmin;\n"
		" pand		%%mm2,%%mm5;\n"
		" pxor		%%mm5,%%mm0;\n"
		" pxor		%%mm0,%%mm2;\n"

		" movq		%%mm2,bbdxy;\n"

	:: "r" (p), "r" (t), "r" (pitch * 8 /* ch */) : "memory");
}

static inline void
mmx_load_pref(char t[16][16])
{
	asm volatile (
		" movq		0*16+0(%0),%%mm0;\n"
		" paddw		1*16+0(%0),%%mm0;\n"
		" movq		0*16+8(%0),%%mm1;\n"
		" paddw		1*16+8(%0),%%mm1;\n"
		" movq		c1b,%%mm7;\n"
		" paddw		2*16+0(%0),%%mm0;\n"
		" movq		%%mm7,%%mm6;\n"
		" paddw		2*16+8(%0),%%mm1;\n"
		" psllq		$7,%%mm7;\n"
		" paddw		3*16+0(%0),%%mm0;\n"
		" psrlw		$8-4,%%mm6;\n"
		" paddw		3*16+8(%0),%%mm1;\n"
		" movq		256+0*16+0(%0),%%mm2;\n"
		" paddw		4*16+0(%0),%%mm0;\n"
		" movq		256+0*16+8(%0),%%mm3;\n"
		" paddw		4*16+8(%0),%%mm1;\n"
		" paddw		5*16+0(%0),%%mm0;\n"
		" paddw		5*16+8(%0),%%mm1;\n"
		" paddw		6*16+0(%0),%%mm0;\n"
		" paddw		6*16+8(%0),%%mm1;\n"
		" paddw		7*16+0(%0),%%mm0;\n"
		" movq		%%mm0,%%mm4;\n"
		" paddw		7*16+8(%0),%%mm1;\n"
		" punpcklwd	%%mm1,%%mm0;\n"
		" paddw		256+1*16+0(%0),%%mm2;\n"
		" punpckhwd	%%mm1,%%mm4;\n"
		" paddw		256+1*16+8(%0),%%mm3;\n"
		" paddw		%%mm4,%%mm0;\n"
		" paddw		256+2*16+0(%0),%%mm2;\n"
		" paddw		256+2*16+8(%0),%%mm3;\n"
		" paddw		256+3*16+0(%0),%%mm2;\n"
		" paddw		256+3*16+8(%0),%%mm3;\n"
		" paddw		256+4*16+0(%0),%%mm2;\n"
		" paddw		256+4*16+8(%0),%%mm3;\n"
		" paddw		256+5*16+0(%0),%%mm2;\n"
		" paddw		256+5*16+8(%0),%%mm3;\n"
		" paddw		256+6*16+0(%0),%%mm2;\n"
		" paddw		256+6*16+8(%0),%%mm3;\n"
		" paddw		256+7*16+0(%0),%%mm2;\n"
		" paddw		256+7*16+8(%0),%%mm3;\n"
		" movq		128+0*16+0(%0),%%mm1;\n"
		" movq		%%mm2,%%mm5;\n"
		" movq		128+0*16+8(%0),%%mm4;\n"
		" punpcklwd	%%mm3,%%mm2;\n"
		" paddw		128+1*16+0(%0),%%mm1;\n"
		" punpckhwd	%%mm3,%%mm5;\n"
		" paddw		128+1*16+8(%0),%%mm4;\n"
		" paddw		%%mm5,%%mm2;\n"
		" paddw		128+2*16+0(%0),%%mm1;\n"
		" movq		%%mm0,%%mm5;\n"
		" paddw		128+2*16+8(%0),%%mm4;\n"
		" punpckldq	%%mm2,%%mm0;\n"
		" paddw		128+3*16+0(%0),%%mm1;\n"
		" punpckhdq	%%mm2,%%mm5;\n"
		" paddw		128+3*16+8(%0),%%mm4;\n"
		" paddw		%%mm5,%%mm0;\n"
		" paddw		128+4*16+0(%0),%%mm1;\n"
		" paddw		%%mm6,%%mm0;\n"
		" paddw		128+4*16+8(%0),%%mm4;\n"
		" psrlw		$5,%%mm0;\n"
		" paddw		128+5*16+0(%0),%%mm1;\n"
		" movq		%%mm0,%%mm2;\n"
		" paddw		128+5*16+8(%0),%%mm4;\n"
		" psllw		$8,%%mm2;\n"
		" paddw		128+6*16+0(%0),%%mm1;\n"
		" por		%%mm2,%%mm0;\n"
		" paddw		128+6*16+8(%0),%%mm4;\n"
		" pxor		%%mm7,%%mm0;\n"
		" paddw		128+7*16+0(%0),%%mm1;\n"
		" movq		%%mm0,%%mm2;\n"
		" paddw		128+7*16+8(%0),%%mm4;\n"
		" punpcklbw	%%mm0,%%mm0;\n"
		" movq		384+0*16+0(%0),%%mm3;\n"
		" punpckhbw	%%mm2,%%mm2;\n"
		" movq		384+0*16+8(%0),%%mm5;\n"
		" movq		%%mm0,0(%1);\n"
		" paddw		384+1*16+0(%0),%%mm3;\n"
		" movq		%%mm2,8(%1);\n"
		" paddw		384+1*16+8(%0),%%mm5;\n"
		" movq		%%mm1,%%mm0;\n"
		" paddw		384+2*16+0(%0),%%mm3;\n"
		" punpcklwd	%%mm4,%%mm1;\n"
		" paddw		384+2*16+8(%0),%%mm5;\n"
		" punpckhwd	%%mm4,%%mm0;\n"
		" paddw		384+3*16+0(%0),%%mm3;\n"
		" paddw		%%mm0,%%mm1;\n"
		" paddw		384+3*16+8(%0),%%mm5;\n"
		" movq		%%mm1,%%mm2;\n"
		" paddw		384+4*16+0(%0),%%mm3;\n"
		" paddw		384+4*16+8(%0),%%mm5;\n"
		" paddw		384+5*16+0(%0),%%mm3;\n"
		" paddw		384+5*16+8(%0),%%mm5;\n"
		" paddw		384+6*16+0(%0),%%mm3;\n"
		" paddw		384+6*16+8(%0),%%mm5;\n"
		" paddw		384+7*16+0(%0),%%mm3;\n"
		" paddw		384+7*16+8(%0),%%mm5;\n"
		" movq		%%mm3,%%mm0;\n"
		" punpcklwd	%%mm5,%%mm3;\n"
		" punpckhwd	%%mm5,%%mm0;\n"
		" paddw		%%mm0,%%mm3;\n"
		" punpckldq	%%mm3,%%mm1;\n"
		" punpckhdq	%%mm3,%%mm2;\n"
		" paddw		%%mm2,%%mm1;\n"
		" paddw		%%mm6,%%mm1;\n"
		" psrlw		$5,%%mm1;\n"
		" movq		%%mm1,%%mm2;\n"
		" psllw		$8,%%mm2;\n"
		" por		%%mm2,%%mm1;\n"
		" pxor		%%mm7,%%mm1;\n"
		" movq		%%mm1,%%mm2;\n"
		" punpcklbw	%%mm1,%%mm1;\n"
		" punpckhbw	%%mm2,%%mm2;\n"
		" movq		%%mm1,16(%1);\n"
		" movq		%%mm2,24(%1);\n"

	:: "S" (&mblock[0][0][0][0]), "D" (&t[0][0]) : "memory");
}

#if USE_SSE2

static inline void
sse2_load_pref(char t[16][16])
{
	asm volatile (
		/* missing in gnu as 2.10.91 */
		" .macro punpcklqdqrr s,d\n"
		" .byte 0x66, 0x0F, 0x6C, \\s+\\d*8+3*64\n"
		" .endm\n"

		" movdqa	(%0),%%xmm0;\n"
		" movq		c1b,%%xmm7;\n"
		" paddw		1*16+0(%0),%%xmm0;\n"
		" punpcklqdqrr  7,7;\n"
		" paddw		2*16+0(%0),%%xmm0;\n"
		" movdqa	%%xmm7,%%xmm6;\n"
		" paddw		3*16+0(%0),%%xmm0;\n"
		" psllq		$7,%%xmm7;\n"
		" paddw		4*16+0(%0),%%xmm0;\n"
		" psrlw		$8-4,%%xmm6;\n"
		" paddw		5*16+0(%0),%%xmm0;\n"
		" movdqa	256+0*16+0(%0),%%xmm2;\n"
		" paddw		6*16+0(%0),%%xmm0;\n"
		" paddw		256+1*16+0(%0),%%xmm2;\n"
		" paddw		7*16+0(%0),%%xmm0;\n"
		" paddw		256+2*16+0(%0),%%xmm2;\n"
		" movdqa	%%xmm0,%%xmm4;\n"
		" paddw		256+3*16+0(%0),%%xmm2;\n"
		" paddw		256+4*16+0(%0),%%xmm2;\n"
		" paddw		256+5*16+0(%0),%%xmm2;\n"
		" paddw		256+6*16+0(%0),%%xmm2;\n"
		" paddw		256+7*16+0(%0),%%xmm2;\n"
		" punpcklwd	%%xmm2,%%xmm0;\n" // 0a1b2c3d
		" movdqa	128+0*16+0(%0),%%xmm1;\n"
		" punpckhwd	%%xmm2,%%xmm4;\n" // 4e5f6g7h
		" paddw		128+1*16+0(%0),%%xmm1;\n"
		" movdqa	%%xmm0,%%xmm2;\n"
		" paddw		128+2*16+0(%0),%%xmm1;\n"
		" punpckldq	%%xmm4,%%xmm0;\n" // 0a4e1b5f
		" paddw		128+3*16+0(%0),%%xmm1;\n"
		" punpckhdq	%%xmm4,%%xmm2;\n" // 2c6g3d7h
		" paddw		128+4*16+0(%0),%%xmm1;\n"
		" paddw		%%xmm2,%%xmm0;\n" // 02 ac 46 eg 13 bd 57 fh
		" paddw		128+5*16+0(%0),%%xmm1;\n"
		" movdqa	%%xmm0,%%xmm2;\n"
		" paddw		128+6*16+0(%0),%%xmm1;\n"
		" paddw		128+7*16+0(%0),%%xmm1;\n"
		" movdqa	%%xmm1,%%xmm5;\n"
		" movdqa	384+0*16+0(%0),%%xmm3;\n"
		" paddw		384+1*16+0(%0),%%xmm3;\n"
		" paddw		384+2*16+0(%0),%%xmm3;\n"
		" paddw		384+3*16+0(%0),%%xmm3;\n"
		" paddw		384+4*16+0(%0),%%xmm3;\n"
		" paddw		384+5*16+0(%0),%%xmm3;\n"
		" paddw		384+6*16+0(%0),%%xmm3;\n"
		" paddw		384+7*16+0(%0),%%xmm3;\n"
		" punpcklwd	%%xmm3,%%xmm1;\n" // 0a1b2c3d
		" punpckhwd	%%xmm3,%%xmm5;\n" // 4e5f6g7h
		" movdqa	%%xmm1,%%xmm3;\n"
		" punpckldq	%%xmm5,%%xmm1;\n" // 0a4e1b5f
		" punpckhdq	%%xmm5,%%xmm3;\n" // 2c6g3d7h
		" paddw		%%xmm3,%%xmm1;\n" // 02' ac' 46' eg' 13' bd' 57' fh'
		" punpcklqdqrr	1,0;\n" 	  // 02 ac 46 eg 02' ac' 46' eg' 
		" punpckhqdq	%%xmm1,%%xmm2;\n" // 13 bd 57 fh 13' bd' 57' fh'
		" paddw		%%xmm2,%%xmm0;\n" // 0123 abcd 4567 efgh 0123' abcd' 4567' efgh'
		" paddw		%%xmm6,%%xmm0;\n"
		" psrlw		$5,%%xmm0;\n"
		" movdqa	%%xmm0,%%xmm2;\n"
		" psllw		$8,%%xmm2;\n"
		" por		%%xmm2,%%xmm0;\n"
		" pxor		%%xmm7,%%xmm0;\n"
		" movq		%%mm0,%%mm1;\n"
		" punpcklbw	%%mm0,%%mm0;\n"
		" punpckhbw	%%mm1,%%mm1;\n"
		" movdqa	%%xmm0,(%1);\n"
		" movdqa	%%xmm1,16(%1);\n"
	:: "S" (&mblock[0][0][0][0]), "D" (&t[0][0]) : "memory");
}

#endif /* USE_SSE2 */

static inline unsigned int
mmx_predict(unsigned char *from, int d2x, int d2y,
      typeof (temp22) *ibuf, int iright, int idown, short dest[6][8][8])
{
	unsigned char *p, *q;
	int mx, my, hy;
	unsigned int s;
	int i;

	pr_start(53, "forward fetch");

	asm volatile (
		" pxor		%mm5,%mm5;\n"
		" pxor		%mm7,%mm7;\n"
	);

	if (iright) {
	for (i = 0; i < 16; i++) {
		asm volatile (
			" movq		1(%0),%%mm0;\n"
			" movq		%%mm0,%%mm1;\n"
			" punpcklbw	%%mm5,%%mm0;\n"
			" punpckhbw	%%mm5,%%mm1;\n"
			" movq		%%mm0,2*768+0*128+0(%3);\n"
			" movq		%%mm1,2*768+0*128+8(%3);\n"
			" movq		0*768+0*128+0(%1),%%mm2;\n"
			" movq		0*768+0*128+8(%1),%%mm3;\n"
			" psubw		%%mm0,%%mm2;\n"
			" psubw		%%mm1,%%mm3;\n"
			" movq		%%mm2,0*768+0*128+0(%3);\n"
			" movq		%%mm3,0*768+0*128+8(%3);\n"
			" pmaddwd	%%mm2,%%mm2;\n"
			" pmaddwd	%%mm3,%%mm3;\n"
			" paddd		%%mm2,%%mm7;\n"
			" paddd		%%mm3,%%mm7;\n"

			" movq		8(%0),%%mm0;\n"
			" psrlq		$8,%%mm0;\n"
			" movq		(%2),%%mm2;\n"
			" psllq		$56,%%mm2;\n"
			" por		%%mm2,%%mm0;\n"
			" movq		%%mm0,%%mm1;\n"
			" punpcklbw	%%mm5,%%mm0;\n"
			" punpckhbw	%%mm5,%%mm1;\n"
			" movq		%%mm0,2*768+2*128+0(%3);\n"
			" movq		%%mm1,2*768+2*128+8(%3);\n"
			" movq		0*768+2*128+0(%1),%%mm2;\n"
			" movq		0*768+2*128+8(%1),%%mm3;\n"
			" psubw		%%mm0,%%mm2;\n"
			" psubw		%%mm1,%%mm3;\n"
			" movq		%%mm2,0*768+2*128+0(%3);\n"
			" movq		%%mm3,0*768+2*128+8(%3);\n"
			" pmaddwd	%%mm2,%%mm2;\n"
			" pmaddwd	%%mm3,%%mm3;\n"
			" paddd		%%mm2,%%mm7;\n"
			" paddd		%%mm3,%%mm7;\n"

		:: "r" (&(* ibuf)[i+idown][0]),
		   "r" (&mblock[0][0][i][0]),
		   "r" (&(* ibuf)[18][i+idown]),
		   "r" (&dest[0][i][0]));
	}
	} else {
	for (i = 0; i < 16; i++) {
		asm volatile (
			" movq		(%0),%%mm0;\n"
			" movq		%%mm0,%%mm1;\n"
			" punpcklbw	%%mm5,%%mm0;\n"
			" punpckhbw	%%mm5,%%mm1;\n"
			" movq		%%mm0,2*768+0*128+0(%2);\n"
			" movq		%%mm1,2*768+0*128+8(%2);\n"
			" movq		0*768+0*128+0(%1),%%mm2;\n"
			" movq		0*768+0*128+8(%1),%%mm3;\n"
			" psubw		%%mm0,%%mm2;\n"
			" psubw		%%mm1,%%mm3;\n"
			" movq		%%mm2,0*768+0*128+0(%2);\n"
			" movq		%%mm3,0*768+0*128+8(%2);\n"
			" pmaddwd	%%mm2,%%mm2;\n"
			" pmaddwd	%%mm3,%%mm3;\n"
			" paddd		%%mm2,%%mm7;\n"
			" paddd		%%mm3,%%mm7;\n"

			" movq		8(%0),%%mm0;\n"
			" movq		%%mm0,%%mm1;\n"
			" punpcklbw	%%mm5,%%mm0;\n"
			" punpckhbw	%%mm5,%%mm1;\n"
			" movq		%%mm0,2*768+2*128+0(%2);\n"
			" movq		%%mm1,2*768+2*128+8(%2);\n"
			" movq		0*768+2*128+0(%1),%%mm2;\n"
			" movq		0*768+2*128+8(%1),%%mm3;\n"
			" psubw		%%mm0,%%mm2;\n"
			" psubw		%%mm1,%%mm3;\n"
			" movq		%%mm2,0*768+2*128+0(%2);\n"
			" movq		%%mm3,0*768+2*128+8(%2);\n"
			" pmaddwd	%%mm2,%%mm2;\n"
			" pmaddwd	%%mm3,%%mm3;\n"
			" paddd		%%mm2,%%mm7;\n"
			" paddd		%%mm3,%%mm7;\n"

		:: "r" (&(* ibuf)[i+idown][0]),
		     "r" (&mblock[0][0][i][0]),
		     "r" (&dest[0][i][0]));
	}
	}

	asm volatile (
		" movq		%%mm7,%%mm0;\n"
		" psrlq		$32,%%mm7;\n"
		" paddd		%%mm0,%%mm7;\n"
		" pslld		$8,%%mm7;\n"
		" movd		%%mm7,%0;\n"
	: "=&r" (s) : "r" (0));

	mx = d2x + mb_col * 16 * 2;
	my = d2y + mb_row * 16 * 2;

	p = from + mb_address.chrom_0
		+ 8 * mb_address.block[0].pitch + 8
		+ (mx >> 2) + (my >> 2) * mb_address.block[4].pitch;
	hy = ((my >> 1) & 1) * mb_address.block[4].pitch;

	if (mx & 2) {
		q = p + mb_address.block[5].offset;
	asm volatile (
		" movq		c2,%mm7;\n"
	);
		for (i = 0; i < 8; i++) {
			asm volatile (
				" pushl		%1\n"
				" movzxb	(%0),%1;\n"
				" movd		%1,%%mm1;\n"
				" popl		%1\n"
				" movq		1(%0),%%mm2;\n"
				" movq		%%mm2,%%mm0;\n"
				" psllq		$8,%%mm0;\n"
				" por		%%mm1,%%mm0;\n"
				" movq		%%mm0,%%mm1;\n"
				" punpcklbw	%%mm5,%%mm0;\n"
				" punpckhbw	%%mm5,%%mm1;\n"
				" movq		%%mm2,%%mm3;\n"
				" punpcklbw	%%mm5,%%mm2;\n"
				" punpckhbw	%%mm5,%%mm3;\n"
				" paddw		%%mm2,%%mm0;\n"
				" paddw		%%mm3,%%mm1;\n"
				" pushl		%1\n"
				" movzxb	(%0,%2),%1;\n"
				" movd		%1,%%mm3;\n"
				" popl		%1\n"
				" movq		1(%0,%2),%%mm2;\n"
				" movq		%%mm2,%%mm4;\n"
				" psllq		$8,%%mm4;\n"
				" por		%%mm3,%%mm4;\n"
				" movq		%%mm4,%%mm3;\n"
				" punpcklbw	%%mm5,%%mm4;\n"
				" punpckhbw	%%mm5,%%mm3;\n"
				" paddw		%%mm4,%%mm0;\n"
				" paddw		%%mm3,%%mm1;\n"
				" movq		%%mm2,%%mm3;\n"
				" punpcklbw	%%mm5,%%mm2;\n"
				" punpckhbw	%%mm5,%%mm3;\n"
				" paddw		%%mm2,%%mm0;\n"
				" paddw		%%mm3,%%mm1;\n"
				" paddw		%%mm7,%%mm0;\n"
				" paddw		%%mm7,%%mm1;\n"
				" psrlw		$2,%%mm0;\n"
				" psrlw		$2,%%mm1;\n"
				" movq		0*768+4*128+0(%4),%%mm2;\n"
				" movq		0*768+4*128+8(%4),%%mm3;\n"
				" psubw		%%mm0,%%mm2;\n"
				" psubw		%%mm1,%%mm3;\n"
				" movq		%%mm0,2*768+4*128+0(%1);\n"
				" movq		%%mm1,2*768+4*128+8(%1);\n"
				" movq		%%mm2,0*768+4*128+0(%1);\n"
				" movq		%%mm3,0*768+4*128+8(%1);\n"

				" pushl		%1\n"
				" movzxb	(%3),%1;\n"
				" movd		%1,%%mm1;\n"
				" popl		%1\n"
				" movq		1(%3),%%mm2;\n"
				" movq		%%mm2,%%mm0;\n"
				" psllq		$8,%%mm0;\n"
				" por		%%mm1,%%mm0;\n"
				" movq		%%mm0,%%mm1;\n"
				" punpcklbw	%%mm5,%%mm0;\n"
				" punpckhbw	%%mm5,%%mm1;\n"
				" movq		%%mm2,%%mm3;\n"
				" punpcklbw	%%mm5,%%mm2;\n"
				" punpckhbw	%%mm5,%%mm3;\n"
				" paddw		%%mm2,%%mm0;\n"
				" paddw		%%mm3,%%mm1;\n"
				" pushl		%1\n"
				" movzxb	(%3,%2),%1;\n"
				" movd		%1,%%mm3;\n"
				" popl		%1\n"
				" movq		1(%3,%2),%%mm2;\n"
				" movq		%%mm2,%%mm4;\n"
				" psllq		$8,%%mm4;\n"
				" por		%%mm3,%%mm4;\n"
				" movq		%%mm4,%%mm3;\n"
				" punpcklbw	%%mm5,%%mm4;\n"
				" punpckhbw	%%mm5,%%mm3;\n"
				" paddw		%%mm4,%%mm0;\n"
				" paddw		%%mm3,%%mm1;\n"
				" movq		%%mm2,%%mm3;\n"
				" punpcklbw	%%mm5,%%mm2;\n"
				" punpckhbw	%%mm5,%%mm3;\n"
				" paddw		%%mm2,%%mm0;\n"
				" paddw		%%mm3,%%mm1;\n"
				" paddw		%%mm7,%%mm0;\n"
				" paddw		%%mm7,%%mm1;\n"
				" psrlw		$2,%%mm0;\n"
				" psrlw		$2,%%mm1;\n"

				" movq		0*768+5*128+0(%4),%%mm2;\n"
				" movq		0*768+5*128+8(%4),%%mm3;\n"
				" psubw		%%mm0,%%mm2;\n"
				" psubw		%%mm1,%%mm3;\n"
				" movq		%%mm0,2*768+5*128+0(%1);\n"
				" movq		%%mm1,2*768+5*128+8(%1);\n"
				" movq		%%mm2,0*768+5*128+0(%1);\n"
				" movq		%%mm3,0*768+5*128+8(%1);\n"

			:: "r" (p), "c" (&dest[0][i][0]), "r" (hy), "r" (q),
			    "r" (&mblock[0][0][i][0]));

		p += mb_address.block[4].pitch;
		q += mb_address.block[4].pitch;
	}
	} else {
	asm volatile (
		" movq		(%0),%%mm3;\n"
		" movq		(%0,%1),%%mm4;\n"
		" movq		c1b,%%mm7;\n"
		" pxor		%%mm6,%%mm6;\n"
	:: "r" (p), "r" (mb_address.block[5].offset), "r" (0));

	p += hy;

	if (hy) // XXX
		asm volatile (
			" pcmpeqb	%mm6,%mm6;\n"
		);

	for (i = 0; i < 8; i++) {
		asm volatile (
			" movq		(%0),%%mm2;\n"
			" movq		%%mm3,%%mm1;\n"
			" pxor		%%mm2,%%mm1;\n"
			" movq		%%mm2,%%mm0;\n"
			" pand		%%mm6,%%mm1;\n"
			" pxor		%%mm1,%%mm0;\n"
			" pxor		%%mm1,%%mm3;\n"
			" movq		%%mm2,%%mm1;\n"
			" por		%%mm0,%%mm2;\n"
			" por		%%mm7,%%mm0;\n"
			" por		%%mm7,%%mm1;\n"
			" psrlq		$1,%%mm0;\n"
			" pand		%%mm7,%%mm2;\n"
			" psrlq		$1,%%mm1;\n"
			" paddb		%%mm1,%%mm0;\n"
			" paddb		%%mm2,%%mm0;\n"
			" movq		0*768+4*128+0(%4),%%mm2;\n"
			" movq		%%mm0,%%mm1;\n"
			" punpcklbw	%%mm5,%%mm0;\n"
			" movq		%%mm0,2*768+4*128+0(%2);\n"
			" psubw		%%mm0,%%mm2;\n"
			" movq		%%mm2,0*768+4*128+0(%2);\n"
			" movq		(%0,%1),%%mm0;\n"
			" movq		0*768+4*128+8(%4),%%mm2;\n"
			" punpckhbw	%%mm5,%%mm1;\n"
			" movq		%%mm1,2*768+4*128+8(%2);\n"
			" psubw		%%mm1,%%mm2;\n"
			" movq		%%mm2,0*768+4*128+8(%2);\n"
			" movq		%%mm4,%%mm1;\n"
			" pxor		%%mm0,%%mm1;\n"
			" movq		%%mm0,%%mm2;\n"
			" pand		%%mm6,%%mm1;\n"
			" pxor		%%mm1,%%mm2;\n"
			" pxor		%%mm1,%%mm4;\n"
			" movq		%%mm0,%%mm1;\n"
			" por		%%mm2,%%mm0;\n"
			" por		%%mm7,%%mm2;\n"
			" por		%%mm7,%%mm1;\n"
			" psrlq		$1,%%mm2;\n"
			" pand		%%mm7,%%mm0;\n"
			" psrlq		$1,%%mm1;\n"
			" paddb		%%mm1,%%mm0;\n"
			" paddb		%%mm2,%%mm0;\n"
			" movq		0*768+5*128+0(%4),%%mm2;\n"
			" movq		%%mm0,%%mm1;\n"
			" punpcklbw	%%mm5,%%mm0;\n"
			" movq		%%mm0,2*768+5*128+0(%2);\n"
			" psubw		%%mm0,%%mm2;\n"
			" movq		%%mm2,0*768+5*128+0(%2);\n"
			" movq		0*768+5*128+8(%4),%%mm2;\n"
			" punpckhbw	%%mm5,%%mm1;\n"
			" movq		%%mm1,2*768+5*128+8(%2);\n"
			" psubw		%%mm1,%%mm2;\n"
			" movq		%%mm2,0*768+5*128+8(%2);\n"

		:: "r" (p), "r" (mb_address.block[5].offset),
		     "r" (&dest[0][i][0]), "r" (0),
		     "r" (&mblock[0][0][i][0]));

		p += mb_address.block[4].pitch;
	}
	}

	pr_end(53);

	return s;
}

static inline unsigned int
tmp_search(int *dhx, int *dhy, unsigned char *from,
       int x, int y, int range, short dest[6][8][8],
       int cpu_type)
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

	if (__builtin_expect(x0 < 0, 0)) {
		x0 = 0;
		x1 = hrange;
	} else if (__builtin_expect(x1 > mb_last_col * 16, 0)) {
		x1 = mb_last_col * 16;
		x0 = x1 - hrange;
	}
				if (__builtin_expect(y0 < 0, 0)) {
					y0 = 0;
					y1 = vrange;
				} else if (__builtin_expect(y1 > mb_last_row * 16, 0)) {
					y1 = mb_last_row * 16;
					y0 = y1 - vrange;
				}
/*
	if (!((x1 - x0) == 4 || ((x1 - x0) & 7) == 0)) {
		fprintf(stderr, "x0=%d x1=%d hrange=%d range=%d\n",
			x0, x1, hrange, range);
	}
	assert((x1 - x0) == 4 || ((x1 - x0) & 7) == 0);
*/
	bbmin = MMXRW(0xFFFE - 0x8000);
	bbdxy = MMXRW(0x0000);

#if 0
	{
		extern void mmx_emu_setverbose(int);

		mmx_emu_setverbose(0);
	}
#endif

	switch (cpu_type) {
	case CPU_PENTIUM_4:
#if USE_SSE2
		sse2_load_pref(tbuf);
		break;
#endif
	default:
		mmx_load_pref(tbuf);
		break;
	}

	p = from + mm_buf_offs + y0 * mb_address.block[0].pitch;

	for (k = 0; k < 4; k++) {
		crdy0.b[k * 2 + 0] = x0 - x + k - 4;
		crdy0.b[k * 2 + 1] = y0 - y - 1;
	}

	if (__builtin_expect((x1 - x0) > 4, 1))
		for (j = y0; j < y1; p += mb_address.block[0].pitch, j++) {
			asm volatile (
				" movq		crdy0,%mm0;\n"
				" paddb		c256,%mm0;\n"
				" movq		%mm0,crdy0;\n"
				" movq		%mm0,crdxy;\n"
			);

			for (i = x0; i < x1; i += 8)
				switch (cpu_type) {
				case CPU_PENTIUM_III:
				case CPU_PENTIUM_4:
					sse_psse_8(tbuf, p + i, mb_address.block[0].pitch);
					break;

				default:
					mmx_psse_8(tbuf, p + i, mb_address.block[0].pitch);
					break;
				}
		}
	else
		for (j = y0; j < y1; p += mb_address.block[0].pitch, j++) {
			asm volatile (
				" movq		crdy0,%mm0;\n"
				" paddb		c256,%mm0;\n"
				" movq		%mm0,crdy0;\n"
				" movq		%mm0,crdxy;\n"
			);

			mmx_psse_4(tbuf, p, mb_address.block[0].pitch);
		}

	p = from + x + y * mb_address.block[0].pitch;

	switch (cpu_type) {
	case CPU_PENTIUM_4:
#if USE_SSE2
		mmx_load_ref(tbuf);
		min = sse2_sad(tbuf, p, mb_address.block[0].pitch);
		break;
#endif
	case CPU_PENTIUM_III:
		mmx_load_ref(tbuf);
		min = sse_sad(tbuf, p, mb_address.block[0].pitch);
		break;

	default:
		mmx_load_ref(tbuf);
		min = mmx_sad(tbuf, p, mb_address.block[0].pitch);
		break;
	}

	min -= (min >> 3);
	dx = 0;			dy = 0;

	for (i = 0; i < 4; i++) {
		switch (cpu_type) {
		case CPU_PENTIUM_4:
#if USE_SSE2
			act = sse2_sad(tbuf,
				p + bbdxy.b[i * 2 + 0] /* x */ 
				+ bbdxy.b[i * 2 + 1] * mb_address.block[0].pitch,
				mb_address.block[0].pitch);
			break;
#endif
		case CPU_PENTIUM_III:
			act = sse_sad(tbuf,
				p + bbdxy.b[i * 2 + 0] /* x */ 
				+ bbdxy.b[i * 2 + 1] * mb_address.block[0].pitch,
				mb_address.block[0].pitch);
			break;

		default:
			act = mmx_sad(tbuf,
				p + bbdxy.b[i * 2 + 0] /* x */ 
				+ bbdxy.b[i * 2 + 1] * mb_address.block[0].pitch,
				mb_address.block[0].pitch);
			break;
		}

		if (act < min) {
			min = act;
			dx = bbdxy.b[i * 2 + 0];
			dy = bbdxy.b[i * 2 + 1];
		}
	}

	*dhx = dx * 2;		*dhy = dy * 2;

#if TEST11
	if (__builtin_expect(min < (16 * 256) && !(dx | dy), 0)) {

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

	switch (cpu_type) {
	case CPU_K6_2:
		_3dn_load_interp(from, mb_address.block[0].pitch,
			(x + dx - 1) >> 1, (y + dy - 1) >> 1);
		break;

	case CPU_PENTIUM_III:
	case CPU_PENTIUM_4:
		sse_load_interp(from, mb_address.block[0].pitch,
			(x + dx - 1) >> 1, (y + dy - 1) >> 1);
		break;

	default:
		mmx_load_interp(from, mb_address.block[0].pitch,
			(x + dx - 1) >> 1, (y + dy - 1) >> 1);
		break;
	}

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
	if (__builtin_expect((dx | dy) & 1, 0)) {
		// act = sad1(tbuf, *pat1, iright, idown); mini[1][1] = act;
		switch (cpu_type) {
		case CPU_PENTIUM_III:
		case CPU_PENTIUM_4:
			act = sse_sad2h(tbuf, *pat1, idown, &act2);
			break;

		default:
			act = mmx_sad2h(tbuf, *pat1, idown, &act2);
			break;
		}
	}

	switch (cpu_type) {
	case CPU_PENTIUM_III:
	case CPU_PENTIUM_4:
		act = sse_sad2h(tbuf, *pat3, idown, &act2); mini[1][0] = act; mini[1][2] = act2;
		act = sse_sad2h(tbuf, *pat4, 0, &act2); mini[0][0] = act; mini[0][2] = act2;
		act = sse_sad2h(tbuf, *pat4, 1, &act2); mini[2][0] = act; mini[2][2] = act2;
		act = sse_sad2v(tbuf, *pat2, iright, &act2); mini[0][1] = act; mini[2][1] = act2;
		break;

	default:
		act = mmx_sad2h(tbuf, *pat3, idown, &act2); mini[1][0] = act; mini[1][2] = act2;
		act = mmx_sad2h(tbuf, *pat4, 0, &act2); mini[0][0] = act; mini[0][2] = act2;
		act = mmx_sad2h(tbuf, *pat4, 1, &act2); mini[2][0] = act; mini[2][2] = act2;
		act = mmx_sad2v(tbuf, *pat2, iright, &act2); mini[0][1] = act; mini[2][1] = act2;
		break;
	}

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

unsigned int
mmx_search(int *dhx, int *dhy, unsigned char *from,
       int x, int y, int range, short dest[6][8][8])
{
	return tmp_search(dhx, dhy, from, x, y, range, dest, CPU_PENTIUM_MMX);
}

unsigned int
_3dn_search(int *dhx, int *dhy, unsigned char *from,
       int x, int y, int range, short dest[6][8][8])
{
	return tmp_search(dhx, dhy, from, x, y, range, dest, CPU_K6_2);
}

unsigned int
sse_search(int *dhx, int *dhy, unsigned char *from,
       int x, int y, int range, short dest[6][8][8])
{
	return tmp_search(dhx, dhy, from, x, y, range, dest, CPU_PENTIUM_III);
}

#if USE_SSE2

unsigned int
sse2_search(int *dhx, int *dhy, unsigned char *from,
       int x, int y, int range, short dest[6][8][8])
{
	return tmp_search(dhx, dhy, from, x, y, range, dest, CPU_PENTIUM_4);
}

#endif

static unsigned int
t4_edu(unsigned char *ref, int *dxp, int *dyp, int sx, int sy,
	int src_range, int max_range, short dest[6][8][8])
{
//	struct motion M;
	int x, y, xs, ys;
	int x0, y0, x1, y1;
	int hrange, vrange;
	unsigned int s;

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
	q = 1.4 * m / dist;

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
//	pdx[mb_row][mb_col] = 127;
}

unsigned int
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

//	pdx[mb_row][mb_col] = *pmx;
//	pdy[mb_row][mb_col] = *pmy;
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

unsigned int
predict_bidirectional_motion(struct motion *M,
	unsigned int *vmc1, unsigned int *vmc2, int bdist /* forward */)
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

	asm volatile (
		" movq		c1,%mm5;\n"
		" pxor		%mm7,%mm7;\n"
	);	

	for (j = i = 0; j < 16 * 16; j += 16, i += 4) {
		asm volatile (
			" movq		3*768+0*128+0(%0),%%mm0;\n"
			" paddw		4*768+0*128+0(%0),%%mm0;\n"
			" paddw		%%mm5,%%mm0;\n"
			" psrlw		$1,%%mm0;\n"
			" movq		0*768+0*128+0(%0),%%mm2;\n"
			" psubw		%%mm0,%%mm2;\n"
			" movq		%%mm2,3*768+0*128+0(%0);\n"
			" pmaddwd	%%mm2,%%mm2;\n"
			" paddd		%%mm2,%%mm7;\n"

			" movq		3*768+0*128+8(%0),%%mm0;\n"
			" paddw		4*768+0*128+8(%0),%%mm0;\n"
			" paddw		%%mm5,%%mm0;\n"
			" psrlw		$1,%%mm0;\n"
			" movq		0*768+0*128+8(%0),%%mm2;\n"
			" psubw		%%mm0,%%mm2;\n"
			" movq		%%mm2,3*768+0*128+8(%0);\n"
			" pmaddwd	%%mm2,%%mm2;\n"
			" paddd		%%mm2,%%mm7;\n"

			" movq		3*768+0*128+16(%0),%%mm0;\n"
			" paddw		4*768+0*128+16(%0),%%mm0;\n"
			" paddw		%%mm5,%%mm0;\n"
			" psrlw		$1,%%mm0;\n"
			" movq		0*768+0*128+16(%0),%%mm2;\n"
			" psubw		%%mm0,%%mm2;\n"
			" movq		%%mm2,3*768+0*128+16(%0);\n"
			" pmaddwd	%%mm2,%%mm2;\n"
			" paddd		%%mm2,%%mm7;\n"

			" movq		3*768+0*128+24(%0),%%mm0;\n"
			" paddw		4*768+0*128+24(%0),%%mm0;\n"
			" paddw		%%mm5,%%mm0;\n"
			" psrlw		$1,%%mm0;\n"
			" movq		0*768+0*128+24(%0),%%mm2;\n"
			" psubw		%%mm0,%%mm2;\n"
			" movq		%%mm2,3*768+0*128+24(%0);\n"
			" pmaddwd	%%mm2,%%mm2;\n"
			" paddd		%%mm2,%%mm7;\n"

			" movq		3*768+4*128+0(%1),%%mm0;\n"
			" paddw		4*768+4*128+0(%1),%%mm0;\n"
			" paddw		%%mm5,%%mm0;\n"
			" psrlw		$1,%%mm0;\n"
			" movq		0*768+4*128+0(%1),%%mm2;\n"
			" psubw		%%mm0,%%mm2;\n"
			" movq		%%mm2,3*768+4*128+0(%1);\n"

			" movq		3*768+5*128+0(%1),%%mm0;\n"
			" paddw		4*768+5*128+0(%1),%%mm0;\n"
			" paddw		%%mm5,%%mm0;\n"
			" psrlw		$1,%%mm0;\n"
			" movq		0*768+5*128+0(%1),%%mm2;\n"
			" psubw		%%mm0,%%mm2;\n"
			" movq		%%mm2,3*768+5*128+0(%1);\n"

		:: "r" (&mblock[0][0][0][j]), "r" (&mblock[0][0][0][i]));
	}

	asm volatile (
		" movq		%%mm7,%%mm0;\n"
		" psrlq		$32,%%mm7;\n"
		" paddd		%%mm0,%%mm7;\n"
		" pslld		$8,%%mm7;\n"
		" movd		%%mm7,%0;\n"
	: "=&r" (si) : "r" (0));

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
unsigned int
predict_forward_packed(unsigned char *from)
{
	int i, n, s2 = 0;

	for (i = 0; i < 4 * 64; i++) {
		mblock[1][0][0][i] = n = mblock[0][0][0][i] - from[i];
		mblock[3][0][0][i] = from[i];
		s2 += n * n;
	}

	for (; i < 6 * 64; i++) {
		mblock[1][0][0][i] = mblock[0][0][0][i] - from[i];
		mblock[3][0][0][i] = from[i];
	}

	return s2 * 256;
}

unsigned int
predict_forward_planar(unsigned char *from)
{
	int i, j, n, s2 = 0;
	unsigned char *p;

	p = from;

	for (i = 0; i < 16; i++) {
		for (j = 0; j < 8; j++) {
			mblock[1][0][i][j] = n = mblock[0][0][i][j] - p[j];
			mblock[3][0][i][j] = p[j];
			s2 += n * n;
			mblock[1][0][i + 16][j] = n = mblock[0][0][i + 16][j] - p[j + 8];
			mblock[3][0][i + 16][j] = p[j + 8];
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

	return s2 * 256;
}

/*
 *  Backward prediction (B frames only, no reconstruction)
 *
 *  mblock[1] = org - new_ref;
 */
unsigned int
predict_backward_packed(unsigned char *from)
{
	int i, n, s2 = 0;

	for (i = 0; i < 4 * 64; i++) {
		mblock[1][0][0][i] = n = mblock[0][0][0][i] - from[i];
		s2 += n * n;
	}

	for (; i < 6 * 64; i++)
		mblock[1][0][0][i] = mblock[0][0][0][i] - from[i];

	return s2 * 256;
}

/*
 *  Bidirectional prediction (B frames only, no reconstruction)
 *
 *  mblock[1] = org - old_ref;
 *  mblock[2] = org - new_ref;
 *  mblock[3] = org - linear_interpolation(old_ref, new_ref);
 */
unsigned int
predict_bidirectional_packed(unsigned char *from1, unsigned char *from2,
	unsigned int *vmc1, unsigned int *vmc2)
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

	*vmc1 = sf * 256;
	*vmc2 = sb * 256;

	return si * 256;
}

unsigned int
predict_bidirectional_planar(unsigned char *from1, unsigned char *from2,
	unsigned int *vmc1, unsigned int *vmc2)
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

	*vmc1 = sf * 256;
	*vmc2 = sb * 256;

	return si * 256;
}
