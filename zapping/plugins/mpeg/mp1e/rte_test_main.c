/*
 *  Test program for RTE (real time encoder)
 *
 *  Copyright (C) 2000 Iñaki García Etxebarria
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
/*
 * This is a simple RTE test.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <asm/types.h>
#include <linux/soundcard.h>
#include "common/log.h"
#include "videodev2.h"
#include "rte.h"

static int			fd; /* fd:video, fd2:audio */
static void*			buffers[16];

static struct v4l2_capability	vcap;
static struct v4l2_standard	vstd;
static struct v4l2_format	vfmt;
static struct v4l2_buffer	vbuf;
static struct v4l2_requestbuffers vrbuf;

static struct v4l2_control	old_mute;

static void
read_video(void * data, double * time, rte_context * context)
{
	struct timeval tv;
	fd_set fds;
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
			FAIL("Video capture timeout");

		ASSERT("execute select", r > 0);
	}

	vbuf.type = V4L2_BUF_TYPE_CAPTURE;

	ASSERT("dequeue capture buffer", ioctl(fd, VIDIOC_DQBUF, &vbuf) == 0);

	memcpy(data, buffers[vbuf.index], context->video_bytes);

	*time = vbuf.timestamp / 1e9; // UST, currently TOD

	ASSERT("enqueue capture buffer", ioctl(fd, VIDIOC_QBUF, &vbuf) == 0);
}

static void
mute_restore(void)
{
	if (old_mute.id)
		ioctl(fd, VIDIOC_S_CTRL, &old_mute);
}

static enum rte_frame_rate
init_video(const char * cap_dev, int width, int height)
{
	int str_type = V4L2_BUF_TYPE_CAPTURE;
	int num_buffers;
	enum rte_frame_rate rate_code=RTE_RATE_NORATE;

	ASSERT("open video capture device", (fd = open(cap_dev, O_RDONLY)) != -1);
	ASSERT("query video capture capabilities", ioctl(fd, VIDIOC_QUERYCAP, &vcap) == 0);

	if (vcap.type != V4L2_TYPE_CAPTURE)
		FAIL("%s ('%s') is not a capture device",
			cap_dev, vcap.name);

	if (!(vcap.flags & V4L2_FLAG_STREAMING) ||
	    !(vcap.flags & V4L2_FLAG_SELECT))
		FAIL("%s ('%s') does not support streaming/select(2),\n"
			"%s will not work with the v4l2 read(2) interface.",
			cap_dev, vcap.name, my_name);

	ASSERT("query current video standard", ioctl(fd, VIDIOC_G_STD, &vstd) == 0);

	rate_code = ((double) vstd.framerate.denominator /
		     vstd.framerate.numerator < 29.0) ?
		RTE_RATE_3 : RTE_RATE_4;

	printv(2, "Video standard is '%s'\n", vstd.name);

	vfmt.type = V4L2_BUF_TYPE_CAPTURE;
	vfmt.fmt.pix.width = width;
	vfmt.fmt.pix.height = height;

	vfmt.fmt.pix.depth = 12;
	vfmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;
	vfmt.fmt.pix.flags = V4L2_FMT_FLAG_INTERLACED;

	ASSERT("set capture mode to YUV420", ioctl(fd, VIDIOC_S_FMT,
						   &vfmt) == 0);

	if ((vfmt.fmt.pix.width != width) || (vfmt.fmt.pix.height !=
					      height))
		FAIL("width and height not valid");

	vrbuf.type = V4L2_BUF_TYPE_CAPTURE;
	vrbuf.count = 8;

	ASSERT("request capture buffers", ioctl(fd, VIDIOC_REQBUFS, &vrbuf) == 0);

	if (vrbuf.count == 0)
		FAIL("No capture buffers granted");

	printv(2, "%d capture buffers granted\n", vrbuf.count);

	// Map capture buffers
	num_buffers = 0;

	while (num_buffers < vrbuf.count) {
		unsigned char *p;

		vbuf.type = V4L2_BUF_TYPE_CAPTURE;
		vbuf.index = num_buffers;

		printv(3, "Mapping capture buffer #%d\n", num_buffers);

		ASSERT("query capture buffer #%d", ioctl(fd, VIDIOC_QUERYBUF, &vbuf) == 0,
			num_buffers);

		p = mmap(NULL, vbuf.length, PROT_READ, MAP_SHARED, fd, vbuf.offset);

		buffers[num_buffers] = p;

		ASSERT("enqueue capture buffer #%d",
		       ioctl(fd, VIDIOC_QBUF, &vbuf) == 0, vbuf.index);

		num_buffers++;
	}

	str_type = 1;
	ASSERT("activate capturing",
	       ioctl(fd, VIDIOC_STREAMON, &str_type) == 0);

	old_mute.id = V4L2_CID_AUDIO_MUTE;

	if (ioctl(fd, VIDIOC_G_CTRL, &old_mute) == 0) {
		struct v4l2_control new_mute;

		atexit(mute_restore);

		new_mute.id = V4L2_CID_AUDIO_MUTE;
		new_mute.value = 0;

		ASSERT("set mute control to %d",
		       ioctl(fd, VIDIOC_S_CTRL, &new_mute) == 0, 0);
	}

	return rate_code;
}

