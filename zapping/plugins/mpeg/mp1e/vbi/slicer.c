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

#include "../common/math.h"

/* $Id: slicer.c,v 1.1 2000-12-06 23:43:31 mschimek Exp $ */

#include "vbi.h"

#define OVERSAMPLING 2		// 1, 2, 4, 8

void
init_bit_slicer(struct bit_slicer *d,
	int raw_bytes, int sampling_rate, int cri_rate, int bit_rate,
	unsigned int cri_frc, int cri_bits, int frc_bits, int payload, int modulation)
{
	d->cri_mask		= (1 << cri_bits) - 1;
	d->cri		 	= (cri_frc >> frc_bits) & d->cri_mask;
	d->cri_bytes		= raw_bytes
		- ((long long) sampling_rate * (8 * payload + frc_bits)) / bit_rate;
	d->cri_rate		= cri_rate;
	d->oversampling_rate	= sampling_rate * OVERSAMPLING;
	d->thresh		= 105 << 9;
	d->frc			= cri_frc & ((1 << frc_bits) - 1);
	d->frc_bits		= frc_bits;
	d->step			= sampling_rate * 256.0 / bit_rate;
	d->payload		= payload;
	d->lsb_endian		= TRUE;

	switch (modulation) {
	case MOD_NRZ_MSB_ENDIAN:
		d->lsb_endian = FALSE;
	case MOD_NRZ_LSB_ENDIAN:
		d->phase_shift = sampling_rate * 256.0 / cri_rate * .5
			         + sampling_rate * 256.0 / bit_rate * .5 + 128;
		break;

	case MOD_BIPHASE_MSB_ENDIAN:
		d->lsb_endian = FALSE;
	case MOD_BIPHASE_LSB_ENDIAN:
		d->phase_shift = sampling_rate * 256.0 / cri_rate * .5
			         + sampling_rate * 256.0 / bit_rate * .25 + 128;
		break;
	}
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
							c = c * 2 + (raw[i >> 8] >= tr);
    							i += d->step;
						}

						if (c ^= d->frc)
							return FALSE;

						if (d->lsb_endian) {
							for (j = d->payload; j > 0; j--) {
								for (k = 0; k < 8; k++) {
						    			c >>= 1;
									c += (raw[i >> 8] >= tr) << 7;
			    						i += d->step;
								}

								*buf++ = c;
					    		}
						} else {
							for (j = d->payload; j > 0; j--) {
								for (k = 0; k < 8; k++) {
									c = c * 2 + (raw[i >> 8] >= tr);
			    						i += d->step;
								}

								*buf++ = c;
					    		}
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
