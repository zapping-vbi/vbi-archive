/*
 *  MPEG-1 Real Time Encoder
 *
 *  Copyright (C) 2001 Michael H. Schimek
 *
 *  Based on code by Justin Schoeman,
 *  modified by Iñaki G. Etxebarria,
 *  and more code from ffmpeg by Gerard Lantau.
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

/* $Id: v4l.c,v 1.13 2001-11-03 23:43:54 mschimek Exp $ */

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

static int			fd;
static fifo			cap_fifo;
static producer			cap_prod;

static struct video_audio	old_vaud;
static pthread_t		thread_id;

static int			use_mmap = 0;
static int			gb_frame = 0;
static struct video_mmap	gb_buf;
static struct video_mbuf	gb_buffers;
static unsigned char *		video_buf;

static double			cap_time;
static double			frame_period_near;
static double			frame_period_far;

#define IOCTL(fd, cmd, data) (TEMP_FAILURE_RETRY(ioctl(fd, cmd, data)))
#define CLEAR(var) (memset((var), 0, sizeof(*(var))))

/*
 * a) Assume no driver provides more than two buffers.
 * b) Assume we can't enqueue buffers out of order.
 * c) Assume frames drop unless we dequeue once every 33-40 ms.
 *
 * What a moron's moronic moronity. See v4l2.c for a slick interface.
 */

static inline void
timestamp(buffer *b)
{
	double now = current_time();

	if (cap_time > 0) {
		double dt = now - cap_time;
		double ddt = frame_period_far - dt;

		if (fabs(frame_period_near)
		    < frame_period_far * 1.5) {
			frame_period_near = (frame_period_near - dt) * 0.8 + dt;
			frame_period_far = ddt * 0.9999 + dt;
			b->time = cap_time += frame_period_far;
		} else {
			frame_period_near = frame_period_far;
			b->time = cap_time = now;
		}
	} else {
		b->time = cap_time = now;
	}
}

