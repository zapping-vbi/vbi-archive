/*
 *  Test program for RTE (real time encoder)
 *
 *  Copyright (C) 2000 I�aki Garc�a Etxebarria
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
#include "common/log.h"
#include "videodev2.h"
#include "rte.h"

#undef USE_ESD /* doesn't work yet */

#ifndef USE_ESD
#include <linux/soundcard.h>
#else
#include <esd.h>
#endif

#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))

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
	old_mute.value = 1;
	if (old_mute.id)
		ioctl(fd, VIDIOC_S_CTRL, &old_mute);
}

static enum rte_frame_rate
init_video(const char * cap_dev, int * width, int * height)
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
	vfmt.fmt.pix.width = *width;
	vfmt.fmt.pix.height = *height;

	vfmt.fmt.pix.depth = 12;
	vfmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;
	vfmt.fmt.pix.flags = V4L2_FMT_FLAG_INTERLACED;

	ASSERT("set capture mode to YUV420", ioctl(fd, VIDIOC_S_FMT,
						   &vfmt) == 0);

	if ((vfmt.fmt.pix.width & 15) || (vfmt.fmt.pix.height & 15))
		FAIL("width and height not valid: %d x %d",
		     vfmt.fmt.pix.width, vfmt.fmt.pix.height);

	*width = vfmt.fmt.pix.width;
	*height = vfmt.fmt.pix.height;

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

/*
 *  PCM Device, OSS API
 */

#define BUFFER_SIZE 32768 // bytes per read(), appx.

/*
 * rte doesn't require buffering at all, but it is more efficient this way.
 */
#ifndef USE_ESD
static int		fd2;
static short *		abuffer;
static int		scan_range;
static int		look_ahead;
static int		buffer_size;
static int		samples_per_frame;
#else /* use esd */
static int		esd_recording_socket;
static short 		abuffer[ESD_BUF_SIZE*2];
#endif

/*
 *  Read window: samples_per_frame (1152 * channels) + look_ahead (480 * channels);
 *  Subband window size 512 samples, step width 32 samples (32 * 3 * 12 total)
 */
/*
 *  If you have a better idea for timestamping go ahead.
 *  [DSP_CAP_TRIGGER, DSP_CAP_MMAP? Not really what I want but maybe
 *   closer to the sampling instant.]
 */
#ifndef USE_ESD
static void
read_audio(void * data, double * time, rte_context * context)
{
	static double rtime, utime;
	static int left = 0;
	static short *p;
	int stereo = (context->audio_mode == RTE_AUDIO_MODE_STEREO) ? 1
		: 0;
	struct timeval tv;
	int sampling_rate = context->audio_rate;

	if (left <= 0)
	{
		struct audio_buf_info info;
		ssize_t r;
		int n;

		memcpy(abuffer, abuffer + scan_range, look_ahead *
		       sizeof(abuffer[0]));

		p = abuffer + look_ahead;
		n = scan_range * sizeof(abuffer[0]);

		while (n > 0) {
			r = read(fd2, p, n);
			
			if (r < 0 && errno == EINTR)
				continue;

			if (r == 0) {
				memset(p, 0, n);
				break;
			}

			ASSERT("read PCM data, %d bytes", r > 0, n);

			(char *) p += r;
			n -= r;
		}

		gettimeofday(&tv, NULL);
		ASSERT("check PCM hw buffer maximum occupancy(tm)",
			ioctl(fd2, SNDCTL_DSP_GETISPACE, &info) != 1);

		rtime = tv.tv_sec + tv.tv_usec / 1e6;
		rtime -= (scan_range - n + info.bytes) / (double)
			sampling_rate;

		left = scan_range - samples_per_frame;
		p = abuffer;

		*time = rtime;
		memcpy(data, p, context->audio_bytes);
		return;
	}

	utime = rtime + ((p - abuffer) >> stereo) / (double) sampling_rate;
	left -= samples_per_frame;

	p += samples_per_frame;

	*time = utime;
	memcpy(data, p, context->audio_bytes);
	return;
}
#else /* use esd */
#warning The ESD interface is still experimental
static void
read_audio(void * data, double * time, rte_context * context)
{
	fd_set rdset;
	struct timeval tv;
	int n, r = context->audio_bytes;
	void * p = data;
	double ltime;
	int stereo = (context->audio_mode == RTE_AUDIO_MODE_STEREO) ? 1
		: 0;
	int sampling_rate = context->audio_rate;

	while (r>0) {
		FD_ZERO(&rdset);
		FD_SET(esd_recording_socket, &rdset);
		
		tv.tv_sec = 1;
		tv.tv_usec = 0;
/*		n = select(esd_recording_socket, &rdset, NULL, NULL, &tv);
		
		gettimeofday(&tv, NULL);
		fprintf(stderr, "selected\n");
		if (n<0)
			continue;
			fprintf(stderr, "reading\n");*/
		n = read(esd_recording_socket, p, r);
//		fprintf(stderr, "n: %d, r: %d\n", n, r);
		r -= n;

		if (r <= 0)
			gettimeofday(&tv, NULL);

		p += n;
	}

	ltime = ((double)context->audio_bytes) / (2 * (double)sampling_rate *
						  (stereo+1));

	ltime += ((double)esd_get_latency(esd_recording_socket))/(44.1e3);

	*time = tv.tv_sec + tv.tv_usec/1e6;

	*time -= ltime;
}
#endif

