/*
 *  MPEG-1 Real Time Encoder
 *
 *  Copyright (C) 1999-2000 Michael H. Schimek
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

/* $Id: vlc.h,v 1.1 2000-07-05 18:09:34 mschimek Exp $ */

typedef struct {
	unsigned char		code;
	unsigned char		length;
} VLC2;

typedef struct {
	unsigned short		code;
	unsigned char		length, mv_length;
} VLC4;

typedef struct {
	unsigned int		code;
	unsigned int		length;
} VLC8;

extern int		dc_dct_pred[2][3];
extern int		PMV[2][2];

extern VLC2		coded_block_pattern[64];
extern VLC2		macroblock_address_increment[33];
extern VLC2		motion_vector_component[128];
extern VLC4		macroblock_type_b_nomc_quant[4];
extern VLC2		macroblock_type_b_nomc[4];
extern VLC2		macroblock_type_b_nomc_notc[4];
extern VLC2		macroblock_type_b_quant[4];
extern unsigned char	iscan[8][8];
extern VLC8		dc_vlc_intra[5][12];
extern VLC2		ac_vlc_zero[176];
extern VLC2		ac_vlc_one[176];

extern void		vlc_init(void);

extern int		mpeg1_encode_intra(void);
extern int		mpeg1_encode_inter(short mblock[6][8][8], unsigned int cbp);
extern int		mpeg2_encode_intra(void);
extern int		mpeg2_encode_inter(short mblock[6][8][8], unsigned int cbp);

extern int		mmx_mpeg1_encode_intra(void);
extern int		mmx_mpeg1_encode_inter(short mblock[6][8][8], unsigned int cbp);

static inline
void reset_dct_pred(void)
{
	dc_dct_pred[0][0] = 0;
	dc_dct_pred[0][1] = 0;
	dc_dct_pred[0][2] = 0;
}
