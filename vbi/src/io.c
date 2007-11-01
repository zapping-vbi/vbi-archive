/*
 *  libzvbi - Device interfaces
 *
 *  Copyright (C) 2002, 2004 Michael H. Schimek
 *  Copyright (C) 2003-2004 Tom Zoerner
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

/* $Id: io.c,v 1.4.2.6 2007-11-01 00:21:23 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <assert.h>

#include <fcntl.h>		/* open() */
#include <unistd.h>		/* close(), mmap(), munmap(), gettimeofday() */
#include <sys/ioctl.h>		/* ioctl() */
#include <sys/mman.h>		/* mmap(), munmap() */
#include <sys/time.h>		/* struct timeval */
#include <sys/types.h>
#include <errno.h>

#include "io.h"
#include "io-priv.h"

/* Preliminary hack for tests. */
vbi3_bool vbi3_capture_force_read_mode = FALSE;

/**
 * @addtogroup Device VBI capture device interface
 * @ingroup Raw
 * @brief Platform independent interface to VBI capture device drivers
 */

/**
 * @param capture Initialized vbi3_capture context.
 * @param data Store the raw vbi data here. Use vbi3_capture_parameters() to
 *   determine the buffer size.
 * @param timestamp On success the capture instant in seconds and fractions
 *   since 1970-01-01 00:00 of the video frame will be stored here.
 * @param timeout Wait timeout, will be read only.
 * 
 * Read a raw vbi frame from the capture device.
 * 
 * @return
 * -1 on error, examine @c errno for details. The function also fails if
 * vbi data is not available in raw format. 0 on timeout, 1 on success.
 */
int
vbi3_capture_read_raw(vbi3_capture *capture, void *data,
		     double *timestamp, struct timeval *timeout)
{
	vbi3_capture_buffer buffer, *bp = &buffer;
	int r;

	assert (capture != NULL);
	assert (timestamp != NULL);
	assert (timeout != NULL);

	buffer.data = data;

	if ((r = capture->read(capture, &bp, NULL, timeout)) > 0)
		*timestamp = buffer.timestamp;

	return r;
}

/**
 * @param capture Initialized vbi capture context.
 * @param data Stores the sliced vbi data here. Use vbi3_capture_parameters() to
 *   determine the buffer size.
 * @param lines Stores number of vbi lines decoded and stored in @a data,
 *   which can be zero, here.
 * @param timestamp On success the capture instant in seconds and fractions
 *   since 1970-01-01 00:00 will be stored here.
 * @param timeout Wait timeout, will be read only.
 * 
 * Read a sliced vbi frame, that is an array of vbi3_sliced structures,
 * from the capture device. 
 *
 * Note: it's generally more efficient to use vbi3_capture_pull_sliced()
 * instead, as that one may avoid having to copy sliced data into the
 * given buffer (e.g. for the VBI proxy)
 * 
 * @return
 * -1 on error, examine @c errno for details. 0 on timeout, 1 on success.
 */
int
vbi3_capture_read_sliced(vbi3_capture *capture, vbi3_sliced *data, int *lines,
			double *timestamp, struct timeval *timeout)
{
	vbi3_capture_buffer buffer, *bp = &buffer;
	int r;

	assert (capture != NULL);
	assert (lines != NULL);
	assert (timestamp != NULL);
	assert (timeout != NULL);

	buffer.data = data;

	if ((r = capture->read(capture, NULL, &bp, timeout)) > 0) {
		*lines = ((unsigned int) buffer.size) / sizeof(vbi3_sliced);
		*timestamp = buffer.timestamp;
	}

	return r;
}

/**
 * @param capture Initialized vbi capture context.
 * @param raw_data Stores the raw vbi data here. Use vbi3_capture_parameters()
 *   to determine the buffer size.
 * @param sliced_data Stores the sliced vbi data here. Use
 *   vbi3_capture_parameters() to determine the buffer size.
 * @param lines Stores number of vbi lines decoded and stored in @a data,
 *   which can be zero, here.
 * @param timestamp On success the capture instant in seconds and fractions
 *   since 1970-01-01 00:00 will be stored here.
 * @param timeout Wait timeout, will be read only.
 * 
 * Read a raw vbi frame from the capture device, decode to sliced data
 * and also read the sliced vbi frame, that is an array of vbi3_sliced
 * structures, from the capture device.
 *
 * Note: depending on the driver, captured raw data may have to be copied
 * from the capture buffer into the given buffer (e.g. for v4l2 streams which
 * use memory mapped buffers.)  It's generally more efficient to use one of
 * the vbi3_capture_pull() interfaces, especially if you don't require access
 * to raw data at all.
 * 
 * @return
 * -1 on error, examine @c errno for details. The function also fails if
 * vbi data is not available in raw format. 0 on timeout, 1 on success.
 */
