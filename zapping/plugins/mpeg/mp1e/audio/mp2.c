/*
 *  MPEG Real Time Encoder
 *  MPEG-1 Audio Layer II
 *  MPEG-2 Audio Layer II Low Frequency Extensions
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

/* $Id: mp2.c,v 1.5 2000-08-12 02:14:37 mschimek Exp $ */

#include <limits.h>
#include "../options.h"
#include "../common/mmx.h"
#include "../common/profile.h"
#include "../common/bstream.h"
#include "../common/math.h"
#include "mpeg.h"
#include "audio.h"
#include "../systems/systems.h"
#include "../systems/mpeg.h"

/* filter.c */

extern void subband_filter_init(void);
extern void mmx_filter_mono(short *, int *);
extern void mmx_filter_stereo(short *, int *);

/* psycho.c */

extern void psycho_init(void);
extern void psycho(short *, float *, int);

#define SCALE_RANGE 64
#define SCALE 32768

#define M0 2.0
#define M1 1.58740105196820
#define M2 1.25992104989487

enum {
	S123, S122, S133, S113, S444, S111, S222, S333
};

static const char
spattern[7][8] = {
	{ S123, S122, S122, S122, S133, S133, S123 },
	{ S113, S111, S111, S111, S444, S444, S113 },
	{ S113, S111, S111, S111, S444, S444, S113 },
	{ S111, S111, S111, S111, S333, S333, S113 },
	{ S222, S222, S222, S222, S333, S333, S123 },
	{ S222, S222, S222, S222, S333, S333, S123 },
	{ S123, S122, S122, S122, S133, S133, S123 }
};

static struct audio_seg
{
	struct bs_rec		out;
	int			sb_samples[2][3][12][SBLIMIT];	// channel, scale group, sample, subband
	char			bit_alloc[2][SBLIMIT];
	struct {
		unsigned char mant, exp;
	}			scalar[2][3][SBLIMIT];		// channel, scale group, subband
	unsigned int		scf[2][SBLIMIT];
	char			scfsi[2][SBLIMIT];
	float			mnr[2][SBLIMIT];

	unsigned char		sb_group[SBLIMIT];
	unsigned char		bits[NUM_SG][MAX_BA_INDICES];
	struct {
		unsigned char quant, shift;
	}			qs[NUM_SG][MAX_BA_INDICES];
	unsigned char		nbal[SBLIMIT];
	unsigned short		bit_incr[8][MAX_BA_INDICES];
	float			mnr_incr[8][SBLIMIT];
	unsigned char		spattern[7][8];
	unsigned char		steps[4];
	unsigned char		sfsPerScfsi[4];
	unsigned char		packed[NUM_SG];
	int			maxr[3];

} aseg __attribute__ ((aligned (4096)));

fifo *			audio_fifo;

int			mpeg_version;
int			bit_rate_code;
int			sampling_freq_code;
int			table;
int			sblimit;
int			sum_nbal;
int			channels;
int			jsbound;
unsigned int		header_template;
int			audio_frame_count;

extern int		mux_mode;
extern int		stereo;
extern int		audio_num_frames;
extern double		audio_stop_time;

double			avg_slots_per_frame,
			frac_SpF,
			slot_lag;
int			whole_SpF;


void
audio_parameters(int *sampling_freq, int *bit_rate)
{
	int i, imin;
	unsigned int dmin;

	imin = 0;
        dmin = UINT_MAX;

	for (i = 0; i < 16; i++)
		if (sampling_freq_value[0][i] > 0)
		{
			unsigned int d = nbabs(*sampling_freq - sampling_freq_value[0][i]);

			if (d < dmin) {	    
				dmin = d;
	    			imin = i;
			}
		}

	mpeg_version = imin / 4;
	sampling_freq_code = imin % 4;

	*sampling_freq = sampling_freq_value[mpeg_version][sampling_freq_code];

	if (mpeg_version == MPEG_VERSION_2_5)
		FAIL("No tables for %d Hz sampling rate (MPEG 2.5)", *sampling_freq);

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

	bit_rate_code = imin;

	*bit_rate = bit_rate_value[mpeg_version][bit_rate_code];
}

