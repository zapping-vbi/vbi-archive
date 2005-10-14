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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/stat.h>
#ifdef HAVE_GETOPT_LONG
#  include <getopt.h>
#endif
#include <assert.h>

#include <X11/Xlib.h>

#include "common/intl-priv.h"
#include "zapping_setup_fb.h"

static const char *	zsfb_version		= "zapping_setup_fb 0.13";
static const char *	default_device_name	= "/dev/video0";

#ifndef HAVE_PROGRAM_INVOCATION_NAME
char *			program_invocation_name;
char *			program_invocation_short_name;
#endif

unsigned int		uid;
unsigned int		euid;

#define VERBOSITY_CHILD_PROCESS -1
#define VERBOSITY_MIN 0
#define VERBOSITY_MAX 3

int			verbosity		= 1;

/* Legacy verbosity value, used in libtv/screen.c. */
int			debug_msg		= 0;

/* Log V4L/V4L2 driver responses. */
FILE *			device_log_fp		= NULL;

void
message				(int			level,
				 const char *		template,
				 ...)
{
  if (VERBOSITY_CHILD_PROCESS == verbosity)
    return;

  assert (level >= VERBOSITY_MIN
	  && level <= VERBOSITY_MAX);

  if (verbosity >= level)
    {
      va_list ap;

      va_start (ap, template);
      vfprintf (stderr, template, ap);
      va_end (ap);
    }
}

void
error_message			(const char *		file,
				 unsigned int		line,
				 const char *		template,
				 ...)
{
  va_list ap;

  va_start (ap, template);

  if (VERBOSITY_CHILD_PROCESS == verbosity)
    {
      vfprintf (stderr, template, ap);
    }
  else if (verbosity > 0)
    {
      fprintf (stderr, "%s:%s:%u: ",
	       program_invocation_short_name, file, line);
      vfprintf (stderr, template, ap);
      fputc ('\n', stderr);
    }

  va_end (ap);
}

#include "common/device.c"	/* generic device access routines */ 

#ifndef major
#  define major(dev)  (((dev) >> 8) & 0xff)
#endif

int
device_open_safer		(const char *		device_name,
				 int			device_fd,
				 int			major_number,
				 int			flags)
{
  struct stat st;
  int fd;

  if (NULL != device_name)
    {
      if (strchr (device_name, '.'))
	{
	  errmsg (_("Device name %s is unsafe, contains dots."),
		  device_name);
	  errno = EINVAL;
	  return -1;
	}

      if (strncmp (device_name, "/dev/", 5))
	{
	  errmsg (_("Device name %s is unsafe, "
		    "does not begin with /dev/."),
		  device_name);
	  errno = EINVAL;
	  return -1;
	}

      message (/* verbosity */ 1,
	       "Opening device %s.\n", device_name);

      flags |= O_NOCTTY;
      flags &= ~(O_CREAT | O_EXCL | O_TRUNC);

      fd = device_open (device_log_fp, device_name, flags, 0600);
      if (-1 == fd)
	{
	  int saved_errno = errno;

	  /* TRANSLATORS: File name, error message. */
	  errmsg (_("Cannot open device %s. %s."),
		  device_name, strerror (saved_errno));

	  errno = saved_errno;
	  return -1;
	}

      if (-1 == fstat (fd, &st))
	{
	  int saved_errno = errno;

	  device_close (device_log_fp, fd);
	  fd = -1;

	  /* TRANSLATORS: File name, error message. */
	  errmsg (_("Cannot identify %s. %s."),
		  device_name, strerror (saved_errno));

	  errno = saved_errno;
	  return -1;
	}

      if (!S_ISCHR (st.st_mode))
	{
	  device_close (device_log_fp, fd);
	  fd = -1;

	  errmsg (_("%s is not a device."),
		  device_name);

	  errno = ENODEV;
	  return -1;
	}

      if (0 != major_number)
	{
	  if (major_number != major (st.st_rdev))
	    {
	      device_close (device_log_fp, fd);
	      fd = -1;

	      errmsg (_("%s is not a video device."),
		      device_name);

	      errno = ENODEV;
	      return -1;
	    }
	}
    }
  else if (-1 != device_fd)
    {
      /* Expect EBADF. */
      if (-1 == fstat (device_fd, &st))
	{
	  int saved_errno = errno;

	  errmsg (_("Cannot identify file descriptor %d. %s."),
		  device_fd, strerror (saved_errno));
	  errno = saved_errno;
	  return -1;
	}

      if (!S_ISCHR (st.st_mode))
	{
	  errmsg (_("File descriptor %d is not a device."),
		  device_fd);
	  errno = ENODEV;
	  return -1;
	}

      if (0 != major_number)
	{
	  if (major_number != major (st.st_rdev))
	    {
	      errmsg (_("File descriptor %d is not a video device."),
		      device_fd);
	      errno = ENODEV;
	      return -1;
	    }
	}

      message (/* verbosity */ 1,
	       "Using device by file descriptor %d.\n",
	       device_fd);

      fd = device_fd;
    }
  else
    {
      assert (0);
    }

  return fd;
}

