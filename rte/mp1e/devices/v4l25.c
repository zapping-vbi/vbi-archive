/*
 *  MPEG-1 Real Time Encoder
 *
 *  Copyright (C) 1999, 2000, 2001, 2002 Michael H. Schimek
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

/* $Id: v4l25.c,v 1.7 2005-09-11 23:07:05 mschimek Exp $ */

#include "site_def.h"

#include <ctype.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <asm/types.h>
#include "../common/videodev.h"
#include "../common/videodev25.h"
#include "../common/_videodev25.h"
#include "../common/device.h"
#include "../common/log.h"
#include "../common/fifo.h"
#include "../common/math.h"
#include "../options.h"
#include "../b_mp1e.h"
#include "../video/video.h"

#define _ioctl(info, cmd, arg)						\
(IOCTL_ARG_TYPE_CHECK_ ## cmd (arg),					\
 device_ioctl (log_fp, fprint_ioctl_arg, fd, cmd, (void *)(arg)))

/* changed to _IOWR */
#define OLD_VIDIOC_S_CTRL _IOW ('V', 28, struct v4l2_control) 

static int			fd;
static FILE *			log_fp;
static fifo			cap_fifo;
static producer			cap_prod;
static buffer *		        buffers;

static struct v4l2_capability	vcap;
static struct v4l2_format	vfmt;
static struct v4l2_buffer	vbuf;
static struct v4l2_requestbuffers vrbuf;

static struct v4l2_control	old_mute;

static struct tfmem		tfmem;

extern int			test_mode;

static char *
le4cc2str(int n)
{
	static char buf[4 * 4 + 1];
	int i, c;

	buf[0] = '\0';

	for (i = 0; i < 4; i++) {
		c = (n >> (8 * i)) & 0xFF;
		sprintf(buf + strlen(buf), isprint(c) ? "%c" : "\\%o", c);
	}

	return buf;
}

static rte_bool
capture_on(fifo *unused)
{
 	int str_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return (0 == _ioctl (fd, VIDIOC_STREAMON, &str_type));
}

static void
wait_full(fifo *f)
{
	struct v4l2_buffer vbuf;
	struct timeval tv;
	fd_set fds;
	buffer *b;
	int r = -1;

#ifdef V4L2_DROP_TEST
drop:
#endif
	for (r = -1; r <= 0;) {
		FD_ZERO(&fds);
		FD_SET(fd, &fds);

		tv.tv_sec = 2;
		tv.tv_usec = 0;

		r = select(fd + 1, &fds, NULL, NULL, &tv);

		if (r < 0 && errno == EINTR)
			continue;

		if (r == 0)
			FAIL("Video capture timeout");

		ASSERT("execute select", r > 0);
	}

	memset (&vbuf, 0, sizeof (vbuf));
	vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	vbuf.memory = V4L2_MEMORY_MMAP;

	ASSERT("dequeue capture buffer",
		0 == _ioctl (fd, VIDIOC_DQBUF, &vbuf));

#ifdef V4L2_DROP_TEST
	if ((rand() % 100) > 50) {
		ASSERT("enqueue capture buffer",
		       0 == _ioctl (fd, VIDIOC_QBUF, &vbuf));
		fprintf(stderr, "video drop\n");
		goto drop;
	}
#endif
	b = buffers + vbuf.index;

	b->time = vbuf.timestamp.tv_sec + vbuf.timestamp.tv_usec * (1 / 1e6);

	send_full_buffer(&cap_prod, b);
}

/* Attention buffers are returned out of order */

static void
send_empty(consumer *c, buffer *b)
{
	struct v4l2_buffer vbuf;

	// XXX
	unlink_node(&c->fifo->full, &b->node);

	memset (&vbuf, 0, sizeof (vbuf));
	vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	vbuf.memory = V4L2_MEMORY_MMAP;
	vbuf.index = b - buffers;

	ASSERT("enqueue capture buffer",
		0 == _ioctl (fd, VIDIOC_QBUF, &vbuf));
}

/*
 *  Initialization 
 */

static void
mute_restore(void)
{
	if (old_mute.id)
		if (-1 == _ioctl (fd, VIDIOC_S_CTRL, &old_mute)
		    && EINVAL == errno)
			ioctl (fd, OLD_VIDIOC_S_CTRL, &old_mute);
}

