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

/* $Id: v4l2.c,v 1.9 2001-03-31 11:10:26 garetxe Exp $ */

#include <ctype.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <asm/types.h>
#include "../videodev2.h"
#include "../common/log.h"
#include "../common/math.h"
#include "vbi.h"

struct v4l2_context {
	struct vbi_context	vbi;
	int			fd;
};

#ifdef V4L2_MAJOR_VERSION

#ifndef V4L2_BUF_TYPE_VBI // V4L2 0.20 erratum
#define V4L2_BUF_TYPE_VBI V4L2_BUF_TYPE_CAPTURE;
#endif

static buffer *
wait_full_stream(fifo *f)
{
	struct v4l2_context *v4l2 = f->user_data;
	struct v4l2_buffer vbuf;
	struct timeval tv;
	fd_set fds;
	buffer *b;
	int r = -1;

	while (r <= 0) {
		FD_ZERO(&fds);
		FD_SET(v4l2->fd, &fds);

		tv.tv_sec = 2;
		tv.tv_usec = 0;

		r = select(v4l2->fd + 1, &fds, NULL, NULL, &tv);

		if (r < 0 && errno == EINTR)
			continue;

		if (r == 0)
			FAIL("VBI capture timeout");

		ASSERT("execute select", r > 0);
	}

	vbuf.type = V4L2_BUF_TYPE_VBI;

	ASSERT("dequeue vbi capture buffer",
		ioctl(v4l2->fd, VIDIOC_DQBUF, &vbuf) == 0);

	b = v4l2->vbi.fifo.buffers + vbuf.index;
	b->time = vbuf.timestamp / 1e9;

    	return b;
}

static void
send_empty_stream(fifo *f, buffer *b)
{
	struct v4l2_context *v4l2 = f->user_data;
	struct v4l2_buffer vbuf;

	vbuf.type = V4L2_BUF_TYPE_VBI;
	vbuf.index = b->index;

	ASSERT("enqueue vbi capture buffer",
		ioctl(v4l2->fd, VIDIOC_QBUF, &vbuf) == 0);
}

static bool
capture_on(fifo *f)
{
	struct v4l2_context *v4l2 = f->user_data;
	int str_type = V4L2_BUF_TYPE_VBI;

	return ioctl(v4l2->fd, VIDIOC_STREAMON, &str_type) == 0;
}

#endif // HAVE_V4L2

static buffer *
wait_full_read(fifo *f)
{
	struct v4l2_context *v4l2 = f->user_data;
	buffer *b = &f->buffers[0];
	struct timeval tv;

	ASSERT("read from vbi device",
		b->size == read(v4l2->fd, b->data, b->size));

	gettimeofday(&tv, NULL);
	b->time = tv.tv_sec + tv.tv_usec / 1e6;

    	return b;
}

static void
send_empty_read(fifo *f, buffer *b)
{
}

#ifdef V4L2_MAJOR_VERSION

