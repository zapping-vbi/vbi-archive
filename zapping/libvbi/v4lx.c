/*
 *  V4L/V4L2 VBI Interface
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

/* $Id: v4lx.c,v 1.14 2001-05-05 23:45:06 garetxe Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <assert.h>
#include <sys/time.h>		// timeval
#include <sys/types.h>		// fd_set
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <asm/types.h>		// v4l2
#include <linux/videodev.h>

#include "decoder.h"
#include "../common/math.h"
#include "../common/fifo.h"

#define IODIAG(templ, args...) //fprintf(stderr, templ ": %s\n" ,##args , strerror(errno))/* with errno */
#define DIAG(templ, args...) //fprintf(stderr, templ,##args)

#define V4L2_LINE 		0 /* API rev. Nov 2000 (-1 -> 0) */

#define MAX_RAW_BUFFERS 	5

#define BTTV_VBISIZE		_IOR('v' , BASE_VIDIOCPRIVATE+8, int)

#define HAVE_V4L_VBI_FORMAT	0 // Linux 2.4 XXX configure

#define HAVE_V4L2 defined (V4L2_MAJOR_VERSION)

typedef struct {
	fifo			fifo;			/* world interface */
	pthread_t		thread_id;

	struct vbi_decoder	dec;			/* raw vbi decoder context */

	int			fd;
	int			btype;			/* v4l2 stream type */
	int			num_raw_buffers;
	bool			streaming;
	bool			buffered;
	double			time_per_frame;

	struct {
		unsigned char *		data;
		int			size;
	}			raw_buffer[MAX_RAW_BUFFERS];

} vbi_device;

/*
 *  Read Interface
 */

static buffer *
wait_full_read(fifo *f)
{
	vbi_device *vbi = PARENT(f, vbi_device, fifo);
	struct timeval tv;
	buffer *b;
	size_t r;

	b = wait_empty_buffer(f);

	for (;;) {
		// XXX use select if possible to set read timeout
		pthread_testcancel();

		r = read(vbi->fd, vbi->raw_buffer[0].data,
			 vbi->raw_buffer[0].size);

		if (r == vbi->raw_buffer[0].size)
			break;

		if (r == -1
		    && (errno == EINTR || errno == ETIME))
			continue;

		IODIAG("VBI read error");

		add_head(&f->empty, &b->node);

		return NULL;
	}

	gettimeofday(&tv, NULL);

	b->data = b->allocated;
	b->time = tv.tv_sec + tv.tv_usec / 1e6;

	b->used = sizeof(vbi_sliced) *
		vbi_decoder(&vbi->dec, vbi->raw_buffer[0].data,
			    (vbi_sliced *) b->data);

	return b;
}

static void *
read_thread(void *p)
{
	fifo *f = (fifo *) p;
	vbi_device *vbi = PARENT(f, vbi_device, fifo);
	double last_time, stacked_time, glitch_time;
	struct timeval tv;
	list stack;
	int stacked;
	buffer *b;
	size_t r;

	init_list(&stack);
	glitch_time = vbi->time_per_frame * 1.25;
	stacked_time = 0.0;
	last_time = 0.0;
	stacked = 0;

	for (;;) {
		b = wait_empty_buffer(f);

		for (;;) {
			// XXX use select if possible to set read timeout
			pthread_testcancel();

			r = read(vbi->fd, vbi->raw_buffer[0].data,
				 vbi->raw_buffer[0].size);

			if (r == vbi->raw_buffer[0].size)
				break;

			if (r == -1
			    && (errno == EINTR || errno == ETIME))
				continue;

			IODIAG("VBI read error");

			for (; stacked > 0; stacked--)
				send_full_buffer(f, (buffer *) rem_head(&stack));

			assert(!"read error in v4lx read thread"); /* XXX */
		}

		gettimeofday(&tv, NULL);

		b->data = b->allocated;
		b->time = tv.tv_sec + tv.tv_usec / 1e6;

		b->used = sizeof(vbi_sliced) *
			vbi_decoder(&vbi->dec, vbi->raw_buffer[0].data,
				    (vbi_sliced *) b->data);
		/*
		 *  This curious construct compensates temporary shifts
		 *  caused by an unusual delay between read() and
		 *  the execution of gettimeofday(). A complete loss
		 *  remains lost.
		 */
		if (last_time > 0 &&
		    (b->time - (last_time + stacked_time)) > glitch_time) {
			if (stacked >= (f->num_buffers >> 2)) {
				/* Not enough space &| hopeless desynced */
				for (stacked_time = 0.0; stacked > 0; stacked--)
					send_full_buffer(f, (buffer *) rem_head(&stack));
			} else {
				add_tail(&stack, &b->node);
				stacked_time += vbi->time_per_frame;
				stacked++;
				continue;
			}
		} else /* (back) on track */ 
			for (stacked_time = 0.0; stacked > 0; stacked--) {
				buffer *b = (buffer *) rem_head(&stack);

				b->time = last_time += vbi->time_per_frame; 
				send_full_buffer(f, b);
			}

		last_time = b->time;
		send_full_buffer(f, b);
	}

	return NULL;
}

