/*
 *  Test program for RTE (real time encoder)
 *
 *  Copyright (C) 2000-2001 Iñaki García Etxebarria
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
 * $Id: rte_test_main.c,v 1.20 2001-04-07 14:48:36 garetxe Exp $
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
#include <pthread.h>
#include "common/log.h"
#include "videodev2.h"
#include "rte.h"

#ifndef USE_ESD
#include <linux/soundcard.h>
#else
#include <esd.h>
#endif

#define TEST_VIDEO_FORMAT RTE_YUV420 /* or RTE_YUYV, etc... */

#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))
#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

static int			fd; /* fd:video, fd2:audio */
static void*			buffers[16];

static struct v4l2_capability	vcap;
static struct v4l2_standard	vstd;
static struct v4l2_format	vfmt;
static struct v4l2_buffer	vbuf;
static struct v4l2_requestbuffers vrbuf;

static struct v4l2_control	old_mute;

static void
read_video(rte_buffer * buffer)
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

	buffer->data = buffers[vbuf.index];
	buffer->user_data = (void*)vbuf.index; /* we need this for
						  queuing on unref */
	buffer->time = vbuf.timestamp / 1e9; // UST, currently TOD
}

static void
unref_callback(rte_context * context, rte_buffer * buffer)
{
	vbuf.type = V4L2_BUF_TYPE_CAPTURE;
	vbuf.index = (int)buffer->user_data;

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

	if (TEST_VIDEO_FORMAT == RTE_YUV420) {
		vfmt.fmt.pix.depth = 12;
		vfmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;
	} else if (TEST_VIDEO_FORMAT == RTE_YUYV) {
		vfmt.fmt.pix.depth = 16;
		vfmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
	} else
		FAIL("Invalid TEST_VIDEO_FORMAT\n");

	vfmt.fmt.pix.flags = V4L2_FMT_FLAG_INTERLACED;

	ASSERT("set capture format", ioctl(fd, VIDIOC_S_FMT,
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

	old_mute.id = V4L2_CID_AUDIO_MUTE;

	if (ioctl(fd, VIDIOC_G_CTRL, &old_mute) == 0) {
		struct v4l2_control new_mute;

		atexit(mute_restore);

		new_mute.id = V4L2_CID_AUDIO_MUTE;
		new_mute.value = 0;

		ASSERT("set mute control to %d",
		       ioctl(fd, VIDIOC_S_CTRL, &new_mute) == 0, 0);
	}

	str_type = 1;
	ASSERT("activate capturing",
	       ioctl(fd, VIDIOC_STREAMON, &str_type) == 0);

	return rate_code;
}

/*
 *  PCM Device, OSS API
 */
/*
 * rte doesn't require buffering at all, but it is more efficient this way.
 */
#ifndef USE_ESD
/*
 * Reduce the buffer size if you loose some frames in the beginning.
 */
#define BUFFER_SIZE 1024*8 // bytes per read(), appx.
static int		fd2;
#else /* use esd */
#define BUFFER_SIZE (ESD_BUF_SIZE)
static int		esd_recording_socket;
#endif
static short *		abuffer;
static int		scan_range;
static int		look_ahead;
static int		buffer_size;
static int		samples_per_frame;

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
}
#else /* use esd */
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
		ssize_t r;
		int n;

		memcpy(abuffer, abuffer + scan_range, look_ahead *
		       sizeof(abuffer[0]));

		p = abuffer + look_ahead;
		n = scan_range * sizeof(abuffer[0]);

		while (n > 0) {
			fd_set rdset;
			int err;

			FD_ZERO(&rdset);
			FD_SET(esd_recording_socket, &rdset);
			tv.tv_sec = 1;
			tv.tv_usec = 0;
			err = select(esd_recording_socket+1, &rdset,
				   NULL, NULL, &tv);

			if ((err == -1) || (err == 0))
				continue;

			r = read(esd_recording_socket, p, n);
			
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

		rtime = tv.tv_sec + tv.tv_usec / 1e6;
		rtime -= (scan_range - n) / (double) sampling_rate;

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

	if (esd_recording_socket <= 0)
		FAIL("couldn't create esd recording socket");

	fprintf(stderr, "Using ESD interface: BUFFER_SIZE = %d\n",
		BUFFER_SIZE);

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

	fprintf(stderr, "Using OSS interface: BUFFER_SIZE = %d\n",
		BUFFER_SIZE);
#endif

	samples_per_frame = 1152 << stereo;

	scan_range = MAX(BUFFER_SIZE / sizeof(short) /
			 samples_per_frame, 1) * samples_per_frame;

	look_ahead = (512 - 32) << stereo;

	buffer_size = (scan_range + look_ahead)	* sizeof(abuffer[0]);
}

static void
buffer_callback(rte_context * context, rte_buffer * buffer,
		enum rte_mux_mode stream)
{
	if (stream & RTE_VIDEO) {
		read_video(buffer);
	}
	else {
		fprintf(stderr, "stream type not supported: %d\n",
			stream);
		exit(1);
	}
}

static void
data_callback(rte_context * context, void * data, double * time,
	      enum rte_mux_mode stream, void * user_data)
{
	if (stream & RTE_AUDIO) {
		read_audio(data, time, context);
	}
	else {
		fprintf(stderr, "stream type not supported: %d\n",
			stream);
		exit(1);
	}
}

/* Set to 1 to shut down audio thread */
static volatile int thread_exit_signal=0;

/* This thread pushes audio into the rte context */
void * audio_thread(void * p)
{
	rte_context * context = (rte_context*) p;
	double timestamp;

	p = rte_push_audio_data(context, NULL, 0);

	while (!thread_exit_signal) {
		if (p)
			read_audio(p, &timestamp, context);
		p = rte_push_audio_data(context, p, timestamp);
	}

	return NULL;
}

int main(int argc, char *argv[])
{
	rte_context * context;
	enum rte_frame_rate rate_code;
	int width = 16, height = 16;
	int sleep_time = 5;
	int audio_rate=44100, stereo=0;
	char * video_device = "/dev/video0";
	char * audio_device = "/dev/audio0";
	char dest_file[] = "tempx.mpeg";
	pthread_t audio_thread_id;
	enum rte_mux_mode mux_mode = RTE_AUDIO | RTE_VIDEO;
	enum rte_interface video_interface = RTE_PUSH;
	enum rte_interface audio_interface = RTE_PUSH;
	int num_encoded_frames;
	void * dest_ptr = NULL;
	int i=0;

	if (!rte_init()) {
		fprintf(stderr, "RTE couldn't be inited\n");
		return 0;
	}

	srand(time(NULL));

	width *= (rand()%20)+10;
	height *= (rand()%20)+10;
	
	width = 352; height = 288;

	fprintf(stderr, "%d x %d\n", width, height);

	rate_code = init_video(video_device, &width, &height);
	init_audio(audio_device, audio_rate, stereo);

	/* create the context we will be using */
	context = rte_context_new(width, height, TEST_VIDEO_FORMAT,
				  (void*)0xdeadbeef);

	rte_set_verbosity(context, 2);

	if (!context) {
		fprintf(stderr, "the context cannot be created\n");
		return 0;
	}

	/* set whether we will be encoding audio and/or video */
	rte_set_mode(context, mux_mode);

	if (mux_mode & RTE_AUDIO) {
		rte_set_audio_parameters(context, audio_rate, stereo ?
					 RTE_AUDIO_MODE_STEREO :
					 RTE_AUDIO_MODE_MONO,
					 context->output_audio_bits);

		abuffer = malloc(buffer_size);
		memset(abuffer, 0, buffer_size);
	}

	/*
	 * Set the input and output methods.
	 */
	/* context, mux_mode, interface, buffered, data_callback,
	   buffer_callback, unref_callback */
	if (audio_interface == RTE_CALLBACKS)
		rte_set_input(context, RTE_AUDIO, RTE_CALLBACKS,
			      FALSE, data_callback, NULL, NULL);
	else
		rte_set_input(context, RTE_AUDIO, RTE_PUSH, FALSE,
			      NULL, NULL, NULL);
	if (video_interface == RTE_CALLBACKS)
		rte_set_input(context, RTE_VIDEO, RTE_CALLBACKS, TRUE,
			      NULL, buffer_callback, unref_callback);
	else
		rte_set_input(context, RTE_VIDEO, RTE_PUSH, FALSE,
			      NULL, NULL, NULL);

	/* do a multi-capture test */
	for (i=0; i<4; i++) {
		dest_file[4] = i + '0';
		fprintf (stderr, "encoding %s\n", dest_file);
		dest_ptr = NULL;

		/* context, encode_callback, filename */
		rte_set_output(context, NULL, dest_file);
		
		fprintf(stderr, "preparing context for encoding\n");
		
		/* Prepare the context for encoding */
		if (!rte_init_context(context)) {
			fprintf(stderr, "cannot init the context: %s\n",
				context->error);
			rte_context_destroy(context);
			return 0;
		}
		
		thread_exit_signal = 0;
		if ((mux_mode & RTE_AUDIO) &&
		    (audio_interface == RTE_PUSH))
			pthread_create(&audio_thread_id, NULL,
				       audio_thread, context);

		if ((mux_mode & RTE_AUDIO) && (mux_mode & RTE_VIDEO))
			fprintf(stderr, "syncing streams\n");

		/* do the sync'ing and start encoding */
		if (!rte_start_encoding(context)) {
			fprintf(stderr, "cannot start encoding: %s\n",
				context->error);
			rte_context_destroy(context);
			return 0;
		}

		fprintf(stderr, "going to bed (%d secs)\n", sleep_time);

		/* let rte encode video for some time */
		if (mux_mode & RTE_VIDEO) {
			if (video_interface == RTE_CALLBACKS)
				sleep(sleep_time);
			else for (num_encoded_frames = 0;
				  num_encoded_frames < (sleep_time*25);
				  num_encoded_frames++) {
				rte_buffer buf;
				if (!dest_ptr) {
					dest_ptr =
						rte_push_video_data(context,
								    NULL,
								    0);
					continue;
				}
				read_video(&buf);
				memcpy(dest_ptr, buf.data,
				       context->video_bytes); 
				dest_ptr =
					rte_push_video_data(context, dest_ptr,
							    buf.time);
				unref_callback(context, &buf);
			}
		} else /* audio only*/
			sleep(sleep_time);
		
		/* Stop pushing before stopping the context */
		thread_exit_signal = 1;
		if ((mux_mode & RTE_AUDIO) &&
		    (audio_interface == RTE_PUSH))
			pthread_join(audio_thread_id, NULL);
		
		fprintf(stderr, "done encoding\n");
		
		rte_stop(context);
	}

	fprintf(stderr, "End of testing, goodbye.\n");
	rte_context_destroy(context);

#ifdef USE_ESD
	if (mux_mode & RTE_AUDIO)
		close(esd_recording_socket);
#endif

	return 0;
}
