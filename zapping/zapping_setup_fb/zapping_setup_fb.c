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
  security flaws here, please mail me to <garetxe@euskalnet.net>
  This program returns 1 in case of error
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifdef ENABLE_V4L
#include <stdio.h>
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
#include <X11/extensions/xf86dga.h>
#endif
#include <X11/Xutil.h>

#include "../common/videodev.h" /* V4L header file */

#define FALSE (0)
#define TRUE (!FALSE)

#define MAX_VERBOSE 2 /* Greatest verbosity allowed */
#define ZSFB_VERSION "zapping_setup_fb 0.8.9" /* Current program version */
#define MY_NAME "zapping_setup_fb"

/* Well, this isn't very clean, but anyway... */
#define EXIT { \
               close(fd); \
               XCloseDisplay(display); \
               return 1; \
             }
             

/* Current DGA values */
int vp_width, vp_height, width, height, addr, bpp;

int verbosity = 0; /* Start quiet */

/* Display bpp, if specified */
int real_bpp = -1;

/* Prints a short usage notice */
void PrintUsage(void);

/* Prints a message only if the verbosity >= min_message*/
void PM(char * message, int min_message);

/* Returns TRUE if DGA can be found and works correctly */
int check_dga(Display * display, int screen);

void PrintUsage(void)
{
  printf("Usage:\n"
	 " zapping_setup_fb [OPTIONS], where OPTIONS stands for\n"
	 " --device dev - The video device to open, /dev/video0 by default\n"
	 " --display d  - The X display to use\n"
	 " --bpp x      - Current X bpp\n"
	 " --verbose    - Increments verbosity level\n"
	 " --quiet      - Decrements verbosity level\n"
	 " --help, -?   - Shows this message\n"
	 " --usage      - The same as --help or -?\n"
	 " --version    - Shows the program version\n");
}

void PM(char * message, int min_message)
{
  if (min_message <= verbosity)
    fprintf(stderr, message);
}

int check_dga(Display * display, int screen)
{
#ifndef DISABLE_X_EXTENSIONS
  int event_base, error_base;
  int major_version, minor_version;
  int flags;
  int banksize, memsize;
  char buffer[256];
  Window root;
  XVisualInfo *info, template;
  XPixmapFormatValues *pf;
  XWindowAttributes wts;
  int found, v, i, n;

  buffer[255] = 0;
  bpp = 0;

  if (!XF86DGAQueryExtension(display, &event_base, &error_base))
    {
      if (verbosity)
	perror("XF86DGAQueryExtension");
      return FALSE;
    }

  if (!XF86DGAQueryVersion(display, &major_version, &minor_version))
    {
      if (verbosity)
	perror("XF86DGAQueryVersion");
      return FALSE;
    }

  if (!XF86DGAQueryDirectVideo(display, screen, &flags))
    {
      if (verbosity)
	perror("XF86DGAQueryDirectVideo");
      return FALSE;
    }

  if (!(flags & XF86DGADirectPresent))
    {
      if (verbosity)
	fprintf(stderr,
		"No DirectVideo present (according to DGA extension %d.%d)\n",
		major_version, minor_version);
      return FALSE;
    }

  if (!XF86DGAGetVideoLL(display, screen, &addr, &width, &banksize,
			 &memsize))
    {
      if (verbosity)
	perror("XF86DGAGetVideoLL");
      return FALSE;
    }

  if (!XF86DGAGetViewPortSize(display, screen, &vp_width, &vp_height))
    {
      if (verbosity)
	perror("XF86DGAGetViewPortSize");
      return FALSE;
    }

  PM("Heuristic bpp search\n", 2);

  /* Get the bpp value */
  root = DefaultRootWindow(display);
  XGetWindowAttributes(display, root, &wts);
  height = wts.height;

  /* The following code is 'stolen' from v4l-conf, since I hadn't the
     slightest idea on how to get the real bpp */
  if (real_bpp == -1) {
    template.screen = screen;
    info = XGetVisualInfo(display, VisualScreenMask, &template, &found);
    v = -1;
    for (i = 0; v == -1 && i < found; i++)
      if (info[i].class == TrueColor && info[i].depth >= 15)
	v = i;
    if (-1 == v) {
      if (verbosity)
	fprintf(stderr, "x11: no approximate visual available\n");
      return FALSE;
    }
    
    /* get depth + bpp (heuristic) */
    pf = XListPixmapFormats(display,&n);
    for (i = 0; i < n; i++) {
      if (pf[i].depth == info[v].depth) {
	bpp   = pf[i].bits_per_pixel;
	break;
      }
    }
    if (0 == bpp) {
      if (verbosity)
	fprintf(stderr,"x11: can't figure out framebuffer depth\n");
      return FALSE;
    }
  } else
    bpp = real_bpp;
  
  /* Print some info about the DGA device in --verbose mode */
  /* This is no security flaw since this info is user-readable anyway */
  PM("DGA info we got:\n", 1);
  snprintf(buffer, 255, " - Version    : %d.%d\n", major_version,
	   minor_version);
  PM(buffer, 1);
  snprintf(buffer, 255, " - Viewport   : %dx%d\n", vp_width,
	   vp_height);
  PM(buffer, 1);
  snprintf(buffer, 255, " - DGA info   : %d width at %p\n", width,
	   (void *)addr);
  PM(buffer, 1);
  snprintf(buffer, 255, " - Screen bpp : %d\n", bpp);
  PM(buffer, 1);

  return TRUE;
#else
  PM("X extensions have been disabled, " MY_NAME " won't work\n", 1);
  return FALSE;
#endif
}

