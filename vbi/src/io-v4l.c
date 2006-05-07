/*
 *  libzvbi - V4L interface
 *
 *  Copyright (C) 1999, 2000, 2001, 2002 Michael H. Schimek
 *
 *  This program is vbi3_free software; you can redistribute it and/or modify
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

static char rcsid[] = "$Id: io-v4l.c,v 1.9.2.13 2006-05-07 06:04:58 mschimek Exp $";

#ifdef HAVE_CONFIG_H
#  include "../config.h"
#endif

#include "vbi.h"
#include "intl-priv.h"
#include "io-priv.h"
#include "misc.h"

#ifdef ENABLE_V4L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <sys/time.h>		/* timeval */
#include <sys/types.h>		/* fd_set */
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <pthread.h>

#include "videodev.h"
#include "_videodev.h"

#define log_fp 0

/* This macro checks at compile time if the arg type is correct,
   repeats the ioctl if interrupted (EINTR), and logs the args
   and result if log_fp is non-zero. */
#define _ioctl(fd, cmd, arg)						\
(IOCTL_ARG_TYPE_CHECK_ ## cmd (arg),					\
 device_ioctl (log_fp, fprint_ioctl_arg, fd, cmd, (void *)(arg)))

#define BTTV_VBISIZE		_IOR('v' , BASE_VIDIOCPRIVATE+8, int)
static __inline__ void IOCTL_ARG_TYPE_CHECK_BTTV_VBISIZE (int *arg) {}

#undef REQUIRE_SELECT
#undef REQUIRE_SVBIFMT		/* else accept current parameters */
#undef REQUIRE_VIDEOSTD		/* if clueless, assume PAL/SECAM */

