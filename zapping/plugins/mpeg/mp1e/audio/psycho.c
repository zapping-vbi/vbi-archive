/*
 *  MPEG Real Time Encoder
 *  MPEG-1/2 Audio Layer II Psychoacoustic Analysis Model 2
 *
 *  Copyright (C) 1999-2000 Michael H. Schimek
 *  Copyright (C) 1996 ISO MPEG Audio Subgroup Software Simulation Group
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

/* $Id: psycho.c,v 1.1 2000-07-05 18:09:34 mschimek Exp $ */

#include <math.h>
#include "../log.h"
#include "../mmx.h"
#include "../misc.h"
#include "../profile.h"
#include "mpeg.h"

/* fft.c */

extern void fft_init(void);
extern void fft_step_1(short *, FLOAT *);
extern void fft_step_2(short *, FLOAT *);

#define psy_align(n) __attribute__ ((SECTION("PSY_TABLES") aligned (n)))

#define BLKSIZE 1024
#define HBLKSIZE 513
#define CBANDS 63

extern int mpeg_version;
extern int sampling_freq_code;
extern int sblimit;
extern int psycho_loops;

/* Buffers */

static FLOAT		h_save[2][3][BLKSIZE]		psy_align(CACHE_LINE);
static FLOAT		e_save[2][2][HBLKSIZE]		psy_align(CACHE_LINE);
static struct {
	FLOAT e, c;
}			grouped[CBANDS]			psy_align(CACHE_LINE);
static FLOAT		nb[HBLKSIZE]			psy_align(CACHE_LINE);
static FLOAT		sum_energy[SBLIMIT]		psy_align(CACHE_LINE);

static int ch;
#define e_save_new e_save_oldest
static FLOAT *e_save_old    = e_save[0][1];
static FLOAT *e_save_oldest = e_save[0][0];
static FLOAT *h_save_new    = h_save[0][2];
static FLOAT *h_save_old    = h_save[0][1];
static FLOAT *h_save_oldest = h_save[0][0];

/* Tables */

static char		partition[HBLKSIZE]		psy_align(CACHE_LINE);		// frequency line to cband mapping
static struct {
	char off, cnt;
}			s_limits[CBANDS]		psy_align(CACHE_LINE);
static FLOAT		s_packed[2048]			psy_align(CACHE_LINE);		// packed spreading function
static FLOAT		absthres[HBLKSIZE]		psy_align(CACHE_LINE);		// absolute thresholds, linear
static FLOAT		p1[CBANDS]			psy_align(CACHE_LINE);
static FLOAT		p2[CBANDS]			psy_align(CACHE_LINE);
static FLOAT		p3[CBANDS - 20]			psy_align(CACHE_LINE);
static FLOAT		p4[CBANDS - 20]			psy_align(CACHE_LINE);
static FLOAT		xnorm[CBANDS]			psy_align(CACHE_LINE);
static float		static_snr[SBLIMIT]		psy_align(CACHE_LINE);

void
create_absthres(int sampling_freq)
{
	int table, i, higher, lower = 0;

	for (table = 0; table < 6; table++)
		if (absthr[table][0].thr == sampling_freq)
			break;

	ASSERT("create threshold table for sampling rate %d Hz",
		table < 6, sampling_freq);

	printv(2, "Psychoacoustic threshold table #%d\n", table);

	for (i = 1; i <= absthr[table][0].line; i++)
		for (higher = absthr[table][i].line; lower < higher; lower++)
			absthres[lower] = pow(10.0, (absthr[table][i].thr + 41.837375) * 0.1);

	while (lower < HBLKSIZE)
		absthres[lower++] = pow(10.0, (96.0 + 41.837375) * 0.1);

	// DUMP(absthres, 0, HBLKSIZE);
}