#if 0
static void
init_audio(const char * pcm_dev, int format, int speed, int stereo)
{
	ASSERT("open PCM device %s", (fd2 = open(pcm_dev, O_RDONLY))
	       != -1, pcm_dev);

	printv(2, "Opened PCM device %s\n", pcm_dev);

	ASSERT("set PCM AFMT_S16_LE", ioctl(fd2, SNDCTL_DSP_SETFMT,
					    &format) != -1);
	ASSERT("set PCM %d channels", ioctl(fd2, SNDCTL_DSP_STEREO,
					    &stereo) != -1, stereo + 1);
	ASSERT("set PCM sampling rate %d Hz",
	       ioctl(fd2, SNDCTL_DSP_SPEED, &speed) != -1, speed);

	fprintf(stderr, "audio doesn't work yet!\n");
}

/*
 * fixme: Audio is harder to get right. Give a look to the routines in
 * audio/oss.c
 */
static void
read_audio(void * data, double * time, rte_context * context)
{
	struct timeval tv;
	int n = context->audio_bytes;
	int r;
	void * p = data;
	double frame_time;

	while (n>0) {
		r = read(fd2, p, n);

		if (r<0 && errno == EINTR)
			continue;

		(char*)p += r;
		n -= r;
	}

	gettimeofday(&tv, NULL);

	*time = tv.tv_sec + tv.tv_usec/1e6;

	frame_time = context->audio_bytes/(double)context->audio_rate;
	if (context->audio_mode == RTE_AUDIO_MODE_STEREO)
		frame_time = frame_time/2;

	*time -= frame_time;;
}
#endif

static void
data_callback(void * data, double * time, int video, rte_context *
	      context, void * user_data)
{
	struct timeval tv;

	if (user_data != (void*)0xdeadbeef)
		fprintf(stderr, "check failed: %p\n", user_data);

	if (video)
		read_video(data, time, context);
	else
//		read_audio(data, time, context);
	{
		gettimeofday(&tv, NULL);
		*time = tv.tv_sec + tv.tv_usec/1e6;		
	}
}

int main(int argc, char *argv[])
{
	rte_context * context;
	enum rte_frame_rate rate_code;
	int width = 32, height = 32;
	int sleep_time = 5;
//	int audio_format=AFMT_S16_LE, audio_rate=44100, stereo=0;

	if (!rte_init()) {
		fprintf(stderr, "RTE couldn't be inited\n");
		return 0;
	}

	srand(time(NULL));

	width *= (rand()%15)+5;
	height *= (rand()%15)+5;

	width = 640; height = 480;

	fprintf(stderr, "%dx%d\n", width, height);

	rate_code = init_video("/dev/video", width, height);
//	init_audio("/dev/dsp", audio_format, audio_rate, stereo);

	context = rte_context_new(width, height, RTE_YUV420,
				  rate_code, "temp.mpeg", NULL,
				  data_callback, (void*)0xdeadbeef);

	if (!context) {
		fprintf(stderr, "the context cannot be created\n");
		return 0;
	}

//	rte_set_audio_parameters(context, audio_rate, stereo ?
//				 RTE_AUDIO_MODE_STEREO :
//				 RTE_AUDIO_MODE_MONO,
//				 context->output_audio_bits);

	rte_set_mode(context, RTE_MUX_VIDEO_ONLY);

	fprintf(stderr, "starting encode\n");
	if (!rte_init_context(context) ||
	    (!rte_start_encoding(context))) {
		fprintf(stderr, "cannot start encoding: %s\n",
			context->error);
		rte_context_destroy(context);
		return 0;
	}
	fprintf(stderr, "going to bed (%d secs)\n", sleep_time);
	// Encode video for 5 secs
	sleep(sleep_time);

	fprintf(stderr, "done encoding\n");

	// stop encoding
	rte_context_destroy(context);

	fprintf(stderr, "exiting\n");

	return 0;
}