void
drop_root_privileges		(void)
{
  if (ROOT_UID == euid && ROOT_UID != uid)
    {
      message (/* verbosity */ 2,
	       "Dropping root privileges\n");

      if (-1 == seteuid (uid))
        {
	  errmsg (_("Cannot drop root privileges despite "
		    "running with UID %d, EUID %d."),
		  uid, euid);

	  exit (EXIT_FAILURE);
        }
    }
  else if (ROOT_UID == uid)
    {
      if (0) /* cannot distinguish between root and consolehelper */
	message (/* verbosity */ 1,
		 "You should not run %s as root,\n"
		 "better use consolehelper, sudo, su or set the "
		 "SUID flag with chmod +s.\n",
		 program_invocation_name);
    }
}

int
restore_root_privileges		(void)
{
  if (ROOT_UID == euid && ROOT_UID != uid)
    {
      message (/* verbosity */ 2,
	       "Restoring root privileges\n");

      if (-1 == seteuid (euid))
        {
	  int saved_errno = errno;

	  errmsg (_("Cannot restore root privileges despite "
		    "running with UID %d, EUID %d."),
		  uid, euid);

	  errno = saved_errno;
	  return -1; /* failed */
        }
    }

  return 0; /* success */
}

static const char
short_options [] = "b:cd:f:hqvD:S:V";

#ifdef HAVE_GETOPT_LONG

static const struct option
long_options [] =
{
  { "bpp",		required_argument,	0, 'b' },
  { "child",		no_argument,		0, 'c' },
  { "device",		required_argument,	0, 'd' },
  { "fd",		required_argument,	0, 'f' },
  { "help",		no_argument,		0, 'h' },
  { "quiet",		no_argument,		0, 'q' },
  { "usage",		no_argument,		0, 'h' },
  { "verbose",		no_argument,		0, 'v' },
  { "display",		required_argument,	0, 'D' },
  { "screen",		required_argument,	0, 'S' },
  { "version",		no_argument,		0, 'V' },
};

#else
#  define getopt_long(ac, av, s, l, i) getopt (ac, av, s)
#endif

static void
usage				(FILE *			fp)
{
  if (VERBOSITY_CHILD_PROCESS == verbosity)
    return;

  fprintf (fp,
	   "Usage: %s [OPTIONS]\n"
	   "Available options:\n"
	   " -b, --bpp x          Color depth hint, bits per pixel on "
	   "                       display in question (24 or 32)\n"
	   " -c, --child          Return localized error messages in UTF-8\n"
	   "                       encoding to parent process on stderr\n"
	   " -d, --device name    The video device to open, default %s\n"
	   " -f, --fd number      Access video device by file descriptor\n"
	   " -h, --help, --usage  Print this message\n"
	   " -q, --quiet          Decrement verbosity level\n"
	   " -v, --verbose        Increment verbosity level\n"
	   " -D, --display name   The X display to use\n"
	   " -S, --screen number  The X screen to use (Xinerama)\n"
	   " -V, --version        Print the program version and exit\n"
	   "",
	   program_invocation_name,
	   default_device_name);
}

int
main				(int			argc,
				 char **		argv)
{
  const char *device_name;
  int device_fd;
  const char *display_name;
  int screen_number;
  int bpp_arg;
  tv_screen *screens;
  tv_screen *xs;

#ifndef HAVE_PROGRAM_INVOCATION_NAME
  program_invocation_name = argv[0];
  program_invocation_short_name = argv[0];
#endif

  /* Make sure fds 0 1 2 are open, otherwise we might end up sending
     error messages to the device file. */
  {
    int flags;
    int fd;

    for (fd = 0; fd <= 2; ++fd)
      if (-1 == fcntl (fd, F_GETFL, &flags))
	exit (EXIT_FAILURE);
  }

  /* Drop root privileges until we need them. */

  uid = getuid ();
  euid = geteuid ();

  drop_root_privileges ();

  /* Parse arguments. */

  device_name = default_device_name;
  device_fd = -1;

  display_name = getenv ("DISPLAY");
  screen_number = -1; /* use default screen of display */

  bpp_arg = -1; /* unknown bpp */

  assert (verbosity >= VERBOSITY_MIN
	  && verbosity <= VERBOSITY_MAX);

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
	    case 24:
	    case 32:
	      break;

	    default:
	      errmsg (_("Invalid bpp argument %d. Expected 24 or 32."),
		      bpp_arg);
	      goto failure;
	    }
	  
	  break;

	case 'c':
