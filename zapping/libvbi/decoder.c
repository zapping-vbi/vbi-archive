/*
 *  Raw VBI Decoder
 *
 *  Copyright (C) 2000 Michael H. Schimek
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

/* $Id: decoder.c,v 1.5 2001-06-18 12:33:58 mschimek Exp $ */

/*
    XXX NTSC transmits 0-4 (AFAIS) CC packets per frame,
    and there's no way to identify true closed caption other than
    the line number.
 */

#include <stdlib.h>
#include <string.h>
#include "decoder.h"
#include "../common/math.h"

/*
 *  Bit Slicer
 */

#define OVERSAMPLING 4		// 1, 2, 4, 8

static inline int
sample(unsigned char *raw, int offs)
{
	unsigned char frac = offs;

	raw += offs >> 8;

	return (raw[1] - raw[0]) * frac + (raw[0] << 8);
}

bool
bit_slicer(struct bit_slicer *d, unsigned char *raw, unsigned char *buf)
{
	int i, j, k, cl = 0, thresh0 = d->thresh;
	unsigned int c = 0, t;
	unsigned char b, b1 = 0, tr;

	for (i = d->cri_bytes; i > 0; raw++, i--) {
		tr = d->thresh >> 9;
		d->thresh += ((int) raw[0] - tr) * nbabs(raw[1] - raw[0]);
		t = raw[0] * OVERSAMPLING;

		for (j = OVERSAMPLING; j > 0; j--) {
			b = ((t + (OVERSAMPLING / 2)) / OVERSAMPLING >= tr);

    			if (b ^ b1) {
				cl = d->oversampling_rate >> 1;
			} else {
				cl += d->cri_rate;

				if (cl >= d->oversampling_rate) {
					cl -= d->oversampling_rate;

					c = c * 2 + b;

					if ((c & d->cri_mask) == d->cri) {
						i = d->phase_shift;
						c = 0;

						for (j = d->frc_bits; j > 0; j--) {
							c = c * 2 + (sample(raw, i) >= (tr * 256));
    							i += d->step;
						}

						if (c ^= d->frc)
							return FALSE;

						switch (d->endian) {
						case 3:
							for (j = 0; j < d->payload; j++) {
					    			c >>= 1;
								c += (sample(raw, i) >= tr) << 7;
			    					i += d->step;

								if ((j & 7) == 7)
									*buf++ = c;
					    		}

							*buf = c >> ((8 - d->payload) & 7);
							break;

						case 2:
							for (j = 0; j < d->payload; j++) {
								c = c * 2 + (sample(raw, i) >= tr);
			    					i += d->step;

								if ((j & 7) == 7)
									*buf++ = c;
					    		}

							*buf = c & ((1 << (d->payload & 7)) - 1);
							break;

						case 1:
							for (j = d->payload; j > 0; j--) {
								for (k = 0; k < 8; k++) {
						    			c >>= 1;
									c += (sample(raw, i) >= tr * 256) << 7;
			    						i += d->step;
								}

								*buf++ = c;
					    		}

							break;

						case 0:
							for (j = d->payload; j > 0; j--) {
								for (k = 0; k < 8; k++) {
									c = c * 2 + (sample(raw, i) >= tr * 256);
			    						i += d->step;
								}

								*buf++ = c;
					    		}

							break;
						}

			    			return TRUE;
					}
				}
			}

			b1 = b;

			if (OVERSAMPLING > 1) {
				t += raw[1];
				t -= raw[0];
			}
		}
	}

	d->thresh = thresh0;

	return FALSE;
}

void
init_bit_slicer(struct bit_slicer *d,
	int raw_bytes, int sampling_rate, int cri_rate, int bit_rate,
	unsigned int cri_frc, unsigned int cri_mask,
	int cri_bits, int frc_bits, int payload, int modulation)
{
	unsigned int c_mask = (unsigned int)(-(cri_bits > 0)) >> (32 - cri_bits);
	unsigned int f_mask = (unsigned int)(-(frc_bits > 0)) >> (32 - frc_bits);

	d->cri_mask		= cri_mask & c_mask;
	d->cri		 	= (cri_frc >> frc_bits) & d->cri_mask;
	d->cri_bytes		= raw_bytes
		- ((long long) sampling_rate * (payload + frc_bits)) / bit_rate;
	d->cri_rate		= cri_rate;
	d->oversampling_rate	= sampling_rate * OVERSAMPLING;
	d->thresh		= 105 << 9;
	d->frc			= cri_frc & f_mask;
	d->frc_bits		= frc_bits;
	d->step			= sampling_rate * 256.0 / bit_rate;

	if (payload & 7) {
		d->payload	= payload;
		d->endian	= 3;
	} else {
		d->payload	= payload >> 3;
		d->endian	= 1;
	}

	switch (modulation) {
	case MOD_NRZ_MSB_ENDIAN:
		d->endian--;
	case MOD_NRZ_LSB_ENDIAN:
		d->phase_shift = sampling_rate * 256.0 / cri_rate * .5
			         + sampling_rate * 256.0 / bit_rate * .5 + 128;
		break;

	case MOD_BIPHASE_MSB_ENDIAN:
		d->endian--;
	case MOD_BIPHASE_LSB_ENDIAN:
		d->phase_shift = sampling_rate * 256.0 / cri_rate * .5
			         + sampling_rate * 256.0 / bit_rate * .25 + 128;
		break;
	}
}

