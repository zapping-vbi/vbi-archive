/*
 *  Zapzilla/libvbi - Raw VBI Decoder
 *
 *  Copyright (C) 2000 Michael H. Schimek
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

/* $Id: decoder.h,v 1.8 2001-09-02 03:25:58 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "../common/types.h"
#include "sliced.h"

/*
 *  Only device interface code includes this file.
 */

/* Bit slicer */

#ifndef TVENG_FRAME_PIXFORMAT
#define TVENG_FRAME_PIXFORMAT

enum tveng_frame_pixformat{
  /* common rgb formats */
  TVENG_PIX_RGB555,
  TVENG_PIX_RGB565,
  TVENG_PIX_RGB24,
  TVENG_PIX_BGR24,
  TVENG_PIX_RGB32,
  TVENG_PIX_BGR32,
  /* common YUV formats */
  TVENG_PIX_YVU420,
  TVENG_PIX_YUV420,
  TVENG_PIX_YUYV,
  TVENG_PIX_UYVY,
  TVENG_PIX_GREY
};

#endif /* TVENG_FRAME_PIXFORMAT */

#define MOD_NRZ_LSB_ENDIAN	0
#define MOD_NRZ_MSB_ENDIAN	1
#define MOD_BIPHASE_LSB_ENDIAN	2
#define MOD_BIPHASE_MSB_ENDIAN	3

struct vbi_bit_slicer {

	/* Private */

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
	int		endian;
	int		skip;
};

typedef bool (vbi_bit_slicer_fn)(struct vbi_bit_slicer *d, void *raw,
				 unsigned char *buf);

extern vbi_bit_slicer_fn *
                vbi_bit_slicer_init(struct vbi_bit_slicer *d,
				    int raw_samples, int sampling_rate,
				    int cri_rate, int bit_rate,
				    unsigned int cri_frc, unsigned int cri_mask,
				    int cri_bits, int frc_bits,
				    int payload, int modulation,
				    enum tveng_frame_pixformat fmt);
extern bool	vbi_bit_slicer(struct vbi_bit_slicer *d,
			       unsigned char *raw, unsigned char *buf);

/* Data services */

struct vbi_decoder {

	/* Sampling parameters */

	int			scanning;		/* 525, 625 */
	int			sampling_rate;		/* Hz */
	int			samples_per_line;
	int			offset;			/* 0H, samples */
	int			start[2];		/* ITU-R numbering */
	int			count[2];		/* field lines */
	bool			interlaced;
	bool			synchronous;

	/* Private */

#define MAX_JOBS		8
#define MAX_WAYS		4

	unsigned int		services;
	int			num_jobs;

	char *			pattern;
	struct vbi_decoder_job {
		unsigned int		id;
		int			offset;
		struct vbi_bit_slicer 	slicer;
	}			jobs[MAX_JOBS];
};

extern void		vbi_decoder_init(struct vbi_decoder *);
extern void		vbi_decoder_reset(struct vbi_decoder *);
extern void		vbi_decoder_destroy(struct vbi_decoder *);
extern unsigned int	vbi_decoder_add_services(struct vbi_decoder *,
						 unsigned int services,
						 int strict);
extern unsigned int	vbi_decoder_remove_services(struct vbi_decoder *,
						    unsigned int services);
extern unsigned int	vbi_decoder_qualify_sampling(struct vbi_decoder *,
						     int *max_rate,
						     unsigned int services);
extern int		vbi_decoder(struct vbi_decoder *vbi, unsigned char *raw,
				    vbi_sliced *out);
