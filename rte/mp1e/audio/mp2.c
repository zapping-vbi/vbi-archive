/*
 *  MPEG Real Time Encoder
 *  MPEG-1 Audio Layer II
 *  MPEG-2 Audio Layer II Low Frequency Extensions
 *
 *  Copyright (C) 1999-2001 Michael H. Schimek
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

/* $Id: mp2.c,v 1.17 2001-11-03 23:43:54 mschimek Exp $ */

#include <limits.h>
#include "../common/log.h"
#include "../common/mmx.h"
#include "../common/profile.h"
#include "../common/bstream.h"
#include "../common/math.h"
#include "../common/alloc.h"
#include "audio.h"
#include "../systems/systems.h"
#include "../systems/mpeg.h"

#define SCALE_RANGE 64
#define SCALE 32768

enum {
	S123, S122, S133, S113, S444, S111, S222, S333
};

static const char
spattern[7][8] __attribute__ ((aligned (CACHE_LINE))) = {
	{ S123, S122, S122, S122, S133, S133, S123 },
	{ S113, S111, S111, S111, S444, S444, S113 },
	{ S113, S111, S111, S111, S444, S444, S113 },
	{ S111, S111, S111, S111, S333, S333, S113 },
	{ S222, S222, S222, S222, S333, S333, S123 },
	{ S222, S222, S222, S222, S333, S333, S123 },
	{ S123, S122, S122, S122, S133, S133, S123 }
};

#define M0 2.0
#define M1 1.58740105196820
#define M2 1.25992104989487

static const int
maxr[3] __attribute__ ((aligned (CACHE_LINE))) = {
	((double)(1 << 30) / M0 + 0.5),
	((double)(1 << 30) / M1 + 0.5),
	((double)(1 << 30) / M2 + 0.5)
};

static const unsigned char
sfsPerScfsi[4] __attribute__ ((aligned (4))) = {
	3 * 6, 2 * 6, 1 * 6, 2 * 6
};

/* XXX remove */
extern long long	audio_num_frames;
extern int		aud_buffers;

/*
 *  Audio compression thread
 */

static void
terminate(mp2_context *mp2)
{
	buffer *buf;

	printv(2, "\nAudio: End of file\n");

	buf = wait_empty_buffer(&mp2->prod);
	buf->used = 0;
	send_full_buffer(&mp2->prod, buf);

	pthread_exit(NULL);
}

static inline buffer *
next_buffer(mp2_context *mp2, buffer *buf, int channels, double elapsed)
{
	double period;

	if (buf)
		send_empty_buffer(&mp2->cons, buf);

	mp2->ibuf = buf = wait_full_buffer(&mp2->cons);

	if (buf->used <= 0) {
		send_empty_buffer(&mp2->cons, buf);
		terminate(mp2);
	} else {
		pthread_mutex_lock(&mp2->codec.mutex);
		mp2->codec.frame_input_count++;
		pthread_mutex_unlock(&mp2->codec.mutex);

		assert(buf->used < (1 << 14));

		mp2->i16 -= mp2->e16; /* (samples / channel) << 16 */
		mp2->e16 = buf->used << (16 - channels + mp2->format_scale);

		if (mp2->i16 > mp2->e16)
			mp2->i16 = 0;

		period = buf->used * mp2->sstr.byte_period;

		mp2->incr = lroundn((period + mp1e_sync_drift(&mp2->sstr, buf->time, elapsed))
			/ period * (double)(1 << 16));

		if (mp2->incr < 0)
			mp2->incr = 0;
	}

	return buf;
}

static inline short *
fetch_samples(mp2_context *mp2, int channels)
{
	/* const */ int cs = channels * sizeof(short);
	/* const */ int spf = SAMPLES_PER_FRAME * cs;
	/* const */ int la = (512 - 32) * cs;
	buffer *buf = mp2->ibuf;
	unsigned char *o;
	int todo;

	if (mp2->codec.frame_output_count > audio_num_frames
	    || mp1e_sync_break(&mp2->sstr, buf->time
			       + (mp2->i16 >> 16) * mp2->nominal_sample_period)) {
		send_empty_buffer(&mp2->cons, buf);
		terminate(mp2);
	}

	memcpy(mp2->wrap, mp2->wrap + spf, la);

	o = mp2->wrap + la;

	if (mp2->format_scale)
		for (todo = 0; todo < SAMPLES_PER_FRAME; todo++) {
			if (mp2->i16 >= mp2->e16)
				buf = next_buffer(mp2, buf, channels,
						  mp2->nominal_time_elapsed + todo
						  * mp2->nominal_sample_period);

			/* 8 -> 16 bit machine endian */

			if (channels > 1) {
				uint32_t temp;

				temp = ((uint16_t *) buf->data)[mp2->i16 >> 16] << 8;
				temp = ((temp << 8) | temp) & 0xFF00FF00;

				*((uint32_t *) o)++ = mp2->format_sign ^ temp;
			} else
				*((uint16_t *) o)++ = mp2->format_sign
					^ (((uint8_t *) buf->data)[mp2->i16 >> 16] << 8);

			mp2->i16 += mp2->incr;
		}
	else
		for (todo = 0; todo < SAMPLES_PER_FRAME; todo++) {
			if (mp2->i16 >= mp2->e16)
				buf = next_buffer(mp2, buf, channels,
						  mp2->nominal_time_elapsed + todo
						  * mp2->nominal_sample_period);

			/* 16 -> 16 bit, same endian */

			if (channels > 1)
				*((uint32_t *) o)++ = mp2->format_sign
					^ ((uint32_t *) buf->data)[mp2->i16 >> 16];
			else
				*((uint16_t *) o)++ = mp2->format_sign
					^ ((uint16_t *) buf->data)[mp2->i16 >> 16];

			mp2->i16 += mp2->incr;
		}

	mp2->nominal_time_elapsed += mp2->coded_frame_period;

	return (short *) mp2->wrap;
}