/*
 *  Data Service Decoder
 */

struct vbi_service_par {
	unsigned int	id;		/* SLICED_ */
	char *		label;
	int		first[2];	/* scanning lines (ITU-R), max. distribution; */
	int		last[2];	/*  zero: no data from this field, requires field sync */
	int		offset;		/* leading edge hsync to leading edge first CRI one bit
					    half amplitude points, nanoseconds */
	int		cri_rate;	/* Hz */
	int		bit_rate;	/* Hz */
	int		scanning;	/* scanning system: 525 (FV = 59.94 Hz, FH = 15734 Hz),
							    625 (FV = 50 Hz, FH = 15625 Hz) */
	unsigned int	cri_frc;	/* Clock Run In and FRaming Code, LSB last txed bit of FRC */
	unsigned int	cri_mask;	/* cri_frc bits significant for identification, */
	char		cri_bits;	/*  advice: ignore leading CRI bits; */
	char		frc_bits;	/*  cri_bits at cri_rate, frc_bits at bit_rate */
	short		payload;	/* in bytes */
	char		modulation;	/* payload modulation */
};

const struct vbi_service_par
vbi_services[] = {
	{
		SLICED_TELETEXT_B_L10_625, "Teletext System B Level 1.5, 625",
		{ 7, 320 },
		{ 22, 335 },
		10300, 6937500, 6937500, /* 444 x FH */
		625, 0x00AAAAE4, ~0, 10, 6, 42 * 8, MOD_NRZ_LSB_ENDIAN
	}, {
		SLICED_TELETEXT_B_L25_625, "Teletext System B, 625",
		{ 6, 318 },
		{ 22, 335 },
		10300, 6937500, 6937500, /* 444 x FH */
		625, 0x00AAAAE4, ~0, 10, 6, 42 * 8, MOD_NRZ_LSB_ENDIAN
	}, {
		SLICED_VPS, "Video Programming System",
		{ 16, 0 },
		{ 16, 0 },
		12500, 5000000, 2500000, /* 160 x FH */
		625, 0xAAAA8A99, ~0, 24, 0, 13 * 8, MOD_BIPHASE_MSB_ENDIAN
	}, {
		SLICED_WSS_625, "Wide Screen Signalling 625",
		{ 23, 0 },
		{ 23, 0 },
		11000, 5000000, 833333, /* 160/3 x FH */
		625, 0xC71E3C1F, 0x924C99CE, 32, 0, 14 * 1, MOD_BIPHASE_LSB_ENDIAN
	}, {
		SLICED_CAPTION_625_F1, "Closed Caption 625, single field",
		{ 22, 0 },
		{ 22, 0 },
		10500, 1000000, 500000, /* 32 x FH */
		625, 0x00005551, ~0, 9, 2, 2 * 8, MOD_NRZ_LSB_ENDIAN
	}, {
		SLICED_CAPTION_625, "Closed Caption 625", /* Videotapes, LD */
		{ 22, 335 },
		{ 22, 335 },
		10500, 1000000, 500000, /* 32 x FH */
		625, 0x00005551, ~0, 9, 2, 2 * 8, MOD_NRZ_LSB_ENDIAN
	}, {
		SLICED_VBI_625, "VBI 625", /* Blank VBI */
		{ 6, 318 },
		{ 22, 335 },
		10000, 1510000, 1510000,
		625, 0, 0, 0, 0, 10 * 8, 0 /* 10.0-2 ... 62.9+1 us */
	}, {
		SLICED_NABTS, "Teletext System C, 525", /* NOT CONFIRMED */
		{ 10, 0 },
		{ 21, 0 },
		10500, 5727272, 5727272,
		525, 0x00AAAAE7, ~0, 10, 6, 33 * 8, MOD_NRZ_LSB_ENDIAN /* Tb. */
	}, {
		SLICED_WSS_CPR1204, "Wide Screen Signalling 525",
		/* NOT CONFIRMED (EIA-J CPR-1204) */
		{ 20, 283 },
		{ 20, 283 },
		11200, 3579545, 447443, /* 1/8 x FSC */
		525, 0x0000FF00, 0x00003C3C, 16, 0, 20 * 1, MOD_NRZ_MSB_ENDIAN
		/* No useful FRC, but a six bit CRC */
	}, {
		SLICED_CAPTION_525_F1, "Closed Caption 525, single field",
		{ 21, 0 },
		{ 21, 0 },
		10500, 1006976, 503488, /* 32 x FH */
		525, 0x00005551, ~0, 9, 2, 2 * 8, MOD_NRZ_LSB_ENDIAN
	}, {
		SLICED_CAPTION_525, "Closed Caption 525",
		{ 21, 284 },
		{ 21, 284 },
		10500, 1006976, 503488, /* 32 x FH */
		525, 0x00005551, ~0, 9, 2, 2 * 8, MOD_NRZ_LSB_ENDIAN
	}, {
		SLICED_2xCAPTION_525, "2xCaption 525", /* NOT CONFIRMED */
		{ 10, 0 },
		{ 21, 0 },
		10500, 1006976, 1006976, /* 64 x FH */
		525, 0x000554ED, ~0, 8, 8, 4 * 8, MOD_NRZ_LSB_ENDIAN /* Tb. */
	}, {
		SLICED_TELETEXT_BD_525, "Teletext System B / D (Japan), 525", /* NOT CONFIRMED */
		{ 10, 0 },
		{ 21, 0 },
		9600, 5727272, 5727272,
		525, 0x00AAAAE4, ~0, 10, 6, 34 * 8, MOD_NRZ_LSB_ENDIAN /* Tb. */
	}, {
		SLICED_VBI_525, "VBI 525", /* Blank VBI */
		{ 10, 272 },
		{ 21, 284 },
		9500, 1510000, 1510000,
		525, 0, 0, 0, 0, 10 * 8, 0 /* 9.5-1 ... 62.4+1 us */
	},
	{ 0 }
};