static void
init_audio(const char * pcm_dev, int speed, int stereo)
{
#ifdef USE_ESD
	esd_format_t format;

	format = ESD_STREAM | ESD_RECORD | ESD_BITS16;

	if (stereo)
		format |= ESD_STEREO;
	else
		format |= ESD_MONO;

	esd_recording_socket =
		esd_record_stream_fallback(format, speed, NULL, NULL);

#else /* don't use ESD */
	int format=AFMT_S16_LE;

	ASSERT("open PCM device %s", (fd2 = open(pcm_dev, O_RDONLY))
	       != -1, pcm_dev);

	printv(2, "Opened PCM device %s\n", pcm_dev);

	ASSERT("set PCM AFMT_S16_LE", ioctl(fd2, SNDCTL_DSP_SETFMT,
					    &format) != -1);
	ASSERT("set PCM %d channels", ioctl(fd2, SNDCTL_DSP_STEREO,
					    &stereo) != -1, stereo + 1);
	ASSERT("set PCM sampling rate %d Hz",
	       ioctl(fd2, SNDCTL_DSP_SPEED, &speed) != -1, speed);

	samples_per_frame = 1152 << stereo;

	scan_range = MAX(BUFFER_SIZE / sizeof(short) /
			 samples_per_frame, 1) * samples_per_frame;

	look_ahead = (512 - 32) << stereo;

	buffer_size = (scan_range + look_ahead)	* sizeof(abuffer[0]);
#endif
}

static void
data_callback(void * data, double * time, int video, rte_context *
	      context, void * user_data)
{
	if (user_data != (void*)0xdeadbeef)
		fprintf(stderr, "check failed: %p\n", user_data);

	if (video)
		read_video(data, time, context);
	else
		read_audio(data, time, context);
}

int main(int argc, char *argv[])
{
	rte_context * context;
	enum rte_frame_rate rate_code;
	int width = 16, height = 16;
	int sleep_time = 10;
	int audio_rate=44100, stereo=0;
	char * video_device = "/dev/video";
	char * audio_device = "/dev/audio";
	char * dest_file = "temp.mpeg";

	if (!rte_init()) {
		fprintf(stderr, "RTE couldn't be inited\n");
		return 0;
	}

	srand(time(NULL));

	width *= (rand()%30)+10;
	height *= (rand()%30)+10;

	width = 320; height = 240;

	fprintf(stderr, "%d x %d\n", width, height);

	rate_code = init_video(video_device, &width, &height);
	init_audio(audio_device, audio_rate, stereo);

	context = rte_context_new(width, height, RTE_YUV420,
				  rate_code, dest_file, NULL,
				  data_callback, (void*)0xdeadbeef);

	if (!context) {
		fprintf(stderr, "the context cannot be created\n");
		return 0;
	}

	rte_set_audio_parameters(context, audio_rate, stereo ?
				 RTE_AUDIO_MODE_STEREO :
				 RTE_AUDIO_MODE_MONO,
				 context->output_audio_bits);

#ifndef USE_ESD
	abuffer = malloc(buffer_size);
	memset(abuffer, 0, buffer_size);
#endif

	fprintf(stderr, "starting encode\n");

	if (!rte_init_context(context) ||
	    (!rte_start_encoding(context))) {
		fprintf(stderr, "cannot start encoding: %s\n",
			context->error);
		rte_context_destroy(context);
		return 0;
	}
	fprintf(stderr, "going to bed (%d secs)\n", sleep_time);
	// let rte encode video for some time
	sleep(sleep_time);

	fprintf(stderr, "done encoding\n");

	// stop encoding
	rte_context_destroy(context);

	fprintf(stderr, "exiting\n");

	return 0;
}
