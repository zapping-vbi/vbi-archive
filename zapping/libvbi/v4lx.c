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

/* $Id: v4lx.c,v 1.32 2001-08-10 16:31:48 mschimek Exp $ */

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
#include <asm/types.h>		// for videodev2.h

#include "v4lx.h"
#include "decoder.h"
#include "../common/math.h"
#include "../common/fifo.h"
#include "../common/videodev2.h"

#ifdef ENABLE_NLS
#    include <libintl.h>
#    define _(String) gettext (String)
#    ifdef gettext_noop
#        define N_(String) gettext_noop (String)
#    else
#        define N_(String) (String)
#    endif
#else
/* Stubs that do something close enough.  */
#    define textdomain(String) (String)
#    define gettext(String) (String)
#    define dgettext(Domain,Message) (Message)
#    define dcgettext(Domain,Message,Type) (Message)
#    define bindtextdomain(Domain,Directory) (Domain)
#    define _(String) (String)
#    define N_(String) (String)
#endif

typedef enum {
	OPEN_UNKNOWN_INTERFACE = -1,
	OPEN_FAILURE = 0,
	OPEN_SUCCESS,
} open_result;

#define IOCTL(fd, cmd, data) (TEMP_FAILURE_RETRY(ioctl(fd, cmd, data)))

extern int /* gboolean */ debug_msg;

#define printv(format, args...)						\
do {									\
	if (debug_msg) {						\
		fprintf(stderr, format ,##args);			\
		fflush(stderr);						\
	}								\
} while (0)

#define V4L2_LINE 		0 /* API rev. Nov 2000 (-1 -> 0) */

#define MAX_RAW_BUFFERS 	5

#define BTTV_VBISIZE		_IOR('v' , BASE_VIDIOCPRIVATE+8, int)

#define WSS_TEST 0
#if WSS_TEST
static unsigned char wss_test_data[768 * 4];
#endif

typedef struct {
	fifo			fifo;			/* world interface */
	producer		producer;
	pthread_t		thread_id;

	struct vbi_decoder	dec;			/* raw vbi decoder context */

#if WSS_TEST
	bit_slicer_fn *		wss_slicer_fn;
	struct bit_slicer	wss_slicer;
#endif

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

} v4l_device;

static char *
maintainer_address(void)
{
	return _("Please contact the Zapzilla maintainers at <http://zapping.sf.net>.");
}

/*
 *  Read Interface
 */

static void
wait_full_read(fifo *f)
{
	v4l_device *v4l = PARENT(f, v4l_device, fifo);
	buffer *b;
	size_t r;

	b = wait_empty_buffer(&v4l->producer);

	for (;;) {
		// XXX use select if possible to set read timeout
		pthread_testcancel();

		r = read(v4l->fd, v4l->raw_buffer[0].data,
			 v4l->raw_buffer[0].size);

		if (r == v4l->raw_buffer[0].size)
			break;

		if (r == -1
		    && (errno == EINTR || errno == ETIME))
			continue;

		b->used = -1;
		b->error = errno;
		b->errstr = _("V4L/V4L2 VBI interface: Failed to read from the device");

		send_full_buffer(&v4l->producer, b);

		return;
	}

	b->data = b->allocated;
	b->time = current_time();

	b->used = sizeof(vbi_sliced) *
		vbi_decoder(&v4l->dec, v4l->raw_buffer[0].data,
			    (vbi_sliced *) b->data);

	if (b->used == 0) {
		((vbi_sliced *) b->data)->id = 0; /* nothing */
		b->used = sizeof(vbi_sliced); /* zero means EOF */
	}

	send_full_buffer(&v4l->producer, b);

	return;
}

static void *
read_thread(void *p)
{
	fifo *f = p;
	v4l_device *v4l = PARENT(f, v4l_device, fifo);
	double last_time, stacked_time, glitch_time;
	list stack;
	int stacked;
	buffer *b;
	size_t r;

	init_list(&stack);
	glitch_time = v4l->time_per_frame * 1.25;
	stacked_time = 0.0;
	last_time = 0.0;
	stacked = 0;

	for (;;) {
		b = wait_empty_buffer(&v4l->producer);

		for (;;) {
			// XXX use select if possible to set read timeout
			pthread_testcancel();

			r = read(v4l->fd, v4l->raw_buffer[0].data,
				 v4l->raw_buffer[0].size);

			if (r == v4l->raw_buffer[0].size)
				break;

			if (r == -1
			    && (errno == EINTR || errno == ETIME))
				continue;

			for (; stacked > 0; stacked--) {
				send_full_buffer(&v4l->producer,
					PARENT(rem_head(&stack), buffer, node));
                        }

			b->used = -1;
			b->error = errno;
			b->errstr = _("V4L/V4L2 VBI interface: Failed to read from the device");

			send_full_buffer(&v4l->producer, b);

			return NULL; /* XXX */
		}

		b->data = b->allocated;
		b->time = current_time();

		b->used = sizeof(vbi_sliced) *
			vbi_decoder(&v4l->dec, v4l->raw_buffer[0].data,
				    (vbi_sliced *) b->data);

		if (b->used == 0) {
			((vbi_sliced *) b->data)->id = 0; /* nothing */
			b->used = sizeof(vbi_sliced); /* zero means EOF */
		}

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
					buffer *b = PARENT(rem_head(&stack), buffer, node);
					send_full_buffer(&v4l->producer, b);
				}
			} else {
				add_tail(&stack, &b->node);
				stacked_time += v4l->time_per_frame;
				stacked++;
				continue;
			}
		} else { /* (back) on track */ 
			for (stacked_time = 0.0; stacked > 0; stacked--) {
				buffer *b = PARENT(rem_head(&stack), buffer, node);
				b->time = last_time += v4l->time_per_frame; 
				send_full_buffer(&v4l->producer, b);
			}
		}

		last_time = b->time;
		send_full_buffer(&v4l->producer, b);

#if WSS_TEST
		if (v4l->wss_slicer_fn) {
			static unsigned char temp[2];
			vbi_sliced *s;

			if (v4l->wss_slicer_fn(&v4l->wss_slicer, wss_test_data, temp)) {
				b = wait_empty_buffer(f);

				s = (void *) b->data = b->allocated;
				s->id = SLICED_WSS_625;
				s->line = 23;
				s->data[0] = temp[0];
				s->data[1] = temp[1];

				b->time = last_time;
				b->used = sizeof(vbi_sliced);

				send_full_buffer(&v4l->producer, b);
			}
		}