#ifdef ENABLE_NLS
	  bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	  textdomain (GETTEXT_PACKAGE);
#endif
	  verbosity = VERBOSITY_CHILD_PROCESS;

	  break;

	case 'd':
	  device_name = strdup (optarg);
	  device_fd = -1;
	  break;

	case 'D':
	  display_name = strdup (optarg);
	  break;

	case 'f':
	  device_name = NULL;
	  device_fd = strtol (optarg, NULL, 0);

	  if (device_fd <= 2)
	    {
	      errmsg (_("Invalid device file descriptor %d."),
		      device_fd);
	      exit (EXIT_FAILURE);
	    }

	  break;

	case 'h':
	  usage (stdout);
	  exit (EXIT_SUCCESS);

	case 'q':
	  if (verbosity > VERBOSITY_MIN)
	    --verbosity;
	  break;

	case 'S':
	  screen_number = strtol (optarg, NULL, 0);
	  break;

	case 'v':
	  if (verbosity < VERBOSITY_MAX)
	    ++verbosity;
	  break;

	case 'V':
	  printf ("%s\n", zsfb_version);
	  exit (EXIT_SUCCESS);

	default:
	  /* getopt(_long) prints option name when unknown or arg missing. */
	  usage (stderr);
	  goto failure;
	}
    }

  if (verbosity >= 2)
    debug_msg = 1;

  if (verbosity >= 3)
    device_log_fp = stderr;

  message (/* verbosity */ 1,
	   "(C) 2000-2005 Iñaki G. Etxebarria, Michael H. Schimek.\n"
	   "This program is freely redistributable under the terms\n"
	   "of the GNU General Public License.\n\n");

  message (/* verbosity */ 1,
	   "Using video device '%s', display '%s', screen %d.\n",
	   device_name, display_name, screen_number);

  message (/* verbosity */ 1,
	   "Querying frame buffer parameters from X server.\n");

  screens = tv_screen_list_new (display_name, bpp_arg);
  if (NULL == screens)
    {
      errmsg (_("No screens found."));
      goto failure;
    }

  for (xs = screens; xs; xs = xs->next)
    {
      message (/* verbosity */ 2,
	       "Screen %d:\n"
	       "  position               %u, %u - %u, %u\n"
	       "  frame buffer address   0x%lx\n"
	       "  frame buffer size      %ux%u pixels, 0x%lx bytes\n"
	       "  bytes per line         %lu bytes\n"
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
	       xs->target.format.bytes_per_line[0],
	       tv_pixfmt_name (xs->target.format.pixel_format->pixfmt));
    }

  if (-1 == screen_number)
    {
      Display *display;

      display = XOpenDisplay (display_name);
      if (NULL == display)
	{
	  errmsg (_("Cannot open display %s."),
		  display_name);
	  goto failure;
	}

      screen_number = XDefaultScreen (display);

      XCloseDisplay (display);
    }

  for (xs = screens; xs; xs = xs->next)
    if (xs->screen_number == screen_number)
      break;

  if (NULL == xs)
    {
      errmsg (_("Screen %d not found."),
	      screen_number);
      goto failure;
    }

  if (!tv_screen_is_target (xs))
    {
      errmsg (_("DMA is not possible on screen %d."),
	      xs->screen_number);
      goto failure;
    }

  {
    int result;

    result = setup_v4l25 (device_name, device_fd, &xs->target);
    if (-2 == result) /* not Linux 2.6 V4L2 */
      {
	result = setup_v4l2 (device_name, device_fd, &xs->target);
	if (-2 == result) /* not V4L2 0.20 */
	  {
	    result = setup_v4l (device_name, device_fd, &xs->target);
	    if (-2 == result) /* not V4L */
	      {
		if (NULL != device_name)
		  errmsg (_("%s is not a V4L or V4L2 device."),
			  device_name);
		else if (-1 != device_fd)
		  errmsg (_("File descriptor %d is not a V4L or V4L2 device."),
			  device_fd);

		goto failure;
	      }
	  }
      }

    if (result < 0)
      goto failure;
  }

  message (/* verbosity */ 1,
	   "Setup completed.\n");

  return EXIT_SUCCESS;

 failure:
  message (/* verbosity */ 1,
	   "Setup failed.  Try -v or -vv for more details.\n");

  return EXIT_FAILURE;
}

/*
Local Variables:
coding: utf-8
End:
 */
