/* Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2000 Iñaki García Etxebarria
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
  This is intended to be an auxiliary suid program for setting up the
  frame buffer and making V4L go into Overlay mode. If you find some
  security flaws here, please report at zapping-misc@lists.sourceforge.net.
  This program returns 1 in case of error
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifdef ENABLE_V4L

#include <stdio.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <errno.h>
#include <linux/kernel.h>
#include <errno.h>

/* We need video extensions (DGA) */

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xfuncs.h>
#ifndef DISABLE_X_EXTENSIONS
#  include <X11/extensions/xf86dga.h>
#endif
#include <X11/Xutil.h>

#include "../common/videodev.h" /* V4L header file */

#define FALSE 0
#define TRUE 1

#define ROOT_UID 0

#define MAX_VERBOSE 2				/* Greatest verbosity allowed */
#define ZSFB_VERSION "zapping_setup_fb 0.9"	/* Current program version */

static char *		my_name;
static int		verbosity = 1;

/* Current DGA values */

static int		vp_width;
static int		vp_height;
static int		width;
static int		height;
static int		addr;
static int		bpp;


#define STF2(x) #x
#define STF1(x) STF2(x)

#define errmsg(template, args...)					\
do {									\
  if (verbosity > 0)							\
    fprintf (stderr, "%s:" __FILE__ ":" STF1(__LINE__) ": "		\
	     template ": %d, %s\n", my_name , ##args,			\
	     errno, strerror (errno));					\
} while (0)

