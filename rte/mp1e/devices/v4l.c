/*
 *  MPEG-1 Real Time Encoder
 *
 *  Copyright (C) 2001 Michael H. Schimek
 *
 *  Based on code by Justin Schoeman,
 *  modified by Iñaki G. Etxebarria,
 *  and more code from ffmpeg by Fabrice Bellard.
 *  Read fixes and AIW hack by Nikolai Zhubr.
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

/* $Id: v4l.c,v 1.6 2002-11-11 21:31:24 zhubr Exp $ */

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
#include "../video/video.h"

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

extern int			test_mode;

#define IOCTL(fd, cmd, data) (TEMP_FAILURE_RETRY(ioctl(fd, cmd, data)))
#define CLEAR(var) (memset((var), 0, sizeof(*(var))))

static inline double
timestamp2(buffer *b)
{
	static double cap_time = 0.0;
	static double dt_acc;
	double now = current_time();

	if (cap_time > 0) {
if (test_mode & 256) {
		double dt = now - cap_time;

		dt_acc += (dt - dt_acc) * 0.1;

		if (dt_acc > tfmem.ref * 1.5) {
			/* bah. */
#if 0
			printv(0, "v4l dropped %f > %f * 1.5\n",
			       dt_acc, tfmem.ref);
#endif
			cap_time = now;
			dt_acc = tfmem.ref;
		} else {
			cap_time += mp1e_timestamp_filter
				(&tfmem, dt, 0.001, 1e-7, 0.1);
		}
#if 0
		printv(0, "now %f dt %+f dta %+f err %+f t/b %+f\n",
		       now, dt, dt_acc, tfmem.err, tfmem.ref);
#endif
} else {
		cap_time += tfmem.ref;
}
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
		/* Apparently we don't reach a cancellation point in shutdown */
		pthread_testcancel();

		b = wait_empty_buffer(&cap_prod);

		if (use_mmap) {
			int num_frame2 = (gb_frame+1) % gb_buffers.frames;

			/* mw: rationally EAGAIN should be returned, instead we get EINVAL, grrr */
			if (IOCTL(fd, VIDIOCSYNC, &gb_frame) < 0)
				ASSERT("VIDIOCSYNC", errno == EAGAIN || errno == EINVAL);
			if (fix_interlaced) {
				if (IOCTL(fd, VIDIOCSYNC, &num_frame2) < 0)
					ASSERT("VIDIOCSYNC", errno == EAGAIN || errno == EINVAL);
			}

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
			
			gb_buf.frame = gb_frame;
		    	ASSERT("VIDIOCMCAPTURE",
				IOCTL(fd, VIDIOCMCAPTURE, &gb_buf) >= 0);
			gb_frame = (gb_frame+1) % gb_buffers.frames;
			if (fix_interlaced) {
				gb_buf.frame = (gb_buf.frame+1) % gb_buffers.frames;
		    		ASSERT("VIDIOCMCAPTURE",
					IOCTL(fd, VIDIOCMCAPTURE, &gb_buf) >= 0);
				gb_frame = (gb_frame+1) % gb_buffers.frames;
			}
		} else {
			unsigned char *p = b->data;
			ssize_t r, n = b->size;

			while (n > 0) {
				r = read(fd, p, n);

				if (r < 0) {
					if (errno == EINTR)
						continue;

					if (errno == EAGAIN) {
						/* We're non-blocking and no data is available,
						   let's take a nap before we try again. */
						usleep(5000);
						continue;
					}

					ASSERT("read video data", 0);
				}

 				p += r;
 				n -= r;
 			}
			
			if (fix_interlaced) {
				n = b->size;
				p = b->data;
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

#define YUV420(mode) (mode == CM_YUV || mode == CM_YVU || \
		      mode == CM_YUV_VERTICAL_DECIMATION)
#define DECIMATING_HOR(mode) (mode == CM_YUYV_HORIZONTAL_DECIMATION || \
			      mode == CM_YUYV_QUAD_DECIMATION)
#define DECIMATING_VERT(mode) (mode == CM_YUYV_VERTICAL_DECIMATION || \
			       mode == CM_YUV_VERTICAL_DECIMATION || \
			       mode == CM_YUYV_QUAD_DECIMATION \
			  /* removed mode == CM_YUYV_EXP_VERTICAL_DECIMATION */)
#define DECIMATING(mode) (DECIMATING_HOR(mode) || DECIMATING_VERT(mode))

fifo *
v4l_init(rte_video_stream_params *par, struct filter_param *fp)
{
	struct video_capability vcap;
	struct video_tuner vtuner;
	struct video_channel vchan;
	struct video_window vwin;
	struct video_picture vpict;
	int min_cap_buffers = video_look_ahead(gop_sequence);
	unsigned long buf_size;
	int max_height, width1, height1, filter_mode1;
	int buf_count;
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

		if (IOCTL(fd, VIDIOCGCHAN, &vchan) == 0) {
			vtuner.mode = vchan.norm;
		} else {
			printv(2, "Failed to query current video input of %s (VIDIOCGCHAN),\n"
			       "VIDIOCGTUNER didn't work either to determine the current\n"
			       "video standard. ", cap_dev);
			if (source_fps > 0.0) {
				printv(2, "Will try user specified frame rate %f Hz.\n", source_fps);
			} else {
				printv(2, "Please add -J frame rate on the command line.\n");
				exit(EXIT_FAILURE);
			}

			vtuner.mode = VIDEO_MODE_AUTO;
		}
	}

	switch (vtuner.mode) {
	case VIDEO_MODE_PAL:
	case VIDEO_MODE_SECAM:
		printv(2, "Video standard is PAL/SECAM\n");
		par->frame_rate = 25.0;
		max_height = 576;
		break;

	case VIDEO_MODE_NTSC:
		printv(2, "Video standard is NTSC\n");
		par->frame_rate = 30000 / 1001.0;
		max_height = 480;
		/* The default size without -swxh assumes PAL/SECAM */
		if (par->height == 288)
			par->height = 240;
		if (par->height == 576)
			par->height = 480;
		break;

	default:
		printv(2, "Current video standard #%d unknown, "
		       "will assume PAL/SECAM.\n", vtuner.mode);
		par->frame_rate = 25.0;
		max_height = 576;
 		break;
 	}
	
	if (source_fps > 0.0) 
		par->frame_rate = source_fps;
	printv(2, "Source frame rate is %f Hz.\n", par->frame_rate);
	mp1e_timestamp_init(&tfmem, 1.0 / par->frame_rate);

	par->width = saturate(par->width, 1, MAX_WIDTH);
	par->height = saturate(par->height, 1, MAX_HEIGHT);

	width1 = par->width;
	height1 = par->height;
	filter_mode1 = filter_mode;

	if (DECIMATING_HOR(filter_mode))
		par->width = width1 * 2;
	else
		par->width  = width1;

	if (DECIMATING_VERT(filter_mode))
		par->height = (height1 * 2 + 31) & -32;
	else
		par->height = (height1 + 15) & -16;

	buf_count = MAX(cap_buffers, min_cap_buffers);

	while (par->height > max_height) {
		if (filter_mode == CM_YUV_VERTICAL_DECIMATION) {
			filter_mode = CM_YUYV_VERTICAL_DECIMATION;
			par->height = par->height;
		} else if (filter_mode == CM_YUYV_VERTICAL_DECIMATION) {
			filter_mode = CM_YUYV_VERTICAL_INTERPOLATION;
			par->height = (height1 + 15) & -16;
		} else if (filter_mode == CM_YUYV_QUAD_DECIMATION) {
			filter_mode = CM_YUYV_VERTICAL_INTERPOLATION;
			par->width = (width1 + 15) & -16;
			par->height = (height1 + 15) & -16;
		} else {
			par->height = max_height;
		}
	}

	/* Capture setup */

	if (IOCTL(fd, VIDIOCGMBUF, &gb_buffers) == -1) {
//		FAIL("V4L read interface does not work, sorry.\n"
//		     "Please send patches to zapping-misc@lists.sf.net.\n");

		printv(2, "VIDICGMBUF failed, using read interface\n");

		use_mmap = 0;

#if 1 /* works? Not with bttv 0.7. */

		/* Set capture format and dimensions */

		CLEAR(&vpict);

		ASSERT("determine the current image format of %s (VIDIOCGPICT)",
		       IOCTL(fd, VIDIOCGPICT, &vpict) == 0, cap_dev);

		if (YUV420(filter_mode))
			vpict.palette = VIDEO_PALETTE_YUV420P;
		else
			vpict.palette = VIDEO_PALETTE_YUYV;

		retry = 0;

		while (IOCTL(fd, VIDIOCSPICT, &vpict) != 0) {
			printv(2, "Image format %d not accepted.\n", vpict.palette);

			if (vpict.palette == VIDEO_PALETTE_YUYV) {
				vpict.palette = VIDEO_PALETTE_YUV422;
				continue;
			}

			if (retry++)
				FAIL("Cannot set image format of %s, "
				     "probably none of YUV 4:2:0 or 4:2:2 (YUYV) are supported",
				     cap_dev);

			if (YUV420(filter_mode)) {
				filter_mode = CM_YUYV;
				vpict.palette = VIDEO_PALETTE_YUYV;
			} else {
				filter_mode = CM_YUV;
				vpict.palette = VIDEO_PALETTE_YUV420P;
				par->width = width1;
				par->height = height1;
			}
		}

		if (YUV420(filter_mode) && fix_interlaced)
			FAIL("-K mode requires YUYV, sorry.\n");
		
		filter_mode1 = filter_mode;

		for (;;) {
			int auto_size = 0;
			CLEAR(&vwin);
			vwin.width = par->width;
			vwin.height = par->height;
			vwin.chromakey = -1;

			if (IOCTL(fd, VIDIOCSWIN, &vwin) == 0)
				break;

			if ((fix_interlaced)||(!DECIMATING(filter_mode))) {
				struct video_window vwin2;
				CLEAR(&vwin2);
				if ((auto_size==0)&&(IOCTL(fd, VIDIOCGWIN, &vwin2) == 0)) {
					printv(1, "WARNING: the driver refused to grab %dx%d, trying %dx%d instead.\n", vwin.width, vwin.height, vwin2.width, vwin2.height);
					auto_size = 1;
					filter_mode = filter_mode1;
					par->height = vwin2.height;
					par->width = vwin2.width;
					continue;
				}
				FAIL("Failed to set the grab size of %s to %dx%d, "
				       "suggest other -s, -G, -F values",
				       cap_dev, width1, height1);
			}

			/*
			 *  When we're decimating a smaller image
			 *  size without decimation may work. 
			 */

			if (YUV420(filter_mode))
				filter_mode = CM_YUV;
			else
				filter_mode = CM_YUYV_VERTICAL_INTERPOLATION;
			par->width = width1;
			par->height = height1;
		}

		par->width = vwin.width;
		par->height = vwin.height;
#endif
	} else {
		int r;

		printv(2, "Using mmap interface, %d capture buffers granted.\n",
			gb_buffers.frames);

		use_mmap = 1;

		if (gb_buffers.frames < 2)
			FAIL("Expected 2+ buffers from %s, got %d",
				cap_dev, gb_buffers.frames);

		printv(2, "Mapping capture buffers\n");

		/* bttv 0.8.x wants PROT_WRITE */
		video_buf = mmap(NULL, gb_buffers.size, PROT_READ | PROT_WRITE,
				 MAP_SHARED, fd, 0);

		ASSERT("map capture buffers from %s",
			video_buf != (void *) -1, cap_dev);

		gb_frame = 0;

		printv(2, "Grab 1st frame and set capture format and dimensions.\n");
		/* @:-} IMHO */

		retry = 0;
		r = 1;

while ((retry <= 2)&&(r!=0)) {  // XXX Kept old indenting for now to make readable diff here.

		CLEAR(&gb_buf);
		gb_buf.frame = 0;
		gb_buf.width = par->width;
		gb_buf.height = par->height;

		if (YUV420(filter_mode))
			gb_buf.format = VIDEO_PALETTE_YUV420P;
		else
			gb_buf.format = VIDEO_PALETTE_YUYV;

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
				par->height = (height1 + 15) & -16;

				gb_buf.width = par->width;
				gb_buf.height = par->height;

				gb_buf.format = VIDEO_PALETTE_YUV420P;
			}
		}

    		if (r != 0 && errno == EAGAIN)
			FAIL("%s does not receive a video signal.\n", cap_dev);

		if (r < 0) {
			struct video_window vwin2;
			CLEAR(&vwin2);
			if (IOCTL(fd, VIDIOCGWIN, &vwin2) == 0) {
					printv(1, "WARNING: the driver refused to grab %dx%d, trying %dx%d instead.\n", gb_buf.width, gb_buf.height, vwin2.width, vwin2.height);
					par->height = vwin2.height;
					par->width = vwin2.width;
					filter_mode = filter_mode1;
					CLEAR(&vpict);
					if (IOCTL(fd, VIDIOCGPICT, &vpict) == 0) {
						switch (vpict.palette) {
						case VIDEO_PALETTE_YUV420P:
							filter_mode = CM_YUV;
							break;
						case VIDEO_PALETTE_YUYV:
						case VIDEO_PALETTE_YUV422:
							filter_mode = CM_YUYV;
							break;
						}
					}
					continue;
			}	
			FAIL("Failed to start capturing (VIDIOCMCAPTURE) from %s, maybe the device doesn't\n"
			       "support YUV 4:2:0 or 4:2:2 (YUYV), or the grab size %dx%d is not suitable.\n"
			       "Different -s, -G, -F values may help.",
			       cap_dev, gb_buf.width, gb_buf.height);
		}
}
		/* In case there are many buffers make full use of them */
		for (gb_buf.frame=1; gb_buf.frame < gb_buffers.frames; gb_buf.frame++)
			ASSERT("queue remaining free buffers (VIDIOCMCAPTURE) of %s\n",
				IOCTL(fd, VIDIOCMCAPTURE, &gb_buf)==0,
				cap_dev);

		par->width = gb_buf.width;
		par->height = gb_buf.height;
	}

	if (width > par->width)
		width = par->width;
	if (height > par->height)
		height = par->height;

	if (YUV420(filter_mode) && fix_interlaced)
		FAIL("-K mode requires YUYV, sorry.\n");

	if (fix_interlaced)
		par->sample_aspect = video_sampling_aspect (par->frame_rate,
			par->width >> 1, par->height >> 1);
	else
		par->sample_aspect = video_sampling_aspect (par->frame_rate,
			par->width >> !!DECIMATING_HOR (filter_mode),
			par->height >> !!DECIMATING_VERT (filter_mode));

	if (YUV420(filter_mode)) {
		par->stride = par->width;
    		filter_init(par, fp);
		buf_size = par->width * par->height * 3 / 2;
	} else {
		par->stride = par->width * 2;
    		filter_init(par, fp);
		buf_size = par->width * par->height * 2;
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