int
vbi_decoder(struct vbi_decoder *vbi, unsigned char *raw1, vbi_sliced *out1)
{
	static int readj = 1;
	int pitch = vbi->samples_per_line << vbi->interlaced;
	char *pat, *pattern = vbi->pattern;
	unsigned char *raw = raw1;
	vbi_sliced *out = out1;
	struct vbi_decoder_job *job;
	int i, j;

	for (i = 0; i < vbi->count[0] + vbi->count[1]; i++) {
		if (vbi->interlaced && i == vbi->count[0])
			raw = raw1 + vbi->samples_per_line;

		for (pat = pattern;; pat++) {
			if ((j = *pat) > 0) {
				job = vbi->jobs + (j - 1);
				if (!bit_slicer(&job->slicer, raw + job->offset, out->data))
					continue;
				if (job->id == SLICED_WSS_CPR1204) {
					const int poly = (1 << 6) + (1 << 1) + 1;
					int crc, j;

					crc = (out->data[0] << 12) + (out->data[1] << 4) + out->data[2];
					crc |= (((1 << 6) - 1) << (14 + 6));

					for (j = 14 + 6 - 1; j >= 0; j--) {
						if (crc & ((1 << 6) << j))
							crc ^= poly << j;
					}

					if (crc)
						continue;
				}
				out->id = job->id;
				if (i >= vbi->count[0])
					out->line = (vbi->start[1] > 0) ? vbi->start[1] - vbi->count[0] + i : 0;
				else
					out->line = (vbi->start[0] > 0) ? vbi->start[0] + i : 0;
				out++;
				pattern[MAX_WAYS - 1] = -128;
			} else if (pat == pattern) {
				if (readj == 0) {
					j = pattern[1];
					pattern[1] = pattern[2];
					pattern[2] = pattern[3];
					pat += MAX_WAYS - 1;
				}
			} else if ((j = pattern[MAX_WAYS - 1]) < 0) {
				pattern[MAX_WAYS - 1] = j + 1;
    				break;
			}

			*pat = pattern[0];
			pattern[0] = j;
			break;
		}

		raw += pitch;
		pattern += MAX_WAYS;
	}

	readj = (readj + 1) & 15;

	return out - out1;
}

