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

/* $Id: v4lx.c,v 1.22 2001-07-16 07:06:01 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifdef ENABLE_V4L

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
#include "../common/videodev2.h"

#include "v4lx.h"
#include "decoder.h"
#include "../common/math.h"
#include "../common/fifo.h"

#define IODIAG(templ, args...) //fprintf(stderr, templ ": %s (%d)\n" ,##args , strerror(errno), errno)/* with errno */
#define DIAG(templ, args...) //fprintf(stderr, templ,##args)

#define V4L2_LINE 		0 /* API rev. Nov 2000 (-1 -> 0) */

#define MAX_RAW_BUFFERS 	5

#define BTTV_VBISIZE		_IOR('v' , BASE_VIDIOCPRIVATE+8, int)

#define HAVE_V4L_VBI_FORMAT	0 // Linux 2.4 XXX configure

#define HAVE_V4L2 defined (V4L2_MAJOR_VERSION)

typedef struct {
	fifo2			fifo;			/* world interface */
	producer		producer;
	pthread_t		thread_id;

	struct vbi_decoder	dec;			/* raw vbi decoder context */

	bit_slicer_fn *		wss_slicer_fn;
	struct bit_slicer	wss_slicer;

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

#define WSS_TEST 0
#if WSS_TEST
static unsigned char wss_test_data[768 * 4];
#endif

/*
 *  Read Interface
 */

static void
wait_full_read(fifo2 *f)
{
	vbi_device *vbi = PARENT(f, vbi_device, fifo);
	struct timeval tv;
	buffer2 *b;
	size_t r;

	b = wait_empty_buffer2(&vbi->producer);

	do {
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

			b->used = -1;
			b->error = errno;

			send_full_buffer2(&vbi->producer, b);

			return;
		}

		gettimeofday(&tv, NULL);

		b->data = b->allocated;
		b->time = tv.tv_sec + tv.tv_usec / 1e6;

		b->used = sizeof(vbi_sliced) *
			vbi_decoder(&vbi->dec, vbi->raw_buffer[0].data,
				    (vbi_sliced *) b->data);
	} while (b->used == 0);

	send_full_buffer2(&vbi->producer, b);

	return;
}

static void *
read_thread(void *p)
{
	fifo2 *f = (fifo2 *) p;
	vbi_device *vbi = PARENT(f, vbi_device, fifo);
	double last_time, stacked_time, glitch_time;
	struct timeval tv;
	list3 stack;
	int stacked;
	buffer2 *b;
	size_t r;

	init_list3(&stack);
	glitch_time = vbi->time_per_frame * 1.25;
	stacked_time = 0.0;
	last_time = 0.0;
	stacked = 0;

	for (;;) {
		b = wait_empty_buffer2(&vbi->producer);

		do {
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
					send_full_buffer2(&vbi->producer,
						(buffer2 *) rem_head3(&stack));

				assert(!"read error in v4lx read thread"); /* XXX */
			}

			gettimeofday(&tv, NULL);

			b->data = b->allocated;
			b->time = tv.tv_sec + tv.tv_usec / 1e6;

			b->used = sizeof(vbi_sliced) *
				vbi_decoder(&vbi->dec, vbi->raw_buffer[0].data,
					    (vbi_sliced *) b->data);
		} while (b->used == 0);

		/*
		 *  This curious construct compensates temporary shifts
		 *  caused by an unusual delay between read() and
		 *  the execution of gettimeofday(). A complete loss
		 *  remains lost.
		 */
		if (last_time > 0 &&
		    (b->time - (last_time + stacked_time)) > glitch_time) {
			if (stacked >= (f->buffers.members >> 2)) {
				/* Not enough space &| hopeless desynced */
				for (stacked_time = 0.0; stacked > 0; stacked--) {
					buffer2 *b = (buffer2 *) rem_head3(&stack);
					send_full_buffer2(&vbi->producer, b);
				}
			} else {
				add_tail3(&stack, &b->node);
				stacked_time += vbi->time_per_frame;
				stacked++;
				continue;
			}
		} else { /* (back) on track */ 
			for (stacked_time = 0.0; stacked > 0; stacked--) {
				buffer2 *b = (buffer2 *) rem_head3(&stack);
				b->time = last_time += vbi->time_per_frame; 
				send_full_buffer2(&vbi->producer, b);
			}
		}

		last_time = b->time;
		send_full_buffer2(&vbi->producer, b);

#if WSS_TEST
		if (vbi->wss_slicer_fn) {
			static unsigned char temp[2];
			vbi_sliced *s;

			if (vbi->wss_slicer_fn(&vbi->wss_slicer, wss_test_data, temp)) {
				b = wait_empty_buffer(f);

				s = (void *) b->data = b->allocated;
				s->id = SLICED_WSS_625;
				s->line = 23;
				s->data[0] = temp[0];
				s->data[1] = temp[1];

				b->time = last_time;
				b->used = sizeof(vbi_sliced);

				send_full_buffer2(&vbi->producer, b);
			}
		}
#endif
	}

	return NULL;
}

