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

/* $Id: mpeg.h,v 1.2 2001-06-05 17:52:08 mschimek Exp $ */

#define PICTURE_START_CODE		0x00000100L
#define SLICE_START_CODE		0x00000101L
#define USER_DATA_START_CODE		0x000001B2L
#define SEQUENCE_HEADER_CODE		0x000001B3L
#define SEQUENCE_ERROR_CODE		0x000001B4L
#define EXTENSION_START_CODE		0x000001B5L
#define SEQUENCE_END_CODE		0x000001B7L
#define GROUP_START_CODE		0x000001B8L

#define I_TYPE				1
#define P_TYPE				2
#define B_TYPE				3
#define D_TYPE				4

#define SEQUENCE_EXTENSION_ID			1
#define SEQUENCE_DISPLAY_EXTENSION_ID		2
#define QUANT_MATRIX_EXTENSION_ID		3
#define COPYRIGHT_EXTENSION_ID			4
#define SEQUENCE_SCALABLE_EXTENSION_ID		5
#define PICTURE_DISPLAY_EXTENSION_ID		7
#define PICTURE_CODING_EXTENSION_ID		8
#define PICTURE_SPATIAL_SCALABLE_EXTENSION_ID	9
#define PICTURE_TEMPORAL_SCALABLE_EXTENSION_ID	10

typedef enum {
	MB_INTRA,
	MB_FORWARD,
	MB_BACKWARD,
	MB_INTERP
} mb_type;

/* tables.c */

extern const double frame_rate_value[16];
extern const unsigned char default_intra_quant_matrix[8][8];
extern const unsigned char default_inter_quant_matrix[8][8];
extern const unsigned char scan[2][8][8];
extern const unsigned char quantiser_scale[2][32];
extern const unsigned long long macroblock_address_increment_vlc[33];
extern const unsigned long long coded_block_pattern_vlc[64];
extern const unsigned long long motion_code_vlc[17];
extern const unsigned long long dct_dc_size_luma_vlc[12];
extern const unsigned long long dct_dc_size_chroma_vlc[12];

extern int vlc(unsigned long long, unsigned int *);
extern int dct_coeff_vlc(int table, int run, int level, unsigned int *);