int
vbi3_capture_read(vbi3_capture *capture, void *raw_data,
		 vbi3_sliced *sliced_data, int *lines,
		 double *timestamp, struct timeval *timeout)
{
	vbi3_capture_buffer rbuffer, *rbp = &rbuffer;
	vbi3_capture_buffer sbuffer, *sbp = &sbuffer;
	int r;

	assert (capture != NULL);
	assert (lines != NULL);
	assert (timestamp != NULL);
	assert (timeout != NULL);

	rbuffer.data = raw_data;
	sbuffer.data = sliced_data;

	if ((r = capture->read(capture, &rbp, &sbp, timeout)) > 0) {
		*lines = ((unsigned int) sbuffer.size) / sizeof(vbi3_sliced);
		*timestamp = sbuffer.timestamp;
	}

	return r;
}

/**
 * @param capture Initialized vbi capture context.
 * @param buffer Store pointer to a vbi3_capture_buffer here.
 * @param timeout Wait timeout, will be read only.
 * 
 * Read a raw vbi frame from the capture device, returning a
 * pointer to the image in @a buffer->data, which has @a buffer->size.
 * The data remains valid until the next
 * vbi3_capture_pull_raw() call and must be read only.
 * 
 * @return
 * -1 on error, examine @c errno for details. The function also fails
 * if vbi data is not available in raw format. 0 on timeout, 1 on success.
 */
int
vbi3_capture_pull_raw(vbi3_capture *capture, vbi3_capture_buffer **buffer,
		     struct timeval *timeout)
{
	assert (capture != NULL);
	assert (buffer != NULL);
	assert (timeout != NULL);

	*buffer = NULL;

	return capture->read(capture, buffer, NULL, timeout);
}

/**
 * @param capture Initialized vbi capture context.
 * @param buffer Store pointer to a vbi3_capture_buffer here.
 * @param timeout Wait timeout, will be read only.
 * 
 * Read a sliced vbi frame, that is an array of vbi3_sliced,
 * from the capture device, returning a pointer to the array as
 * @a buffer->data. @a buffer->size is the size of the array, that is
 * the number of lines decoded, which can be zero, <i>times the size
 * of structure vbi3_sliced</i>. The data remains valid until the
 * next vbi3_capture_pull_sliced() call and must be read only.
 * 
 * @return
 * -1 on error, examine @c errno for details. 0 on timeout, 1 on success.
 */
int
vbi3_capture_pull_sliced(vbi3_capture *capture, vbi3_capture_buffer **buffer,
			struct timeval *timeout)
{
	assert (capture != NULL);
	assert (buffer != NULL);
	assert (timeout != NULL);

	*buffer = NULL;

	return capture->read(capture, NULL, buffer, timeout);
}

/**
 * @param capture Initialized vbi capture context.
 * @param raw_buffer Store pointer to a vbi3_capture_buffer here.
 * @param sliced_buffer Store pointer to a vbi3_capture_buffer here.
 * @param timeout Wait timeout, will be read only.
 * 
 * Read a raw vbi frame from the capture device and decode to sliced
 * data. Both raw and sliced data is returned, a pointer to the raw image
 * as raw_buffer->data and a pointer to an array of vbi3_sliced as
 * sliced_buffer->data. Note sliced_buffer->size is the size of the array
 * in bytes. That is the number of lines decoded, which can be zero,
 * times the size of the vbi3_sliced structure.
 *
 * The raw and sliced data remains valid
 * until the next vbi3_capture_pull() call and must be read only.
 * 
 * @return
 * -1 on error, examine @c errno for details. The function also fails
 * if vbi data is not available in raw format. 0 on timeout, 1 on success.
 */
