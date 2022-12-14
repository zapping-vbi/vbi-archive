/*
 *  MPEG-1 Real Time Encoder
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

/* $Id: file.c,v 1.2 2002-08-22 22:05:14 mschimek Exp $ */

#include <ctype.h>
#include <assert.h>
#include "../common/log.h"
#include "../common/fifo.h"
#include "../options.h"
#include "../b_mp1e.h"
#include "../common/math.h"
#include "../video/video.h"

#define FILTER_MODE CM_YUYV // CM_YUV | CM_YUYV

enum { FREE = 0, BUSY };

static fifo			cap_fifo;
static producer			cap_prod;

static int			buffer_size;
static int			width0, height0;

static int
ppm_getc(FILE *fi)
{
	int c = getc(fi);

	if (c == '#')
		do c = getc(fi);
		while (c != '\n' && c != '\r' && c != EOF);

	return c;
}

static int
ppm_getint(FILE *fi, int *val)
{
	int c;

	do c = ppm_getc(fi);
	while (c != EOF && isspace(c));

	*val = 0;

	while (isdigit(c)) {
		*val = *val * 10 + c - '0';
		c = ppm_getc(fi);
	}

	return (c != EOF);
}

static int
ppm_read(unsigned char *d1, char *name_template, int count)
{
	static const double coef[7][3] = {
		{ 0.2125,0.7154,0.0721 }, /* ITU-R Rec. 709 (1990) */
		{ 0.299, 0.587, 0.114 },  /* unspecified */
		{ 0.299, 0.587, 0.114 },  /* reserved */
		{ 0.30,  0.59,  0.11 },   /* FCC */
		{ 0.299, 0.587, 0.114 },  /* ITU-R Rec. 624-4 System B, G */
		{ 0.299, 0.587, 0.114 },  /* SMPTE 170M */
		{ 0.212, 0.701, 0.087 }   /* SMPTE 240M (1987) */
	};
	char name[1024], buf[2];
	double cr, cg, cb, cu, cv;
	int maxcol, n, x, y, w, h;
	unsigned char *d;
	unsigned char *du1, *dv1;
	unsigned char *du, *dv;
	int pitch;
	FILE *fi;

	cr = coef[1][0];
	cg = coef[1][1];
	cb = coef[1][2];
	cu = 0.5 / (1.0 - cb);
	cv = 0.5 / (1.0 - cr);

	snprintf(name, sizeof(name), name_template, count);

	if (!(fi = fopen(name, "r"))) {
		if (count > 0 && errno == ENOENT)
			return -1;
		ASSERT("open '%s'", 0, name);
	}

	printv(3, "Reading %s'%s'\n", d1 ? "" : "image size from ", name);

	n = fread(buf, sizeof(char), 2, fi);

	ASSERT("read image file '%s'", !ferror(fi), name);

	if (n != 2 || buf[0] != 'P' || buf[1] != '6') {
		fprintf(stderr, "%s: '%s' is not a raw .ppm file\n",
			program_invocation_short_name, name);
		return 0;
	}

	ASSERT("read image file", ppm_getint(fi, &w));
	ASSERT("read image file", ppm_getint(fi, &h));
	ASSERT("read image file", ppm_getint(fi, &maxcol));

	if (w <= 0 || h <= 0) {
		fprintf(stderr, "%s: '%s' is corrupted\n",
			program_invocation_short_name, name);
		return 0;
	}

	if (d1 == NULL) {
		width0 = width = w;
		height0 = height = h;
	} else {
		if (width0 != w || height0 != h) {
			return 0;
		}

		pitch = ((width + 15) & -16);

		du1 = d1 + pitch * height;
		dv1 = d1 + pitch * height * 5 / 4;

		for (y = 0; y < h; y++) {
			d = d1;
			du = du1;
			dv = dv1;

			for (x = 0; x < w; x += 2) {
				int r, g, b;
				double y1, u1, v1;
				double y2, u2, v2;

				r = getc(fi);
				g = getc(fi);
				b = getc(fi);

				if (r == EOF || g == EOF || b == EOF)
					return 0;

				y1 = cr * r + cg * g + cb * b;
				u1 = cu * (b - y1);
				v1 = cv * (r - y1);

				if (x + 1 == w)
					r = g = b = 0;
				else {
					r = getc(fi);
					g = getc(fi);
					b = getc(fi);
				}

				if (r == EOF || g == EOF || b == EOF)
					return 0;

				y2 = cr * r + cg * g + cb * b;
				u2 = cu * (b - y2);
				v2 = cv * (r - y2);

				u1 = (u1 + u2 + 1) / 2.0;
				v1 = (v1 + v2 + 1) / 2.0;

				if (filter_mode == CM_YUV) {
					d[0] = saturate((219.0 / 256.0) * y1 + 16.5, 0, 255);
					d[1] = saturate((219.0 / 256.0) * y2 + 16.5, 0, 255);

					d += 2;

					if (!(y & 1)) {
						*du++ = saturate((224.0 / 256.0) * u1 + 128.5, 0, 255);
						*dv++ = saturate((224.0 / 256.0) * v1 + 128.5, 0, 255);
					}
				} else {
					d[0] = saturate((219.0 / 256.0) * y1 + 16.5, 0, 255);
					d[1] = saturate((224.0 / 256.0) * u1 + 128.5, 0, 255);
					d[2] = saturate((219.0 / 256.0) * y2 + 16.5, 0, 255);
					d[3] = saturate((224.0 / 256.0) * v1 + 128.5, 0, 255);

					d += 4;
				}
			}

			if (filter_mode == CM_YUV) {
				d1 += pitch;

				if (!(y & 1)) {
					du1 += pitch >> 1; 
					dv1 += pitch >> 1;
				}
			} else
				d1 += pitch * 2;
		}
	}

	fclose(fi);
	return 1;
}