static void
send_empty_read(fifo *f, buffer *b)
{
	add_head(&f->empty, &b->node);
}

static bool
start_read(fifo *f)
{
	vbi_device *vbi = PARENT(f, vbi_device, fifo);
	buffer *b;

	if (vbi->buffered)
		return pthread_create(&vbi->thread_id, NULL,
			read_thread, f) == 0;

	if ((b = wait_full_read(f))) {
		unget_full_buffer(f, b);
		return TRUE; /* access should be finally granted */
	}
	return FALSE;
}

#include <dirent.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

#warning guess_bttv_v4l not reliable

static bool
guess_bttv_v4l(vbi_device *vbi, int *strict, int given_fd)
{
	struct video_capability vcap;
	struct video_tuner vtuner;
	struct video_channel vchan;
	struct video_unit vunit;
	int video_fd = -1;
	int mode = -1;
	struct stat dir_stat;

	memset(&vtuner, 0, sizeof(struct video_tuner));
	memset(&vchan, 0, sizeof(struct video_channel));

	if (ioctl(vbi->fd, VIDIOCGTUNER, &vtuner) != -1)
		mode = vtuner.mode;
	else if (ioctl(vbi->fd, VIDIOCGCHAN, &vchan) != -1)
		mode = vchan.norm;
	else do {
		struct dirent dirent, *pdirent = &dirent;
		struct stat vbi_stat;
		DIR *dir;

		/*
		 *  Bttv vbi has no VIDIOCGUNIT pointing back to
		 *  the associated video device, now it's getting
		 *  dirty. We're dumb enough to walk only /dev,
		 *  first level of, and assume v4l major is still 81.
		 *  Not tested with devfs.
		 */

		if (fstat(vbi->fd, &vbi_stat) == -1)
			break;

		if (!S_ISCHR(vbi_stat.st_mode))
			return FALSE;

		if (major(vbi_stat.st_rdev) != 81)
			return FALSE; /* break? */

		/* We are given a device, try it */
		if (given_fd > 0 &&
		    ioctl(given_fd, VIDIOCGCAP, &vcap) != -1 &&
		    (vcap.type & VID_TYPE_CAPTURE) &&
		    ioctl(given_fd, VIDIOCGUNIT, &vunit) != -1 &&
		    vunit.vbi == minor(vbi_stat.st_rdev)) {
			video_fd = given_fd;
			goto device_found;
		}

		/* Try first /dev/video0, this will speed things up in
		 the common case */
		if (stat("/dev/video0", &dir_stat) == -1
		    || !S_ISCHR(dir_stat.st_mode)
		    || major(dir_stat.st_rdev) != 81
		    || minor(dir_stat.st_rdev) == minor(vbi_stat.st_rdev)
		    || (video_fd = open("/dev/video0",
					O_RDONLY | O_TRUNC)) == -1) {
			goto the_hard_way;
		}
			
		if (ioctl(video_fd, VIDIOCGCAP, &vcap) == -1
		    || !(vcap.type & VID_TYPE_CAPTURE)
		    || ioctl(video_fd, VIDIOCGUNIT, &vunit) == -1
		    || vunit.vbi != minor(vbi_stat.st_rdev)) {
			close(video_fd);
			video_fd = -1;
			goto the_hard_way;
		}

		goto device_found;

	the_hard_way:
		if (!(dir = opendir("/dev")))
			break;

		while (readdir_r(dir, &dirent, &pdirent) == 0 && pdirent) {
			char *s;

			if (!asprintf(&s, "/dev/%s", dirent.d_name))
				continue;
			
			/*
			 *  V4l2 O_NOIO == O_TRUNC,
			 *  shouldn't affect v4l devices.
			 */
			if (stat(s, &dir_stat) == -1
			    || !S_ISCHR(dir_stat.st_mode)
			    || major(dir_stat.st_rdev) != 81
			    || minor(dir_stat.st_rdev) == minor(vbi_stat.st_rdev)
			    || (video_fd = open(s, O_RDONLY | O_TRUNC)) == -1) {
				free(s);
				continue;
			}
			
			if (ioctl(video_fd, VIDIOCGCAP, &vcap) == -1
			    || !(vcap.type & VID_TYPE_CAPTURE)
			    || ioctl(video_fd, VIDIOCGUNIT, &vunit) == -1
			    || vunit.vbi != minor(vbi_stat.st_rdev)) {
				close(video_fd);
				video_fd = -1;
				free(s);
				continue;
			}
			
			free(s);
			break;
		}
		
		closedir(dir);

		if (video_fd == -1)
			break; /* not found in /dev or some other problem */

	device_found:
		if (ioctl(video_fd, VIDIOCGTUNER, &vtuner) != -1)
			mode = vtuner.mode;
		else if (ioctl(video_fd, VIDIOCGCHAN, &vchan) != -1)
			mode = vchan.norm;
		
		if (video_fd != given_fd)
			close(video_fd);
	} while (0);

	switch (mode) {
	case VIDEO_MODE_NTSC:
		vbi->dec.scanning = 525;
		break;

	case VIDEO_MODE_PAL:
	case VIDEO_MODE_SECAM:
		vbi->dec.scanning = 625;
		break;

	default:
		/*
		 *  One last chance, we'll try to guess
		 *  the scanning if GVBIFMT is available.
		 */
		vbi->dec.scanning = 0;
		*strict = TRUE;
		break;
	}

	return TRUE;
}