static bool
start_read(fifo2 *f)
{
	vbi_device *vbi = PARENT(f, vbi_device, fifo);
	consumer c;
	buffer2 *b;

	if (vbi->buffered)
		return pthread_create(&vbi->thread_id, NULL,
			read_thread, f) == 0;

	if (!add_consumer(f, &c))
		return FALSE;

	if ((b = wait_full_buffer2(&c))) {
		unget_full_buffer2(&c, b);
		rem_consumer(&c);
		return TRUE; /* access should be finally granted */
	}

	rem_consumer(&c);

	// XXX thread?
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
		if (!init_buffered_fifo2(&vbi->fifo, "vbi-v4l", fifo_depth,
		    sizeof(vbi_sliced) * (vbi->dec.count[0] + vbi->dec.count[1]))) {
			goto failure;
		}
	} else {
		if (!init_callback_fifo2(&vbi->fifo, "vbi-v4l",
		    NULL, NULL, wait_full_read, NULL, fifo_depth,
		    sizeof(vbi_sliced) * (vbi->dec.count[0] + vbi->dec.count[1]))) {
			goto failure;
		}
	}

	assert(add_producer(&vbi->fifo, &vbi->producer));

	if (!(vbi->raw_buffer[0].data = malloc(buffer_size))) {
		destroy_fifo(&vbi->fifo);
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

static void
wait_full_stream(fifo2 *f)
{
	vbi_device *vbi = PARENT(f, vbi_device, fifo);
	struct v4l2_buffer vbuf;
	struct timeval tv;
	double time;
	fd_set fds;
	buffer2 *b;
	int r;

	while (vbi->buffered) {
		b = wait_empty_buffer2(&vbi->producer);

		do {
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
					b->used = -1;
					b->error = ETIME;
					send_full_buffer2(&vbi->producer, b);
					if (vbi->buffered)
						continue; /* XXX */
					return;
				} else if (r < 0) {
					IODIAG("Unknown VBI select failure");
					b->used = -1;
					b->error = errno;
					send_full_buffer2(&vbi->producer, b);
					if (vbi->buffered)
						assert(0); /* XXX */
					return;
				}
			}

			vbuf.type = vbi->btype;

			if (ioctl(vbi->fd, VIDIOC_DQBUF, &vbuf) == -1) {
				IODIAG("Cannot dequeue streaming I/O VBI buffer "
					"(broken driver or application?)");
				b->used = -1;
				b->error = errno;
				send_full_buffer2(&vbi->producer, b);
				if (vbi->buffered)
					assert(0); /* XXX */
				return;
			}

			b->data = b->allocated;
			b->time = time = vbuf.timestamp / 1e9;

			b->used = sizeof(vbi_sliced) *
				vbi_decoder(&vbi->dec, vbi->raw_buffer[vbuf.index].data,
					    (vbi_sliced *) b->data);

			if (ioctl(vbi->fd, VIDIOC_QBUF, &vbuf) == -1) {
				IODIAG("Cannot enqueue streaming I/O VBI buffer "
					"(broken driver?)");
				b->used = -1;
				b->error = errno;
				send_full_buffer2(&vbi->producer, b);
				if (vbi->buffered)
					assert(0); /* XXX */
				return;
			}

		} while (b->used == 0);

#if WSS_TEST
		if (vbi->wss_slicer_fn) {
			static unsigned char temp[2];
			vbi_sliced *s;

			if (vbi->wss_slicer_fn(&vbi->wss_slicer, wss_test_data, temp)) {
				b = wait_empty_buffer2(&vbi->producer);

				s = (void *) b->data = b->allocated;
				s->id = SLICED_WSS_625;
				s->line = 23;
				s->data[0] = temp[0];
				s->data[1] = temp[1];

				b->time = time;
				b->used = sizeof(vbi_sliced);

				send_full_buffer2(&vbi->producer, b);
			}
		}
#endif
		send_full_buffer2(&vbi->producer, b);

	} /* loop if buffered (we're a thread) */
}