static void *
v4l_cap_thread(void *unused)
{
	buffer *b;

	for (;;) {
		/* Just in case wait_empty_buffer never waits */
		pthread_testcancel();

		b = wait_empty_buffer(&cap_prod);

		if (use_mmap) {
			gb_buf.frame = gb_frame;

		    	ASSERT("VIDIOCMCAPTURE",
				IOCTL(fd, VIDIOCMCAPTURE, &gb_buf) >= 0);

			gb_frame = (gb_frame+1) % gb_buffers.frames;

			if (IOCTL(fd, VIDIOCSYNC, &gb_frame) < 0)
				ASSERT("VIDIOCSYNC", errno == EAGAIN);

			timestamp(b);

			/* NB: typ. 3-20 MB/s, a horrible waste of cycles. */
			if (filter_mode != CM_YVU)
				memcpy(b->data, video_buf
				       + gb_buffers.offsets[gb_frame],
				       b->size);
			else {
				unsigned char *p=video_buf +
					gb_buffers.offsets[gb_frame];
				int bytes = gb_buf.width * gb_buf.height;
				memcpy(b->data, p, bytes);
				memcpy(b->data+bytes, p+(int)(bytes*1.25),
				       bytes>>2);
				memcpy(b->data+(int)(bytes*1.25), p+bytes, bytes>>2);
			}

			b->used = b->size;
		} else {
			unsigned char *p = b->data;
			ssize_t r, n = b->size;

			while (n > 0) {
				r = read(fd, p, n);

				if (r < 0) {
					if (errno == EINTR)
						continue;

					if (errno == EAGAIN) {
						usleep(5000);
						continue;
					}

					ASSERT("read video data", 0);
				}

				p += r;
				n -= r;
			}

			timestamp(b);

			b->used = b->size;
		}

		send_full_buffer(&cap_prod, b);
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

#define DECIMATING(mode) (mode == CM_YUYV_VERTICAL_DECIMATION ||	\
			  mode == CM_YUYV_EXP_VERTICAL_DECIMATION)

fifo *
v4l_init(double *frame_rate)
{
	struct video_capability vcap;
	int min_cap_buffers = video_look_ahead(gop_sequence);
	int aligned_width, aligned_height;
	unsigned long buf_size;
	int buf_count;

	/* FIXME */
	if (filter_mode == CM_YUYV_VERTICAL_DECIMATION)
		filter_mode = CM_YUYV;

	grab_width = width = saturate(width, 1, MAX_WIDTH);
	grab_height = height = saturate(height, 1, MAX_HEIGHT);

	aligned_width  = (grab_width + 15) & -16;
	aligned_height = (grab_height + 15) & -16;

	buf_count = MAX(cap_buffers, min_cap_buffers);

	ASSERT("open capture device",
		(fd = open(cap_dev, O_RDWR)) >= 0);

	ASSERT("query video capture capabilities of %s (no v4l device?)",
		IOCTL(fd, VIDIOCGCAP, &vcap) >= 0, cap_dev);

	if (!(vcap.type & VID_TYPE_CAPTURE))
		FAIL("%s ('%s') is not a video capture device",
			cap_dev, vcap.name);

	/* Unmute audio (bttv) */

	CLEAR(&old_vaud);

	if (IOCTL(fd, VIDIOCGAUDIO, &old_vaud) >= 0) {
		struct video_audio vaud;

		memcpy(&vaud, &old_vaud, sizeof(vaud));

		vaud.flags &= ~VIDEO_AUDIO_MUTE;
		/* vaud.volume = 60000; */

		ASSERT("enable sound of %s",
			IOCTL(fd, VIDIOCSAUDIO, &vaud) >= 0,
			cap_dev);

		atexit(restore_audio);
	}

	printv(2, "Opened %s ('%s')\n", cap_dev, vcap.name);

	/* Determine current video standard */

	{
		struct video_tuner vtuner;
		struct video_channel vchan;

		CLEAR(&vtuner);
		vtuner.tuner = 0; /* first tuner */

		if (IOCTL(fd, VIDIOCGTUNER, &vtuner) == -1) {
			printv(2, "Apparently the device has no tuner\n");

			CLEAR(&vchan);
			vchan.channel = 0; /* first channel */

			ASSERT("query current video input of %s (VIDIOCGCHAN), "
			       "cannot determine video standard (VIDIOCGTUNER didn't work either)\n",
			       IOCTL(fd, VIDIOCGCHAN, &vchan) == 0,
			       cap_dev);

			vtuner.mode = vchan.norm;
		}

		switch (vtuner.mode) {
		case VIDEO_MODE_PAL:
		case VIDEO_MODE_SECAM:
			printv(2, "Video standard is PAL/SECAM\n");
//			vseg.frame_rate_code = 3;
			cap_time = 0;
			frame_period_near =
			frame_period_far = 1 / 25.0;
			break;

		case VIDEO_MODE_NTSC:
			printv(2, "Video standard is NTSC\n");
//			vseg.frame_rate_code = 4;
			cap_time = 0;
			frame_period_near =
			frame_period_far = 1001 / 30000.0;

			if (grab_height == 288) /* that's the default, assuming PAL */
				height = aligned_height = grab_height = 240;
			if (grab_height == 576)
				height = aligned_height = grab_height = 480;

			break;

		default:
			FAIL("Current video standard #%d unknown.\n", vtuner.mode);
			break;
		}

		*frame_rate = 1.0 / frame_period_far;
	}

	if (IOCTL(fd, VIDIOCGMBUF, &gb_buffers) == -1) {
		struct video_window	win;
		struct video_picture	pict;

		FAIL("V4L read interface does not work, sorry.\n"
		    "Please send patches to zapping-misc@lists.sf.net.\n");

		printv(2, "VIDICGMBUF failed, using read interface\n");

		use_mmap = 0;

		/* Set capture format and dimensions */

		CLEAR(&win);
		win.width = aligned_width;
		win.height = aligned_height;

		if (DECIMATING(filter_mode))
			win.height *= 2;

		win.chromakey = -1;

		ASSERT("set the grab size of %s to %dx%d, "
		       "suggest other -s, -G, -F values",
			!IOCTL(fd, VIDIOCSWIN, &win),
			cap_dev, win.width, win.height);

		CLEAR(&pict);

		ASSERT("determine the current image format of %s (VIDIOCGPICT)",
		       IOCTL(fd, VIDIOCGPICT, &pict) == 0, cap_dev);

		if (filter_mode == CM_YUV)
			pict.palette = VIDEO_PALETTE_YUV420P;
		else
			pict.palette = VIDEO_PALETTE_YUV422;

		if (IOCTL(fd, VIDIOCSPICT, &pict) != 0) {
			printv(2, "Image format %d not accepted.\n",
				pict.palette);

			if (filter_mode == CM_YUV) {
				filter_mode = CM_YUYV;
				pict.palette = VIDEO_PALETTE_YUV422;
			} else {
				if (DECIMATING(filter_mode)) {
					CLEAR(&win);
					win.width = aligned_width;
					win.height = aligned_height;
					win.chromakey = -1;

					ASSERT("set the grab size of %s to %dx%d, "
					       "suggest other -s, -G, -F values",
						IOCTL(fd, VIDIOCSWIN, &win) == 0,
						cap_dev, win.width, win.height);
				}

				filter_mode = CM_YUV;
				pict.palette = VIDEO_PALETTE_YUV420P;
			}

			ASSERT("set image format of %s, "
			       "probably none of YUV 4:2:0 or 4:2:2 are supported",
		    		IOCTL(fd, VIDIOCSPICT, &pict) == 0, cap_dev);
		}

		if (filter_mode == CM_YUV || filter_mode == CM_YVU) {
	    		filter_init(win.width); /* line stride in bytes */
			buf_size = win.width * win.height * 3 / 2;
		} else {
	    		filter_init(win.width * 2);
			buf_size = win.width * win.height * 2;
		}
	} else {
		int r;

		printv(2, "Using mmap interface, %d capture buffers granted.\n",
			gb_buffers.frames);

		use_mmap = 1;

		if (gb_buffers.frames < 2)
			FAIL("Expected 2+ buffers from %s, got %d",
				cap_dev, gb_buffers.frames);

		printv(2, "Mapping capture buffers\n");

		video_buf = mmap(NULL, gb_buffers.size, PROT_READ,
				 MAP_SHARED, fd, 0);

		ASSERT("map capture buffers from %s",
			video_buf != (void *) -1, cap_dev);

		gb_frame = 0;

		printv(2, "Grab 1st frame and set capture format and dimensions.\n");
		/* @:-} IMHO */

		CLEAR(&gb_buf);
		gb_buf.frame = (gb_frame+1) % gb_buffers.frames;
		gb_buf.width = aligned_width;
		gb_buf.height = aligned_height;

		if (DECIMATING(filter_mode))
			gb_buf.height *= 2;

		if (filter_mode == CM_YUV || filter_mode == CM_YVU)
			gb_buf.format = VIDEO_PALETTE_YUV420P;
		else
			gb_buf.format = VIDEO_PALETTE_YUV422;

    		r = IOCTL(fd, VIDIOCMCAPTURE, &gb_buf);

		if (r != 0 && errno != EAGAIN) {
			printv(2, "Image format %d not accepted.\n",
				gb_buf.format);

			if (filter_mode == CM_YUV) {
				filter_mode = CM_YUYV;
				gb_buf.format = VIDEO_PALETTE_YUV422;
			} else {
				filter_mode = CM_YUV;
				gb_buf.width = aligned_width;
				gb_buf.height = aligned_height;
				gb_buf.format = VIDEO_PALETTE_YUV420P;
			}

			r = IOCTL(fd, VIDIOCMCAPTURE, &gb_buf);
		}

    		if (r != 0 && errno == EAGAIN)
			FAIL("%s does not receive a video signal.\n", cap_dev);

		ASSERT("start capturing (VIDIOCMCAPTURE) from %s, maybe the device doesn't\n"
		       "support YUV 4:2:0 or 4:2:2, or the grab size %dx%d is not suitable.\n"
		       "Different -s, -G, -F values may help.",
		       r >= 0,
		       cap_dev, gb_buf.width, gb_buf.height);

		if (filter_mode == CM_YUV || filter_mode == CM_YVU) {
	    		filter_init(gb_buf.width); /* line stride in bytes */
			buf_size = gb_buf.width * gb_buf.height * 3 / 2;
		} else {
	    		filter_init(gb_buf.width * 2);
			buf_size = gb_buf.width * gb_buf.height * 2;
		}
        }

	ASSERT("initialize v4l fifo", init_buffered_fifo(
		&cap_fifo, "video-v4l2",
		buf_count, buf_size));

	printv(2, "Allocated %d bounce buffers.\n", buf_count);

	ASSERT("init v4l capture producer",
		add_producer(&cap_fifo, &cap_prod));

	ASSERT("create v4l capture thread",
		!pthread_create(&thread_id, NULL,
		v4l_cap_thread, NULL));

	printv(2, "V4L capture thread launched\n");

	return &cap_fifo;
}
