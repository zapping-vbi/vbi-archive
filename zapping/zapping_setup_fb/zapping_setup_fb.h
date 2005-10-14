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

/* $Id: zapping_setup_fb.h,v 1.8 2005-10-14 23:37:39 mschimek Exp $ */

#ifndef ZAPPING_SETUP_FB_H
#define ZAPPING_SETUP_FB_H

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "libtv/screen.h"	/* Xinerama & DGA interface */
#include "common/device.h"	/* generic device access routines */

#define ROOT_UID 0

#ifdef HAVE_GNU_C_VARIADIC_MACROS
#  define errmsg(template, args...)					\
     error_message (__FILE__, __LINE__, template , ##args)
#else
#  error Compiler doesn't support GNU C variadic macros
#endif

#define privilege_hint()						\
  message (/* verbosity */ 1,						\
	   "%s must run with root privileges.\n"			\
	   "Try consolehelper, sudo, su or set the SUID flag with "	\
	   "chmod +s.\n", program_invocation_name);

extern char *           program_invocation_name;
extern char *           program_invocation_short_name;
extern int		verbosity;
extern unsigned int	uid;
extern unsigned int	euid;
extern FILE *		device_log_fp;

extern void
drop_root_privileges		(void);
extern int
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
  __attribute__ ((nonnull (1), format (printf, 3, 4)));

extern int
device_open_safer		(const char *		device_name,
				 int			device_fd,
				 int			major_number,
				 int			flags);

extern int
setup_v4l	 		(const char *		device_name,
				 int			device_fd,
				 const tv_overlay_buffer *buffer)
  __attribute__ ((nonnull (3)));
extern int
setup_v4l2	 		(const char *		device_name,
				 int			device_fd,
				 const tv_overlay_buffer *buffer)
  __attribute__ ((nonnull (3)));
extern int
setup_v4l25	 		(const char *		device_name,
				 int			device_fd,
				 const tv_overlay_buffer *buffer)
  __attribute__ ((nonnull (3)));

#endif /* ZAPPING_SETUP_FB_H */