/*
 *  Bits available in current frame
 */
static inline int
adbits(mp2_context *mp2)
{
	int adb = mp2->bits_per_frame;

	if (mp2->spf_lag >= 0) {
		mp2->header_template &= ~(1 << 9);
	} else {
		mp2->header_template |= 1 << 9; /* padded */
		mp2->spf_lag += mp2->sampling_freq;
		adb += BITS_PER_SLOT;
	}

	mp2->spf_lag -= mp2->spf_rest;

	return adb;
}

/*
 *  Calculate scale factors and transmission pattern
 */
static inline void
scale_pattern(mp2_context *mp2, int channels)
{
	int sb;

	for (sb = 0; sb < mp2->sblimit; sb++) {
		int t, ch, s[6];

		for (t = 0; t < channels * 3; t++) {
			unsigned int sx = nbabs(mp2->sb_samples[0][t][0][sb]);
			int j, e, m;

			for (j = 1; j < SCALE_BLOCK; j++) {
				unsigned int s = nbabs(mp2->sb_samples[0][t][j][sb]);

				if (s > sx)
					sx = s;
			}

			sx <<= e = 30 - ffsr(sx | (1 << 10 /* SCALE_RANGE 0..62 */));
			m = (sx < (int)(M1 * (double)(1 << 30) + 0.5)) + 
			    (sx < (int)(M2 * (double)(1 << 30) + 0.5));

			mp2->scalar[0][t][sb].mant = m;
			mp2->scalar[0][t][sb].exp = e;

			s[t] = e * 3 + m;
		}

		for (ch = 0; ch < channels; ch++) {
			int d0, d1;
			int *sp = s + 3 * ch;

			d0 = saturate(sp[0] - sp[1], -3, +3);
			d1 = saturate(sp[1] - sp[2], -3, +3);

			switch (spattern[d0 + 3][d1 + 3]) {
			case S123:  mp2->scfsi[ch][sb] = 0;
				    mp2->sf.scf[ch][sb] = (sp[0] << 12) | (sp[1] << 6) | sp[2];
				    break;

			case S122:  mp2->scfsi[ch][sb] = 3;
				    mp2->sf.scf[ch][sb] = (sp[0] << 6) | sp[1];
				    mp2->scalar[ch][2][sb] = mp2->scalar[ch][1][sb];
				    break;

			case S133:  mp2->scfsi[ch][sb] = 3; mp2->sf.scf[ch][sb] = (sp[0] << 6) | sp[2];
				    mp2->scalar[ch][1][sb] = mp2->scalar[ch][2][sb];
				    break;

			case S113:  mp2->scfsi[ch][sb] = 1; mp2->sf.scf[ch][sb] = (sp[0] << 6) | sp[2];
				    mp2->scalar[ch][1][sb] = mp2->scalar[ch][0][sb];
				    break;

			case S444:  if (sp[0] > sp[2]) {
					    mp2->scfsi[ch][sb] = 2; mp2->sf.scf[ch][sb] = sp[2];
				    	    mp2->scalar[ch][1][sb] = mp2->scalar[ch][0][sb] = mp2->scalar[ch][2][sb];
				    } else {
			case S111:	    mp2->scfsi[ch][sb] = 2; mp2->sf.scf[ch][sb] = sp[0];
					    mp2->scalar[ch][1][sb] = mp2->scalar[ch][2][sb] = mp2->scalar[ch][0][sb];
				    }
				    break;

			case S222:  mp2->scfsi[ch][sb] = 2; mp2->sf.scf[ch][sb] = sp[1];
				    mp2->scalar[ch][0][sb] = mp2->scalar[ch][2][sb] = mp2->scalar[ch][1][sb];
				    break;

			case S333:  mp2->scfsi[ch][sb] = 2; mp2->sf.scf[ch][sb] = sp[2];
				    mp2->scalar[ch][0][sb] = mp2->scalar[ch][1][sb] = mp2->scalar[ch][2][sb];
				    break;
			}
		}
	}

	// DUMP(mp2->scfsi[0], 0, channels * SBLIMIT);
}

/*
 *  Scale and quantize samples
 */
static inline void
scale_quant(mp2_context *mp2, int channels)
{
	int t, j, sb, ba, ch;

	for (t = 0; t < channels * 3; t++) {
		ch = (channels > 1) ? (t >= 3) : 0;

		for (j = 0; j < SCALE_BLOCK; j++)
			for (sb = 0; sb < mp2->sblimit; sb++)
				if ((ba = mp2->bit_alloc[ch][sb])) {
					int sg  = mp2->sb_group[sb];
					int qnt = mp2->qs[sg][ba].quant;
					int n   = mp2->qs[sg][ba].shift;

					// -1.0*2**30 < sb_sample < 1.0*2**30

					int di = ((long long)(mp2->sb_samples[0][t][j][sb] << mp2->scalar[0][t][sb].exp) *
						maxr[(int) mp2->scalar[0][t][sb].mant]) >> 32;

					di += 1 << 28; // += 1.0

					if (qnt < 2)
						mp2->sb_samples[0][t][j][sb] = (di + (di >> (qnt += 2))) >> (29 - n);
					else
						mp2->sb_samples[0][t][j][sb] = (di - (di >> qnt)) >> (28 - n);
				}
	}

	// DUMP(mp2->sb_samples[0][0][0], 0, channels * 3 * SCALE_BLOCK * SBLIMIT);
}