static int
open_v4l(vbi_device **pvbi, char *dev_name,
	 int fifo_depth, unsigned int services,
	 int strict, int given_fd, int buffered)
{
#if HAVE_V4L_VBI_FORMAT
	struct vbi_format vfmt;
#endif
	struct video_capability vcap;
	vbi_device *vbi;
	int max_rate, buffer_size;

	if (!(vbi = calloc(1, sizeof(vbi_device)))) {
		DIAG("Virtual memory exhausted");
		return 0;
	}

	if ((vbi->fd = open(dev_name, O_RDONLY)) == -1) {
		free(vbi);
		IODIAG("Cannot open %s", dev_name);
		return 0;
	}

	if (ioctl(vbi->fd, VIDIOCGCAP, &vcap) == -1) {
		/*
		 *  Older bttv drivers don't support any
		 *  vbi ioctls, let's see if we can guess the beast.
		 */
		if (!guess_bttv_v4l(vbi, &strict, given_fd)) {
			close(vbi->fd);
			free(vbi);
			return -1; /* Definately not V4L */
		}

		DIAG("Opened %s, ", dev_name);
	} else {
		DIAG("Opened %s, ", dev_name);

		if (!(vcap.type & VID_TYPE_TELETEXT)) {
			DIAG("not a raw VBI device");
			goto failure;
		}

		guess_bttv_v4l(vbi, &strict, given_fd);
	}

	max_rate = 0;

#if HAVE_V4L_VBI_FORMAT

	/* May need a rewrite */
	if (ioctl(vbi->fd, VIDIOCGVBIFMT, &vfmt) == -1) {
		if (!vbi->dec.scanning
		    && vbi->dec.start[1] > 0
		    && vbi->dec.count[1]) {
			if (vbi->dec.start[1] >= 286)
				vbi->dec.scanning = 625;
			else
				vbi->dec.scanning = 525;
		}

		/* Speculative, vbi_format is not documented */
		if (strict >= 0 && vbi->dec.scanning) {
			if (!(services = qualify_vbi_sampling(&vbi->dec, &max_rate, services))) {
				DIAG("device cannot capture requested data services");
				goto failure;
			}

			memset(&vfmt, 0, sizeof(struct vbi_format));

			vfmt.sample_format	= VIDEO_PALETTE_RAW;
			vfmt.sampling_rate	= vbi->dec.sampling_rate;
			vfmt.samples_per_line	= vbi->dec.samples_per_line;
			vfmt.fmt.vbi.start[0]	= vbi->dec.start[0];
			vfmt.fmt.vbi.count[0]	= vbi->dec.count[1];
			vfmt.fmt.vbi.start[1]	= vbi->dec.start[0];
			vfmt.fmt.vbi.count[1]	= vbi->dec.count[1];

			/* Single field allowed? */

			if (!vfmt.count[0]) {
				vfmt.start[0] = (vbi->dec.scanning == 625) ? 6 : 10;
				vfmt.count[0] = 1;
			} else if (!vfmt.count[1]) {
				vfmt.start[1] = (vbi->dec.scanning == 625) ? 318 : 272;
				vfmt.count[1] = 1;
			}

			if (ioctl(vbi->fd, VIDIOCSVBIFMT, &vfmt) == -1) {
				switch (errno) {
				case EBUSY:
					DIAG("device is already in use");
					break;

		    		default:
					IODIAG("VBI parameters rejected");
					break;
				}

				goto failure;
			}

		} /* strict >= 0 */

		if (vfmt.sample_format != VIDEO_PALETTE_RAW) {
			DIAG("unknown VBI sampling format %d, "
			     "please contact the maintainer of "
			     "this program for service", vfmt.sample_format);
			goto failure;
		}

		vbi->dec.sampling_rate		= vfmt.sampling_rate;
		vbi->dec.samples_per_line 	= vfmt.samples_per_line;
		if (vbi->dec.scanning == 625)
			vbi->dec.offset 	= 10.2e-6 * vfmt.sampling_rate;
		else if (vbi->dec.scanning == 525)
			vbi->dec.offset		= 9.2e-6 * vfmt.sampling_rate;
		else /* we don't know */
			vbi->dec.offset		= 9.7e-6 * vfmt.sampling_rate;
		vbi->dec.start[0] 		= vfmt.start[0];
		vbi->dec.count[0] 		= vfmt.count[0];
		vbi->dec.start[1] 		= vfmt.start[1];
		vbi->dec.count[1] 		= vfmt.count[1];
		vbi->dec.interlaced		= !!(vfmt.flags & VBI_INTERLACED);
		vbi->dec.synchronous		= !(vfmt.flags & VBI_UNSYNC);
		vbi->time_per_frame 		= (vbi->dec.scanning == 625) ? 1.0 / 25 : 1001.0 / 30000;

	} else /* VIDIOCGVBIFMT failed */

#endif /* HAVE_V4L_VBI_FORMAT */

	{
		int size;

		/*
		 *  If a more reliable method exists to identify the bttv
		 *  driver I'll be glad to hear about it. Lesson: Don't
		 *  call a v4l private ioctl without knowing who's
		 *  listening. All we know at this point: It's a csf, and
		 *  it may be a v4l device.
		 *  garetxe: This isn't reliable, bttv doesn't return
		 *  anything useful in vcap.name.
		 */
/*
		if (!strstr(vcap.name, "bttv") && !strstr(vcap.name, "BTTV")) {
			DIAG("unable to identify driver, has no standard VBI interface");
			goto failure;
		}
*/
		switch (vbi->dec.scanning) {
		case 625:
			vbi->dec.sampling_rate = 35468950;
			vbi->dec.offset = 10.2e-6 * 35468950;
			vbi->dec.start[0] = -1; // XXX FIX ME for CC-625
			vbi->dec.start[1] = -1;
			break;

		case 525:
			vbi->dec.sampling_rate = 28636363;
			vbi->dec.offset = 9.2e-6 * 28636363;
			vbi->dec.start[0] = 10;	 // confirmed for bttv 0.7.52
			vbi->dec.start[1] = 273;
			break;

		default:
			DIAG("driver clueless about video standard (1)");
			goto failure;
		}

		vbi->dec.samples_per_line 	= 2048;
		vbi->dec.interlaced		= FALSE;
		vbi->dec.synchronous		= TRUE;
		vbi->time_per_frame 		= (vbi->dec.scanning == 625) ? 1.0 / 25 : 1001.0 / 30000;

		if ((size = ioctl(vbi->fd, BTTV_VBISIZE, 0)) == -1) {
			// BSD or older bttv driver.
			vbi->dec.count[0] = 16;
			vbi->dec.count[1] = 16;
		} else if (size % 2048) {
			DIAG("unexpected size of raw VBI buffer (broken driver?)");
			goto failure;
		} else {
			size /= 2048;
			vbi->dec.count[0] = size >> 1;
			vbi->dec.count[1] = size - vbi->dec.count[0];
		}
	}

	if (!services) {
		DIAG("device cannot capture requested data services");
		goto failure;
	}

	if (!vbi->dec.scanning && strict >= 1) {
		if (vbi->dec.start[1] <= 0 || !vbi->dec.count[1]) {
			/*
			 *  We may have requested single field capture
			 *  ourselves, but then we had guessed already.
			 */
			DIAG("driver clueless about video standard (2)");
			goto failure;
		}

		if (vbi->dec.start[1] >= 286) {
			vbi->dec.scanning = 625;
			vbi->time_per_frame = 1.0 / 25;
		} else {
			vbi->dec.scanning = 525;
			vbi->time_per_frame = 1001.0 / 30000;
		}
	}

	/* Nyquist */

	if (vbi->dec.sampling_rate < max_rate * 3 / 2) {
		DIAG("VBI sampling frequency too low");
		goto failure;
	} else if (vbi->dec.sampling_rate < max_rate * 6 / 2) {
		DIAG("VBI sampling frequency too low for me");
		goto failure; /* Need smarter bit slicer */
	}

	if (!add_vbi_services(&vbi->dec, services, strict)) {
		DIAG("device cannot capture requested data services");
		goto failure;
	}

	buffer_size = (vbi->dec.count[0] + vbi->dec.count[1])
		      * vbi->dec.samples_per_line;

	if ((vbi->buffered = buffered)) {
		if (!init_buffered_fifo(&vbi->fifo, "vbi-v4l", fifo_depth,
		    sizeof(vbi_sliced) * (vbi->dec.count[0] + vbi->dec.count[1]))) {
			goto failure;
		}
	} else {
		if (!init_callback_fifo(&vbi->fifo, "vbi-v4l",
		    wait_full_read, send_empty_read, fifo_depth,
		    sizeof(vbi_sliced) * (vbi->dec.count[0] + vbi->dec.count[1]))) {
			goto failure;
		}
	}

	if (!(vbi->raw_buffer[0].data = malloc(buffer_size))) {
		uninit_fifo(&vbi->fifo);
		DIAG("virtual memory exhausted");
		goto failure;
	}

	vbi->raw_buffer[0].size = buffer_size;
	vbi->num_raw_buffers = 1;

	vbi->fifo.start = start_read;

	*pvbi = vbi;

	return 1;

failure:
	close(vbi->fd);
	free(vbi);

	return 0;
}