#define printv(format, args...)						\
do {									\
	if (trace) {							\
		fprintf(stderr, format ,##args);			\
		fflush(stderr);						\
	}								\
} while (0)

typedef struct vbi3_capture_v4l {
	vbi3_capture		capture;

	int			fd;
	vbi3_bool		select;

	vbi3_raw_decoder		dec;

	double			time_per_frame;

	vbi3_capture_buffer	*raw_buffer;
	int			num_raw_buffers;

	vbi3_capture_buffer	sliced_buffer;

} vbi3_capture_v4l;

static int
v4l_read(vbi3_capture *vc, vbi3_capture_buffer **raw,
	 vbi3_capture_buffer **sliced, struct timeval *timeout)
{
	vbi3_capture_v4l *v = PARENT(vc, vbi3_capture_v4l, capture);
	vbi3_capture_buffer *my_raw = v->raw_buffer;
	struct timeval tv;
	int r;

	while (v->select) {
		fd_set fds;

		FD_ZERO(&fds);
		FD_SET(v->fd, &fds);

		tv = *timeout; /* Linux kernel overwrites this */

		r = select(v->fd + 1, &fds, NULL, NULL, &tv);

		if (r < 0 && errno == EINTR)
			continue;

		if (r <= 0)
			return r; /* timeout or error */

		break;
	}

	if (!raw)
		raw = &my_raw;
	if (!*raw)
		*raw = v->raw_buffer;
	else
		(*raw)->size = v->raw_buffer[0].size;

	for (;;) {
		/* from zapping/libvbi/v4lx.c */
		pthread_testcancel();

		r = read(v->fd, (*raw)->data, (*raw)->size);

		if (r == -1 && (errno == EINTR || errno == ETIME))
			continue;

		if (r == (*raw)->size)
			break;
		else
			return -1;
	}

	gettimeofday(&tv, NULL);

	(*raw)->timestamp = tv.tv_sec + tv.tv_usec * (1 / 1e6);

	if (sliced) {
		int lines;

		if (*sliced) {
			lines = vbi3_raw_decoder_decode
			  (&v->dec,
			   (vbi3_sliced *)(*sliced)->data,
			   /* FIXME */ 50,
			   (*raw)->data);
		} else {
			*sliced = &v->sliced_buffer;
			lines = vbi3_raw_decoder_decode
			  (&v->dec,
			   (vbi3_sliced *)(v->sliced_buffer.data),
			   /* FIXME */ 50,
			   (*raw)->data);
		}

		(*sliced)->size = lines * sizeof(vbi3_sliced);
		(*sliced)->timestamp = (*raw)->timestamp;
	}

	return 1;
}

/* Molto rumore per nulla. */

#include <dirent.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

static void
perm_check(const char *name, vbi3_bool trace)
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

static vbi3_bool
reverse_lookup(int fd, struct stat *vbi3_stat, vbi3_bool trace)
{
	struct video_capability vcap;
	struct video_unit vunit;

	if (_ioctl (fd, VIDIOCGCAP, &vcap) != 0) {
		printv("Driver doesn't support VIDIOCGCAP, probably not v4l\n");
		return FALSE;
	}

	if (!(vcap.type & VID_TYPE_CAPTURE)) {
		printv("Driver is no video capture device\n");
		return FALSE;
	}

	if (_ioctl (fd, VIDIOCGUNIT, &vunit) != 0) {
		printv("Driver doesn't support VIDIOCGUNIT\n");
		return FALSE;
	}

	if (vunit.vbi != minor(vbi3_stat->st_rdev)) {
		printv("Driver reports vbi minor %d, need %d\n",
			vunit.vbi, minor(vbi3_stat->st_rdev));
		return FALSE;
	}

	printv("Matched\n");
	return TRUE;
}

static vbi3_bool
get_videostd(int fd, int *mode, vbi3_bool trace)
{
	struct video_tuner vtuner;
	struct video_channel vchan;

	memset(&vtuner, 0, sizeof(vtuner));
	memset(&vchan, 0, sizeof(vchan));

	if (_ioctl (fd, VIDIOCGTUNER, &vtuner) != -1) {
		printv("Driver supports VIDIOCGTUNER: %d\n", vtuner.mode);
		*mode = vtuner.mode;
		return TRUE;
	} else if (_ioctl (fd, VIDIOCGCHAN, &vchan) != -1) {
		printv("Driver supports VIDIOCGCHAN: %d\n", vchan.norm);
		*mode = vchan.norm;
		return TRUE;
	} else
		printv("Driver doesn't support VIDIOCGTUNER or VIDIOCGCHAN\n");

	return FALSE;
}

static vbi3_bool
probe_video_device(char *name, struct stat *vbi3_stat,
		   int *mode, vbi3_bool trace)
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

	if (major(vid_stat.st_rdev) != major(vbi3_stat->st_rdev)) {
		printv("Mismatch of major device number: "
			"%s: %d, %d; vbi: %d, %d\n", name,
			major(vid_stat.st_rdev), minor(vid_stat.st_rdev),
			major(vbi3_stat->st_rdev), minor(vbi3_stat->st_rdev));
		return FALSE;
	}

	if (!(fd = device_open(log_fp, name, O_RDONLY | O_TRUNC, 0))) {
		printv("Cannot open %s: %d, %s\n", name, errno, strerror(errno));
		perm_check(name, trace);
		return FALSE;
	}

	if (!reverse_lookup(fd, vbi3_stat, trace)
	    || !get_videostd(fd, mode, trace)) {
		device_close(log_fp, fd);
		return FALSE;
	}

	device_close(log_fp, fd);

	return TRUE;
}