/*
 *  Allocate bits for subbands
 */
static inline void
bit_allocation(mp2_context *mp2, int adb, int channels)
{
	int i, incr, ba, sg, sb, ch = 0;

	memset(mp2->bit_alloc, 0, sizeof(mp2->bit_alloc));

	adb -= HEADER_BITS + mp2->sum_nbal * channels; /* + auxiliary bits */

	for (;;) {
		double min = -1e37;
		sb = -1;

		for (i = 0; i < mp2->sblimit; i++) {
			if (mp2->mnr[0][i] > min) {
				min = mp2->mnr[0][i];
				sb = i;
				ch = 0;
			}

			if (channels > 1 && mp2->mnr[1][i] > min) {
				min = mp2->mnr[1][i];
				sb = i;
				ch = 1;
			}
		}

		if (sb < 0)
			break; /* all done */

		ba = mp2->bit_alloc[ch][sb];
		sg = mp2->sb_group[sb];
		incr = mp2->bit_incr[sg][ba];

		if (ba == 0)
			incr += sfsPerScfsi[(int) mp2->scfsi[ch][sb]] - 3 * 6;

		if (incr <= adb) {
			mp2->bit_alloc[ch][sb] = ba + 1;
			adb -= incr;

			if (mp2->bit_incr[sg][ba + 1] > adb)
				mp2->mnr[ch][sb] = -1e38; /* all bits allocated */
			else
				mp2->mnr[ch][sb] *= mp2->mnr_incr[sg][ba];
		} else
			mp2->mnr[ch][sb] = -1e38; /* can't incr bits for this band */

		// DUMP(mp2->bit_alloc[0], 0, SBLIMIT);
	}
}

/*
 *  Encode bits
 */
static inline void
encode(mp2_context *mp2, unsigned char *buf, int bpf, int channels)
{
	int j, sb = 0;

	bstart(&mp2->out, buf);

	mp2->out.buf = MMXD(0, mp2->header_template);
	mp2->out.n = 32;

	bepilog(&mp2->out);

	/* Bit allocation table */

	for (; sb < mp2->sblimit; sb++) {
		int bi = mp2->nbal[sb];

		if (channels == 1)
			bputl(&mp2->out, mp2->bit_alloc[0][sb], bi);
		else
			bputl(&mp2->out, (mp2->bit_alloc[0][sb] << bi)
					  + mp2->bit_alloc[1][sb], bi * 2);
	}

	/* Scale factor selector information */

	for (sb = 0; sb < mp2->sblimit; sb++) {
    		if (mp2->bit_alloc[0][sb])
			bputl(&mp2->out, mp2->scfsi[0][sb], 2);
    		if (channels > 1 && mp2->bit_alloc[1][sb])
			bputl(&mp2->out, mp2->scfsi[1][sb], 2);
	}

	/* Scale factors */

	for (sb = 0; sb < mp2->sblimit; sb++) {
    		if (mp2->bit_alloc[0][sb])
			bputl(&mp2->out, mp2->sf.scf[0][sb], sfsPerScfsi[(int) mp2->scfsi[0][sb]]);
		if (channels > 1 && mp2->bit_alloc[1][sb])
			bputl(&mp2->out, mp2->sf.scf[1][sb], sfsPerScfsi[(int) mp2->scfsi[1][sb]]);
	}

	/* Samples */

	for (j = 0; j < 3 * SCALE_BLOCK; j += 3) {
		for (sb = 0; sb < mp2->sblimit; sb++) {
			int sg = mp2->sb_group[sb];
			int pk = mp2->packed[sg];
			int ba;

			if ((ba = mp2->bit_alloc[0][sb])) {
				int bi = mp2->bits[sg][ba];

				if (pk & (1 << ba)) {
					int t, y = mp2->steps[ba - 1];

			    	        t = (    mp2->sb_samples[0][0][j + 2][sb]) * y;
					t = (t + mp2->sb_samples[0][0][j + 1][sb]) * y;
				        t = (t + mp2->sb_samples[0][0][j + 0][sb]);

					bputl(&mp2->out, t, bi);
				} else {
					bstartq(mp2->sb_samples[0][0][j + 0][sb]);
					bcatq(mp2->sb_samples[0][0][j + 1][sb], bi);
					bcatq(mp2->sb_samples[0][0][j + 2][sb], bi);
					bputq(&mp2->out, bi * 3);
				}
			}

			if (channels > 1 && (ba = mp2->bit_alloc[1][sb])) {
				int bi = mp2->bits[sg][ba];

				if (pk & (1 << ba)) {
					int t, y = mp2->steps[ba - 1];

			    	        t = (    mp2->sb_samples[1][0][j + 2][sb]) * y;
					t = (t + mp2->sb_samples[1][0][j + 1][sb]) * y;
				        t = (t + mp2->sb_samples[1][0][j + 0][sb]);

					bputl(&mp2->out, t, bi);
				} else {
					bstartq(mp2->sb_samples[1][0][j + 0][sb]);
					bcatq(mp2->sb_samples[1][0][j + 1][sb], bi);
					bcatq(mp2->sb_samples[1][0][j + 2][sb], bi);
					bputq(&mp2->out, bi * 3);
				}
			}
		}
	}

	/* Auxiliary bits (none) */

	bprolog(&mp2->out);

	emms();

	bflush(&mp2->out);

	while (((char *) mp2->out.p - (char *) mp2->out.p1) < bpf)
		*((unsigned int *)(mp2->out.p))++ = 0;
}