static bool
start_stream(fifo2 *f)
{
	vbi_device *vbi = PARENT(f, vbi_device, fifo);
	consumer c;
	buffer2 *b;

	if (ioctl(vbi->fd, VIDIOC_STREAMON, &vbi->btype) == -1) {
		IODIAG("Cannot start VBI capturing");
		return FALSE;
	}

	if (vbi->buffered)
		return pthread_create(&vbi->thread_id, NULL,
			(void *(*)(void *)) wait_full_stream, f) == 0;

	/* Subsequent I/O shouldn't fail, let's try anyway */

	if (!add_consumer(f, &c))
		return FALSE;

	if ((b = wait_full_buffer2(&c))) {
		unget_full_buffer2(&c, b);
		rem_consumer(&c);
		return TRUE;
	}

	rem_consumer(&c);

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
			if (!init_buffered_fifo2(&vbi->fifo, "vbi-v4l2-stream",
			    fifo_depth,
			    sizeof(vbi_sliced) * (vbi->dec.count[0] + vbi->dec.count[1]))) {
				goto failure;
			}
		} else {
			if (!init_callback_fifo2(&vbi->fifo, "vbi-v4l2-stream",
			    NULL, NULL, wait_full_stream, NULL, fifo_depth,
			    sizeof(vbi_sliced) * (vbi->dec.count[0] + vbi->dec.count[1]))) {
				goto failure;
			}
		}

		vbi->fifo.start = start_stream;

		assert(add_producer(&vbi->fifo, &vbi->producer));

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
					fprintf(stderr,
					       "Security warning: driver %s (%s) seems to mmap "
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
			if (!init_buffered_fifo2(&vbi->fifo, "vbi-v4l2-read",
			    fifo_depth,
			    sizeof(vbi_sliced) * (vbi->dec.count[0] + vbi->dec.count[1]))) {
				goto failure;
			}
		} else {
			if (!init_callback_fifo2(&vbi->fifo, "vbi-v4l2-read",
			    NULL, NULL, wait_full_read, NULL, fifo_depth,
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

		assert(add_producer(&vbi->fifo, &vbi->producer));
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
	destroy_fifo(&vbi->fifo);

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

#if WSS_TEST

/* 0001 0100 000 000 == Full 4:3, PALPlus, other flags zero */

static const unsigned char
genuine_pal_wss_green[720] = {
	 54, 76, 92, 82, 81, 73, 63, 91,200,254,253,183,162,233,246,195,
	190,224,247,237,203,150, 80, 23, 53,103, 93, 38, 61,172,244,243,
	240,202,214,251,233,125, 44, 34, 59, 95, 86, 40, 54,149,235,255,
	240,208,206,247,249,164, 67, 28, 38, 82,100, 49, 26,121,227,249,
	240,206,203,242,248,182, 87, 26, 33, 82, 94, 51, 37,102,197,250,
	251,222,205,226,247,210,118, 38, 41, 78, 91, 67, 55,103,190,254,
	252,240,220,206,211,229,245,254,207,108, 28, 42, 93,100, 60, 21,
	120,200,245,241,223,221,220,216,222,233,234,188, 97, 27, 30, 70,
	 70, 68, 67, 70, 71, 67, 58, 49, 77,159,240,248,226,203,211,227,
	233,221,208,201,221,245,196, 99, 22, 39, 73, 88, 54, 34, 92,181,
	243,246,217,186,212,245,192, 95, 23, 40, 72, 81, 46, 25, 84,170,
	250,246,213,194,225,251,211,136, 22, 39, 73, 91, 62, 38, 84,157,
	236,247,241,208,201,226,235,219,222,233,222,208,230,250,220,158,
	 54, 36, 42, 74, 79, 58, 56, 76, 72, 49, 53, 80, 65, 20, 35,107,
	246,249,239,225,217,221,226,227,218,225,220,215,231,250,236,200,
	 68, 39, 31, 51, 71, 66, 53, 46, 63, 60, 58, 67, 70, 54, 50, 66,
	194,245,246,209,193,228,240,219,116, 35,  8, 64, 93, 58, 39, 61,
	170,232,249,234,210,224,237,231,127, 64, 27, 52, 84, 77, 56, 46,
	136,224,245,233,196,198,240,240,162, 74, 20, 48, 84, 72, 49, 46,
	126,200,246,242,205,202,227,245,172, 87, 19, 29, 72, 83, 59, 36,
	 89,186,249,244,196,192,229,250,202, 94, 24, 37, 76, 95, 82, 49,
	 71,171,251,251,203,189,221,253,220,123, 36, 31, 75, 96, 76, 49,
	 79,160,247,252,232,199,207,234,233,138, 44, 28, 66, 91, 76, 53,
	 59,148,229,252,240,202,202,241,252,165, 64, 20, 39, 72, 78, 65,
	 57, 62, 63, 62, 64, 67, 62, 54, 58, 67, 74, 72, 65, 63, 62, 64,
	 57, 68, 67, 55, 54, 66, 68, 57, 57, 56, 60, 63, 55, 50, 64
};

static void
wss_test_init(vbi_device *vbi)
{
	static const int std_widths[] = {
		768, 704, 640, 384, 352, 320, 192, 176, 160, -160
	};
	unsigned char *p = wss_test_data;
	int i, j, g, fmt, width, spl, bpp = 0;
	int sampling_rate;

	printf("WSS test enabled\n");

	fmt = 2;	/* see below */
	width = 352;	/* pixels per line; assume a line length
			   of 52 usec, i.e. no clipping */

	/* use this for capture widths reported by the driver, not scaled images */
	for (i = 0; width < ((std_widths[i] + std_widths[i + 1]) >> 1); i++);

	spl = std_widths[i]; /* samples in 52 usec */
	sampling_rate = spl / 52.148e-6;

	memset(wss_test_data, 0, sizeof wss_test_data);

	for (i = 0; i < width; i++) {
		double off = i * 704 / (double) spl;

		j = off;
		off -= j;

		g = (genuine_pal_wss_green[j + 1] * off
		    + genuine_pal_wss_green[j + 0] * (1 - off));

		switch (fmt) {
		case 0: /* RGB / BGR 888 */
			*p++ = rand();
			*p++ = g;
			*p++ = rand();
			bpp = 3;
			break;

		case 1: /* RGBA / BGRA 8888 */
			*p++ = rand();
			*p++ = g;
			*p++ = rand();
			*p++ = rand();
			bpp = 4;
			break;

		case 2: /* RGB / BGR 565 */
			g <<= 3;
			g ^= rand() & 0xF81F;
			*p++ = g;
			*p++ = g >> 8;
			bpp = 2;
			break;

		case 3: /* RGB / BGR 5551 */
			g <<= 2;
			g ^= rand() & 0xFC1F;
			*p++ = g;
			*p++ = g >> 8;
			bpp = 2;
			break;

		case 8: /* YUV 4:2:0 */
			*p++ = g;
			bpp = 1;
			break;

		case 9: /* YUYV / YVYU */
			*p++ = g;
			*p++ = rand();
			bpp = 2;
			break;

		case 10: /* UYVY / VYUY */
			*p++ = rand();
			*p++ = g;
			bpp = 2;
			break;
		}
	}

	vbi->wss_slicer_fn =
	init_bit_slicer(&vbi->wss_slicer, 
		width,
		sampling_rate, 
		/* cri_rate */ 5000000, 
		/* bit_rate */ 833333,
		/* cri_frc */ 0xC71E3C1F, 
		/* cri_mask */ 0x924C99CE,
		/* cri_bits */ 32, 
		/* frc_bits */ 0, 
		/* payload */ 14 * 1,
		MOD_BIPHASE_LSB_ENDIAN,
		fmt);
}

#endif

#define SLICED_TELETEXT_B	(SLICED_TELETEXT_B_L10_625 | SLICED_TELETEXT_B_L25_625)
#define SLICED_CAPTION		(SLICED_CAPTION_625_F1 | SLICED_CAPTION_625 \
				 | SLICED_CAPTION_525_F1 | SLICED_CAPTION_525)
#define SLICED_WSS		(SLICED_WSS_625 | SLICED_WSS_CPR1204)

/*
 *  Preliminary. Need something to re-open the
 *  device for multiple consumers.
 */

void
vbi_close_v4lx(fifo2 *f)
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

	destroy_fifo(&vbi->fifo);

	close(vbi->fd);

	free(vbi);
}

fifo2 *
vbi_open_v4lx(char *dev_name, int given_fd, int buffered, int fifo_depth)
{
	vbi_device *vbi=NULL;
	int r;

	if (!(r = open_v4l2(&vbi, dev_name, buffered ? fifo_depth : 1,
	    SLICED_TELETEXT_B | SLICED_VPS | SLICED_CAPTION | SLICED_WSS, -1, buffered)))
		goto failure;

	if (r < 0)
		if (!(r = open_v4l(&vbi, dev_name, buffered ? fifo_depth : 1,
		    SLICED_TELETEXT_B | SLICED_VPS | SLICED_CAPTION | SLICED_WSS,
		    -1, given_fd, buffered)))
			goto failure;
	if (r < 0)
		goto failure;

#if WSS_TEST
	wss_test_init(vbi);
#endif

	if (!start_fifo2(&vbi->fifo)) /* XXX consider moving this into vbi_mainloop */
		goto failure;

	return &vbi->fifo;

failure:
	if (vbi)
		vbi_close_v4lx(&vbi->fifo);

	return NULL;
}

#else /* !ENABLE_V4L */

#include "../common/fifo.h"
#include "v4lx.h"

/* Stubs for systems without V4L */

fifo2 *
vbi_open_v4lx(char *dev_name, int given_fd, int buffered, int fifo_depth)
{
	return NULL;
}

void
vbi_close_v4lx(fifo2 *f)
{
	return;
}

#endif