int
vbi3_capture_pull(vbi3_capture *capture, vbi3_capture_buffer **raw_buffer,
		 vbi3_capture_buffer **sliced_buffer, struct timeval *timeout)
{
	assert (capture != NULL);
	assert (timeout != NULL);

	if (raw_buffer)
		*raw_buffer = NULL;
	if (sliced_buffer)
		*sliced_buffer = NULL;

	return capture->read(capture, raw_buffer, sliced_buffer, timeout);
}

vbi3_bool
vbi3_capture_sampling_point	(vbi3_capture *		cap,
				 vbi3_bit_slicer_point *point,
				 unsigned int		row,
				 unsigned int		nth_bit)
{
	assert (NULL != cap);

	if (NULL == cap->sampling_point)
		return FALSE;
	else
		return cap->sampling_point (cap, point, row, nth_bit);
}

vbi3_bool
vbi3_capture_debug		(vbi3_capture *		cap,
				 vbi3_bool		enable)
{
	assert (NULL != cap);

	if (NULL == cap->debug)
		return FALSE;
	else
		return cap->debug (cap, enable);
}

/**
 * @param capture Initialized vbi capture context.
 * 
 * Describe the captured data. Raw vbi frames consist of
 * vbi3_raw_decoder.count[0] + vbi3_raw_decoder.count[1] lines in
 * vbi3_raw_decoder.sampling_format, each vbi3_raw_decoder.bytes_per_line.
 * Sliced vbi arrays consist of zero to
 * vbi3_raw_decoder.count[0] + vbi3_raw_decoder.count[1] vbi3_sliced
 * structures.
 * 
 * @return
 * Pointer to a vbi3_raw_decoder structure, read only.
 **/
#if 3 == VBI_VERSION_MINOR
const vbi3_sampling_par *
vbi3_capture_parameters(vbi3_capture *capture)
{
	assert (capture != NULL);

	return capture->parameters(capture);
}
#else
vbi3_raw_decoder *
vbi3_capture_parameters(vbi3_capture *capture)
{
	assert (capture != NULL);

	return capture->parameters(capture);
}
#endif

/**
 * @param capture Initialized vbi capture context.
 * @param reset @c TRUE to clear all previous services before adding
 *   new ones (by invoking vbi3_raw_decoder_reset() at the appropriate
 *   time.)
 * @param commit @c TRUE to apply all previously added services to
 *   the device; when doing subsequent calls of this function,
 *   commit should be set @c TRUE for the last call.  Reading data
 *   cannot continue before changes were commited (because capturing
 *   has to be suspended to allow resizing the VBI image.)  Note this
 *   flag is ignored when using the VBI proxy.
 * @param services This must point to a set of @ref VBI3_SLICED_
 *   symbols describing the
 *   data services to be decoded. On return the services actually
 *   decodable will be stored here. See vbi3_raw_decoder_add()
 *   for details. If you want to capture raw data only, set to
 *   @c VBI3_SLICED_VBI3_525, @c VBI3_SLICED_VBI3_625 or both.
 * @param strict Will be passed to vbi3_raw_decoder_add().
 * @param errorstr If not @c NULL this function stores a pointer to an error
 *   description here. You must free() this string when no longer needed.
 *
 * Add and/or remove one or more services to an already initialized capture
 * context.  Can be used to dynamically change the set of active services.
 * Internally the function will restart parameter negotiation with the
 * VBI device driver and then call vbi3_raw_decoder_add_services().
 * You may call vbi3_raw_decoder_reset() before using this function
 * to rebuild your service mask from scratch.  Note that the number of
 * VBI lines may change with this call (even if a negative result is
 * returned) so you have to check the size of your buffers.
 *
 * @return
 * Bitmask of supported services among those requested (not including
 * previously added services), 0 upon errors.
 */
unsigned int
vbi3_capture_update_services(vbi3_capture *capture,
			    vbi3_bool reset, vbi3_bool commit,
			    unsigned int services, int strict,
			    char ** errorstr)
{
	assert (capture != NULL);

	return capture->update_services(capture, reset, commit,
					services, strict, errorstr);
}

/**
 * @param capture Initialized vbi capture context, can be @c NULL.
 * 
 * @return
 * The file descriptor used to read from the device. If not
 * applicable (e.g. when using the proxy) or the @a capture context is
 * invalid -1 will be returned.
 */