/*
 *  Compress one audio frame
 */
static inline void
audio_frame(mp2_context *mp2, int channels)
{
	buffer *obuf;
	short *p;
	int bpf;

	p = fetch_samples(mp2, channels);

	// DUMP(p, 0, (1152 + 480) * channels);

	pr_start(38, "Audio frame (1152/2304 samples)");

		pr_start(35, "Subband filter x 36/72");

			if (channels > 1)
				mmx_filter_stereo(mp2, p, mp2->sb_samples[0][0][0]);
			else
				mmx_filter_mono(mp2, p, mp2->sb_samples[0][0][0]);

			// DUMP(mp2->sb_samples[0][0][0], 0, 3 * SCALE_BLOCK * SBLIMIT * channels);

		pr_end(35);

		pr_start(34, "Psychoacoustic analysis");
	
			mp1e_mp2_psycho(mp2, p, mp2->mnr[0], channels);
		
			if (channels > 1)
				mp1e_mp2_psycho(mp2, p + 1, mp2->mnr[1], 2);

		pr_end(34);

		pr_start(36, "Bit twiddling");

			bpf = adbits(mp2);

			scale_pattern(mp2, channels);

			bit_allocation(mp2, bpf, channels);

			scale_quant(mp2, channels);

		pr_end(36);

		obuf = wait_empty_buffer(&mp2->prod);
		obuf->used = bpf >> 3;

		pr_start(37, "Encoding");

			encode(mp2, obuf->data, obuf->used, channels);

		pr_end(37);

	pr_end(38); /* audio frame */

	// DUMP(((unsigned char *) obuf->data), 0, obuf->used);

	pthread_mutex_lock(&mp2->codec.mutex);

	obuf->time = mp2->codec.coded_time_elapsed;
	mp2->codec.coded_time_elapsed += mp2->coded_frame_period;

	mp2->codec.frame_output_count++;
	mp2->codec.byte_output_count += obuf->used;

	pthread_mutex_unlock(&mp2->codec.mutex);

	send_full_buffer(&mp2->prod, obuf);
}

static void *
mainloop(void *p)
{
	mp2_context *mp2 = PARENT(p, mp2_context, codec);
	int frame_frac = 0, channels;
//XXX
	assert(mp2->codec.status == RTE_STATUS_READY);

	// fpu_control(FPCW_PRECISION_SINGLE, FPCW_PRECISION_MASK);

	mp1e_sync_run_in(&mp2->sstr, &mp2->cons, &frame_frac);

	channels = (mp2->audio_mode != AUDIO_MODE_MONO) + 1;

	next_buffer(mp2, NULL, channels, 0);

	mp2->i16 = frame_frac << (16 - channels + mp2->format_scale);
	mp2->nominal_time_elapsed = frame_frac * mp2->sstr.byte_period;

	if (mp2->audio_mode == AUDIO_MODE_MONO)
		for (;;)
			audio_frame(mp2, 1); /* inlined */
	else
		for (;;)
			audio_frame(mp2, 2); /* inlined */

	return NULL;
}

/*
 *  Non time critical code
 */

/*
 *  XXX historical
 */
void
audio_parameters(int *sampling_freq, int *bit_rate)
{
	int i, imin;
	unsigned int dmin;
	int mpeg_version;

	imin = 0;
        dmin = UINT_MAX;

	for (i = 0; i < 16; i++)
		if (sampling_freq_value[0][i] > 0)
		{
			unsigned int d = nbabs(*sampling_freq - sampling_freq_value[0][i]);

			if (d < dmin) {	    
				if (i / 4 == MPEG_VERSION_2_5)
					continue; // not supported

				dmin = d;
	    			imin = i;
			}
		}

	mpeg_version = imin / 4;

	*sampling_freq = sampling_freq_value[0][imin];

	imin = 0;
        dmin = UINT_MAX;

	// total bit_rate, not per channel

	for (i = 0; i < 16; i++)
		if (bit_rate_value[mpeg_version][i] > 0)
		{
			unsigned int d = nbabs(*bit_rate - bit_rate_value[mpeg_version][i]);

			if (d < dmin) {
				dmin = d;
			        imin = i;
			}
		}

	*bit_rate = bit_rate_value[mpeg_version][imin];
}

static void
uninit(mp2_context *mp2)
{
	/* FIXME */

	mp2->codec.status = RTE_STATUS_PARAM;
}

/*
 *  Prepare for compression
 *  (api preliminary)
 */
