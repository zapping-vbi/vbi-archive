/*
 * Zapping (TV viewer for the Gnome Desktop)
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

/*
  This is an auxiliary suid program to prepare a kernel video capture
  device for DMA overlay onto video memory. When you find any security
  flaws here, please report at zapping-misc@lists.sourceforge.net.
*/

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#ifdef HAVE_GETOPT_LONG
#  include <getopt.h>
#endif
#include <sys/stat.h>
#include <assert.h>

#include <X11/Xlib.h>

#include "zapping_setup_fb.h"

static const char *	zsfb_version		= "zapping_setup_fb 0.12";
static const char *	default_device_name	= "/dev/video0";
static const int	max_verbosity		= 3;

#ifndef HAVE_PROGRAM_INVOCATION_NAME
char *			program_invocation_name;
char *			program_invocation_short_name;
#endif

unsigned int		uid;
unsigned int		euid;
int			verbosity		= 1;
/* legacy verbosity value, used in libtv/screen.c */
int			debug_msg		= 0;
FILE *			log_fp			= NULL;

#include "../common/device.c"   /* generic device access routines */ 

#ifndef major
#  define major(dev)  (((dev) >> 8) & 0xff)
#endif

int
device_open_safer		(const char *		device_name,
				 int			major_number,
				 int			flags)
{
  struct stat st;
  int fd;

  /* Sanity checks */

  if (strchr (device_name, '.'))
    {
      message (1, "Device name '%s' rejected, has dots.\n", device_name);
      return -1;
    }

  if (strncmp (device_name, "/dev/", 5))
    {
      message (1, "Device name '%s' rejected, must start with '/dev/'.\n",
	       device_name);
      return -1;
    }

  if (major_number != 0)
    {
      if (-1 == stat (device_name, &st))
	{
	  errmsg ("Cannot stat device '%s'", device_name);
	  return -1;
	}

      if (!S_ISCHR (st.st_mode))
	{
	  message (1, "'%s' is not a character device file.\n", device_name);
	  return -1;
	}

      if (major_number != major (st.st_rdev))
	{
	  message (1, "'%s' has suspect major number %d, expected %d.\n",
		   device_name, major (st.st_rdev), major_number);
	  return -1;
	}
    }

  message (2, "Opening device '%s'.\n", device_name);

  if (-1 == (fd = device_open (log_fp, device_name, flags, 0600)))
    {
      errmsg ("Cannot open device '%s'", device_name);
    }

  return fd;
}

void
drop_root_privileges		(void)
{
  if (ROOT_UID == euid && ROOT_UID != uid)
    {
      message (2, "Dropping root privileges\n");

      if (-1 == seteuid (uid))
        {
	  errmsg ("Cannot drop root privileges "
		  "despite uid=%d, euid=%d\nAborting.", uid, euid);
	  exit (EXIT_FAILURE);
        }
    }
  else if (ROOT_UID == uid)
    {
#if 0 /* cannot distinguish between root and consolehelper */
      message (1, "You should not run %s as root,\n"
	       "better use consolehelper, sudo, su or set the "
	       "SUID flag with chmod +s.\n",
	       program_invocation_name);
#endif
    }
}

int
restore_root_privileges		(void)
{
  if (ROOT_UID == euid && ROOT_UID != uid)
    {
      message (2, "Restoring root privileges\n");

      if (-1 == seteuid (euid))
        {
	  errmsg ("Cannot restore root privileges "
		  "despite uid=%d, euid=%d", uid, euid);
	  return FALSE;
        }
    }

  return TRUE;
}

static const char
short_options [] = "b:d:D:hqS:vV";

#ifdef HAVE_GETOPT_LONG

static const struct option
long_options [] =
{
  { "bpp",		required_argument,	0, 'b' },
  { "device",		required_argument,	0, 'd' },
  { "display",		required_argument,	0, 'D' },
  { "help",		no_argument,		0, 'h' },
  { "quiet",		no_argument,		0, 'q' },
  { "screen",		required_argument,	0, 'S' },
  { "usage",		no_argument,		0, 'h' },
  { "verbose",		no_argument,		0, 'v' },
  { "version",		no_argument,		0, 'V' },
};

#else

#  define getopt_long(ac, av, s, l, i) getopt (ac, av, s)

#endif

static void
usage				(FILE *			fp)
{
  fprintf (fp,
	   "Usage: %s [OPTIONS]\n"
	   "Available options:\n"
	   " -b, --bpp x           - Color depth, bits per pixel on "
	   "said display\n"
	   " -d, --device name     - The video device to open, default %s\n"
	   " -D, --display name    - The X display to use\n"
	   " -h, --help, --usage   - Show this message\n"
	   " -q, --quiet           - Decrement verbosity level\n"
	   " -S, --screen number   - X screen to use (Xinerama)\n"
	   " -v, --verbose         - Increment verbosity level\n"
	   " -V, --version         - Print the program version and exit\n"
	   "",
	   program_invocation_name,
	   default_device_name);
}

