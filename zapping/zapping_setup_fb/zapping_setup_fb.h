/*
 *  Zapping (TV viewer for the Gnome Desktop)
 *
 * Copyright (C) 2000 Iñaki García Etxebarria
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

/* $Id: zapping_setup_fb.h,v 1.1 2003-01-21 05:18:39 mschimek Exp $ */

#include <stdio.h>
#include <string.h>
#include <errno.h>

#undef FALSE
#undef TRUE
#define FALSE 0
#define TRUE 1

#define ROOT_UID 0

#define STF2(x) #x
#define STF1(x) STF2(x)

/* NB ##args is a GNU ext but ##__VA_ARGS__ is not bw compat */

#define errmsg(template, args...)					\
do {									\
  if (verbosity > 0)							\
    fprintf (stderr, "%s:" __FILE__ ":" STF1(__LINE__) ": "		\
	     template ": %d, %s.\n",					\
	     program_invocation_short_name , ##args,			\
	     errno, strerror (errno));					\
} while (0)

#define message(level, template, args...)				\
do {									\
  if ((int) level <= verbosity)						\
    fprintf (stderr, template , ##args);				\
} while (0)

extern char *           program_invocation_short_name;
extern int		verbosity;

/* Frame buffer parameters */

extern unsigned long	addr;
extern unsigned int	bpl;
extern unsigned int	width;
extern unsigned int	height;
extern unsigned int	depth;
extern unsigned int	bpp;

extern int		uid, euid;

extern int
query_dga			(const char *		display_name,
				 int			bpp_arg);

#define CASE(x) case x: if (!arg) { fputs ("#x", fp); return; }
#define SYM(x) #x, (unsigned long)(x)

typedef void (ioctl_log_fn)	(FILE *			fp,
				 int			cmd,
				 void *			arg);
extern void
fprintf_symbolic		(FILE *			fp,
				 int			mode,
				 unsigned long		value,
				 ...);
extern int
dev_ioctl			(int			fd,
				 unsigned int		cmd,
				 void *			arg,
				 ioctl_log_fn *		fn);
extern int
dev_open			(const char *		device_name,
				 int			major_number,
				 int			flags);

extern int
drop_root_privileges		(int			uid,
				 int			euid);
extern int
restore_root_privileges		(int			uid,
				 int			euid);

extern int
setup_v4l	 		(const char *		device_name);
extern int
setup_v4l2	 		(const char *		device_name);
extern int
setup_v4l25	 		(const char *		device_name);