#endif
	}

	return NULL;
}

static bool
start_read(fifo *f)
{
	v4l_device *v4l = PARENT(f, v4l_device, fifo);
	consumer c;
	buffer *b;

	if (v4l->buffered)
		return pthread_create(&v4l->thread_id, NULL,
			read_thread, f) == 0;

	if (!add_consumer(f, &c))
		return FALSE;

/* XXX can block infinitely (?) */
	if ((b = wait_full_buffer(&c))) {
		send_empty_buffer(&c, b);
		rem_consumer(&c);
		return TRUE; /* access should be finally granted */
	}

	rem_consumer(&c);

	return FALSE;
}

#include <dirent.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

static void
perm_check(char *name, char **err_str)
{
	struct stat st;
	int old_errno = errno;
	uid_t uid = geteuid();
	gid_t gid = getegid();

	if (stat(name, &st) == -1) {
		printv("stat %s failed: %d, %s\n", name, errno, strerror(errno));
		errno = old_errno;
		return;
	}

	printv("%s permissions: user=%d.%d mode=0%o, I am %d.%d\n",
		name, st.st_uid, st.st_gid, st.st_mode, uid, gid);

	errno = old_errno;
}

static bool
reverse_lookup(int fd, struct stat *vbi_stat)
{
	struct video_capability vcap;
	struct video_unit vunit;

	if (IOCTL(fd, VIDIOCGCAP, &vcap) != 0) {
		printv("Driver doesn't support VIDIOCGCAP, probably not v4l\n");
		return FALSE;
	}

	if (!(vcap.type & VID_TYPE_CAPTURE)) {
		printv("Driver is no video capture device\n");
		return FALSE;
	}

	if (IOCTL(fd, VIDIOCGUNIT, &vunit) != 0) {
		printv("Driver doesn't support VIDIOCGUNIT\n");
		return FALSE;
	}

	if (vunit.vbi != minor(vbi_stat->st_rdev)) {
		printv("Driver reports vbi minor %d, need %d\n",
			vunit.vbi, minor(vbi_stat->st_rdev));
		return FALSE;
	}

	printv("Matched\n");
	return TRUE;
}

static bool
get_videostd(int fd, int *mode)
{
	struct video_tuner vtuner;
	struct video_channel vchan;

	memset(&vtuner, 0, sizeof(vtuner));
	memset(&vchan, 0, sizeof(vchan));

	if (IOCTL(fd, VIDIOCGTUNER, &vtuner) != -1) {
		printv("Driver supports VIDIOCGTUNER: %d\n", vtuner.mode);
		*mode = vtuner.mode;
		return TRUE;
	} else if (IOCTL(fd, VIDIOCGCHAN, &vchan) != -1) {
		printv("Driver supports VIDIOCGCHAN: %d\n", vchan.norm);
		*mode = vchan.norm;
		return TRUE;
	} else
		printv("Driver doesn't support VIDIOCGTUNER or VIDIOCGCHAN\n");

	return FALSE;
}

#define VIDEO_DEV "/dev/video0"

static bool
probe_video_device(char *name, struct stat *vbi_stat, int *mode)
{
	struct stat vid_stat;
	int fd;

	if (stat(name, &vid_stat) == -1) {
		printv("stat failed: %d, %s\n",	errno, strerror(errno));
		return FALSE;
	}

	if (!S_ISCHR(vid_stat.st_mode)) {
		printv("%s is no character special file\n", name);
		return FALSE;
	}

	if (major(vid_stat.st_rdev) != major(vbi_stat->st_rdev)) {
		printv("Mismatch of major device number: "
			"%s: %d, %d; vbi: %d, %d\n", name,
			major(vid_stat.st_rdev), minor(vid_stat.st_rdev),
			major(vbi_stat->st_rdev), minor(vbi_stat->st_rdev));
		return FALSE;
	}

	if (!(fd = open(name, O_RDONLY | O_TRUNC))) {
		printv("Cannot open %s: %d, %s\n", name, errno, strerror(errno));
		perm_check(name, NULL);
		return FALSE;
	}

	if (!reverse_lookup(fd, vbi_stat)
	    || !get_videostd(fd, mode)) {
		close(fd);
		return FALSE;
	}

	close(fd);

	return TRUE;
}

static bool
guess_bttv_v4l(v4l_device *v4l, int *strict, int given_fd, char **err_str)
{
	static char *video_devices[] = {
		"/dev/video",
		"/dev/video0",
		"/dev/video1",
		"/dev/video2",
		"/dev/video3",
	};
	struct dirent dirent, *pdirent = &dirent;
	struct stat vbi_stat;
	DIR *dir;
	int mode = -1;
	int i;

	printv("Attempt to guess the videostandard\n");

	if (get_videostd(v4l->fd, &mode))
		goto finish;

	/*
	 *  Bttv v4l has no VIDIOCGUNIT pointing back to
	 *  the associated video device, now it's getting
	 *  dirty. We're dumb enough to walk only /dev,
	 *  first level of, and assume v4l major is still 81.
	 *  Not tested with devfs.
	 */
	printv("Attempt to find a reverse VIDIOCGUNIT\n");

	if (fstat(v4l->fd, &vbi_stat) == -1) {
		printv("fstat failed: %d, %s\n", errno, strerror(errno));
		goto finish;
	}

	if (!S_ISCHR(vbi_stat.st_mode)) {
		printv("VBI device is no character special file, reject\n");
		return FALSE;
	}

	if (major(vbi_stat.st_rdev) != 81) {
		printv("VBI device CSF has major number %d, expect 81\n"
			"Warning: will assume this is still a v4l device\n",
			major(vbi_stat.st_rdev));
		goto finish;
	}

	printv("VBI device type verified\n");

	if (given_fd > 0) {
		printv("Try suggested corresponding video fd:\n");

		if (reverse_lookup(given_fd, &vbi_stat))
			if (get_videostd(given_fd, &mode))
				goto finish;
	}

	for (i = 0; i < sizeof(video_devices) / sizeof(video_devices[0]); i++) {
		printv("Try %s: ", video_devices[i]);

		if (probe_video_device(video_devices[i], &vbi_stat, &mode))
			goto finish;
	}

	printv("Traversing /dev\n");

	if (!(dir = opendir("/dev"))) {
		printv("Cannot open /dev: %d, %s\n", errno, strerror(errno));
		perm_check("/dev", NULL);
		goto finish;
	}

	while (readdir_r(dir, &dirent, &pdirent) == 0 && pdirent) {
		char *s;

		if (!asprintf(&s, "/dev/%s", dirent.d_name))
			continue;

		printv("Try %s: ", s);

		if (probe_video_device(s, &vbi_stat, &mode)) {
			free(s);
			goto finish;
		}

		free(s);
	}

	closedir(dir);

	printv("Traversing finished\n");

 finish:
	switch (mode) {
	case VIDEO_MODE_NTSC:
		printv("Videostandard is NTSC\n");
		v4l->dec.scanning = 525;
		break;

	case VIDEO_MODE_PAL:
	case VIDEO_MODE_SECAM:
		printv("Videostandard is PAL/SECAM\n");
		v4l->dec.scanning = 625;
		break;

	default:
		/*
		 *  One last chance, we'll try to guess
		 *  the scanning if GVBIFMT is available.
		 */
		printv("Videostandard unknown (%d)\n", mode);
		v4l->dec.scanning = 0;
		*strict = TRUE;
		break;
	}

	return TRUE;
}