#if HAVE_V4L2

#ifndef V4L2_BUF_TYPE_VBI /* API rev. Sep 2000 */
#define V4L2_BUF_TYPE_VBI V4L2_BUF_TYPE_CAPTURE;
#endif

/*
 *  Streaming I/O Interface
 */

static buffer *
wait_full_stream(fifo *f)
{
	vbi_device *vbi = PARENT(f, vbi_device, fifo);
	struct v4l2_buffer vbuf;
	struct timeval tv;
	fd_set fds;
	buffer *b;
	int r;

	for (;;) {
		b = wait_empty_buffer(f);
		r = -1;

		while (r <= 0) {
			FD_ZERO(&fds);
			FD_SET(vbi->fd, &fds);

			tv.tv_sec = 2;
			tv.tv_usec = 0;

			pthread_testcancel();
			r = select(vbi->fd + 1, &fds, NULL, NULL, &tv);

			if (r < 0 && errno == EINTR)
				continue;

			if (r == 0) { /* timeout */
				DIAG("VBI capture stalled, no station tuned in?");
				if (vbi->buffered)
					continue; /* XXX */
				return NULL;
			} else if (r < 0) {
				IODIAG("Unknown VBI select failure");
				if (vbi->buffered)
					assert(0); /* XXX */
				return NULL;
			}
		}

		vbuf.type = vbi->btype;

		if (ioctl(vbi->fd, VIDIOC_DQBUF, &vbuf) == -1) {
			IODIAG("Cannot dequeue streaming I/O VBI buffer "
				"(broken driver or application?)");
			if (vbi->buffered)
				assert(0); /* XXX */
			return NULL;
		}

		b->data = b->allocated;
		b->time = vbuf.timestamp / 1e9;

		b->used = sizeof(vbi_sliced) *
			vbi_decoder(&vbi->dec, vbi->raw_buffer[vbuf.index].data,
				    (vbi_sliced *) b->data);

		if (ioctl(vbi->fd, VIDIOC_QBUF, &vbuf) == -1) {
			unget_full_buffer(f, b);
			IODIAG("Cannot enqueue streaming I/O VBI buffer (broken driver?)");
			if (vbi->buffered)
				assert(0); /* XXX */
			return NULL;
		}

		if (vbi->buffered)
			send_full_buffer(f, b);
		else
			return b;
	}

	return NULL;
}

