/*
 *  V4L/V4L2 VBI Decoder  DRAFT
 *
 *  gcc -O2 -othis this.c mp1e/vbi/tables.c -L/usr/X11R6/lib -lm -lX11
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

/* $Id: vbi_decoder.c,v 1.7 2000-12-01 13:12:11 mschimek Exp $ */

/*
    TODO:
    - test streaming
    - write close functions
    - write T thread & multi consumer fifo
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <assert.h>
#include <sys/time.h>		// timeval
#include <sys/types.h>		// fd_set
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <asm/types.h>		// v4l2
#include <linux/videodev.h>



#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

typedef char bool;
enum { FALSE, TRUE };

typedef struct {
	double			time;
	unsigned char *		data;
	long			used;
	unsigned char *		allocated;
	long			size;
} buffer;

typedef struct _fifo {
	buffer *		(* wait_full)(struct _fifo *);
	void			(* send_empty)(struct _fifo *, buffer *);
	buffer *		(* wait_empty)(struct _fifo *);
	void			(* send_full)(struct _fifo *, buffer *);
	bool			(* start)(struct _fifo *);

	buffer *		buffers;
	int			num_buffers;

	void *			user_data;
} fifo;

static inline unsigned int
nbabs(register int n)
{
	register int t = n;

        t >>= 31;
	n ^= t;
	n -= t;

	return n;
}

typedef long long int tsc_t;

#if #cpu (i386)

tsc_t rdtsc(void)
{
	tsc_t tsc;

	asm ("\trdtsc\n" : "=A" (tsc));
	return tsc;
}

#endif

char err_str[1024];

#define IODIAG(templ, args...)				\
do {							\
	snprintf(err_str + strlen(err_str),		\
		sizeof(err_str) - strlen(err_str) - 1,	\
		templ ":\n %d, %s"			\
		,##args, errno, strerror(errno));	\
} while (0)

#define DIAG(templ, args...)				\
do {							\
	snprintf(err_str + strlen(err_str),		\
		sizeof(err_str) - strlen(err_str) - 1,	\
		templ,##args);				\
} while (0)


#define V4L2_LINE -1 // API rev. Nov 2000 (-1 -> 0)



/* Options */

int			opt_teletext;
int			opt_cni;
int			opt_vps;
int			opt_caption;
int			opt_xds;
int			opt_sliced;
int			opt_profile;
int			opt_ccsim;
int			opt_pattern;
int			opt_all;
int			opt_raw;
int			opt_graph;
int			opt_strict = 0;
int			opt_surrender = 1;



/*
 *  Bit Slicer
 */

#define MOD_NRZ_LSB_ENDIAN	0
#define MOD_NRZ_MSB_ENDIAN	1
#define MOD_BIPHASE_LSB_ENDIAN	2
#define MOD_BIPHASE_MSB_ENDIAN	3

struct bit_slicer {
	unsigned int	cri;
	unsigned int	cri_mask;
	int		thresh;
	int		cri_bytes;
	int		cri_rate;
	int		oversampling_rate;
	int		phase_shift;
	int		step;
	unsigned int	frc;
	int		frc_bits;
	int		payload;
	int		lsb_endian;
};

#define OVERSAMPLING 2		// 1, 2, 4, 8

static inline void
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

static bool
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



#define MAX_RAW_BUFFERS 	5
#define MAX_JOBS		8
#define MAX_WAYS		4

struct job {
	unsigned int		id;
	int			offset;
	struct bit_slicer 	slicer;
};

struct vbi_capture {
	fifo			fifo;

	int			fd;
	int			num_raw_buffers;
	struct {
		unsigned char *		data;
		int			size;
	}			raw_buffer[MAX_RAW_BUFFERS];

	/* test */
	bool			stream;
	buffer 			buf;

	int			scanning;
	int			sampling_rate;
	int			samples_per_line;
	int			offset;
	int			start[2];
	int			count[2];
	bool			interlaced;
	bool			synchronous;

	unsigned int		services;
	int			num_jobs;

	char *			pattern;
	struct job		jobs[MAX_JOBS];
};

/*
 *  Data Service Decoder
 */

#define SLICED_TELETEXT_B_L10_625	(1UL << 0)
#define SLICED_TELETEXT_B_L25_625	(1UL << 1)
#define SLICED_VPS			(1UL << 2)
#define SLICED_CAPTION_625_F1		(1UL << 3)
#define SLICED_CAPTION_625		(1UL << 4)
#define SLICED_CAPTION_525_F1		(1UL << 5)
#define SLICED_CAPTION_525		(1UL << 6)
#define SLICED_2xCAPTION_525		(1UL << 7)
#define SLICED_NABTS			(1UL << 8)
#define SLICED_TELETEXT_BD_525		(1UL << 9)
#define SLICED_VBI_625			(1UL << 30)
#define SLICED_VBI_525			(1UL << 31)

struct vbi_service_par {
	unsigned int	id;
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
	char		cri_bits;	/* cri_frc bits significant for identification, advice: ignore */
	char		frc_bits;	/*  leading CRI bits; frc_bits at bit_rate */
	char		payload;	/* in bytes */
	char		modulation;	/* payload modulation */
};

const struct vbi_service_par
vbi_services[] = {
	{
		SLICED_TELETEXT_B_L10_625, "Teletext System B Level 1.5, 625",
		{ 7, 320 },
		{ 22, 335 },
		/*
		    "F.4 Allocation of Teletext packets to VBI lines:
		     Some existing Level 1 and 1.5 decoders may not decode Teletext
		     signals on lines 6, 318 and 319. Thus these lines should be used
		     for Level 2.5 or 3.5 enhancement data only, or non-Teletext signals
		     (see annex P). Further information can be found in TR 101 233 [7]."
		 */
		10300, 6937500, 6937500, /* 444 x FH */
		625, 0x00AAAAE4, 10, 6, 42, MOD_NRZ_LSB_ENDIAN
	}, {
		SLICED_TELETEXT_B_L25_625, "Teletext System B, 625",
		{ 6, 318 },
		{ 22, 335 },
		10300, 6937500, 6937500, /* 444 x FH */
		625, 0x00AAAAE4, 10, 6, 42, MOD_NRZ_LSB_ENDIAN
	}, {
		SLICED_VPS, "Video Programming System",
		{ 16, 0 },
		{ 16, 0 },
		12500, 5000000, 2500000, /* 160 x FH */
		625, 0xAAAA8A99, 24, 0, 13, MOD_BIPHASE_MSB_ENDIAN
	}, {
		SLICED_CAPTION_625_F1, "Closed Caption 625, single field",
		{ 22, 0 },
		{ 22, 0 },
		10500, 1000000, 500000, /* 32 x FH */
		625, 0x00005551, 9, 2, 2, MOD_NRZ_LSB_ENDIAN
	}, {
		SLICED_CAPTION_625, "Closed Caption 625", /* Videotapes, LD */
		{ 22, 335 },
		{ 22, 335 },
		10500, 1000000, 500000, /* 32 x FH */
		625, 0x00005551, 9, 2, 2, MOD_NRZ_LSB_ENDIAN
	}, {
		SLICED_VBI_625, "VBI 625", /* Blank VBI */
		{ 6, 318 },
		{ 22, 335 },
		10000, 1510000, 1510000,
		625, 0, 0, 0, 10, 0 /* 10.0-2 ... 62.9+1 us */
	}, {
		SLICED_NABTS, "Teletext System C, 525", /* NOT CONFIRMED */
		{ 10, 0 },
		{ 21, 0 },
		10500, 5727272, 5727272,
		525, 0x00AAAAE7, 10, 6, 33, MOD_NRZ_LSB_ENDIAN /* Tb. */
	}, {
		SLICED_CAPTION_525_F1, "Closed Caption 525, single field",
		{ 21, 0 },
		{ 21, 0 },
		10500, 1006976, 503488, /* 32 x FH */
		525, 0x00005551, 9, 2, 2, MOD_NRZ_LSB_ENDIAN
	}, {
		SLICED_CAPTION_525, "Closed Caption 525",
		{ 21, 284 },
		{ 21, 284 },
		10500, 1006976, 503488, /* 32 x FH */
		525, 0x00005551, 9, 2, 2, MOD_NRZ_LSB_ENDIAN
	}, {
		SLICED_2xCAPTION_525, "2xCaption 525", /* NOT CONFIRMED */
		{ 10, 0 },
		{ 21, 0 },
		10500, 1006976, 1006976, /* 64 x FH */
		525, 0x000554ED, 8, 8, 4, MOD_NRZ_LSB_ENDIAN /* Tb. */
	}, {
		SLICED_TELETEXT_BD_525, "Teletext System B / D (Japan), 525", /* NOT CONFIRMED */
		{ 10, 0 },
		{ 21, 0 },
		9600, 5727272, 5727272,
		525, 0x00AAAAE4, 10, 6, 34, MOD_NRZ_LSB_ENDIAN /* Tb. */
	}, {
		SLICED_VBI_525, "VBI 525", /* Blank VBI */
		{ 10, 272 },
		{ 21, 284 },
		9500, 1510000, 1510000,
		525, 0, 0, 0, 10, 0 /* 9.5-1 ... 62.4+1 us */
	},
	{ 0 }
};

typedef struct {
	unsigned int		id;		/* set of SLICED_.. */
	int			line;		/* ITU-R line number 1..n, 0: unknown */
	unsigned char		data[48];
} vbi_sliced;

