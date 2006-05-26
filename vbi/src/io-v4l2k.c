/*
 *  libzvbi - V4L2 (version 2002-10) interface
 *
 *  Copyright (C) 2002-2003 Michael H. Schimek
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

static char rcsid[] = "$Id: io-v4l2k.c,v 1.2.2.18 2006-05-26 00:43:05 mschimek Exp $";

/*
 *  Around Oct-Nov 2002 the V4L2 API was revised for inclusion into
 *  Linux 2.5/2.6/3.0. There are a few subtle differences, in order to
 *  keep the source clean this interface has been forked off from the
 *  old V4L2 interface. "v4l2k" is no official designation, there is
 *  none, take it as v4l2-kernel or v4l-2000.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "misc.h"
#include "vbi.h"
#include "version.h"
#include "intl-priv.h"
#include "io-priv.h"

#ifdef ENABLE_V4L2

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
#include <asm/types.h>		/* for videodev2.h */
#include <pthread.h>

#ifndef HAVE_S64_U64
/* Linux 2.6.x/x86 defines them only if __GNUC__.
   They're required to compile videodev2.h. */
typedef int64_t __s64;
typedef uint64_t __u64;
#endif

#include "videodev2k.h"
#include "_videodev2k.h"

#define log_fp 0

/* This macro checks at compile time if the arg type is correct,
   repeats the ioctl if interrupted (EINTR), and logs the args
   and result if log_fp is non-zero. */
#define _ioctl(fd, cmd, arg)						\
(IOCTL_ARG_TYPE_CHECK_ ## cmd (arg),					\
 device_ioctl (log_fp, fprint_ioctl_arg, fd, cmd, (void *)(arg)))

#undef REQUIRE_SELECT
#undef REQUIRE_G_FMT		/* before S_FMT */
#undef REQUIRE_S_FMT		/* else accept current format */