void
mp1e_mp2_init(rte_codec *codec, unsigned int module,
	      fifo *cap_fifo, multiplexer *mux)
{
	mp2_context *mp2 = PARENT(codec, mp2_context, codec);
	int sb, min_sg, bit_rate, bit_rate_per_ch;
	int channels, table, sampling_freq, temp, i;

	pthread_mutex_lock(&mp2->codec.mutex);

	switch (mp2->codec.status) {
	case RTE_STATUS_PARAM:
		break;
	case RTE_STATUS_READY:
		uninit(mp2);
		break;
	default:
		pthread_mutex_unlock(&mp2->codec.mutex);
		return;
	}

	channels = 1 + (mp2->audio_mode != AUDIO_MODE_MONO);

	sampling_freq = sampling_freq_value[mp2->mpeg_version][mp2->sampling_freq_code];

	bit_rate = bit_rate_value[mp2->mpeg_version][mp2->bit_rate_code];
	bit_rate_per_ch = bit_rate / channels;

	if (mp2->mpeg_version == MPEG_VERSION_2)
		table = 4;
	else if ((sampling_freq == 48000 && bit_rate_per_ch >= 56000) ||
		 (bit_rate_per_ch >= 56000 && bit_rate_per_ch <= 80000))
		table = 0;
	else if (sampling_freq != 48000 && bit_rate_per_ch >= 96000)
		table = 1;
	else if (sampling_freq != 32000 && bit_rate_per_ch <= 48000)
		table = 2;
	else
		table = 3;

	/* Number of subbands */

	for (mp2->sblimit = SBLIMIT; mp2->sblimit > 0; mp2->sblimit--)
		if (subband_group[table][mp2->sblimit - 1])
			break;

	for (min_sg = NUM_SG, sb = mp2->sblimit; sb > 0; sb--)
		if (subband_group[table][sb - 1] < min_sg)
			min_sg = subband_group[table][sb - 1];

	printv(3, "Audio table #%d, %d Hz cut-off\n",
		table, sampling_freq * mp2->sblimit / 32);

	mp2->sum_nbal = 0;

	for (sb = 0; sb < mp2->sblimit; sb++) {
		int ba, ba_indices;
		int sg = subband_group[table][sb];
		int sg0 = sg - min_sg;

		/* Subband number to index into tables of subband properties */

		mp2->sb_group[sb] = sg0;

		/* 1 << n indicates packed sample encoding in subband n */

		mp2->packed[sg0] = pack_table[sg];

		/* Bit allocation index into bits-per-sample table */

		for (ba_indices = MAX_BA_INDICES; ba_indices > 0; ba_indices--)
			if (bits_table[sg][ba_indices - 1] > 0)
				break;

		for (ba = 0; ba < ba_indices; ba++)
			mp2->bits[sg0][ba] = bits_table[sg][ba];

		/* Quantization step size for packed samples */

		for (ba = 0; ba < ba_indices; ba++)
			if ((int) pack_table[sg] & (1 << ba))
				mp2->steps[ba - 1] = steps_table[sg][ba];

		for (ba = 0; ba < ba_indices; ba++) {
			int q, s, n;

			q = quant_table[sg][ba];

			if (q == 0)		q = 2;
			else if (q == 1)	q = 0;
			else if (q == 2)	q = 3;
			else if (q == 3)	q = 1;

			mp2->qs[sg0][ba].quant = q;

			s = steps_table[sg][ba];

			for (n = 0; (1L << n) < s; n++);

			mp2->qs[sg0][ba].shift = n - 1;
		}

		/* How many bits to encode bit allocation index */

		mp2->nbal[sb] = ffs(ba_indices) - 1;
		mp2->sum_nbal += ffs(ba_indices) - 1;

		/*
		 *  How many bits to encode, and mnr gained, for a subband
		 *  when increasing its bit allocation index ba (0..n) by one
		 */
		for (ba = 0; ba < 16; ba++)
			if (ba + 1 >=  ba_indices) {
				mp2->bit_incr[sg0][ba] = 32767; /* all bits allocated */
				mp2->mnr_incr[sg0][ba] = 0;
			} else {
				int bit_incr;
				double mnr_incr;

				if (pack_table[sg] & (2 << ba))
					bit_incr = SCALE_BLOCK * bits_table[sg][ba + 1];
		    		else
					bit_incr = SCALE_BLOCK * 3 * bits_table[sg][ba + 1];

				if (pack_table[sg] & (1 << ba))
					bit_incr -= SCALE_BLOCK * bits_table[sg][ba];
		    		else
					bit_incr -= SCALE_BLOCK * 3 * bits_table[sg][ba];

				mnr_incr = SNR[quant_table[sg][ba + 1] + 1];

				if (ba > 0)
					mnr_incr -= SNR[quant_table[sg][ba] + 1];
				else
					bit_incr += 2 + 3 * 6; /* scfsi code, 3 scfsi */

				mp2->bit_incr[sg0][ba] = bit_incr;
				mp2->mnr_incr[sg0][ba] = 1.0 / pow(10.0, mnr_incr * 0.1); /* see psycho.c */
			}
	}

	mp1e_mp2_psycho_init(mp2, sampling_freq);

	/* Header */

	temp = bit_rate * SAMPLES_PER_FRAME / BITS_PER_SLOT;

	mp2->bits_per_frame = (temp / sampling_freq) * BITS_PER_SLOT;
	mp2->spf_rest = temp % sampling_freq;
	mp2->spf_lag = sampling_freq / 2;
	mp2->sampling_freq = sampling_freq;

	mp2->coded_frame_period = SAMPLES_PER_FRAME / (double) sampling_freq;
	mp2->nominal_sample_period = 1 / (double) sampling_freq;

	mp2->header_template =
		(0x7FF << 21) | (mp2->mpeg_version << 19) | (LAYER_II << 17) | (1 << 16) |
		/* sync, version, layer, CRC none */
		(mp2->bit_rate_code << 12) | (mp2->sampling_freq_code << 10) | (0 << 8) |
		/* bit rate, sampling rate, private_bit */
		(mp2->audio_mode << 6) | (0 << 4) | (0 << 3) | (0 << 2) | 0;
		/* audio_mode, mode_ext, copyright no, original no, emphasis none */

	binit_write(&mp2->out);

	mp2->fifo = mux_add_input_stream(mux,
		AUDIO_STREAM, "audio-mp2",
		2048 * channels, 0, /* aud_buffers, */
		sampling_freq / (double) SAMPLES_PER_FRAME, bit_rate);

	for (i = 0; i < aud_buffers; i++)
		assert(add_buffer(mp2->fifo, alloc_buffer(2048 * channels)));

	ASSERT("add audio producer",
		add_producer(mp2->fifo, &mp2->prod));

	/* Input */

	mp2->sstr.this_module = module;

	memset(mp2->wrap, 0, sizeof(mp2->wrap));

	ASSERT("add audio consumer",
		add_consumer(cap_fifo, &mp2->cons));

	mp2->codec.frame_input_count = 0;
	mp2->codec.frame_input_missed = 0; /* n/a */
	mp2->codec.frame_output_count = 0;
	mp2->codec.byte_output_count = 0;
	mp2->codec.coded_time_elapsed = 0.0;
	mp2->codec.frame_output_rate = 1 / (double) mp2->coded_frame_period; 

	mp2->codec.status = RTE_STATUS_READY;

	pthread_mutex_unlock(&mp2->codec.mutex);
}