static open_result
open_v4l(v4l_device **pvbi, char *dev_name,
	 int fifo_depth, unsigned int services,
	 int strict, int given_fd, int buffered,
	 char **err_str)
{
	struct vbi_format vfmt;
	struct video_capability vcap;
	int max_rate, raw_buffer_size, sliced_buffer_size;
	char *driver_name = _("driver unknown");
	v4l_device *v4l;

	printv("Try to open v4l vbi device\n");

	if (!(v4l = calloc(1, sizeof(v4l_device)))) {
		errno = ENOMEM;
		asprintf(err_str, _("Virtual memory exhausted.\n"));
		return OPEN_FAILURE;
	}

	if ((v4l->fd = open(dev_name, O_RDONLY)) == -1) {
		asprintf(err_str, _("Cannot open '%s': %d, %s."),
			dev_name, errno, strerror(errno));
		perm_check(dev_name, err_str);
		free(v4l);
		return OPEN_FAILURE;
	}

	printv("Opened %s\n", dev_name);

	if (IOCTL(v4l->fd, VIDIOCGCAP, &vcap) == -1) {
		/*
		 *  Older bttv drivers don't support any
		 *  v4l ioctls, let's see if we can guess the beast.
		 */
		printv("Driver doesn't support VIDIOCGCAP\n");

		if (!guess_bttv_v4l(v4l, &strict, given_fd, err_str)) {
			close(v4l->fd);
			free(v4l);
			return OPEN_UNKNOWN_INTERFACE; /* Definately not V4L */
		}
	} else {
		if (vcap.name) {
			printv("Driver name '%s'\n", vcap.name);
			driver_name = vcap.name;
		}

		if (!(vcap.type & VID_TYPE_TELETEXT)) {
			asprintf(err_str, _("%s (%s) is not a raw VBI device.\n"),
				dev_name, driver_name);
			goto failure;
		}

		guess_bttv_v4l(v4l, &strict, given_fd, err_str);
	}

	printv("Guessed videostandard %d\n", v4l->dec.scanning);

	max_rate = 0;

	/* May need a rewrite */
	if (IOCTL(v4l->fd, VIDIOCGVBIFMT, &vfmt) == 0) {
		if (!v4l->dec.scanning
		    && v4l->dec.start[1] > 0
		    && v4l->dec.count[1]) {
			if (v4l->dec.start[1] >= 286)
				v4l->dec.scanning = 625;
			else
				v4l->dec.scanning = 525;
		}

		printv("Driver supports VIDIOCGVBIFMT, "
			"guessed videostandard %d\n", v4l->dec.scanning);

		/* Speculative, vbi_format is not documented */
		if (strict >= 0 && v4l->dec.scanning) {
			printv("Attempt to set vbi capture parameters\n");

			if (!(services = qualify_vbi_sampling(&v4l->dec, &max_rate, services))) {
				asprintf(err_str, _("Sorry, %s (%s) cannot capture the requested data services.\n"),
					dev_name, driver_name);
				goto failure;
			}

			memset(&vfmt, 0, sizeof(struct vbi_format));

			vfmt.sample_format	= VIDEO_PALETTE_RAW;
			vfmt.sampling_rate	= v4l->dec.sampling_rate;
			vfmt.samples_per_line	= v4l->dec.samples_per_line;
			vfmt.start[0]		= v4l->dec.start[0];
			vfmt.count[0]		= v4l->dec.count[1];
			vfmt.start[1]		= v4l->dec.start[0];
			vfmt.count[1]		= v4l->dec.count[1];

			/* Single field allowed? */

			if (!vfmt.count[0]) {
				vfmt.start[0] = (v4l->dec.scanning == 625) ? 6 : 10;
				vfmt.count[0] = 1;
			} else if (!vfmt.count[1]) {
				vfmt.start[1] = (v4l->dec.scanning == 625) ? 318 : 272;
				vfmt.count[1] = 1;
			}

			if (IOCTL(v4l->fd, VIDIOCSVBIFMT, &vfmt) == -1) {
				switch (errno) {
				case EBUSY:
					asprintf(err_str, _("%s (%s) is already in use.\n"),
						dev_name, driver_name);
					break;

		    		default:
					asprintf(err_str, _("Could not set the vbi capture parameters for %s (%s), "
							  "possibly a driver bug. %s\n"),
						dev_name, driver_name, maintainer_address());
					break;
				}

				goto failure;
			}

		} /* strict >= 0 */

		printv("Accept current vbi parameters\n");

		if (vfmt.sample_format != VIDEO_PALETTE_RAW) {
			asprintf(err_str, _("%s (%s) offers unknown vbi sampling format #%d. %s"),
				dev_name, driver_name, vfmt.sample_format, maintainer_address());
			goto failure;
		}

		v4l->dec.sampling_rate		= vfmt.sampling_rate;
		v4l->dec.samples_per_line 	= vfmt.samples_per_line;
		if (v4l->dec.scanning == 625)
			v4l->dec.offset 	= 10.2e-6 * vfmt.sampling_rate;
		else if (v4l->dec.scanning == 525)
			v4l->dec.offset		= 9.2e-6 * vfmt.sampling_rate;
		else /* we don't know */
			v4l->dec.offset		= 9.7e-6 * vfmt.sampling_rate;
		v4l->dec.start[0] 		= vfmt.start[0];
		v4l->dec.count[0] 		= vfmt.count[0];
		v4l->dec.start[1] 		= vfmt.start[1];
		v4l->dec.count[1] 		= vfmt.count[1];
		v4l->dec.interlaced		= !!(vfmt.flags & VBI_INTERLACED);
		v4l->dec.synchronous		= !(vfmt.flags & VBI_UNSYNC);
		v4l->time_per_frame 		= (v4l->dec.scanning == 625) ? 1.0 / 25 : 1001.0 / 30000;

	} else { 
		int size;

		/*
		 *  If a more reliable method exists to identify the bttv
		 *  driver I'll be glad to hear about it. Lesson: Don't
		 *  call a v4l private IOCTL without knowing who's
		 *  listening. All we know at this point: It's a csf, and
		 *  it may be a v4l device.
		 *  garetxe: This isn't reliable, bttv doesn't return
		 *  anything useful in vcap.name.
		 */
		printv("Driver doesn't support VIDIOCGVBIFMT, will assume bttv interface.\n");

		if (0 && !strstr(driver_name, "bttv") && !strstr(driver_name, "BTTV")) {
			asprintf(err_str, _("Cannot capture with %s (%s), has no standard vbi interface.\n"),
				dev_name, driver_name);
			goto failure;
		}

		switch (v4l->dec.scanning) {
		default:
			if (0) {
				asprintf(err_str, _("Cannot set or determine current videostandard of %s (%s).\n"),
					dev_name, driver_name);
				goto failure;
			}

			printv("Warning: Videostandard not confirmed, will assume PAL/SECAM\n");

			v4l->dec.scanning = 625;

		case 625:
			v4l->dec.sampling_rate = 35468950;
			v4l->dec.offset = 10.2e-6 * 35468950;
			v4l->dec.start[0] = -1; // XXX FIX ME for CC-625
			v4l->dec.start[1] = -1;
			break;

		case 525:
			v4l->dec.sampling_rate = 28636363;
			v4l->dec.offset = 9.2e-6 * 28636363;
			v4l->dec.start[0] = 10;	 // confirmed for bttv 0.7.52
			v4l->dec.start[1] = 273;
			break;
		}

		v4l->dec.samples_per_line 	= 2048;
		v4l->dec.interlaced		= FALSE;
		v4l->dec.synchronous		= TRUE;
		v4l->time_per_frame 		= (v4l->dec.scanning == 625) ? 1.0 / 25 : 1001.0 / 30000;

		printv("Attempt to determine vbi frame size\n");

		if ((size = IOCTL(v4l->fd, BTTV_VBISIZE, 0)) == -1) {
			printv("Driver does not support BTTV_VBISIZE, "
				"assume BSD or old BTTV driver\n");
			v4l->dec.count[0] = 16;
			v4l->dec.count[1] = 16;
		} else if (size % 2048) {
			asprintf(err_str, _("%s (%s) reports improbable vbi frame size. %s\n"),
				dev_name, driver_name, maintainer_address());
			goto failure;
		} else {
			printv("Driver supports BTTV_VBISIZE: %d bytes, assume top field "
				"dominance and 2048 bpl\n", size);
			size /= 2048;
			v4l->dec.count[0] = size >> 1;
			v4l->dec.count[1] = size - v4l->dec.count[0];
		}
	}

	if (!services) {
		asprintf(err_str, _("Sorry, %s (%s) cannot capture any of the requested data services.\n"),
			dev_name, driver_name);
		goto failure;
	}

	if (!v4l->dec.scanning && strict >= 1) {
		printv("Try to guess videostandard from vbi bottom field "
			"boundaries: start=%d, count=%d\n",
			v4l->dec.start[1], v4l->dec.count[1]);

		if (v4l->dec.start[1] <= 0 || !v4l->dec.count[1]) {
			/*
			 *  We may have requested single field capture
			 *  ourselves, but then we had guessed already.
			 */
			if (0) {
				asprintf(err_str, _("Cannot set or determine current videostandard of %s (%s).\n"),
					dev_name, driver_name);
				goto failure;
			}

			printv("Warning: Videostandard not confirmed, will assume PAL/SECAM\n");

			v4l->dec.scanning = 625;
			v4l->time_per_frame = 1.0 / 25;
		} else if (v4l->dec.start[1] < 286) {
			v4l->dec.scanning = 525;
			v4l->time_per_frame = 1001.0 / 30000;
		} else {
			v4l->dec.scanning = 625;
			v4l->time_per_frame = 1.0 / 25;
		}
	}

	printv("Guessed videostandard %d\n", v4l->dec.scanning);

	/* Nyquist */

	if (v4l->dec.sampling_rate < max_rate * 3 / 2) {
		asprintf(err_str, _("Cannot capture the requested data services with "
			"%s (%s), the sampling frequency is too low.\n"),
			dev_name, driver_name);
		goto failure;
	}

	printv("Nyquist check passed\n");

	if (!add_vbi_services(&v4l->dec, services, strict)) {
		asprintf(err_str, _("Sorry, %s (%s) cannot capture any of the requested data services.\n"),
			dev_name, driver_name);
		goto failure;
	}

	printv("VBI decoder initialized\n");

	raw_buffer_size = (v4l->dec.count[0] + v4l->dec.count[1])
		* v4l->dec.samples_per_line;

	sliced_buffer_size = (v4l->dec.count[0] + v4l->dec.count[1])
		* sizeof(vbi_sliced);

	if ((v4l->buffered = buffered)) {
		if (!init_buffered_fifo(&v4l->fifo, "vbi-v4l-buffered",
		     fifo_depth, sliced_buffer_size)) {
			errno = ENOMEM;
			asprintf(err_str, _("Not enough memory to allocate vbi data buffers (%d KB).\n"),
				(fifo_depth * sliced_buffer_size + 1023) >> 10);
			goto failure;
		}
	} else {
		if (!init_callback_fifo(&v4l->fifo, "vbi-v4l-callback",
		    NULL, NULL, wait_full_read, NULL, fifo_depth, sliced_buffer_size)) {
			errno = ENOMEM;
			asprintf(err_str, _("Not enough memory to allocate vbi data buffers (%d KB).\n"),
				(fifo_depth * sliced_buffer_size + 1023) >> 10);
			goto failure;
		}
	}

	v4l->fifo.start = start_read;

	assert(add_producer(&v4l->fifo, &v4l->producer));

	printv("Fifo initialized\n");

	if (!(v4l->raw_buffer[0].data = malloc(raw_buffer_size))) {
		asprintf(err_str, _("Not enough memory to allocate vbi capture buffer (%d KB).\n"),
			(raw_buffer_size + 1023) >> 10);
		destroy_fifo(&v4l->fifo);
		goto failure;
	}

	printv("Capture buffer allocated\nSuccessful opened %s (%s)\n",
		dev_name, driver_name);

	v4l->raw_buffer[0].size = raw_buffer_size;
	v4l->num_raw_buffers = 1;

	*pvbi = v4l;

	*err_str = NULL;
	errno = 0;

	return OPEN_SUCCESS;

failure:
	close(v4l->fd);
	free(v4l);

	return OPEN_FAILURE;
}

