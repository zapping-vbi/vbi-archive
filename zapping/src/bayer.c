/*
 *  Zapping TV viewer
 *
 *  Copyright (C) 2004 Michael H. Schimek
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

/* $Id: bayer.c,v 1.2 2004-12-11 11:46:24 mschimek Exp $ */

#include <inttypes.h>
#include "bayer.h"

#define AVG2(a,b) (((unsigned int)(a) + (b) + 1) / 2)
#define AVG3(a,b,c) (((unsigned int)(a) + (b) + (c) + 1) / 3)
#define AVG4(a,b,c,d) (((unsigned int)(a) + (b) + (c) + (d) + 2) / 4)

#define LOOP								\
    /* first column */							\
    PIXEL (s[+w+1], AVG2 (s[+1],s[+w]), s[0]);				\
    /* first line */							\
    for (hcount = (w - 2) / 2; hcount > 0; --hcount) {			\
      PIXEL (s[+w], s[0], AVG2 (s[-1],s[+1]));				\
      PIXEL (AVG2 (s[+w-1],s[+w+1]), AVG3 (s[-1],s[+1],s[+w]), s[0]);	\
    }									\
    /* last column */							\
    PIXEL (s[+w], s[0], s[-1]);						\
									\
    for (vcount = (h - 2) / 2; vcount > 0; --vcount) {			\
									\
      /* first column */						\
      PIXEL (s[1], s[0], AVG2 (s[-w],s[+w]));				\
      /* odd line */							\
      for (hcount = (w - 2) / 2; hcount > 0; --hcount) {		\
	PIXEL (s[0], AVG4 (s[-w],s[-1],s[+1],s[+w]),			\
	       AVG4 (s[-w-1],s[-w+1],s[+w-1],s[+w+1]));			\
	PIXEL (AVG2 (s[-1],s[+1]), s[0], AVG2 (s[-w],s[+w]));		\
      }									\
      /* last column */							\
      PIXEL (s[0], AVG3 (s[-w],s[-1],s[+w]), AVG2 (s[-w-1],s[+w-1]));	\
									\
      /* first column */						\
      PIXEL (AVG2 (s[-w+1],s[+w+1]), AVG3 (s[-w],s[+1],s[+w]), s[0]);	\
      /* even line */							\
      for (hcount = (w - 2) / 2; hcount > 0; --hcount) {		\
	PIXEL (AVG2 (s[-w],s[+w]), s[0], AVG2 (s[-1],s[+1]));		\
	PIXEL (AVG4 (s[-w-1],s[-w+1],s[+w-1],s[+w+1]),			\
	       AVG4 (s[-w],s[-1],s[+1],s[+w]), s[0]);			\
      }									\
      /* last column */							\
      PIXEL (AVG2 (s[-w],s[+w]), s[0], s[-1]);				\
    }									\
									\
    /* first column */							\
    PIXEL (s[1], s[0], s[-w]);						\
    /* last line */							\
    for (hcount = (w - 2) / 2; hcount > 0; --hcount) {			\
      PIXEL (s[0], AVG3 (s[-w],s[-1],s[+1]), AVG2 (s[-w-1],s[-w+1]));	\
      PIXEL (AVG2 (s[-1],s[+1]), s[0], s[-w]);				\
    }									\
    /* last column */							\
    PIXEL (s[0], AVG2 (s[-w],s[-1]), s[-w-1]);

#undef PIXEL
#define PIXEL(r,g,b)				\
    d[0] = b;					\
    d[1] = g;					\
    d[2] = r;					\
    d[3] = 0;					\
    d += 4;					\
    ++s;

void
sbggr8_to_bgra32_le		(void *			dst,
				 void *			src,
				 unsigned int		width,
				 unsigned int		height)
{
  uint8_t *s = src, *d = dst;
  unsigned int w = width, h = height;
  unsigned int vcount, hcount;

  LOOP;
}

#undef PIXEL
#define PIXEL(r,g,b)				\
    d[0] = b;					\
    d[1] = g;					\
    d[2] = r;					\
    d += 3;					\
    ++s;

void
sbggr8_to_bgr24_le		(void *			dst,
				 void *			src,
				 unsigned int		width,
				 unsigned int		height)
{
  uint8_t *s = src, *d = dst;
  unsigned int w = width, h = height;
  unsigned int vcount, hcount;

  LOOP;
}

/* rrrrrggg gggbbbbb */
#undef PIXEL
#define PIXEL(r,g,b)				\
    gr = g & 0xFC;				\
    d[0] = (b >> 3) + (gr << 3);		\
    d[1] = (r & 0xF8) + (gr >> 5);		\
    d += 2;					\
    ++s;

void
sbggr8_to_bgr16_le		(void *			dst,
				 void *			src,
				 unsigned int		width,
				 unsigned int		height)
{
  uint8_t *s = src, *d = dst;
  unsigned int w = width, h = height;
  unsigned int vcount, hcount;
  unsigned int gr;

  LOOP;
}
