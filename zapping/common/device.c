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

/* $Id: device.c,v 1.7 2004-10-09 02:52:23 mschimek Exp $ */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>

#include <fcntl.h>		/* open() */
#include <unistd.h>		/* close(), mmap(), munmap() */
#include <sys/ioctl.h>		/* ioctl() */
#include <sys/mman.h>		/* mmap(), munmap() */
#include <errno.h>

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
	fn (fp, cmd, IOCTL_READ (cmd) ? 2 : 3, &buf);

      if (-1 == err)
	{
	  fprintf (fp, "), errno = %d, %s\n",
		   errno, strerror (errno));
	}
      else 
	{
	  if (IOCTL_READ (cmd))
	    {
	      fputs (") -> (", fp);
	      fn (fp, cmd, IOCTL_WRITE (cmd) ? 1 : 3, arg);
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