int
vbi3_capture_fd(vbi3_capture *capture)
{
	if (capture && (capture->get_fd != NULL))
		return capture->get_fd(capture);
	else
		return -1;
}

#warning document me
void
vbi3_capture_set_log_fn		(vbi3_capture *		cap,
				 vbi3_log_mask		mask,
				 vbi3_log_fn *		log_fn,
				 void *			user_data)
{
	assert (NULL != cap);

	if (NULL == log_fn)
		mask = 0;

	cap->log.mask = mask;
	cap->log.fn = log_fn;
	cap->log.user_data = user_data;
}

/**
 * @param capture Initialized vbi capture context.
 *
 * @brief Queries the capture device for the current norm
 *
 * This function is intended to allow the application to check for
 * asynchronous norm changes, i.e. by a different application using
 * the same device.
 *
 * @return
 * Value 625 for PAL/SECAM norms, 525 for NTSC;
 * 0 if unknown, -1 on error.
 */
int
vbi3_capture_get_scanning(vbi3_capture *capture)
{
	if (capture && (capture->get_scanning != NULL))
		return capture->get_scanning(capture);
	else
		return -1;
}

/**
 * @param capture Initialized vbi capture context.
 *
 * After a channel change this function should be used to discard all
 * VBI data in intermediate buffers which may still originate from the
 * previous channel.
 */
void
vbi3_capture_flush(vbi3_capture *capture)
{
	assert (capture != NULL);

	if (capture->flush != NULL) {
		capture->flush(capture);
        }
}

/**
 * @param capture Initialized vbi capture context.
 * @param p_dev_video Path to a video device (e.g. /dev/video) which
 *   refers to the same hardware as the VBI device which is used for
 *   capturing.  Note: only useful for old video4linux drivers which
 *   don't support norm queries through VBI devices.
 * 
 * @brief Set path to video device for TV norm queries
 *
 * @return
 * Returns @c TRUE if the configuration option and parameters are
 * supported; else @c FALSE.
 */
vbi3_bool
vbi3_capture_set_video_path(vbi3_capture *capture, const char * p_dev_video)
{
	assert (capture != NULL);

	if (capture->set_video_path != NULL)
		return capture->set_video_path(capture, p_dev_video);
	else
		return FALSE;
}

/**
 * @param capture Initialized vbi capture context.
 * 
 * @brief Query properties of the capture device file handle
 */
VBI3_CAPTURE_FD_FLAGS
vbi3_capture_get_fd_flags(vbi3_capture *capture)
{
	assert (capture != NULL);

	if (capture->get_fd_flags != NULL)
		return capture->get_fd_flags(capture);
	else
		return 0;
}

/**
 * @param capture Initialized vbi capture context, can be @c NULL.
 * 
 * Free all resources associated with the @a capture context.
 */
void
vbi3_capture_delete(vbi3_capture *capture)
{
	if (capture)
		capture->_delete(capture);
}

static __inline__ void
timeval_subtract		(struct timeval *	delta,
				 const struct timeval *	tv1,
				 const struct timeval *	tv2)
{
	if (tv1->tv_usec < tv2->tv_usec) {
		delta->tv_sec = tv1->tv_sec - tv2->tv_sec - 1;
		delta->tv_usec = 1000000 + tv1->tv_usec - tv2->tv_usec;
	} else {
		delta->tv_sec = tv1->tv_sec - tv2->tv_sec;
		delta->tv_usec = tv1->tv_usec - tv2->tv_usec;
	}
}

/**
 * @internal
 *
 * @param timeout Timeout value given to select, will be reduced by the
 *   difference since start time.
 * @param tv_start Actual time before select() was called
 *
 * @brief Substract time spent waiting in select from a given
 *   max. timeout struct
 *
 * This functions is intended for functions which call select() repeatedly
 * with a given overall timeout.  After each select() call the time already
 * spent in waiting has to be substracted from the timeout. (Note that we don't
 * use the Linux select(2) feature to return the time not slept in the timeout
 * struct, because that's not portable.)
 * 
 * @return
 * No direct return; modifies timeout value in the struct pointed to by the
 * second pointer argument as described above.
 */
void
vbi3_capture_io_update_timeout	(struct timeval *	timeout,
				 const struct timeval *	tv_start)
				 
