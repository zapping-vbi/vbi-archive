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
  frame buffer and making V4L2 go into Overlay mode. If you find some
  security flaws here, please mail me to <garetxe@euskalnet.net>
  This program returns 1 in case of error
*/

#include <stdio.h>
#include "../tveng.h" /* V4L2 header file */
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#define MAX_VERBOSE 2 /* Greatest verbosity allowed */
#define VERSION "zapping_setup_fb 0.8" /* Current program version */

/* Well, this isn't very clean, but anyway... */
#define EXIT { \
               close(fd); \
               XCloseDisplay(display); \
               return 1; \
             }
             

/* Current DGA values */
int vp_width, vp_height, width, height, addr, bpp;

int verbosity = 0; /* Start quiet */

/* Prints a short usage notice */
void PrintUsage(void);

/* Prints a message only if the verbosity >= min_message*/
void PM(char * message, int min_message);

/* Returns TRUE if DGA can be found and works correctly */
gboolean check_dga(Display * display, int screen);

void PrintUsage(void)
{
  printf(_("Usage:\n"
	   " zapping_setup_fb [OPTIONS], where OPTIONS stands for\n"
	   " --device dev - The video device to open, /dev/video by default\n"
	   " --display d  - The X display to use\n"
	   " --verbose    - Increments verbosity level\n"
	   " --quiet      - Decrements verbosity level\n"
	   " --help, -?   - Shows this message\n"
	   " --usage      - The same as --help or -?\n"
	   " --version    - Shows the program version\n"));
}

void PM(char * message, int min_message)
{
  if (min_message <= verbosity)
    printf(message);
}

gboolean check_dga(Display * display, int screen)
{
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
      perror("XF86DGAQueryExtension");
      return FALSE;
    }

  if (!XF86DGAQueryVersion(display, &major_version, &minor_version))
    {
      perror("XF86DGAQueryVersion");
      return FALSE;
    }

  if (!XF86DGAQueryDirectVideo(display, screen, &flags))
    {
      perror("XF86DGAQueryDirectVideo");
      return FALSE;
    }

  if (!(flags & XF86DGADirectPresent))
    {
      fprintf(stderr,
	      "No DirectVideo present (according to DGA extension %d.%d)\n",
	      major_version, minor_version);
      return FALSE;
    }

  if (!XF86DGAGetVideoLL(display, screen, &addr, &width, &banksize,
			 &memsize))
    {
      perror("XF86DGAGetVideoLL");
      return FALSE;
    }

  if (!XF86DGAGetViewPortSize(display, screen, &vp_width, &vp_height))
    {
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
  template.screen = screen;
  info = XGetVisualInfo(display, VisualScreenMask, &template, &found);
    v = -1;
    for (i = 0; v == -1 && i < found; i++)
	if (info[i].class == TrueColor && info[i].depth >= 15)
	    v = i;
    if (-1 == v) {
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
	fprintf(stderr,"x11: can't figure out framebuffer depth\n");
	return FALSE;
    }

  /* Print some info about the DGA device in --verbose mode */
  /* This is no security flaw since this info is user-readable anyway */
  PM("DGA info we got:\n", 1);
  g_snprintf(buffer, 255, " - Version    : %d.%d\n", major_version,
	     minor_version);
  PM(buffer, 1);
  g_snprintf(buffer, 255, " - Viewport   : %dx%d\n", vp_width,
	     vp_height);
  PM(buffer, 1);
  g_snprintf(buffer, 255, " - DGA info   : %d width at %p\n", width,
	     (gpointer)addr);
  PM(buffer, 1);
  g_snprintf(buffer, 255, " - Screen bpp : %d\n", bpp);
  PM(buffer, 1);

  return TRUE;
}