static vbi3_bool
guess_bttv_v4l(vbi3_capture_v4l *v, int *strict,
	       int given_fd, int scanning, vbi3_bool trace)
{
	static const char *video_devices[] = {
		"/dev/video",
		"/dev/video0",
		"/dev/video1",
		"/dev/video2",
		"/dev/video3",
	};
	struct dirent dirent, *pdirent = &dirent;
	struct stat vbi3_stat;
	DIR *dir;
	int mode = -1;
	unsigned int i;

	switch (scanning) {
        case 525:
		v->dec.sampling.videostd_set = VBI3_VIDEOSTD_SET_525_60;
                return TRUE;
        case 625:
                v->dec.sampling.videostd_set = VBI3_VIDEOSTD_SET_625_50;
                return TRUE;
        default:
                break;
        }

	printv("Attempt to guess the videostandard\n");

	if (get_videostd(v->fd, &mode, trace))
		goto finish;

	/*
	 *  Bttv v4l has no VIDIOCGUNIT pointing back to
	 *  the associated video device, now it's getting
	 *  dirty. We'll walk /dev, first level of, and
	 *  assume v4l major is still 81. Not tested with devfs.
	 */
	printv("Attempt to find a reverse VIDIOCGUNIT\n");

	if (fstat(v->fd, &vbi3_stat) == -1) {
		printv("fstat failed: %d, %s\n", errno, strerror(errno));
		goto finish;
	}

	if (!S_ISCHR(vbi3_stat.st_mode)) {
		printv("VBI device is no character special file, reject\n");
		return FALSE;
	}

	if (major(vbi3_stat.st_rdev) != 81) {
		printv("VBI device CSF has major number %d, expect 81\n"
			"Warning: will assume this is still a v4l device\n",
			major(vbi3_stat.st_rdev));
		goto finish;
	}

	printv("VBI device type verified\n");

	if (given_fd > -1) {
		printv("Try suggested corresponding video fd\n");

		if (reverse_lookup(given_fd, &vbi3_stat, trace))
			if (get_videostd(given_fd, &mode, trace))
				goto finish;
	}

	for (i = 0; i < sizeof(video_devices) / sizeof(video_devices[0]); i++) {
		printv("Try %s: ", video_devices[i]);

		if (probe_video_device(video_devices[i], &vbi3_stat, &mode, trace))
			goto finish;
	}

	printv("Traversing /dev\n");

	if (!(dir = opendir("/dev"))) {
		printv("Cannot open /dev: %d, %s\n", errno, strerror(errno));
		perm_check("/dev", trace);
		goto finish;
	}

	while (readdir_r(dir, &dirent, &pdirent) == 0 && pdirent) {
		char name[256];

		snprintf(name, sizeof(name), "/dev/%s", dirent.d_name);

		printv("Try %s: ", name);

		if (probe_video_device(name, &vbi3_stat, &mode, trace))
			goto finish;
	}

	closedir(dir);

	printv("Traversing finished\n");

 finish:
	switch (mode) {
	case VIDEO_MODE_NTSC:
		printv("Videostandard is NTSC\n");
		v->dec.sampling.videostd_set = VBI3_VIDEOSTD_SET_525_60;
		break;

	case VIDEO_MODE_PAL:
	case VIDEO_MODE_SECAM:
		printv("Videostandard is PAL/SECAM\n");
		v->dec.sampling.videostd_set = VBI3_VIDEOSTD_SET_625_50;
		break;

	default:
		/*
		 *  One last chance, we'll try to guess
		 *  the scanning if GVBIFMT is available.
		 */
		printv("Videostandard unknown (%d)\n", mode);
		v->dec.sampling.videostd_set = 0;
		*strict = TRUE;
		break;
	}

	return TRUE;
}

vbi3_inline vbi3_bool
set_parameters(vbi3_capture_v4l *v, struct vbi_format *vfmt, int *max_rate,
	       const char *dev_name, char *driver_name,
	       unsigned int *services, int strict,
	       char **errorstr, vbi3_bool trace)
{
	struct vbi_format vfmt_temp = *vfmt;

	/* Speculative, vbi_format is not documented */

	printv("Attempt to set vbi capture parameters\n");

	*services = vbi3_sampling_par_from_services
	  (&v->dec.sampling, max_rate, v->dec.sampling.videostd_set, *services);

	if (*services == 0) {
		_vbi3_asprintf(errorstr, _("Sorry, %s (%s) cannot capture any of the "
					 "requested data services."),
			     dev_name, driver_name, trace);
		return FALSE;
	}

	memset(vfmt, 0, sizeof(*vfmt));

	vfmt->sample_format	= VIDEO_PALETTE_RAW;
	vfmt->sampling_rate	= v->dec.sampling.sampling_rate;
	vfmt->samples_per_line	= v->dec.sampling.samples_per_line;
	vfmt->start[0]		= v->dec.sampling.start[0];
	vfmt->count[0]		= v->dec.sampling.count[1];
	vfmt->start[1]		= v->dec.sampling.start[0];
	vfmt->count[1]		= v->dec.sampling.count[1];

	/* Single field allowed? */

	if (!vfmt->count[0]) {
		if (VBI3_VIDEOSTD_SET_525_60 & v->dec.sampling.videostd_set)
			vfmt->start[0] = 10;
		else
			vfmt->start[0] = 6;
		vfmt->count[0] = 1;
	} else if (!vfmt->count[1]) {
		if (VBI3_VIDEOSTD_SET_525_60 & v->dec.sampling.videostd_set)
			vfmt->start[1] = 272;
		else
			vfmt->start[1] = 318;
		vfmt->count[1] = 1;
	}

	if (_ioctl (v->fd, VIDIOCSVBIFMT, vfmt) == 0)
		return TRUE;

	switch (errno) {
	case EBUSY:
#ifndef REQUIRE_SVBIFMT
		printv("VIDIOCSVBIFMT returned EBUSY, "
		       "will try the current parameters\n");
		*vfmt = vfmt_temp;
		return TRUE;
#endif
		_vbi3_asprintf(errorstr, _("Cannot initialize %s (%s), "
					 "the device is already in use."),
			     dev_name, driver_name);
		break;

	case EINVAL:
                if (strict < 2) {
		        printv("VIDIOCSVBIFMT returned EINVAL, "
		               "will try the current parameters\n");
                        *vfmt = vfmt_temp;
                        return TRUE;
                }
		break;
	default:
		_vbi3_asprintf(errorstr, _("Could not set the vbi "
					 "capture parameters for %s (%s): %d, %s."),
			     dev_name, driver_name, errno, strerror(errno));
		/* guess = _("Maybe a bug in the driver or libzvbi."); */
		break;
	}

	return FALSE;
}

