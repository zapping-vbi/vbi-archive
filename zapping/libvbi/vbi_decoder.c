/*
 *  V4L/V4L2 VBI Decoder  DRAFT
 *
 *  gcc -O2 -othis this.c mp1e/vbi/tables.c
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

/* $Id: vbi_decoder.c,v 1.1 2000-11-22 06:49:30 mschimek Exp $ */

/*
    TODO:
    - test streaming
    - write v4l interface
    - write cc test
    - write T thread & multi consumer fifo
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
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

	void *			user_data; // Useful for callbacks
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

tsc_t rdtsc(void)
{
	tsc_t tsc;

	asm ("\trdtsc\n" : "=A" (tsc));
	return tsc;
}

#define PROFILE 0

#define V4L2_LINE -1 // API rev. Nov 2000 (-1 -> 0)




/*
 *  Bit Slicer
 */

#define MOD_NRZ_LSB_ENDIAN	0
#define MOD_NRZ_MSB_ENDIAN	1
#define MOD_BIPHASE_LSB_ENDIAN	2
#define MOD_BIPHASE_MSB_ENDIAN	3

struct bit_slicer {
	unsigned int	framing_code, frc_mask;
	int		frc_bytes;
	int		phase_shift, step8;
	int		frc_rate;
	int		oversampling_rate;
	int		payload;
	int		thresh;
	bool		lsb_endian;
};

#define OVERS 2		// 1, 2, 4, 8

static inline void
init_bit_slicer(struct bit_slicer *d,
	int raw_bytes, int sampling_rate, int frc_rate, int bit_rate,
	unsigned int frc, unsigned int frc_mask, int payload, int modulation)
{
	d->frc_mask		= frc_mask;
	d->framing_code 	= frc & frc_mask;
	d->frc_bytes		= raw_bytes - ((long long) sampling_rate * (8 * payload + 1)) / bit_rate;
	d->frc_rate		= frc_rate;
	d->oversampling_rate	= sampling_rate * OVERS;
	d->payload		= payload;
	d->thresh		= 105 << 10;
	d->step8		= sampling_rate * 256.0 / bit_rate;
	d->lsb_endian		= TRUE;

	switch (modulation) {
	case MOD_NRZ_MSB_ENDIAN:
		d->lsb_endian = FALSE;
	case MOD_NRZ_LSB_ENDIAN:
		d->phase_shift = sampling_rate * 256.0 / frc_rate * .5
			         + sampling_rate * 256.0 / bit_rate * .5 + 128;
		break;

	case MOD_BIPHASE_MSB_ENDIAN:
		d->lsb_endian = FALSE;
	case MOD_BIPHASE_LSB_ENDIAN:
		d->phase_shift = sampling_rate * 256.0 / frc_rate * .5
			         + sampling_rate * 256.0 / bit_rate * .25 + 128;
		break;
	}
}

static inline bool
bit_slicer(struct bit_slicer *d, unsigned char *raw, unsigned char *buf)
{
	int i, j, k;
        int tr, b, b1 = 0, cl = 0;
	unsigned int c = 0, t, s;

	for (i = d->frc_bytes; i > 0; raw++, i--) {
		tr = d->thresh >> 10;
		d->thresh += ((int) raw[0] - tr) * nbabs(raw[1] - raw[0]);
		t = raw[0] * OVERS;

		for (j = OVERS; j > 0; j--) {
			s = (t + (OVERS / 2)) / OVERS;
			b = (s >= tr);

    			if (b ^ b1) {
				cl = d->oversampling_rate >> 1;
			} else {
				cl += d->frc_rate;

				if (cl >= d->oversampling_rate) {
					cl -= d->oversampling_rate;

					c = c * 2 + b;

					if ((c & d->frc_mask) == d->framing_code) {
						i = d->phase_shift;
						c = 0;

						if (d->lsb_endian)
							for (j = d->payload; j > 0; j--) {
								for (k = 8; k > 0; k--) {
						    			c >>= 1;
									c += (raw[i >> 8] >= tr) << 7;
			    						i += d->step8;
								}

								*buf++ = c;
					    		}
						else
							for (j = d->payload; j > 0; j--) {
								for (k = 8; k > 0; k--) {
									c = c * 2 + (raw[i >> 8] >= tr);
			    						i += d->step8;
								}

								*buf++ = c;
					    		}

			    			return TRUE;
					}
				}
			}

			b1 = b;

			if (OVERS > 1) {
				t += raw[1];
				t -= raw[0];
			}
		}
	}

	return FALSE;
}