int main(int argc, char * argv[])
{
  char * video_device = "/dev/video0";
  char * display_name = getenv("DISPLAY");
  Display * display; /* X Display */
  int fd;
  int screen;
  int i; /* For args parsing */
  int print_usage = FALSE; /* --help, -? or --usage has been
				   specified */
  struct video_capability caps;
  struct video_buffer fb; /* The framebuffer device */
  int show_version = FALSE;
  int uid=getuid(), euid=geteuid();

  /* Parse given args */
  for (i = 1; i < argc; i++)
    {
      if (!strcasecmp(argv[i], "--device"))
	{
	  /* New video device specified */
	  if ((i+1) == argc)
	    fprintf(stderr,
		    "Video device name seems to be missing\n");
	  else
	    video_device = argv[++i];
	}
      else if (!strcasecmp(argv[i], "--display"))
	{
	  /* X display to use */
	  if ((i+1) == argc)
	    fprintf(stderr, "X display name seems to be missing\n");
	  else
	    display_name = argv[++i];
	}
      else if (!strcasecmp(argv[i], "--bpp"))
	{
	  /* We are told the real screen depth, no need for heuristics
	   */
	  if ((i+1) == argc)
	    fprintf(stderr, "Real bpp seems to be missing\n");
	  else if (!sscanf(argv[++i], "%d", &real_bpp))
	    fprintf(stderr, "Impossible to understand --bpp %s, ignored\n",
		    argv[i-1]);
	}
      else if ((!strcasecmp(argv[i], "--help")) ||
	       (!strcasecmp(argv[i], "--usage")) ||
	       (!strcasecmp(argv[i], "-?")))
	print_usage = TRUE;
      else if (!strcasecmp(argv[i], "--verbose"))
	{
	  if (verbosity < MAX_VERBOSE)
	    verbosity ++;
	}
      else if (!strcasecmp(argv[i], "--quiet"))
	{
	  if (verbosity > 0)
	    verbosity --;
	}
      else if (!strcasecmp(argv[i], "--version"))
	show_version = TRUE;
      else
	fprintf(stderr,
		"Unknown command line option %s, ignoring it\n", argv[i]);
    }

  if (show_version)
    {
      printf("%s\n", ZSFB_VERSION);
      return 0;
    }

  if (verbosity)
    /* Print a short copyright notice if we aren't totally quiet */
    printf("(C) 2000-2001 Iñaki García Etxebarria.\n"
	     "This program is under the GNU General Public License.\n");

  /* The user has asked for help, give it and exit */
  if (print_usage)
    {
      PrintUsage();
      return 0;
    }

  if (!video_device)
    {
      PM("Video device not given", 1);
      PrintUsage();
      return 1;
    }

  /* Print the scanned video device*/
  PM("Using video device ", 1);
  PM(video_device, 1);
  PM("\n", 1);

  PM("Sanity checking given name...\n", 2);

  /* Do a sanity check on the given name */
  /* This can stop some dummy attacks, like giving /dev/../desired_dir
   */
  for (i=0; i < strlen(video_device); i++)
    {
      if (video_device[i] == '.')
	{
	  fprintf(stderr,
		  "Given device %s has dots in it, cannot use it\n",
		  video_device);
	  return 1;
	}
    }

  /* Check that it's on the /dev/ directory */
  if (strlen(video_device) < 7) /* Minimum size */
    {
      fprintf(stderr,
	      "Given device %s is too short, cannot use it\n",
	      video_device);
      return 1;      
    }

  if (strncmp(video_device, "/dev/", 5))
    {
      fprintf(stderr,
	      "Given device %s doesn't start with /dev/, cannot use it\n",
	      video_device);
      return 1;
    }

  PM("Opening video device\n", 2);

  /* Open the video device */
  fd = open (video_device, O_TRUNC);
  if (fd <= 0)
    {
      if (verbosity)
	perror("open()");
      if (verbosity)
	fprintf(stderr, 
		"Cannot open given device %s\n", video_device);
      return 1;
    }

  PM("Querying video device capabilities\n", 2);
  if (ioctl(fd, VIDIOCGCAP, &caps))
    {
      if (verbosity)
	perror("VIDIOCGCAP");
      close(fd);
      return 1;
    }

  PM("Checking returned capabilities for overlay devices\n", 2);
  if (!(caps.type & VID_TYPE_OVERLAY))
    {
      if (verbosity)
	fprintf(stderr,
		"The given device doesn't have the VID_TYPE_OVERLAY flag\n"
		"set, " MY_NAME " is nonsense for the device.\n");
      close(fd);
      return 1;
    }

  /* Drop root privileges if we are root and the real user id
   * isn't root so we can access the .Xauthority file if it is
   * on NFS.
   */
  PM("Dropping SUID privilages\n", 2);
  if(!euid && uid)
  {
       if(seteuid(uid)==-1)
       {
               fprintf(stderr,"uid %d, euid %d, ", uid, euid);
               perror("but can't set euid");
       }
  }
  /* OK, the video device seems to work, open the given X display */
  PM("Opening X display\n", 1);
  display = XOpenDisplay(display_name);
  if (!display)
    {
      fprintf(stderr,
	      "Cannot open display %s, quitting\n", display_name);
      close(fd);
      return 1;
    }
  /* Restore root privileges. */
  if(!euid && uid)
  {
       if(seteuid(euid)==-1)
       {
               fprintf(stderr,"uid %d, euid %d, ", uid, euid);
               perror("but can't set euid");
       }
  }

  uid=getuid();
  euid=geteuid();
  PM("Getting default screen for the given display\n", 2);
  screen = XDefaultScreen(display);

  PM("Checking DGA for the given display and screen\n", 1);
  if (!check_dga(display, screen))
    {
      close(fd);
      XCloseDisplay(display);
      return 1;
    }
 
  /* OK, the DGA is working and we have its info, set up the V4L2
     overlay */
  PM("Getting current FB characteristics\n", 2);
  if (ioctl(fd, VIDIOCGFBUF, &fb))
    {
      if (verbosity)
	perror("VIDIOCGFBUF");
      EXIT
    }

  fb.base = (void *) addr;
  fb.width = width;
  fb.height = vp_height;
  fb.depth = bpp;
  fb.bytesperline = width * ((fb.depth+7) >> 3);

  PM("Setting new FB characteristics\n", 2);
  if (ioctl(fd, VIDIOCSFBUF, &fb))
    {
      if ((errno == EPERM) && (geteuid()))
	PM(MY_NAME " must be run as root, or marked as SUID root\n",
	   0);
      if (verbosity)
	perror("VIDIOCSFBUF");
      EXIT
    }

  /* Everything seems to have worked, exit */
  PM("No errors, exiting...\n", 2);
  PM("Closing X display and video device\n", 2);
  XCloseDisplay(display);
  close(fd);
  return 0;
}
#else /* !ENABLED_V4L */

/* stub */
int main(int argc, char *argv[])
{
  return 0;
}

#endif