void
psycho_init(void)
{
	static float crit_band[27] =
	{
		0, 100, 200, 300, 400, 510, 630, 770,
        	920, 1080, 1270, 1480, 1720, 2000, 2320, 2700,
        	3150, 3700, 4400, 5300, 6400, 7700, 9500, 12000,
        	15500, 25000, 30000
	};
	static float bmax[27] =
	{
		20.0, 20.0, 20.0, 20.0, 20.0, 17.0, 15.0,
    	        10.0,  7.0,  4.4,  4.5,  4.5,  4.5,  4.5,
        	 4.5,  4.5,  4.5,  4.5,  4.5,  4.5,  4.5,
        	 4.5,  4.5,  4.5,  3.5,  3.5,  3.5
	};
	double bval[CBANDS], temp[HBLKSIZE];
	double temp1, temp2, freq_mult, bval_lo;
	FLOAT *s_pp = s_packed, s[CBANDS][CBANDS];
	int numlines[CBANDS];
	int sampling_freq = sampling_freq_value[mpeg_version][sampling_freq_code];
	int i, j, k, b;

	fft_init();	
	create_absthres(sampling_freq);

	// Compute fft frequency multiplicand

	freq_mult = (double) sampling_freq / BLKSIZE;

	// Calculate fft frequency, then bark value of each line
	
	for (i = 0; i < HBLKSIZE; i++) { // 513
		temp1 = i * freq_mult;
		for (j = 1; temp1 > crit_band[j]; j++);
		temp[i] = j - 1 + (temp1 - crit_band[j - 1]) / (crit_band[j] - crit_band[j - 1]);
	}

	partition[0] = 0;
	temp2 = 1; // counter of the # of frequency lines in each partition
	bval[0] = temp[0];
	bval_lo = temp[0];

	for (i = 1; i < HBLKSIZE; i++)
	{
		if ((temp[i] - bval_lo) > 0.33) {
			partition[i] = partition[i - 1] + 1;
			bval[(int) partition[i - 1]] = bval[(int) partition[i - 1]] / temp2;
			bval[(int) partition[i]] = temp[i];
    			bval_lo = temp[i];
    			numlines[(int) partition[i - 1]] = temp2;
			temp2 = 1;
    		} else {
    			partition[i] = partition[i - 1];
	    		bval[(int) partition[i]] += temp[i];
			temp2++;
		}
	}

	numlines[(int) partition[i - 1]] = temp2;
	bval[(int) partition[i - 1]] = bval[(int) partition[i - 1]] / temp2;

	/*
	 *  Compute the spreading function, s[j][i], the value of the
	 *  spreading function, centered at band j, for band i
	 */
	for (j = 0; j < CBANDS; j++) {
		for (i = 0; i < CBANDS; i++) {
			double temp1, temp2, temp3;

			temp1 = (bval[i] - bval[j]) * 1.05;

			if (temp1 >= 0.5 && temp1 <= 2.5) {
				temp2 = temp1 - 0.5;
				temp2 = 8.0 * (temp2 * temp2 - 2.0 * temp2);
			} else
				temp2 = 0;

			temp1 += 0.474;
			temp3 = 15.811389 + 7.5 * temp1
				- 17.5 * sqrt((double)(1.0 + temp1 * temp1));

			if (temp3 <= -100.0)
				s[i][j] = 0.0;
			else
				s[i][j] = pow(10.0, (temp2 + temp3) / 10.0);
		}
	}

	for (b = 0; b < CBANDS; b++)
	{
		double NMT, TMN, minval, bc, rnorm;

		// Noise Masking Tone value (in dB) for all partitions
		NMT = 5.5;

		// Tone Masking Noise value (in dB) for this partition
		TMN = MAX(15.5 + bval[b], 24.5);

		// SNR lower limit
		minval = bmax[(int)(bval[b] + 0.5)];

		bc = (TMN - NMT) * 0.1;

		if (b < 20) {
			p1[b] = pow(10.0, -(minval - NMT) / (TMN - NMT)) / 2.0;
			p2[b] = pow(MIN(0.5, p1[b]) * 2.0, bc);
		} else {
			p4[b - 20] = bc;
			p3[b - 20] = pow(10.0, (minval - NMT) / bc);
			p2[b] = pow(p3[b - 20] * 2.0, bc);
			p1[b] = pow(0.05 * 2.0, bc);
		}

		// Calculate normalization factor for the net spreading function

		for (i = 0, rnorm = 0.0; i < CBANDS; i++)
			rnorm += s[b][i];

		xnorm[b] = pow(10.0, -NMT * 0.1) / (rnorm * numlines[b]);

		// Pack spreading function

	        for (k = 0; s[b][k] == 0.0; k++);
			s_limits[b].off = k;
	        for (j = 0; s[b][k + j] != 0.0 && (k + j) < CBANDS; j++)
		        *s_pp++ = s[b][k + j];
	        s_limits[b].cnt = j;
	}

	for (i = 0; i < 3; i++)
		for (j = 0; j < HBLKSIZE; j++) {
			h_save[0][i][j] = 1.0; // force oldest, old phi = 0.0
			e_save[0][i & 1][j] = 1.0; // should be 0.0, see below
		}

	for (j = 0; j < SBLIMIT; j++)
		static_snr[j] = pow(1.005, 1024 + j * j - 64 * j);

	// DUMP(static_snr, 0, SBLIMIT);
}