unsigned int
remove_vbi_services(struct vbi_decoder *vbi, unsigned int services)
{
	int i, j;
	int pattern_size = (vbi->count[0] + vbi->count[1]) * MAX_WAYS;

	for (i = 0; i < vbi->num_jobs;) {
		if (vbi->jobs[i].id & services) {
			if (vbi->pattern)
				for (j = 0; j < pattern_size; j++)
					if (vbi->pattern[j] == i + 1) {
						int ways_left = MAX_WAYS - 1 - j % MAX_WAYS;

						memmove(&vbi->pattern[j], &vbi->pattern[j + 1],
							ways_left * sizeof(&vbi->pattern[0]));

						vbi->pattern[j + ways_left] = 0;
					}

			memmove(vbi->jobs + i, vbi->jobs + (i + 1),
				(MAX_JOBS - i - 1) * sizeof(vbi->jobs[0]));

			vbi->jobs[--vbi->num_jobs].id = 0;
		} else
			i++;
	}

	return vbi->services &= ~services;
}

unsigned int
add_vbi_services(struct vbi_decoder *vbi, unsigned int services, int strict)
{
	double off_min = (vbi->scanning == 525) ? 7.9e-6 : 8.0e-6;
	int row[2], count[2], way;
	struct vbi_decoder_job *job;
	char *pattern;
	int i, j, k;

	services &= ~(SLICED_VBI_525 | SLICED_VBI_625);

	if (!vbi->pattern)
		vbi->pattern = calloc((vbi->count[0] + vbi->count[1]) * MAX_WAYS, sizeof(vbi->pattern[0]));

	for (i = 0; vbi_services[i].id; i++) {
		double signal;
		int skip = 0;

		if (vbi->num_jobs >= MAX_JOBS)
			break;

		if (!(vbi_services[i].id & services))
			continue;

		if (vbi_services[i].scanning != vbi->scanning)
			goto eliminate;

		signal = vbi_services[i].cri_bits / (double) vbi_services[i].cri_rate
			 + (vbi_services[i].frc_bits + vbi_services[i].payload)
			   / (double) vbi_services[i].bit_rate;

		if (vbi->offset > 0 && strict > 0) {
			double offset = vbi->offset / (double) vbi->sampling_rate;
			double samples_end = (vbi->offset + vbi->samples_per_line)
					     / (double) vbi->sampling_rate;

			if (offset > (vbi_services[i].offset / 1e9 - 0.5e-6))
				goto eliminate;

			if (samples_end < (vbi_services[i].offset / 1e9 + signal + 0.5e-6))
				goto eliminate;

			if (offset < off_min) // skip colour burst
				skip = off_min * vbi->sampling_rate;
		} else {
			double samples = vbi->samples_per_line / (double) vbi->sampling_rate;

			if (samples < (signal + 1.0e-6))
				goto eliminate;
		}

		for (j = 0, job = vbi->jobs; j < vbi->num_jobs; job++, j++) {
			unsigned int id = job->id | vbi_services[i].id;

			if ((id & ~(SLICED_TELETEXT_B_L10_625 | SLICED_TELETEXT_B_L25_625)) == 0
			    || (id & ~(SLICED_CAPTION_625_F1 | SLICED_CAPTION_625)) == 0
			    || (id & ~(SLICED_CAPTION_525_F1 | SLICED_CAPTION_525)) == 0)
				break;
			/*
			 *  General form implies the special form. If both are
			 *  available from the device, decode() will set both
			 *  bits in the id field for the respective line. 
			 */
		}

		for (j = 0; j < 2; j++) {
			int start = vbi->start[j];
			int end = start + vbi->count[j] - 1;

			if (!vbi->synchronous)
				goto eliminate; // too difficult

			if (!(vbi_services[i].first[j] && vbi_services[i].last[j])) {
				count[j] = 0;
				continue;
			}

			if (vbi->count[j] == 0)
				goto eliminate;

			if (vbi->start[j] > 0 && strict > 0) {
				/*
				 *  May succeed if not all scanning lines
				 *  available for the service are actually used.
				 */
				if (strict > 1
				    || (vbi_services[i].first[j] ==
					vbi_services[i].last[j]))
					if (start > vbi_services[i].first[j] ||
					    end < vbi_services[i].last[j])
						goto eliminate;

				row[j] = MAX(0, (int) vbi_services[i].first[j] - start);
				count[j] = MIN(end, vbi_services[i].last[j]) - start + 1;
			} else {
				row[j] = 0;
				count[j] = vbi->count[j];
			}

			row[1] += vbi->count[0];

			for (pattern = vbi->pattern + row[j] * MAX_WAYS, k = count[j]; k > 0; pattern += MAX_WAYS, k--) {
				int free = 0;

				for (way = 0; way < MAX_WAYS; way++)
					free += (pattern[way] <= 0 || (pattern[way] - 1) == job - vbi->jobs);

				if (free <= 1) // reserve one NULL way
					goto eliminate;
			}
		}

		for (j = 0; j < 2; j++)
			for (pattern = vbi->pattern + row[j] * MAX_WAYS, k = count[j]; k > 0; pattern += MAX_WAYS, k--) {
				for (way = 0; pattern[way] > 0 && (pattern[way] - 1) != (job - vbi->jobs); way++);
				pattern[way] = (job - vbi->jobs) + 1;
				pattern[MAX_WAYS - 1] = -128;
			}

		job->id |= vbi_services[i].id;
		job->offset = skip;

		init_bit_slicer(&job->slicer,
				vbi->samples_per_line - skip,
				vbi->sampling_rate,
		    		vbi_services[i].cri_rate,
				vbi_services[i].bit_rate,
				vbi_services[i].cri_frc,
				vbi_services[i].cri_mask,
				vbi_services[i].cri_bits,
				vbi_services[i].frc_bits,
				vbi_services[i].payload,
				vbi_services[i].modulation);

		if (job >= vbi->jobs + vbi->num_jobs)
			vbi->num_jobs++;

		vbi->services |= vbi_services[i].id;
eliminate:
	}

	return vbi->services;
}

