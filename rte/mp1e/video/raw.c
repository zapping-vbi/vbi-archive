/*
 *  MPEG-1 Real Time Encoder
 *
 *  Copyright (C) 1999-2001 Michael H. Schimek
 *
 *  YUV stream interface 2001 Harm van der Heijden
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

/* $Id: raw.c,v 1.2 2002-02-25 06:22:20 mschimek Exp $ */

#include <ctype.h>
#include <assert.h>
#include <unistd.h>
#include "../common/log.h"
#include "../common/fifo.h"
#include "../options.h"
#include "../b_mp1e.h"
#include "../common/math.h"
#include "video.h"
#include "mpeg.h"

enum { FREE = 0, BUSY };

static fifo			cap_fifo;
static producer			cap_prod;

static int			buffer_size;
static double			input_frame_rate;

static int raw_read(unsigned char* buf)
{
	long cnt;
	/*cnt = read(0, buf, buffer_size); */
	cnt = fread(buf, buffer_size, 1, stdin); 
	if (feof(stdin)) return -1;
	if (cnt != 1) {
		fprintf(stderr, "read only %ld bytes!\n", cnt);
		return 0;
	}
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

	switch (raw_read(b->data)) {
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

	time += 1.0 / input_frame_rate;

	count++;
}

#define PARAM_FAIL FAIL("Malformed parameter line '%s':\n" \
    "require raw:colorspace-width-height-frameratenum-framerateden", cap_dev)

/* param syntax: raw:colorspace-width-height-frameratenum-framerateden 
 * example: raw:yuv420-720-480-30000-1001 (typical NTSC values with 420 yuv) 
 *          raw:yuyv-720-576-25-1 (PAL with planar 422 yuv data) 
 */
static void parse_param()
{
	char *p, format[16];
	int fn,fd;

	if (strncmp(cap_dev, "raw:", 4))
		FAIL("Not a raw input descriptor: '%s'", cap_dev);
	p = strtok(&cap_dev[4], "-"); if (!p) PARAM_FAIL;

	if (! strcmp(p, "yuv420"))
		filter_mode = CM_YUV;
	else if (! strcmp(p, "yuyv"))
		filter_mode = CM_YUYV;
	else
		FAIL("Unknown format %s ; supported is yuv420", p);
	strncpy(format, p, 16); format[15]=0;
	p = strtok(0, "-"); if (!p) PARAM_FAIL;
	width = atoi(p);
	p = strtok(0, "-"); if (!p) PARAM_FAIL;
	height = atoi(p);
	p = strtok(0, "-"); if (!p) PARAM_FAIL;
	fn = atoi(p);
	if (fn <=0) FAIL("Illegal frame rate num %d", fn);
	p = strtok(0, "-"); if (!p) PARAM_FAIL;
	fd = atoi(p);
	if (fd <=0) FAIL("Illegal frame rate den %d", fd);
	input_frame_rate = (double)fn/(double)fd;
	printv(2, "format = %s, %dx%d @ %g Hz\n", format, width, height,
		input_frame_rate);	
}

fifo *
raw_init(rte_video_stream_params *par)
{
	/* get format, with, height, and framerate */
	parse_param();

	if (width < 1 || height < 1 ||
	    width > MAX_WIDTH || height > MAX_HEIGHT)
		FAIL("Images '%s' too big", cap_dev);

	if (width % 16)
		FAIL("Width %d not a multiple of 16\n", width);
	if (height % 16)
		FAIL("Height %d not a multiple of 16\n", height);

	par->frame_rate = input_frame_rate;
	par->width = width;
	par->height = height;

	switch (filter_mode) {
	case CM_YUV:
		par->pixfmt = RTE_PIXFMT_YUV420;
		buffer_size = par->height * par->width * 3 / 2;
		break;

	case CM_YUYV:
		par->pixfmt = RTE_PIXFMT_YUYV;
		buffer_size = par->height * par->width * 2;
		break;

	default:
		FAIL("Filter '%s' not supported", filter_labels[filter_mode]);
	}

	filter_init(par);

	ASSERT("init capture fifo", init_callback_fifo(
		&cap_fifo, "video-raw",
		NULL, NULL, wait_full, NULL,
		video_look_ahead(gop_sequence), buffer_size));

	ASSERT("init capture producer",
		add_producer(&cap_fifo, &cap_prod));

	printv(2, "Reading raw input %d x %d format '%s'\n",
		par->width, par->height, filter_labels[filter_mode]);

	return &cap_fifo;
}