{
	struct timeval delta;
	struct timeval tv_stop;
        int            errno_saved;

        errno_saved = errno;
	gettimeofday(&tv_stop, NULL);
        errno = errno_saved;

	/* first calculate difference between start and current time */
	timeval_subtract (&delta, &tv_stop, tv_start);

	if ((delta.tv_sec | delta.tv_usec) < 0) {
		delta.tv_sec = 0;
		delta.tv_usec = 0;
	} else {
		/* substract delta from the given max. timeout */
		timeval_subtract (timeout, timeout, &delta);

		/* check if timeout was underrun -> set rest timeout to zero */
		if ((timeout->tv_sec | timeout->tv_usec) < 0) {
			timeout->tv_sec  = 0;
			timeout->tv_usec = 0;
		}
	}
}


/**
 * @internal
 *
 * @param fd file handle
 * @param timeout maximum time to wait; when the function returns the
 *   value is reduced by the time spent waiting.
 *
 * @brief Waits in select() for the given file handle to become readable.
 *
 * If the syscall is interrupted by an interrupt, the select() call
 * is repeated with a timeout reduced by the time already spent
 * waiting.
 */
int
vbi3_capture_io_select		(int			fd,
				 struct timeval *	timeout)
{
	struct timeval tv_start;
	struct timeval tv;
	fd_set fds;
	int ret;

	while (1) {
		FD_ZERO(&fds);
		FD_SET(fd, &fds);

		tv = *timeout; /* Linux kernel overwrites this */
		gettimeofday(&tv_start, NULL);

		ret = select(fd + 1, &fds, NULL, NULL, &tv);

		vbi3_capture_io_update_timeout (timeout, &tv_start);

		if ((ret < 0) && (errno == EINTR))
			continue;

		return ret;
	}
}

/* Helper functions to log the communication between the library and drivers.
   FIXME remove fp arg, call user log function instead (0.3). */

#define MODE_GUESS	0
#define MODE_ENUM	1
#define MODE_SET_FLAGS	2
#define MODE_ALL_FLAGS	3

/**
 * @internal
 * @param mode
 *   - GUESS if value is enumeration or flags (used by structpr.pl)
 *   - ENUM interpret value as an enumerated item
 *   - SET_FLAGS interpret value as a set of flags, print set ones
 *   - ALL_FLAGS interpret value as a set of flags, print all
 * @param value
 * @param ... vector of symbol (const char *) and value
 *   (unsigned long) pairs.  Last parameter must be NULL.
 */
size_t
snprint_symbolic		(char *			str,
				 size_t			size,
				 int			mode,
				 unsigned long		value,
				 ...)
{
	unsigned int i, j = 0;
	unsigned long v;
	const char *s;
	va_list ap;
	size_t n;

	assert (size > 0);

	n = 0;

	if (mode == 0) {
		unsigned int n[2] = { 0, 0 };

		va_start (ap, value);

		while ((s = va_arg (ap, const char *))) {
			v = va_arg (ap, unsigned long);
			n[0 == (v & (v - 1))]++; /* single bit? */
		}

		mode = MODE_ENUM + (n[1] > n[0]);

		va_end (ap); 
	}

	va_start (ap, value);

	for (i = 0; (s = va_arg (ap, const char *)); ++i) {
		v = va_arg (ap, unsigned long);

		if (v == value
		    || MODE_ALL_FLAGS == mode
		    || (MODE_SET_FLAGS == mode && 0 != (v & value))) {
			if (j++ > 0
			    && n < size - 1) {
				str[n++] = '|';
			}
			if (MODE_ALL_FLAGS == mode
			    && 0 == (v & value)
			    && n < size - 1) {
				str[n++] = '!';
			}
			n += strlcpy (str + n, s, size - n);
			n = MIN (n, size);
			value &= ~v;
		}
	}

	if (0 == value
	    && 0 == j
	    && n < size - 1) {
		str[n++] = '0';
		str[n] = 0;
	} else if (value) {
		n = snprintf (str + n, size - n,
			      "%s0x%lx", j ? "|" : "", value);
		n = MIN (n, size - 1);
	}

	va_end (ap);

	return n;
}

/**
 * @internal
 * Used by function printing ioctl arguments generated by structpr.pl.
 */