#ifndef V4L2_BUF_TYPE_VBI /* API rev. Sep 2000 */
#define V4L2_BUF_TYPE_VBI V4L2_BUF_TYPE_CAPTURE;
#endif

/*
 *  Streaming I/O Interface
 */

static void *
wait_full_stream(fifo *f)
{
	v4l_device *v4l = PARENT(f, v4l_device, fifo);
	struct v4l2_buffer vbuf;
	double time;
	fd_set fds;
	buffer *b;
	int r;

	while (v4l->buffered) {
		b = wait_empty_buffer(&v4l->producer);

		r = -1;

		while (r <= 0) {
			struct timeval tv;

			FD_ZERO(&fds);
			FD_SET(v4l->fd, &fds);

			tv.tv_sec = 2;
			tv.tv_usec = 0;

			pthread_testcancel();

			r = select(v4l->fd + 1, &fds, NULL, NULL, &tv);

			if (r < 0 && errno == EINTR)
				continue;

			if (r == 0) {
				/* timeout */
				b->used = -1;
				b->error = ETIME;
				b->errstr = _("V4L2 VBI interface: Capture stalled, "
					      "no station tuned in?");

				send_full_buffer(&v4l->producer, b);

				if (v4l->buffered)
					continue; /* XXX yes? */

				return NULL;
			} else if (r < 0) {
				b->used = -1;
				b->error = errno;
				b->errstr = _("V4L2 VBI interface: Failed to read from the device");

				send_full_buffer(&v4l->producer, b);

				if (v4l->buffered)
					return NULL; /* XXX */

				return NULL;
			}
		}

		vbuf.type = v4l->btype;

		if (IOCTL(v4l->fd, VIDIOC_DQBUF, &vbuf) == -1) {
			b->used = -1;
			b->error = errno;
			b->errstr = _("V4L2 VBI interface: Cannot dequeue "
				      "buffer, driver or application bug?");

			send_full_buffer(&v4l->producer, b);

			if (v4l->buffered)
				return NULL; /* XXX */

			return NULL;
		}

		b->data = b->allocated;
		b->time = time = vbuf.timestamp / 1e9;

		b->used = sizeof(vbi_sliced) *
			vbi_decoder(&v4l->dec, v4l->raw_buffer[vbuf.index].data,
				    (vbi_sliced *) b->data);

		if (IOCTL(v4l->fd, VIDIOC_QBUF, &vbuf) == -1) {
			b->used = -1;
			b->error = errno;
			b->errstr = _("V4L2 VBI interface: Cannot enqueue "
				      "buffer, driver or application bug?");

			send_full_buffer(&v4l->producer, b);

			if (v4l->buffered)
				return NULL; /* XXX */

			return NULL;
		}

		if (b->used == 0) {
			((vbi_sliced *) b->data)->id = 0; /* nothing */
			b->used = sizeof(vbi_sliced); /* zero means EOF */
		}

#if WSS_TEST
		if (v4l->wss_slicer_fn) {
			static unsigned char temp[2];
			vbi_sliced *s;

			if (v4l->wss_slicer_fn(&v4l->wss_slicer, wss_test_data, temp)) {
				b = wait_empty_buffer(&v4l->producer);

				s = (void *) b->data = b->allocated;
				s->id = SLICED_WSS_625;
				s->line = 23;
				s->data[0] = temp[0];
				s->data[1] = temp[1];

				b->time = time;
				b->used = sizeof(vbi_sliced);

				send_full_buffer(&v4l->producer, b);
			}
		}
#endif
		send_full_buffer(&v4l->producer, b);

	} /* loop if buffered (we're a thread) */

	return NULL;
}