#define printv(format, args...)						\
do {									\
	if (trace) {							\
		fprintf(stderr, format ,##args);			\
		fflush(stderr);						\
	}								\
} while (0)

typedef struct vbi3_capture_v4l2 {
	vbi3_capture		capture;

	int			fd;
	vbi3_bool		close_me;
	int			btype;			/* v4l2 stream type */
	vbi3_bool		streaming;
	vbi3_bool		select;
	int			enqueue;

	vbi3_raw_decoder		dec;

	double			time_per_frame;

	vbi3_capture_buffer	*raw_buffer;
	int			num_raw_buffers;

	vbi3_capture_buffer	sliced_buffer;

} vbi3_capture_v4l2;

static int
v4l2_stream(vbi3_capture *vc, vbi3_capture_buffer **raw,
	    vbi3_capture_buffer **sliced, struct timeval *timeout)
{
	vbi3_capture_v4l2 *v = PARENT(vc, vbi3_capture_v4l2, capture);
	struct v4l2_buffer vbuf;
	double time;

	memset (&vbuf, 0, sizeof (vbuf));

	if (v->enqueue == -2) {
		if (_ioctl (v->fd, VIDIOC_STREAMON, &v->btype) == -1)
			return -1;
	} else if (v->enqueue >= 0) {
		vbuf.type = v->btype;
		vbuf.memory = V4L2_MEMORY_MMAP;
		vbuf.index = v->enqueue;

		if (_ioctl (v->fd, VIDIOC_QBUF, &vbuf) == -1)
			return -1;
	}

	v->enqueue = -1;

	for (;;) {
		struct timeval tv;
		fd_set fds;
		int r;

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

	vbuf.type = v->btype;
	vbuf.memory = V4L2_MEMORY_MMAP;

	if (_ioctl (v->fd, VIDIOC_DQBUF, &vbuf) == -1)
		return -1;

	time = vbuf.timestamp.tv_sec
		+ vbuf.timestamp.tv_usec * (1 / 1e6);

	if (raw) {
		if (*raw) {
			(*raw)->size = v->raw_buffer[vbuf.index].size;
			memcpy((*raw)->data, v->raw_buffer[vbuf.index].data,
			       (*raw)->size);
		} else {
			*raw = v->raw_buffer + vbuf.index;
			v->enqueue = vbuf.index;
		}

		(*raw)->timestamp = time;
	}

	if (sliced) {
		int lines;

		if (*sliced) {
			lines = vbi3_raw_decoder_decode
			  (&v->dec, 
			   (vbi3_sliced *)(*sliced)->data,
			   /* FIXME */ 50,
			   v->raw_buffer[vbuf.index].data);
		} else {
			*sliced = &v->sliced_buffer;
			lines = vbi3_raw_decoder_decode
			  (&v->dec, 
			   (vbi3_sliced *)(v->sliced_buffer.data),
			   /* FIXME */ 50,
			   v->raw_buffer[vbuf.index].data);
		}

		(*sliced)->size = lines * sizeof(vbi3_sliced);
		(*sliced)->timestamp = time;
	}

	if (v->enqueue == -1) {
		if (_ioctl (v->fd, VIDIOC_QBUF, &vbuf) == -1)
			return -1;
	}

	return 1;
}

static int
v4l2_read(vbi3_capture *vc, vbi3_capture_buffer **raw,
	  vbi3_capture_buffer **sliced, struct timeval *timeout)
{
	vbi3_capture_v4l2 *v = PARENT(vc, vbi3_capture_v4l2, capture);
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

		if (r == -1  && (errno == EINTR || errno == ETIME))
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

static vbi3_raw_decoder *
v4l2_parameters(vbi3_capture *vc)
{
	vbi3_capture_v4l2 *v = PARENT(vc, vbi3_capture_v4l2, capture);

	return &v->dec;
}

static void
v4l2_delete(vbi3_capture *vc)
{
	vbi3_capture_v4l2 *v = PARENT(vc, vbi3_capture_v4l2, capture);

	if (v->sliced_buffer.data)
		vbi3_free(v->sliced_buffer.data);

	for (; v->num_raw_buffers > 0; v->num_raw_buffers--)
		if (v->streaming)
			munmap(v->raw_buffer[v->num_raw_buffers - 1].data,
			       v->raw_buffer[v->num_raw_buffers - 1].size);
		else
			vbi3_free(v->raw_buffer[v->num_raw_buffers - 1].data);

	if (v->close_me && v->fd != -1)
		device_close(log_fp, v->fd);

	vbi3_free(v);
}

static int
v4l2_fd(vbi3_capture *vc)
{
	vbi3_capture_v4l2 *v = PARENT(vc, vbi3_capture_v4l2, capture);

	return v->fd;
}

static void
print_vfmt(const char *s, struct v4l2_format *vfmt)
{
	fprintf(stderr, "%sformat %08x, %d Hz, %d bpl, offs %d, "
		"F1 %d+%d, F2 %d+%d, flags %08x\n", s,
		vfmt->fmt.vbi.sample_format,
		vfmt->fmt.vbi.sampling_rate,
		vfmt->fmt.vbi.samples_per_line,
		vfmt->fmt.vbi.offset,
		vfmt->fmt.vbi.start[0], vfmt->fmt.vbi.count[0],
		vfmt->fmt.vbi.start[1], vfmt->fmt.vbi.count[1],
		vfmt->fmt.vbi.flags);
}

/* document below */
vbi3_capture *
vbi3_capture_v4l2k_new		(const char *		dev_name,
				 int			fd,
				 int			buffers,
				 unsigned int *		services,
				 int			strict,
				 char **		errorstr,
				 vbi3_bool		trace)
{
	struct v4l2_capability vcap;
	struct v4l2_format vfmt;
	struct v4l2_requestbuffers vrbuf;
	struct v4l2_buffer vbuf;
	struct v4l2_standard vstd;
	v4l2_std_id stdid;
	const char *guess = "";
	vbi3_capture_v4l2 *v;
	int max_rate, g_fmt;
	int r;

	//	pthread_once (&vbi3_init_once, vbi3_init);

	assert(services && *services != 0);

	printv("Try to open v4l2 (2002-10) vbi device, libzvbi interface rev.\n"
	       "%s", rcsid);

	if (!(v = vbi3_malloc(sizeof(*v)))) {
		asprintf(errorstr, _("Virtual memory exhausted."));
		errno = ENOMEM;
		return NULL;
	}

	CLEAR (*v);

	v->capture.parameters = v4l2_parameters;
	v->capture._delete = v4l2_delete;
	v->capture.get_fd = v4l2_fd;

	if (dev_name) {
		if ((v->fd = device_open(log_fp, dev_name, O_RDWR, 0)) == -1) {
			asprintf(errorstr, _("Cannot open '%s': %d, %s."),
				     dev_name, errno, strerror(errno));
			goto io_error;
		}

		v->close_me = TRUE;

		printv("Opened %s\n", dev_name);
	} else {
		v->fd = fd;
		v->close_me = FALSE;

		printv("Using v4l2k device fd %d\n", fd);
	}

	if (_ioctl (v->fd, VIDIOC_QUERYCAP, &vcap) == -1) {
		asprintf(errorstr, _("Cannot identify '%s': %d, %s."),
			     dev_name, errno, strerror(errno));
		guess = _("Probably not a v4l2 device.");
		goto io_error;
	}

	if (!(vcap.capabilities & V4L2_CAP_VBI_CAPTURE)) {
		asprintf(errorstr, _("%s (%s) is not a raw vbi device."),
			     dev_name, vcap.card);
		goto failure;
	}

	printv("%s (%s) is a v4l2 vbi device,\n", dev_name, vcap.card);
	printv("driver %s, version 0x%08x\n", vcap.driver, vcap.version);

	v->select = TRUE; /* mandatory 2002-10 */

#ifdef REQUIRE_SELECT
	if (!v->select) {
		asprintf(errorstr, _("%s (%s) does not support the select() function."),
			     dev_name, vcap.card);
		goto failure;
	}
#endif

	if (-1 == _ioctl (v->fd, VIDIOC_G_STD, &stdid)) {
		asprintf(errorstr, _("Cannot query current videostandard of %s (%s): %d, %s."),
			     dev_name, vcap.card, errno, strerror(errno));
		guess = _("Probably a driver bug.");
		goto io_error;
	}

	vstd.index = 0;

	while (0 == (r = _ioctl (v->fd, VIDIOC_ENUMSTD, &vstd))) {
		if (vstd.id & stdid)
			break;
		vstd.index++;
	}

	if (-1 == r) {
		asprintf(errorstr, _("Cannot query current videostandard of %s (%s): %d, %s."),
			     dev_name, vcap.card, errno, strerror(errno));
		guess = _("Probably a driver bug.");
		goto io_error;
	}

	printv ("Current scanning system is %d\n", vstd.framelines);

        switch (vstd.framelines) {
        case 525:
		v->dec.sampling.videostd_set = VBI3_VIDEOSTD_SET_525_60;
                break;
        case 625:
                v->dec.sampling.videostd_set = VBI3_VIDEOSTD_SET_625_50;
                break;
        default:
                v->dec.sampling.videostd_set = VBI3_VIDEOSTD_SET_EMPTY;
                break;
        }

	memset(&vfmt, 0, sizeof(vfmt));

	vfmt.type = v->btype = V4L2_BUF_TYPE_VBI_CAPTURE;

	max_rate = 0;

	printv("Querying current vbi parameters... ");

	if ((g_fmt = _ioctl (v->fd, VIDIOC_G_FMT, &vfmt)) == -1) {
		printv("failed\n");
#ifdef REQUIRE_G_FMT
		asprintf(errorstr, _("Cannot query current vbi parameters of %s (%s): %d, %s."),
			     dev_name, vcap.card, errno, strerror(errno));
		goto io_error;
#else
		strict = MAX(0, strict);
#endif
	} else {
		printv("success\n");

		if (trace)
			print_vfmt("VBI capture parameters supported: ", &vfmt);
	}

	if (strict >= 0) {
		struct v4l2_format vfmt_temp = vfmt;

		printv("Attempt to set vbi capture parameters\n");

		*services = vbi3_sampling_par_from_services
		  (&v->dec.sampling, &max_rate,
		   v->dec.sampling.videostd_set, *services);

		if (*services == 0) {
			asprintf(errorstr, _("Sorry, %s (%s) cannot capture any of the "
					       "requested data services."), dev_name, vcap.card);
			goto failure;
		}

		vfmt.fmt.vbi.sample_format	= V4L2_PIX_FMT_GREY;
		vfmt.fmt.vbi.sampling_rate	= v->dec.sampling.sampling_rate;
		vfmt.fmt.vbi.samples_per_line	= v->dec.sampling.samples_per_line;
		vfmt.fmt.vbi.offset		= v->dec.sampling.offset;
		vfmt.fmt.vbi.start[0]		= v->dec.sampling.start[0];
		vfmt.fmt.vbi.count[0]		= v->dec.sampling.count[0];
		vfmt.fmt.vbi.start[1]		= v->dec.sampling.start[1];
		vfmt.fmt.vbi.count[1]		= v->dec.sampling.count[1];

		if (trace)
			print_vfmt("VBI capture parameters requested: ", &vfmt);

		if (_ioctl (v->fd, VIDIOC_S_FMT, &vfmt) == -1) {
			switch (errno) {
			case EBUSY:
#ifndef REQUIRE_S_FMT
				if (g_fmt != -1) {
					printv("VIDIOC_S_FMT returned EBUSY, "
					       "will try the current parameters\n");
					vfmt = vfmt_temp;
					break;
				}
#endif
				asprintf(errorstr, _("Cannot initialize %s (%s), "
						       "the device is already in use."),
					     dev_name, vcap.card);
				goto failure;

			default:
				asprintf(errorstr, _("Could not set the vbi capture parameters "
						       "for %s (%s): %d, %s."),
					     dev_name, vcap.card, errno, strerror(errno));
				guess = _("Possibly a driver bug.");
				goto io_error;
			}
		} else {
			printv("Successful set vbi capture parameters\n");
		}
	}

	if (trace)
		print_vfmt("VBI capture parameters granted: ", &vfmt);

	v->dec.sampling.sampling_rate		= vfmt.fmt.vbi.sampling_rate;
	v->dec.sampling.samples_per_line	= vfmt.fmt.vbi.samples_per_line;
	v->dec.sampling.bytes_per_line		= vfmt.fmt.vbi.samples_per_line;
	v->dec.sampling.offset			= vfmt.fmt.vbi.offset;
	v->dec.sampling.start[0] 		= vfmt.fmt.vbi.start[0];
	v->dec.sampling.count[0] 		= vfmt.fmt.vbi.count[0];
	v->dec.sampling.start[1] 		= vfmt.fmt.vbi.start[1];
	v->dec.sampling.count[1] 		= vfmt.fmt.vbi.count[1];
	v->dec.sampling.interlaced		= !!(vfmt.fmt.vbi.flags & V4L2_VBI_INTERLACED);
	v->dec.sampling.synchronous		= !(vfmt.fmt.vbi.flags & V4L2_VBI_UNSYNC);

	if (VBI3_VIDEOSTD_SET_525_60 & v->dec.sampling.videostd_set)
		v->time_per_frame = 1001.0 / 30000;
	else
		v->time_per_frame = 1.0 / 25;

 	if (vfmt.fmt.vbi.sample_format != V4L2_PIX_FMT_GREY) {
		asprintf(errorstr, _("%s (%s) offers unknown vbi sampling format #%d. "
				       "This may be a driver bug or libzvbi is too old."),
			     dev_name, vcap.card, vfmt.fmt.vbi.sample_format);
		goto failure;
	}

	v->dec.sampling.sampling_format = VBI3_PIXFMT_Y8;

	if (*services & ~(VBI3_SLICED_VBI3_525 | VBI3_SLICED_VBI3_625)) {
		/* Nyquist (we're generous at 1.5) */

		if (v->dec.sampling.sampling_rate < max_rate * 3 / 2) {
			asprintf(errorstr, _("Cannot capture the requested "
						 "data services with "
						 "%s (%s), the sampling frequency "
						 "%.2f MHz is too low."),
				     dev_name, vcap.card,
				     v->dec.sampling.sampling_rate / 1e6);
			goto failure;
		}

		printv("Nyquist check passed\n");

		printv("Request decoding of services 0x%08x\n", *services);

		*services = vbi3_raw_decoder_add_services(&v->dec, *services, strict);

		if (*services == 0) {
			asprintf(errorstr, _("Sorry, %s (%s) cannot capture any of "
					       "the requested data services."),
				     dev_name, vcap.card);
			goto failure;
		}

		v->sliced_buffer.data =
			vbi3_malloc((v->dec.sampling.count[0]
				+ v->dec.sampling.count[1])
			       * sizeof(vbi3_sliced));

		if (!v->sliced_buffer.data) {
			asprintf(errorstr, _("Virtual memory exhausted."));
			errno = ENOMEM;
			goto failure;
		}
	}

	printv("Will decode services 0x%08x\n", *services);

	/* FIXME - api revision */
	if (1 /* vcap.flags & V4L2_FLAG_STREAMING */) {
		printv("Using streaming interface\n");

		if (!v->select) {
			/* Mandatory; dequeue buffer is non-blocking. */
			asprintf(errorstr, _("%s (%s) does not support the select() function."),
				     dev_name, vcap.card);
			goto failure;
		}

		v->streaming = TRUE;
		v->enqueue = -2;

		v->capture.read = v4l2_stream;

		printv("Fifo initialized\nRequesting streaming i/o buffers\n");

		memset (&vrbuf, 0, sizeof (vrbuf));

		vrbuf.type = v->btype;
		vrbuf.memory = V4L2_MEMORY_MMAP;
		vrbuf.count = buffers;

		if (_ioctl (v->fd, VIDIOC_REQBUFS, &vrbuf) == -1) {
			asprintf(errorstr, _("Cannot request streaming i/o buffers "
					       "from %s (%s): %d, %s."),
				     dev_name, vcap.card, errno, strerror(errno));
			guess = _("Possibly a driver bug.");
			goto failure;
		}

		if (vrbuf.count == 0) {
			asprintf(errorstr, _("%s (%s) granted no streaming i/o buffers, "
					       "perhaps the physical memory is exhausted."),
				     dev_name, vcap.card);
			goto failure;
		}

		printv("Mapping %d streaming i/o buffers\n", vrbuf.count);

		v->raw_buffer = vbi3_malloc (vrbuf.count
					* sizeof(v->raw_buffer[0]));
		if (!v->raw_buffer) {
			asprintf(errorstr, _("Virtual memory exhausted."));
			errno = ENOMEM;
			goto failure;
		}

		memset (v->raw_buffer, 0,
			vrbuf.count * sizeof(v->raw_buffer[0]));

		v->num_raw_buffers = 0;

		while (v->num_raw_buffers < vrbuf.count) {
			uint8_t *p;

			vbuf.type = v->btype;
			vbuf.memory = V4L2_MEMORY_MMAP;
			vbuf.index = v->num_raw_buffers;

			if (_ioctl (v->fd, VIDIOC_QUERYBUF, &vbuf) == -1) {
				asprintf(errorstr, _("Querying streaming i/o buffer #%d "
						       "from %s (%s) failed: %d, %s."),
					     v->num_raw_buffers, dev_name, vcap.card,
					     errno, strerror(errno));
				goto mmap_failure;
			}

			/* bttv 0.8.x wants PROT_WRITE */
			p = mmap(NULL, vbuf.length, PROT_READ | PROT_WRITE,
				 MAP_SHARED, v->fd, vbuf.m.offset); /* MAP_PRIVATE ? */

			if (p == MAP_FAILED)
			  p = mmap(NULL, vbuf.length, PROT_READ,
				   MAP_SHARED, v->fd, vbuf.m.offset); /* MAP_PRIVATE ? */

			if (p == MAP_FAILED) {
				if (errno == ENOMEM && v->num_raw_buffers >= 2) {
					printv("Memory mapping buffer #%d failed: %d, %s (ignored).",
					       v->num_raw_buffers, errno, strerror(errno));
					break;
				}

				asprintf(errorstr, _("Memory mapping streaming i/o buffer #%d "
						       "from %s (%s) failed: %d, %s."),
					     v->num_raw_buffers, dev_name, vcap.card,
					     errno, strerror(errno));
				goto mmap_failure;
			} else {
				unsigned int i, s;

				v->raw_buffer[v->num_raw_buffers].data = p;
				v->raw_buffer[v->num_raw_buffers].size = vbuf.length;

				for (i = s = 0; i < vbuf.length; i++)
					s += p[i];

				if (s % vbuf.length) {
					fprintf(stderr,
					       "Security warning: driver %s (%s) seems to mmap "
					       "physical memory uncleared. Please contact the "
					       "driver author.\n", dev_name, vcap.card);
					exit(EXIT_FAILURE);
				}
			}

			if (_ioctl (v->fd, VIDIOC_QBUF, &vbuf) == -1) {
				asprintf(errorstr, _("Cannot enqueue streaming i/o buffer #%d "
						       "to %s (%s): %d, %s."),
					     v->num_raw_buffers, dev_name, vcap.card,
					     errno, strerror(errno));
				guess = _("Probably a driver bug.");
				goto mmap_failure;
			}

			v->num_raw_buffers++;
		}
	/* FIXME */
	} else if (0 /* vcap.flags & V4L2_FLAG_READ */) {
		printv("Using read interface\n");

		if (!v->select)
			printv("Warning: no read select, reading will block\n");

		v->capture.read = v4l2_read;

		v->raw_buffer = vbi3_malloc (sizeof(v->raw_buffer[0]));

		if (!v->raw_buffer) {
			asprintf(errorstr, _("Virtual memory exhausted."));
			errno = ENOMEM;
			goto failure;
		}

		CLEAR (v->raw_buffer[0]);

		v->raw_buffer[0].size = (v->dec.sampling.count[0]
					 + v->dec.sampling.count[1])
			* v->dec.sampling.bytes_per_line;

		v->raw_buffer[0].data = vbi3_malloc(v->raw_buffer[0].size);

		if (!v->raw_buffer[0].data) {
			asprintf(errorstr, _("Not enough memory to allocate "
					       "vbi capture buffer (%d KB)."),
				     (v->raw_buffer[0].size + 1023) >> 10);
			goto failure;
		}

		v->num_raw_buffers = 1;

		printv("Capture buffer allocated\n");
	} else {
		asprintf(errorstr, _("%s (%s) lacks a vbi read interface, "
				       "possibly an output only device or a driver bug."),
			     dev_name, vcap.card);
		goto failure;
	}

	printv("Successful opened %s (%s)\n",
	       dev_name, vcap.card);

	return &v->capture;

mmap_failure:
	for (; v->num_raw_buffers > 0; v->num_raw_buffers--)
		munmap(v->raw_buffer[v->num_raw_buffers - 1].data,
		       v->raw_buffer[v->num_raw_buffers - 1].size);

io_error:
failure:
	v4l2_delete(&v->capture);

	return NULL;
}

#else

/**
 * @param dev_name Name of the device to open, usually one of
 *   @c /dev/vbi or @c /dev/vbi0 and up.
 * @param buffers Number of device buffers for raw vbi data, when
 *   the driver supports streaming. Otherwise one bounce buffer
 *   is allocated for vbi3_capture_pull().
 * @param services This must point to a set of @ref VBI3_SLICED_
 *   symbols describing the
 *   data services to be decoded. On return the services actually
 *   decodable will be stored here. See vbi3_raw_decoder_add()
 *   for details. If you want to capture raw data only, set to
 *   @c VBI3_SLICED_VBI3_525, @c VBI3_SLICED_VBI3_625 or both.
 * @param strict Will be passed to vbi3_raw_decoder_add().
 * @param errorstr If not @c NULL this function stores a pointer to an error
 *   description here. You must free() this string when no longer needed.
 * @param trace If @c TRUE print progress messages on stderr.
 * 
 * @return
 * Initialized vbi3_capture context, @c NULL on failure.
 */
vbi3_capture *
vbi3_capture_v4l2k_new(const char *dev_name, int fd, int buffers,
		      unsigned int *services, int strict,
		      char **errorstr, vbi3_bool trace)
{
  //	pthread_once (&vbi3_init_once, vbi3_init);
	asprintf(errorstr, _("V4L2 interface not compiled."));
	return NULL;
}

#endif /* !ENABLE_V4L2 */