#define YUV420(mode) (mode == CM_YUV || mode == CM_YUV_VERTICAL_DECIMATION)
#define DECIMATING_HOR(mode) (mode == CM_YUYV_HORIZONTAL_DECIMATION || \
			      mode == CM_YUYV_QUAD_DECIMATION)
#define DECIMATING_VERT(mode) (mode == CM_YUYV_VERTICAL_DECIMATION || \
			       mode == CM_YUV_VERTICAL_DECIMATION || \
			       mode == CM_YUYV_QUAD_DECIMATION \
			  /* removed mode == CM_YUYV_EXP_VERTICAL_DECIMATION */)
#define DECIMATING(mode) (DECIMATING_HOR(mode) || DECIMATING_VERT(mode))
#define PROGRESSIVE(mode) (mode == CM_YUYV_PROGRESSIVE || \
			   mode == CM_YUYV_PROGRESSIVE_TEMPORAL)

fifo *
v4l25_init(rte_video_stream_params *par, struct filter_param *fp)
{
	unsigned int probed_modes = 0;
	int min_cap_buffers = video_look_ahead(gop_sequence);
	int hmod, vmod, i, width1, height1;
	v4l2_std_id std;
	struct v4l2_standard standard;
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;

	if (verbose >= 3)
		log_fp = stderr;
	else
		log_fp = NULL;

	ASSERT("open video capture device",
	       -1 != (fd = device_open (log_fp, cap_dev, O_RDWR, 0)));

	if (-1 == _ioctl (fd, VIDIOC_QUERYCAP, &vcap)) {
		device_close (log_fp, fd);
		return NULL; /* No V4L2.5 device, we'll try V4L2 */
	}

	if (!(vcap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
		FAIL("%s ('%s') is not a capture device",
			cap_dev, vcap.card);

	if (!(vcap.capabilities & V4L2_CAP_STREAMING))
		FAIL("%s ('%s') does not support streaming i/o.",
		     cap_dev, vcap.card);

	printv(2, "Opened V4L2 (new) %s ('%s')\n", cap_dev, vcap.card);

	ASSERT("query current video standard",
	       0 == _ioctl (fd, VIDIOC_G_STD, &std));

	standard.index = 0;

	for (;;) {
		ASSERT("query current video standard",
		       0 == _ioctl (fd, VIDIOC_ENUMSTD, &standard));

		if (standard.id & std)
			break;

		standard.index++;
	}

	par->frame_rate = (double) standard.frameperiod.denominator
		/ standard.frameperiod.numerator;
	mp1e_timestamp_init(&tfmem, 1.0 / par->frame_rate);

	if (par->frame_rate > 29.0 && par->height == 288)
		par->height = 240;
	if (par->frame_rate > 29.0 && par->height == 576)
		par->height = 480;

	if (PROGRESSIVE(filter_mode)) {
		FAIL("Sorry, progressive mode out of order\n");
		min_cap_buffers++;
	}

	printv(2, "Video standard is '%s' (%5.2f Hz)\n",
		standard.name, par->frame_rate);

	if (mute != 2) {
		old_mute.id = V4L2_CID_AUDIO_MUTE;

		if (0 == _ioctl (fd, VIDIOC_G_CTRL, &old_mute)) {
			static const char *mute_options[] = { "unmuted", "muted" };
			struct v4l2_control new_mute;

			atexit(mute_restore);

			new_mute.id = V4L2_CID_AUDIO_MUTE;
			new_mute.value = !!mute;

			if (-1 == _ioctl (fd, VIDIOC_S_CTRL, &new_mute)
			    && EINVAL == errno) {
				ASSERT("set mute control to %d",
				       0 == ioctl (fd, OLD_VIDIOC_S_CTRL,
						    &new_mute),	!!mute);
			}

			printv(2, "Audio %s\n", mute_options[!!mute]);
		} else {
			old_mute.id = 0;

			ASSERT("read mute control", errno == EINVAL /* unsupported */);
		}
	}

	if (1) {
		struct video_audio audio;

		if (0 == ioctl (fd, VIDIOCGAUDIO, &audio)) {
			switch (audio_mode) {
			case 0: /* stereo */
			case 1: /* joint stereo */
			case 2: /* bilingual */
				audio.mode = VIDEO_SOUND_STEREO;
				break;

			case 3: /* mono */
				audio.mode = VIDEO_SOUND_MONO;
				break;
			}

			ASSERT("set audio mode",
			       0 == ioctl (fd, VIDIOCSAUDIO, &audio));
		}
	}

	par->width = saturate(par->width, 1, MAX_WIDTH);
	par->height = saturate(par->height, 1, MAX_HEIGHT);

	width1 = par->width;
	height1 = par->height;

	if (DECIMATING_HOR(filter_mode))
		par->width = (width1 * 2 + 31) & -32;
	else
		par->width  = (width1 + 15) & -16;

	if (DECIMATING_VERT(filter_mode))
		par->height = (height1 * 2 + 31) & -32;
	else
		par->height = (height1 + 15) & -16;

	memset (&cropcap, 0, sizeof (cropcap));
	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (0 == _ioctl (fd, VIDIOC_CROPCAP, &cropcap)) {
		memset (&crop, 0, sizeof (crop));
		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		crop.c = cropcap.defrect;

		if (-1 == _ioctl (fd, VIDIOC_S_CROP, &crop))
			FAIL ("VIDIOC_S_CROP failed\n");
	}

	for (;;) {
		int new_mode, new_width, new_height;

		probed_modes |= 1 << filter_mode;

		vfmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		vfmt.fmt.pix.width = par->width;
		vfmt.fmt.pix.height = par->height;

		if (cropcap.defrect.height > 0) {
			/* If we have cropcap data, make sure we don't
			    overscan. */
			if (vfmt.fmt.pix.height > cropcap.defrect.height)
				vfmt.fmt.pix.height = cropcap.defrect.height;
		}

		if (YUV420(filter_mode)) {
			vfmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;
 		} else {
			vfmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
		}

		if (PROGRESSIVE(filter_mode))
			vfmt.fmt.pix.field = V4L2_FIELD_SEQ_TB;
		else
			vfmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

		if (0 == _ioctl (fd, VIDIOC_S_FMT, &vfmt)) {
			if (!DECIMATING(filter_mode))
				break;
			if (vfmt.fmt.pix.height > par->height * 0.7
			    && vfmt.fmt.pix.width > par->width * 0.7)
				break;
		}

		if (vfmt.fmt.pix.pixelformat == V4L2_PIX_FMT_YUYV) {
			/* Maybe this works. */
			vfmt.fmt.pix.pixelformat = V4L2_PIX_FMT_UYVY;

			if (0 == _ioctl (fd, VIDIOC_S_FMT, &vfmt)) {
				if (!DECIMATING(filter_mode))
					break;
				if (vfmt.fmt.pix.height > par->height * 0.7
				    && vfmt.fmt.pix.width > par->width * 0.7)
					break;
			}
		}

		if (filter_mode == CM_YUYV) {
			new_mode = CM_YUV;
			new_width = par->width;
			new_height = par->height;
		} else if (filter_mode == CM_YUYV_VERTICAL_DECIMATION) {
			new_mode = CM_YUYV_VERTICAL_INTERPOLATION;
			new_width = par->width;
			new_height = (height1 + 15) & -16;
		} else if (filter_mode == CM_YUV_VERTICAL_DECIMATION) {
			new_mode = CM_YUYV_VERTICAL_DECIMATION;
			new_width = par->width;
			new_height = par->height;
		} else if (filter_mode == CM_YUYV_HORIZONTAL_DECIMATION
			   || filter_mode == CM_YUYV_QUAD_DECIMATION) {
			new_mode = CM_YUYV;
			new_width = (width1 + 15) & -16;
			new_height = (height1 + 15) & -16;
		} else {
			new_mode = CM_YUYV;
			new_width = par->width;
			new_height = par->height;
		}

		if (probed_modes & (1 << new_mode)) {
			FAIL("%s ('%s') does not support the requested format at size %d x %d",
				cap_dev, vcap.card, par->width, par->height);
		}

		printv(3, "Format '%s' %d x %d not accepted,\ntrying '%s'\n",
			filter_labels[filter_mode], par->width, par->height,
			filter_labels[new_mode]);

		filter_mode = new_mode;
		par->width = new_width;
		par->height = new_height;
	}

	if (vfmt.fmt.pix.pixelformat == V4L2_PIX_FMT_YUV420) {
		par->pixfmt = RTE_PIXFMT_YUV420;
	} else if (vfmt.fmt.pix.pixelformat == V4L2_PIX_FMT_YUYV) {
		par->pixfmt = RTE_PIXFMT_YUYV;
	} else {
		par->pixfmt = RTE_PIXFMT_UYVY;
	}

	vmod = DECIMATING_VERT(filter_mode) ? 32 : 16;
	hmod = DECIMATING_HOR(filter_mode) ? 32 : 16;

	if (vfmt.fmt.pix.width & (hmod - 1) || vfmt.fmt.pix.height & (vmod - 1)) {
		printv(3, "Format granted %d x %d, attempt to modify\n",
			vfmt.fmt.pix.width, vfmt.fmt.pix.height);

		vfmt.fmt.pix.width	&= -hmod;
		vfmt.fmt.pix.height	&= -vmod;

		if (0 != _ioctl (fd, VIDIOC_S_FMT, &vfmt) ||
		    vfmt.fmt.pix.width & (hmod - 1) || vfmt.fmt.pix.height & (vmod - 1)) {
			FAIL("Please try a different grab size");
		}
	}

	if (vfmt.fmt.pix.width != par->width ||
	    vfmt.fmt.pix.height != par->height) {
		if (verbose > 0) {
			char str[256];

			fprintf(stderr, "'%s' offers a grab size %d x %d, continue? ",
				vcap.card,
				vfmt.fmt.pix.width / (DECIMATING_HOR(filter_mode) ? 2 : 1),
				vfmt.fmt.pix.height / (DECIMATING_VERT(filter_mode) ? 2 : 1));
			fflush(stderr);

			fgets(str, 256, stdin);

			if (tolower(*str) != 'y')
				exit(EXIT_FAILURE);
		} else
			FAIL("Requested grab size not available");
	}

	par->width = vfmt.fmt.pix.width & -hmod;
        par->height = vfmt.fmt.pix.height & -vmod;

	if (width > par->width)
		width = par->width;
	if (height > par->height)
		height = par->height;

	par->sample_aspect = video_sampling_aspect (par->frame_rate,
			  par->width >> !!DECIMATING_HOR (filter_mode),
			  par->height >> !!DECIMATING_VERT (filter_mode));

	if (vfmt.fmt.pix.pixelformat == V4L2_PIX_FMT_UYVY) {
		/* switch to UYVY modes (yeah, an ugly hack) */
		filter_mode += CM_UYVY - CM_YUYV;
	}

	filter_init(par, fp);

	printv(2, "Image format '%s' %d x %d granted\n",
		le4cc2str(vfmt.fmt.pix.pixelformat),
	       vfmt.fmt.pix.width, vfmt.fmt.pix.height);

	/* Phase 2, i/o setup */

	memset (&vrbuf, 0, sizeof (vrbuf));

	vrbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	vrbuf.memory = V4L2_MEMORY_MMAP;
	vrbuf.count = MAX(cap_buffers, min_cap_buffers);

	ASSERT("request capture buffers",
		0 == _ioctl (fd, VIDIOC_REQBUFS, &vrbuf));

	if (vrbuf.count == 0)
		FAIL("No capture buffers granted");

	printv(2, "%d capture buffers granted\n", vrbuf.count);

	ASSERT("allocate capture buffers",
		(buffers = calloc(vrbuf.count, sizeof(buffer))));

	init_callback_fifo(&cap_fifo, "video-v4l2",
			   NULL, NULL, wait_full, send_empty, 0, 0);

	ASSERT("init capture producer",
	       add_producer(&cap_fifo, &cap_prod));

	cap_fifo.start = capture_on;

	// Map capture buffers

	for (i = 0; i < vrbuf.count; i++) {
		unsigned char *p;

		vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		vbuf.memory = V4L2_MEMORY_MMAP;
		vbuf.index = i;

		printv(3, "Mapping capture buffer #%d\n", i);

		ASSERT("query capture buffer #%d",
			0 == _ioctl (fd, VIDIOC_QUERYBUF, &vbuf), i);

		/* bttv 0.8.x wants PROT_WRITE */
		p = mmap(NULL, vbuf.length, PROT_READ | PROT_WRITE,
			 MAP_SHARED, fd, vbuf.m.offset);

		if ((int) p == -1) {
			if (errno == ENOMEM && i > 0)
				break;
			else
				ASSERT("map capture buffer #%d", 0, i);
		} else {
			add_buffer(&cap_fifo, buffers + i);

			buffers[i].data = p;
			buffers[i].used = vbuf.length;
		}

		ASSERT("enqueue capture buffer #%d",
			0 == _ioctl (fd, VIDIOC_QBUF, &vbuf),
			vbuf.index);
	}

	if (i < min_cap_buffers)
		FAIL("Cannot allocate enough (%d) capture buffers", min_cap_buffers);

	return &cap_fifo;
}
