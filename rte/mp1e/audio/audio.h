/*
 *  MPEG-1 Real Time Encoder
 *
 *  Copyright (C) 1999, 2000, 2001, 2002 Michael H. Schimek
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

/* $Id: audio.h,v 1.16 2002-12-14 00:43:43 mschimek Exp $ */

#include <inttypes.h>
#include <pthread.h>
#include "../common/bstream.h"

#include "libaudio.h"
#include "mpeg.h"

#define BLKSIZE 1024
#define HBLKSIZE 513
#define CBANDS 63
#define e_save_new e_save_oldest

typedef struct mp2_context {
	/* Buffers */

	uint8_t			wrap[(SAMPLES_PER_FRAME + 512 - 32) * 4];

	int			sb_samples[2][3][12][SBLIMIT];	/* channel, scale group, sample */
	int8_t			bit_alloc[2][SBLIMIT];
	struct {
		uint8_t			mant, exp;
	}			scalar[2][3][SBLIMIT];		/* channel, scale group */
	union {
		unsigned int		scf[2][SBLIMIT];	/* scaling factors */
		mmx_t			fb_temp[17];		/* filterbank temp */
	}			sf;
	int8_t			scfsi[2][SBLIMIT];		/* scale factor selector information */
	float			mnr[2][SBLIMIT];
	int			sblimit;
	int			sum_nbal;

	struct bs_rec		out;

	/* Tables */

	uint8_t			sb_group[SBLIMIT];
	uint8_t			bits[4][MAX_BA_INDICES];
	struct {
		uint8_t		quant, shift;
	}			qs[4][MAX_BA_INDICES];
	uint8_t			nbal[SBLIMIT];
	uint16_t		bit_incr[8][MAX_BA_INDICES];
	float			mnr_incr[8][SBLIMIT];
	uint8_t			steps[4];
	uint8_t			packed[4];

	/* Psycho buffers */

	FLOAT			h_save[2][3][BLKSIZE];
	FLOAT			e_save[2][2][HBLKSIZE];
	struct {
		FLOAT			e, c;
	}			grouped[CBANDS];
	FLOAT			nb[HBLKSIZE];
	FLOAT			sum_energy[SBLIMIT];

	/* Psycho tables */

	int8_t			partition[HBLKSIZE];		/* frequency line to cband mapping */
	struct {
		int8_t			off, cnt;
	}			s_limits[CBANDS];
	FLOAT			s_packed[2048];			/* packed spreading function */
	FLOAT			absthres[HBLKSIZE];		/* absolute thresholds, linear */
	FLOAT			xnorm[CBANDS];
	FLOAT			p1[CBANDS];
	FLOAT			p2[CBANDS];
	FLOAT			p3[CBANDS - 20];
	FLOAT			p4[CBANDS - 20];

	int			ch;
	FLOAT *			e_save_old;
	FLOAT *			e_save_oldest;
	FLOAT *			h_save_new;
	FLOAT *			h_save_old;
	FLOAT *			h_save_oldest;

	int			psycho_loops;

	/* Input */

	consumer		cons;
	buffer *		ibuf;
	unsigned int		i16, e16;
	int			incr;
	double			nominal_time_elapsed;
	double			nominal_sample_period;
	uint32_t		format_sign;
	rte_bool		format_scale;

	/* Output */

	producer		prod;
	unsigned int		header_template;
	double			coded_frame_period;
	double			coded_frames_countd;
	int			sampling_freq;
	int			bits_per_frame;
	int			spf_rest, spf_lag;

	/* Options */

	mp1e_codec		codec;

	int			mpeg_version;
	int			sampling_freq_code;
	int			bit_rate_code;
	int			audio_mode;
	double			num_frames;

} mp2_context;

/* psycho.c */

extern void		mp1e_mp2_psycho_init(mp2_context *, int sampling_freq);
extern void		mp1e_mp2_psycho(mp2_context *, short *, float *, int);

/* fft.c */

extern void		mp1e_mp2_fft_init(int test);
extern void		mp1e_mp2_fft_step_1(short *, FLOAT *);
extern void		mp1e_mp2_fft_step_2(short *, FLOAT *);

/* filter.c */

extern void		mp1e_mp2_subband_filter_init(int test);

extern void		mp1e_mp2_mmx_window_mono(short *z, mmx_t *) __attribute__ ((regparm (2)));
extern void		mp1e_mp2_mmx_window_left(short *z, mmx_t *) __attribute__ ((regparm (2)));
extern void		mp1e_mp2_mmx_window_right(short *z, mmx_t *) __attribute__ ((regparm (2)));
extern void		mp1e_mp2_mmx_filterbank(int *, mmx_t *) __attribute__ ((regparm (2)));

static inline void
mmx_filter_mono(mp2_context *mp2, short *p, int *samples)
{
	int j;

	mp2->sf.fb_temp[0].d[0] = 0L;
	mp2->sf.fb_temp[0].d[1] = -1L;
	mp2->sf.fb_temp[1].d[0] = 32768L;
	mp2->sf.fb_temp[1].d[1] = 32768L;

	for (j = 0; j < 3 * SCALE_BLOCK; j++, p += 32, samples += 32) {
		mp1e_mp2_mmx_window_mono(p, mp2->sf.fb_temp);
		mp1e_mp2_mmx_filterbank(samples, mp2->sf.fb_temp);
	}

	emms();
}

static inline void
mmx_filter_stereo(mp2_context *mp2, short *p, int *samples)
{
	int j;

	mp2->sf.fb_temp[0].d[0] = 0L;
	mp2->sf.fb_temp[0].d[1] = -1L;
	mp2->sf.fb_temp[1].d[0] = 32768L;
	mp2->sf.fb_temp[1].d[1] = 32768L;

	for (j = 0; j < 3 * SCALE_BLOCK; j++, p += 32 * 2, samples += 32) {
		/*
		 *  Subband window code could be optimized,
		 *  I've just adapted the mono version.
		 */
		mp1e_mp2_mmx_window_left(p, mp2->sf.fb_temp);
		mp1e_mp2_mmx_filterbank(samples, mp2->sf.fb_temp);

		mp1e_mp2_mmx_window_right(p, mp2->sf.fb_temp);
		mp1e_mp2_mmx_filterbank(samples + 3 * SCALE_BLOCK * 32, mp2->sf.fb_temp);
	}

	emms();
}