static vbi3_raw_decoder *
v4l_parameters(vbi3_capture *vc)
{
	vbi3_capture_v4l *v = PARENT(vc, vbi3_capture_v4l, capture);

	return &v->dec;
}

static void
v4l_delete(vbi3_capture *vc)
{
	vbi3_capture_v4l *v = PARENT(vc, vbi3_capture_v4l, capture);

	if (v->sliced_buffer.data)
		vbi3_free(v->sliced_buffer.data);

	for (; v->num_raw_buffers > 0; v->num_raw_buffers--)
		vbi3_free(v->raw_buffer[v->num_raw_buffers - 1].data);

	if (v->fd != -1)
		device_close(log_fp, v->fd);

	vbi3_free(v);
}

static int
v4l_fd(vbi3_capture *vc)
{
	vbi3_capture_v4l *v = PARENT(vc, vbi3_capture_v4l, capture);

	return v->fd;
}

static void
print_vfmt(const char *s, struct vbi_format *vfmt)
{
	fprintf(stderr, "%sformat %08x, %d Hz, %d bpl, "
		"F1 %d+%d, F2 %d+%d, flags %08x\n", s,
		vfmt->sample_format,
		vfmt->sampling_rate, vfmt->samples_per_line,
		vfmt->start[0], vfmt->count[0],
		vfmt->start[1], vfmt->count[1],
		vfmt->flags);
}