int
main				(int			argc,
				 char **		argv)
{
  const char *device_name;
  const char *display_name;
  int screen_number;
  int bpp_arg;
  tv_screen *screens;
  tv_screen *xs;

#ifndef HAVE_PROGRAM_INVOCATION_NAME
  program_invocation_name = argv[0];
  program_invocation_short_name = argv[0];
#endif

  /* Make sure fd's 0 1 2 are open, otherwise
     we might end up sending error messages to
     the device file. */
  {
    int i, n;

    for (i = 0; i < 3; i++)
      if (-1 == fcntl (i, F_GETFL, &n))
	exit (EXIT_FAILURE);
  }

  /* Drop root privileges until we need them */

  uid = getuid ();
  euid = geteuid ();

  drop_root_privileges ();

  /* Parse arguments */

  device_name = default_device_name;
  display_name = getenv ("DISPLAY");
  screen_number = -1; /* default */

  bpp_arg = -1;

  for (;;)
    {
      int c;

      c = getopt_long (argc, argv, short_options, long_options, NULL);
      if (-1 == c)
        break;

      switch (c)
        {
	case 'b':
	  bpp_arg = strtol (optarg, NULL, 0);

	  switch (bpp_arg)
	    {
	    case 8:
	    case 15:
	    case 16:
	    case 24:
	    case 32:
	      break;

	    default:
	      message (1, "Invalid bpp argument %d. Expected "
		       "color depth 8, 15, 16, 24 or 32.\n", bpp_arg);
	      goto failure;
	    }
	  
	  break;

	case 'd':
	  device_name = strdup (optarg);
	  break;

	case 'D':
	  display_name = strdup (optarg);
	  break;

	case 'h':
	  usage (stdout);
	  exit (EXIT_SUCCESS);

	case 'q':
	  if (verbosity > 0)
	    verbosity--;
	  break;

	case 'S':
	  screen_number = strtol (optarg, NULL, 0);
	  break;

	case 'v':
	  if (verbosity < max_verbosity)
	    verbosity++;
	  break;

	case 'V':
	  message (0, "%s\n", zsfb_version);
	  exit (EXIT_SUCCESS);

	default:
	  /* getopt_long prints option name when unknown or arg missing */
	  usage (stderr);
	  goto failure;
	}
    }

  if (verbosity >= 2)
    debug_msg = 1; /* log X access */

  if (verbosity >= 3)
    log_fp = stderr; /* log ioctls */

  message (1, "(C) 2000-2004 Iñaki G. Etxebarria, Michael H. Schimek.\n"
	   "This program is freely redistributable under the terms\n"
	   "of the GNU General Public License.\n\n");

  message (1, "Using video device '%s', display '%s', screen %d.\n",
	   device_name, display_name, screen_number);

  message (1, "Querying frame buffer parameters from X server.\n");

  screens = tv_screen_list_new (display_name, bpp_arg);
  if (!screens)
    {
      message (1, "No screens found.\n");
      goto failure;
    }

  for (xs = screens; xs; xs = xs->next)
    {
      message (2, "Screen %d:\n"
	       "  position               %u, %u - %u, %u\n"
	       "  frame buffer address   0x%lx\n"
	       "  frame buffer size      %ux%u pixels, 0x%x bytes\n"
	       "  bytes per line         %u bytes\n"
	       "  pixfmt                 %s\n",
	       xs->screen_number,
	       xs->x,
	       xs->y,
	       xs->x + xs->width,
	       xs->y + xs->height,
	       xs->target.base,
	       xs->target.format.width,
	       xs->target.format.height,
	       xs->target.format.size,
	       xs->target.format.bytes_per_line,
	       tv_pixfmt_name (xs->target.format.pixfmt));
    }

  if (-1 == screen_number)
    {
      Display *display;

      display = XOpenDisplay (display_name);
      if (NULL == display)
	{
	  goto failure;
	}

      screen_number = XDefaultScreen (display);

      XCloseDisplay (display);
    }

  for (xs = screens; xs; xs = xs->next)
    if (xs->screen_number == screen_number)
      break;

  if (!xs)
    {
      message (1, "Screen %d not found.\n",
	       screen_number);
      goto failure;
    }

  if (!tv_screen_is_target (xs))
    {
      message (1, "DMA not possible on screen %d.\n",
	       xs->screen_number);
      goto failure;
    }

  do
    {
      if (1 == setup_v4l25 (device_name, &xs->target))
	break;

      if (1 == setup_v4l2 (device_name, &xs->target))
	break;

      if (1 == setup_v4l (device_name, &xs->target))
	break;

      goto failure;
    }
  while (0);

  message (1, "Setup completed.\n");

  return EXIT_SUCCESS;

 failure:
  message (1, "Setup failed. %s\n",
	   (verbosity <= 1) ? "Try -vv for details." : "");

  return EXIT_FAILURE;
}

/*
Local Variables:
coding: utf-8
End:
 */
