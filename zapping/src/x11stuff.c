/* Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2000-2001 Iñaki García Etxebarria
 *
 * The wmhooks code from xawtv 3.58 (C) Gerd Knorr
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
 * the routines contained here are all the X-specific stuff in Zapping
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#define ZCONF_DOMAIN "/zapping/options/main/"
#include "zconf.h"
#include "x11stuff.h"
#include "zmisc.h"

#ifndef DISABLE_X_EXTENSIONS
#ifdef HAVE_LIBXDPMS
#define USE_XDPMS 1
#include <X11/extensions/dpms.h>
#endif
#endif

/*
 * Returns a pointer to the data contained in the given GdkImage
 */
gpointer
x11_get_data(GdkImage * image)
{
  return (image -> mem);
}

/*
 * Returns the byte order of the X server
 */
GdkByteOrder
x11_get_byte_order(void)
{
  if (ImageByteOrder(GDK_DISPLAY()) == LSBFirst)
    return GDK_LSB_FIRST;

  return GDK_MSB_FIRST;
}

/*
 * Returns the bits per pixel needed for a GdkImage. -1 on error.
 */
gint
x11_get_bpp(void)
{
  GdkImage * tmp_image;
  gint result;

  tmp_image =
    gdk_image_new(GDK_IMAGE_FASTEST, gdk_visual_get_system(), 16, 16);

  if (!tmp_image)
    return -1;

  result = tmp_image->depth;

  if (result == 24 &&
      tmp_image->bpp == 4)
    result = 32;

  gdk_image_destroy(tmp_image);

  return result;
}

/*
 * Adds a clip to the given struct and incs num_clips.
 */
static
void x11_add_clip(int x1, int y1, int x2, int y2,
		  struct tveng_clip ** clips, gint* num_clips)
{
  /* the border is because of the possible dword-alignings */
  *clips = g_realloc(*clips, ((*num_clips)+1)*sizeof(struct tveng_clip));
  (*clips)[*num_clips].x = x1;
  (*clips)[*num_clips].y = y1;
  (*clips)[*num_clips].width = x2-x1;
  (*clips)[*num_clips].height = y2-y1;
  (*num_clips)++;
}

/*
 * Do-nothing error handler. At certain times errors (specially
 * BadWindow) should be ignored. For example, when getting the clips,
 * a window can disappear while we are checking other windows. This
 * window will still be in our array, but will raise a BadWindow when
 * getting its attributes. Btw, this isn't a hack ;-)
 */
static
int xerror(Display * dpy, XErrorEvent *event)
{
  return 0;
}

/**
 * Stores in X, Y the absolute position (relative to the root window)
 * of the given window
 */
static void
get_absolute_pos(Window w, Window root, gint *x, gint *y)
{
  Display *dpy = GDK_DISPLAY();
  Window *children;
  XWindowAttributes wts;
  int nchildren;

  *x = *y = 0;

  while (w != root)
    {
      XGetWindowAttributes(dpy, w, &wts);

      *x += wts.x;
      *y += wts.y;

      XQueryTree(GDK_DISPLAY(), w, &root, &w, &children, &nchildren);
      if (children)
	XFree(children);
    }
}


/*
 * Returns a pointer to a clips struct (that you need to g_free()
 * afterwards if not NULL).
 * Pass the GdkWindow that you want to get the clip status of.
 * x, y, w, h are the coords of the overlay in that window.
 */