/*
 *  API
 */

#include <stdarg.h>

/* todo */
#undef _
#undef ENABLE_NLS

#ifndef _
#ifdef ENABLE_NLS
#    include <libintl.h>
#    define _(String) gettext (String)
#    ifdef gettext_noop
#        define N_(String) gettext_noop (String)
#    else
#        define N_(String) (String)
#    endif
#else
/* Stubs that do something close enough.  */
#    define textdomain(String) (String)
#    define gettext(String) (String)
#    define dgettext(Domain,Message) (Message)
#    define dcgettext(Domain,Message,Type) (Message)
#    define bindtextdomain(Domain,Directory) (Domain)
#    define _(String) (String)
#    define N_(String) (String)
#endif
#endif

#define elements(array) (sizeof(array) / sizeof(array[0]))

/* Attention: Canonical order */
static char *
menu_audio_mode[] = {
	/* 0 */ N_("Mono"),
	/* 1 */ N_("Stereo"),
	/* (NLS) Bilingual: for example left audio channel English, right French */
	/* 2 */ N_("Bilingual"), 
	/* 3 TODO N_("Joint Stereo"), */
};

/* Attention: Canonical order */
static char *
menu_psycho[] = {
	/* (NLS) Psychoaccoustic analysis: Static (none), Fast, Accurate */
	/* 0 */ N_("Static"),
	/* 1 */ N_("Fast"),
	/* 2 */ N_("Accurate"),
};

static rte_option
mpeg1_options[] = {
	RTE_OPTION_INT_INITIALIZER
	  ("bit_rate", N_("Bit rate"),
	   80000, 32000, 384000, 1,
	   (int *) &bit_rate_value[MPEG_VERSION_1][1], 14,
	   N_("Output bit rate, all channels together")),
	RTE_OPTION_INT_INITIALIZER
	  ("sampling_rate", N_("Sampling frequency"),
	   44100, 32000, 48000, 1,
	   (int *) &sampling_freq_value[MPEG_VERSION_1][0], 3, (NULL)),
	RTE_OPTION_MENU_INITIALIZER
	  ("audio_mode", N_("Mode"),
	   0, menu_audio_mode, elements(menu_audio_mode), (NULL)),
	RTE_OPTION_MENU_INITIALIZER
	  ("psycho", N_("Psychoacoustic analysis"),
	   0, menu_psycho, elements(menu_psycho),
	   N_("Speed/quality tradeoff. Selecting 'Accurate' is recommended "
	      "below 80 kbit/s per channel, when you have bat ears or a "
	      "little more CPU load doesn't matter.")),
};

static rte_option
mpeg2_options[elements(mpeg1_options)];

static rte_option *
option_enum(rte_codec *codec, int index)
{
	mp2_context *mp2 = PARENT(codec, mp2_context, codec);

	if (mp2->mpeg_version == MPEG_VERSION_1) {
		if (index < 0 || index >= elements(mpeg1_options))
			return NULL;
		return mpeg1_options + index;
	} else if (mp2->mpeg_version == MPEG_VERSION_2) {
		if (index < 0 || index >= elements(mpeg2_options))
			return NULL;
		return mpeg2_options + index;
	}

	return NULL;
}

static int
option_get(rte_codec *codec, char *keyword, rte_option_value *v)
{
	mp2_context *mp2 = PARENT(codec, mp2_context, codec);

	pthread_mutex_lock(&mp2->codec.mutex);

	if (strcmp(keyword, "bit_rate") == 0) {
		v->num = bit_rate_value[mp2->mpeg_version][mp2->bit_rate_code];
	} else if (strcmp(keyword, "sampling_rate") == 0) {
		v->num = sampling_freq_value[mp2->mpeg_version][mp2->sampling_freq_code];
	} else if (strcmp(keyword, "audio_mode") == 0) {
		/* mo, st, bi (user) <- st, jst, bi, mo (mpeg) */
		v->num = "\1\3\2\0"[mp2->audio_mode];
	} else if (strcmp(keyword, "psycho") == 0) {
		v->num = mp2->psycho_loops;
	} else {
		pthread_mutex_unlock(&mp2->codec.mutex);
		return 0;
	}

	pthread_mutex_unlock(&mp2->codec.mutex);

	return 1;
}