/* forward */ static void
draw(struct vbi_capture *vbi, unsigned char *data);

static int
decode(struct vbi_capture *vbi, unsigned char *raw1, vbi_sliced *out1)
{
	static int readj = 1;
	int pitch = vbi->samples_per_line << vbi->interlaced;
	char *pat, *pattern = vbi->pattern;
	unsigned char *raw = raw1;
	vbi_sliced *out = out1;
	struct job *job;
	int i, j;
#if #cpu (i386)
	tsc_t sum = 0LL, begin;
	int repeats = 0;
#endif

	if (opt_graph)
		draw(vbi, raw1);

	if (opt_pattern && readj == 0) {
		for (i = 0; i < (vbi->count[0] + vbi->count[1]) * MAX_WAYS; i++) {
			if (vbi->interlaced && i == vbi->count[0])
				raw = raw1 + vbi->samples_per_line;

			if (i % MAX_WAYS == 0) {
				if ((j = i / MAX_WAYS) < vbi->count[0])
					printf("%3d ", (vbi->start[0] >= 0) ? vbi->start[0] + j : 0);
				else
					printf("%3d ", (vbi->start[1] >= 0) ? vbi->start[1] - vbi->count[0] + j : 0);
			}

			printf("%04x ", (vbi->pattern[i] > 0) ? vbi->jobs[vbi->pattern[i] - 1].id : 0);

			if (i % MAX_WAYS == MAX_WAYS - 1) {
				if (vbi->pattern[i - 3] > 0) {
					int jj = 0;

					job = vbi->jobs + (vbi->pattern[i - 3] - 1);

					for (j = 0; vbi_services[j].id; j++)
						if (vbi_services[j].id & job->id)
							jj = j; // broadest last

					printf("%s\n", vbi_services[jj].label);
				} else {
					int s = 0, sd = 0;

					for (j = 0; j < vbi->samples_per_line; j++)
						s += raw[j];

					s /= vbi->samples_per_line;

					for (j = 0; j < vbi->samples_per_line; j++)
						sd += nbabs(raw[j] - s);

					sd /= vbi->samples_per_line;

					if (sd < 5)
						puts("Blank");
					else
						puts("Unknown signal");
				}

				raw += pitch;
			}
		}

		putchar('\n');

		raw = raw1;
	}

#if #cpu (i386)
	if (opt_profile)
		begin = rdtsc();
#endif

	for (i = 0; i < vbi->count[0] + vbi->count[1]; i++) {
		if (vbi->interlaced && i == vbi->count[0])
			raw = raw1 + vbi->samples_per_line;

		/*
		 *  The pattern magic serves these purposes:
		 *
		 *  * Probe only lines which are recommended for
		 *    a particular service, e.g. Caption only line 21.
		 *    add_service() may decide to probe more lines
		 *    if the vbi device line numbering is unreliable.
		 *  * If several services are expected, probe in turn.
		 *  * Learn which service is carried on line and probe
		 *    that first to speed things up.
		 *  * Learn which lines carry no signal and skip them
		 *    to speed things up.
		 *  * Audit the pattern of blank lines at readj
		 *    intervals, in case we were wrong or it changed
		 *    e.g. due to channel switch.
		 *  * vbi_sliced data will be sorted by line and
		 *    field number (used to be id).
		 */

		for (pat = pattern;; pat++) {
			if ((j = *pat) > 0) {
				job = vbi->jobs + (j - 1);
#if #cpu (i386)
				if (opt_profile) {
					tsc_t begin = rdtsc();
					bool r = bit_slicer(&job->slicer, raw + job->offset, out->data);

					sum += rdtsc() - begin;
					repeats++;
					
					if (!r)
						continue;
				} else
#endif
				if (!bit_slicer(&job->slicer, raw + job->offset, out->data))
					continue;

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

#if #cpu (i386)
	if (opt_profile) {
		begin = rdtsc() - begin;

		printf("decode() %lld cycles, %d jobs, %d raw lines, %d active; "
		       "bit_slicer() %lld cycles, %d/frame\n",
			begin, vbi->num_jobs, vbi->count[0] + vbi->count[1], out - out1,
			sum / repeats, repeats);
	}
#endif

	readj = (readj + 1) & 15;

	return out - out1;
}

static unsigned int
remove_services(struct vbi_capture *vbi, unsigned int services)
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

			memmove(vbi->jobs + i, vbi->jobs + (i + 1), (MAX_JOBS - i - 1) * sizeof(vbi->jobs[0]));
			vbi->jobs[--vbi->num_jobs].id = 0;
		} else
			i++;
	}

	return vbi->services &= ~services;
}

