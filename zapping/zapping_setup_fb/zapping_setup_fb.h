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

/* $Id: zapping_setup_fb.h,v 1.4 2003-11-29 19:43:24 mschimek Exp $ */

#include <stdio.h>
#include <string.h>
#include <errno.h>

#define ROOT_UID 0

#define STF2(x) #x
#define STF1(x) STF2(x)

/* NB ##args is a GCC ext but ##__VA_ARGS__ is not bw compat */

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

#define privilege_hint()						\
  message (1, "%s must run with root privileges.\n"			\
	   "Try consolehelper, sudo, su or set the SUID flag with "	\
	   "chmod +s.\n", program_invocation_name);

#undef FALSE
#define FALSE 0
#undef TRUE
#define TRUE 1

extern char *           program_invocation_name;
extern char *           program_invocation_short_name;
extern int		verbosity;

#include "../common/device.h"	/* generic device access routines */

extern int		uid;
extern int		euid;
extern FILE *		log_fp;

extern void
drop_root_privileges		(void);
extern int
restore_root_privileges		(void);

extern int
device_open_safer		(const char *		device_name,
				 int			major_number,
				 int			flags);


typedef int gboolean;

typedef struct _x11_dga_parameters x11_dga_parameters;

struct _x11_dga_parameters {
  void *		base;			/* frame buffer */
  unsigned int		size;			/* 2048 * 768, bytes */

  unsigned int		width;			/* 1024 */
  unsigned int		height;			/* 768 */

  /* XXX rgb1? 1rgb? 1bgr? bgr1? le/be? */
  unsigned int		depth;			/* 15 */

  unsigned int		bytes_per_line;		/* 2048 */
  unsigned int		bits_per_pixel;		/* 16 */
};

gboolean
x11_dga_query			(x11_dga_parameters *	par,
				 const char *		display,
				 int			bpp_hint);


extern int
setup_v4l	 		(const char *		device_name,
				 x11_dga_parameters *	dga);
extern int
setup_v4l2	 		(const char *		device_name,
				 x11_dga_parameters *	dga);
extern int
setup_v4l25	 		(const char *		device_name,
				 x11_dga_parameters *	dga);