#define MAX_RAW_BUFFERS 	5
#define MAX_JOBS		16

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

	struct job {
		unsigned int		id;
		struct bit_slicer 	slicer;
		int			offset;
		int			start;
		int			count;
	}			jobs[MAX_JOBS];
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
	int		frc_rate;	/* Hz */
	int		bit_rate;	/* Hz */
	int		scanning;	/* scanning system: 525 (FV = 59.94 Hz, FH = 15734 Hz),
							    625 (FV = 50 Hz, FH = 15625 Hz) */
	unsigned int	frc;		/* Clock Run In and FRaming Code, LSB last txed bit of FRC */
	unsigned int	frc_mask;	/* identification, advice: ignore leading CRI bits */
	int		payload;	/* in bytes */
	int		modulation;	/* payload modulation */
};

const struct vbi_service_par
vbi_services[] = {
	{
		SLICED_TELETEXT_B_L10_625, "Teletext System B, 625",
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
		625, 0x00AAAAE4, 0x0000FFFF, 42, MOD_NRZ_LSB_ENDIAN
	}, {
		SLICED_TELETEXT_B_L25_625, "Teletext System B Level 2.5, 625",
		{ 6, 318 },
		{ 22, 335 },
		10300, 6937500, 6937500, /* 444 x FH */
		625, 0x00AAAAE4, 0x0000FFFF, 42, MOD_NRZ_LSB_ENDIAN
	}, {
		SLICED_VPS, "Video Programming System",
		{ 16, 0 },
		{ 16, 0 },
		12500, 5000000, 2500000, /* 160 x FH */
		625, 0xAAAA8A99, 0x00FFFFFF, 13, MOD_BIPHASE_MSB_ENDIAN
	}, {
		SLICED_CAPTION_625_F1, "Closed Caption 625, single field",
		{ 22, 0 },
		{ 22, 0 },
		10500, 1000000, 500000, /* 32 x FH */
		625, 0x00055543, 0x00000FFF, 2, MOD_NRZ_LSB_ENDIAN
	}, {
		SLICED_CAPTION_625, "Closed Caption 625", /* Videotapes, LD */
		{ 22, 335 },
		{ 22, 335 },
		10500, 1000000, 500000, /* 32 x FH */
		625, 0x00055543, 0x00000FFF, 2, MOD_NRZ_LSB_ENDIAN
	}, {
		SLICED_VBI_625, "VBI 625", /* Blank VBI */
		{ 6, 318 },
		{ 22, 335 },
		10000, 1510000, 1510000,
		625, 0, 0, 10, 0 /* 10.0-2 ... 62.9+1 us */
	}, {
		SLICED_NABTS, "Teletext System C, 525", /* NOT CONFIRMED */
		{ 10, 0 },
		{ 21, 0 },
		10500, 5727272, 5727272,
		525, 0x00AAAAE7, 0x0000FFFF, 33, MOD_NRZ_LSB_ENDIAN /* Tb. */
	}, {
		SLICED_CAPTION_525_F1, "Closed Caption 525, single field",
		{ 21, 0 },
		{ 21, 0 },
		10500, 1006976, 503488, /* 32 x FH */
		525, 0x00055543, 0x00000FFF, 2, MOD_NRZ_LSB_ENDIAN
		/*
		 *  CRI is actually 6.5 cycles at 32 x FH
		 *  ('10101010101' at 64 x FH), FRC '001' at 32 x FH
		 */
	}, {
		SLICED_CAPTION_525, "Closed Caption 525",
		{ 21, 284 },
		{ 21, 284 },
		10500, 1006976, 503488, /* 32 x FH */
		525, 0x00055543, 0x00000FFF, 2, MOD_NRZ_LSB_ENDIAN
	}, {
		SLICED_2xCAPTION_525, "2xCaption 525", /* NOT CONFIRMED */
		{ 10, 0 },
		{ 21, 0 },
		10500, 1006976, 1006976, /* 64 x FH */
		525, 0x000554ED, 0x00000FFF, 4, MOD_NRZ_LSB_ENDIAN /* Tb. */
	}, {
		SLICED_TELETEXT_BD_525, "Teletext System B / D (Japan), 525", /* NOT CONFIRMED */
		{ 10, 0 },
		{ 21, 0 },
		9600, 5727272, 5727272,
		525, 0x00AAAAE4, 0x0000FFFF, 34, MOD_NRZ_LSB_ENDIAN /* Tb. */
	}, {
		SLICED_VBI_525, "VBI 525", /* Blank VBI */
		{ 10, 272 },
		{ 21, 284 },
		9500, 1510000, 1510000,
		525, 0, 0, 10, 0 /* 9.5-1 ... 62.4+1 us */
	},
	{ 0 }
};