static bool
start_stream(fifo *f)
{
	v4l_device *v4l = PARENT(f, v4l_device, fifo);
	consumer c;
	buffer *b;

	if (IOCTL(v4l->fd, VIDIOC_STREAMON, &v4l->btype) == -1) {
//		IODIAG("Cannot start VBI capturing");
		return FALSE;
	}

	if (v4l->buffered)
		return pthread_create(&v4l->thread_id, NULL,
			(void *(*)(void *)) wait_full_stream, f) == 0;

	/* Subsequent I/O shouldn't fail, let's try anyway */

	if (!add_consumer(f, &c))
		return FALSE;

/* XXX can block infinitely (?) destroy thread? */
	if ((b = wait_full_buffer(&c))) {
		send_empty_buffer(&c, b);
		rem_consumer(&c);
		return TRUE;
	}

	rem_consumer(&c);

	return FALSE;
}

static int
open_v4l2(v4l_device **pvbi, char *dev_name,
	int fifo_depth, unsigned int services, int strict,
	int buffered, char **err_str)
{
	struct v4l2_capability vcap;
	struct v4l2_format vfmt;
	struct v4l2_requestbuffers vrbuf;
	struct v4l2_buffer vbuf;
	struct v4l2_standard vstd;
	v4l_device *v4l;
	int max_rate;

	assert(services != 0);
	assert(fifo_depth > 0);

	printv("Try to open v4l2 vbi device\n");

	if (!(v4l = calloc(1, sizeof(v4l_device)))) {
		errno = ENOMEM;
		asprintf(err_str, _("Virtual memory exhausted.\n"));
		return OPEN_FAILURE;
	}

	if ((v4l->fd = open(dev_name, O_RDONLY)) == -1) {
		asprintf(err_str, _("Cannot open '%s': %d, %s."),
			dev_name, errno, strerror(errno));
		perm_check(dev_name, err_str);
		free(v4l);
		return OPEN_FAILURE;
	}

	printv("Opened %s\n", dev_name);

	if (IOCTL(v4l->fd, VIDIOC_QUERYCAP, &vcap) == -1) {
		printv("Driver doesn't support VIDIOC_QUERYCAP, assume v4l device\n");
		close(v4l->fd);
		free(v4l);
		return OPEN_UNKNOWN_INTERFACE; // not V4L2
	}

	if (vcap.type != V4L2_TYPE_VBI) {
		asprintf(err_str, _("%s (%s) is not a raw VBI device.\n"),
			dev_name, vcap.name);
		goto failure;
	}

	printv("%s (%s) is a v4l2 vbi device\n", dev_name, vcap.name);

	if (IOCTL(v4l->fd, VIDIOC_G_STD, &vstd) == -1) {
		/* mandatory, http://www.thedirks.org/v4l2/v4l2dsi.htm */
		asprintf(err_str, _("Cannot query current videostandard of %s (%s): %d, %s. "
			"Probably a driver bug.\n"), dev_name, vcap.name, errno, strerror(errno));
		goto failure;
	}

	v4l->dec.scanning = vstd.framelines;
	/* add_vbi_services() eliminates non 525/625 */

	printv("Current scanning system %d\n", v4l->dec.scanning);

	memset(&vfmt, 0, sizeof(vfmt));

	vfmt.type = v4l->btype = V4L2_BUF_TYPE_VBI;

	max_rate = 0;

	printv("Querying current vbi parameters... ");

	if (IOCTL(v4l->fd, VIDIOC_G_FMT, &vfmt) == -1) {
		printv("failed\n");
		strict = MAX(0, strict);
#if 0
		asprintf(err_str, _("Cannot query current vbi parameters of %s (%s): %d, %s.\n"),
			dev_name, vcap.name, errno, strerror(errno));
		goto failure;
#endif
	} else
		printv("success\n");

	if (strict >= 0) {
		printv("Attempt to set vbi capture parameters\n");

		if (!(services = qualify_vbi_sampling(&v4l->dec, &max_rate, services))) {
			asprintf(err_str, _("Sorry, %s (%s) cannot capture the requested "
				"data services.\n"), dev_name, vcap.name);
			goto failure;
		}

		vfmt.fmt.vbi.sample_format	= V4L2_VBI_SF_UBYTE;
		vfmt.fmt.vbi.sampling_rate	= v4l->dec.sampling_rate;
		vfmt.fmt.vbi.samples_per_line	= v4l->dec.samples_per_line;
		vfmt.fmt.vbi.offset		= v4l->dec.offset;
		vfmt.fmt.vbi.start[0]		= v4l->dec.start[0] + V4L2_LINE;
		vfmt.fmt.vbi.count[0]		= v4l->dec.count[1];
		vfmt.fmt.vbi.start[1]		= v4l->dec.start[0] + V4L2_LINE;
		vfmt.fmt.vbi.count[1]		= v4l->dec.count[1];

		/* API rev. Nov 2000 paranoia */

		if (!vfmt.fmt.vbi.count[0]) {
			vfmt.fmt.vbi.start[0] = ((v4l->dec.scanning == 625) ? 6 : 10) + V4L2_LINE;
			vfmt.fmt.vbi.count[0] = 1;
		} else if (!vfmt.fmt.vbi.count[1]) {
			vfmt.fmt.vbi.start[1] = ((v4l->dec.scanning == 625) ? 318 : 272) + V4L2_LINE;
			vfmt.fmt.vbi.count[1] = 1;
		}

		if (IOCTL(v4l->fd, VIDIOC_S_FMT, &vfmt) == -1) {
			switch (errno) {
			case EBUSY:
				asprintf(err_str, _("%s (%s) is already in use.\n"),
					dev_name, vcap.name);
				break;


			case EINVAL:
				printv("VIDIOC_S_FMT failed, trying bttv2 rev. 021100 workaround\n");

				vfmt.type = v4l->btype = V4L2_BUF_TYPE_CAPTURE;
				vfmt.fmt.vbi.start[0] = 0;
				vfmt.fmt.vbi.count[0] = 16;
				vfmt.fmt.vbi.start[1] = 313;
				vfmt.fmt.vbi.count[1] = 16;

				if (IOCTL(v4l->fd, VIDIOC_S_FMT, &vfmt) == -1) {
			default:
					asprintf(err_str, _("Could not set the vbi capture parameters for %s (%s), "
							  "possibly a driver bug. %s\n"),
						dev_name, vcap.name, maintainer_address());
					goto failure;
				}

				vfmt.fmt.vbi.start[0] = 7 + V4L2_LINE;
				vfmt.fmt.vbi.start[1] = 320 + V4L2_LINE;

				break;
			}
		}

		printv("Successful set vbi capture parameters\n");
	}

	v4l->dec.sampling_rate		= vfmt.fmt.vbi.sampling_rate;
	v4l->dec.samples_per_line 	= vfmt.fmt.vbi.samples_per_line;
	v4l->dec.offset			= vfmt.fmt.vbi.offset;
	v4l->dec.start[0] 		= vfmt.fmt.vbi.start[0] - V4L2_LINE;
	v4l->dec.count[0] 		= vfmt.fmt.vbi.count[0];
	v4l->dec.start[1] 		= vfmt.fmt.vbi.start[1] - V4L2_LINE;
	v4l->dec.count[1] 		= vfmt.fmt.vbi.count[1];
	v4l->dec.interlaced		= !!(vfmt.fmt.vbi.flags & V4L2_VBI_INTERLACED);
	v4l->dec.synchronous		= !(vfmt.fmt.vbi.flags & V4L2_VBI_UNSYNC);
	v4l->time_per_frame 		= (v4l->dec.scanning == 625) ? 1.0 / 25 : 1001.0 / 30000;

	if (vfmt.fmt.vbi.sample_format != V4L2_VBI_SF_UBYTE) {
		asprintf(err_str, _("%s (%s) offers unknown vbi sampling format #%d. %s"),
			dev_name, vcap.name, vfmt.fmt.vbi.sample_format, maintainer_address());
		goto failure;
	}

	/* Nyquist */

	if (v4l->dec.sampling_rate < max_rate * 3 / 2) {
		asprintf(err_str, _("Cannot capture the requested data services with "
			"%s (%s), the sampling frequency is too low.\n"),
			dev_name, vcap.name);
		goto failure;
	}

	printv("Nyquist check passed\n");

	if (!add_vbi_services(&v4l->dec, services, strict)) {
		asprintf(err_str, _("Sorry, %s (%s) cannot capture any of the requested data services.\n"),
			dev_name, vcap.name);
		goto failure;
	}

	if (vcap.flags & V4L2_FLAG_STREAMING
	    && vcap.flags & V4L2_FLAG_SELECT) {
		int sliced_buffer_size = (v4l->dec.count[0] + v4l->dec.count[1])
			* sizeof(vbi_sliced);

		printv("Using streaming interface\n");

		v4l->streaming = TRUE;

		if ((v4l->buffered = buffered)) {
			if (!init_buffered_fifo(&v4l->fifo, "v4l-v4l2-buffered-stream",
			    fifo_depth, sliced_buffer_size)) {
				asprintf(err_str, _("Not enough memory to allocate vbi data buffers (%d KB).\n"),
					(fifo_depth * sliced_buffer_size + 1023) >> 10);
				goto failure;
			}
		} else {
			if (!init_callback_fifo(&v4l->fifo, "v4l-v4l2-callback-stream",
			    NULL, NULL, (void (*)(fifo *)) wait_full_stream, NULL,
			    fifo_depth, sliced_buffer_size)) {
				asprintf(err_str, _("Not enough memory to allocate vbi data buffers (%d KB).\n"),
					(fifo_depth * sliced_buffer_size + 1023) >> 10);
				goto failure;
			}
		}

		v4l->fifo.start = start_stream;

		assert(add_producer(&v4l->fifo, &v4l->producer));

		printv("Fifo initialized\nRequesting streaming i/o buffers\n");

		vrbuf.type = v4l->btype;
		vrbuf.count = MAX_RAW_BUFFERS;

		if (IOCTL(v4l->fd, VIDIOC_REQBUFS, &vrbuf) == -1) {
			asprintf(err_str, _("Cannot request streaming i/o buffers from %s (%s): %d, %s. Driver bug?\n"),
				dev_name, vcap.name, errno, strerror(errno));
			goto fifo_failure;
		}

		if (vrbuf.count == 0) {
			asprintf(err_str, _("%s (%s) granted no streaming i/o buffers, "
				"physical memory exhausted?\n"),
				dev_name, vcap.name);
			goto fifo_failure;
		}

		printv("Mapping streaming i/o buffers\n");

		v4l->num_raw_buffers = 0;

		while (v4l->num_raw_buffers < vrbuf.count) {
			unsigned char *p;

			vbuf.type = v4l->btype;
			vbuf.index = v4l->num_raw_buffers;

			if (IOCTL(v4l->fd, VIDIOC_QUERYBUF, &vbuf) == -1) {
				asprintf(err_str, _("Querying streaming i/o buffer #%d "
					"from %s (%s) failed: %d, %s.\n"),
					v4l->num_raw_buffers, dev_name, vcap.name,
					errno, strerror(errno));
				goto mmap_failure;
			}

			p = mmap(NULL, vbuf.length, PROT_READ,
				MAP_SHARED, v4l->fd, vbuf.offset); /* MAP_PRIVATE ? */

			if ((int) p == -1) {
				if (errno == ENOMEM && v4l->num_raw_buffers >= 2)
					break;

				asprintf(err_str, _("Memory mapping streaming i/o buffer #%d "
					"from %s (%s) failed: %d, %s.\n"),
					v4l->num_raw_buffers, dev_name, vcap.name,
					errno, strerror(errno));
				goto mmap_failure;
			} else {
				int i, s;

				v4l->raw_buffer[v4l->num_raw_buffers].data = p;
				v4l->raw_buffer[v4l->num_raw_buffers].size = vbuf.length;

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

			if (IOCTL(v4l->fd, VIDIOC_QBUF, &vbuf) == -1) {
				asprintf(err_str, _("Cannot enqueue streaming i/o buffer #%d "
					"to %s (%s): %d, %s. Driver bug?\n"),
					v4l->num_raw_buffers, dev_name, vcap.name,
					errno, strerror(errno));
				goto mmap_failure;
			}

			v4l->num_raw_buffers++;
		}
	} else if (vcap.flags & V4L2_FLAG_READ) {
		int raw_buffer_size = (v4l->dec.count[0] + v4l->dec.count[1])
			* v4l->dec.samples_per_line;
		int sliced_buffer_size = (v4l->dec.count[0] + v4l->dec.count[1])
			* sizeof(vbi_sliced);

		printv("Using read interface\n");

		if (buffered) {
			if (!init_buffered_fifo(&v4l->fifo, "v4l-v4l2-read",
			    fifo_depth, sliced_buffer_size)) {
				asprintf(err_str, _("Not enough memory to allocate vbi data buffers (%d KB).\n"),
					(fifo_depth * sliced_buffer_size + 1023) >> 10);
				goto failure;
			}
		} else {
			if (!init_callback_fifo(&v4l->fifo, "v4l-v4l2-read",
			    NULL, NULL, wait_full_read, NULL,
			    fifo_depth, sliced_buffer_size)) {
				asprintf(err_str, _("Not enough memory to allocate vbi data buffers (%d KB).\n"),
					(fifo_depth * sliced_buffer_size + 1023) >> 10);
				goto failure;
			}
		}

		v4l->fifo.start = start_read;

		assert(add_producer(&v4l->fifo, &v4l->producer));

		v4l->buffered = buffered;

		printv("Fifo initialized\n");

		if (!(v4l->raw_buffer[0].data = malloc(raw_buffer_size))) {
			asprintf(err_str, _("Not enough memory to allocate "
				"vbi capture buffer (%d KB).\n"), (raw_buffer_size + 1023) >> 10);
			goto fifo_failure;
		}

		v4l->raw_buffer[0].size = raw_buffer_size;
		v4l->num_raw_buffers = 1;

		printv("Capture buffer allocated\n");
	} else {
		asprintf(err_str, _("%s (%s) lacks a vbi read interface, possibly an output only "
			"device or a driver bug. %s\n"),
			dev_name, vcap.name, maintainer_address());
		goto failure;
	}

	printv("Successful opened %s (%s)\n",
		dev_name, vcap.name);

	*pvbi = v4l;

	return OPEN_SUCCESS;

mmap_failure:
	for (; v4l->num_raw_buffers > 0; v4l->num_raw_buffers--)
		munmap(v4l->raw_buffer[v4l->num_raw_buffers - 1].data,
		       v4l->raw_buffer[v4l->num_raw_buffers - 1].size);

fifo_failure:
	destroy_fifo(&v4l->fifo);

failure:
	close(v4l->fd);
	free(v4l);

	return OPEN_FAILURE;
}

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
wss_test_init(v4l_device *v4l)
{
	static const int std_widths[] = {
		768, 704, 640, 384, 352, 320, 192, 176, 160, -160
	};
	unsigned char *p = wss_test_data;
	int i, j, g, width, spl, bpp = 0;
	enum tveng_frame_pixformat fmt;
	int sampling_rate;

	fprintf(stderr, "WSS test enabled\n");

	fmt = TVENG_PIX_RGB565;	/* see below */
	width = 352;		/* pixels per line; assume a line length
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
		case TVENG_PIX_RGB24:
		case TVENG_PIX_BGR24:
			*p++ = rand();
			*p++ = g;
			*p++ = rand();
			bpp = 3;
			break;

		case TVENG_PIX_RGB32: /* RGBA / BGRA */
		case TVENG_PIX_BGR32:
			*p++ = rand();
			*p++ = g;
			*p++ = rand();
			*p++ = rand();
			bpp = 4;
			break;

		case TVENG_PIX_RGB565:
			g <<= 3;
			g ^= rand() & 0xF81F;
			*p++ = g;
			*p++ = g >> 8;
			bpp = 2;
			break;

		case TVENG_PIX_RGB555:
			g <<= 2;
			g ^= rand() & 0xFC1F;
			*p++ = g;
			*p++ = g >> 8;
			bpp = 2;
			break;

		case TVENG_PIX_YVU420:
		case TVENG_PIX_YUV420:
			*p++ = g;
			bpp = 1;
			break;

		case TVENG_PIX_YUYV:
			*p++ = g;
			*p++ = rand();
			bpp = 2;
			break;

		case TVENG_PIX_UYVY:
			*p++ = rand();
			*p++ = g;
			bpp = 2;
			break;
		}
	}

	v4l->wss_slicer_fn =
	init_bit_slicer(&v4l->wss_slicer, 
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

#endif /* WSS_TEST */

#define SLICED_TELETEXT_B	(SLICED_TELETEXT_B_L10_625 | SLICED_TELETEXT_B_L25_625)
#define SLICED_CAPTION		(SLICED_CAPTION_625_F1 | SLICED_CAPTION_625 \
				 | SLICED_CAPTION_525_F1 | SLICED_CAPTION_525)
