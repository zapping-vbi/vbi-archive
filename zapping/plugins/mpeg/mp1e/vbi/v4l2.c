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

/* $Id: v4l2.c,v 1.3 2000-10-15 21:24:49 mschimek Exp $ */

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
#include "../common/fifo.h"
#include "../common/math.h"
#include "../options.h"
#include "vbi.h"

static int			fd;
static fifo			cap_fifo;

#ifdef V4L2_MAJOR_VERSION

static struct v4l2_capability	vcap;
static struct v4l2_standard	vstd;
static struct v4l2_format	vfmt;
static struct v4l2_requestbuffers vrbuf;
static struct v4l2_buffer	vbuf;

#ifndef V4L2_BUF_TYPE_VBI // V4L2 0.20 erratum
#define V4L2_BUF_TYPE_VBI V4L2_BUF_TYPE_CAPTURE;
#endif

static buffer *
wait_full_stream(fifo *f)
{
	struct timeval tv;
	fd_set fds;
	buffer *b;
	int r = -1;

	while (r <= 0) {
		FD_ZERO(&fds);
		FD_SET(fd, &fds);

		tv.tv_sec = 2;
		tv.tv_usec = 0;

		r = select(fd + 1, &fds, NULL, NULL, &tv);

		if (r < 0 && errno == EINTR)
			continue;

		if (r == 0)
			FAIL("VBI capture timeout");

		ASSERT("execute select", r > 0);
	}

	vbuf.type = V4L2_BUF_TYPE_VBI;

	ASSERT("dequeue vbi capture buffer", ioctl(fd, VIDIOC_DQBUF, &vbuf) == 0);

	b = cap_fifo.buffers + vbuf.index;
	b->time = vbuf.timestamp / 1e9;

    	return b;
}

static void
send_empty_stream(fifo *f, buffer *b)
{
	vbuf.type = V4L2_BUF_TYPE_VBI;
	vbuf.index = b->index;

	ASSERT("enqueue vbi capture buffer", ioctl(fd, VIDIOC_QBUF, &vbuf) == 0);
}

#endif

static buffer *
wait_full_read(fifo *f)
{
	struct timeval tv;
	buffer *b = &cap_fifo.buffers[0];

	ASSERT("read from vbi device", b->size == read(fd, b->data, b->size));

	gettimeofday(&tv, NULL);
	b->time = tv.tv_sec + tv.tv_usec / 1e6;

    	return b;
}

// Attn: in-order fifo

static void
send_empty_read(fifo *f, buffer *b)
{
}

static bool
capture_on(fifo *unused)
{
	int str_type = V4L2_BUF_TYPE_VBI;

	return ioctl(fd, VIDIOC_STREAMON, &str_type) == 0;
}

