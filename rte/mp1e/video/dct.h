/*
 *  MPEG-1 Real Time Encoder
 *
 *  Copyright (C) 1999-2000 Michael H. Schimek
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
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

/* $Id: dct.h,v 1.2 2001-08-22 01:28:09 mschimek Exp $ */

#define reg(n) __attribute__ ((regparm (n)))

extern void		fdct_intra(int quant_scale) reg(1);
extern unsigned int	fdct_inter(short iblock[6][8][8], int quant_scale) reg(2);
extern void		mpeg1_idct_intra(int quant_scale) reg(1);
extern void		mpeg1_idct_inter(int quant_scale, unsigned int cbp) reg(2);

extern void		mmx_fdct_intra(int quant_scale) reg(1);
extern unsigned int	mmx_fdct_inter(short iblock[6][8][8], int quant_scale) reg(2);
extern void		mmx_mpeg1_idct_intra(int quant_scale) reg(1);
extern void		mmx_mpeg1_idct_intra2(int quant_scale) reg(1);
extern void		mmx_mpeg1_idct_inter(int quant_scale, unsigned int cbp) reg(2);

extern void		mmx_copy_refblock(void);