#define SLICED_WSS		(SLICED_WSS_625 | SLICED_WSS_CPR1204)

/*
 *  Preliminary. Need something to re-open the
 *  device for multiple consumers.
 */

void
vbi_close_v4lx(fifo *f)
{
	v4l_device *v4l = PARENT(f, v4l_device, fifo);

	printv("\n");

	if (v4l->buffered && v4l->thread_id) {
		printv("  Cancelling v4lx thread\n");
		if (pthread_cancel(v4l->thread_id) == 0)
			pthread_join(v4l->thread_id, NULL);
	}

	printv("  Unmapping/freeing v4lx capture buffers\n");

	if (v4l->streaming)
		for (; v4l->num_raw_buffers > 0; v4l->num_raw_buffers--)
			munmap(v4l->raw_buffer[v4l->num_raw_buffers - 1].data,
			       v4l->raw_buffer[v4l->num_raw_buffers - 1].size);
	else
		for (; v4l->num_raw_buffers > 0; v4l->num_raw_buffers--)
			free(v4l->raw_buffer[v4l->num_raw_buffers - 1].data);

	printv("  Destroy fifo and close vbi device\n");

	destroy_fifo(&v4l->fifo);

	close(v4l->fd);

	free(v4l);

	printv("  V4LX closing sequence complete\n");
}