struct tveng_clip *
x11_get_clips(GdkWindow *win, gint x, gint y, gint w, gint h,
	      gint *return_num_clips)
{
  struct tveng_clip * clips = NULL;
  int x1,y1,x2,y2;
  Display *dpy = GDK_DISPLAY();
  XWindowAttributes wts;
  Window root, me, rroot, parent, *children;
  uint nchildren, i;
  gint num_clips=0;
  int wx, wy, wwidth, wheight, swidth, sheight;
  XErrorHandler olderror;

  static void get_children_clips(Window id, Window stack_level)
    {
      gint paren_x, paren_y;

      get_absolute_pos(id, root, &paren_x, &paren_y);

      XQueryTree(dpy, id, &rroot, &parent, &children, &nchildren);

      for (i = 0; i < nchildren; i++)
	if (children[i]==stack_level)
	  break;

      if (i == nchildren)
	i = 0;
      else
	i++;

      /* enter error-ignore mode */
      olderror = XSetErrorHandler(xerror);
      for (; i<nchildren; i++) {
	XGetWindowAttributes(dpy, children[i], &wts);
	if (!(wts.map_state & IsViewable))
	  continue;
    
	x1=(wts.x+paren_x)-wx;
	y1=(wts.y+paren_y)-wy;
	x2=x1+wts.width+2*wts.border_width;
	y2=y1+wts.height+2*wts.border_width;
	if ((x2 < 0) || (x1 > (int)wwidth) || (y2 < 0) || (y1 > (int)wheight))
	  continue;
    
	if (x1<0)            x1=0;
	if (y1<0)            y1=0;
	if (x2>(int)wwidth)  x2=wwidth;
	if (y2>(int)wheight) y2=wheight;
	x11_add_clip(x1, y1, x2, y2, &clips, &num_clips);
      }
      XSetErrorHandler(olderror);
      /* leave error-ignore mode */
      
      if (children)
	XFree((char *) children);
    }

  if ((win == NULL) || (return_num_clips == NULL))
    return NULL;

  wx = x; wy = y; wwidth = w; wheight = h;

  swidth = gdk_screen_width();
  sheight = gdk_screen_height();
  if (wx<0)
    x11_add_clip(0, 0, (uint)(-wx), wheight, &clips, &num_clips);
  if (wy<0)
    x11_add_clip(0, 0, wwidth, (uint)(-wy), &clips, &num_clips);
  if ((wx+wwidth) > swidth)
    x11_add_clip(swidth-wx, 0, wwidth, wheight, &clips,
		   &num_clips);
  if ((wy+wheight) > sheight)
    x11_add_clip(0, sheight-wy, wwidth, wheight, &clips, &num_clips);
  
  root=GDK_ROOT_WINDOW();
  me=GDK_WINDOW_XWINDOW(win);

  /* Get the cliplist of the childs of a given window */
  get_children_clips(me, None);

  /* Walk up to the root window and get clips */
  for (;;) {
    XQueryTree(dpy, me, &rroot, &parent, &children, &nchildren);
    if (children)
      XFree((char *) children);
    if (root == parent)
      break;
    me = parent;
  }

  get_children_clips(root, me);

  *return_num_clips = num_clips;
  return clips;
}

/*
 * Maps and unmaps a window of the given (screen) geometry, thus
 * forcing an expose event in that area
 */
void
x11_force_expose(gint x, gint y, gint w, gint h)
{
  XSetWindowAttributes xswa;
  Window win;
  unsigned long mask;

  xswa.override_redirect = TRUE;
  xswa.backing_store = NotUseful;
  xswa.save_under = FALSE;
  mask = ( CWSaveUnder | CWBackingStore | CWOverrideRedirect );

  win = XCreateWindow(GDK_DISPLAY(), GDK_ROOT_WINDOW(), x, y, w, h, 
		      0, CopyFromParent, InputOutput, CopyFromParent,
		      mask, &xswa);

  XMapWindow(GDK_DISPLAY(), win);
  XUnmapWindow(GDK_DISPLAY(), win);

  XDestroyWindow(GDK_DISPLAY(), win);
}

#if 0
static void
_x11_force_expose(gint x, gint y, gint w, gint h)
{
  Display *dpy = GDK_DISPLAY();
  XWindowAttributes wts;
  Window root, rroot, parent, *children;
  uint nchildren, i;
  XErrorHandler olderror;
  XExposeEvent event;

  event.type = Expose;
  event.count = 0;

  root=GDK_ROOT_WINDOW();
  XQueryTree(dpy, root, &rroot, &parent, &children, &nchildren);
    
  /* enter error-ignore mode */
  olderror = XSetErrorHandler(xerror);
  for (i=0; i<nchildren; i++) {
    XGetWindowAttributes(dpy, children[i], &wts);
    if (!(wts.map_state & IsViewable))
      continue;
    if (wts.class != InputOutput)
      continue;
    if ((wts.x >= x+w) || (wts.y >= x+h) ||
    	(wts.x+wts.width < x) || (wts.y+wts.height < y))
      continue;
    
    event.window = children[i];
    event.x = 0;
    event.y = 0;
    event.width = wts.width;
    event.height = wts.height;
    XSendEvent(GDK_DISPLAY(), children[i], False,
  	       ExposureMask, (XEvent*)&event);
  }
  XSync (GDK_DISPLAY(), False);
  XSetErrorHandler(olderror);
  /* leave error-ignore mode */

  if (children)
    XFree((char *) children);
}
#endif

/*
 * Returns TRUE if the window is viewable
 */
gboolean
x11_window_viewable(GdkWindow *window)
{
  XWindowAttributes wts;

  XGetWindowAttributes(GDK_DISPLAY(), GDK_WINDOW_XWINDOW(window), &wts);

  return ((wts.map_state & IsViewable) ? TRUE : FALSE);
}