void
audio_init(void)
{
	int sb, bit_rate, bit_rate_per_ch, sampling_freq;

	if (audio_mode == AUDIO_MODE_JOINT_STEREO)
		audio_mode = AUDIO_MODE_STEREO;

	channels = 1 << stereo;

	binit_write(&aseg.out);

        bit_rate = bit_rate_value[mpeg_version][bit_rate_code];
        bit_rate_per_ch = bit_rate / channels;
        sampling_freq = sampling_freq_value[mpeg_version][sampling_freq_code];

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

	for (sblimit = SBLIMIT; sblimit > 0; sblimit--)
		if (subband_group[table][sblimit - 1])
			break;

	printv(3, "Audio table #%d, %d Hz\n", table, sampling_freq * sblimit / 32);

	sum_nbal = 0;

	for (sb = 0; sb < sblimit; sb++)
	{
		int ba, ba_indices;
		int sg = subband_group[table][sb];

		// Subband number to index into tables of subband properties

		aseg.sb_group[sb] = sg;

		// 1 << n indicates packed sample encoding in subband n

		aseg.packed[sg] = pack_table[sg];

		// Bit allocation index into bits-per-sample table

		for (ba_indices = MAX_BA_INDICES; ba_indices > 0; ba_indices--)
			if (bits_table[sg][ba_indices - 1] > 0)
				break;

		for (ba = 0; ba < ba_indices; ba++)
			aseg.bits[sg][ba] = bits_table[sg][ba];

		// Quantization step size for packed samples

		for (ba = 0; ba < ba_indices; ba++)
			if ((int) pack_table[sg] & (1 << ba))
				aseg.steps[ba - 1] = steps_table[sg][ba];

		for (ba = 0; ba < ba_indices; ba++)
			aseg.qs[sg][ba].quant = quant_table[sg][ba];

		// How many bits to encode bit allocation index

		aseg.nbal[sb] = ffs(ba_indices) - 1;
		sum_nbal += ffs(ba_indices) - 1;

		// How many bits to encode, and mnr gained, for a subband
		// when increasing its bit allocation index ba (0..n) by one

		for (ba = 0; ba < 16; ba++)
			if (ba + 1 >=  ba_indices) {
				aseg.bit_incr[sg][ba] = 32767; // All bits allocated
				aseg.mnr_incr[sg][ba] = 0;
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

				aseg.bit_incr[sg][ba] = bit_incr;
				aseg.mnr_incr[sg][ba] = 1.0 / pow(10.0, mnr_incr * 0.1); // see psycho.c
			}
	}

	{
		int i, steps, n;

		for (i = 0; i < NUM_SG * MAX_BA_INDICES; i++)
			if (aseg.qs[0][i].quant == 0)
				aseg.qs[0][i].quant = 2;
			else if (aseg.qs[0][i].quant == 1)
				aseg.qs[0][i].quant = 0;
			else if (aseg.qs[0][i].quant == 2)
				aseg.qs[0][i].quant = 3;
			else if (aseg.qs[0][i].quant == 3)
				aseg.qs[0][i].quant = 1;

		for (i = 0; i < NUM_SG * MAX_BA_INDICES; i++) {
			n = 0;
			steps = steps_table[0][i];
			while ((1L << n) < steps) n++;
			aseg.qs[0][i].shift = n - 1;
		}
	}

	aseg.maxr[0] = ((double)(1 << 30) / M0 + 0.5);
	aseg.maxr[1] = ((double)(1 << 30) / M1 + 0.5);
	aseg.maxr[2] = ((double)(1 << 30) / M2 + 0.5);

	aseg.sfsPerScfsi[0] = 3 * 6;
	aseg.sfsPerScfsi[1] = 2 * 6;
	aseg.sfsPerScfsi[2] = 1 * 6;
	aseg.sfsPerScfsi[3] = 2 * 6;

	memcpy(aseg.spattern, spattern, sizeof(spattern));

    	avg_slots_per_frame = ((double) bit_rate / BITS_PER_SLOT) /
			      ((double) sampling_freq_value[mpeg_version][sampling_freq_code] / SAMPLES_PER_FRAME);

	whole_SpF = floor(avg_slots_per_frame);

	frac_SpF  = avg_slots_per_frame - (double) whole_SpF;
	slot_lag  = -frac_SpF;

	header_template =
		(0x7FF << 21) | (mpeg_version << 19) | (LAYER_II << 17) | (1 << 16) |
		/* sync, version, layer, CRC none */
		(bit_rate_code << 12) | (sampling_freq_code << 10) | (0 << 8) |
		/* bit rate, sampling rate, private_bit */
		(audio_mode << 6) | (0 << 4) | (0 << 3) | (0 << 2) | 0;
		/* audio_mode, mode_ext, copyright no, original no, emphasis none */

	subband_filter_init();
	psycho_init();

	audio_frame_count = 0;

	audio_fifo = mux_add_input_stream(AUDIO_STREAM,
		2048 << stereo, aud_buffers,
		sampling_freq / (double) SAMPLES_PER_FRAME, bit_rate);
}

