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

/* $Id: v4l.c,v 1.17 2002-02-08 15:03:11 mschimek Exp $ */

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
static struct tfmem		tfmem;

static int			use_mmap = 0;
static int			gb_frame = 0;
static struct video_mmap	gb_buf;
static struct video_mbuf	gb_buffers;
static unsigned char *		video_buf;

#define IOCTL(fd, cmd, data) (TEMP_FAILURE_RETRY(ioctl(fd, cmd, data)))
#define CLEAR(var) (memset((var), 0, sizeof(*(var))))

static inline double
timestamp2(buffer *b)
{
	static double cap_time = 0.0;
	static double dt_acc;
	double now = current_time();

	if (cap_time > 0) {
		double dt = now - cap_time;

		dt_acc += (dt - dt_acc) * 0.1;

		if (dt_acc > tfmem.ref * 1.5) {
			/* bah. */
#if 1
			printv(0, "v4l dropped %f > %f * 1.5\n",
			       dt_acc, tfmem.ref);
#endif
			cap_time = now;
			dt_acc = tfmem.ref;
		} else {
			cap_time += mp1e_timestamp_filter
				(&tfmem, dt, 0.001, 1e-7, 0.1);
		}
#if 1
		printv(0, "now %f dt %+f dta %+f err %+f t/b %+f\n",
		       now, dt, dt_acc, tfmem.err, tfmem.ref);
#endif
	} else {
		cap_time = now;
		dt_acc = tfmem.ref;
	}

	return cap_time;
}