static unsigned int
add_services(struct vbi_capture *vbi, unsigned int services, int strict)
{
	double off_min = (vbi->scanning == 525) ? 7.9e-6 : 8.0e-6;
	int row[2], count[2], way;
	struct job *job;
	char *pattern;
	int i, j, k;

	services &= ~(SLICED_VBI_525 | SLICED_VBI_625);
	/*
	 *  These only exist to program the vbi device
	 *  to capture all vbi lines.
	 */

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
			 + (vbi_services[i].frc_bits + vbi_services[i].payload * 8)
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

static void
reset_decoder(struct vbi_capture *vbi)
{
	if (vbi->pattern)
		free(vbi->pattern);

	vbi->services = 0;
	vbi->num_jobs = 0;

	vbi->pattern = NULL;

	memset(vbi->jobs, 0, sizeof(vbi->jobs));
}

/*
 *  Closed Caption Signal Simulator
 *  (not approved by the FCC :-)
 */

static inline double
cc_sim(double t, double F, unsigned char b1, unsigned char b2)
{
	int bits = (b2 << 10) + (b1 << 2) + 2; // start bit 0 -> 1
	double t1 = 10.5e-6 - .25 / F;
	double t2 = t1 + 7 / F;		// CRI 7 cycles
	double t3 = t2 + 1.5 / F;
	double t4 = t3 + 18 / F;	// 17 bits + raise and fall time
	double ph;

	if (t < t1)
		return 0.0;
	else if (t < t2) {
		t -= t2;
		ph = M_PI * 2 * t * F - (M_PI * .5);
		return sin(ph) / 2 + .5;
	} else if (t < t3)
		return 0.0;
	else if (t < t4) {
		int i, n;

		t -= t3;
		i = (t * F - .5);
		n = (bits >> i) & 3; // low = 0, up, down, high
		if (n == 0)
			return 0.0;
		else if (n == 3)
			return 1.0;

		if ((n ^ i) & 1) // down
			ph = M_PI * 2 * (t - 1 / F) * F / 4;
		else // up
			ph = M_PI * 2 * (t - 0 / F) * F / 4;
		return sin(ph) * sin(ph);
	} else
		return 0.0;
}

static void
cc_gen(struct vbi_capture *vbi, unsigned char *buf)
{
	static unsigned char c;
	double start, inc = 1 / (double) vbi->sampling_rate;
	int i, row;

	if (vbi->start[0] >= 0)
		row = MAX(MIN(0, 22 + (vbi->scanning == 625) - vbi->start[0]), vbi->count[0] - 1);
	else
		row = vbi->count[0] - 1;

	buf += row * vbi->samples_per_line;

	if (vbi->offset)
		start = vbi->offset / (double) vbi->sampling_rate;
	else
		start = 10e-6;

	for (i = 0; i < vbi->samples_per_line; i++) {
		if (vbi->scanning == 525)
			buf[i] = cc_sim(start + i * inc, 15734 * 32, c & 0x7F, (c + 1) & 0x7F) * 110 + 10;
		else
			buf[i] = cc_sim(start + i * inc, 15625 * 32, c & 0x7F, (c + 1) & 0x7F) * 110 + 10;
	}

	c += 2;
}

/*
 *  Device Specific Code / Callback Interface
 */

#define BTTV_VBISIZE		_IOR('v' , BASE_VIDIOCPRIVATE+8, int)
#define BTTV_VERSION		_IOR('v' , BASE_VIDIOCPRIVATE+6, int)
#define HAVE_V4L_VBI_FORMAT	0 // Linux 2.4

static buffer *
wait_full_read(fifo *f)
{
	struct vbi_capture *vbi = f->user_data;
	struct timeval tv;
	buffer *b;
	size_t r;

//	b = (buffer *) rem_head(&f->empty);
        b = &vbi->buf;

	r = read(vbi->fd, vbi->raw_buffer[0].data,
		 vbi->raw_buffer[0].size);

	if (r != vbi->raw_buffer[0].size) {
		IODIAG("VBI Read error");
		return NULL;
	}

	gettimeofday(&tv, NULL);

	b->data = b->allocated;
	b->time = tv.tv_sec + tv.tv_usec / 1e6;

	if (opt_ccsim)
		cc_gen(vbi, vbi->raw_buffer[0].data);

	b->used = sizeof(vbi_sliced) *
		decode(vbi, vbi->raw_buffer[0].data,
		       (vbi_sliced *) b->data);

    	return b;
}

static void
send_empty_read(fifo *f, buffer *b)
{
//	add_head(&f->empty, &b->node);
}

static bool
capture_on_read(fifo *f)
{
	return TRUE;
/*
	buffer *b;

	if ((b = wait_full_read(f))) {
		unget_full_buffer(f, b);
		return TRUE; // access should be finally granted
	}

	return FALSE; // EBUSY et al
*/
}

#include <dirent.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

static bool
guess_bttv_v4l(struct vbi_capture *vbi)
{
	struct video_tuner vtuner;
	struct video_channel vchan;
	struct video_unit vunit;
	int video_fd = -1;
	int mode = -1;

	memset(&vtuner, 0, sizeof(struct video_tuner));
	memset(&vchan, 0, sizeof(struct video_channel));
	memset(&vunit, 0, sizeof(struct video_unit));

	if (ioctl(vbi->fd, VIDIOCGTUNER, &vtuner) != -1)
		mode = vtuner.mode;
	else if (ioctl(vbi->fd, VIDIOCGCHAN, &vchan) != -1)
		mode = vchan.norm;
	else do {
		struct dirent, *pdirent = &dirent;
		struct stat vbi_stat;
		DIR *dir;

		/*
		 *  Bttv vbi has no VIDIOCGUNIT pointing back to
		 *  the associated video device, now it's getting
		 *  dirty. We're dumb enough to walk only /dev,
		 *  first level of, and assume v4l major is still 81.
		 *  Not tested with devfs.
		 */

		if (fstat(vbi->fd, &vbi_stat) == -1)
			break;

		if (!S_ISCHR(vbi_stat.st_mode))
			return FALSE;

		printf("VBI is a character device %d,%d\n",
			major(vbi_stat.st_dev), minor(vbi_stat.st_dev));

		if (major(vbi_stat.st_dev) != 81)
			return FALSE; /* break? */

		if (!(dir = opendir("/dev")))
			break;

		while (readdir_r(dir, &dirent, &pdirent) == 0 && pdirent) {
			struct stat stat;
			unsigned char *s;

			if (!asprintf(&s, "/dev/%s", dirent.d_name))
				continue;
			/*
			 *  V4l2 O_NOIO == O_TRUNC,
			 *  shouldn't affect v4l devices.
			 */
			if (stat(s, &stat) == -1
			    || !S_ISCHR(stat.st_mode)
			    || major(stat.st_dev) != 81
			    || (video_fd = open(s, O_RDONLY | O_TRUNC)) == -1) {
				free(s);
				continue;
			}

			printf("Trying %s, a character device %d,%d\n",
				s, major(stat.st_dev), minor(stat.st_dev));

			if (ioctl(video_fd, VIDIOCGUNIT, &vunit) == -1
			    || vunit.vbi != minor(vbi_stat.st_dev)) {
				close(video_fd);
				video_fd = -1;
				free(s);
				continue;
			}

			printf("And the winner is: %s\nThank you, thank you.", s);

			free(s);
			break;
		}

		closedir(dir);

		if (video_fd == -1)
			break; /* not found in /dev */

		if (ioctl(video_fd, VIDIOCGTUNER, &vtuner) != -1)
			mode = vtuner.mode;
		else if (ioctl(video_fd, VIDIOCGCHAN, &vchan) != -1)
			mode = vchan.norm;

		close(video_fd);
	} while (0);

	switch (norm) {
	case VIDEO_MODE_NTSC:
		vbi->scanning = 525;
		break;

	case VIDEO_MODE_PAL:
	case VIDEO_MODE_SECAM:
		vbi->scanning = 625;
		break;

	default:
		/*
		 *  You get one last chance, we'll try
		 *  to guess the scanning if GVBIFMT is
		 *  available.
		 */
		vbi->scanning = 0;
		opt_surrender = TRUE;
		break;
	}

	return TRUE;
}

static int
open_v4l(struct vbi_capture **pvbi, char *dev_name,
	 int fifo_depth, unsigned int services)
{
#if HAVE_V4L_VBI_FORMAT
	struct vbi_format vfmt;
#endif
	struct video_capability vcap;
	struct vbi_capture *vbi;
	int max_rate, buffer_size;

	if (!(vbi = calloc(1, sizeof(struct vbi_capture)))) {
		DIAG("Virtual memory exhausted");
		return 0;
	}

	if ((vbi->fd = open(dev_name, O_RDONLY)) == -1) {
		free(vbi);
		IODIAG("Cannot open %s", dev_name);
		return 0;
	}

	if (ioctl(vbi->fd, VIDIOCGCAP, &vcap) == -1) {
		/*
		 *  Older bttv drivers don't support any
		 *  vbi ioctls, let's see if we can guess the beast.
		 */
		if (!guess_bttv_v4l(vbi)) {
			close(vbi->fd);
			free(vbi);
			return -1; /* Definately not V4L */
		}

		DIAG("Opened %s, ", dev_name);
	} else {
		DIAG("Opened %s, ", dev_name);

		if (!(vcap.type & VID_TYPE_TELETEXT)) {
			DIAG("not a raw VBI device");
			goto failure;
		}

		guess_bttv_v4l(vbi);
	}

	max_rate = 0;

#if HAVE_V4L_VBI_FORMAT

	if (ioctl(vbi->fd, VIDIOCGVBIFMT, &vfmt) != -1) {
		if (!vbi->scanning
		    && vbi->start[1] > 0
		    && vbi->count[1])
			if (vbi->start[1] >= 286)
				vbi->scanning = 625;
			else
				vbi->scanning = 525;
		/*
		 *  Speculative, vbi_format is not documented.
		 */
		if (!opt_surrender && vbi->scanning) {
			memset(&vfmt, 0, sizeof(struct vbi_format));

			vfmt.sampling_rate = 27000000; // ITU-R Rec. 601
			vfmt.sample_format = VIDEO_PALETTE_RAW;

			vfmt.start[0] = 1000;
			vfmt.start[1] = 1000;

			for (i = 0; vbi_services[i].id; i++) {
				if (!(vbi_services[i].id & services))
					continue;

				if (vbi_services[i].scanning != vbi->scanning) {
					services &= ~vbi_services[i].id;
				}

				max_rate = MAX(max_rate,
					MAX(vbi_services[i].cri_rate,
					    vbi_services[i].bit_rate));

				/*
				 *  vfmt.samples_per_line is zero, the driver
				 *  has to return the actual value. Setting
				 *  samples_per_line is pointless when we
				 *  don't know a 0H offset.
				 */

				for (j = 0; j < 2; j++)
					if (vbi_services[i].first[j] &&
					    vbi_services[i].last[j]) {
						vfmt.start[j] =
							MIN(vfmt.start[j], vbi_services[i].first[j]);
						vfmt.count[j] =
							MAX(vfmt.start[j] + vfmt.count[j],
							    vbi_services[i].last[j] + 1) - vfmt.start[j];
					}
			}

			/* Single field allowed? */

			if (!vfmt.count[0]) {
				vfmt.start[0] = (vbi->scanning == 625) ? 6 : 10;
				vfmt.count[0] = 1;
			} else if (!vfmt.count[1]) {
				vfmt.start[1] = (vbi->scanning == 625) ? 318 : 272;
				vfmt.count[1] = 1;
			}

			if (!services) {
				DIAG("device cannot capture requested data services");
				goto failure;
			}

			if (ioctl(vbi->fd, VIDIOCSVBIFMT, &vfmt) == -1) {
				switch (errno) {
				case EBUSY:
					DIAG("device is already in use");
					break;

		    		default:
					IODIAG("VBI parameters rejected");
					break;
				}

				goto failure;
			}

		} // !opt_surrender

		if (vfmt.sample_format != VIDEO_PALETTE_RAW) {
			DIAG("unknown VBI sampling format %d, "
			     "please contact the maintainer of "
			     "this program for service", vfmt.sample_format);
			goto failure;
		}

		vbi->sampling_rate	= vfmt.sampling_rate;
		vbi->samples_per_line 	= vfmt.samples_per_line;
		if (vbi->scanning == 625)
			vbi->offset = 10.2e-6 * vfmt.sampling_rate;
		else if (vbi->scanning == 525)
			vbi->offset = 9.2e-6 * vfmt.sampling_rate;
		else /* we don't know */
			vbi->offset = 9.7e-6 * vfmt.sampling_rate;
		vbi->start[0] 		= vfmt.start[0];
		vbi->count[0] 		= vfmt.count[0];
		vbi->start[1] 		= vfmt.start[1];
		vbi->count[1] 		= vfmt.count[1];
		vbi->interlaced		= !!(vfmt.flags & VBI_INTERLACED);
		vbi->synchronous	= !(vfmt.flags & VBI_UNSYNC);

	} else // VIDIOCGVBIFMT failed

#endif // HAVE_V4L_VBI_FORMAT

	{
		int size;

		/*
		 *  If a more reliable method exists to identify the bttv
		 *  driver I'll be glad to hear about it. Lesson: Don't
		 *  call a v4l private ioctl without knowing who's
		 *  listening. All we know at this point: It's a csf, and
		 *  it may be a v4l device.
		 *  garetxe: This isn't reliable, bttv doesn't return
		 *  anything useful in vcap.name.
		 */
/*
		if (!strstr(vcap.name, "bttv") && !strstr(vcap.name, "BTTV")) {
			DIAG("unable to identify driver, has no standard VBI interface");
			goto failure;
		}
*/
		switch (vbi->scanning) {
		case 625:
			vbi->sampling_rate = 35468950;
			vbi->offset = 10.2e-6 * 35468950;
			break;

		case 525:
			vbi->sampling_rate = 28636363;
			vbi->offset = 9.2e-6 * 28636363;
			break;

		default:
			DIAG("driver clueless about video standard");
			goto failure;
		}

		vbi->samples_per_line 	= 2048;
		vbi->start[0] 		= -1; // who knows
		vbi->start[1] 		= -1;
		vbi->interlaced		= FALSE;
		vbi->synchronous	= TRUE;

		if ((size = ioctl(vbi->fd, BTTV_VBISIZE, 0)) == -1) {
			// BSD or older bttv driver.
			vbi->count[0] = 16;
			vbi->count[1] = 16;
		} else if (size % 2048) {
			DIAG("unexpected size of raw VBI buffer (broken driver?)");
			goto failure;
		} else {
			size /= 2048;
			vbi->count[0] = size >> 1;
			vbi->count[1] = size - vbi->count[0];
		}
	}

	if (!services) {
		DIAG("device cannot capture requested data services");
		goto failure;
	}

	if (!vbi->scanning && opt_strict >= 1) {
		if (vbi->start[1] <= 0 || !vbi->count[1]) {
			/*
			 *  We may have requested single field capture
			 *  ourselves, but then we had guessed already.
			 */
			DIAG("driver clueless about video standard");
			goto failure;
		}

		if (vbi->start[1] >= 286)
			vbi->scanning = 625;
		else
			vbi->scanning = 525;
	}

	// Nyquist

	if (vbi->sampling_rate < max_rate * 3 / 2) {
		DIAG("VBI sampling frequency too low");
		goto failure;
	} else if (vbi->sampling_rate < max_rate * 6 / 2) {
		DIAG("VBI sampling frequency too low for me");
		goto failure; // Need smarter bit slicer
	}

	add_services(vbi, services, opt_strict);

	if (!vbi->services) {
		DIAG("device cannot capture requested data services");
		goto failure;
	}

	buffer_size = (vbi->count[0] + vbi->count[1])
		      * vbi->samples_per_line;
/*
	if (!init_callback_fifo(&vbi->fifo,
	    wait_full_read, send_empty_read, NULL, NULL,
	    sizeof(vbi_sliced) * (vbi->count[0] + vbi->count[1]), fifo_depth)) {
		goto failure;
	}
*/

vbi->fifo.wait_full = wait_full_read;
vbi->fifo.send_empty = send_empty_read;
vbi->buf.allocated = malloc(sizeof(vbi_sliced) * (vbi->count[0] + vbi->count[1]));
vbi->buf.size = sizeof(vbi_sliced) * (vbi->count[0] + vbi->count[1]);

	if (!(vbi->raw_buffer[0].data = malloc(buffer_size))) {
//		uninit_fifo(&vbi->fifo);
		DIAG("virtual memory exhausted");
		goto failure;
	}

	vbi->raw_buffer[0].size = buffer_size;
	vbi->num_raw_buffers = 1;

	vbi->fifo.start = capture_on_read;
	vbi->fifo.user_data = vbi;

	*pvbi = vbi;

	return 1;

failure:
	close(vbi->fd);
	free(vbi);

	return 0;
}

#ifdef V4L2_MAJOR_VERSION
// #if HAVE_V4L2

#ifndef V4L2_BUF_TYPE_VBI // API rev. Sep 2000
#define V4L2_BUF_TYPE_VBI V4L2_BUF_TYPE_CAPTURE;
#endif

static buffer *
wait_full_stream(fifo *f)
{
	struct vbi_capture *vbi = f->user_data;
	struct v4l2_buffer vbuf;
	struct timeval tv;
	fd_set fds;
	buffer *b;
	int r = -1;

//	b = (buffer *) rem_head(&f->empty);
	b = &vbi->buf;

	while (r <= 0) {
		FD_ZERO(&fds);
		FD_SET(vbi->fd, &fds);

		tv.tv_sec = 2;
		tv.tv_usec = 0;

		r = select(vbi->fd + 1, &fds, NULL, NULL, &tv);

		if (r < 0 && errno == EINTR)
			continue;

		if (r == 0) { /* timeout */
			DIAG("VBI capture stalled, no station tuned in?");
			return NULL;
		} else if (r < 0) {
			IODIAG("Unknown VBI select failure");
			return NULL;
		}
	}

	vbuf.type = V4L2_BUF_TYPE_VBI;

	if (ioctl(vbi->fd, VIDIOC_DQBUF, &vbuf) == -1) {
		IODIAG("Cannot dequeue streaming I/O VBI buffer "
			"(broken driver or application?)");
		return NULL;
	}

	b->data = b->allocated;
	b->time = vbuf.timestamp / 1e9;

	if (opt_ccsim)
		cc_gen(vbi, vbi->raw_buffer[vbuf.index].data);

	b->used = sizeof(vbi_sliced) *
		decode(vbi, vbi->raw_buffer[vbuf.index].data,
		       (vbi_sliced *) b->data);

	if (ioctl(vbi->fd, VIDIOC_QBUF, &vbuf) == -1) {
//		unget_full_buffer(f, b);
		IODIAG("Cannot enqueue streaming I/O VBI buffer (broken driver?)");
		return NULL;
	}

	return b;
}

static void
send_empty_stream(fifo *f, buffer *b)
{
//	add_head(&f->empty, &b->node);
}

static bool
capture_on_stream(fifo *f)
{
	struct vbi_capture *vbi = f->user_data;
	int str_type = V4L2_BUF_TYPE_VBI;

	if (ioctl(vbi->fd, VIDIOC_STREAMON, &str_type) == -1) {
		IODIAG("Cannot start VBI capturing");
		return FALSE;
	}

	// Subsequent I/O shouldn't fail, let's try anyway
/*
	buffer *b;

	if ((b = wait_full_stream(f))) {
		unget_full_buffer(f, b);
		return TRUE;
	}

	return FALSE;
*/
}

static int
open_v4l2(struct vbi_capture **pvbi, char *dev_name,
	int fifo_depth, unsigned int services)
{
	struct v4l2_capability vcap;
	struct v4l2_format vfmt;
	struct v4l2_requestbuffers vrbuf;
	struct v4l2_buffer vbuf;
	struct v4l2_standard vstd;
	struct vbi_capture *vbi;
	int max_rate, i, j;

	if (!(vbi = calloc(1, sizeof(struct vbi_capture)))) {
		DIAG("Virtual memory exhausted\n");
		return 0;
	}

	if ((vbi->fd = open(dev_name, O_RDONLY)) == -1) {
		free(vbi);
		IODIAG("Cannot open %s", dev_name);
		return 0;
	}

	if (ioctl(vbi->fd, VIDIOC_QUERYCAP, &vcap) == -1) {
		close(vbi->fd);
		free(vbi);
		return -1; // not V4L2
	}

	DIAG("Opened %s (%s), ", dev_name, vcap.name);

	if (vcap.type != V4L2_TYPE_VBI) {
		DIAG("not a raw VBI device");
		goto failure;
	}

	if (ioctl(vbi->fd, VIDIOC_G_STD, &vstd) == -1) {
		/* mandatory, http://www.thedirks.org/v4l2/v4l2dsi.htm */
		IODIAG("cannot query current video standard (broken driver?)");
		goto failure;
	}

	vbi->scanning = vstd.framelines;
	/* add_services() eliminates non 525/625 */

	memset(&vfmt, 0, sizeof(vfmt));

	vfmt.type = V4L2_BUF_TYPE_VBI;

	if (ioctl(vbi->fd, VIDIOC_G_FMT, &vfmt) == -1) {
		IODIAG("cannot query current VBI parameters (broken driver?)");
		goto failure;
	}

	max_rate = 0;

	if (!opt_surrender) {
		vfmt.fmt.vbi.sampling_rate = 27000000;	// ITU-R Rec. 601
		vfmt.fmt.vbi.sample_format = V4L2_VBI_SF_UBYTE;

		vfmt.fmt.vbi.offset = 1000e-6 * 27e6;
		vfmt.fmt.vbi.start[0] = 1000;
		vfmt.fmt.vbi.count[0] = 0;
		vfmt.fmt.vbi.start[1] = 1000;
		vfmt.fmt.vbi.count[1] = 0;

		/*
		 *  Will only allocate as much as we need, eg.
		 *  two lines per frame for CC 525, and set
		 *  reasonable parameters.
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

			max_rate = MAX(max_rate,
				MAX(vbi_services[i].cri_rate,
				    vbi_services[i].bit_rate));

			signal = vbi_services[i].cri_bits / (double) vbi_services[i].cri_rate
				 + (vbi_services[i].frc_bits + vbi_services[i].payload * 8)
				   / (double) vbi_services[i].bit_rate;

			offset = (vbi_services[i].offset / 1e9 - left_margin) * 27e6 + 0.5;
			samples = (signal + left_margin + 1.0e-6) * 27e6 + 0.5;

			vfmt.fmt.vbi.offset =
				MIN(vfmt.fmt.vbi.offset, offset);

			vfmt.fmt.vbi.samples_per_line =
				MIN(vfmt.fmt.vbi.samples_per_line + vfmt.fmt.vbi.offset,
				    samples + offset) - vfmt.fmt.vbi.offset;

			for (j = 0; j < 2; j++)
				if (vbi_services[i].first[j] &&
				    vbi_services[i].last[j]) {
					vfmt.fmt.vbi.start[j] =
						MIN(vfmt.fmt.vbi.start[j],
						    vbi_services[i].first[j] + V4L2_LINE);
					vfmt.fmt.vbi.count[j] =
						MAX(vfmt.fmt.vbi.start[j] + vfmt.fmt.vbi.count[j],
						    vbi_services[i].last[j] + 1) - vfmt.fmt.vbi.start[j];
				}
		}

		/* API rev. Nov 2000 paranoia */

		if (!vfmt.fmt.vbi.count[0]) {
			vfmt.fmt.vbi.start[0] = ((vbi->scanning == 625) ? 6 : 10) + V4L2_LINE;
			vfmt.fmt.vbi.count[0] = 1;
		} else if (!vfmt.fmt.vbi.count[1]) {
			vfmt.fmt.vbi.start[1] = ((vbi->scanning == 625) ? 318 : 272) + V4L2_LINE;
			vfmt.fmt.vbi.count[1] = 1;
		}

		if (!services) {
			DIAG("device cannot capture requested data services");
			goto failure;
		}

		if (ioctl(vbi->fd, VIDIOC_S_FMT, &vfmt) == -1) {
			switch (errno) {
			case EBUSY:
				DIAG("device is already in use");
				break;

	    		default:
				IODIAG("VBI parameters rejected (broken driver?)");
			}

			goto failure;
		}
	}

	if (!services) {
		DIAG("device cannot capture requested data services");
		goto failure;
	}

	vbi->sampling_rate	= vfmt.fmt.vbi.sampling_rate;
	vbi->samples_per_line 	= vfmt.fmt.vbi.samples_per_line;
	vbi->offset		= vfmt.fmt.vbi.offset;
	vbi->start[0] 		= vfmt.fmt.vbi.start[0] - V4L2_LINE;
	vbi->count[0] 		= vfmt.fmt.vbi.count[0];
	vbi->start[1] 		= vfmt.fmt.vbi.start[1] - V4L2_LINE;
	vbi->count[1] 		= vfmt.fmt.vbi.count[1];
	vbi->interlaced		= !!(vfmt.fmt.vbi.flags & V4L2_VBI_INTERLACED);
	vbi->synchronous	= !(vfmt.fmt.vbi.flags & V4L2_VBI_UNSYNC);

	if (vfmt.fmt.vbi.sample_format != V4L2_VBI_SF_UBYTE) {
		DIAG("unknown VBI sampling format %d, "
		    "please contact maintainer of this program for service", vfmt.fmt.vbi.sample_format);
		goto failure;
	}

	// Nyquist

	if (vbi->sampling_rate < max_rate * 3 / 2) {
		DIAG("VBI sampling frequency too low");
		goto failure;
	} else if (vbi->sampling_rate < max_rate * 6 / 2) {
		DIAG("VBI sampling frequency too low for me");
		goto failure; // Need smarter bit slicer
	}

	add_services(vbi, services, opt_strict);

	if (!vbi->services) {
		DIAG("device cannot capture requested data services");
		goto failure;
	}

	if (vcap.flags & V4L2_FLAG_STREAMING && vcap.flags & V4L2_FLAG_SELECT) {
		vbi->stream = TRUE;
/*
		if (!init_callback_fifo(&vbi->fifo,
		    wait_full_stream, send_empty_stream, NULL, NULL,
		    sizeof(vbi_sliced) * (vbi->count[0] + vbi->count[1]), fifo_depth)) {
			goto failure;
		}
*/

vbi->fifo.wait_full = wait_full_stream;
vbi->fifo.send_empty = send_empty_stream;
vbi->buf.allocated = malloc(sizeof(vbi_sliced) * (vbi->count[0] + vbi->count[1]));
vbi->buf.size = sizeof(vbi_sliced) * (vbi->count[0] + vbi->count[1]);

		vbi->fifo.start = capture_on_stream;
		vbi->fifo.user_data = vbi;

		vrbuf.type = V4L2_BUF_TYPE_VBI;
		vrbuf.count = MAX_RAW_BUFFERS;

		if (ioctl(vbi->fd, VIDIOC_REQBUFS, &vfmt) == -1) {
			IODIAG("streaming I/O buffer request failed (broken driver?)");
			goto fifo_failure;
		}

		if (vrbuf.count == 0) {
			DIAG("no streaming I/O buffers granted, physical memory exhausted?");
			goto fifo_failure;
		}

		/*
		 *  Map capture buffers
		 */

		vbi->num_raw_buffers = 0;

		while (vbi->num_raw_buffers < vrbuf.count) {
			unsigned char *p;

			vbuf.type = V4L2_BUF_TYPE_VBI;
			vbuf.index = vbi->num_raw_buffers;

			if (ioctl(vbi->fd, VIDIOC_QUERYBUF, &vbuf) == -1) {
				IODIAG("streaming I/O buffer query failed");
				goto mmap_failure;
			}

			p = mmap(NULL, vbuf.length, PROT_READ | PROT_WRITE, // _WRITE for cc_sim
				MAP_SHARED, vbi->fd, vbuf.offset); // _PRIVATE ?

			if ((int) p == -1) {
				if (errno == ENOMEM && vbi->num_raw_buffers >= 2)
					break;
			    	else {
					IODIAG("memory mapping failure");
					goto mmap_failure;
				}
			} else {
				int i, s;

				vbi->raw_buffer[vbi->num_raw_buffers].data = p;
				vbi->raw_buffer[vbi->num_raw_buffers].size = vbuf.length;

				for (i = s = 0; i < vbuf.length; i++)
					s += p[i];

				if (s % vbuf.length)
					printf("Security warning: driver %s (%s) seems to mmap "
					       "physical memory uncleared.\n", dev_name, vcap.name);
			}

			if (ioctl(vbi->fd, VIDIOC_QBUF, &vbuf) == -1) {
				IODIAG("cannot enqueue streaming I/O buffer (broken driver?)");
				goto mmap_failure;
			}

			vbi->num_raw_buffers++;
		}
	} else if (vcap.flags & V4L2_FLAG_READ) {
		int buffer_size = (vbi->count[0] + vbi->count[1])
				  * vbi->samples_per_line;
/*
		if (!init_callback_fifo(&vbi->fifo,
		    wait_full_read, send_empty_read, NULL, NULL,
		    sizeof(vbi_sliced) * (vbi->count[0] + vbi->count[1]), fifo_depth)) {
			goto failure;
		}
*/

vbi->fifo.wait_full = wait_full_read;
vbi->fifo.send_empty = send_empty_read;
vbi->buf.allocated = malloc(sizeof(vbi_sliced) * (vbi->count[0] + vbi->count[1]));
vbi->buf.size = sizeof(vbi_sliced) * (vbi->count[0] + vbi->count[1]);

		if (!(vbi->raw_buffer[0].data = malloc(buffer_size))) {
			DIAG("virtual memory exhausted");
			goto fifo_failure;
		}

		vbi->raw_buffer[0].size = buffer_size;
		vbi->num_raw_buffers = 1;

		vbi->fifo.start = capture_on_read;
		vbi->fifo.user_data = vbi;
	} else {
		DIAG("broken driver, no data interface");
		goto failure;
	}

	*pvbi = vbi;

	return 1;

mmap_failure:
	for (; vbi->num_raw_buffers > 0; vbi->num_raw_buffers--)
		munmap(vbi->raw_buffer[vbi->num_raw_buffers - 1].data,
		       vbi->raw_buffer[vbi->num_raw_buffers - 1].size);

fifo_failure:
//	uninit_fifo(&vbi->fifo);

failure:
	close(vbi->fd);
	free(vbi);

	return 0;
}

#else // !HAVE_V4L2

static int
open_v4l2(struct vbi_capture **pvbi, char *dev_name,
	int fifo_depth, unsigned int services)
{
	return -1;
}

#endif // !HAVE_V4L2

/*
 *  Service decoding (test)
 */

#define CACHE_LINE 32

/*
 *  ETS 300 706 8.1 Odd parity
 */
unsigned char
odd_parity[256] __attribute__ ((aligned(CACHE_LINE))) =
{
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0
};

unsigned char
bit_reverse[256] __attribute__ ((aligned(CACHE_LINE))) = {
	0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,	0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
	0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,	0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
	0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,	0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
	0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,	0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
	0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,	0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
	0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,	0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
	0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,	0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
	0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,	0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
	0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,	0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
	0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,	0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
	0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,	0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
	0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,	0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
	0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,	0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
	0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,	0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
	0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,	0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
	0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,	0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff
};

/*
 *  ETS 300 706 8.2 Hamming 8/4
 */
char
hamming84[256] __attribute__ ((aligned(CACHE_LINE))) =
{
	0x01,   -1, 0x01, 0x01,   -1, 0x00, 0x01,   -1,	  -1, 0x02, 0x01,   -1, 0x0a,   -1,   -1, 0x07,
	  -1, 0x00, 0x01,   -1, 0x00, 0x00,   -1, 0x00,	0x06,   -1,   -1, 0x0b,   -1, 0x00, 0x03,   -1,
	  -1, 0x0c, 0x01,   -1, 0x04,   -1,   -1, 0x07,	0x06,   -1,   -1, 0x07,   -1, 0x07, 0x07, 0x07,
	0x06,   -1,   -1, 0x05,   -1, 0x00, 0x0d,   -1,	0x06, 0x06, 0x06,   -1, 0x06,   -1,   -1, 0x07,
	  -1, 0x02, 0x01,   -1, 0x04,   -1,   -1, 0x09,	0x02, 0x02,   -1, 0x02,   -1, 0x02, 0x03,   -1,
	0x08,   -1,   -1, 0x05,   -1, 0x00, 0x03,   -1,	  -1, 0x02, 0x03,   -1, 0x03,   -1, 0x03, 0x03,
	0x04,   -1,   -1, 0x05, 0x04, 0x04, 0x04,   -1,	  -1, 0x02, 0x0f,   -1, 0x04,   -1,   -1, 0x07,
	  -1, 0x05, 0x05, 0x05, 0x04,   -1,   -1, 0x05,	0x06,   -1,   -1, 0x05,   -1, 0x0e, 0x03,   -1,
	  -1, 0x0c, 0x01,   -1, 0x0a,   -1,   -1, 0x09,	0x0a,   -1,   -1, 0x0b, 0x0a, 0x0a, 0x0a,   -1,
	0x08,   -1,   -1, 0x0b,   -1, 0x00, 0x0d,   -1,	  -1, 0x0b, 0x0b, 0x0b, 0x0a,   -1,   -1, 0x0b,
	0x0c, 0x0c,   -1, 0x0c,   -1, 0x0c, 0x0d,   -1,	  -1, 0x0c, 0x0f,   -1, 0x0a,   -1,   -1, 0x07,
	  -1, 0x0c, 0x0d,   -1, 0x0d,   -1, 0x0d, 0x0d,	0x06,   -1,   -1, 0x0b,   -1, 0x0e, 0x0d,   -1,
	0x08,   -1,   -1, 0x09,   -1, 0x09, 0x09, 0x09,	  -1, 0x02, 0x0f,   -1, 0x0a,   -1,   -1, 0x09,
	0x08, 0x08, 0x08,   -1, 0x08,   -1,   -1, 0x09,	0x08,   -1,   -1, 0x0b,   -1, 0x0e, 0x03,   -1,
	  -1, 0x0c, 0x0f,   -1, 0x04,   -1,   -1, 0x09,	0x0f,   -1, 0x0f, 0x0f,   -1, 0x0e, 0x0f,   -1,
	0x08,   -1,   -1, 0x05,   -1, 0x0e, 0x0d,   -1,	  -1, 0x0e, 0x0f,   -1, 0x0e, 0x0e,   -1, 0x0e
};

static inline int
unham84(unsigned char *d)
{
	int c1, c2;

	c1 = hamming84[d[0]];
	c2 = hamming84[d[1]];

	if ((int)(c1 | c2) < 0)
		return -1;

	return c2 * 16 + c1;
}

/* tables.c */

extern const char * country_names_en[];

extern const struct pdc_cni {
	int		country;
	char *		short_name;	/* 8 chars */
	char *		long_name;
	unsigned short	cni1;		/* Packet 8/30 format 1 */
	unsigned short	cni2;		/* Packet 8/30 format 2 */
	unsigned char	a, b;		/* Packet X/26 */
} PDC_CNI[];

extern const struct vps_cni {
	int		country;
	char *		short_name;
	char *		long_name;
	unsigned int	cni;
} VPS_CNI[];

extern const char *program_class[16];
extern const char *program_type[8][16];

/* end of tables.c */

static const char *pcs_names[] = {
	"unknown", "mono", "stereo", "bilingual"
};

static const char *month_names[] = {
	"0", "Jan", "Feb", "Mar", "Apr",
	"May", "Jun", "Jul", "Aug",
	"Sep", "Oct", "Nov", "Dec",
	"13", "14", "15"
};

#define printable(c) ((((c) & 0x7F) < 0x20 || ((c) & 0x7F) > 0x7E) ? '.' : ((c) & 0x7F))

#define PIL(day, mon, hour, min) \
	(((day) << 15) + ((mon) << 11) + ((hour) << 6) + ((min) << 0))

static void
dump_pil(int pil)
{
	int day, mon, hour, min;

	day = pil >> 15;
	mon = (pil >> 11) & 0xF;
	hour = (pil >> 6) & 0x1F;
	min = pil & 0x3F;

	if (pil == PIL(0, 15, 31, 63))
		printf(" PDC: Timer-control (no PDC)\n");
	else if (pil == PIL(0, 15, 30, 63))
		printf(" PDC: Recording inhibit/terminate\n");
	else if (pil == PIL(0, 15, 29, 63))
		printf(" PDC: Interruption\n");
	else if (pil == PIL(0, 15, 28, 63))
		printf(" PDC: Continue\n");
	else if (pil == PIL(31, 15, 31, 63))
		printf(" PDC: No time\n");
	else
		printf(" PDC: %05x, %2d %s %02d:%02d\n",
			pil, day, month_names[mon], hour, min);
}

static void
dump_pty(int pty)
{
	if (pty == 0xFF)
		printf(" Prog. type: %02x unused", pty);
	else
		printf(" Prog. type: %02x class %s", pty, program_class[pty >> 4]);

	if (pty < 0x80) {
		if (program_type[pty >> 4][pty & 0xF])
			printf(", type %s", program_type[pty >> 4][pty & 0xF]);
		else
			printf(", type undefined");
	}

	putchar('\n');
}

static void
dump_status(unsigned char *buf)
{
	int j;

	printf(" Status: \"");

	for (j = 0; j < 20; j++) {
		char c = odd_parity[buf[j]] ? buf[j] : '?';

		c = printable(c);
		putchar(c);
	}

	printf("\"\n");
}

static void
decode_vps2(unsigned char *buf)
{
	static char pr_label[20];
	static char label[20];
	static int l = 0;
	int cni, pcs, pty, pil;
	int c, j;

	printf("\nVPS:\n");

	c = bit_reverse[buf[1]];

	if ((char) c < 0) {
		label[l] = 0;
		memcpy(pr_label, label, sizeof(pr_label));
		l = 0;
	}

	c &= 0x7F;

	label[l] = printable(c);

	l = (l + 1) % 16;

	printf(" 3-10: %02x %02x %02x %02x %02x %02x %02x %02x (\"%s\")\n",
		buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], pr_label);

	pcs = buf[2] >> 6;

	cni = + ((buf[10] & 3) << 10)
	      + ((buf[11] & 0xC0) << 2)
	      + ((buf[8] & 0xC0) << 0)
	      + (buf[11] & 0x3F);

	pil = ((buf[8] & 0x3F) << 14) + (buf[9] << 6) + (buf[10] >> 2);

	pty = buf[12];

	if (cni)
		for (j = 0; VPS_CNI[j].short_name; j++)
			if (VPS_CNI[j].cni == cni) {
				printf(" Country: %s\n Station: %s%s\n",
					country_names_en[VPS_CNI[j].country],
					VPS_CNI[j].long_name,
					(cni == 0x0DC3) ? ((buf[2] & 0x10) ? " (ZDF)" : " (ARD)") : "");
				break;
			}

//	if (!cni || !VPS_CNI[j].short_name)
		printf(" CNI: %04x\n", cni);

	printf(" Analog audio: %s\n", pcs_names[pcs]);

	dump_pil(pil);
	dump_pty(pty);
}