/*
 *  This code is equivalent to the psychoacoustic model 2 in the
 *  MPEG-2 Audio Simulation Software Distribution 10, just
 *  a bit :) optimized for speed.
 *
 *  Doesn't return SNR in dB but linear values (pow(10.0, SNR * 0.1)),
 *  the mnr_incr calculation and bit allocation has been modified accordingly.
 */

void
psycho(short *buffer, float *snr, int step)
{
	int i, j;

	if (!psycho_loops) {
		memcpy(snr, static_snr, sizeof(static_snr));
		return;
	}

	for (i = 0; i < psycho_loops; i++)
	{
		/*
		 *  Filterbank 3 * 12 * 32 samples: 0..1151 (look ahead 480 samples)
		 *  Pass #1: 0..1023
		 *  Pass #2: 576..1599 (sic)
	         */

		// DUMP(buffer, 0, 16);

		pr_start(30, "FFT & Hann window");

		if (step == 1)
			fft_step_1(buffer, h_save_new);
		else
			fft_step_2(buffer, h_save_new);

		pr_end(30);


		pr_start(31, "Psy/1");

		/*
		 *  Calculate the grouped, energy-weighted unpredictability measure,
		 *  grouped[].c, and the grouped energy, grouped[].e
		 *
		 *  XXX optimize here
                 */

		memset(grouped, 0, sizeof(grouped));

		{
			double energy, e_sqrt, r_prime, temp3;
			double r0 = h_save_new[0];
		        double r2 = h_save_oldest[0];

			sum_energy[0] = grouped[0].e = energy = r0 * r0;

			r_prime = 2.0 * e_save_old[0] - e_save_oldest[0];
			e_save_new[0] = e_sqrt = sqrt(energy);

			if ((temp3 = e_sqrt + fabs(r_prime)) != 0.0) {
				if (r2 < 0.0)
					r_prime = -r_prime;	
				grouped[0].c = energy * fabs(r0 - r_prime) / temp3;
			}
		}

		for (j = 1; j < HBLKSIZE - 1; j++)
		{
	    		int pt = partition[j];
			int j4 = j >> 4;
			double energy, e_sqrt, r_prime, temp3;
			double i0 = h_save_new[BLKSIZE - j];
			double i1 = h_save_old[BLKSIZE - j];
			double i2 = h_save_oldest[BLKSIZE - j];
			double r0 = h_save_new[j];
			double r1 = h_save_old[j];
			double r2 = h_save_oldest[j];

			energy = r0 * r0 + i0 * i0;
			grouped[pt].e += energy;
			if (j & 15)
				sum_energy[j4] += energy;
			else {
				sum_energy[j4 - 1] += energy;
				sum_energy[j4] = energy;
			}

			r_prime = 2.0 * e_save_old[j] - e_save_oldest[j];
			e_sqrt = sqrt(energy);

			temp3 = e_sqrt + fabs(r_prime);

			if (temp3 == 0.0)
				; // grouped[pt].c += energy * 0.0;
			else {
				double c1, s1, c2, s2, ll;
/*
				double phi = atan2(i0, r0);
				double phi_prime = 2.0 * atan2(i1, r1) - atan2(i2, r2);

				c1 = e_sqrt * cos(phi) - r_prime * cos(phi_prime);
				s1 = e_sqrt * sin(phi) - r_prime * sin(phi_prime);

				grouped[pt].c += energy * sqrt(c1 * c1 + s1 * s1) / temp3;
*/
				c2 = r1 * r1;
				s2 = i1 * i1;

				c1 = r1 * i1 * 2.0;
				s1 = c2 - s2;
				ll = c2 + s2;

				c2 = s1 * r2 + c1 * i2;
				s2 = c1 * r2 - s1 * i2;

				r_prime /= ll * e_save_oldest[j];
				/*
				 *  This should be r_prime /= ll * sqrt(r2 * r2 + i2 * i2), initializing e_save with all
				 *  zeroes. By initializing to all ones we can omit one sqrt(). The +1 error in temp3
				 *  (frame #1 only) is negligible.
				 */

				c1 = r0 - r_prime * c2;
				s1 = i0 - r_prime * s2;

				grouped[pt].c += energy * sqrt(c1 * c1 + s1 * s1) / temp3;
			}

			e_save_new[j] = e_sqrt;
		}
#if 0
		for (j = 206; j < HBLKSIZE - 1; j++)
		{
	    		int pt = partition[j], j4 = j >> 4;
			double i0 = h_save_new[BLKSIZE - j];
			double r0 = h_save_new[j];

			grouped[pt].e += energy = r0 * r0 + i0 * i0;
			grouped[pt].c += energy * 0.3;
			if (j & 15)
				sum_energy[j4] += energy;
			else {
				sum_energy[j4 - 1] += energy;
				sum_energy[j4] = energy;
			}
		}
#endif
		{
			double energy, e_sqrt, r_prime, temp3;
		        double r0 = h_save_new[HBLKSIZE - 1];
			double r2 = h_save_oldest[HBLKSIZE - 1];

			grouped[CBANDS - 1].e += energy = r0 * r0;
			sum_energy[SBLIMIT - 1] += energy;

			r_prime = 2.0 * e_save_old[HBLKSIZE - 1] - e_save_oldest[HBLKSIZE - 1];
			e_save_new[HBLKSIZE - 1] = e_sqrt = sqrt(energy);

			if ((temp3 = e_sqrt + fabs(r_prime)) != 0.0) {
				if (r2 < 0.0)
					r_prime = -r_prime;
				grouped[CBANDS - 1].c += energy * fabs(r0 - r_prime) / temp3;
			}
		}

		// DUMP(grouped_e, 0, CBANDS);    
		// DUMP(grouped_c, 0, CBANDS);

		pr_end(31);
		pr_start(32, "Psy/2");

		{
			FLOAT *s_pp = s_packed;

			for (j = 0; j < 20; j++)
			{
				double ecb = 0.0, cb = 0.0;
				int k, cnt = s_limits[j].cnt, off = s_limits[j].off;

				/*
				 *  Convolve the grouped energy-weighted unpredictability measure
				 *  and the grouped energy with the spreading function, s[j][k]
				 */
				for (k = 0; k < cnt; k++) {
				    	double ss = *s_pp++;
					ecb += ss * grouped[off + k].e;
    				    	cb += ss * grouped[off + k].c;
				}

				if (ecb == 0.0)
					nb[j] = 0.0;
				else {
					cb /= ecb;

					/*
					 *  Calculate the required SNR for each of the
					 *  frequency partitions
					 */
					ecb *= xnorm[j];

			    		if (cb < 0.05)
						nb[j] = ecb * 0.0125892541179416766333743; // pow(0.05 * 2.0, 1.9);
					else if (cb > 0.5 || cb > p1[j])
						nb[j] = ecb * p2[j];
					else
						nb[j] = ecb * pow(cb * 2.0, 1.9);
				}
			}

			for (; j < CBANDS; j++) // 63
			{
				double ecb = 0.0, cb = 0.0;
				int k, cnt = s_limits[j].cnt, off = s_limits[j].off;

				for (k = 0; k < cnt; k++) {
					double ss = *s_pp++;
					ecb += ss * grouped[off + k].e;
        				cb += ss * grouped[off + k].c;
				}

				if (ecb == 0.0)
					nb[j] = 0.0;
				else {
					cb /= ecb;
					ecb *= xnorm[j];

					if (cb < 0.05)
						nb[j] = ecb * p1[j];
					else if (cb > 0.5)
						nb[j] = ecb; // not exactly, but the error is negligible
					else if (cb > p3[j - 20])
						nb[j] = ecb * pow(cb * 2.0, p4[j - 20]);
					else
						nb[j] = ecb * p2[j];
				}
			}

			// DUMP(nb, 0, CBANDS);
		}

		// DUMP(sum_energy, 0, SBLIMIT);

		pr_end(32);
		pr_start(33, "Psy/3");

		/*
		 *  Calculate the permissible noise energy level in each of the frequency
		 *  partitions and translate the 512 threshold values to the 32 filter
		 *  bands of the coder
		 */

		if (i == 0) {
			for (j = 0; j < 13; j++)
			{
				double minthres = 60802371420160.0;
				int k;

				for (k = 0; k < 17; k++) {
					double fthr = MAX(absthres[j * 16 + k], nb[(int) partition[j * 16 + k]]);

					if (minthres > fthr)
						minthres = fthr;
				}

				snr[j] = sum_energy[j] / (minthres * 17.0);
			}

			for (j = 13; j < sblimit; j ++)
			{
    				double minthres = 0.0;
				int k;

				for (k = 0; k < 17; k++)
					minthres += MAX(absthres[j * 16 + k], nb[(int) partition[j * 16 + k]]);

				snr[j] = sum_energy[j] / minthres;
			}
		} else {
			for (j = 0; j < 13; j++)
			{
				double t, minthres = 60802371420160.0;
				int k;

				for (k = 0; k < 17; k++) {
					double fthr = MAX(absthres[j * 16 + k], nb[(int) partition[j * 16 + k]]);

					if (minthres > fthr)
						minthres = fthr;
				}

				t = sum_energy[j] / (minthres * 17.0);
				if (t > snr[j]) snr[j] = t;
			}

			for (j = 13; j < sblimit; j++)
			{
    				double t, minthres = 0.0;
				int k;

				for (k = 0; k < 17; k++)
					minthres += MAX(absthres[j * 16 + k], nb[(int) partition[j * 16 + k]]);

				t = sum_energy[j] / minthres;
				if (t > snr[j]) snr[j] = t;
			}
		}

		// DUMP(snr, 0, SBLIMIT);

		pr_end(33);

		{
			FLOAT *t;

			t = e_save_oldest;
			e_save_oldest = e_save_old;
			e_save_old = t;

			t = h_save_oldest;
			h_save_oldest = h_save_old;
			h_save_old = h_save_new;
			h_save_new = t;
		}

		buffer += 576;
	}

	if (step == 2) {
		if (ch) {
			h_save_oldest -= sizeof(h_save[0]) / sizeof(FLOAT);
			h_save_old    -= sizeof(h_save[0]) / sizeof(FLOAT);
			h_save_new    -= sizeof(h_save[0]) / sizeof(FLOAT);
			e_save_oldest -= sizeof(e_save[0]) / sizeof(FLOAT);
			e_save_old    -= sizeof(e_save[0]) / sizeof(FLOAT);
		} else {
			h_save_oldest += sizeof(h_save[0]) / sizeof(FLOAT);
			h_save_old    += sizeof(h_save[0]) / sizeof(FLOAT);
			h_save_new    += sizeof(h_save[0]) / sizeof(FLOAT);
			e_save_oldest += sizeof(e_save[0]) / sizeof(FLOAT);
			e_save_old    += sizeof(e_save[0]) / sizeof(FLOAT);
		}
		ch ^= 1;
	}
}
