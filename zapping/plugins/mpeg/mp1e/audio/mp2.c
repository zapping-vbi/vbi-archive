/*
 *  MPEG Real Time Encoder
 *  MPEG-1 Audio Layer II
 *  MPEG-2 Audio Layer II Low Frequency Extensions
 *
 *  Copyright (C) 1999-2001 Michael H. Schimek
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

/* $Id: mp2.c,v 1.16 2001-07-28 06:55:57 mschimek Exp $ */

#include <limits.h>
#include "../common/log.h"
#include "../options.h"
#include "../common/mmx.h"
#include "../common/profile.h"
#include "../common/bstream.h"
#include "../common/math.h"
#include "../common/remote.h"
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

struct audio_seg aseg __attribute__ ((aligned (4096)));

fifo2 *			audio_fifo;
static producer		audio_prod;

extern int		audio_num_frames;
extern int		aud_buffers;



/* UI convenience, find closest valid parameters */

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





void
audio_init(int sampling_freq, int stereo, int audio_mode, int bit_rate, int psycho_loops)
{
	int mpeg_version, sampling_freq_code, bit_rate_code;
	int sb, min_sg, bit_rate_per_ch;
	int i, temp, channels, table;
	struct audio_seg *mp2 = &aseg;

	audio_mode &= 3;

	if (audio_mode == AUDIO_MODE_JOINT_STEREO)
		audio_mode = AUDIO_MODE_STEREO;

	channels = 1 << (!!stereo);

	for (i = 0; i < 16; i++)
		if (sampling_freq_value[0][i] == sampling_freq)
			break;

	mpeg_version = i / 4;
	sampling_freq_code = i % 4;

	if (mpeg_version == MPEG_VERSION_2_5 || i >= 16)
		FAIL("Invalid audio sampling rate %d Hz", sampling_freq);

	for (i = 0; i < 16; i++)
		if (bit_rate_value[mpeg_version][i] == bit_rate)
			break;

	bit_rate_code = i;

	if (i >= 16)
		FAIL("Invalid audio bit rate %d bits/s", bit_rate);

        bit_rate_per_ch = bit_rate / channels;

	if (mpeg_version == MPEG_VERSION_2)
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

	// Number of subbands

	for (mp2->sblimit = SBLIMIT; mp2->sblimit > 0; mp2->sblimit--)
		if (subband_group[table][mp2->sblimit - 1])
			break;

	for (min_sg = NUM_SG, sb = mp2->sblimit; sb > 0; sb--)
		if (subband_group[table][sb - 1] < min_sg)
			min_sg = subband_group[table][sb - 1];

	printv(3, "Audio table #%d, %d Hz\n", table, mp2->sampling_freq * mp2->sblimit / 32);

	mp2->sum_nbal = 0;

	for (sb = 0; sb < mp2->sblimit; sb++) {
		int ba, ba_indices;
		int sg = subband_group[table][sb];
		int sg0 = sg - min_sg;

		// Subband number to index into tables of subband properties

		mp2->sb_group[sb] = sg0;

		// 1 << n indicates packed sample encoding in subband n

		mp2->packed[sg0] = pack_table[sg];

		// Bit allocation index into bits-per-sample table

		for (ba_indices = MAX_BA_INDICES; ba_indices > 0; ba_indices--)
			if (bits_table[sg][ba_indices - 1] > 0)
				break;

		for (ba = 0; ba < ba_indices; ba++)
			mp2->bits[sg0][ba] = bits_table[sg][ba];

		// Quantization step size for packed samples

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

		// How many bits to encode bit allocation index

		mp2->nbal[sb] = ffs(ba_indices) - 1;
		mp2->sum_nbal += ffs(ba_indices) - 1;

		// How many bits to encode, and mnr gained, for a subband
		// when increasing its bit allocation index ba (0..n) by one

		for (ba = 0; ba < 16; ba++)
			if (ba + 1 >=  ba_indices) {
				mp2->bit_incr[sg0][ba] = 32767; // All bits allocated
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
					bit_incr += 2 + 3 * 6; // scfsi code, 3 scfsi

				mp2->bit_incr[sg0][ba] = bit_incr;
				mp2->mnr_incr[sg0][ba] = 1.0 / pow(10.0, mnr_incr * 0.1); // see psycho.c
			}
	}

	temp = bit_rate * SAMPLES_PER_FRAME / BITS_PER_SLOT;

	mp2->bits_per_frame = (temp / sampling_freq) * BITS_PER_SLOT;
	mp2->spf_rest = temp % sampling_freq;
	mp2->spf_lag = sampling_freq / 2;



        mp2->sampling_freq = sampling_freq;

	mp2->e_save_old    = mp2->e_save[0][1];
	mp2->e_save_oldest = mp2->e_save[0][0];
	mp2->h_save_new    = mp2->h_save[0][2];
	mp2->h_save_old    = mp2->h_save[0][1];
	mp2->h_save_oldest = mp2->h_save[0][0];

	binit_write(&mp2->out);

	mp2->header_template =
		(0x7FF << 21) | (mpeg_version << 19) | (LAYER_II << 17) | (1 << 16) |
		/* sync, version, layer, CRC none */
		(bit_rate_code << 12) | (sampling_freq_code << 10) | (0 << 8) |
		/* bit rate, sampling rate, private_bit */
		(audio_mode << 6) | (0 << 4) | (0 << 3) | (0 << 2) | 0;
		/* audio_mode, mode_ext, copyright no, original no, emphasis none */

	subband_filter_init(&aseg);
	psycho_init(mp2, sampling_freq, psycho_loops);

	mp2->audio_frame_count = 0;

	mp2->frame_period = SAMPLES_PER_FRAME / (double) sampling_freq;

	audio_fifo = mux_add_input_stream(
		AUDIO_STREAM, "audio-mp2",
		2048 * channels, aud_buffers,
		sampling_freq / (double) SAMPLES_PER_FRAME, bit_rate);

	add_producer(audio_fifo, &audio_prod);
}