static int
ivec_imin(int *vec, int size, int val)
{
	int i, imin = 0;
	unsigned int d, dmin = UINT_MAX;

	assert(size > 0);

	for (i = 0; i < size; i++) {
		d = nbabs(val - vec[i]);

		if (d < dmin) {
			dmin = d;
		        imin = i;
		}
	}

	return imin;
}

static int
option_set(rte_codec *codec, char *keyword, va_list args)
{
	mp2_context *mp2 = PARENT(codec, mp2_context, codec);

	pthread_mutex_lock(&mp2->codec.mutex);

	switch (mp2->codec.status) {
	case RTE_STATUS_NEW:
	case RTE_STATUS_PARAM:
		break;
	case RTE_STATUS_READY:
		uninit(mp2);
		break;
	default:
		pthread_mutex_unlock(&mp2->codec.mutex);
		return 0;
	}

	if (strcmp(keyword, "bit_rate") == 0) {
		mp2->bit_rate_code =
			ivec_imin((int *) &bit_rate_value[mp2->mpeg_version][1], 14,
				  va_arg(args, int)) + 1;
 	} else if (strcmp(keyword, "sampling_rate") == 0) {
		mp2->sampling_freq_code =
			ivec_imin((int *) sampling_freq_value[mp2->mpeg_version], 3,
				  va_arg(args, int));
	} else if (strcmp(keyword, "audio_mode") == 0) {
		int val = va_arg(args, int);

		if (val < 0 || val > 2)
			return 0;
		/* mo, st, bi (user) -> st, jst, bi, mo (mpeg) */
		mp2->audio_mode = "\3\0\2\1"[val];
	} else if (strcmp(keyword, "psycho") == 0) {
		int val = va_arg(args, int);

		if (val < 0 || val > 2)
			return 0;
		mp2->psycho_loops = val;
	} else {
		pthread_mutex_unlock(&mp2->codec.mutex);
		return 0;
	}

	mp2->codec.status = RTE_STATUS_NEW;

	pthread_mutex_unlock(&mp2->codec.mutex);

	return 1;
}

static char *
option_print(rte_codec *codec, char *keyword, va_list args)
{
	mp2_context *mp2 = PARENT(codec, mp2_context, codec);
	char buf[80];

	if (strcmp(keyword, "bit_rate") == 0) {
		snprintf(buf, sizeof(buf), _("%u kbit/s"),
			 bit_rate_value[mp2->mpeg_version][ivec_imin(
				 (int *) &bit_rate_value[mp2->mpeg_version][1], 14,
				 va_arg(args, int)) + 1] / 1000);
	} else if (strcmp(keyword, "sampling_rate") == 0) {
		snprintf(buf, sizeof(buf), _("%u Hz"),
			 sampling_freq_value[mp2->mpeg_version][ivec_imin((int *)
				 sampling_freq_value[mp2->mpeg_version], 3,
				 va_arg(args, int))]);
	} else if (strcmp(keyword, "audio_mode") == 0) {
		int val = va_arg(args, int);
		if (val < 0 || val > 2)
			return 0;
		return strdup(_(menu_audio_mode[val]));
	} else if (strcmp(keyword, "psycho") == 0) {
		int val = va_arg(args, int);
		if (val < 0 || val > 2)
			return 0;
		return strdup(_(menu_psycho[val]));
	} else
		return 0;

	return strdup(buf);
}