typedef struct {
	unsigned int		id;		/* SLICED_.. */
	int			line;		/* ITU-R line number 1..n, 0: unknown */
	unsigned char		data[48];
} vbi_sliced;

#if PROFILE

/* bit_slicer appx 20 kcycles on my machine, too slow?
   Sum appx 1 (CC only),
   20 (PAL: WST & VPS & CC),
   40 (PAL, probe all lines) Mcycles/s
   decode() should learn which lines carry what services
 */

static int
decode(struct vbi_capture *vbi, unsigned char *raw1, vbi_sliced *out1)
{
	int pitch = vbi->samples_per_line << vbi->interlaced;
	struct job *job = vbi->jobs;
	vbi_sliced *out = out1;
	int i, j, repeats = 0;
	tsc_t sum = 0LL;

	for (i = 0; i < vbi->num_jobs; i++, job++) {
		unsigned char *raw = raw1 + job->offset;

		for (j = 0; j < job->count; j++, raw += pitch) {
			tsc_t begin = rdtsc();

			if (bit_slicer(&job->slicer, raw, out->data)) {
				out->id = job->id;
				out->line = job->start ? job->start + j : 0;
				out++;
			}

			sum += rdtsc() - begin;
			repeats++;
		}
	}

	fprintf(stderr, "decode() %d jobs, %d raw lines, %d active; bit_slicer() %lld cycles, %d/frame\n",
		vbi->num_jobs, vbi->count[0] + vbi->count[1], out - out1,
		sum / repeats, repeats);

	return out - out1;
}

#else

static int
decode(struct vbi_capture *vbi, unsigned char *raw1, vbi_sliced *out1)
{
	int pitch = vbi->samples_per_line << vbi->interlaced;
	struct job *job = vbi->jobs;
	vbi_sliced *out = out1;
	int i, j;

	for (i = 0; i < vbi->num_jobs; i++, job++) {
		unsigned char *raw = raw1 + job->offset;

		for (j = 0; j < job->count; j++, raw += pitch)
			if (bit_slicer(&job->slicer, raw, out->data)) {
				out->id = job->id;
				out->line = job->start ? job->start + j : 0;
				out++;
			}
	}

	return out - out1;
}

#endif

static unsigned int
remove_services(struct vbi_capture *vbi, unsigned int services)
{
	int i;

	for (i = 0; i < vbi->num_jobs;) {
		if (vbi->jobs[i].id & services) {
			memmove(&vbi->jobs[i], &vbi->jobs[i + 1],
				(MAX_JOBS - i - 1) * sizeof(vbi->jobs[0]));

			vbi->num_jobs--;
		} else
			i++;
	}

	return vbi->services &= ~services;
}

