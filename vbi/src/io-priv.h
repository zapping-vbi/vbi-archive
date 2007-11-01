/*
 *  libzvbi - Device interfaces
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

/* $Id: io-priv.h,v 1.1.2.7 2007-11-01 00:21:23 mschimek Exp $ */

#ifndef IO_PRIV_H
#define IO_PRIV_H

#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "io.h"
#include "misc.h"

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

typedef size_t
ioctl_log_fn			(char *			str,
				 size_t			size,
				 unsigned int		cmd,
				 int			rw,
				 void *			arg);
extern size_t
snprint_symbolic		(char *			str,
				 size_t			size,
				 int			mode,
				 unsigned long		value,
				 ...);
extern size_t
snprint_unknown_ioctl		(char *			str,
				 size_t			size,
				 unsigned int		cmd,
				 void *			arg);
extern int
_vbi3_log_open			(_vbi3_log_hook *	log,
				 const char *		context,
				 const char *		pathname,
				 int			flags,
				 mode_t			mode);
extern int
_vbi3_log_close			(_vbi3_log_hook *	log,
				 const char *		context,
				 int			fd);
extern int
_vbi3_log_ioctl			(_vbi3_log_hook *	log,
				 const char *		context,
				 ioctl_log_fn *		fn,
				 int			fd,
				 unsigned int		cmd,
				 void *			arg);
extern void *
_vbi3_log_mmap			(_vbi3_log_hook *	log,
				 const char *		context,
				 void *			start,
				 size_t			length,
				 int			prot,
				 int			flags,
				 int			fd,
				 off_t			offset);
extern int
_vbi3_log_munmap		(_vbi3_log_hook *	log,
				 const char *		context,
				 void *			start,
				 size_t			length);

/**
 * @ingroup Devmod
 */
struct vbi3_capture {
	vbi3_bool		(* read)
					(vbi3_capture *,
					 vbi3_capture_buffer **,
					 vbi3_capture_buffer **,
					 const struct timeval *);

	vbi3_bool		(* sampling_point)
					(vbi3_capture *,
					 vbi3_bit_slicer_point *,
					 unsigned int row,
					 unsigned int nth_bit);
	vbi3_bool		(* debug)
					(vbi3_capture *,
					 vbi3_bool enable);
#if 3 == VBI_VERSION_MINOR
	const vbi3_sampling_par *
				(* parameters)
					(vbi3_capture *);
#else
	vbi_raw_decoder *	(* parameters)
					(vbi_capture *);
#endif
        unsigned int            (* update_services)
					(vbi3_capture *,
                                         vbi3_bool,
					 vbi3_bool,
                                         unsigned int,
					 int,
					 char **);
        int                     (* get_scanning)
					(vbi3_capture *);

	void			(* flush)
					(vbi3_capture *);

	int			(* get_fd)
					(vbi3_capture *);

	VBI3_CAPTURE_FD_FLAGS	(* get_fd_flags)
					(vbi3_capture *);

	vbi3_bool 		(* set_video_path)
					(vbi3_capture *,
					 const char *);

	void			(* _delete)
					(vbi3_capture *);

	_vbi3_log_hook		log;
};

extern void
vbi3_capture_io_update_timeout	(struct timeval *	timeout,
				 const struct timeval *	tv_start);
extern int
vbi3_capture_io_select		(int			fd,
				 struct timeval *	timeout);

/* Preliminary hack for tests. */
extern vbi3_bool vbi3_capture_force_read_mode;

#endif /* IO_PRIV_H */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