static void
decode_pdc2(unsigned char *buf)
{
	int lci, luf, prf, pcs, mi, pty;
	int cni, cni_vps, pil;
	int j;

	lci = (buf[0] >> 2) & 3;
	luf = !!(buf[0] & 2);
	prf = buf[0] & 1;
	pcs = buf[1] >> 6;
	mi = !!(buf[1] & 0x20);

	cni_vps =
	      + ((buf[4] & 0x03) << 10)
	      + ((buf[5] & 0xC0) << 2)
	      + (buf[2] & 0xC0)
	      + (buf[5] & 0x3F);

	cni = cni_vps + ((buf[1] & 0x0F) << 12);

	pil = ((buf[2] & 0x3F) << 14) + (buf[3] << 6) + (buf[4] >> 2);

	pty = buf[6];

	printf(" Label channel %d: label update %d,"
	       " prepare to record %d, mode %d\n",
		lci, luf, prf, mi);

	if (cni) {
		for (j = 0; VPS_CNI[j].short_name; j++)
			if (VPS_CNI[j].cni == cni_vps) {
				printf(" Country: %s\n Station: %s%s\n",
					country_names_en[VPS_CNI[j].country],
					VPS_CNI[j].long_name,
					(cni_vps == 0x0DC3) ? ((buf[2] & 0x10) ? " (ZDF)" : " (ARD)") : "");
				break;
			}

		if (!VPS_CNI[j].short_name)
			for (j = 0; PDC_CNI[j].short_name; j++)
				if (PDC_CNI[j].cni2 == cni) {
					printf(" Country: %s\n Station: %s\n",
						country_names_en[PDC_CNI[j].country],
						PDC_CNI[j].long_name);
					break;
				}
	}

//	if (!cni || !PDC_CNI[j].short_name)
		printf(" CNI: %04x\n", cni);

	printf(" Analog audio: %s\n", pcs_names[pcs]);

	dump_pil(pil);
	dump_pty(pty);
}