unsigned int
qualify_vbi_sampling(struct vbi_decoder *vbi, int *max_rate, unsigned int services)
{
	int i, j;

	vbi->sampling_rate = 27000000;	// ITU-R Rec. 601

	vbi->offset = 1000e-6 * 27e6;
	vbi->start[0] = 1000;
	vbi->count[0] = 0;
	vbi->start[1] = 1000;
	vbi->count[1] = 0;

	/*
	 *  Will only allocate as many vbi lines as
	 *  we need, eg. two lines per frame for CC 525,
	 *  and set reasonable parameters.
	 */
	for (i = 0; vbi_services[i].id; i++) {
		double left_margin = (vbi->scanning == 525) ? 1.0e-6 : 2.0e-6;
		int offset, samples;
		double signal;

		if (!(vbi_services[i].id & services))
			continue;

		if (vbi_services[i].scanning != vbi->scanning) {
			services &= ~vbi_services[i].id;
		}

		*max_rate = MAX(*max_rate,
			MAX(vbi_services[i].cri_rate,
			    vbi_services[i].bit_rate));

		signal = vbi_services[i].cri_bits / (double) vbi_services[i].cri_rate
			 + (vbi_services[i].frc_bits + vbi_services[i].payload)
			   / (double) vbi_services[i].bit_rate;

		offset = (vbi_services[i].offset / 1e9 - left_margin) * 27e6 + 0.5;
		samples = (signal + left_margin + 1.0e-6) * 27e6 + 0.5;

		vbi->offset = MIN(vbi->offset, offset);

		vbi->samples_per_line =
			MIN(vbi->samples_per_line + vbi->offset,
			    samples + offset) - vbi->offset;

		for (j = 0; j < 2; j++)
			if (vbi_services[i].first[j] &&
			    vbi_services[i].last[j]) {
				vbi->start[j] =
					MIN(vbi->start[j],
					    vbi_services[i].first[j]);
				vbi->count[j] =
					MAX(vbi->start[j] + vbi->count[j],
					    vbi_services[i].last[j] + 1) - vbi->start[j];
			}
	}

	if (!vbi->count[0])
		vbi->start[0] = -1;

	if (!vbi->count[1])
		vbi->start[1] = -1;

	return services;
}

void
uninit_vbi_decoder(struct vbi_decoder *vbi)
{
	if (vbi->pattern)
		free(vbi->pattern);

	vbi->services = 0;
	vbi->num_jobs = 0;

	vbi->pattern = NULL;

	memset(vbi->jobs, 0, sizeof(vbi->jobs));
}

void
reset_vbi_decoder(struct vbi_decoder *vbi)
{
	uninit_vbi_decoder(vbi);
}

void
init_vbi_decoder(struct vbi_decoder *vbi)
{
	vbi->pattern = NULL;

	uninit_vbi_decoder(vbi);
}
