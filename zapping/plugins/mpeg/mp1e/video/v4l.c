/*
 *  MPEG-1 Real Time Encoder
 *
 *  Copyright (C) 2001 Michael H. Schimek
 *
 *  Based on code by Justin Schoeman,
 *  modified by Iñaki G. Etxebarria
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

/* $Id: v4l.c,v 1.17 2001-08-01 13:01:37 mschimek Exp $ */

#include <ctype.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <asm/types.h>
#include "../common/videodev.h"
#include "../common/log.h"
#include "../common/fifo.h"
#include "../common/alloc.h"
#include "../common/math.h"
#include "../options.h"
#include "video.h"

#warning The V4L interface has not been tested.

static int			fd;
static fifo2			cap_fifo;
static producer			cap_prod;

static struct video_capability	vcap;
static struct video_channel	chan;
static struct video_mbuf	buf;
static struct video_mmap	vmmap;
static struct video_audio	old_vaud;
static unsigned long		buf_base;

static pthread_t		thread_id;

#define IOCTL(fd, cmd, data) (TEMP_FAILURE_RETRY(ioctl(fd, cmd, data)))

/*
 * a) Assume no driver provides more than two buffers.
 * b) Assume we can't enqueue buffers out of order.
 * c) Assume frames drop unless we dequeue once every 33-40 ms.
 *
 * What a moron's moronic moronity. See v4l2.c for a slick interface.
 */

static void *
v4l_cap_thread(void *unused)
{
	buffer2 *b;
	int cframe;
	int r;

	for (cframe = 0; cframe < buf.frames; cframe++) {
		vmmap.frame = cframe;
		ASSERT("queue v4l capture buffer #%d",
			ioctl(fd, VIDIOCMCAPTURE, &vmmap) == 0,
			cframe);
	}

	cframe = 0;

	for (;;) {
		/* Just in case wait_empty never waits */
		pthread_testcancel();

		b = wait_empty_buffer2(&cap_prod);

		r = IOCTL(fd, VIDIOCSYNC, &cframe);

		b->time = current_time();

		ASSERT("ioctl VIDIOCSYNC", r >= 0);

		memcpy(b->data, (unsigned char *)(buf_base
			+ buf.offsets[cframe]), b->used);
		b->used = b->size;

		vmmap.frame = cframe;

		ASSERT("queue v4l capture buffer #%d",
			IOCTL(fd, VIDIOCMCAPTURE, &vmmap) == 0,
			cframe);

		send_full_buffer2(&cap_prod, b);

		if (++cframe >= buf.frames)
			cframe = 0;
	}

	return NULL;
}

static void
restore_audio(void)
{
	pthread_cancel(thread_id);
	pthread_join(thread_id, NULL);

	IOCTL(fd, VIDIOCSAUDIO, &old_vaud);
}

fifo2 *
v4l_init(void)
{
	int min_cap_buffers = video_look_ahead(gop_sequence);
	int aligned_width;
	int aligned_height;
	unsigned long buf_size;
	int buf_count;

	grab_width = width = saturate(width, 1, MAX_WIDTH);
	grab_height = height = saturate(height, 1, MAX_HEIGHT);

	aligned_width  = (width + 15) & -16;
	aligned_height = (height + 15) & -16;

	buf_count = MAX(cap_buffers, min_cap_buffers);

	ASSERT("open capture device", (fd = open(cap_dev, O_RDONLY)) != -1);

	ASSERT("query video capture capabilities of %s (no v4l device?)",
		IOCTL(fd, VIDIOCGCAP, &vcap) == 0, cap_dev);

	if (!(vcap.type & VID_TYPE_CAPTURE))
		FAIL("%s ('%s') is not a video capture device",
			cap_dev, vcap.name);

	if (IOCTL(fd, VIDIOCGAUDIO, &old_vaud) == 0) {
		struct video_audio vaud;

		memcpy(&vaud, &old_vaud, sizeof(vaud));

		vaud.flags &= ~VIDEO_AUDIO_MUTE;
		vaud.volume = 60000;

		ASSERT("enable sound of %s",
			IOCTL(fd, VIDIOCSAUDIO, &vaud) == 0,
			cap_dev);

		atexit(restore_audio);
	}

	printv(2, "Opened %s ('%s')\n", cap_dev, vcap.name);

	ASSERT("query video channel", IOCTL(fd, VIDIOCGCHAN, &chan) == 0);

	if (chan.norm == 0) /* PAL */
		frame_rate_code = 3;
	else /* NTSC */
		frame_rate_code = 4;

	printv(2, "Video standard is '%s'\n",
		chan.norm == 0 ? "PAL" : "NTSC");

	if (frame_rate_code == 4 && grab_height == 288)
		height = aligned_height = grab_height = 240;
	if (frame_rate_code == 4 && grab_height == 576)
		height = aligned_height = grab_height = 480;

	vmmap.width	= aligned_width;
	vmmap.height	= aligned_height;

	if (filter_mode == CM_YUV) {
		vmmap.format = VIDEO_PALETTE_YUV420P;
		buf_size = vmmap.width * vmmap.height * 3 / 2;
	} else {
		vmmap.format = VIDEO_PALETTE_YUV422;
		buf_size = vmmap.width * vmmap.height * 2;
	}

	filter_init(vmmap.width);

	ASSERT("request capture buffers", IOCTL(fd, VIDIOCGMBUF, &buf) == 0);

	if (buf.frames == 0)
		FAIL("No capture buffers granted");

	printv(2, "%d capture buffers granted\n", buf.frames);

	printv(3, "Mapping capture buffers.\n");

	buf_base = (unsigned long) mmap(NULL, buf.size, PROT_READ,
			MAP_SHARED, fd, 0);

	ASSERT("map capture buffers", buf_base != -1);

	ASSERT("initialize v4l fifo", init_buffered_fifo2(
		&cap_fifo, "video-v4l2",
		buf_count, buf_size));

	printv(2, "Allocated %d bounce buffers.\n", buf_count);

	ASSERT("init v4l capture producer",
		add_producer(&cap_fifo, &cap_prod));

	ASSERT("create v4l capture thread",
		!pthread_create(&thread_id, NULL,
			v4l_cap_thread, NULL));

	printv(2, "V4L capture thread launched (you should really use V4L2...)\n");

	return &cap_fifo;
}