static void
decode_8301(unsigned char *buf)
{
	int j, cni, cni_vps, lto;
	int mjd, utc_h, utc_m, utc_s;
	struct tm tm;
	time_t ti;

	cni = bit_reverse[buf[9]] * 256 + bit_reverse[buf[10]];
	cni_vps = cni & 0x0FFF;

	lto = (buf[11] & 0x7F) >> 1;

	mjd = + ((buf[12] & 0xF) - 1) * 10000
	      + ((buf[13] >> 4) - 1) * 1000
	      + ((buf[13] & 0xF) - 1) * 100
	      + ((buf[14] >> 4) - 1) * 10
	      + ((buf[14] & 0xF) - 1);

	utc_h = ((buf[15] >> 4) - 1) * 10 + ((buf[15] & 0xF) - 1);
	utc_m = ((buf[16] >> 4) - 1) * 10 + ((buf[16] & 0xF) - 1);
	utc_s = ((buf[17] >> 4) - 1) * 10 + ((buf[17] & 0xF) - 1);

	if (cni) {
		for (j = 0; VPS_CNI[j].short_name; j++)
			if (VPS_CNI[j].cni == cni_vps) {
				printf(" Country: %s\n Station: %s%s\n",
					country_names_en[VPS_CNI[j].country],
					VPS_CNI[j].long_name,
					(cni_vps == 0x0DC3) ? ((buf[2] & 0x10) ? " (ZDF)" : " (ARD)") : "");
				break;
			}

		if (!VPS_CNI[j].short_name)
			for (j = 0; PDC_CNI[j].short_name; j++)
				if (PDC_CNI[j].cni1 == cni) {
					printf(" Country: %s\n Station: %s\n",
					    	country_names_en[PDC_CNI[j].country],
						PDC_CNI[j].long_name);
					break;
				}
	}

//	if (!cni || !PDC_CNI[j].short_name)
		printf(" CNI: %04x\n", cni);

	ti = (mjd - 40587) * 86400 + 43200;
	localtime_r(&ti, &tm);

	printf(" Local time: MJD %d %02d %s %04d, UTC %02d:%02d:%02d %c%02d%02d\n",
		mjd, tm.tm_mday, month_names[tm.tm_mon + 1], tm.tm_year + 1900,
		utc_h, utc_m, utc_s,
		(buf[11] & 0x80) ? '-' : '+', lto >> 1, (lto & 1) * 30);
}