static gint
stop_xscreensaver_timeout (gpointer unused)
{
  system ("xscreensaver-command -deactivate >&- 2>&- &");

  return TRUE;
}

/*
 * Sets the X screen saver on/off
 */
void
x11_set_screensaver(gboolean on)
{
#ifdef USE_XDPMS
  static BOOL dpms_was_on;
  CARD16 dpms_state;
  int dummy;
#endif
  static int timeout=-2, interval, prefer_blanking, allow_exposures;
  static gint gtimeout = -1;

  if (on) {
    if (timeout == -2) {
      g_warning("cannot activate screensaver before deactivating");
      return;
    }
    XSetScreenSaver(GDK_DISPLAY(), timeout, interval, prefer_blanking,
		    allow_exposures);
    gtk_timeout_remove(gtimeout);
    gtimeout = -1;
#ifdef USE_XDPMS
    if ( (DPMSQueryExtension(GDK_DISPLAY(), &dummy, &dummy) ) &&
	 (DPMSCapable(GDK_DISPLAY())) && (dpms_was_on) )
      DPMSEnable(GDK_DISPLAY());
#endif
  } else {
    XGetScreenSaver(GDK_DISPLAY(), &timeout, &interval,
		    &prefer_blanking, &allow_exposures);
    /* This disables the builtin X screensaver... */
    XSetScreenSaver(GDK_DISPLAY(), 0, interval, prefer_blanking,
		    allow_exposures);
    /* and this XScreensaver. */
    //    gtimeout = gtk_timeout_add(55000, stop_xscreensaver_timeout, NULL);
    gtimeout = gtk_timeout_add(5000, stop_xscreensaver_timeout, NULL);

#ifdef USE_XDPMS
    if ( (DPMSQueryExtension(GDK_DISPLAY(), &dummy, &dummy)) &&
	 (DPMSCapable(GDK_DISPLAY())) ) {
      DPMSInfo(GDK_DISPLAY(), &dpms_state, &dpms_was_on);
      DPMSDisable(GDK_DISPLAY());
    }
#endif
  }
}

/**
 * Accelerated backends handling.
 */
extern gboolean		add_backend_xv(video_backend *xv);

static gint num_backends = 0;
static video_backend *backends = NULL;
static gboolean port_grabbed = FALSE;
static gint cur_backend = 0;

static inline void
add_backend(video_backend *p)
{
  backends = g_realloc(backends, sizeof(video_backend)*(num_backends+1));
  memcpy(backends + num_backends, p, sizeof(video_backend));
  num_backends++;
}

void	startup_xvz(void)
{
  video_backend tmp;
  gint i;

  if (add_backend_xv(&tmp))
    add_backend(&tmp);

  if (!num_backends)
    return;

  printv("* Registered output video backends:\n");
  for (i=0; i<num_backends; i++)
    printv("\t+ %s\n", backends[i].name);
}

void	shutdown_xvz(void)
{
  g_free(backends);
}

gboolean xvz_grab_port(tveng_device_info *info)
{
  gint i;

  g_assert(port_grabbed == FALSE);

  for (i = 0; i<num_backends; i++)
    if (backends[i].grab(info))
      break;

  if (i == num_backends)
    return FALSE;

  cur_backend = i;
  port_grabbed = TRUE;

  return TRUE;
}

void xvz_ungrab_port(tveng_device_info *info)
{
  if (!port_grabbed)
    return;

  port_grabbed = FALSE;
  backends[cur_backend].ungrab(info);
}

xvzImage * xvzImage_new(enum tveng_frame_pixformat pixformat,
			gint width, gint height)
{
  g_assert(port_grabbed == TRUE);

  return backends[cur_backend].image_new(pixformat, width, height);
}

void xvzImage_put(xvzImage *image, GdkWindow *window, GdkGC *gc)
{
  g_assert(port_grabbed == TRUE);

  backends[cur_backend].image_put(image, window, gc);
}

void xvzImage_destroy(xvzImage *image)
{
  backends[cur_backend].image_destroy(image);
}

#include <libgnomeui/gnome-winhints.h>
#include <zmisc.h>

static void
dummy_window_on_top		(GtkWidget *		widget,
				 gboolean		on)
{
}

/*
 * Some WindowManager specific stuff
 *
 */

#if defined(USE_WMHOOKS) && !defined(DISABLE_X_EXTENSIONS)

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>

/* ------------------------------------------------------------------------ */

void		(* window_on_top)	(GtkWidget *		widget,
					 gboolean		on);

/* ------------------------------------------------------------------------ */

