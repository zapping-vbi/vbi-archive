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

/* $Id: vlc.h,v 1.7 2001-06-01 20:24:35 mschimek Exp $ */

#ifndef VLC_H
#define VLC_H

#include "../common/math.h"
#include "../options.h"

typedef struct {
	unsigned char		code;
	unsigned char		length;
} VLC2;

typedef struct {
	unsigned 		code : 12;
	unsigned 		length : 4;
} VLCM;

typedef struct {
	unsigned short		code;
	unsigned char		length, mv_length;
} VLC4;

typedef struct {
	unsigned int		code;
	unsigned int		length;
} VLC8;

extern int		dc_dct_pred[2][3];

extern VLC2		coded_block_pattern[64];
extern VLC2		macroblock_address_increment[33];
extern VLCM		motion_vector_component[480];
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

extern int		p6_mpeg1_encode_intra(void);
extern int		p6_mpeg1_encode_inter(short mblock[6][8][8], unsigned int cbp);

static inline
void reset_dct_pred(void)
{
	dc_dct_pred[0][0] = 0;
	dc_dct_pred[0][1] = 0;
	dc_dct_pred[0][2] = 0;
}

#define F_CODE_MIN 1
#define F_CODE_MAX 4

struct motion {
	VLCM *			vlc;
	int			f_code;
	int			f_mask;
	int			src_range;
	int			max_range;

	int			PMV[2], MV[2];
};

static inline void
reset_pmv(struct motion *m)
{
	memset(m->PMV, 0, sizeof(m->PMV));
}

static inline void
motion_init(struct motion *m, int range)
{
	int f;

	range = saturate(range, motion_min, motion_max);
	f = saturate(ffsr(range - 1) - 1, F_CODE_MIN, F_CODE_MAX);
	m->max_range = 4 << f;
	m->src_range = saturate(range, 4, 4 << f);
	m->f_mask = 0xFF >> (4 - f);
	m->f_code = f;

	m->vlc = motion_vector_component + ((15 << f) & 480);
	// = motion_vector_component + ((1 << (f - 1)) - 1) * 32;

	reset_pmv(m);
}

#endif // VLC_H
