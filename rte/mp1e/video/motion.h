/*
 *  MPEG-1 Real Time Encoder
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

/* $Id: motion.h,v 1.1.1.1 2001-08-07 22:10:09 garetxe Exp $ */

#ifndef MOTION_H
#define MOTION_H

#include "vlc.h"
#include "mblock.h"

#define reg(n) __attribute__ ((regparm (n)))

extern int		motion;
extern int		mm_buf_offs;

/* motion.c */

typedef unsigned int (search_fn)(int *dhx, int *dhy, unsigned char *from,
			int x, int y, int range, short dest[6][8][8]);

extern search_fn	mmx_search, _3dn_search, sse_search, sse2_search;
extern search_fn *	search;

extern unsigned int	predict_forward_packed(unsigned char *from) reg(1);
extern unsigned int	predict_forward_planar(unsigned char *from) reg(1);
extern unsigned int	predict_backward_packed(unsigned char *from) reg(1);
extern unsigned int	predict_bidirectional_packed(unsigned char *from1, unsigned char *from2, unsigned int *vmc1, unsigned int *vmc2);
extern unsigned int	predict_bidirectional_planar(unsigned char *from1, unsigned char *from2, unsigned int *vmc1, unsigned int *vmc2);

extern unsigned int	predict_forward_motion(struct motion *M, unsigned char *, int);
extern unsigned int	predict_bidirectional_motion(struct motion *M, unsigned int *, unsigned int *, int);

extern void		zero_forward_motion(void);
extern void		t7(int range, int dist);

/* motion_mmx.s */

/*
 *  NB we use mmx_predict_forward also for backward prediction (in B pictures
 *  within a closed gop, low profile) discarding the reconstruction.
 *  No mmx_predict_bidi_planar, use reference version.
 */
extern unsigned int	mmx_predict_forward_packed(unsigned char *) reg(1);
extern unsigned int	mmx_predict_forward_planar(unsigned char *) reg(1);
extern unsigned int	mmx_predict_bidirectional_packed(unsigned char *from1, unsigned char *from2, unsigned int *vmc1, unsigned int *vmc2);

/*
 *  Attention mmx_mbsum uses mblock[4] as permanent scratch in picture_i|p();
 *  Source mblock[0], dest mm_mbrow and bp
 */
extern void		mmx_mbsum(char * /* eax */) reg(1);
extern int		mmx_sad(unsigned char t[16][16] /* eax */, unsigned char *p /* edx */, int pitch /* ecx */) reg(3);
extern int		sse_sad(unsigned char t[16][16] /* eax */, unsigned char *p /* edx */, int pitch /* ecx */) reg(3);
/* <t> must be 16 byte aligned */
extern int		sse2_sad(unsigned char t[16][16] /* eax */, unsigned char *p /* edx */, int pitch /* ecx */) reg(3);

#endif /* MOTION_H */