size_t
snprint_unknown_ioctl		(char *			str,
				 size_t			size,
				 unsigned int		cmd,
				 void *			arg)
{
	size_t n;

	n = snprintf (str, size,
		      "<unknown cmd 0x%x %c%c arg=%p size=%u>",
		      cmd, IOCTL_READ (cmd) ? 'R' : '-',
		      IOCTL_WRITE (cmd) ? 'W' : '-',
		      arg, IOCTL_ARG_SIZE (cmd)); 

	return MIN (n, size - 1);
}

/**
 * @internal
 * Drop-in for open(). Logs the request on fp if not NULL.
 */
int
_vbi3_log_open			(_vbi3_log_hook *	log,
				 const char *		context,
				 const char *		pathname,
				 int			flags,
				 mode_t			mode)
{
	int fd;

	assert (NULL != log);

	fd = open (pathname, flags, mode);

	if (log->mask & VBI3_LOG_DRIVER) {
		char buffer[1024];
		int saved_errno;
		size_t size;
		size_t n;

		saved_errno = errno;

		size = sizeof (buffer);

		n = snprintf (buffer, size,
			      "%d = open (\"%s\", ", fd, pathname);
		n = MIN (n, size);
		n += snprint_symbolic (buffer + n, size - n,
				       MODE_SET_FLAGS, flags,
				       "RDONLY", O_RDONLY,
				       "WRONLY", O_WRONLY,
				       "RDWR", O_RDWR,
				       "CREAT", O_CREAT,
				       "EXCL", O_EXCL,
				       "TRUNC", O_TRUNC,
				       "APPEND", O_APPEND,
				       "NONBLOCK", O_NONBLOCK,
				       0);
		n += snprintf (buffer + n, size - n,
			       ", 0%o)", mode);
		n = MIN (n, size);

		if (-1 == fd) {
			snprintf (buffer + n, size - n,
				  ", errno=%d (%s)",
				  saved_errno, strerror (saved_errno));
		}

		log->fn (VBI3_LOG_DRIVER, context,
			 buffer, log->user_data);

		errno = saved_errno;
	}

	return fd;
}

/**
 * @internal
 * Drop-in for close(). Logs the request on fp if not NULL.
 */
int
_vbi3_log_close			(_vbi3_log_hook *	log,
				 const char *		context,
				 int			fd)
{
	int err;

	assert (NULL != log);

	err = close (fd);

	if (log->mask & VBI3_LOG_DRIVER) {
		int saved_errno;

		saved_errno = errno;

		if (-1 == err) {
			_vbi3_log_printf (log->fn,
					  log->user_data,
					  VBI3_LOG_DRIVER,
					  __FILE__,
					  context,
					  "%d = close (%d), errno=%d (%s)",
					  err, fd, saved_errno,
					  strerror (saved_errno));
		} else {
			_vbi3_log_printf (log->fn,
					  log->user_data,
					  VBI3_LOG_DRIVER,
					  __FILE__,
					  context,
					  "%d = close (%d)",
					  err, fd);
		}

		errno = saved_errno;
	}

	return err;
}

/**
 * @internal
 * Drop-in for ioctl(). Logs the request on fp if not NULL, repeats
 * the ioctl if interrupted (EINTR). You must supply a function
 * printing the arguments, structpr.pl generates one for you
 * from a header file.
 */
