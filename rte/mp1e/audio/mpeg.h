/*
 *  MPEG Real Time Encoder
 *  MPEG-1/2 Audio Layer II Definitions
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

/* $Id: mpeg.h,v 1.2 2001-08-22 01:28:07 mschimek Exp $ */

#define MPEG_VERSION_1           	3 // ISO/IEC 11172-3
#define MPEG_VERSION_2			2 // ISO/IEC 13818-3
#define MPEG_VERSION_2_5		0 // not supported
#define MPEG_VERSIONS			4

#define LAYER_II			2

#define AUDIO_MODE_STEREO		0
#define AUDIO_MODE_JOINT_STEREO		1
#define AUDIO_MODE_DUAL_CHANNEL		2
#define AUDIO_MODE_MONO			3

#define TABLES				5
#define SBLIMIT				32
#define MAX_BA_INDICES			16
#define NUM_SG				8

#define GRANULE				96
#define SCALE_BLOCK			12
#define BITS_PER_SLOT			8
#define SAMPLES_PER_FRAME		1152
#define HEADER_BITS			32

struct absthr_rec {
	int			line;	// fft higher line
	float			thr;	// absolute threshold (dB)
};

extern const int		bit_rate_value[MPEG_VERSIONS][16];
extern const int		sampling_freq_value[MPEG_VERSIONS][4];
extern const unsigned char	subband_group[TABLES][SBLIMIT];
extern const unsigned char	bits_table[NUM_SG][MAX_BA_INDICES];
extern const unsigned int	steps_table[NUM_SG][MAX_BA_INDICES];
extern const unsigned char	quant_table[NUM_SG][MAX_BA_INDICES];
extern const unsigned char	pack_table[NUM_SG];

extern const float		SNR[18];
extern const double		C[512];
extern const struct absthr_rec	absthr[6][134];
