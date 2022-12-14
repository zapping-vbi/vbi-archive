/*
 * Zapping (TV viewer for the Gnome Desktop)
 *
 * Copyright (C) 2003 Michael H. Schimek
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* $Id: device.c,v 1.13 2006-06-20 18:14:47 mschimek Exp $ */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>		/* LONG_MAX */
#include <assert.h>
#include <inttypes.h>

#include <fcntl.h>		/* open() */
#include <unistd.h>		/* close(), mmap(), munmap() */
#include <sys/ioctl.h>		/* ioctl() */
#include <sys/mman.h>		/* mmap(), munmap() */
#include <errno.h>

#ifndef PRId64
#  define PRId64 "lld"
#endif
#ifndef PRIu64
#  define PRIu64 "llu"
#endif
#ifndef PRIx64
#  define PRIx64 "llx"
#endif

#include "device.h"

void
fprint_symbolic			(FILE *			fp,
				 int			mode,
				 unsigned long		value,
				 ...)
{
	unsigned int i, j = 0;
	unsigned long v;
	const char *s;
	va_list ap;

	if (mode == 0) {
		unsigned int n[2] = { 0, 0 };

		va_start (ap, value);

		for (i = 0; (s = va_arg (ap, const char *)); i++) {
			v = va_arg (ap, unsigned long);
			n[((v & (v - 1)) == 0)]++; /* single bit */
		}

		mode = 1 + (n[1] > n[0]); /* 1-enum, 2-set flags, 3-all flags */

		va_end (ap); 
	}

	va_start (ap, value);

	for (i = 0; (s = va_arg (ap, const char *)); i++) {
		v = va_arg (ap, unsigned long);
		if (mode == 3 || v == value
		    || (mode == 2 && (v & value))) {
			fprintf (fp, "%s%s%s", j++ ? "|" : "",
				 (mode == 3 && (v & value) == 0) ? "!" : "", s);
			value &= ~v;
		}
	}

	if (0 == value && 0 == j)
		fputc ('0', fp);
	else if (value)
		fprintf (fp, "%s0x%lx", j ? "|" : "", value);

	va_end (ap); 
}

void
fprint_unknown_ioctl		(FILE *			fp,
				 unsigned int		cmd,
				 void *			arg)
{
  fprintf (fp, "<unknown cmd 0x%x %c%c arg=%p size=%u>",
           cmd, IOCTL_READ (cmd) ? 'R' : 'r',
	   IOCTL_WRITE (cmd) ? 'W' : 'w',
	   arg, IOCTL_ARG_SIZE (cmd)); 
}

/**
 * Helper function to calculate remaining time for select().
 * result = timeout - (now - start).
 */
void
timeout_subtract_elapsed	(struct timeval *	result,
				 const struct timeval *	timeout,
				 const struct timeval *	now,
				 const struct timeval *	start)
{
	if (!timeout) {
		result->tv_sec = LONG_MAX;
		result->tv_usec = LONG_MAX;
	} else {
		struct timeval elapsed;

		timeval_subtract (&elapsed, now, start);

		if ((elapsed.tv_sec | elapsed.tv_usec) > 0) {
			timeval_subtract (result, timeout, &elapsed);

			if ((result->tv_sec | result->tv_usec) < 0) {
				result->tv_sec = 0;
				result->tv_usec = 0;
			}
		} else {
			*result = *timeout;
		}
	}
}

int
device_open			(FILE *			fp,
				 const char *		pathname,
				 int			flags,
				 mode_t			mode)
{
  int fd;

  fd = open (pathname, flags, mode);

  if (fp)
    {
      int saved_errno;

      saved_errno = errno;

      fprintf (fp, "%d = open (\"%s\", ", fd, pathname);
      fprint_symbolic (fp, 2, (unsigned long) flags,
		       "RDONLY", O_RDONLY,
		       "WRONLY", O_WRONLY,
		       "RDWR", O_RDWR,
		       "CREAT", O_CREAT,
		       "EXCL", O_EXCL,
		       "TRUNC", O_TRUNC,
		       "APPEND", O_APPEND,
		       "NONBLOCK", O_NONBLOCK,
		       0);
      fprintf (fp, ", 0%o)", mode);

      if (-1 == fd)
	fprintf (fp, ", errno=%d, %s\n",
		 saved_errno, strerror (saved_errno));
      else
	fputc ('\n', fp);

      errno = saved_errno;
    }

  return fd;
}

int
device_close			(FILE *			fp,
				 int			fd)
{
  int r;

  r = close (fd);

  if (fp)
    {
      int saved_errno;

      saved_errno = errno;

      if (-1 == r)
	fprintf (fp, "%d = close (%d), errno=%d, %s\n",
		 r, fd, saved_errno, strerror (saved_errno));
      else
	fprintf (fp, "%d = close (%d)\n", r, fd);

      errno = saved_errno;
    }

  return r;
}