static void
send_empty_stream(fifo *f, buffer *b)
{
	add_head(&f->empty, &b->node);
}

static bool
start_stream(fifo *f)
{
	vbi_device *vbi = PARENT(f, vbi_device, fifo);
	buffer *b;

	if (ioctl(vbi->fd, VIDIOC_STREAMON, &vbi->btype) == -1) {
		IODIAG("Cannot start VBI capturing");
		return FALSE;
	}

	if (vbi->buffered)
		return pthread_create(&vbi->thread_id, NULL,
			(void *(*)(void *)) wait_full_stream, f) == 0;

	/* Subsequent I/O shouldn't fail, let's try anyway */

	if ((b = wait_full_stream(f))) {
		unget_full_buffer(f, b);
		return TRUE;
	}

	return FALSE;
}

static int
open_v4l2(vbi_device **pvbi, char *dev_name,
	int fifo_depth, unsigned int services, int strict, int buffered)
{
	struct v4l2_capability vcap;
	struct v4l2_format vfmt;
	struct v4l2_requestbuffers vrbuf;
	struct v4l2_buffer vbuf;
	struct v4l2_standard vstd;
	vbi_device *vbi;
	int max_rate;

	assert(services != 0);
	assert(fifo_depth > 0);

	if (!(vbi = calloc(1, sizeof(vbi_device)))) {
		DIAG("Virtual memory exhausted\n");
		return 0;
	}

	if ((vbi->fd = open(dev_name, O_RDONLY)) == -1) {
		IODIAG("Cannot open %s", dev_name);
		free(vbi);
		return 0;
	}

	if (ioctl(vbi->fd, VIDIOC_QUERYCAP, &vcap) == -1) {
		close(vbi->fd);
		free(vbi);
		return -1; // not V4L2
	}

	DIAG("Opened %s (%s), ", dev_name, vcap.name);

	if (vcap.type != V4L2_TYPE_VBI) {
		DIAG("not a raw VBI device");
		goto failure;
	}

	if (ioctl(vbi->fd, VIDIOC_G_STD, &vstd) == -1) {
		/* mandatory, http://www.thedirks.org/v4l2/v4l2dsi.htm */
		IODIAG("cannot query current video standard (broken driver?)");
		goto failure;
	}

	vbi->dec.scanning = vstd.framelines;
	/* add_vbi_services() eliminates non 525/625 */

	memset(&vfmt, 0, sizeof(vfmt));

	vfmt.type = vbi->btype = V4L2_BUF_TYPE_VBI;

	max_rate = 0;

	if (ioctl(vbi->fd, VIDIOC_G_FMT, &vfmt) == -1) {
		strict = MAX(0, strict);
		/* IODIAG("cannot query current VBI parameters");
		   goto failure; */
	}

	if (strict >= 0) {
		if (!(services = qualify_vbi_sampling(&vbi->dec, &max_rate, services))) {
			DIAG("device cannot capture requested data services");
			goto failure;
		}

		vfmt.fmt.vbi.sample_format	= V4L2_VBI_SF_UBYTE;
		vfmt.fmt.vbi.sampling_rate	= vbi->dec.sampling_rate;
		vfmt.fmt.vbi.samples_per_line	= vbi->dec.samples_per_line;
		vfmt.fmt.vbi.offset		= vbi->dec.offset;
		vfmt.fmt.vbi.start[0]		= vbi->dec.start[0] + V4L2_LINE;
		vfmt.fmt.vbi.count[0]		= vbi->dec.count[1];
		vfmt.fmt.vbi.start[1]		= vbi->dec.start[0] + V4L2_LINE;
		vfmt.fmt.vbi.count[1]		= vbi->dec.count[1];

		/* API rev. Nov 2000 paranoia */

		if (!vfmt.fmt.vbi.count[0]) {
			vfmt.fmt.vbi.start[0] = ((vbi->dec.scanning == 625) ? 6 : 10) + V4L2_LINE;
			vfmt.fmt.vbi.count[0] = 1;
		} else if (!vfmt.fmt.vbi.count[1]) {
			vfmt.fmt.vbi.start[1] = ((vbi->dec.scanning == 625) ? 318 : 272) + V4L2_LINE;
			vfmt.fmt.vbi.count[1] = 1;
		}

		if (ioctl(vbi->fd, VIDIOC_S_FMT, &vfmt) == -1) {
			switch (errno) {
			case EBUSY:
				DIAG("device is already in use");
				goto failure;

			case EINVAL:
				/* Try bttv2 rev. 021100 */
				vfmt.type = vbi->btype = V4L2_BUF_TYPE_CAPTURE;
				vfmt.fmt.vbi.start[0] = 0;
				vfmt.fmt.vbi.count[0] = 16;
				vfmt.fmt.vbi.start[1] = 313;
				vfmt.fmt.vbi.count[1] = 16;

				if (ioctl(vbi->fd, VIDIOC_S_FMT, &vfmt) == -1) {
			default:
					IODIAG("VBI parameters rejected (broken driver?)");
					goto failure;
				}

				vfmt.fmt.vbi.start[0] = 7 + V4L2_LINE;
				vfmt.fmt.vbi.start[1] = 320 + V4L2_LINE;

				break;
			}
		}
	}

	vbi->dec.sampling_rate		= vfmt.fmt.vbi.sampling_rate;
	vbi->dec.samples_per_line 	= vfmt.fmt.vbi.samples_per_line;
	vbi->dec.offset			= vfmt.fmt.vbi.offset;
	vbi->dec.start[0] 		= vfmt.fmt.vbi.start[0] - V4L2_LINE;
	vbi->dec.count[0] 		= vfmt.fmt.vbi.count[0];
	vbi->dec.start[1] 		= vfmt.fmt.vbi.start[1] - V4L2_LINE;
	vbi->dec.count[1] 		= vfmt.fmt.vbi.count[1];
	vbi->dec.interlaced		= !!(vfmt.fmt.vbi.flags & V4L2_VBI_INTERLACED);
	vbi->dec.synchronous		= !(vfmt.fmt.vbi.flags & V4L2_VBI_UNSYNC);
	vbi->time_per_frame 		= (vbi->dec.scanning == 625) ? 1.0 / 25 : 1001.0 / 30000;

	if (vfmt.fmt.vbi.sample_format != V4L2_VBI_SF_UBYTE) {
		DIAG("unknown VBI sampling format %d, "
		     "please contact the maintainer of this program for service",
		     vfmt.fmt.vbi.sample_format);
		goto failure;
	}

	/* Nyquist */

	if (vbi->dec.sampling_rate < max_rate * 3 / 2) {
		DIAG("VBI sampling frequency too low");
		goto failure;
	} else if (vbi->dec.sampling_rate < max_rate * 6 / 2) {
		DIAG("VBI sampling frequency too low for me");
		goto failure; /* Need smarter bit slicer */
	}

	if (!add_vbi_services(&vbi->dec, services, strict)) {
		DIAG("device cannot capture requested data services");
		goto failure;
	}

	if (vcap.flags & V4L2_FLAG_STREAMING
	    && vcap.flags & V4L2_FLAG_SELECT) {
		vbi->streaming = TRUE;

		if ((vbi->buffered = buffered)) {
			if (!init_buffered_fifo(&vbi->fifo, "vbi-v4l2-stream",
			    fifo_depth,
			    sizeof(vbi_sliced) * (vbi->dec.count[0] + vbi->dec.count[1]))) {
				goto failure;
			}
		} else {
			if (!init_callback_fifo(&vbi->fifo, "vbi-v4l2-stream",
			    wait_full_stream, send_empty_stream, fifo_depth,
			    sizeof(vbi_sliced) * (vbi->dec.count[0] + vbi->dec.count[1]))) {
				goto failure;
			}
		}

		vbi->fifo.start = start_stream;

		vrbuf.type = vbi->btype;
		vrbuf.count = MAX_RAW_BUFFERS;

		if (ioctl(vbi->fd, VIDIOC_REQBUFS, &vrbuf) == -1) {
			IODIAG("streaming I/O buffer request failed (broken driver?)");
			goto fifo_failure;
		}

		if (vrbuf.count == 0) {
			DIAG("no streaming I/O buffers granted, physical memory exhausted?");
			goto fifo_failure;
		}

		/*
		 *  Map capture buffers
		 */

		vbi->num_raw_buffers = 0;

		while (vbi->num_raw_buffers < vrbuf.count) {
			unsigned char *p;

			vbuf.type = vbi->btype;
			vbuf.index = vbi->num_raw_buffers;

			if (ioctl(vbi->fd, VIDIOC_QUERYBUF, &vbuf) == -1) {
				IODIAG("streaming I/O buffer query failed");
				goto mmap_failure;
			}

			p = mmap(NULL, vbuf.length, PROT_READ,
				MAP_SHARED, vbi->fd, vbuf.offset); /* MAP_PRIVATE ? */

			if ((int) p == -1) {
				if (errno == ENOMEM && vbi->num_raw_buffers >= 2)
					break;
			    	else {
					IODIAG("memory mapping failure");
					goto mmap_failure;
				}
			} else {
				int i, s;

				vbi->raw_buffer[vbi->num_raw_buffers].data = p;
				vbi->raw_buffer[vbi->num_raw_buffers].size = vbuf.length;

				for (i = s = 0; i < vbuf.length; i++)
					s += p[i];

				if (s % vbuf.length) {
					printf("Security warning: driver %s (%s) seems to mmap "
					       "physical memory uncleared. Please contact the "
					       "driver author.\n", dev_name, vcap.name);
					exit(EXIT_FAILURE);
				}
			}

			if (ioctl(vbi->fd, VIDIOC_QBUF, &vbuf) == -1) {
				IODIAG("cannot enqueue streaming I/O buffer (broken driver?)");
				goto mmap_failure;
			}

			vbi->num_raw_buffers++;
		}
	} else if (vcap.flags & V4L2_FLAG_READ) {
		int buffer_size = (vbi->dec.count[0] + vbi->dec.count[1])
				  * vbi->dec.samples_per_line;

		if ((vbi->buffered = buffered)) {
			if (!init_buffered_fifo(&vbi->fifo, "vbi-v4l2-read",
			    fifo_depth,
			    sizeof(vbi_sliced) * (vbi->dec.count[0] + vbi->dec.count[1]))) {
				goto failure;
			}
		} else {
			if (!init_callback_fifo(&vbi->fifo, "vbi-v4l2-read",
			    wait_full_read, send_empty_read, fifo_depth,
			    sizeof(vbi_sliced) * (vbi->dec.count[0] + vbi->dec.count[1]))) {
				goto failure;
			}
		}

		if (!(vbi->raw_buffer[0].data = malloc(buffer_size))) {
			DIAG("virtual memory exhausted");
			goto fifo_failure;
		}

		vbi->raw_buffer[0].size = buffer_size;
		vbi->num_raw_buffers = 1;

		vbi->fifo.start = start_read;
	} else {
		DIAG("broken driver, no data interface");
		goto failure;
	}

	*pvbi = vbi;

	return 1;

mmap_failure:
	for (; vbi->num_raw_buffers > 0; vbi->num_raw_buffers--)
		munmap(vbi->raw_buffer[vbi->num_raw_buffers - 1].data,
		       vbi->raw_buffer[vbi->num_raw_buffers - 1].size);

fifo_failure:
	uninit_fifo(&vbi->fifo);

failure:
	close(vbi->fd);
	free(vbi);

	return 0;
}