static void
decode_ttx2(unsigned char *buf, int line)
{
	int packet_address;
	int magazine, packet;
	int designation;
	int c, j;

	packet_address = unham84(buf + 0);

	if (packet_address < 0)
		return; /* hamming error */

	magazine = packet_address & 7;
	packet = packet_address >> 3;

	if (magazine != 0 /* 0 -> 8 */ || packet != 30) {
		if (opt_teletext) {
			printf("WST %x %02d %03d >", magazine, packet, line);

			for (j = 0; j < 42; j++) {
				char c = printable(buf[j]);

				putchar(c);
			}

			putchar('<');
			putchar('\n');
		}

		return;
	}

	designation = hamming84[buf[2]]; 

	if (designation < 0) {
		return; /* hamming error */
	} else if (designation <= 1 && opt_cni) {
		printf("\nPacket 8/30/1:\n");

		decode_8301(buf);
		dump_status(buf + 22);
	} else if (designation <= 3 && opt_cni) {
		printf("\nPacket 8/30/2:\n");

		for (j = 0; j < 7; j++) {
			c = unham84(buf + j * 2 + 8);

			if (c < 0)
				return; /* hamming error */

			buf[j] = bit_reverse[c];
		}

		decode_pdc2(buf);
		dump_status(buf + 22);
	}
}