void
vbi_v4l2_init(void)
{
	int buffer_size;

	ASSERT("open vbi capture device", (fd = open(vbi_dev, O_RDONLY)) != -1);

#ifdef V4L2_MAJOR_VERSION

	if (ioctl(fd, VIDIOC_QUERYCAP, &vcap) != -1) {
		printv(2, "Opened %s ('%s')\n", vbi_dev, vcap.name);

		if (vcap.type != V4L2_TYPE_VBI)
			FAIL("%s ('%s') is not a vbi device",
				vbi_dev, vcap.name);

		ASSERT("query current video standard", ioctl(fd, VIDIOC_G_STD, &vstd) == 0);

		if ((double) vstd.framerate.denominator / vstd.framerate.numerator >= 29.0)
			FAIL("Sorry, no NTSC VBI support\n");

		memset(&vfmt, 0, sizeof(vfmt));

		// Let's be a nice application and make some suggestions

		vfmt.type = V4L2_BUF_TYPE_VBI;
		vfmt.fmt.vbi.sampling_rate = 27000000;	// ITU-R Rec. 601
		vfmt.fmt.vbi.sample_format = V4L2_VBI_SF_UBYTE;
		vfmt.fmt.vbi.start[0] = 6;
		vfmt.fmt.vbi.count[0] = 16;
		vfmt.fmt.vbi.start[1] = 318;
		vfmt.fmt.vbi.count[1] = 16;

		ASSERT("request vbi format", ioctl(fd, VIDIOC_S_FMT, &vfmt) == 0);

		// Let's see what we got

		if (vfmt.fmt.vbi.sampling_rate < 14000000
		    || vfmt.fmt.vbi.start[0] > 6
		    || vfmt.fmt.vbi.start[1] > 318
		    || vfmt.fmt.vbi.start[0] + vfmt.fmt.vbi.count[0] < 22
		    || vfmt.fmt.vbi.start[1] + vfmt.fmt.vbi.count[1] < 334) {
			FAIL("Cannot capture Teletext with %s ('%s')\n", vbi_dev, vcap.name);
			// XXX check strong field order (for VPS, top field only)
			// XXX check hor. offset and line width
			// XXX relax requirements, may still work (VPS, certain TTX)
		}

		vbi_para.interlaced = !!(vfmt.fmt.vbi.flags & V4L2_VBI_INTERLACED); // we're smart

    		if (vfmt.fmt.vbi.sample_format != V4L2_VBI_SF_UBYTE) {
			FAIL("Unknown vbi sampling format %d\n", vfmt.fmt.vbi.sample_format);
		}

		vbi_para.start[0] 		= vfmt.fmt.vbi.start[0];
		vbi_para.count[0] 		= vfmt.fmt.vbi.count[0];
		vbi_para.start[1] 		= vfmt.fmt.vbi.start[1];
		vbi_para.count[1] 		= vfmt.fmt.vbi.count[1];
		vbi_para.samples_per_line 	= vfmt.fmt.vbi.samples_per_line;
		vbi_para.sampling_rate		= vfmt.fmt.vbi.sampling_rate;

		buffer_size = (vbi_para.count[0] + vbi_para.count[1])
			      * vbi_para.samples_per_line;

		if (vcap.flags & V4L2_FLAG_STREAMING) {
			int str_type = V4L2_BUF_TYPE_VBI;

			printv(2, "Using V4L2 VBI streaming interface\n");

			vrbuf.type = V4L2_BUF_TYPE_VBI;
			vrbuf.count = 5;

			ASSERT("request capture buffers", ioctl(fd, VIDIOC_REQBUFS, &vrbuf) == 0);

			if (vrbuf.count == 0)
				FAIL("No vbi capture buffers granted");

			printv(2, "%d vbi capture buffers granted\n", vrbuf.count);

			ASSERT("init vbi capture fifo", init_callback_fifo(vbi_cap_fifo = &cap_fifo,
				wait_full_stream, send_empty_stream, NULL, NULL, 0, vrbuf.count));

			cap_fifo.start = capture_on;

			// Map capture buffers

			cap_fifo.num_buffers = 0;

			while (cap_fifo.num_buffers < vrbuf.count) {
				unsigned char *p;

				vbuf.type = V4L2_BUF_TYPE_VBI;
				vbuf.index = cap_fifo.num_buffers;

				printv(3, "Mapping vbi capture buffer #%d\n", cap_fifo.num_buffers);

				ASSERT("query vbi capture buffer #%d", ioctl(fd, VIDIOC_QUERYBUF, &vbuf) == 0,
					cap_fifo.num_buffers);

				p = mmap(NULL, vbuf.length, PROT_READ, MAP_SHARED, fd, vbuf.offset);

				if ((int) p == -1) {
					if (errno == ENOMEM && cap_fifo.num_buffers > 0)
						break;
				    	else
						ASSERT("map capture buffer #%d", 0, cap_fifo.num_buffers);
				} else
					cap_fifo.buffers[cap_fifo.num_buffers].data = p;

				ASSERT("enqueue vbi capture buffer #%d",
					ioctl(fd, VIDIOC_QBUF, &vbuf) == 0, vbuf.index);

				cap_fifo.num_buffers++;
			}
		} else {
			printv(2, "Using V4L2 VBI read interface\n");

			ASSERT("init vbi capture fifo", init_callback_fifo(vbi_cap_fifo = &cap_fifo,
				wait_full_read, send_empty_read, NULL, NULL, buffer_size, 1));
		}
	} else
#endif
	{
		// Obscure other device

		printv(2, "Trying BTTV VBI interface\n");

		vbi_para.start[0] 		= 6;
		vbi_para.count[0] 		= 16;
		vbi_para.start[1] 		= 318;
		vbi_para.count[1] 		= 16;
		vbi_para.samples_per_line 	= 2048;
		vbi_para.sampling_rate		= 35468950;
		vbi_para.interlaced		= FALSE;

		buffer_size = 32 * 2048;

		ASSERT("init vbi capture fifo", init_callback_fifo(vbi_cap_fifo = &cap_fifo,
			wait_full_read, send_empty_read, NULL, NULL, buffer_size, 1));
	}
}