static void *
v4l_cap_thread(void *unused)
{
	buffer *b;

	for (;;) {
		b = wait_empty_buffer(&cap_prod);

		if (use_mmap) {
			gb_buf.frame = gb_frame;

		    	ASSERT("VIDIOCMCAPTURE",
				IOCTL(fd, VIDIOCMCAPTURE, &gb_buf) >= 0);

			gb_frame = (gb_frame+1) % gb_buffers.frames;

			if (IOCTL(fd, VIDIOCSYNC, &gb_frame) < 0)
				ASSERT("VIDIOCSYNC", errno == EAGAIN);

			b->time = timestamp2(b);

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

			b->time = timestamp2(b);

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
	struct video_tuner vtuner;
	struct video_channel vchan;
	/* struct video_window vwin; */
	/* struct video_picture vpict; */
	int min_cap_buffers = video_look_ahead(gop_sequence);
	int aligned_width, aligned_height;
	unsigned long buf_size;
	int buf_count;
	int max_height;
	int retry;

	ASSERT("open capture device",
		(fd = open(cap_dev, O_RDWR)) >= 0);

	ASSERT("query video capture capabilities of %s (no v4l device?)",
		IOCTL(fd, VIDIOCGCAP, &vcap) >= 0, cap_dev);

	if (!(vcap.type & VID_TYPE_CAPTURE))
		FAIL("%s ('%s') is not a video capture device",
			cap_dev, vcap.name);

	printv(2, "Opened %s ('%s')\n", cap_dev, vcap.name);


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


	/* Determine current video standard */

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
		*frame_rate = 25.0;
		mp1e_timestamp_init(&tfmem, 1 / 25.0);
		max_height = 576;
		break;

	case VIDEO_MODE_NTSC:
		printv(2, "Video standard is NTSC\n");
		*frame_rate = 30000 / 1001.0;
		mp1e_timestamp_init(&tfmem, 1001 / 30000.0);
		max_height = 480;
		if (grab_height == 288) /* that's the default, assuming PAL */
			grab_height = 240;
		if (grab_height == 576)
			grab_height = 480;
		break;

	default:
		FAIL("Current video standard #%d unknown.\n", vtuner.mode);
		break;
	}



	grab_width = saturate(grab_width, 1, MAX_WIDTH);
	grab_height = saturate(grab_height, 1, MAX_HEIGHT);

	if (DECIMATING(filter_mode))
		aligned_height = (grab_height * 2 + 15) & -16;
	else
		aligned_height = (grab_height + 15) & -16;

	aligned_width  = (grab_width + 15) & -16;

	buf_count = MAX(cap_buffers, min_cap_buffers);


	while (aligned_height > max_height) {
		if (DECIMATING(filter_mode)) {
			filter_mode = CM_YUYV_VERTICAL_INTERPOLATION;
			aligned_height = (grab_height + 15) & -16;
		} else {
			aligned_height = max_height;
		}
	}

#if 0 /* works? */

	/* Set capture format and dimensions */

	CLEAR(&pict);

	ASSERT("determine the current image format of %s (VIDIOCGPICT)",
	       IOCTL(fd, VIDIOCGPICT, &pict) == 0, cap_dev);

	if (filter_mode == CM_YUV)
		pict.palette = VIDEO_PALETTE_YUV420P;
	else
		pict.palette = VIDEO_PALETTE_YUYV;

	retry = 0;

	while (IOCTL(fd, VIDIOCSPICT, &pict) != 0) {
		printv(2, "Image format %d not accepted.\n", pict.palette);

		if (pict.palette == VIDEO_PALETTE_YUYV) {
			pict.palette = VIDEO_PALETTE_YUV422;
			continue;
		}

		if (retry++)
			FAIL("Cannot set image format of %s, "
			     "probably none of YUV 4:2:0 or 4:2:2 (YUYV) are supported",
	    		     cap_dev);

		if (filter_mode == CM_YUV) {
			filter_mode = CM_YUYV;
			pict.palette = VIDEO_PALETTE_YUYV;
		} else {
			filter_mode = CM_YUV;
			pict.palette = VIDEO_PALETTE_YUV420P;
			aligned_height = (grab_height + 15) & -16;
		}
	}

	for (;;) {
		CLEAR(&win);
		vwin.width = aligned_width;
		vwin.height = aligned_height;
		vwin.chromakey = -1;

		if (IOCTL(fd, VIDIOCSWIN, &vwin) == 0)
			break;

		ASSERT("set the grab size of %s to %dx%d, "
		       "suggest other -s, -G, -F values",
		       DECIMATING(filter_mode),
		       cap_dev, vwin.width, vwin.height);

		filter_mode = CM_YUYV_VERTICAL_INTERPOLATION;
		aligned_height = (grab_height + 15) & -16;
	}

	aligned_width = vwin.width;
	aligned_height = vwin.height;

#endif

	/* Capture setup */

	if (IOCTL(fd, VIDIOCGMBUF, &gb_buffers) == -1) {
		FAIL("V4L read interface does not work, sorry.\n"
		     "Please send patches to zapping-misc@lists.sf.net.\n");

		printv(2, "VIDICGMBUF failed, using read interface\n");

		use_mmap = 0;
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

		if (filter_mode == CM_YUV || filter_mode == CM_YVU)
			gb_buf.format = VIDEO_PALETTE_YUV420P;
		else
			gb_buf.format = VIDEO_PALETTE_YUYV;

		retry = 0;

		while ((r = IOCTL(fd, VIDIOCMCAPTURE, &gb_buf)) != 0 && errno != EAGAIN) {
			printv(2, "Image filter %d, palette %d not accepted.\n",
				filter_mode, gb_buf.format);

			if (gb_buf.format == VIDEO_PALETTE_YUYV) {
				gb_buf.format = VIDEO_PALETTE_YUV422;
				continue;
			}

			if (retry++)
				break;

			if (filter_mode == CM_YUV) {
				filter_mode = CM_YUYV;
				gb_buf.format = VIDEO_PALETTE_YUYV;
			} else {
				filter_mode = CM_YUV;

				if (DECIMATING(filter_mode))
					aligned_height = (grab_height + 15) & -16;

				gb_buf.width = aligned_width;
				gb_buf.height = aligned_height;

				gb_buf.format = VIDEO_PALETTE_YUV420P;
			}
		}

    		if (r != 0 && errno == EAGAIN)
			FAIL("%s does not receive a video signal.\n", cap_dev);

		ASSERT("start capturing (VIDIOCMCAPTURE) from %s, maybe the device doesn't\n"
		       "support YUV 4:2:0 or 4:2:2 (YUYV), or the grab size %dx%d is not suitable.\n"
		       "Different -s, -G, -F values may help.",
		       r >= 0,
		       cap_dev, gb_buf.width, gb_buf.height);

		grab_width = gb_buf.width;
		grab_height = gb_buf.height;

		if (width > grab_width)
			width = grab_width;
		if (height > grab_height)
			height = grab_height;

		if (filter_mode == CM_YUV || filter_mode == CM_YVU) {
	    		filter_init(gb_buf.width); /* line stride in bytes */
			buf_size = gb_buf.width * gb_buf.height * 3 / 2;
		} else {
	    		filter_init(gb_buf.width * 2);
			buf_size = gb_buf.width * gb_buf.height * 2;
		}
        }

	ASSERT("initialize v4l fifo", init_buffered_fifo(
		&cap_fifo, "video-v4l",
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