static int
parameters(rte_codec *codec, rte_stream_parameters *rsp)
{
	static const rte_sndfmt valid_format[] = {
		RTE_SNDFMT_S16LE,
		RTE_SNDFMT_U16LE,
		RTE_SNDFMT_S8,
		RTE_SNDFMT_U8,
	};
	mp2_context *mp2 = PARENT(codec, mp2_context, codec);
	int fragment_size, sampling_freq;
	int i;

	pthread_mutex_lock(&mp2->codec.mutex);

	switch (mp2->codec.status) {
	case RTE_STATUS_NEW:
	case RTE_STATUS_PARAM:
		break;
	case RTE_STATUS_READY:
		uninit(mp2);
		break;
	default:
		pthread_mutex_unlock(&mp2->codec.mutex);
		return 0;
	}

	/*
	 *  Accept sample format in decreasing order of quality
	 */
	for (i = 0; i < elements(valid_format); i++)
		if (rsp->audio.sndfmt == valid_format[i])
			break;

	if (i >= elements(valid_format))
		rsp->audio.sndfmt = valid_format[0];

	/*
	 *  Accept sampling freq within 10% coded sampling freq (preliminary)
	 */
	sampling_freq =	sampling_freq_value[mp2->mpeg_version][mp2->sampling_freq_code];

	if (nbabs(rsp->audio.sampling_freq - sampling_freq)
	    > (sampling_freq / 10))
		rsp->audio.sampling_freq = sampling_freq;

	/*
	 *  No splitting/merging of channels
	 */
	rsp->audio.channels =
		(mp2->audio_mode == AUDIO_MODE_MONO) ? 1 : 2;

	memset(&mp2->sstr, 0, sizeof(mp2->sstr));

	switch (rsp->audio.sndfmt) {
	case RTE_SNDFMT_S8:
	case RTE_SNDFMT_U8:
		mp2->sstr.bytes_per_sample =
			rsp->audio.channels * 1;
		mp2->format_scale = TRUE;
		break;

	case RTE_SNDFMT_S16LE:
	case RTE_SNDFMT_U16LE:
		mp2->sstr.bytes_per_sample =
			rsp->audio.channels * 2;
		mp2->format_scale = FALSE;
		break;

	default:
		FAIL("unsupported rte sample format %d",
		     rsp->audio.sndfmt);
	}

	switch (rsp->audio.sndfmt) {
	case RTE_SNDFMT_U8:
	case RTE_SNDFMT_U16LE:
		mp2->format_sign = 0x80008000;
		break;

	default:
		mp2->format_sign = 0;
		break;
	}

	mp2->sstr.byte_period = 1.0
		/ (rsp->audio.sampling_freq * mp2->sstr.bytes_per_sample);

	printv(3, "Using rte sample format %d, bps %d, bp %.20f\n",
	       rsp->audio.sndfmt,
	       mp2->sstr.bytes_per_sample,
	       mp2->sstr.byte_period);

	/*
	 *  Suggest a minimum buffer size to keep overhead reasonable.
	 */
	fragment_size = 2048 * sizeof(short) * rsp->audio.channels;

	rsp->audio.fragment_size =
		MAX(rsp->audio.fragment_size, fragment_size);

	mp2->codec.status = RTE_STATUS_PARAM;

	pthread_mutex_unlock(&mp2->codec.mutex);

	return 1;
}

static void
codec_delete(rte_codec *codec)
{
	mp2_context *mp2 = PARENT(codec, mp2_context, codec);

	pthread_mutex_lock(&mp2->codec.mutex);

	switch (mp2->codec.status) {
	case RTE_STATUS_READY:
		uninit(mp2);
		break;
	case RTE_STATUS_RUNNING:
		fprintf(stderr, "mp1e bug warning: attempt to delete "
			"running mp2 codec ignored\n");
		pthread_mutex_unlock(&mp2->codec.mutex);
		return;
	default:
		break;
	}

	pthread_mutex_destroy(&mp2->codec.mutex);
	free_aligned(mp2);
}

static rte_codec *
codec_new(int mpeg_version)
{
	mp2_context *mp2;
	rte_option *option = NULL;
	int i = 0;

	if (!(mp2 = calloc_aligned(sizeof(*mp2), 8192)))
		return NULL;

	switch ((mp2->mpeg_version = mpeg_version)) {
	case MPEG_VERSION_1:
		option = mpeg1_options;
		i = elements(mpeg1_options);
		mp2->codec.class = &mp1e_mpeg1_layer2_codec;
		break;

	case MPEG_VERSION_2:
		option = mpeg2_options;
		i = elements(mpeg2_options);
		mp2->codec.class = &mp1e_mpeg2_layer2_codec;
		break;

	default:
		assert(!"reached");
	}

	pthread_mutex_init(&mp2->codec.mutex, NULL);

	mp2->codec.status = RTE_STATUS_NEW;

	rte_helper_reset_options(&mp2->codec);

	return &mp2->codec;
}

static rte_codec *
codec_mpeg1_new(void)
{
	return codec_new(MPEG_VERSION_1);
}

rte_codec_class
mp1e_mpeg1_layer2_codec = {
	.public = {
		.stream_type	= RTE_STREAM_AUDIO,
		.keyword	= "mpeg1_audio_layer2",
		.label		= N_("MPEG-1 Audio Layer II"),
	},

	.new		= codec_mpeg1_new,
	.delete         = codec_delete,

	.option_enum	= option_enum,
	.option_get	= option_get,
	.option_set	= option_set,
	.option_print	= option_print,

	.parameters	= parameters,

	.mainloop	= mainloop,
};

static rte_codec *
codec_mpeg2_new(void)
{
	return codec_new(MPEG_VERSION_2);
}

rte_codec_class
mp1e_mpeg2_layer2_codec = {
	.public = {
		.stream_type	= RTE_STREAM_AUDIO,
		.keyword	= "mpeg2_audio_layer2",
		.label		= N_("MPEG-2 Audio Layer II LFE"),
		.tooltip	= N_("MPEG-2 Low (Sampling) Frequency Extension to MPEG-1 "
				     "Audio Layer II. Caution: Not all MPEG video and "
				     "audio players support MPEG-2 audio."),
	},

	.new		= codec_mpeg2_new,
	.delete         = codec_delete,

	.option_enum	= option_enum,
	.option_get	= option_get,
	.option_set	= option_set,
	.option_print	= option_print,

	.parameters	= parameters,

	.mainloop	= mainloop,
};

static void mp1e_mp2(void) __attribute__ ((constructor));

static void
mp1e_mp2(void)
{
	assert(sizeof(mpeg1_options) == sizeof(mpeg2_options));
	memcpy(mpeg2_options, mpeg1_options, sizeof(mpeg2_options));

	mpeg2_options[0].menu.num = (int *) &bit_rate_value[MPEG_VERSION_2][1];
	mpeg2_options[1].menu.num = (int *) &sampling_freq_value[MPEG_VERSION_2][0];

	mp1e_mp2_subband_filter_init(0);
	mp1e_mp2_fft_init(0);
}