int
device_ioctl			(FILE *			fp,
				 ioctl_log_fn *		fn,
				 int			fd,
				 unsigned int		cmd,
				 void *			arg)
{
  int buf[2048];
  int err;

  if (fp && IOCTL_WRITE (cmd))
    {
      assert (sizeof (buf) >= IOCTL_ARG_SIZE (cmd));
      memcpy (buf, arg, IOCTL_ARG_SIZE (cmd));
    }

  do err = ioctl (fd, cmd, arg);
  while (-1 == err && EINTR == errno);

  if (fp && fn)
    {
      int saved_errno;

      saved_errno = errno;

      fprintf (fp, "%d = ", err);
      fn (fp, cmd, 0, NULL);
      fputc ('(', fp);
      
      if (IOCTL_WRITE (cmd))
	fn (fp, cmd, IOCTL_READ (cmd) ? 3 : 2, &buf);

      if (-1 == err)
	{
	  fprintf (fp, "), errno = %d, %s\n",
		   saved_errno, strerror (saved_errno));
	}
      else 
	{
	  if (IOCTL_READ (cmd))
	    {
	      fputs (") -> (", fp);
	      fn (fp, cmd, IOCTL_WRITE (cmd) ? 3 : 1, arg);
	    }

	  fputs (")\n", fp);
	}

      errno = saved_errno;
    }
  
  return err;
}

void *
device_mmap			(FILE *			fp,
				 void *			start,
				 size_t			length,
				 int			prot,
				 int			flags,
				 int			fd,
				 off_t			offset)
{
  void *r;

  r = mmap (start, length, prot, flags, fd, offset);

  if (fp)
    {
      int saved_errno;

      saved_errno = errno;

      fprintf (fp, "%p = mmap (start=%p length=%d prot=",
	       r, start, (int) length);
      fprint_symbolic (fp, 2, (unsigned long) prot,
		       "EXEC", PROT_EXEC,
		       "READ", PROT_READ,
		       "WRITE", PROT_WRITE,
		       "NONE", PROT_NONE,
		       0);
      fputs (" flags=", fp);
      fprint_symbolic (fp, 2, (unsigned long) flags,
		       "FIXED", MAP_FIXED,
		       "SHARED", MAP_SHARED,
		       "PRIVATE", MAP_PRIVATE,
		       0);
      fprintf (fp, " fd=%d offset=%d)", fd, (int) offset);

      if (MAP_FAILED == r)
	fprintf (fp, ", errno=%d, %s\n",
		 saved_errno, strerror (saved_errno));
      else
	fputc ('\n', fp);

      errno = saved_errno;
    }

  return r;
}

int
device_munmap			(FILE *			fp,
				 void *			start,
				 size_t			length)
{
  int r;

  r = munmap (start, length);

  if (fp)
    {
      int saved_errno;

      saved_errno = errno;

      if (-1 == r)
	fprintf (fp, "%d = munmap (start=%p length=%d), errno=%d, %s\n",
		 r, start, (int) length,
		 saved_errno, strerror (saved_errno));
      else
	fprintf (fp, "%d = munmap (start=%p length=%d)\n",
		 r, start, (int) length);

      errno = saved_errno;
    }

  return r;
}

ssize_t
device_read			(FILE *			fp,
				 int			fd,
				 void *			buf,
				 size_t			count)
{
	ssize_t actual;

	actual = read (fd, buf, count);

	if (fp) {
		int saved_errno;

		saved_errno = errno;

		if (-1 == actual) {
			fprintf (fp, "%" PRId64 " = 0x%" PRIx64
				 " = read (fd=%d buf=%p count=%" PRIu64
				 "=0x%" PRIx64 "), errno=%d, %s\n",
				 (int64_t) actual, (int64_t) actual,
				 fd, buf,
				 (uint64_t) count, (uint64_t) count,
				 saved_errno, strerror (saved_errno));
		} else {
			fprintf (fp, "%" PRId64 " = 0x%" PRIx64
				 " = read (fd=%d buf=%p count=%" PRIu64
				 "=0x%" PRIx64 ")\n",
				 (int64_t) actual, (int64_t) actual,
				 fd, buf,
				 (uint64_t) count, (uint64_t) count);
		}

		errno = saved_errno;
	}

	return actual;
}

int
device_select			(FILE *			fp,
				 int			n,
				 fd_set *		readfds,
				 fd_set *		writefds,
				 fd_set *		exceptfds,
				 struct timeval *	timeout)
{
	struct timeval tv;
	int r;

	/* Linux select() may change tv. */
	if (NULL != timeout)
		tv = *timeout;

	r = select (n, readfds, writefds, exceptfds, timeout ? &tv : NULL);

	if (fp) {
		int saved_errno;

		saved_errno = errno;

		fprintf (fp, "%d = select (n=%d readfds=%p "
			 "writefds=%p exceptfds=%p timeout=",
			 r, n, readfds, writefds, exceptfds);

		if (NULL == timeout) {
			fputs ("NULL)", fp);
		} else {
			fprintf (fp, "{%u %u})",
				 (unsigned int) timeout->tv_sec,
				 (unsigned int) timeout->tv_usec);
		}

		if (r <= 0) {
			fprintf (fp, ", errno=%d (%s)\n",
				 saved_errno, strerror (saved_errno));
		} else {
			fputc ('\n', fp);
		}

		errno = saved_errno;
	}

	return r;
}