/* untested */

static void
decode_xds(unsigned char *buf)
{
	if (opt_xds) {
		char c;

		c = odd_parity[buf[0]] ? buf[0] & 0x7F : '?';
		c = printable(c);
		putchar(c);
		c = odd_parity[buf[1]] ? buf[1] & 0x7F : '?';
		c = printable(c);
		putchar(c);
		fflush(stdout);
	} else {
		// http://chroot.ath.cx/fade/etexts/hitch1.txt
	}
}

static void
decode_caption(unsigned char *buf)
{
	static bool xds_transport = FALSE;
	char c = buf[0] & 0x7F;

	// field 2 only, check?
	// 0x01xx..0x0Exx ASCII_or_NUL[0..32] 0x0Fcks
	if (odd_parity[buf[0]] && (c >= 0x01 && c <= 0x0F)) {
		decode_xds(buf);
		xds_transport = (c != 0x0F);
		return;
	} else if (xds_transport) {
		decode_xds(buf);
		return;
	}

	if (opt_caption) {
		c = odd_parity[buf[0]] ? buf[0] & 0x7F : '?';
		c = printable(c);
		putchar(c);
		c = odd_parity[buf[1]] ? buf[1] & 0x7F : '?';
		c = printable(c);
		putchar(c);
		fflush(stdout);
	}
}

/*
 *  Sliced
 */

#define SLICED_TELETEXT_B		(SLICED_TELETEXT_B_L10_625 | SLICED_TELETEXT_B_L25_625)
#define SLICED_CAPTION			(SLICED_CAPTION_625_F1 | SLICED_CAPTION_625 \
					 | SLICED_CAPTION_525_F1 | SLICED_CAPTION_525)
static void
decode_sliced(vbi_sliced *s, int lines)
{
	for (; lines; s++, lines--) {
		if ((s->id & SLICED_VPS) && opt_vps) {
			decode_vps2(s->data);
		} else if ((s->id & SLICED_TELETEXT_B) && (opt_teletext | opt_cni)) {
			decode_ttx2(s->data, s->line);
		} else if ((s->id & SLICED_CAPTION) && (opt_caption | opt_xds)) {
			decode_caption(s->data);
		}
	}
}

static void
dump_sliced(vbi_sliced *s, int lines, double time)
{
	int i, j;

	printf("sliced frame time: %f\n", time);

	for (; lines; s++, lines--)
		for (i = 0; vbi_services[i].id; i++) {
			if (s->id & vbi_services[i].id) {
				printf("%04x %3d >", s->id, s->line);

				for (j = 0; j < vbi_services[i].payload; j++) {
					char c = printable(s->data[j]);

					putchar(c);
				}

				putchar('<');
				putchar('\n');

				break;
			}
		}
}

static void
dump_sliced_raw(vbi_sliced *s, int lines, double time)
{
	static double last = 0.0;
	int i;

	fprintf(stderr, "%f\n%c", time - last, lines);

	for (; lines; s++, lines--)
		for (i = 0; vbi_services[i].id; i++) {
			if (s->id & vbi_services[i].id) {
				fprintf(stderr, "%c%c%c", i, s->line & 0xFF, s->line >> 8);
				fwrite(s->data, 1, vbi_services[i].payload, stderr);
				break;
			}
		}

	last = time;
}

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <X11/xpm.h>

Display *		display;
int			screen;
Colormap		cmap;
Window			window;
int			dst_w, dst_h;
GC			gc;
XEvent			event;
XImage *		ximage;
void *			ximgdata;
int			palette[256];
int			depth;
unsigned char *		save_raw;
int			draw_row, draw_offset;
int			draw_count = -1;

static void
draw(struct vbi_capture *vbi, unsigned char *data1)
{
	int lines = vbi->count[0] + vbi->count[1];
	int rem = vbi->samples_per_line - draw_offset;
	unsigned char buf[256];
	unsigned char *data = data1;
	int i, v, h0, field, end;
        XTextItem xti;

	if (draw_count == 0)
		return;

	if (draw_count > 0)
		draw_count--;

	memcpy(save_raw, data1, vbi->samples_per_line * lines);

	if (depth == 24) {
		unsigned int *p = ximgdata;
		
		for (i = (vbi->count[0] + vbi->count[1])
		     * vbi->samples_per_line; i >= 0; i--)
			*p++ = palette[(int) *data++];
	} else {
		unsigned short *p = ximgdata; // 64 bit safe?

		for (i = (vbi->count[0] + vbi->count[1])
		     * vbi->samples_per_line; i >= 0; i--)
			*p++ = palette[(int) *data++];
	}

	XPutImage(display, window, gc, ximage,
		draw_offset, 0, 0, 0, rem, lines);

	XSetForeground(display, gc, 0);

	if (rem < dst_w)
		XFillRectangle(display, window, gc,
			rem, 0, dst_w, lines);

	if ((v = dst_h - lines) <= 0)
		return;

	XSetForeground(display, gc, 0);
	XFillRectangle(display, window, gc,
		0, lines, dst_w, dst_h);

	XSetForeground(display, gc, ~0);

	field = (draw_row >= vbi->count[0]);

	if (vbi->start[field] < 0)
		xti.nchars = snprintf(buf, 255, "Row %d Line ?", draw_row);
	else if (field == 0) 
		xti.nchars = snprintf(buf, 255, "Row %d Line %d", draw_row,
			draw_row + vbi->start[0]);
	else
		xti.nchars = snprintf(buf, 255, "Row %d Line %d", draw_row,
			draw_row - vbi->count[0] + vbi->start[1]);

	xti.chars = buf;
	xti.delta = 0;
	xti.font = 0;

	XDrawText(display, window, gc, 4, lines + 12, &xti, 1);

	data1 += draw_offset + draw_row * vbi->samples_per_line;
	h0 = dst_h - (data1[0] * v) / 256;
	end = MIN(vbi->samples_per_line - draw_offset, dst_w);

	for (i = 1; i < end; i++) {
		int h = dst_h - (data1[i] * v) / 256;

		XDrawLine(display, window, gc, i - 1, h0, i, h);
		h0 = h;
	}
}

