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

/* $Id: vlc.h,v 1.7 2002-09-12 12:24:49 mschimek Exp $ */

#ifndef VLC_H
#define VLC_H

#include "../common/math.h"
#include "../common/bstream.h"
#include "video.h"

typedef struct {
	uint8_t			code;
	uint8_t			length;
} VLC2;

typedef struct {
	unsigned 		code : 12;
	unsigned 		length : 4;
} VLCM;

typedef struct {
	uint16_t		code;
	uint8_t			length, mv_length;
} VLC4;

typedef struct {
	uint32_t		code;
	uint32_t		length;
} VLC8;

/* Attn asm depends on order */
typedef struct vlc_rec {
	struct bs_rec		bstream;
	int			dc_dct_pred[4];
} vlc_rec;

static inline void
reset_dct_pred			(vlc_rec *		vr)
{
	vr->dc_dct_pred[0] = 0;
	vr->dc_dct_pred[1] = 0;
	vr->dc_dct_pred[2] = 0;
}

extern const VLC2	mp1e_coded_block_pattern[64];
extern const VLC2	mp1e_macroblock_address_increment[33];
extern const VLCM	mp1e_motion_vector_component[480];
extern const uint8_t	mp1e_iscan0[8][8];
extern const uint8_t	mp1e_iscan1[8][8];
extern const VLC4	mp1e_macroblock_type_b_nomc_quant[4];
extern const VLC2	mp1e_macroblock_type_b_nomc[4];
extern const VLC2	mp1e_macroblock_type_b_nomc_notc[4];
extern const VLC2	mp1e_macroblock_type_b_quant[4];

typedef int mp1e_encode_intra_fn (vlc_rec *		vr,
				  const int16_t		mblock[6][8][8],
				  const uint8_t		iscan[8][8],
				  unsigned int		count);

typedef int mp1e_encode_inter_fn (const int16_t		mblock[6][8][8],
				  unsigned int		cbp);

/* Reference */

extern mp1e_encode_intra_fn mp1e_mpeg1_encode_intra;
extern mp1e_encode_inter_fn mp1e_mpeg1_encode_inter;
extern mp1e_encode_intra_fn mp1e_mpeg2_encode_intra_14;
extern mp1e_encode_intra_fn mp1e_mpeg2_encode_intra_15;
extern mp1e_encode_inter_fn mp1e_mpeg2_encode_inter_14;

extern mp1e_encode_intra_fn mp1e_p6_mpeg1_encode_intra;
extern mp1e_encode_inter_fn mp1e_p6_mpeg1_encode_inter;
extern mp1e_encode_intra_fn mp1e_p6_mpeg2_encode_intra_14;
extern mp1e_encode_intra_fn mp1e_p6_mpeg2_encode_intra_15;
extern mp1e_encode_inter_fn mp1e_p6_mpeg2_encode_inter_14;

#define F_CODE_MIN 1
#define F_CODE_MAX 4

struct motion {
	const VLCM *		vlc;
	int			f_code;
	int			f_mask;
	int			src_range;
	int			max_range;

	int			PMV[2], MV[2];
};

static inline void
motion_init(mpeg1_context *mpeg1, struct motion *m, int range)
{
	int f;

	range = saturate(range, mpeg1->motion_min, mpeg1->motion_max);
	f = saturate(ffsr(range - 1) - 1, F_CODE_MIN, F_CODE_MAX);
	m->max_range = 4 << f;
	m->src_range = saturate(range, 4, 4 << f);
	m->f_mask = 0xFF >> (4 - f);
	m->f_code = f;

	m->vlc = mp1e_motion_vector_component + ((15 << f) & 480);
	/* = mp1e_motion_vector_component + ((1 << (f - 1)) - 1) * 32; */

	m->PMV[0] = 0;
	m->PMV[1] = 0;
}

#endif /* VLC_H */