static vbi3_capture *
v4l_new(const char *dev_name, int given_fd, int scanning,
	unsigned int *services, int strict,
	char **errorstr, vbi3_bool trace)
{
	struct video_capability vcap;
	struct vbi_format vfmt;
	int max_rate;
	char *driver_name = _("driver unknown");
	vbi3_capture_v4l *v;

	//	pthread_once (&vbi3_init_once, vbi3_init);

	assert(services && *services != 0);

	if (scanning != 525 && scanning != 625)
		scanning = 0;

	printv("Try to open v4l vbi device, libzvbi interface rev.\n"
	       "%s", rcsid);

	if (!(v = (vbi3_capture_v4l *) vbi3_malloc(sizeof(*v)))) {
		_vbi3_asprintf(errorstr, _("Virtual memory exhausted."));
		errno = ENOMEM;
		return NULL;
	}

	CLEAR (*v);

	v->capture.parameters = v4l_parameters;
	v->capture._delete = v4l_delete;
	v->capture.get_fd = v4l_fd;

	if ((v->fd = device_open(log_fp, dev_name, O_RDONLY, 0)) == -1) {
		_vbi3_asprintf(errorstr, _("Cannot open '%s': %d, %s."),
			     dev_name, errno, strerror(errno));
		perm_check(dev_name, trace);
		goto io_error;
	}

	printv("Opened %s\n", dev_name);

	if (_ioctl (v->fd, VIDIOCGCAP, &vcap) == -1) {
		/*
		 *  Older bttv drivers don't support any
		 *  v4l ioctls, let's see if we can guess the beast.
		 */
		printv("Driver doesn't support VIDIOCGCAP\n");

		if (!guess_bttv_v4l(v, &strict, given_fd, scanning, trace))
			goto failure;
	} else {
		if (vcap.name) {
			printv("Driver name '%s'\n", vcap.name);
			driver_name = vcap.name;
		}

		if (!(vcap.type & VID_TYPE_TELETEXT)) {
			_vbi3_asprintf(errorstr,
				     _("%s (%s) is not a raw vbi device."),
				     dev_name, driver_name);
			goto failure;
		}

		guess_bttv_v4l(v, &strict, given_fd, scanning, trace);
	}

	printv("%s (%s) is a v4l vbi device\n", dev_name, driver_name);

	v->select = FALSE; /* FIXME if possible */

	printv("Hinted video standard %d, guessed 0x%08" PRIx64 "\n",
	       scanning, v->dec.sampling.videostd_set);

	max_rate = 0;

	/* May need a rewrite */
	if (_ioctl (v->fd, VIDIOCGVBIFMT, &vfmt) == 0) {
		if (v->dec.sampling.start[1] > 0 && v->dec.sampling.count[1]) {
			if (v->dec.sampling.start[1] >= 286)
				v->dec.sampling.videostd_set = VBI3_VIDEOSTD_SET_625_50;
			else
				v->dec.sampling.videostd_set = VBI3_VIDEOSTD_SET_525_60;
		}

		printv("Driver supports VIDIOCGVBIFMT, "
		       "guessed videostandard 0x%08" PRIx64 "\n",
		       v->dec.sampling.videostd_set);

		if (trace)
			print_vfmt("VBI capture parameters supported: ", &vfmt);

		if (strict >= 0 && v->dec.sampling.videostd_set)
			if (!set_parameters(v, &vfmt, &max_rate,
					    dev_name, driver_name,
					    services, strict,
					    errorstr, trace))
				goto failure;

		printv("Accept current vbi parameters\n");

		if (vfmt.sample_format != VIDEO_PALETTE_RAW) {
			_vbi3_asprintf(errorstr, _("%s (%s) offers unknown vbi "
						 "sampling format #%d. "
						 "This may be a driver bug "
						 "or libzvbi is too old."),
				     dev_name, driver_name, vfmt.sample_format);
			goto failure;
		}

		v->dec.sampling.sampling_rate		= vfmt.sampling_rate;
		v->dec.sampling.samples_per_line	= vfmt.samples_per_line;
		v->dec.sampling.bytes_per_line 		= vfmt.samples_per_line;
		if (VBI3_VIDEOSTD_SET_625_50 & v->dec.sampling.videostd_set)
			/* v->dec.sampling.offset 		= (int)(10.2e-6 * vfmt.sampling_rate); */
			v->dec.sampling.offset           = (int)(6.8e-6 * vfmt.sampling_rate);  /* XXX TZ FIX */
		else if (VBI3_VIDEOSTD_SET_525_60 & v->dec.sampling.videostd_set)
			v->dec.sampling.offset		= (int)(9.2e-6 * vfmt.sampling_rate);
		else /* we don't know */
			v->dec.sampling.offset		= (int)(9.7e-6 * vfmt.sampling_rate);
		v->dec.sampling.start[0] 		= vfmt.start[0];
		v->dec.sampling.count[0] 		= vfmt.count[0];
		v->dec.sampling.start[1] 		= vfmt.start[1];
		v->dec.sampling.count[1] 		= vfmt.count[1];
		v->dec.sampling.interlaced		= !!(vfmt.flags & VBI_INTERLACED);
		v->dec.sampling.synchronous		= !(vfmt.flags & VBI_UNSYNC);

		if (VBI3_VIDEOSTD_SET_525_60 & v->dec.sampling.videostd_set)
			v->time_per_frame = 1001.0 / 30000;
		else
			v->time_per_frame = 1.0 / 25;
	} else { 
		int size;

		/*
		 *  If a more reliable method exists to identify the bttv
		 *  driver I'll be glad to hear about it. Lesson: Don't
		 *  call a v4l private ioctl  without knowing who's
		 *  listening. All we know at this point: It's a csf, and
		 *  it may be a v4l device.
		 *  garetxe: This isn't reliable, bttv doesn't return
		 *  anything useful in vcap.name.
		 */
		printv("Driver doesn't support VIDIOCGVBIFMT, "
		       "will assume bttv interface\n");

		v->select = TRUE; /* it does */

		if (0 && !strstr(driver_name, "bttv")
		      && !strstr(driver_name, "BTTV")) {
			_vbi3_asprintf(errorstr, _("Cannot capture with %s (%s), "
						 "has no standard vbi interface."),
				     dev_name, driver_name);
			goto failure;
		}

		v->dec.sampling.samples_per_line	= 2048;
		v->dec.sampling.bytes_per_line 		= 2048;
		v->dec.sampling.interlaced		= FALSE;
		v->dec.sampling.synchronous		= TRUE;

		printv("Attempt to determine vbi frame size\n");

		if ((size = _ioctl (v->fd, BTTV_VBISIZE, 0)) == -1) {
			printv("Driver does not support BTTV_VBISIZE, "
				"assume old BTTV driver\n");
			v->dec.sampling.count[0] = 16;
			v->dec.sampling.count[1] = 16;
		} else if (size % 2048) {
			_vbi3_asprintf(errorstr, _("Cannot identify %s (%s), reported "
						 "vbi frame size suggests this is "
						 "not a bttv driver."),
				     dev_name, driver_name);
			goto failure;
		} else {
			printv("Driver supports BTTV_VBISIZE: %d bytes, "
			       "assume top field dominance and 2048 bpl\n", size);
			size /= 2048;
			v->dec.sampling.count[0] = size >> 1;
			v->dec.sampling.count[1] = size - v->dec.sampling.count[0];
		}

		if (VBI3_VIDEOSTD_SET_525_60 & v->dec.sampling.videostd_set) {
			/* Confirmed for bttv 0.7.52 */
			v->dec.sampling.sampling_rate = 28636363;
			v->dec.sampling.offset = (int)(9.2e-6 * 28636363);
			v->dec.sampling.start[0] = 10;
			v->dec.sampling.start[1] = 273;
		} else if (VBI3_VIDEOSTD_SET_625_50 & v->dec.sampling.videostd_set) {
			/* Not confirmed */
			v->dec.sampling.sampling_rate = 35468950;
			v->dec.sampling.offset = (int)(9.2e-6 * 35468950);  /* XXX TZ FIX */
			v->dec.sampling.start[0] = 22 + 1 - v->dec.sampling.count[0];
			v->dec.sampling.start[1] = 335 + 1 - v->dec.sampling.count[1];
		} else {
#ifdef REQUIRE_VIDEOSTD
			_vbi3_asprintf(errorstr, _("Cannot set or determine current "
						 "videostandard of %s (%s)."),
				     dev_name, driver_name);
			goto failure;
#endif
			printv("Warning: Videostandard not confirmed, "
			       "will assume PAL/SECAM\n");

			v->dec.sampling.videostd_set = VBI3_VIDEOSTD_SET_625_50;

			/* Not confirmed */
			v->dec.sampling.sampling_rate = 35468950;
			v->dec.sampling.offset = (int)(9.2e-6 * 35468950);  /* XXX TZ FIX */
			v->dec.sampling.start[0] = 22 + 1 - v->dec.sampling.count[0];
			v->dec.sampling.start[1] = 335 + 1 - v->dec.sampling.count[1];
		}

		if (VBI3_VIDEOSTD_SET_525_60 & v->dec.sampling.videostd_set)
			v->time_per_frame = 1001.0 / 30000;
		else
			v->time_per_frame = 1.0 / 25;
	}

#ifdef REQUIRE_SELECT
	if (!v->select) {
		_vbi3_asprintf(errorstr, _("%s (%s) does not support "
					 "the select() function."),
			     dev_name, driver_name);
		goto failure;
	}
#endif

	if (*services == 0) {
		_vbi3_asprintf(errorstr, _("Sorry, %s (%s) cannot capture any of the "
					 "requested data services."),
			     dev_name, driver_name);
		goto failure;
	}

	if (!v->dec.sampling.videostd_set && strict >= 1) {
		printv("Try to guess video standard from vbi bottom field "
			"boundaries: start=%d, count=%d\n",
		       v->dec.sampling.start[1], v->dec.sampling.count[1]);

		if (v->dec.sampling.start[1] <= 0 || !v->dec.sampling.count[1]) {
			/*
			 *  We may have requested single field capture
			 *  ourselves, but then we had guessed already.
			 */
#ifdef REQUIRE_VIDEOSTD
			_vbi3_asprintf(errorstr, _("Cannot set or determine current "
						 "videostandard of %s (%s)."),
				     dev_name, driver_name);
			goto failure;
#endif
			printv("Warning: Videostandard not confirmed, "
			       "will assume PAL/SECAM\n");

			v->dec.sampling.videostd_set = VBI3_VIDEOSTD_SET_625_50;
			v->time_per_frame = 1.0 / 25;
		} else if (v->dec.sampling.start[1] < 286) {
			v->dec.sampling.videostd_set = VBI3_VIDEOSTD_SET_525_60;
			v->time_per_frame = 1001.0 / 30000;
		} else {
			v->dec.sampling.videostd_set = VBI3_VIDEOSTD_SET_625_50;
			v->time_per_frame = 1.0 / 25;
		}
	}

	printv("Guessed videostandard %08" PRIx64 "\n",
	       v->dec.sampling.videostd_set);

	v->dec.sampling.sampling_format = VBI3_PIXFMT_Y8;

	if (*services & ~(VBI3_SLICED_VBI3_525 | VBI3_SLICED_VBI3_625)) {
		/* Nyquist */

		if (v->dec.sampling.sampling_rate < max_rate * 3 / 2) {
			_vbi3_asprintf(errorstr, _("Cannot capture the requested "
						 "data services with "
						 "%s (%s), the sampling frequency "
						 "%.2f MHz is too low."),
				     dev_name, driver_name,
				     v->dec.sampling.sampling_rate / 1e6);
			goto failure;
		}

		printv("Nyquist check passed\n");

		*services = vbi3_raw_decoder_add_services
			(&v->dec, *services, strict);

		if (*services == 0) {
			_vbi3_asprintf(errorstr, _("Sorry, %s (%s) cannot "
						 "capture any of "
						 "the requested data services."),
				     dev_name, driver_name);
			goto failure;
		}

		v->sliced_buffer.data =
			vbi3_malloc((v->dec.sampling.count[0] + v->dec.sampling.count[1])
			       * sizeof(vbi3_sliced));

		if (!v->sliced_buffer.data) {
			_vbi3_asprintf(errorstr, _("Virtual memory exhausted."));
			errno = ENOMEM;
			goto failure;
		}
	}

	printv("Will dec.sampling.de services 0x%08x\n", *services);

	/* Read mode */

	if (!v->select)
		printv("Warning: no read select, reading will block\n");

	v->capture.read = v4l_read;

	v->raw_buffer = vbi3_malloc (sizeof(v->raw_buffer[0]));

	if (!v->raw_buffer) {
		_vbi3_asprintf(errorstr, _("Virtual memory exhausted."));
		errno = ENOMEM;
		goto failure;
	}

	CLEAR (v->raw_buffer[0]);

	v->raw_buffer[0].size = (v->dec.sampling.count[0] + v->dec.sampling.count[1])
		* v->dec.sampling.bytes_per_line;

	v->raw_buffer[0].data = vbi3_malloc(v->raw_buffer[0].size);

	if (!v->raw_buffer[0].data) {
		_vbi3_asprintf(errorstr, _("Not enough memory to allocate "
					 "vbi capture buffer (%d KB)."),
			     (v->raw_buffer[0].size + 1023) >> 10);
		goto failure;
	}

	v->num_raw_buffers = 1;

	printv("Capture buffer allocated\n");

	printv("Successful opened %s (%s)\n",
	       dev_name, driver_name);

	return &v->capture;

failure:
io_error:
	v4l_delete(&v->capture);

	return NULL;
}