static Atom net_wm;
static Atom net_wm_state;
static Atom net_wm_top;

#define _NET_WM_STATE_REMOVE        0    /* remove/unset property */
#define _NET_WM_STATE_ADD           1    /* add/set property */

static void
net_wm_window_on_top		(GtkWidget *		widget,
				 gboolean		on)
{
  GdkWindow *window = GTK_BIN (widget)->child->window;
  Display *dpy = GDK_DISPLAY ();
  Window win = GDK_WINDOW_XWINDOW (window);
  XEvent e;

    e.xclient.type = ClientMessage;
    e.xclient.message_type = net_wm_state;
    e.xclient.display = dpy;
    e.xclient.window = win;
    e.xclient.format = 32;
    e.xclient.data.l[0] = on ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE;
    e.xclient.data.l[1] = net_wm_top;
    e.xclient.data.l[2] = 0l;
    e.xclient.data.l[3] = 0l;
    e.xclient.data.l[4] = 0l;

    XSendEvent(dpy, DefaultRootWindow(dpy), False,
	       SubstructureRedirectMask, &e);
}

/* ------------------------------------------------------------------------ */

static Atom gnome;
static Atom gnome_layer;

/* Note sawfish (1.0.1) reports _WIN_LAYER compatibility but
   really supports only WIN_LAYER_NORMAL. Pity. */

static void
gnome_window_on_top		(GtkWidget *		widget,
				 gboolean		on)
{
  if (1)
    {
      /* Supports only Gnome WM hints */
      gnome_win_hints_set_layer (widget, on ? WIN_LAYER_ONTOP :
				 	      WIN_LAYER_NORMAL);
    }
  else /* from xawtv */
    {
      GdkWindow *window = GTK_BIN (widget)->child->window;
      Display *dpy = GDK_DISPLAY ();
      Window win = GDK_WINDOW_XWINDOW (window);
      XClientMessageEvent  xev;

      if (0 == win)
	return;

      memset(&xev, 0, sizeof(xev));
      xev.type = ClientMessage;
      xev.window = win;
      xev.message_type = gnome_layer;
      xev.format = 32;
      switch (on) {
      case -1: xev.data.l[0] = WIN_LAYER_BELOW;    break;
      case  0: xev.data.l[0] = WIN_LAYER_NORMAL;   break;
      case  1: xev.data.l[0] = WIN_LAYER_ONTOP;    break;
      }

      XSendEvent(dpy,DefaultRootWindow(dpy),False,
		 SubstructureNotifyMask,(XEvent*)&xev);
      if (on)
	XRaiseWindow(dpy,win);
    }
}

/* ------------------------------------------------------------------------ */

int
wm_detect(void)
{
    Display * dpy = GDK_DISPLAY ();
    Atom            type;
    int             format;
    unsigned long   nitems, bytesafter;
    unsigned char  *args = NULL;
    Window root = DefaultRootWindow(dpy);

    /* build atoms */
    net_wm       = XInternAtom(dpy, "_NET_SUPPORTED", False);
    net_wm_state = XInternAtom(dpy, "_NET_WM_STATE", False);
    net_wm_top   = XInternAtom(dpy, "_NET_WM_STATE_STAYS_ON_TOP", False);
    gnome        = XInternAtom(dpy, "_WIN_SUPPORTING_WM_CHECK", False);
    gnome_layer  = XInternAtom(dpy, "_WIN_LAYER", False);

    /* gnome-compilant */
    if (Success == XGetWindowProperty
	(dpy, root, gnome, 0, (65536 / sizeof(long)), False,
	 AnyPropertyType, &type, &format, &nitems, &bytesafter, &args)) {
        if (nitems > 0) {
	    printv("wmhooks: gnome\n");
	    /* FIXME: check capabilities */
	    window_on_top = gnome_window_on_top;
	    XFree(args);
	    return 0;
	}
    }

    /* netwm compliant */
    if (Success == XGetWindowProperty
        (dpy, root, net_wm, 0, (65536 / sizeof(long)), False,
         AnyPropertyType, &type, &format, &nitems, &bytesafter, &args) &&
	nitems > 0) {
        printv("wmhooks: netwm\n");
	window_on_top = net_wm_window_on_top;
        XFree(args);
        return 0;
    }

    /* nothing found... */

    printv("wmhooks: nothing\n");
    window_on_top = dummy_window_on_top;

    return -1;
}

#else /* !WMHOOKS */

void (* window_on_top)(GtkWidget *widget, gboolean on) =
  dummy_window_on_top;

int
wm_detect			(void)
{
  return -1;
}

#endif