/* given_fd points to an opened video device, or -1, ignored for V4L2 */
fifo *
vbi_open_v4lx(char *dev_name, int given_fd, int buffered,
	int fifo_depth, char **err_str)
{
	v4l_device *v4l = NULL;
	open_result r = OPEN_UNKNOWN_INTERFACE;

	*err_str = NULL;

	if (r == OPEN_UNKNOWN_INTERFACE)
		if ((r = open_v4l2(&v4l, dev_name, buffered ? fifo_depth : 1,
		    SLICED_TELETEXT_B | SLICED_VPS | SLICED_CAPTION | SLICED_WSS, -1,
		    buffered, err_str)) == OPEN_FAILURE)
			goto failure;

	if (r == OPEN_UNKNOWN_INTERFACE)
		if ((r = open_v4l(&v4l, dev_name, buffered ? fifo_depth : 1,
		    SLICED_TELETEXT_B | SLICED_VPS | SLICED_CAPTION | SLICED_WSS,
		    -1, given_fd, buffered, err_str)) == OPEN_FAILURE)
			goto failure;

	if (r == OPEN_UNKNOWN_INTERFACE)
		goto failure;

#if WSS_TEST
	wss_test_init(v4l);
#endif

	if (!start_fifo(&v4l->fifo)) /* XXX consider moving this into vbi_mainloop */
		goto failure;

	return &v4l->fifo;

failure:
	if (v4l)
		vbi_close_v4lx(&v4l->fifo);

	if (*err_str)
		printv("Error message: %s\n", *err_str);

	return NULL;
}

#else /* !ENABLE_V4L */

#include "../common/fifo.h"
#include "v4lx.h"

/* Stubs for systems without V4L/V4L2 */

fifo *
vbi_open_v4lx(char *dev_name, int given_fd, int buffered,
	int fifo_depth, char **err_str)
{
	printv("vbi_open_v4lx: not compiled with Video4Linux support\n");
	return NULL;
}

void
vbi_close_v4lx(fifo *f)
{
	return;
}

#endif /* !ENABLE_V4L */
