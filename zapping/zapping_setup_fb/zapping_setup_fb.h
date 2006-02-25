/*
 *  Zapping (TV viewer for the Gnome Desktop)
 *
 * Copyright (C) 2000 Iñaki García Etxebarria
 * Copyright (C) 2003-2004 Michael H. Schimek
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

/* $Id: zapping_setup_fb.h,v 1.10 2006-02-25 17:34:36 mschimek Exp $ */

#ifndef ZAPPING_SETUP_FB_H
#define ZAPPING_SETUP_FB_H

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "libtv/screen.h"	/* Xinerama & DGA interface */
#include "common/device.h"	/* generic device access routines */

#define ROOT_UID 0

/* Keep this in sync with tveng.c. */
typedef enum {
	ZSFB_SUCCESS = EXIT_SUCCESS,

	ZSFB_BUG = 60,
	ZSFB_NO_PERMISSION,
	ZSFB_NO_SCREEN,
	ZSFB_INVALID_BPP,
	ZSFB_BAD_DEVICE_NAME,
	ZSFB_BAD_DEVICE_FD,
	ZSFB_UNKNOWN_DEVICE,
	ZSFB_OPEN_ERROR,
	ZSFB_IOCTL_ERROR,
	ZSFB_OVERLAY_IMPOSSIBLE,
} zsfb_status;

#ifdef HAVE_GNU_C_VARIADIC_MACROS
#  define errmsg(template, args...)					\
     error_message (__FILE__, __LINE__, template , ##args)
#  define errmsg_ioctl(name, errno)					\
     error_message (__FILE__, __LINE__, _("Ioctl %s failed: %s."),	\
		    name, strerror (errno))
#else
#  error Compiler does not support GNU C variadic macros
#endif

#define privilege_hint()						\
  message (/* verbosity */ 1,						\
	   "%s must run with root privileges.\n"			\
	   "Try consolehelper, sudo, su or set the SUID flag with "	\
	   "chmod +s.\n", program_invocation_name);

#if (__GNUC__ == 3 && __GNUC_MINOR__ >= 3) || __GNUC__ >= 4
#  define _zsfb_nonnull(args...) nonnull(args)
#else
#  define _zsfb_nonnull(args...)
#endif

extern char *           program_invocation_name;
extern char *           program_invocation_short_name;
extern int		verbosity;
extern unsigned int	uid;
extern unsigned int	euid;
extern FILE *		device_log_fp;

extern void
drop_root_privileges		(void);
extern zsfb_status
restore_root_privileges		(void);

extern void
message				(int			level,
				 const char *		template,
				 ...)
  __attribute__ ((format (printf, 2, 3)));

extern void
error_message			(const char *		file,
				 unsigned int		line,
				 const char *		template,
				 ...)
  __attribute__ ((_zsfb_nonnull (1), format (printf, 3, 4)));

extern zsfb_status
device_open_safer		(int *			fd,
				 const char *		device_name,
				 int			device_fd,
				 unsigned int		major_number,
				 unsigned int		flags)
  __attribute__ ((_zsfb_nonnull (1)));
extern zsfb_status
setup_v4l	 		(const char *		device_name,
				 int			device_fd,
				 const tv_overlay_buffer *buffer)
  __attribute__ ((_zsfb_nonnull (3)));
extern zsfb_status
setup_v4l2	 		(const char *		device_name,
				 int			device_fd,
				 const tv_overlay_buffer *buffer)
  __attribute__ ((_zsfb_nonnull (3)));
extern zsfb_status
setup_v4l25	 		(const char *		device_name,
				 int			device_fd,
				 const tv_overlay_buffer *buffer)
  __attribute__ ((_zsfb_nonnull (3)));

#endif /* ZAPPING_SETUP_FB_H */
