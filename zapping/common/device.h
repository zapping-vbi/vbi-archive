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

/* $Id: device.h,v 1.8 2005-01-08 14:54:19 mschimek Exp $ */

#ifndef DEVICE_H
#define DEVICE_H

#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#if defined (_IOC_SIZE) /* Linux */

#define IOCTL_ARG_SIZE(cmd)	_IOC_SIZE (cmd)
#define IOCTL_READ(cmd)		(_IOC_DIR (cmd) & _IOC_READ)
#define IOCTL_WRITE(cmd)	(_IOC_DIR (cmd) & _IOC_WRITE)
#define IOCTL_READ_WRITE(cmd)	(_IOC_DIR (cmd) == (_IOC_READ | _IOC_WRITE))
#define IOCTL_NUMBER(cmd)	_IOC_NR (cmd)

#elif defined (IOCPARM_LEN) /* FreeBSD */

#define IOCTL_ARG_SIZE(cmd)	IOCPARM_LEN (cmd)
#define IOCTL_READ(cmd)		((cmd) & IOC_OUT)
#define IOCTL_WRITE(cmd)	((cmd) & IOC_IN)
#define IOCTL_READ_WRITE(cmd)	(((cmd) & IOC_DIRMASK) == (IOC_IN | IOC_OUT))
#define IOCTL_NUMBER(cmd)	((cmd) & 0xFF)

#else /* Don't worry, only used for debugging */

#define IOCTL_ARG_SIZE(cmd)	0
#define IOCTL_READ(cmd)		0
#define IOCTL_WRITE(cmd)	0
#define IOCTL_READ_WRITE(cmd)	0
#define IOCTL_NUMBER(cmd)	0

#endif

typedef void (ioctl_log_fn)	(FILE *			fp,
				 unsigned int		cmd,
				 int			rw,
				 void *			arg);
extern void
fprint_symbolic			(FILE *			fp,
				 int			mode,
				 unsigned long		value,
				 ...);
extern void
fprint_unknown_ioctl		(FILE *			fp,
				 unsigned int		cmd,
				 void *			arg);

static __inline__ void
timeval_add			(struct timeval *	sum,
				 const struct timeval *	tv1,
				 const struct timeval *	tv2)
{
	long usec;

	usec = tv1->tv_usec + tv2->tv_usec;

	if (usec > 1000000) {
		sum->tv_sec = tv1->tv_sec + tv2->tv_sec + 1;
		sum->tv_usec = usec - 1000000;
	} else {
		sum->tv_sec = tv1->tv_sec + tv2->tv_sec;
		sum->tv_usec = usec;
	}
}

static __inline__ void
timeval_subtract		(struct timeval *	diff,
				 const struct timeval *	tv1,
				 const struct timeval *	tv2)
{
	if (tv1->tv_usec < tv2->tv_usec) {
		diff->tv_sec = tv1->tv_sec - tv2->tv_sec - 1;
		diff->tv_usec = 1000000 + tv1->tv_usec - tv2->tv_usec;
	} else {
		diff->tv_sec = tv1->tv_sec - tv2->tv_sec;
		diff->tv_usec = tv1->tv_usec - tv2->tv_usec;
	}
}

static __inline__ long
timeval_cmp			(const struct timeval *	tv1,
				 const struct timeval *	tv2)
{
	if (tv1->tv_sec == tv2->tv_sec)
		return tv1->tv_usec - tv2->tv_usec;
	else
		return tv1->tv_sec - tv2->tv_sec;
}

extern void
timeout_subtract_elapsed	(struct timeval *	result,
				 const struct timeval *	timeout,
				 const struct timeval *	now,
				 const struct timeval *	start);

extern int
device_open			(FILE *			fp,
				 const char *		pathname,
				 int			flags,
				 mode_t			mode);
extern int
device_close			(FILE *			fp,
				 int			fd);
extern int
device_ioctl			(FILE *			fp,
				 ioctl_log_fn *		fn,
				 int			fd,
				 unsigned int		cmd,
				 void *			arg);
extern void *
device_mmap			(FILE *			fp,
				 void *			start,
				 size_t			length,
				 int			prot,
				 int			flags,
				 int			fd,
				 off_t			offset);
extern int
device_munmap			(FILE *			fp,
				 void *			start,
				 size_t			length);

#endif /* DEVICE_H */
