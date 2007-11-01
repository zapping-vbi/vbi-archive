/*
 *  libzvbi - FreeBSD/OpenBSD bktr driver interface
 *
 *  Copyright (C) 2002 Michael H. Schimek
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

static const char rcsid [] =
"$Id: io-bktr.c,v 1.2.2.14 2007-11-01 00:21:23 mschimek Exp $";

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <pthread.h>
#include "misc.h"
#include "version.h"
#include "intl-priv.h"
#include "io.h"
#include "vbi.h"

#define printv(format, args...)						\
do {									\
	if (trace) {							\
		fprintf(stderr, format ,##args);			\
		fflush(stderr);						\
	}								\
} while (0)

#ifdef ENABLE_BKTR

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>
#include <sys/time.h>		/* timeval */
#include <sys/select.h>		/* fd_set */
#include <pthread.h>

typedef struct vbi3_capture_bktr {
	vbi3_capture		capture;

	int			fd;
	vbi3_bool		select;

	vbi3_raw_decoder		dec;

	double			time_per_frame;

	vbi3_capture_buffer	*raw_buffer;
	int			num_raw_buffers;

	vbi3_capture_buffer	sliced_buffer;

} vbi3_capture_bktr;

static int
bktr_read			(vbi3_capture *		vc,
				 vbi3_capture_buffer **	raw,
				 vbi3_capture_buffer **	sliced,
				 const struct timeval *	timeout)
{
	vbi3_capture_bktr *v = PARENT(vc, vbi3_capture_bktr, capture);
	vbi3_capture_buffer *my_raw = v->raw_buffer;
	struct timeval tv;
	int r;

	while (v->select) {
		fd_set fds;

		FD_ZERO(&fds);
		FD_SET(v->fd, &fds);

		tv = *timeout; /* kernel overwrites this? */

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

		if (r == -1 && errno == EINTR)
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
			lines = vbi3_raw_decode(&v->dec, (*raw)->data,
					       (vbi3_sliced *)(*sliced)->data);
		} else {
			*sliced = &v->sliced_buffer;
			lines = vbi3_raw_decode(&v->dec, (*raw)->data,
					       (vbi3_sliced *)(v->sliced_buffer.data));
		}

		(*sliced)->size = lines * sizeof(vbi3_sliced);
		(*sliced)->timestamp = (*raw)->timestamp;
	}

	return 1;
}

static vbi3_raw_decoder *
bktr_parameters(vbi3_capture *vc)
{
	vbi3_capture_bktr *v = PARENT(vc, vbi3_capture_bktr, capture);

	return &v->dec;
}

static void
bktr_delete(vbi3_capture *vc)
{
	vbi3_capture_bktr *v = PARENT(vc, vbi3_capture_bktr, capture);

	if (v->sliced_buffer.data)
		vbi3_free(v->sliced_buffer.data);

	for (; v->num_raw_buffers > 0; v->num_raw_buffers--)
		vbi3_free(v->raw_buffer[v->num_raw_buffers - 1].data);

	vbi3_raw_decoder_destroy (&v->dec);

	if (-1 != v->fd)
		device_close (v->capture.sys_log_fp, v->fd);

	vbi3_free(v);
}

static int
bktr_fd(vbi3_capture *vc)
{
	vbi3_capture_bktr *v = PARENT(vc, vbi3_capture_bktr, capture);

	return v->fd;
}