static unsigned int
add_services(struct vbi_capture *vbi, unsigned int services, int strict)
{
	double off_min = (vbi->scanning == 525) ? 7.9e-6 : 8.0e-6;
	struct job *job = &vbi->jobs[vbi->num_jobs];
	int i, j, k;

	services &= ~(SLICED_VBI_525 | SLICED_VBI_625);

	for (i = 0; vbi_services[i].id; i++) {
		double signal;
		int frc_bits, skip = 0;

		if (vbi->num_jobs >= MAX_JOBS - 2)
			break;

		if (!(vbi_services[i].id & services))
			continue;

		if (vbi_services[i].scanning != vbi->scanning)
			goto eliminate;

		if ((int) vbi_services[i].frc < 0)
			frc_bits = 32;
		else
			for (frc_bits = 0; vbi_services[i].frc >> frc_bits; frc_bits++);

		signal = frc_bits / (double) vbi_services[i].frc_rate
			 + vbi_services[i].payload * 8 / (double) vbi_services[i].bit_rate;

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

		for (j = k = 0; j < 2; j++) {
			int start = vbi->start[j];
			int end = start + vbi->count[j] - 1;
			int row;

			if (!vbi->synchronous)
				goto eliminate; // too difficult

			if (!(vbi_services[i].first[j] && vbi_services[i].last[j]))
				continue;

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

				row = MAX(0, (int) vbi_services[i].first[j] - start);
				job[k].start = MAX(start, vbi_services[i].first[j]);
				job[k].count = MIN(end, vbi_services[i].last[j])
					       - job->start + 1;
			} else {
				row = 0;
				job[k].start = (vbi->start[j] > 0) ? vbi->start[j] : 0;
				job[k].count = vbi->count[j];
			}

			if (vbi->interlaced)
				job[k].offset = skip + (row * 2 + j) *
					vbi->samples_per_line;
			else
				job[k].offset = skip + (row + vbi->count[0] * j) *
					vbi->samples_per_line;

			job[k].id = vbi_services[i].id;

			init_bit_slicer(&job[k].slicer,
				vbi->samples_per_line - skip,
				vbi->sampling_rate,
		    		vbi_services[i].frc_rate,
				vbi_services[i].bit_rate,
				vbi_services[i].frc,
				vbi_services[i].frc_mask,
				vbi_services[i].payload,
				vbi_services[i].modulation);

			k++;
		}

		job += k;
		vbi->num_jobs += k;
		vbi->services |= vbi_services[i].id;
eliminate:
	}

	services = 0;

	if (vbi->services & SLICED_TELETEXT_B_L25_625)
		services |= SLICED_TELETEXT_B_L10_625;
	if (vbi->services & SLICED_CAPTION_625)
		services |= SLICED_CAPTION_625_F1;
	if (vbi->services & SLICED_CAPTION_525)
		services |= SLICED_CAPTION_525_F1;

	if (vbi->services & services)
		remove_services(vbi, services);

	return vbi->services;
}

static void
reset_decoder(struct vbi_capture *vbi)
{
	vbi->services = 0;
	vbi->num_jobs = 0;

	memset(vbi->jobs, 0, sizeof(vbi->jobs));
}



/*
 *  Device Specific Code / Callback Interface
 */

static buffer *
wait_full_read(fifo *f)
{
	struct vbi_capture *vbi = f->user_data;
	struct timeval tv;
	buffer *b;
	size_t r;

//	dequeue

    b = &vbi->buf;

	r = read(vbi->fd, vbi->raw_buffer[0].data,
		 vbi->raw_buffer[0].size);

	if (r != vbi->raw_buffer[0].size) {
		return NULL;
	}

	gettimeofday(&tv, NULL);

	b->data = b->allocated;
	b->time = tv.tv_sec + tv.tv_usec / 1e6;

	b->used = sizeof(vbi_sliced) *
		decode(vbi, vbi->raw_buffer[0].data,
		       (vbi_sliced *) b->data);

    	return b;
}

static void
send_empty_read(fifo *f, buffer *b)
{
//	enqueue
}

static bool
capture_on_read(fifo *f)
{
	return TRUE;
// XXX EBUSY test
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

		if (r == 0) {
			/* VBI capture timeout */
			return NULL;
		} else if (r < 0) {
			/* select failure -> errno */
			return NULL;
		}
	}

	vbuf.type = V4L2_BUF_TYPE_VBI;

	if (ioctl(vbi->fd, VIDIOC_DQBUF, &vbuf) == -1) {
		return NULL;
	}

	b->time = vbuf.timestamp / 1e9;
	b->used = sizeof(vbi_sliced) *
		decode(vbi, vbi->raw_buffer[vbuf.index].data,
		       (vbi_sliced *) b->data);

	if (ioctl(vbi->fd, VIDIOC_QBUF, &vbuf) == -1) {
// unget b
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

	return (ioctl(vbi->fd, VIDIOC_STREAMON, &str_type) != -1);

// XXX EBUSY
}