static void
wait_full(fifo *f)
{
	static double time = 0.0;
	static int count = 0;
	buffer *b;

	b = wait_empty_buffer(&cap_prod);

	b->time = time;

	switch (ppm_read(b->data, cap_dev, count)) {
	case 0:
		exit(EXIT_FAILURE);

	case -1:
		b->used = 0; /* EOF */
		send_full_buffer(&cap_prod, b);
		return;

	default:
		b->used = buffer_size;
		send_full_buffer(&cap_prod, b);
		break;
	}

	time += 1.0 / 24.0;

	count++;
}

fifo *
file_init(rte_video_stream_params *par, struct filter_param *fp)
{
	int len = strlen(cap_dev);

	if (len < 4 || strcmp(cap_dev + len - 4, ".ppm"))
		FAIL("Unknown file type '%s'", cap_dev);

	if (ppm_read(NULL, cap_dev, 0) <= 0)
		exit(EXIT_FAILURE);

	if (width < 1 || height < 1 ||
	    width > MAX_WIDTH || height > MAX_HEIGHT)
		FAIL("Images '%s' too big", cap_dev);

	par->frame_rate = 24.0;
	par->width  = (grab_width + 15) & -16;
	par->height = (grab_height + 15) & -16;

	switch (filter_mode = FILTER_MODE) {
	case CM_YUV:
		par->pixfmt = RTE_PIXFMT_YUV420;
		buffer_size = par->height * par->width * 3 / 2;
		par->stride = par->width;
		break;

	case CM_YUYV:
		par->pixfmt = RTE_PIXFMT_YUYV;
		buffer_size = par->height * par->width * 2;
		par->stride = par->width * 2;
		break;

	default:
		FAIL("Filter '%s' not supported",
			filter_labels[filter_mode]);
	}

	par->sample_aspect = video_sampling_aspect (
		par->frame_rate, par->width, par->height);

	filter_init(par, fp);

	ASSERT("init capture fifo", init_callback_fifo(
		&cap_fifo, "video-ppm",
		NULL, NULL, wait_full, NULL,
		video_look_ahead(gop_sequence), buffer_size));

	ASSERT("init capture producer",
		add_producer(&cap_fifo, &cap_prod));

	printv(2, "Reading images %d x %d named '%s'\n",
		par->width, par->height, cap_dev);

	return &cap_fifo;
}