int main(int argc, char * argv[])
{
  char * video_device = "/dev/video";
  char * display_name = getenv("DISPLAY");
  Display * display; /* X Display */
  int fd;
  int screen;
  int i; /* For args parsing */
  gboolean print_usage = FALSE; /* --help, -? or --usage has been
				   specified */
  struct v4l2_capability caps;
  struct v4l2_framebuffer fb; /* The framebuffer device */
  gboolean show_version = FALSE;

  /* Parse given args */
  for (i = 1; i < argc; i++)
    {
      if (!strcasecmp(argv[i], "--device"))
	{
	  /* New video device specified */
	  if ((i+1) == argc)
	    printf(_("Video device name seems to be missing\n"));
	  else
	    video_device = argv[++i];
	}
      else if (!strcasecmp(argv[i], "--display"))
	{
	  /* X display to use */
	  if ((i+1) == argc)
	    printf(_("X display name seems to be missing\n"));
	  else
	    display_name = argv[++i];
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
	printf(_("Unknown command line option %s, ignoring it\n"), argv[i]);
    }

  if (show_version)
    {
      printf("%s\n", VERSION);
      return 0;
    }

  if (verbosity)
    /* Print a short copyright notice if we aren't totally quiet */
    printf(_("(C) 2000 Iñaki García Etxebarria.\n"
	     "This file is under the GNU General Public License.\n"));

  /* The user has asked for help, give it and exit */
  if (print_usage)
    {
      PrintUsage();
      return 0;
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
	  printf(_("Given device %s has dots in it, cannot use it\n"),
		 video_device);
	  return 1;
	}
    }

  /* Check that it's on the /dev/ directory */
  if (strlen(video_device) < 7) /* Minimum size */
    {
      printf(_("Given device %s is too short, cannot use it\n"),
	     video_device);
      return 1;      
    }

  if (strncmp(video_device, "/dev/", 5))
    {
      printf(_("Given device %s doesn't start with /dev/, cannot use it\n"),
	     video_device);
      return 1;
    }

  PM("Opening video device\n", 2);

  /* Open the video device */
  fd = open (video_device, O_NOIO);
  if (fd <= 0)
    {
      printf(_("Cannot open given device %s\n"), video_device);
      return 1;
    }

  PM("Querying video device capabilities\n", 2);
  if (ioctl(fd, VIDIOC_QUERYCAP, &caps))
    {
      perror("VIDIOC_QUERYCAP");
      close(fd);
      return 1;
    }

  PM("Checking returned capabilities for overlay devices\n", 2);
  if (!(caps.flags & V4L2_FLAG_PREVIEW))
    {
      printf("The given device doesn't have the V4L2_FLAG_PREVIEW flag\n"
	     "set, this program is nonsense then for this device.\n");
      close(fd);
      return 1;
    }

  /* OK, the video device seems to work, open the given X display */
  PM("Opening X display\n", 1);
  display = XOpenDisplay(display_name);
  if (!display)
    {
      printf(_("Cannot open display %s, quitting\n"), display_name);
      close(fd);
      return 1;
    }

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
  if (ioctl(fd, VIDIOC_G_FBUF, &fb))
    {
      perror("VIDIOC_G_FBUF");
      EXIT
    }

  fb.base[0] = fb.base[1] = fb.base[2] = (gpointer) addr;
  fb.fmt.width = vp_width;
  fb.fmt.height = vp_height;
  fb.fmt.depth = bpp;
  switch (fb.fmt.depth)
    {
    case 15:
      fb.fmt.pixelformat = V4L2_PIX_FMT_RGB555;
      break;
    case 16:
      fb.fmt.pixelformat = V4L2_PIX_FMT_RGB565;
      break;
    case 24:
      fb.fmt.pixelformat = V4L2_PIX_FMT_BGR24;
      break;
    case 32:
      fb.fmt.pixelformat = V4L2_PIX_FMT_BGR32;
      break;
    default:
      fprintf(stderr, _("Your screen depth %d isn't supported, exiting\n"),
	      fb.fmt.depth);
      EXIT;
    };

  /* Go to the primary screen */
  fb.fmt.flags = V4L2_FMT_FLAG_BYTESPERLINE;
  fb.fmt.bytesperline = width * ((fb.fmt.depth+7) >> 3);
  fb.fmt.sizeimage = fb.fmt.bytesperline * vp_height;
  fb.flags = V4L2_FBUF_FLAG_PRIMARY;

  PM("Setting new FB characteristics\n", 2);
  if (ioctl(fd, VIDIOC_S_FBUF, &fb))
    {
      perror("VIDIOC_S_FBUF");
      EXIT
    }

  /* Everything seems to have worked, exit */
  PM("No errors, exiting...\n", 2);
  PM("Closing X display and video device\n", 2);
  XCloseDisplay(display);
  close(fd);
  return 0;
}