int
_vbi3_log_ioctl			(_vbi3_log_hook *	log,
				 const char *		context,
				 ioctl_log_fn *		log_fn,
				 int			fd,
				 unsigned int		cmd,
				 void *			arg)
{
	int buf[256];
	int err;

	assert (NULL != log);
	assert (NULL != log_fn);

	if ((log->mask & VBI3_LOG_DRIVER)
	    && IOCTL_WRITE (cmd)) {
		assert (sizeof (buf) >= IOCTL_ARG_SIZE (cmd));
		memcpy (buf, arg, IOCTL_ARG_SIZE (cmd));
	}

	do {
		err = ioctl (fd, cmd, arg);
	} while (-1 == err && EINTR == errno);

	if (log->mask & VBI3_LOG_DRIVER) {
		char buffer[1024];
		int saved_errno;
		size_t size;
		size_t n;

		saved_errno = errno;

		size = sizeof (buffer);

		n = snprintf (buffer, size, "%d = ", err);
		n = MIN (n, size);

		/* Ioctl name. */
		n += log_fn (buffer + n, size - n, cmd, 0, NULL);
		n = MIN (n, size);

		if (n < size - 1)
			buffer[n++] = '(';

		if (IOCTL_WRITE (cmd)) {
			n += log_fn (buffer + n, size - n,
				     cmd, IOCTL_READ (cmd) ? 3 : 2, &buf);
			n = MIN (n, size);
		}

		if (-1 == err) {
			snprintf (buffer + n, size - n,
				  "), errno = %d (%s)",
				  saved_errno, strerror (saved_errno));
		} else {
			if (IOCTL_READ (cmd)) {
				n += strlcpy (buffer + n, ") -> (",
					      size - n);
				n = MIN (n, size);
				n += log_fn (buffer + n, size - n,
					     cmd,
					     IOCTL_WRITE (cmd) ? 3 : 1,
					     arg);
				n = MIN (n, size);
			}

			strlcpy (buffer + n, ")", size - n);
		}

		log->fn (VBI3_LOG_DRIVER, context,
			 buffer, log->user_data);

		errno = saved_errno;
	}

	return err;
}

/**
 * @internal
 * Drop-in for mmap(). Logs the request on fp if not NULL.
 */
void *
_vbi3_log_mmap			(_vbi3_log_hook *	log,
				 const char *		context,
				 void *			start,
				 size_t			length,
				 int			prot,
				 int			flags,
				 int			fd,
				 off_t			offset)
{
	void *r;

	assert (NULL != log);

	r = mmap (start, length, prot, flags, fd, offset);

	if (log->mask & VBI3_LOG_DRIVER) {
		char buffer[1024];
		int saved_errno;
		size_t size;
		size_t n;

		saved_errno = errno;

		size = sizeof (buffer);

		n = snprintf (buffer, size,
			      "%p = mmap (start=%p length=%d prot=",
			      r, start, (int) length);
		n = MIN (n, size);
		n += snprint_symbolic (buffer + n, size - n,
				       2, (unsigned long) prot,
				       "EXEC", PROT_EXEC,
				       "READ", PROT_READ,
				       "WRITE", PROT_WRITE,
				       "NONE", PROT_NONE,
				       0);
		n = MIN (n, size);
		n += strlcpy (buffer + n, " flags=", size - n);
		n = MIN (n, size);
		n += snprint_symbolic (buffer + n, size - n,
				       2, (unsigned long) flags,
				       "FIXED", MAP_FIXED,
				       "SHARED", MAP_SHARED,
				       "PRIVATE", MAP_PRIVATE,
				       0);
		n = MIN (n, size);
		n += snprintf (buffer + n, size - n,
			       " fd=%d offset=%d)", fd, (int) offset);
		n = MIN (n, size);

		if (MAP_FAILED == r) {
			snprintf (buffer + n, size - n,
				  ", errno=%d (%s)",
				  saved_errno, strerror (saved_errno));
		}

		log->fn (VBI3_LOG_DRIVER, context,
			 buffer, log->user_data);

		errno = saved_errno;
	}

	return r;
}

/**
 * @internal
 * Drop-in for munmap(). Logs the request on fp if not NULL.
 */
int
_vbi3_log_munmap		(_vbi3_log_hook *	log,
				 const char *		context,
				 void *			start,
				 size_t			length)
{
	int r;

	assert (NULL != log);
	
	r = munmap (start, length);
	
	if (log->mask & VBI3_LOG_DRIVER) {
		int saved_errno;
		
		saved_errno = errno;
		
		if (-1 == r) {
			_vbi3_log_printf (log->fn,
					  log->user_data,
					  VBI3_LOG_DRIVER,
					  __FILE__,
					  context,
					  "%d = munmap (start=%p length=%d), "
					  "errno=%d (%s)",
					  r, start, (int) length,
					  saved_errno, strerror (saved_errno));
		} else {
			_vbi3_log_printf (log->fn,
					  log->user_data,
					  VBI3_LOG_DRIVER,
					  __FILE__,
					  context,
					  "%d = munmap (start=%p length=%d)",
					  r, start, (int) length);
		}

		errno = saved_errno;
	}

	return r;
}

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