static void
terminate(void)
{
	buffer *obuf;

	printv(2, "Audio: End of file\n");

	for (;;) {
		obuf = wait_empty_buffer(audio_fifo);
		obuf->used = 0;
		// XXX other?
		send_full_buffer(audio_fifo, obuf);
	}
}

void *
mpeg_audio_layer_ii_mono(void *unused)
{
	// fpu_control(FPCW_PRECISION_SINGLE, FPCW_PRECISION_MASK);

	for (;;) {
		buffer *ibuf, *obuf;
		unsigned int adb, bpf;
		double time;

		header_template &= ~(1 << 9);
		adb = whole_SpF * BITS_PER_SLOT;

		if (slot_lag > (frac_SpF - 1.0)) {
			slot_lag -= frac_SpF;
		} else {
			header_template |= 1 << 9; /* padded */
			adb += BITS_PER_SLOT;
			slot_lag += (1.0 - frac_SpF);
		}

		bpf = adb >> 3;

		// FDCT and psychoacoustic analysis

		if (audio_frame_count > audio_num_frames)
			terminate();

		ibuf = wait_full_buffer(audio_cap_fifo);

		if (!ibuf || ibuf->time >= audio_stop_time) {
			if (ibuf)
				send_empty_buffer(audio_cap_fifo, ibuf);
			terminate();
		}

		time = ibuf->time;

		audio_frame_count++;

		pr_start(38, "Audio frame (1152 samples)");
		pr_start(35, "Subband filter x 36");

		mmx_filter_mono((short *) ibuf->data, aseg.sb_samples[0][0][0]);

		pr_end(35);

		pr_start(34, "Psychoacoustic analysis");

		psycho((short *) ibuf->data, aseg.mnr[0], 1);

		pr_end(34);

		send_empty_buffer(audio_cap_fifo, ibuf);

		pr_start(36, "Bit twiddling");

		// Calculate scale factors and transmission pattern

		{
			int sb;

			for (sb = 0; sb < sblimit; sb++)
			{
				int t, s[3];

				for (t = 0; t < 3; t++)
				{
					unsigned int sx = nbabs(aseg.sb_samples[0][t][0][sb]);
					int j, e, m;

					for (j = 1; j < SCALE_BLOCK; j++) {
						unsigned int s = nbabs(aseg.sb_samples[0][t][j][sb]);
						if (s > sx) sx = s;
					}

					sx <<= e = 30 - ffsr(sx | (1 << 10 /* SCALE_RANGE 0..62 */));
					m = (sx < ((int)(M1 * (double)(1 << 30) + .5))) +
					    (sx < ((int)(M2 * (double)(1 << 30) + .5)));

					aseg.scalar[0][t][sb].mant = m;
					aseg.scalar[0][t][sb].exp = e;

					s[t] = e * 3 + m;
				}

				{
					int d0, d1;

					d0 = s[0] - s[1]; if (d0 < -3) d0 = -3; else if (d0 > +3) d0 = +3;
					d1 = s[1] - s[2]; if (d1 < -3) d1 = -3; else if (d1 > +3) d1 = +3;

					switch (spattern[d0 + 3][d1 + 3]) {
					case S123:  aseg.scfsi[0][sb] = 0;
						    aseg.scf[0][sb] = (s[0] << 12) | (s[1] << 6) | s[2];
						    break;
					case S122:  aseg.scfsi[0][sb] = 3;
						    aseg.scf[0][sb] = (s[0] << 6) | s[1];
						    aseg.scalar[0][2][sb] = aseg.scalar[0][1][sb];
						    break;
					case S133:  aseg.scfsi[0][sb] = 3; aseg.scf[0][sb] = (s[0] << 6) | s[2];
						    aseg.scalar[0][1][sb] = aseg.scalar[0][2][sb];
						    break;
					case S113:  aseg.scfsi[0][sb] = 1; aseg.scf[0][sb] = (s[0] << 6) | s[2];
						    aseg.scalar[0][1][sb] = aseg.scalar[0][0][sb];
						    break;
					case S444:  if (s[0] > s[2]) {
							    aseg.scfsi[0][sb] = 2; aseg.scf[0][sb] = s[2];
						    	    aseg.scalar[0][1][sb] = aseg.scalar[0][0][sb] = aseg.scalar[0][2][sb];
						    	    break;
						    } // else fall through
					case S111:  aseg.scfsi[0][sb] = 2; aseg.scf[0][sb] = s[0];
						    aseg.scalar[0][1][sb] = aseg.scalar[0][2][sb] = aseg.scalar[0][0][sb];
						    break;
					case S222:  aseg.scfsi[0][sb] = 2; aseg.scf[0][sb] = s[1];
						    aseg.scalar[0][0][sb] = aseg.scalar[0][2][sb] = aseg.scalar[0][1][sb];
						    break;
					case S333:  aseg.scfsi[0][sb] = 2; aseg.scf[0][sb] = s[2];
						    aseg.scalar[0][0][sb] = aseg.scalar[0][1][sb] = aseg.scalar[0][2][sb];
						    break;
					}
				}
			}
		}

		// Bit allocation

		{
			int i, incr, ba, bg, sb;

			memset(aseg.bit_alloc, 0, sizeof(aseg.bit_alloc));

			adb -= HEADER_BITS + sum_nbal; // + auxiliary bits

			for (;;)
			{
				double min = -1e37;
				sb = -1;

				for (i = 0; i < sblimit; i++)
					if (aseg.mnr[0][i] > min) {
						min = aseg.mnr[0][i];
						sb = i;
    					}

				if (sb < 0) break; // all done

				ba = aseg.bit_alloc[0][sb];
				bg = aseg.sb_group[sb];
				incr = aseg.bit_incr[bg][ba];

				if (ba == 0)
					incr += aseg.sfsPerScfsi[(int) aseg.scfsi[0][sb]] - 3 * 6;

				if (incr <= adb)
				{
					aseg.bit_alloc[0][sb] = ba + 1;
					adb -= incr;

					if (aseg.bit_incr[bg][ba + 1] > adb)
						aseg.mnr[0][sb] = -1e38; // all bits allocated
					else
						aseg.mnr[0][sb] *= aseg.mnr_incr[bg][ba];
				} else
    					aseg.mnr[0][sb] = -1e38; // can't incr bits for this band
			}

			// DUMP(aseg.bit_alloc[0], 0, SBLIMIT);
		}

		// Scale and quantize samples

		{
			int t, j, sb, ba;

			for (t = 0; t < 3; t++)
				for (j = 0; j < SCALE_BLOCK; j++)
					for (sb = 0; sb < sblimit; sb++)
						if ((ba = aseg.bit_alloc[0][sb]))
						{
							int sg  = aseg.sb_group[sb];
							int qnt = aseg.qs[sg][ba].quant;
							int n   = aseg.qs[sg][ba].shift;

							// -1.0*2**30 < sb_sample < 1.0*2**30

							int di = ((long long)(aseg.sb_samples[0][t][j][sb] << aseg.scalar[0][t][sb].exp) *
								aseg.maxr[(int) aseg.scalar[0][t][sb].mant]) >> 32;

						        di += 1 << 28; // += 1.0

							if (qnt < 2)
								aseg.sb_samples[0][t][j][sb] = (di + (di >> (qnt += 2))) >> (29 - n);
							else
								aseg.sb_samples[0][t][j][sb] = (di - (di >> qnt)) >> (28 - n);
						}
		}

		pr_end(36);

		obuf = wait_empty_buffer(audio_fifo);

		bstart(&aseg.out, obuf->data);

		aseg.out.buf = MMXD(0, header_template);
		aseg.out.n = 32;

		pr_start(37, "Encoding");

		bepilog(&aseg.out);

		// Encode

		{
			int j, sb = 0;

			for (; sb < sblimit; sb++)
				bputl(&aseg.out, aseg.bit_alloc[0][sb], aseg.nbal[sb]);

			for (sb = 0; sb < sblimit; sb++)
		    		if (aseg.bit_alloc[0][sb])
					bputl(&aseg.out, aseg.scfsi[0][sb], 2);

			for (sb = 0; sb < sblimit; sb++)
		    		if (aseg.bit_alloc[0][sb])
					bputl(&aseg.out, aseg.scf[0][sb], aseg.sfsPerScfsi[(int) aseg.scfsi[0][sb]]);

			for (j = 0; j < 3 * SCALE_BLOCK; j += 3)
				for (sb = 0; sb < sblimit; sb++)
				{
					int ba;

					if ((ba = aseg.bit_alloc[0][sb]))
					{
						int sg = aseg.sb_group[sb];
						int bi = aseg.bits[sg][ba];

						if ((int) aseg.packed[sg] & (1 << ba))
						{
							int t, y = aseg.steps[ba - 1];

					    	        t = (    aseg.sb_samples[0][0][j + 2][sb]) * y;
							t = (t + aseg.sb_samples[0][0][j + 1][sb]) * y;
						        t = (t + aseg.sb_samples[0][0][j + 0][sb]);

							bputl(&aseg.out, t, bi);
						}
						else
						{
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

		obuf->used = bpf;
		obuf->time = time;

		send_full_buffer(audio_fifo, obuf);
	}

	return NULL; // never
}

void *
mpeg_audio_layer_ii_stereo(void *unused)
{
	// fpu_control(FPCW_PRECISION_SINGLE, FPCW_PRECISION_MASK);

	for (;;) {
		buffer *ibuf, *obuf;
		unsigned int adb, bpf;
		double time;

		header_template &= ~(1 << 9);
		adb = whole_SpF * BITS_PER_SLOT;

		if (slot_lag > (frac_SpF - 1.0)) {
			slot_lag -= frac_SpF;
		} else {
			header_template |= 1 << 9; /* padded */
			adb += BITS_PER_SLOT;
			slot_lag += (1.0 - frac_SpF);
		}

		bpf = adb >> 3;

		// FDCT and psychoacoustic analysis

		if (audio_frame_count > audio_num_frames)
			terminate();

		ibuf = wait_full_buffer(audio_cap_fifo);

		if (!ibuf || ibuf->time >= audio_stop_time) {
			if (ibuf)
				send_empty_buffer(audio_cap_fifo, ibuf);
			terminate();
		}

		time = ibuf->time;

		audio_frame_count++;

		pr_start(38, "Audio frame (2304 samples)");
		pr_start(35, "Subband filter x 72");

		mmx_filter_stereo((short *) ibuf->data, aseg.sb_samples[0][0][0]);

		pr_end(35);

		pr_start(34, "Psychoacoustic analysis");

		psycho(((short *) ibuf->data),     aseg.mnr[0], 2);
		psycho(((short *) ibuf->data) + 1, aseg.mnr[1], 2);

		pr_end(34);

		send_full_buffer(audio_cap_fifo, ibuf);

		pr_start(36, "Bit twiddling");

		// Calculate scale factors and transmission pattern

		{
			int sb;

			for (sb = 0; sb < sblimit; sb++)
			{
				int t, ch, s[6];

				for (t = 0; t < 2 * 3; t++)
				{
					unsigned int sx = nbabs(aseg.sb_samples[0][t][0][sb]);
					int j, e, m;

					for (j = 1; j < SCALE_BLOCK; j++) {
						unsigned int s = nbabs(aseg.sb_samples[0][t][j][sb]);
						if (s > sx) sx = s;
					}

					sx <<= e = 30 - ffsr(sx | (1 << 10 /* SCALE_RANGE 0..62 */));
					m = (sx < ((int)(M1 * (double)(1 << 30) + .5))) +
					    (sx < ((int)(M2 * (double)(1 << 30) + .5)));

					aseg.scalar[0][t][sb].mant = m;
					aseg.scalar[0][t][sb].exp = e;

					s[t] = e * 3 + m;
				}

				for (ch = 0; ch < 2; ch++)
				{
					int d0, d1;
					int *sp = &s[3 * ch];

					d0 = sp[0] - sp[1]; if (d0 < -3) d0 = -3; else if (d0 > +3) d0 = +3;
					d1 = sp[1] - sp[2]; if (d1 < -3) d1 = -3; else if (d1 > +3) d1 = +3;

					switch (spattern[d0 + 3][d1 + 3]) {
					case S123:  aseg.scfsi[ch][sb] = 0;
						    aseg.scf[ch][sb] = (sp[0] << 12) | (sp[1] << 6) | sp[2];
						    break;
					case S122:  aseg.scfsi[ch][sb] = 3;
						    aseg.scf[ch][sb] = (sp[0] << 6) | sp[1];
						    aseg.scalar[ch][2][sb] = aseg.scalar[ch][1][sb];
						    break;
					case S133:  aseg.scfsi[ch][sb] = 3; aseg.scf[ch][sb] = (sp[0] << 6) | sp[2];
						    aseg.scalar[ch][1][sb] = aseg.scalar[ch][2][sb];
						    break;
					case S113:  aseg.scfsi[ch][sb] = 1; aseg.scf[ch][sb] = (sp[0] << 6) | sp[2];
						    aseg.scalar[ch][1][sb] = aseg.scalar[ch][0][sb];
						    break;
					case S444:  if (sp[0] > sp[2]) {
							    aseg.scfsi[ch][sb] = 2; aseg.scf[ch][sb] = sp[2];
						    	    aseg.scalar[ch][1][sb] = aseg.scalar[ch][0][sb] = aseg.scalar[ch][2][sb];
						    	    break;
						    } // else fall through
					case S111:  aseg.scfsi[ch][sb] = 2; aseg.scf[ch][sb] = sp[0];
						    aseg.scalar[ch][1][sb] = aseg.scalar[ch][2][sb] = aseg.scalar[ch][0][sb];
						    break;
					case S222:  aseg.scfsi[ch][sb] = 2; aseg.scf[ch][sb] = sp[1];
						    aseg.scalar[ch][0][sb] = aseg.scalar[ch][2][sb] = aseg.scalar[ch][1][sb];
						    break;
					case S333:  aseg.scfsi[ch][sb] = 2; aseg.scf[ch][sb] = sp[2];
						    aseg.scalar[ch][0][sb] = aseg.scalar[ch][1][sb] = aseg.scalar[ch][2][sb];
						    break;
					}
				}
			}
		}

		// Bit allocation

		{
			int i, incr, ba, bg, sb, ch = 0;

			memset(aseg.bit_alloc, 0, sizeof(aseg.bit_alloc));

			adb -= HEADER_BITS + sum_nbal * 2; // + auxiliary bits

			for (;;)
			{
				double min = -1e37;
				sb = -1;

				for (i = 0; i < sblimit; i++) {
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
				bg = aseg.sb_group[sb];
				incr = aseg.bit_incr[bg][ba];

				if (ba == 0)
					incr += aseg.sfsPerScfsi[(int) aseg.scfsi[ch][sb]] - 3 * 6;

				if (incr <= adb)
				{
					aseg.bit_alloc[ch][sb] = ba + 1;
					adb -= incr;

					if (aseg.bit_incr[bg][ba + 1] > adb)
						aseg.mnr[ch][sb] = -1e38; // all bits allocated
					else
						aseg.mnr[ch][sb] *= aseg.mnr_incr[bg][ba];
				} else
    					aseg.mnr[ch][sb] = -1e38; // can't incr bits for this band
			}
		}

		// Scale and quantize samples

		{
			int t, j, sb, ba;

			for (t = 0; t < 2 * 3; t++) {
				for (j = 0; j < SCALE_BLOCK; j++)
					for (sb = 0; sb < sblimit; sb++)
						if ((ba = aseg.bit_alloc[t >= 3][sb]))
						{
							int sg  = aseg.sb_group[sb];
							int qnt = aseg.qs[sg][ba].quant;
							int n   = aseg.qs[sg][ba].shift;

							// -1.0*2**30 < sb_sample < 1.0*2**30

							int di = ((long long)(aseg.sb_samples[0][t][j][sb] << aseg.scalar[0][t][sb].exp) *
								aseg.maxr[(int) aseg.scalar[0][t][sb].mant]) >> 32;

						        di += 1 << 28; // += 1.0

							if (qnt < 2)
								aseg.sb_samples[0][t][j][sb] = (di + (di >> (qnt += 2))) >> (29 - n);
							else
								aseg.sb_samples[0][t][j][sb] = (di - (di >> qnt)) >> (28 - n);
						}
			}
		}

		pr_end(36);

		obuf = wait_empty_buffer(audio_fifo);

		bstart(&aseg.out, obuf->data);

		aseg.out.buf = MMXD(0, header_template);
		aseg.out.n = 32;

		pr_start(37, "Encoding");

		bepilog(&aseg.out);

		// Encode

		{
			int j, sb = 0;

			// Bit allocation table

			for (; sb < sblimit; sb++) {
				int bi = aseg.nbal[sb];
				bputl(&aseg.out, (aseg.bit_alloc[0][sb] << bi) + aseg.bit_alloc[1][sb], bi * 2);
			}

			// Scale factor selector information

			for (sb = 0; sb < sblimit; sb++) {
		    		if (aseg.bit_alloc[0][sb])
					bputl(&aseg.out, aseg.scfsi[0][sb], 2);
		    		if (aseg.bit_alloc[1][sb])
					bputl(&aseg.out, aseg.scfsi[1][sb], 2);
			}

			// Scale factors

			for (sb = 0; sb < sblimit; sb++) {
		    		if (aseg.bit_alloc[0][sb])
					bputl(&aseg.out, aseg.scf[0][sb], aseg.sfsPerScfsi[(int) aseg.scfsi[0][sb]]);
		    		if (aseg.bit_alloc[1][sb])
					bputl(&aseg.out, aseg.scf[1][sb], aseg.sfsPerScfsi[(int) aseg.scfsi[1][sb]]);
			}

			// Samples
	
			for (j = 0; j < 3 * SCALE_BLOCK; j += 3)
				for (sb = 0; sb < sblimit; sb++)
				{
					int sg = aseg.sb_group[sb];
					int pk = aseg.packed[sg];
					int ba;

					if ((ba = aseg.bit_alloc[0][sb]))
					{
						int bi = aseg.bits[sg][ba];

						if (pk & (1 << ba))
						{
							int t, y = aseg.steps[ba - 1];

					    	        t = (    aseg.sb_samples[0][0][j + 2][sb]) * y;
							t = (t + aseg.sb_samples[0][0][j + 1][sb]) * y;
						        t = (t + aseg.sb_samples[0][0][j + 0][sb]);

							bputl(&aseg.out, t, bi);
						}
						else
						{
							int t = aseg.sb_samples[0][0][j + 0][sb] << bi;
		
							bputl(&aseg.out, t | aseg.sb_samples[0][0][j + 1][sb], bi << 1);
							bputl(&aseg.out, aseg.sb_samples[0][0][j + 2][sb], bi);
						}
					}

					if ((ba = aseg.bit_alloc[1][sb]))
					{
						int bi = aseg.bits[sg][ba];

						if (pk & (1 << ba))
						{
							int t, y = aseg.steps[ba - 1];

					    	        t = (    aseg.sb_samples[1][0][j + 2][sb]) * y;
							t = (t + aseg.sb_samples[1][0][j + 1][sb]) * y;
						        t = (t + aseg.sb_samples[1][0][j + 0][sb]);

							bputl(&aseg.out, t, bi);
						}
						else
						{
							int t = aseg.sb_samples[1][0][j + 0][sb] << bi;
		
							bputl(&aseg.out, t | aseg.sb_samples[1][0][j + 1][sb], bi << 1);
							bputl(&aseg.out, aseg.sb_samples[1][0][j + 2][sb], bi);
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

		obuf->used = bpf;
		obuf->time = time;

		send_full_buffer(audio_fifo, obuf);
	}

	return NULL; // never
}