static bool
open_v4l2(struct v4l2_context *v4l2, char *dev_name)
{
	struct v4l2_capability vcap;
	struct v4l2_standard vstd;
	struct v4l2_format vfmt;
	struct v4l2_requestbuffers vrbuf;
	struct v4l2_buffer vbuf;
	int buffer_size;

	if (ioctl(v4l2->fd, VIDIOC_QUERYCAP, &vcap) != 0)
		return FALSE;

	printv(2, "Opened %s (%s)\n", dev_name, vcap.name);

	if (vcap.type != V4L2_TYPE_VBI)
		FAIL("%s (%s) is not a vbi device",
			dev_name, vcap.name);

	ASSERT("query current video standard",
		ioctl(v4l2->fd, VIDIOC_G_STD, &vstd) == 0);

	if ((double) vstd.framerate.denominator / vstd.framerate.numerator >= 29.0)
		FAIL("Sorry, no NTSC VBI support\n");

	memset(&vfmt, 0, sizeof(vfmt));

	// Let's be a nice application and make some suggestions

	vfmt.type = V4L2_BUF_TYPE_VBI;
	vfmt.fmt.vbi.sampling_rate = 27000000;	// ITU-R Rec. 601
	vfmt.fmt.vbi.sample_format = V4L2_VBI_SF_UBYTE;
	// XXX offset, bpl
	vfmt.fmt.vbi.start[0] = 6;
	vfmt.fmt.vbi.count[0] = 16;
	vfmt.fmt.vbi.start[1] = 318;
	vfmt.fmt.vbi.count[1] = 16;

	ASSERT("request vbi format",
		ioctl(v4l2->fd, VIDIOC_S_FMT, &vfmt) == 0);

	// Let's see what we got

	if (vfmt.fmt.vbi.sampling_rate < 14000000
	    || vfmt.fmt.vbi.start[0] > 6
	    || vfmt.fmt.vbi.start[1] > 318
	    || vfmt.fmt.vbi.start[0] + vfmt.fmt.vbi.count[0] < 22
	    || vfmt.fmt.vbi.start[1] + vfmt.fmt.vbi.count[1] < 334) {
		FAIL("Cannot capture Teletext with %s (%s)\n",
			dev_name, vcap.name);
		// XXX check strong field order (for VPS, top field only)
		// XXX check hor. offset and line width
		// XXX relax requirements, may still work (VPS, certain TTX)
	}

	// We're smart

	v4l2->vbi.interlaced = !!(vfmt.fmt.vbi.flags & V4L2_VBI_INTERLACED);

	// Not *that* smart

    	if (vfmt.fmt.vbi.sample_format != V4L2_VBI_SF_UBYTE) {
		FAIL("Unknown vbi sampling format %d\n",
			vfmt.fmt.vbi.sample_format);
	}

	v4l2->vbi.sampling_rate		= vfmt.fmt.vbi.sampling_rate;
	v4l2->vbi.samples_per_line 	= vfmt.fmt.vbi.samples_per_line;
	v4l2->vbi.start[0] 		= vfmt.fmt.vbi.start[0];
	v4l2->vbi.count[0] 		= vfmt.fmt.vbi.count[0];
	v4l2->vbi.start[1] 		= vfmt.fmt.vbi.start[1];
	v4l2->vbi.count[1] 		= vfmt.fmt.vbi.count[1];

	buffer_size = (v4l2->vbi.count[0] + v4l2->vbi.count[1])
			* v4l2->vbi.samples_per_line;

	if (vcap.flags & V4L2_FLAG_STREAMING) {
		printv(2, "Using V4L2 VBI streaming interface\n");

		vrbuf.type = V4L2_BUF_TYPE_VBI;
		vrbuf.count = 5;

		ASSERT("request capture buffers",
			ioctl(v4l2->fd, VIDIOC_REQBUFS, &vrbuf) == 0);

		if (vrbuf.count == 0)
			FAIL("No vbi capture buffers granted");

		printv(2, "%d vbi capture buffers granted\n", vrbuf.count);

		ASSERT("init vbi capture fifo", init_callback_fifo(
			&v4l2->vbi.fifo, "vbi-v4l2-stream",
			wait_full_stream, send_empty_stream, NULL, vrbuf.count, 0));

		v4l2->vbi.fifo.start = capture_on;
		v4l2->vbi.fifo.user_data = v4l2;

		// Map capture buffers

		v4l2->vbi.fifo.num_buffers = 0;

		while (v4l2->vbi.fifo.num_buffers < vrbuf.count) {
			unsigned char *p;

			vbuf.type = V4L2_BUF_TYPE_VBI;
			vbuf.index = v4l2->vbi.fifo.num_buffers;

			printv(3, "Mapping vbi capture buffer #%d\n", v4l2->vbi.fifo.num_buffers);

			ASSERT("query vbi capture buffer #%d", ioctl(v4l2->fd, VIDIOC_QUERYBUF, &vbuf) == 0,
				v4l2->vbi.fifo.num_buffers);

			p = mmap(NULL, vbuf.length, PROT_READ, MAP_SHARED, v4l2->fd, vbuf.offset);

			if ((int) p == -1) {
				if (errno == ENOMEM && v4l2->vbi.fifo.num_buffers > 0)
					break;
			    	else
					FAIL("Failed to map capture buffer #%d",
						v4l2->vbi.fifo.num_buffers);
			} else
				v4l2->vbi.fifo.buffers[v4l2->vbi.fifo.num_buffers].data = p;

			ASSERT("enqueue vbi capture buffer #%d",
				ioctl(v4l2->fd, VIDIOC_QBUF, &vbuf) == 0, vbuf.index);

			v4l2->vbi.fifo.num_buffers++;
		}
	} else {
		printv(2, "Using V4L2 VBI read interface\n");

		ASSERT("init vbi capture fifo", init_callback_fifo(
			&v4l2->vbi.fifo, "vbi-v4l2-read",
			wait_full_read, send_empty_read, NULL, buffer_size, 1));

		v4l2->vbi.fifo.user_data = v4l2;
	}

	return TRUE;
}

#else // !HAVE_V4L2

static bool
open_v4l2(struct v4l2_context *v4l2, char *dev_name)
{
	return FALSE;
}

#endif // !HAVE_V4L2

void
close_vbi_v4l2(fifo *f)
{
	struct v4l2_context *v4l2;

	if (f) {
		v4l2 = f->user_data;

		uninit_fifo(f);

		if (v4l2->fd >= 0)
			close(v4l2->fd);

		v4l2->fd = -1;

		free(v4l2);
	}
}

fifo *
open_vbi_v4l2(char *dev_name)
{
	struct v4l2_context *v4l2;

	ASSERT("allocate vbi context",
		(v4l2 = calloc(1, sizeof(struct v4l2_context))));

	ASSERT("open vbi capture device %s",
		(v4l2->fd = open(dev_name, O_RDONLY)) != -1, dev_name);

	if (!open_v4l2(v4l2, dev_name)) {
		int buffer_size;

		// Obscure other device

		printv(2, "Trying BTTV VBI interface\n");
		
		// XXX check capab

		v4l2->vbi.start[0] 		= 6;
		v4l2->vbi.count[0] 		= 16;
		v4l2->vbi.start[1] 		= 318;
		v4l2->vbi.count[1] 		= 16;
		v4l2->vbi.samples_per_line 	= 2048;
		v4l2->vbi.sampling_rate		= 35468950;
		v4l2->vbi.interlaced		= FALSE;

		buffer_size = 32 * 2048;

		ASSERT("init vbi capture fifo", init_callback_fifo(
			&v4l2->vbi.fifo, "vbi-v4l",
			wait_full_read, send_empty_read, NULL, buffer_size, 1));

		v4l2->vbi.fifo.user_data = v4l2;
	}

	return &v4l2->vbi.fifo;
}