vbi3_capture *
vbi3_capture_bktr_new		(const char *		dev_name,
				 int			scanning,
				 unsigned int *		services,
				 int			strict,
				 char **		errstr,
				 vbi3_bool		trace)
{
	char *error = NULL;
	char *driver_name = _("BKTR driver");
	vbi3_capture_bktr *v;

	pthread_once (&vbi3_init_once, vbi3_init);

	assert(services && *services != 0);

	if (!errstr)
		errstr = &error;
	*errstr = NULL;

	printv ("Try to open bktr vbi device, "
		"libzvbi interface rev.\n  %s\n", rcsid);

	if (!(v = (vbi3_capture_bktr *) calloc(1, sizeof(*v)))) {
		asprintf(errstr, _("Virtual memory exhausted."));
		errno = ENOMEM;
		goto failure;
	}

	vbi3_raw_decoder_init (&v->dec);

	v->capture.parameters = bktr_parameters;
	v->capture._delete = bktr_delete;
	v->capture.get_fd = bktr_fd;

	v->fd = device_open (v->capture.sys_log_fp, dev_name, O_RDONLY, 0);
	if (-1 == v->fd) {
		asprintf(errstr, _("Cannot open '%s': %s."),
			 dev_name, strerror(errno));
		goto io_error;
	}

	printv("Opened %s\n", dev_name);

	/*
	 *  XXX
	 *  Can we somehow verify this really is /dev/vbiN (bktr) and not
	 *  /dev/hcfr (halt and catch fire on read) ?
	 */

	v->dec.bytes_per_line = 2048;
	v->dec.interlaced = FALSE;
	v->dec.synchronous = TRUE;

	v->dec.count[0]	= 16;
	v->dec.count[1] = 16;

	switch (scanning) {
	default:
		/* fall through */

	case 625:
		/* Not confirmed */
		v->dec.scanning = 625;
		v->dec.sampling_rate = 35468950;
		v->dec.offset = (int)(10.2e-6 * 35468950);
		v->dec.start[0] = 22 + 1 - v->dec.count[0];
		v->dec.start[1] = 335 + 1 - v->dec.count[1];
		break;

	case 525:
		/* Not confirmed */
		v->dec.scanning = 525;
		v->dec.sampling_rate = 28636363;
		v->dec.offset = (int)(9.2e-6 * 28636363);
		v->dec.start[0] = 10;
		v->dec.start[1] = 273;
		break;
	}

	v->time_per_frame =
		(v->dec.scanning == 625) ? 1.0 / 25 : 1001.0 / 30000;

	v->select = FALSE; /* XXX ? */

	printv("Guessed videostandard %d\n", v->dec.scanning);

	v->dec.sampling_format = VBI3_PIXFMT_YUV420;

	if (*services & ~(VBI3_SLICED_VBI3_525 | VBI3_SLICED_VBI3_625)) {
		*services = vbi3_raw_decoder_add_services (&v->dec, *services, strict);

		if (*services == 0) {
			asprintf(errstr, _("Sorry, %s (%s) cannot "
					   "capture any of "
					   "the requested data services."),
				 dev_name, driver_name);
			goto failure;
		}

		v->sliced_buffer.data =
			vbi3_malloc((v->dec.count[0] + v->dec.count[1])
			       * sizeof(vbi3_sliced));

		if (!v->sliced_buffer.data) {
			asprintf(errstr, _("Virtual memory exhausted."));
			errno = ENOMEM;
			goto failure;
		}
	}

	printv("Will decode services 0x%08x\n", *services);

	/* Read mode */

	if (!v->select)
		printv("Warning: no read select, reading will block\n");

	v->capture.read = bktr_read;

	v->raw_buffer = calloc(1, sizeof(v->raw_buffer[0]));

	if (!v->raw_buffer) {
		asprintf(errstr, _("Virtual memory exhausted."));
		errno = ENOMEM;
		goto failure;
	}

	v->raw_buffer[0].size = (v->dec.count[0] + v->dec.count[1])
		* v->dec.bytes_per_line;

	v->raw_buffer[0].data = vbi3_malloc(v->raw_buffer[0].size);

	if (!v->raw_buffer[0].data) {
		asprintf(errstr, _("Not enough memory to allocate "
				   "vbi capture buffer (%d KB)."),
			 (v->raw_buffer[0].size + 1023) >> 10);
		goto failure;
	}

	v->num_raw_buffers = 1;

	printv("Capture buffer allocated\n");

	printv("Successful opened %s (%s)\n",
	       dev_name, driver_name);

	if (errstr == &error) {
		vbi3_free (error);
		error = NULL;
	}

	return &v->capture;

failure:
io_error:
	if (v)
		bktr_delete(&v->capture);

	if (errstr == &error) {
		vbi3_free (error);
		error = NULL;
	}

	return NULL;
}

#else

/**
 * @param dev_name Name of the device to open.
 * @param scanning The current video standard. Value is 625
 *   (PAL/SECAM family) or 525 (NTSC family).
 * @param services This must point to a set of @ref VBI3_SLICED_
 *   symbols describing the
 *   data services to be decoded. On return the services actually
 *   decodable will be stored here. See vbi3_raw_decoder_add()
 *   for details. If you want to capture raw data only, set to
 *   @c VBI3_SLICED_VBI3_525, @c VBI3_SLICED_VBI3_625 or both.
 * @param strict Will be passed to vbi3_raw_decoder_add().
 * @param errstr If not @c NULL this function stores a pointer to an error
 *   description here. You must free() this string when no longer needed.
 * @param trace If @c TRUE print progress messages on stderr.
 * 
 * @bug You must enable continuous video capturing to read VBI data from
 * the bktr driver, using an RGB video format, and the VBI device must be
 * opened before video capturing starts (METEORCAPTUR).
 * 
 * @return
 * Initialized vbi3_capture context, @c NULL on failure.
 */
vbi3_capture *
vbi3_capture_bktr_new		(const char *		dev_name,
				 int			scanning,
				 unsigned int *		services,
				 int			strict,
				 char **		errstr,
				 vbi3_bool		trace)
{
	dev_name = dev_name;
	scanning = scanning;
	services = services;
	strict = strict;
	trace = trace;

#if 2 == VBI_VERSION_MINOR
	pthread_once (&vbi3_init_once, vbi3_init);
#endif

	printv ("Libzvbi bktr interface rev.\n  %s\n", rcsid);

	if (errstr)
		asprintf (errstr,
			  _("BKTR driver interface not compiled."));

	return NULL;
}

#endif /* !ENABLE_BKTR */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