#else /* !HAVE_V4L2 */

static int
open_v4l2(vbi_device **pvbi, char *dev_name,
	int fifo_depth, unsigned int services, int strict, int buffered)
{
	return -1;
}

#endif /* !HAVE_V4L2 */

#define SLICED_TELETEXT_B	(SLICED_TELETEXT_B_L10_625 | SLICED_TELETEXT_B_L25_625)
#define SLICED_CAPTION		(SLICED_CAPTION_625_F1 | SLICED_CAPTION_625 \
				 | SLICED_CAPTION_525_F1 | SLICED_CAPTION_525)

/*
 *  Preliminary. Need something to re-open the
 *  device for multiple consumers.
 */

void
vbi_close_v4lx(fifo *f)
{
	vbi_device *vbi = PARENT(f, vbi_device, fifo);

	if (vbi->buffered)
		if (pthread_cancel(vbi->thread_id) == 0)
			pthread_join(vbi->thread_id, NULL);

	if (vbi->streaming)
		for (; vbi->num_raw_buffers > 0; vbi->num_raw_buffers--)
			munmap(vbi->raw_buffer[vbi->num_raw_buffers - 1].data,
			       vbi->raw_buffer[vbi->num_raw_buffers - 1].size);
	else
		for (; vbi->num_raw_buffers > 0; vbi->num_raw_buffers--)
			free(vbi->raw_buffer[vbi->num_raw_buffers - 1].data);

	uninit_fifo(&vbi->fifo);

	close(vbi->fd);

	free(vbi);
}

fifo *
vbi_open_v4lx(char *dev_name, int given_fd, int buffered, int fifo_depth)
{
	vbi_device *vbi=NULL;
	int r;

	if (!(r = open_v4l2(&vbi, dev_name, buffered ? fifo_depth : 1,
	    SLICED_TELETEXT_B | SLICED_VPS | SLICED_CAPTION, -1, buffered)))
		goto failure;

	if (r < 0)
		if (!(r = open_v4l(&vbi, dev_name, buffered ? fifo_depth : 1,
		    SLICED_TELETEXT_B | SLICED_VPS | SLICED_CAPTION,
		    -1, given_fd, buffered)))
			goto failure;
	if (r < 0)
		goto failure;

	if (!start_fifo(&vbi->fifo)) /* XXX consider moving this into vbi_mainloop */
		goto failure;

	return &vbi->fifo;

failure:
	if (vbi)
		vbi_close_v4lx(&vbi->fifo);

	return NULL;
}