#define message(level, template, args...)				\
do {									\
  if ((int) level <= verbosity)						\
    fprintf (stderr, template , ##args);				\
} while (0)


#ifdef DISABLE_X_EXTENSIONS

static int
check_dga			(Display *		display,
				 int			screen,
				 int			bpp_arg)
{
  message (1, "X extensions have been disabled, %s won't work\n", my_name);
  return FALSE;
}

#else /* !DISABLE_X_EXTENSIONS */

static int
check_dga			(Display *		display,
				 int			screen,
				 int			bpp_arg)
{
  int event_base, error_base;
  int major_version, minor_version;
  int flags;
  int banksize, memsize;
  char buffer[256];
  Window root;
  XVisualInfo *info, templ;
  XPixmapFormatValues *pf;
  XWindowAttributes wts;
  int found, v, i, n;

  buffer[255] = 0;
  bpp = 0;

  if (!XF86DGAQueryExtension (display, &event_base, &error_base))
    {
      errmsg ("XF86DGAQueryExtension() failed");
      return FALSE;
    }

  if (!XF86DGAQueryVersion (display, &major_version, &minor_version))
    {
      errmsg ("XF86DGAQueryVersion() failed");
      return FALSE;
    }

  if (!XF86DGAQueryDirectVideo (display, screen, &flags))
    {
      errmsg ("XF86DGAQueryDirectVideo() failed");
      return FALSE;
    }

  if (!(flags & XF86DGADirectPresent))
    {
      message (1, "No DirectVideo present (according to DGA extension %d.%d)\n",
	       major_version, minor_version);
      return FALSE;
    }

  if (!XF86DGAGetVideoLL (display, screen, &addr, &width, &banksize, &memsize))
    {
      errmsg ("XF86DGAGetVideoLL() failed");
      return FALSE;
    }

  if (!XF86DGAGetViewPortSize (display, screen, &vp_width, &vp_height))
    {
      errmsg ("XF86DGAGetViewPortSize() failed");
      return FALSE;
    }

  message (2, "Heuristic bpp search\n");

  /* Get the bpp value */

  root = DefaultRootWindow(display);
  XGetWindowAttributes(display, root, &wts);
  height = wts.height;

  /* The following code is 'stolen' from v4l-conf, since I hadn't the
     slightest idea on how to get the real bpp */

  if (bpp_arg == -1)
    {
      templ.screen = screen;

      info = XGetVisualInfo (display, VisualScreenMask, &templ, &found);

      for (i = 0, v = -1; v == -1 && i < found; i++)
        if (info[i].class == TrueColor && info[i].depth >= 15)
	  v = i;

      if (-1 == v)
        {
          message (1, "No appropriate X visual available\n");
          return FALSE;
        }

      /* get depth + bpp (heuristic) */
      pf = XListPixmapFormats(display,&n);

      for (i = 0; i < n; i++)
        if (pf[i].depth == info[v].depth)
	  {
	    bpp = pf[i].bits_per_pixel;
	    break;
          }

      if (0 == bpp)
        {
          message (1, "Cannot figure out framebuffer depth\n");
          return FALSE;
        }
    }
  else
    {
      bpp = bpp_arg;
    }

  /* Print some info about the DGA device in --verbose mode */
  /* This is no security flaw since this info is user-readable anyway */

  message (2, "DGA info we got:\n");
  message (2, " - Version    : %d.%d\n", major_version, minor_version);
  message (2, " - Viewport   : %dx%d\n", vp_width, vp_height);
  message (2, " - DGA info   : %d width at %p\n", width, (void *) addr);
  message (2, " - Screen bpp : %d\n", bpp);

  return TRUE;
}

#endif /* DISABLE_X_EXTENSIONS */


static int
drop_root_privileges		(int			uid,
				 int			euid)
{
  if (euid == ROOT_UID && uid != ROOT_UID)
    {
      message (2, "Dropping root privileges\n");

      if (seteuid (uid) == -1)
        {
	  errmsg ("Cannot drop root privileges "
		  "despite uid=%d, euid=%d", uid, euid);
	  return FALSE;
        }
    }

  return TRUE;
}

static int
restore_root_privileges		(int			uid,
				 int			euid)
{
  if (euid == ROOT_UID && uid != ROOT_UID)
    {
      message (2, "Restoring root privileges\n");

      if (seteuid (euid) == -1)
        {
	  errmsg ("Cannot restore root privileges "
		  "despite uid=%d, euid=%d", uid, euid);
	  return FALSE;
        }
    }

  return TRUE;
}

static const char *	short_options = "d:D:b:vqh?V";

static struct option
long_options [] =
{
  { "device",		required_argument,	0, 'd' },
  { "display",		required_argument,	0, 'D' },
  { "bpp",		required_argument,	0, 'b' },
  { "verbose",		no_argument,		0, 'v' },
  { "quiet",		no_argument,		0, 'q' },
  { "help",		no_argument,		0, 'h' },
  { "usage",		no_argument,		0, 'h' },
  { "version",		no_argument,		0, 'V' },
};

static void
PrintUsage			(void)
{
  printf("Usage:\n"
	 " %s [OPTIONS], where OPTIONS stands for\n"
	 " --device dev - The video device to open, /dev/video0 by default\n"
	 " --display d  - The X display to use\n"
	 " --bpp x      - Current X bpp\n"
	 " --verbose    - Increments verbosity level\n"
	 " --quiet      - Decrements verbosity level\n"
	 " --help, -h   - Shows this message\n"
	 " --usage      - The same as --help or -h\n"
	 " --version    - Shows the program version\n"
	 "", my_name);
}


int
main				(int			argc,
				 char **		argv)
{
  char *device_name;
  char *display_name;
  Display *display;
  int screen;
  int bpp_arg;
  int fd;
  struct video_capability caps;
  struct video_buffer fb; /* The framebuffer device */
  int uid, euid;

  fd = -1;
  display = 0;

  my_name = argv[0];

  /* Drop root privileges until we need them */

  uid = getuid ();
  euid = geteuid ();

  if (!drop_root_privileges (uid, euid))
    goto failure;

  /* Parse arguments */

  device_name = "/dev/video0";
  display_name = getenv("DISPLAY");
  bpp_arg = -1;

  for (;;)
    {
      int c = getopt_long (argc, argv, short_options, long_options, NULL);

      if (c == -1)
        break;

      switch (c)
        {
	case 'd':
	  device_name = strdup (optarg);
	  break;
	    
	case 'D':
	  display_name = strdup (optarg);
	  break;

	case 'b':
	  bpp_arg = strtol (optarg, NULL, 0);
	    
	  if (bpp_arg < 8 || bpp_arg > 32)
	    {
	      message (1, "Invalid bpp argument %d\n", bpp_arg);
	      goto failure;
	    }

	  break;

	case 'v':
	  if (verbosity < MAX_VERBOSE)
	    verbosity++;
	  break;

	case 'q':
	  if (verbosity > 0)
	    verbosity--;
	  break;

	case 'V':
	  message (0, "%s\n", ZSFB_VERSION);
	  exit (EXIT_SUCCESS);

	case 'h':
	  PrintUsage();
	  exit (EXIT_SUCCESS);

	default:
	  /* getopt_long prints option name when unknown or arg missing */
	  PrintUsage();
	  goto failure;
	}
    }

  message (1, "(C) 2000-2001 Iñaki García Etxebarria.\n"
	   "This program is under the GNU General Public License.\n");

  message (1, "Using video device '%s', display '%s'\n",
	   device_name, display_name);

  message (2, "Sanity checking device name...\n");

  /* Do a sanity check on the given name */
  /* This can stop some dummy attacks, like giving /dev/../desired_dir
   */
  if (strchr (device_name, '.'))
    {
      message (1, "Device name '%s' rejected, has dots\n", device_name);
      goto failure;
    }

  /* Check that it's on the /dev/ directory */
  if (strlen (device_name) < 7) /* Minimum size */
    {
      message (1, "Device name '%s' rejected, is too short\n", device_name);
      goto failure;
    }
  else if (strncmp (device_name, "/dev/", 5))
    {
      message (1, "Device name '%s' rejected, must start "
	       "with '/dev/'\n", device_name);
      goto failure;
    }

  message (2, "Opening video device\n");

  if ((fd = open (device_name, O_TRUNC)) == -1)
    {
      errmsg ("Cannot open device '%s'", device_name);
      goto failure;
    }

  message (2, "Querying video device capabilities\n");

  if (ioctl (fd, VIDIOCGCAP, &caps))
    {
      errmsg ("VIDIOCGCAP ioctl failed");
      goto failure;
    }

  message (2, "Checking returned capabilities for overlay devices\n");

  if (!(caps.type & VID_TYPE_OVERLAY))
    {
      message (1, "Device '%s' does not support video overlay\n", device_name);
      goto failure;
    }

  message (2, "Opening X display\n");

  if ((display = XOpenDisplay (display_name)) == 0)
    {
      message (1, "Cannot open display '%s'", display_name);
      goto failure;
    }

  message (2, "Getting default screen for the given display\n");
  screen = XDefaultScreen (display);

  message (2, "Checking DGA for the given display and screen\n");
  if (!check_dga (display, screen, bpp_arg))
    goto failure;
 
  /* OK, the DGA is working and we have its info, set up the V4L2
     overlay */

  message (2, "Getting current FB characteristics\n");
  if (ioctl (fd, VIDIOCGFBUF, &fb))
    {
      errmsg ("VIDIOCGFBUF ioctl failed");
      goto failure;
    }

  fb.base = (void *) addr;
  fb.width = width;
  fb.height = vp_height;
  fb.depth = bpp;
  fb.bytesperline = width * ((fb.depth + 7) >> 3);

  message (2, "Setting new FB characteristics\n");

  /*
   *  This ioctl is privileged because it sets up
   *  DMA to a random (video memory) address. 
   */
  {
    int success;

    if (!restore_root_privileges (uid, euid))
      goto failure;

    success = ioctl (fd, VIDIOCSFBUF, &fb);

    if (!drop_root_privileges (uid, euid))
      ; /* ignore */

    if (success == -1)
      {
        errmsg ("VIDIOCSFBUF ioctl failed");

        if (errno == EPERM && euid != ROOT_UID)
	  message (1, "%s must be run as root, "
		   "or marked as SUID root\n", my_name);

        goto failure;
      }
  }

  message (2, "No errors, exiting...\n");

  XCloseDisplay (display);
  close (fd);

  return EXIT_SUCCESS;

 failure:
  if (display != 0)
    XCloseDisplay (display);

  if (fd != -1)
    close (fd);

  return EXIT_FAILURE;
}

#else /* !ENABLED_V4L */

int
main				(int			argc,
				 char **		argv)
{
  return EXIT_FAILURE;
}

#endif /* !ENABLED_V4L */
