/*
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

/* $Id: device.c,v 1.2 2004-05-21 05:34:09 mschimek Exp $ */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
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
      fprint_symbolic (fp, 2, flags,
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
  int err;

  err = close (fd);

  if (err)
    {
      int saved_errno;

      saved_errno = errno;

      if (-1 == err)
	fprintf (fp, "%d = close (%d), errno=%d, %s\n",
		 err, fd, saved_errno, strerror (saved_errno));
      else
	fprintf (fp, "%d = close (%d)\n", err, fd);

      errno = saved_errno;
    }

  return err;
}

int
device_ioctl			(FILE *			fp,
				 ioctl_log_fn *		fn,
				 int			fd,
				 unsigned int		cmd,
				 void *			arg)
{
  int buf[256];
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