static int
open_v4l2(struct vbi_capture **pvbi, char *dev_name, int fifo_depth, bool surrender, unsigned int services)
{
	struct v4l2_capability vcap;
	struct v4l2_format vfmt;
	struct v4l2_standard vstd;
	struct v4l2_requestbuffers vrbuf;
	struct v4l2_buffer vbuf;
	struct vbi_capture *vbi;
	int max_rate, i, j;

	if (!(vbi = calloc(1, sizeof(struct vbi_capture))))
		return 0;

	if ((vbi->fd = open(dev_name, O_RDONLY)) == -1) {
		free(vbi);
		return 0;
	}

	if (ioctl(vbi->fd, VIDIOC_QUERYCAP, &vcap) == -1) {
		close(vbi->fd);
		free(vbi);
		return -1; // not V4L2
	}

	if (vcap.type != V4L2_TYPE_VBI) {
		goto failure;
	}

	if (ioctl(vbi->fd, VIDIOC_G_STD, &vstd) == -1) {
		goto failure;
	}

	vbi->scanning = vstd.framelines;

	memset(&vfmt, 0, sizeof(vfmt));

	vfmt.type = V4L2_BUF_TYPE_VBI;

	if (ioctl(vbi->fd, VIDIOC_G_FMT, &vfmt) == -1) {
		goto failure;
	}

	vfmt.fmt.vbi.sample_format = V4L2_VBI_SF_UBYTE;

	max_rate = 0;

	if (!surrender) {
		vfmt.fmt.vbi.sampling_rate = 27000000;	// ITU-R Rec. 601

		vfmt.fmt.vbi.offset = 1000e-6 * 27e6;
		vfmt.fmt.vbi.start[0] = 1000;
		vfmt.fmt.vbi.start[1] = 1000;

		/*
		 *  Will only allocate as much as we need, eg.
		 *  two lines per frame for CC 525, and set
		 *  reasonable parameters.
		 */
		for (i = 0; vbi_services[i].id; i++) {
			double left_margin = (vbi->scanning == 525) ? 1.0e-6 : 2.0e-6;
			int frc_bits, offset, samples;
			double signal;

			if (!(vbi_services[i].id & services))
				continue;

			if (vbi_services[i].scanning != vbi->scanning) {
				services &= ~vbi_services[i].id;
			}

			max_rate = MAX(max_rate,
				MAX(vbi_services[i].frc_rate,
				    vbi_services[i].bit_rate));

			if ((int) vbi_services[i].frc < 0)
				frc_bits = 32;
			else
				for (frc_bits = 0; vbi_services[i].frc >> frc_bits; frc_bits++);

			signal = frc_bits / (double) vbi_services[i].frc_rate
				 + vbi_services[i].payload * 8 / (double) vbi_services[i].bit_rate;

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
						MAX(vfmt.fmt.vbi.count[j],
						    vbi_services[i].last[j]
						    - vbi_services[i].first[j] + 1);
				}
		}

		// Paranoia

		if (!vfmt.fmt.vbi.count[0]) {
			vfmt.fmt.vbi.start[0] = ((vbi->scanning == 625) ? 6 : 10) + V4L2_LINE;
			vfmt.fmt.vbi.count[0] = vfmt.fmt.vbi.count[1];
		} else if (!vfmt.fmt.vbi.count[1]) {
			vfmt.fmt.vbi.start[1] = ((vbi->scanning == 625) ? 318 : 272) + V4L2_LINE;
			vfmt.fmt.vbi.count[1] = vfmt.fmt.vbi.count[0];
		}
	}

	if (!services) {
		goto failure;
	}

	if (ioctl(vbi->fd, VIDIOC_S_FMT, &vfmt) == -1) {
		switch (errno) {
		case EBUSY:
			break;

		default:
			break;
		}

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
		goto failure;
	}

	// Nyquist

	if (vbi->sampling_rate < max_rate * 3 / 2) {
		goto failure;
	} else if (vbi->sampling_rate < max_rate * 6 / 2) {
		goto failure; // Need smarter bit slicer
	}

	add_services(vbi, services, 0);

	if (!vbi->services) {
		goto failure;
	}

	if (vcap.flags & V4L2_FLAG_STREAMING
	    && vcap.flags & V4L2_FLAG_SELECT) {
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
			goto fifo_failure;
		}

		if (vrbuf.count == 0) {
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
				goto mmap_failure;
			}

			p = mmap(NULL, vbuf.length, PROT_READ, MAP_SHARED, vbi->fd, vbuf.offset);

			if ((int) p == -1) {
				if (errno == ENOMEM && vbi->num_raw_buffers > 0)
					break;
			    	else {
					goto mmap_failure;
				}
			} else {
				vbi->raw_buffer[vbi->num_raw_buffers].data = p;
				vbi->raw_buffer[vbi->num_raw_buffers].size = vbuf.length;
			}

			if (ioctl(vbi->fd, VIDIOC_QBUF, &vbuf) == -1) {
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
			goto fifo_failure;
		}

		vbi->raw_buffer[0].size = buffer_size;
		vbi->num_raw_buffers = 1;

		vbi->fifo.start = capture_on_read;
		vbi->fifo.user_data = vbi;
	} else {
		goto failure;
	}

	*pvbi = vbi;

	return 1;