vbi3_capture *
vbi3_capture_v4l_sidecar_new(const char *dev_name, int video_fd,
			    unsigned int *services, int strict,
			    char **errorstr, vbi3_bool trace)
{
	return v4l_new(dev_name, video_fd, 0,
		       services, strict, errorstr, trace);
}

vbi3_capture *
vbi3_capture_v4l_new(const char *dev_name, int scanning,
		    unsigned int *services, int strict,
		    char **errorstr, vbi3_bool trace)
{
	return v4l_new(dev_name, -1, scanning,
		       services, strict, errorstr, trace);
}

#else

vbi3_capture *
vbi3_capture_v4l_sidecar_new(const char *dev_name, int given_fd,
			    unsigned int *services, int strict,
			    char **errorstr, vbi3_bool trace)
{
  //	pthread_once (&vbi3_init_once, vbi3_init);
	_vbi3_asprintf(errorstr, _("V4L interface not compiled."));
	return NULL;
}

vbi3_capture *
vbi3_capture_v4l_new(const char *dev_name, int scanning,
		     unsigned int *services, int strict,
		     char **errorstr, vbi3_bool trace)
{
  //	pthread_once (&vbi3_init_once, vbi3_init);
	_vbi3_asprintf(errorstr, _("V4L interface not compiled."));
	return NULL;
}

#endif /* !ENABLE_V4L */
