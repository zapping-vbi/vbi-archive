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
fprintf_symbolic		(FILE *			fp,
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

	if (value)
		fprintf (fp, "%s0x%lx", j ? "|" : "", value);

	va_end (ap); 
}

void
fprintf_unknown_cmd		(FILE *			fp,
				 unsigned int		cmd,
				 void *			arg)
{
  fprintf (fp, "<unknown cmd 0x%x %c%c arg=%p size=%u>",
           cmd, IOCTL_READ (cmd) ? 'R' : 'r',
	   IOCTL_WRITE (cmd) ? 'W' : 'w',
	   arg, IOCTL_ARG_SIZE (cmd)); 
}

int
device_ioctl			(int			fd,
				 unsigned int		cmd,
				 void *			arg,
				 FILE *			fp,
				 ioctl_log_fn *		fn)
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
      fn (fp, cmd, NULL);
      fputc ('(', fp);
      
      if (IOCTL_WRITE (cmd))
	fn (fp, cmd, &buf);

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
	      fn (fp, cmd, arg);
	    }

	  fputs (")\n", fp);
	}

      errno = saved_errno;
    }
  
  return err;
}