/*
 *  Bits available in current frame
 */
static inline int
adbits(struct audio_seg *mp2)
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

static void
terminate(void)
{
	buffer2 *obuf;
	extern volatile int mux_thread_done;

	printv(2, "Audio: End of file\n");

	while (!mux_thread_done) {
		obuf = wait_empty_buffer2(&audio_prod);
		obuf->used = 0;
		// XXX other?
		send_full_buffer2(&audio_prod, obuf);
	}

	pthread_exit(NULL);
}

/*
 *  Calculate scale factors and transmission pattern
 */
static inline void
scale_pattern(struct audio_seg *mp2, int channels)
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
scale_quant(struct audio_seg *mp2, int channels)
{
	int t, j, sb, ba, ch;

	for (t = 0; t < channels * 3; t++) {
		ch = (channels >= 2) ? (t >= 3) : 0;

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


void *
mpeg_audio_layer_ii_mono(void *cap_fifo)
{
	consumer cons;

	// fpu_control(FPCW_PRECISION_SINGLE, FPCW_PRECISION_MASK);

	ASSERT("add audio consumer",
		add_consumer((fifo2 *) cap_fifo, &cons));

	remote_sync(&cons, MOD_AUDIO, aseg.frame_period);

	for (;;) {
		buffer2 *ibuf;
		buffer2 *obuf;
		unsigned int adb, bpf;
		double time;

		// FDCT and psychoacoustic analysis

		if (aseg.audio_frame_count > audio_num_frames)
			terminate();

		ibuf = wait_full_buffer2(&cons);

		if (!ibuf || ibuf->used == 0
		    || remote_break(ibuf->time, aseg.frame_period)) {
			if (ibuf)
				send_empty_buffer2(&cons, ibuf);
			terminate();
		}

		time = ibuf->time;

		aseg.audio_frame_count++;

		pr_start(38, "Audio frame (1152 samples)");
		pr_start(35, "Subband filter x 36");

		// DUMP((short *) ibuf->data, 0, 1152);

		mmx_filter_mono(&aseg, (short *) ibuf->data, aseg.sb_samples[0][0][0]);

		// DUMP(aseg.sb_samples[0][0][0], 0, 3 * SCALE_BLOCK * SBLIMIT);

		pr_end(35);

		pr_start(34, "Psychoacoustic analysis");

		psycho(&aseg, (short *) ibuf->data, aseg.mnr[0], 1);

		pr_end(34);

		send_empty_buffer2(&cons, ibuf);

		pr_start(36, "Bit twiddling");

		bpf = (adb = adbits(&aseg)) >> 3;

		scale_pattern(&aseg, 1);

		// Bit allocation

		{
			int i, incr, ba, sg, sb;

			memset(aseg.bit_alloc, 0, sizeof(aseg.bit_alloc));

			adb -= HEADER_BITS + aseg.sum_nbal; // + auxiliary bits

			for (;;) {
				double min = -1e37;
				sb = -1;

				for (i = 0; i < aseg.sblimit; i++)
					if (aseg.mnr[0][i] > min) {
						min = aseg.mnr[0][i];
						sb = i;
    					}

				if (sb < 0) break; // all done

				ba = aseg.bit_alloc[0][sb];
				sg = aseg.sb_group[sb];
				incr = aseg.bit_incr[sg][ba];

				if (ba == 0)
					incr += sfsPerScfsi[(int) aseg.scfsi[0][sb]] - 3 * 6;

				if (incr <= adb) {
					aseg.bit_alloc[0][sb] = ba + 1;
					adb -= incr;

					if (aseg.bit_incr[sg][ba + 1] > adb)
						aseg.mnr[0][sb] = -1e38; // all bits allocated
					else
						aseg.mnr[0][sb] *= aseg.mnr_incr[sg][ba];
				} else
    					aseg.mnr[0][sb] = -1e38; // can't incr bits for this band
			}

			// DUMP(aseg.bit_alloc[0], 0, SBLIMIT);
		}

		scale_quant(&aseg, 1);

		pr_end(36);

		obuf = wait_empty_buffer2(&audio_prod);

		bstart(&aseg.out, obuf->data);

		aseg.out.buf = MMXD(0, aseg.header_template);
		aseg.out.n = 32;

		pr_start(37, "Encoding");

		bepilog(&aseg.out);

		// Encode

		{
			int j, sb = 0;

			for (; sb < aseg.sblimit; sb++)
				bputl(&aseg.out, aseg.bit_alloc[0][sb], aseg.nbal[sb]);

			for (sb = 0; sb < aseg.sblimit; sb++)
		    		if (aseg.bit_alloc[0][sb])
					bputl(&aseg.out, aseg.scfsi[0][sb], 2);

			for (sb = 0; sb < aseg.sblimit; sb++)
		    		if (aseg.bit_alloc[0][sb])
					bputl(&aseg.out, aseg.sf.scf[0][sb], sfsPerScfsi[(int) aseg.scfsi[0][sb]]);

			for (j = 0; j < 3 * SCALE_BLOCK; j += 3)
				for (sb = 0; sb < aseg.sblimit; sb++) {
					int ba;

					if ((ba = aseg.bit_alloc[0][sb])) {
						int sg = aseg.sb_group[sb];
						int bi = aseg.bits[sg][ba];

						if ((int) aseg.packed[sg] & (1 << ba)) {
							int t, y = aseg.steps[ba - 1];

					    	        t = (    aseg.sb_samples[0][0][j + 2][sb]) * y;
							t = (t + aseg.sb_samples[0][0][j + 1][sb]) * y;
						        t = (t + aseg.sb_samples[0][0][j + 0][sb]);

							bputl(&aseg.out, t, bi);
						} else {
							bstartq(aseg.sb_samples[0][0][j + 0][sb]);
							bcatq(aseg.sb_samples[0][0][j + 1][sb], bi);
							bcatq(aseg.sb_samples[0][0][j + 2][sb], bi);
							bputq(&aseg.out, bi * 3);
						}
					}
				}
		}

		bprolog(&aseg.out);
		emms();

		pr_end(37);
		pr_end(38);

		bflush(&aseg.out);

		while (((char *) aseg.out.p - (char *) aseg.out.p1) < bpf)
			*((unsigned int *)(aseg.out.p))++ = 0;

		// DUMP(((unsigned char *) obuf->data), 0, bpf);

		obuf->used = bpf;
		obuf->time = time;

		send_full_buffer2(&audio_prod, obuf);
	}

	return NULL; // never
}

void *
mpeg_audio_layer_ii_stereo(void *cap_fifo)
{
	consumer cons;

	// fpu_control(FPCW_PRECISION_SINGLE, FPCW_PRECISION_MASK);

	ASSERT("add audio consumer",
		add_consumer((fifo2 *) cap_fifo, &cons));

	remote_sync(&cons, MOD_AUDIO, aseg.frame_period);

	for (;;) {
		buffer2 *ibuf;
		buffer2 *obuf;
		unsigned int adb, bpf;
		double time;

		// FDCT and psychoacoustic analysis

		if (aseg.audio_frame_count > audio_num_frames)
			terminate();

		ibuf = wait_full_buffer2(&cons);

		if (!ibuf || ibuf->used == 0
		    || remote_break(ibuf->time, aseg.frame_period)) {
			if (ibuf)
				send_empty_buffer2(&cons, ibuf);
			terminate();
		}

		time = ibuf->time;

		aseg.audio_frame_count++;

		pr_start(38, "Audio frame (2304 samples)");
		pr_start(35, "Subband filter x 72");

		// DUMP((short *) ibuf->data, 0, 2 * 1152);

		mmx_filter_stereo(&aseg, (short *) ibuf->data, aseg.sb_samples[0][0][0]);

		// DUMP(aseg.sb_samples[0][0][0], 0, 2 * 3 * SCALE_BLOCK * SBLIMIT);

		pr_end(35);

		pr_start(34, "Psychoacoustic analysis");

		psycho(&aseg, ((short *) ibuf->data),     aseg.mnr[0], 2);
		psycho(&aseg, ((short *) ibuf->data) + 1, aseg.mnr[1], 2);

		pr_end(34);

		send_empty_buffer2(&cons, ibuf);

		pr_start(36, "Bit twiddling");

		bpf = (adb = adbits(&aseg)) >> 3;

		scale_pattern(&aseg, 2);

		// Bit allocation

		{
			int i, incr, ba, sg, sb, ch = 0;

			memset(aseg.bit_alloc, 0, sizeof(aseg.bit_alloc));

			adb -= HEADER_BITS + aseg.sum_nbal * 2; // + auxiliary bits

			for (;;) {
				double min = -1e37;
				sb = -1;

				for (i = 0; i < aseg.sblimit; i++) {
					if (aseg.mnr[0][i] > min) {
						min = aseg.mnr[0][i];
						sb = i;
						ch = 0;
    					}
					if (aseg.mnr[1][i] > min) {
						min = aseg.mnr[1][i];
						sb = i;
						ch = 1;
    					}
				}

				if (sb < 0) break; // all done

				ba = aseg.bit_alloc[ch][sb];
				sg = aseg.sb_group[sb];
				incr = aseg.bit_incr[sg][ba];

				if (ba == 0)
					incr += sfsPerScfsi[(int) aseg.scfsi[ch][sb]] - 3 * 6;

				if (incr <= adb) {
					aseg.bit_alloc[ch][sb] = ba + 1;
					adb -= incr;

					if (aseg.bit_incr[sg][ba + 1] > adb)
						aseg.mnr[ch][sb] = -1e38; // all bits allocated
					else
						aseg.mnr[ch][sb] *= aseg.mnr_incr[sg][ba];
				} else
    					aseg.mnr[ch][sb] = -1e38; // can't incr bits for this band
			}
		}

		scale_quant(&aseg, 2);

		pr_end(36);

		obuf = wait_empty_buffer2(&audio_prod);

		bstart(&aseg.out, obuf->data);

		aseg.out.buf = MMXD(0, aseg.header_template);
		aseg.out.n = 32;

		pr_start(37, "Encoding");

		bepilog(&aseg.out);

		// Encode

		{
			int j, sb = 0;

			// Bit allocation table

			for (; sb < aseg.sblimit; sb++) {
				int bi = aseg.nbal[sb];
				bputl(&aseg.out, (aseg.bit_alloc[0][sb] << bi) + aseg.bit_alloc[1][sb], bi * 2);
			}

			// Scale factor selector information

			for (sb = 0; sb < aseg.sblimit; sb++) {
		    		if (aseg.bit_alloc[0][sb])
					bputl(&aseg.out, aseg.scfsi[0][sb], 2);
		    		if (aseg.bit_alloc[1][sb])
					bputl(&aseg.out, aseg.scfsi[1][sb], 2);
			}

			// Scale factors

			for (sb = 0; sb < aseg.sblimit; sb++) {
		    		if (aseg.bit_alloc[0][sb])
					bputl(&aseg.out, aseg.sf.scf[0][sb], sfsPerScfsi[(int) aseg.scfsi[0][sb]]);
		    		if (aseg.bit_alloc[1][sb])
					bputl(&aseg.out, aseg.sf.scf[1][sb], sfsPerScfsi[(int) aseg.scfsi[1][sb]]);
			}

			// Samples

			for (j = 0; j < 3 * SCALE_BLOCK; j += 3)
				for (sb = 0; sb < aseg.sblimit; sb++) {
					int sg = aseg.sb_group[sb];
					int pk = aseg.packed[sg];
					int ba;

					if ((ba = aseg.bit_alloc[0][sb])) {
						int bi = aseg.bits[sg][ba];

						if (pk & (1 << ba)) {
							int t, y = aseg.steps[ba - 1];

					    	        t = (    aseg.sb_samples[0][0][j + 2][sb]) * y;
							t = (t + aseg.sb_samples[0][0][j + 1][sb]) * y;
						        t = (t + aseg.sb_samples[0][0][j + 0][sb]);

							bputl(&aseg.out, t, bi);
						} else {
							bstartq(aseg.sb_samples[0][0][j + 0][sb]);
							bcatq(aseg.sb_samples[0][0][j + 1][sb], bi);
							bcatq(aseg.sb_samples[0][0][j + 2][sb], bi);
							bputq(&aseg.out, bi * 3);
						}
					}

					if ((ba = aseg.bit_alloc[1][sb])) {
						int bi = aseg.bits[sg][ba];

						if (pk & (1 << ba)) {
							int t, y = aseg.steps[ba - 1];

					    	        t = (    aseg.sb_samples[1][0][j + 2][sb]) * y;
							t = (t + aseg.sb_samples[1][0][j + 1][sb]) * y;
						        t = (t + aseg.sb_samples[1][0][j + 0][sb]);

							bputl(&aseg.out, t, bi);
						} else {
							bstartq(aseg.sb_samples[1][0][j + 0][sb]);
							bcatq(aseg.sb_samples[1][0][j + 1][sb], bi);
							bcatq(aseg.sb_samples[1][0][j + 2][sb], bi);
							bputq(&aseg.out, bi * 3);
						}
					}
				}

			// Auxiliary bits (none)
		}

		bprolog(&aseg.out);
		emms();

		bflush(&aseg.out);

		while (((char *) aseg.out.p - (char *) aseg.out.p1) < bpf)
			*((unsigned int *)(aseg.out.p))++ = 0;

		pr_end(37);
		pr_end(38);

		// DUMP(((unsigned char *) obuf->data), 0, bpf);

		obuf->used = bpf;
		obuf->time = time;

		send_full_buffer2(&audio_prod, obuf);
	}

	return NULL; // never
}