static void
xevent(struct vbi_capture *vbi)
{
	while (XPending(display)) {
		XNextEvent(display, &event);

		switch (event.type) {
		case KeyPress:
		{
			switch (XLookupKeysym(&event.xkey, 0)) {
			case 'g':
				draw_count = 1;
				break;

			case 'l':
				draw_count = -1;
				break;

			case 'q':
			case 'c':
				exit(EXIT_SUCCESS);

			case XK_Up:
			    if (draw_row > 0)
				    draw_row--;
			    goto redraw;

			case XK_Down:
			    if (draw_row < (vbi->count[0] + vbi->count[1] - 1))
				    draw_row++;
			    goto redraw;

			case XK_Left:
			    if (draw_offset > 0)
				    draw_offset -= 10;
			    goto redraw;

			case XK_Right:
			    if (draw_offset < vbi->samples_per_line - 10)
				    draw_offset += 10;
			    goto redraw;		    
			}

			break;
		}

	        case ButtonPress:
			break;

		case FocusIn:
			break;

		case ConfigureNotify:
			dst_w = event.xconfigurerequest.width;
			dst_h = event.xconfigurerequest.height;
redraw:
			if (draw_count == 0) {
				draw_count = 1;
				draw(vbi, save_raw);
			}

			break;

		case ClientMessage:
			exit(EXIT_SUCCESS);
		}
	}
}

static bool
init_graph(int ac, char **av, struct vbi_capture *vbi)
{
	Atom delete_window_atom;
	XWindowAttributes wa;
	int lines = vbi->count[0] + vbi->count[1];
	int i;

	if (!(display = XOpenDisplay(NULL))) {
		DIAG("No display");
		return FALSE;
	}

	screen = DefaultScreen(display);
	cmap = DefaultColormap(display, screen);
 
	window = XCreateSimpleWindow(display,
		RootWindow(display, screen),
		0, 0,		// x, y
		dst_w = 768, dst_h = lines + 110,
				// w, h
		2,		// borderwidth
		0xffffffff,	// fgd
		0x00000000);	// bgd 

	if (!window) {
		DIAG("No window");
		return FALSE;
	}
			
	XGetWindowAttributes(display, window, &wa);
	depth = wa.depth;
			
	if (depth != 15 && depth != 16 && depth != 24) {
		DIAG("Cannot run at colour depth %d\n", depth);
		return FALSE;
	}

	for (i = 0; i < 256; i++) {
		switch (depth) {
		case 15:
			palette[i] = ((i & 0xF8) << 7)
				   + ((i & 0xF8) << 2)
				   + ((i & 0xF8) >> 3);
				break;

		case 16:
			palette[i] = ((i & 0xF8) << 8)
				   + ((i & 0xFC) << 3)
				   + ((i & 0xF8) >> 3);
				break;

		case 24:
			palette[i] = (i << 16) + (i << 8) + i;
				break;
		}
	}

	if (depth == 24) {
		if (!(ximgdata = malloc(vbi->samples_per_line * lines * 4))) {
			DIAG("virtual memory exhausted");
			return FALSE;
		}
	} else
		if (!(ximgdata = malloc(vbi->samples_per_line * lines * 2))) {
			DIAG("virtual memory exhausted");
			return FALSE;
		}

	if (!(save_raw = malloc(vbi->samples_per_line * lines))) {
		DIAG("virtual memory exhausted");
		return FALSE;
	}

	ximage = XCreateImage(display,
		DefaultVisual(display, screen),
		DefaultDepth(display, screen),
		ZPixmap, 0, (char *) ximgdata,
		vbi->samples_per_line, lines,
		8, 0);

	if (!ximage) {
		DIAG("No ximage");
		return FALSE;
	}

	delete_window_atom = XInternAtom(display, "WM_DELETE_WINDOW", False);

	XSelectInput(display, window, KeyPressMask | ExposureMask | StructureNotifyMask);
	XSetWMProtocols(display, window, &delete_window_atom, 1);
	XStoreName(display, window, "VBI Graph - [cursor] [g]rab [l]ive");

	gc = XCreateGC(display, window, 0, NULL);

	XMapWindow(display, window);
	       
	XSync(display, False);

	return TRUE;
}


/*
 *  Main
 */

unsigned char *		dev_name = "/dev/vbi";

static const struct option
long_options[] = {
	{ "help",			no_argument,		NULL,		'h' },
	{ "device",			required_argument,	NULL,		'd' },
	{ "strict",			required_argument,	NULL,		's' },
	{ "surrender",			no_argument,		NULL,		'u' },
	{ "teletext",			no_argument,		&opt_teletext,	1 },
	{ "pdc",			no_argument,		&opt_cni,	1 },
	{ "vps",			no_argument,		&opt_vps,	1 },
	{ "caption",			no_argument,		&opt_caption,	1 },
	{ "xds",			no_argument,		&opt_xds,	1 },
	{ "sliced",			no_argument,		&opt_sliced,	1 },
	{ "profile",			no_argument,		&opt_profile,	1 },
	{ "ccsim",			no_argument,		&opt_ccsim,	1 },
	{ "pattern",			no_argument,		&opt_pattern,	1 },
	{ "all",			no_argument,		&opt_all,	1 },
	{ "graph",			no_argument,		&opt_graph,	1 },
	{ "raw",			no_argument,		&opt_raw,	1 },
	{ NULL }
};

void
usage(int ac, char **av)
{
	printf("Usage: %s options\n"
		"-d <dev>     open VBI device <dev>, default /dev/vbi\n"
		"Output options:\n"
		"--teletext   dump raw Teletext data\n"
		"--pdc        dump cooked PDC/CNI data\n"
		"--vps        dump cooked VPS data\n"
		"--caption    dump raw Closed Caption data\n"
		"--xds        dump raw XDS data\n"
		"--all        dump all of the above\n"
		"--sliced     dump cooked sliced data buffer\n"
		"Special offer:\n"
		"--ccsim      add simulated Closed Caption signal on line 21/22\n"
		"--profile    the decoder and bit_slicer.\n"
		"--surrender  accept the driver's current vbi parameters,\n"
		"             default yes, option toggles yes/no.\n"
		"--pattern    dump the decoder pattern memory\n"
		"             plus rough vbi analysis. Other output\n"
		"             options not recommended.\n"
		"--graph      draw funny patterns and lines\n"
		"--strict n   0: ignore the driver's vbi parameters as\n"
		"             far as possible; 1: match with data service\n"
		"             parameters; 2: paranoid matching. Default 0.\n"
		"\n"
		"--raw        dump RAW sliced data buffer on STDERR,\n"
		"             '%s --all --raw 2>raw_data' recommended.\n",
		av[0], av[0]);
}

void
options(int ac, char **av)
{
	char c;
	int index;

	while ((c = getopt_long(ac, av, "hd:s:", long_options, &index)) != -1) {
		switch (c) {
		case 0:
			break;

		case 'd':
			dev_name = optarg;
			break;
		
		case 's':
			opt_strict = strtol(optarg, NULL, 0);
			break;

		case 'u':
			opt_surrender ^= TRUE;
			break;

		case 'h':
		default:
			usage(ac, av);
			exit(EXIT_SUCCESS);
		}
	}
}

int
main(int ac, char **av)
{
	struct vbi_capture *vbi;
	buffer *b;
	int r;

	options(ac, av);

	if (opt_all)
		opt_teletext =
		opt_cni =
		opt_vps =
		opt_caption =
		opt_xds = 1;

	if (!opt_raw && !opt_sliced && !opt_pattern && !opt_profile && !opt_graph
	    && !opt_teletext && !opt_cni && !opt_vps && !opt_caption && !opt_xds) {
		printf("Please choose an output option\n");
		usage(ac, av);
		exit(EXIT_FAILURE);
	}

		if (!(r = open_v4l2(&vbi, dev_name, 1,
		    SLICED_TELETEXT_B | SLICED_VPS | SLICED_CAPTION)))
			goto failure;
	if (r < 0)
		if (!(r = open_v4l(&vbi, dev_name, 1,
		    SLICED_TELETEXT_B | SLICED_VPS | SLICED_CAPTION)))
			goto failure;
	if (r < 0)
		goto failure;

	if (opt_graph)
		if (!(init_graph(ac, av, vbi)))
			goto failure;

	if (!vbi->fifo.start(&vbi->fifo))
		goto failure;

	for (;;) {
		b = vbi->fifo.wait_full(&vbi->fifo);

		if (!b)
			goto failure;

		if (opt_raw)
			dump_sliced_raw((vbi_sliced *) b->data,	b->used / sizeof(vbi_sliced), b->time);

		if (opt_sliced)
			dump_sliced((vbi_sliced *) b->data, b->used / sizeof(vbi_sliced), b->time);

		if (opt_teletext | opt_cni | opt_vps | opt_caption | opt_xds)
			decode_sliced((vbi_sliced *) b->data, b->used / sizeof(vbi_sliced));

		vbi->fifo.send_empty(&vbi->fifo, b);

		if (opt_graph)
			xevent(vbi);
	}

	exit(EXIT_SUCCESS);

failure:
	puts(err_str);

	exit(EXIT_FAILURE);
}