mmap_failure:
	for (; vbi->num_raw_buffers > 0; vbi->num_raw_buffers--)
		munmap(vbi->raw_buffer[vbi->num_raw_buffers - 1].data,
		       vbi->raw_buffer[vbi->num_raw_buffers - 1].size);

fifo_failure:
/*
	uninit_fifo(&vbi->fifo);
*/

failure:
	close(vbi->fd);
	free(vbi);

	return 0;
}

#endif // HAVE_V4L2

/*
 *  Station identification
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
		if (0) {
			printf("%x %02d %02d >", magazine, packet, line);

			for (j = 0; j < 42; j++) {
				char c = printable(buf[j]);

				putchar(c);
			}

			putchar('\n');
		}

		return;
	}

	designation = hamming84[buf[2]]; 

	if (designation < 0) {
		return; /* hamming error */
	} else if (designation <= 1) {
		printf("\nPacket 8/30/1:\n");

		decode_8301(buf);
		dump_status(buf + 22);
	} else if (designation <= 3) {
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

#define SLICED_TELETEXT_B		(SLICED_TELETEXT_B_L10_625 | SLICED_TELETEXT_B_L25_625)
#define SLICED_CAPTION			(SLICED_CAPTION_625_F1 | SLICED_CAPTION_625 \
					 | SLICED_CAPTION_525_F1 | SLICED_CAPTION_525)
static void
cni_sliced(vbi_sliced *s, int lines)
{
	for (; lines; s++, lines--) {
		if (s->id & SLICED_VPS) {
			decode_vps2(s->data);
		} else if (s->id & SLICED_TELETEXT_B) {
			decode_ttx2(s->data, s->line);
		} else if (s->id & SLICED_CAPTION) {
			;
		}
	}
}

static void
dump_sliced(vbi_sliced *s, int lines, double time)
{
	int i, j;

	printf("time: %f\n", time);

	for (; lines; s++, lines--)
		for (i = 0; vbi_services[i].id; i++) {
			if (s->id == vbi_services[i].id) {
				printf("%04x %3d >", s->id, s->line);

				for (j = 0; j < vbi_services[i].payload; j++) {
					char c = s->data[j] & 0x7F;

					if (c < 0x20 || c == 0x7F)
						c = '.';

					putchar(c);
				}

				putchar('<');
				putchar('\n');

				break;
			}
		}
}

int
main(int ac, char **av)
{
	struct vbi_capture *vbi;
	buffer *b;

	if (open_v4l2(&vbi, "/dev/vbi1", 1, 1,
	    SLICED_TELETEXT_B | SLICED_VPS | SLICED_CAPTION) <= 0)
		exit(EXIT_FAILURE);

	if (!vbi->fifo.start(&vbi->fifo))
		exit(EXIT_FAILURE);

	for (;;) {
		b = vbi->fifo.wait_full(&vbi->fifo);

		if (!b)
			exit(EXIT_FAILURE);

		if (1)
			dump_sliced((vbi_sliced *) b->data,
				b->used / sizeof(vbi_sliced), b->time);

		if (1) // check if WST & VPS are correctly sampled
			cni_sliced((vbi_sliced *) b->data,
				b->used / sizeof(vbi_sliced));

		vbi->fifo.send_empty(&vbi->fifo, b);
	}

	exit(EXIT_SUCCESS);
}
