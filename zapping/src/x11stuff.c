/*
 * Zapping (TV viewer for the Gnome Desktop)
 *
 * Copyright (C) 2000-2001 Iñaki García Etxebarria
 * Copyright (C) 2002-2003 Michael H. Schimek
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
 *
 * Some of this code is based on the xscreensaver source code:
 * Copyright (c) 1991-1998 by Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/*
 * the routines contained here are all the X-specific stuff in Zapping
 */

#include "../site_def.h"

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#define ZCONF_DOMAIN "/zapping/options/main/"
#include "zconf.h"
#include "x11stuff.h"
#include "zmisc.h"
#include "globals.h"

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>


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
  static gint depth = -1;

  if (depth > 0)
    return depth;

  tmp_image =
    gdk_image_new(GDK_IMAGE_FASTEST, gdk_visual_get_system(), 16, 16);

  if (!tmp_image)
    return -1;

  depth = tmp_image->depth;

  if (depth == 24 &&
      tmp_image->bpp == 4)
    depth = 32;

  g_object_unref (G_OBJECT (tmp_image));

  return depth;
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

/*
 *  Window property & event helpers
 */

static XErrorHandler	old_error_handler	= 0;
static Bool		bad_window		= False;

static int
bad_window_handler		(Display *		display,
				 XErrorEvent *		error)
{
  if (error->error_code == BadWindow)
    bad_window = True;
  else if (old_error_handler)
    return old_error_handler (display, error);

  return 0;
}

static inline int
get_window_property		(Display *		display,
				 Window			window,
				 Atom			property,
				 Atom			req_type,
				 unsigned long *	nitems_return,
				 unsigned char **	prop_return)	 
{
  int status;
  Atom type;
  int format;
  unsigned long bytes_after;

  bad_window = False;
  
  status = XGetWindowProperty (display, window, property,
                    	       0, (65536 / sizeof (long)),
			       False, req_type,
			       &type, &format,
			       nitems_return, &bytes_after,
			       prop_return);
  if (bad_window)
    {
      status = BadWindow;
    }
  else if (status == Success)
    {
      if (type == None)
        status = BadAtom;
    }

  return status;
}

static void
send_event			(Display *		display,
				 Window			window,
				 Atom			message_type,
				 long			l0,
				 long			l1)
{
  XEvent xev;
  int status;

  CLEAR (xev);

  xev.type = ClientMessage;
  xev.xclient.display = display;
  xev.xclient.window = window;
  xev.xclient.send_event = TRUE;
  xev.xclient.message_type = message_type;
  xev.xclient.format = 32;
  xev.xclient.data.l[0] = l0;
  xev.xclient.data.l[1] = l1;

  status = XSendEvent (display, DefaultRootWindow (display),
		       False,
		       SubstructureNotifyMask |
		       SubstructureRedirectMask,
		       &xev);
  if (0)
    fprintf (stderr, "%d = XSendEvent(message_type=%u l0=%ld l1=%ld)\n",
	     status, (unsigned int) message_type, l0, l1);

  XSync (display, False);
}

static void
gtk_window_send_x11_event	(GtkWindow *		window,
				 Atom			message_type,
				 long			l0,
				 long			l1)
{
  GdkWindow *toplevel;

  if (window->frame)
    toplevel = window->frame;
  else
    toplevel = GTK_WIDGET (window)->window;

  g_assert (toplevel != NULL);
  g_assert (GTK_WIDGET_MAPPED (window));

  send_event (GDK_WINDOW_XDISPLAY (toplevel),
	      GDK_WINDOW_XWINDOW (toplevel),
	      message_type,
	      l0, l1);
}



/*
 *  WindowManager hints for stay-on-top option
 */

#ifndef X11STUFF_WM_HINTS_DEBUG
#define X11STUFF_WM_HINTS_DEBUG 0
#endif

static void
dummy_window_on_top		(GtkWindow *		window,
				 gboolean		on)
{ }

/**
 * window_on_top:
 * @window:
 * @on:
 *
 * Tell the WM to keep the window on top of other windows.
 * You must call wm_hints_detect () and gtk_widget_show (window) first.
 */
void (* window_on_top)		(GtkWindow *		window,
				 gboolean		on)
  = dummy_window_on_top;

static GdkFilterReturn
wm_event_handler		(GdkXEvent *		xevent,
				 GdkEvent *		event,
				 gpointer		data)
{
  return GDK_FILTER_REMOVE; /* ignore */
}

enum {
  _NET_WM_STATE_REMOVE,
  _NET_WM_STATE_ADD,
  _NET_WM_STATE_TOGGLE
};

static Atom _XA_NET_SUPPORTED;
static Atom _XA_NET_WM_STATE;
static Atom _XA_NET_WM_STATE_ABOVE;
static Atom _XA_NET_WM_STATE_FULLSCREEN;

/* Tested: Sawfish 1.2 */
static void
net_wm_window_on_top		(GtkWindow *		window,
				 gboolean		on)
{
  gtk_window_send_x11_event (window,
			     _XA_NET_WM_STATE,
			     on ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE,
			     _XA_NET_WM_STATE_ABOVE);
}

enum {
  WIN_LAYER_BELOW = 2,
  WIN_LAYER_NORMAL = 4,
  WIN_LAYER_ONTOP = 6,
  WIN_LAYER_DOCK = 8,
  WIN_LAYER_ABOVE_DOCK = 10
};

static Atom _XA_WIN_SUPPORTING_WM_CHECK;
static Atom _XA_WIN_LAYER;

/* Tested: IceWM 1.2.2 */
static void
gnome_window_on_top		(GtkWindow *		window,
				 gboolean		on)
{
  gtk_window_send_x11_event (window,
			     _XA_WIN_LAYER,
			     on ? WIN_LAYER_ONTOP : WIN_LAYER_NORMAL,
			     0);
}

/**
 * wm_hints_detect:
 * Check if we can tell the WM to keep a window on top of other windows.
 * This must be called before window_on_top().
 *
 * @returns:
 * TRUE if possible.
 */
gboolean
wm_hints_detect			(void)
{
  Display *display;
  Window root;
  unsigned char *args;
  Atom *atoms;
  unsigned long nitems;
  int i;

  display = gdk_x11_get_default_xdisplay ();
  g_assert (display != 0);

  root = DefaultRootWindow (display);

  _XA_NET_SUPPORTED		= XInternAtom (display, "_NET_SUPPORTED", False);
  _XA_NET_WM_STATE		= XInternAtom (display, "_NET_WM_STATE", False);
  _XA_NET_WM_STATE_FULLSCREEN	= XInternAtom (display, "_NET_WM_STATE_FULLSCREEN", False);
  _XA_NET_WM_STATE_ABOVE	= XInternAtom (display, "_NET_WM_STATE_ABOVE", False);

  /* Netwm compliant */

  atoms = NULL;

  if (Success == get_window_property (display, root, _XA_NET_SUPPORTED,
                                      AnyPropertyType, &nitems,
				      (unsigned char **) &atoms))
    {
      printv ("WM supports _NET_SUPPORTED (%lu)\n", nitems);

      for (i = 0; i < nitems; i++)
	{
	  /* atoms[i] != _XA_... */
	  char *atom_name = XGetAtomName (display, atoms[i]);

	  if (X11STUFF_WM_HINTS_DEBUG)
	    printv ("  atom %s\n", atom_name);

	  /* E.g. IceWM 1.2.2 _NET yes, _ABOVE no, but _LAYER */
	  if (0 == strcmp (atom_name, "_NET_WM_STATE_ABOVE"))
	    {
	      XFree (atom_name);
	      XFree (atoms);

	      window_on_top = net_wm_window_on_top;

	      gdk_add_client_message_filter
		(gdk_x11_xatom_to_atom (_XA_NET_WM_STATE),
		 wm_event_handler, 0);

	      return TRUE;
	    }

	  XFree (atom_name);
	}

      XFree (atoms);

      printv ("  ... nothing useful\n");
    }

  _XA_WIN_SUPPORTING_WM_CHECK	= XInternAtom (display, "_WIN_SUPPORTING_WM_CHECK", False);
  _XA_WIN_LAYER			= XInternAtom (display, "_WIN_LAYER", False);

  /* Gnome compliant */

  args = NULL;

  if (Success == get_window_property (display, root,
                                      _XA_WIN_SUPPORTING_WM_CHECK,
				      AnyPropertyType, &nitems, &args))
    if (nitems > 0)
      {
        printv ("WM supports _WIN_SUPPORTING_WM_CHECK\n");

        /* FIXME: check capabilities */

        XFree (args);

        window_on_top = gnome_window_on_top;

	gdk_add_client_message_filter
	  (gdk_x11_xatom_to_atom (_XA_WIN_LAYER),
	   wm_event_handler, 0);

        return TRUE;
      }

  printv ("No WM hints\n");

  return FALSE;
}

/*
 *  XFree86VidMode helpers
 */

#ifndef X11STUFF_VIDMODE_DEBUG
#define X11STUFF_VIDMODE_DEBUG 0
#endif

/**
 * x11_vidmode_clear_state:
 * @vs:
 *
 * Clear a struct x11_vidmode_state such that
 * x11_vidmode_restore() will do nothing.
 */
void
x11_vidmode_clear_state		(x11_vidmode_state *	vs)
{
  if (!vs)
    return;

  vs->new.vm = NULL;		/* don't restore */
  vs->new.pt.x = 1 << 20;	/* implausible, don't restore */
  vs->new.vp.x = 1 << 20;
}

#ifdef HAVE_VIDMODE_EXTENSION

#include <X11/extensions/xf86vmode.h>

struct vidmode {
  x11_vidmode_info	pub;
  XF86VidModeModeInfo	info;
};

#define VIDMODE(p) PARENT (p, struct vidmode, pub)

/**
 * x11_vidmode_list_delete:
 * @list:
 *
 * Deletes a list of struct x11_vidmode_info as returned
 * by x11_vidmode_list_new().
 **/
void
x11_vidmode_list_delete		(x11_vidmode_info *	list)
{
  struct vidmode *v;

  while (list)
    {
      v = VIDMODE (list);
      list = v->pub.next;
      free (v);
    }
}

/**
 * x11_vidmode_list_new:
 * Creates a sorted list of selectable VidModes, i.e. all valid
 * Modelines in XF86Config including those switchable with kp+
 * and kp-. The list must be freed with x11_vidmode_list_delete().
 * Result can be NULL for various reasons.
 */
x11_vidmode_info *
x11_vidmode_list_new		(void)
{
  Display *display;
  int event_base, error_base;
  int major_version, minor_version;
  int screen;
  XF86VidModeModeInfo **mode_info;
  int mode_count;
  x11_vidmode_info *list;
  int i;

  display = GDK_DISPLAY ();
  screen = DefaultScreen (display);

  if (!XF86VidModeQueryExtension (display, &event_base, &error_base))
    {
      printv ("XF86VidMode extension not available.\n");
      return NULL;
    }

  if (!XF86VidModeQueryVersion (display, &major_version, &minor_version))
    {
      printv ("XF86VidMode extension not usable.\n");
      return NULL;
    }

  printv ("XF86VidMode base %d, %d, version %d.%d.\n",
	  event_base, error_base,
	  major_version, minor_version);

  if (major_version != 2)
    {
      printv ("Unknown XF86VidMode version\n");
      return NULL;
    }

  if (!XF86VidModeGetAllModeLines (display, screen, &mode_count, &mode_info))
    {
      if (X11STUFF_VIDMODE_DEBUG)
	printv ("No mode lines\n");
      return NULL;
    }

  list = NULL;

  for (i = 0; i < mode_count; i++)
    {
      XF86VidModeModeInfo *m = mode_info[i];
      struct vidmode *v;

      if (m->privsize > 0)
	{
	  XFree (m->private);
	  m->private = NULL;
	  m->privsize = 0;
	}

      if (Success == XF86VidModeValidateModeLine (display, screen, m))
	{
	  if (X11STUFF_VIDMODE_DEBUG)
	    printv ("Valid mode dot=%u flags=%08x "
		    "hd=%u hss=%u hse=%u ht=%u "
		    "vd=%u vss=%u vse=%u vt=%u\n",
		    m->dotclock, m->flags,
		    m->hdisplay, m->hsyncstart, m->hsyncend, m->htotal,
		    m->vdisplay, m->vsyncstart, m->vsyncend, m->vtotal);

	  if ((v = calloc (1, sizeof (*v))))
	    {
	      unsigned int dothz = m->dotclock * 1000;
	      x11_vidmode_info *vv, **vvv;

	      v->pub.width = m->hdisplay;
	      v->pub.height = m->vdisplay;
	      v->pub.hfreq = dothz / (double) m->htotal;
	      v->pub.vfreq = dothz / (double)(m->htotal * m->vtotal);
	      v->pub.aspect = 1.0; /* Sigh. */

	      v->info = *m;

	      for (vvv = &list; (vv = *vvv); vvv = &vv->next)
		if (v->pub.width == vv->width)
		  {
		    if (v->pub.height == vv->height)
		      {
		        if (v->pub.vfreq > vv->vfreq)
		          break;
		      }
		    else if (v->pub.height > vv->height)
		      break;
		  }
		else if (v->pub.width > vv->width)
		  break;

	      if (v)
	        {
	          v->pub.next = vv;
	          *vvv = &v->pub;
		}
	    }
	}
      else
	{
	  if (X11STUFF_VIDMODE_DEBUG)
	    printv ("Ignore bad mode dot=%u hd=%u vd=%u\n",
		    m->dotclock, m->hdisplay, m->vdisplay);
	}
    }

  XFree (mode_info);

  return list;
}

/**
 * x11_vidmode_by_name:
 * @list: List of VidModes as returned by x11_vidmode_list_new().
 * @name: VidMode name "width [sep height [sep vfreq [sep]]]".
 *   All whitespace is ignored, sep can be any sequence of non-digits,
 *   characters following the last sep are ignored. Numbers strtoul.
 *
 * Returns a pointer to the first VidMode matching the name, NULL
 * when the mode is not in the list. (This is intended to select
 * a mode from config file.)
 */
x11_vidmode_info *
x11_vidmode_by_name		(x11_vidmode_info *	list,
				 const gchar *		name)
{
  unsigned int val[3] = { 0, 0, 0 };
  x11_vidmode_info *info;
  double fmin;
  unsigned int i;
  
  if (!name)
    return NULL;

  for (i = 0; i < 3; i++)
    {
      while (isspace (*name))
        name++;

      if (0 == *name || !isdigit (*name))
        break;

      val[i] = strtoul (name, (char **) &name, 0);

      while (isspace (*name))
        name++;
      while (*name && !isdigit (*name))
        name++;
    }

  if (val[0] == 0)
    return NULL;

  info = NULL;
  fmin = 1e20;

  for (; list; list = list->next)
    if (   (list->width == val[0])
        && (val[1] == 0 || list->height == val[1]))
      {
        double d;
      
        if (val[2] == 0)
	  {
	    info = list;
	    break;
	  }
	
	d = fabs (list->vfreq - val[2]);

	if (d < fmin)
	  {
	    info = list;
	    fmin = d;
	  }
      }

  return info;
}

/**
 * x11_vidmode_current:
 * @list: List of VidModes as returned by x11_vidmode_list_new().
 *
 * Returns a pointer to the current VidMode, NULL when the mode
 * is not in the list or some other problem occurred.
 */
x11_vidmode_info *
x11_vidmode_current		(x11_vidmode_info *	list)
{
  Display *display;
  int screen;
  XF86VidModeModeLine mode_line;
  int dot_clock;
  struct vidmode *v;

  if (!list)
    return NULL;

  display = GDK_DISPLAY ();
  screen = DefaultScreen (display);

  if (!XF86VidModeGetModeLine (display, screen, &dot_clock, &mode_line))
    {
      if (X11STUFF_VIDMODE_DEBUG)
	printv ("XF86VidModeGetModeLine() failed\n");
      return NULL;
    }

  if (mode_line.privsize > 0)
    {
      XFree (mode_line.private);
      mode_line.private = NULL;
      mode_line.privsize = 0;
    }

  for (v = VIDMODE (list); v; v = VIDMODE (v->pub.next))
    if (   v->info.dotclock	== dot_clock
	&& v->info.hdisplay	== mode_line.hdisplay
	&& v->info.hsyncstart	== mode_line.hsyncstart
	&& v->info.hsyncend	== mode_line.hsyncend
	&& v->info.htotal	== mode_line.htotal
	&& v->info.vdisplay	== mode_line.vdisplay
	&& v->info.vsyncstart	== mode_line.vsyncstart
	&& v->info.vsyncend	== mode_line.vsyncend
	&& v->info.vtotal	== mode_line.vtotal)
      break;

  if (!v)
    {
      if (X11STUFF_VIDMODE_DEBUG)
	printv ("Current VidMode dot=%u hd=%u vd=%u not in list\n",
		dot_clock, mode_line.hdisplay, mode_line.vdisplay);
      return NULL;
    }

  return &v->pub;
}

/**
 * x11_vidmode_current:
 * @list: List of VidModes as returned by x11_vidmode_list_new().
 * @vm: VidMode to switch to, must be member of @list.
 * @vs: If given save settings for x11_vidmode_restore() here.
 *
 * Switches to VidMode @vm, can be NULL to keep the current mode.
 * Then centers the viewport over the root window, and if
 * necessary moves the pointer into sight.
 */
gboolean
x11_vidmode_switch		(x11_vidmode_info *	list,
				 x11_vidmode_info *	vm,
				 x11_vidmode_state *	vs)
{
  Display *display;
  x11_vidmode_state state;
  int screen;
  Window root, dummy1;
  int x, y, dummy2;
  unsigned int w, h;
  unsigned int dummy3;
  int warp;

  if (!vs)
    vs = &state;

  x11_vidmode_clear_state (vs);

  display = GDK_DISPLAY ();
  screen = DefaultScreen (display);
  root = DefaultRootWindow (display);

  XGetGeometry (display, root, &dummy1, &x, &y, &w, &h, &dummy3, &dummy3);
  XF86VidModeGetViewPort (display, screen, &vs->old.vp.x, &vs->old.vp.y);
  XQueryPointer (display, root, &dummy1, &dummy1,
		 &vs->old.pt.x, &vs->old.pt.y, &dummy2, &dummy2, &dummy3);

  /* Switch to requested mode, if any */

  vs->old.vm = x11_vidmode_current (list);

  if (vm)
    {
      if (vs->old.vm != vm)
	{
	  if (XF86VidModeSwitchToMode (display, screen, &VIDMODE (vm)->info))
	    {
	      vs->new.vm = vm;
	    }
	  else
	    {
	      if (X11STUFF_VIDMODE_DEBUG)
		printv ("XF86VidModeSwitchToMode() failed\n");
	      return FALSE;
	    }
	}
    }
  else if (vs->old.vm)
    {
      vm = vs->old.vm;
    }
  else
    {
      return TRUE;
    }

  /* Center ViewPort */

  x = (w - vm->width) >> 1;
  y = (h - vm->height) >> 1;

  assert (x >= 0 && y >= 0);

  if (XF86VidModeSetViewPort (display, screen, x, y))
    {
      vs->new.vp.x = x;
      vs->new.vp.y = y;
    }
  else
    {
      if (X11STUFF_VIDMODE_DEBUG)
	printv ("XF86VidModeSetViewPort() failed\n");

      x = vs->old.vp.x;
      y = vs->old.vp.y;
    }

  /* Make pointer visible */

  warp = 0;

  if (vs->old.pt.x < x)
    warp = 1;
  else if (vs->old.pt.x > x + vm->width - 16)
    {
      x += vm->width - 16;
      warp = 1;
    }
  else
    {
      x = vs->old.pt.x;
    }

  if (vs->old.pt.y < y)
    warp = 1;
  else if (vs->old.pt.y > y + vm->height - 16)
    {
      y += vm->height - 16;
      warp = 1;
    }
  else
    {
      y = vs->old.pt.y;
    }

  if (warp)
    {
      XWarpPointer (display, None, root, 0, 0, 0, 0, x, y);

      vs->new.pt.x = x;
      vs->new.pt.y = y;
    }

  XSync (display, False);

  return TRUE;
}

/**
 * x11_vidmode_current:
 * @list: List of VidModes as returned by x11_vidmode_list_new(),
 *        must be the same as given to the corresponding
 *        x11_vidmode_switch().
 * @vs: Settings saved with x11_vidmode_switch(), can be NULL.
 *
 * Restores from a x11_vidmode_switch(), except a third party
 * changed the settings since x11_vidmode_switch(). Calls
 * x11_vidmode_clear_state() to prevent restore twice.
 */
void
x11_vidmode_restore		(x11_vidmode_info *	list,
				 x11_vidmode_state *	vs)
{
  Display *display;
  int screen;
  Window root, dummy1;
  int vpx, vpy, ptx, pty, dummy2;
  unsigned int dummy3;

  if (!vs)
    return;

  display = GDK_DISPLAY ();
  screen = DefaultScreen (display);
  root = DefaultRootWindow (display);

  XF86VidModeGetViewPort (display, screen, &vpx, &vpy);
  XQueryPointer (display, root, &dummy1, &dummy1,
		 &ptx, &pty, &dummy2, &dummy2, &dummy3);

  if (vs->old.vm && vs->new.vm)
    {
      x11_vidmode_info *new_vm;

      new_vm = x11_vidmode_current (list);

      if (vs->new.vm != new_vm)
	goto done; /* user changed vidmode, keep that */

      if (!XF86VidModeSwitchToMode (display, screen,
				    &VIDMODE (vs->old.vm)->info))
	{
	  if (X11STUFF_VIDMODE_DEBUG)
	    printv ("Cannot restore old mode, "
		    "XF86VidModeSwitchToMode() failed\n");
	  goto done;
	}
    }

  if (vs->new.vp.x == vpx && vs->new.vp.y == vpy)
    XF86VidModeSetViewPort (display, screen, vs->old.vp.x, vs->old.vp.y);
  else
    goto done; /* user moved viewport, keep that */

  if (abs (vs->new.pt.x - ptx) < 10 && abs (vs->new.pt.y - pty) < 10)
    XWarpPointer (display, None, root, 0, 0, 0, 0,
		  vs->old.pt.x, vs->old.pt.y);
  /* else user moved pointer, keep that */

 done:
  XSync (display, False);

  x11_vidmode_clear_state (vs);
}

#else /* !HAVE_VIDMODE_EXTENSION */

void
x11_vidmode_list_delete		(x11_vidmode_info *	list)
{
}

x11_vidmode_info *
x11_vidmode_list_new		(void)
{
  printv ("VidMode extension support not compiled in\n");

  return NULL;
}

x11_vidmode_info *
x11_vidmode_by_name		(x11_vidmode_info *	list,
				 const gchar *		name)
{
  return NULL;
}

x11_vidmode_info *
x11_vidmode_current		(x11_vidmode_info *	list)
{
  return NULL;
}

gboolean
x11_vidmode_switch		(x11_vidmode_info *	list,
				 x11_vidmode_info *	vm,
				 x11_vidmode_state *	vs)
{
  return FALSE;
}

void
x11_vidmode_restore		(x11_vidmode_info *	list,
				 x11_vidmode_state *	vs)
{
}

#endif /* !HAVE_VIDMODE_EXTENSION */

/*
 *  Screensaver
 */

#ifndef X11STUFF_SCREENSAVER_DEBUG
#define X11STUFF_SCREENSAVER_DEBUG 0
#endif

#ifdef HAVE_DPMS_EXTENSION
/* xc/doc/hardcopy/Xext/DPMSLib.PS.gz */
#include <X11/Xproto.h> /* CARD16 */
#include <X11/extensions/dpms.h>
#endif

/*
   Tested:				on		check-test (TTX mode)
   xset s 7 (blank/noblank)		blocked		blanks
   sleep 1; xset s active		deactivates	no action
   xset dpms 7				blocked		blanks
   sleep 1; xset dpms force off		forces on+	no action
   xscreensaver 4.06:
     detect start & kill		ok
     transition from / to xset s 7	blocked
     blank after 1m			blocked		blanks
     dpms blank 1m			blocked+	blanks
     ++
   xautolock 2.1 (w/MIT saver)+++	blocked		blanks

  + after work-around
  ++ Various methods to detect inactivity (XIdle, monitoring kernel
     device etc) UNTESTED or NOT BLOCKED.
  +++ XIdle UNTESTED. Without MIT or XIdle xautolock scans all
      windows for KeyPress events, that is NOT BLOCKED.
*/

static Atom _XA_SCREENSAVER_VERSION;
static Atom _XA_SCREENSAVER;
static Atom _XA_DEACTIVATE;

static gboolean		screensaver_enabled;
static unsigned int	screensaver_level;
static gboolean		dpms_usable;
static gint		gtimeout;

static gboolean
find_xscreensaver_window	(Display *		display,
				 Window *		window_return)
{
  Window root = RootWindowOfScreen (DefaultScreenOfDisplay (display));
  Window root2, parent, *kids;
  unsigned int nkids;
  unsigned int i;

  if (!XQueryTree (display, root, &root2, &parent, &kids, &nkids)
      || root != root2 || parent
      || !kids || nkids == 0)
    return FALSE;

  /* We're walking the list of root-level windows and trying to find
     the one that has a particular property on it.  We need to trap
     BadWindows errors while doing this, because it's possible that
     some random window might get deleted in the meantime.  (That
     window won't have been the one we're looking for.)
   */
  old_error_handler = XSetErrorHandler (bad_window_handler);

  for (i = 0; i < nkids; i++)
    {
      unsigned long nitems;
      char *v;

      XSync (display, False);

      if (Success == get_window_property (display, kids[i],
                                          _XA_SCREENSAVER_VERSION,
                                          XA_STRING, &nitems,
				          (unsigned char **) &v))
        {
	  XSetErrorHandler (old_error_handler);
          *window_return = kids[i];
          XFree (kids);
	  return TRUE;
	}
    }

  XSetErrorHandler (old_error_handler);
  XFree (kids);

  return FALSE;
}

/* Here we reset the idle counter at regular intervals,
   an inconvenience outweight by a number of advantages:
   No need to remember and restore saver states. It works
   nicely with other apps because no synchronization is
   neccessary. When we unexpectedly bite the dust, the
   screensaver kicks in as usual, no cleanup necessary. */
static gboolean
screensaver_timeout		(gpointer		unused)
{
  Display *display = GDK_DISPLAY ();
  Window window;
  gboolean xss;

  /* xscreensaver is a client, it may come and go at will. */
  xss = find_xscreensaver_window (display, &window);

  if (X11STUFF_SCREENSAVER_DEBUG)
    printv ("screensaver_timeout() xss=%d dpms=%d\n", xss, dpms_usable);

  if (xss)
    {
      /* xscreensaver overrides the X savers, so it takes
	 priority here. This call unblanks the display and/or
	 re-starts the idle counter. Error ignored, response ignored. */
      send_event (display, window, _XA_SCREENSAVER, (long) _XA_DEACTIVATE, 0);
    }
  /* xscreensaver 4.06 doesn't appear to call XForceScreenSaver() on
     _XA_DEACTIVATE when DPMS was enabled, only when MIT or SGI
     screensaver extensions are used, so we're forced to do it anyway.*/ 
  /* else */
    {
      /* Restart the idle counter, this impacts the internal
	 saver, MIT-SCREEN-SAVER extension and DPMS extension
	 idle counter (in XFree86 4.2). Curiously it forces
	 the monitor back on after blanking by the internal
	 saver, but not after DPMS blanking. */
      XForceScreenSaver (display, ScreenSaverReset);

#ifdef HAVE_DPMS_EXTENSION

      if (dpms_usable)
	{
	  CARD16 power_level;
	  BOOL state;

	  if (DPMSInfo (display, &power_level, &state))
	    {
	      if (X11STUFF_SCREENSAVER_DEBUG)
		printv ("DPMSInfo(level=%d enabled=%d)\n",
			(int) power_level, (int) state);

	      if (power_level != DPMSModeOn)
		/* Error ignored */
		DPMSForceLevel (display, DPMSModeOn);
	    }
	}

#endif

    }

  return TRUE; /* call again */
}

/**
 * x11_screensaver_set:
 * @level:
 *
 * Switch the screensaver on or off. @level can be one of
 * X11_SCREENSAVER_ON or X11_SCREENSAVER_DISPLAY_ACTIVE.
 * When the function is disabled it does nothing.
 */
void
x11_screensaver_set		(unsigned int		level)
{
  Display *display = GDK_DISPLAY ();

  if (X11STUFF_SCREENSAVER_DEBUG)
    printv ("x11_screensaver_set (level=%d) enabled=%d dpms=%d\n",
	    (int) level, screensaver_enabled, dpms_usable);

  if (screensaver_enabled)
    {
      if (level == X11_SCREENSAVER_ON)
	{
	  if (gtimeout != -1)
	    {
	      gtk_timeout_remove (gtimeout);
	      gtimeout = -1;
	    }
	}
      else
	{
	  if (level & X11_SCREENSAVER_CPU_ACTIVE)
	    {
	      /* to do - this should block APM */
	    }
	  
	  if (level & X11_SCREENSAVER_DISPLAY_ACTIVE)
	    {
	      if (gtimeout == -1)
		{
		  /* Make sure the display is on now. */
		  screensaver_timeout (NULL);	      
		  XSync (display, False);
		  
		  gtimeout = gtk_timeout_add (5432 /* ms */, screensaver_timeout, NULL);
		}
	    }
	}
    }

  screensaver_level = level;
}

/**
 * x11_screensaver_control:
 * @enable:
 *
 * Enable or disable the x11_screensaver_set() function.
 */
void
x11_screensaver_control		(gboolean		enable)
{
  if (enable)
    {
      screensaver_enabled = TRUE;
      x11_screensaver_set (screensaver_level);
    }
  else if (screensaver_level != X11_SCREENSAVER_ON)
    {
      unsigned int level = screensaver_level;

      x11_screensaver_set (X11_SCREENSAVER_ON);

      screensaver_enabled = FALSE;
      screensaver_level = level;
    }
}

void
x11_screensaver_init		(void)
{
  Display *display = GDK_DISPLAY ();
  int event_base, error_base;
  int major_version, minor_version;

  _XA_SCREENSAVER_VERSION = XInternAtom (display, "_SCREENSAVER_VERSION", False);
  _XA_SCREENSAVER	  = XInternAtom (display, "SCREENSAVER", False);
  _XA_DEACTIVATE	  = XInternAtom (display, "DEACTIVATE", False);

  screensaver_enabled	  = FALSE;
  screensaver_level	  = X11_SCREENSAVER_ON;
  dpms_usable		  = FALSE;
  gtimeout		  = -1;

#ifdef HAVE_DPMS_EXTENSION

  if (!DPMSQueryExtension (display, &event_base, &error_base))
    {
      printv ("DPMS extension not available\n");
      return;
    }

  if (!DPMSGetVersion (display, &major_version, &minor_version))
    {
      printv ("DPMS extension not usable\n");
      return;
    }

  printv ("DPMS base %d, %d, version %d.%d\n",
	  event_base, error_base,
	  major_version, minor_version);

  if (major_version != 1)
    {
      printv ("Unknown DPMS version\n");
      return;
    }

  if (!DPMSCapable (display))
    {
      printv ("Display does not support DPMS\n");
      return;
    }

  dpms_usable = TRUE;

#endif /* HAVE_DPMS_EXTENSION */

}

#ifdef HAVE_DGA_EXTENSION

/* man XF86DGA */
#include <X11/extensions/xf86dga.h>

gboolean
x11_dga_query			(x11_dga_parameters *	par,
				 int			bpp_hint)
{
  x11_dga_parameters param;

  Display *display = GDK_DISPLAY ();
  int event_base, error_base;
  int major_version, minor_version;
  int screen;
  int flags;

  if (par)
    CLEAR (*par);
  else
    par = &param;

  if (!XF86DGAQueryExtension (display, &event_base, &error_base))
    {
      printv ("DGA extension not available\n");
      return FALSE;
    }

  if (!XF86DGAQueryVersion (display, &major_version, &minor_version))
    {
      printv ("DGA extension not usable\n");
      return FALSE;
    }

  printv ("DGA base %d, %d, version %d.%d\n",
	  event_base, error_base,
	  major_version, minor_version);

  if (major_version != 1
      && major_version != 2)
    {
      printv ("Unknown DGA version\n");
      return FALSE;
    }

  screen = XDefaultScreen (display);

  if (!XF86DGAQueryDirectVideo (display, screen, &flags))
    {
      printv ("DGA DirectVideo not available\n");
      return FALSE;
    }

  if (!(flags & XF86DGADirectPresent))
    {
      printv ("DGA DirectVideo not supported\n");
      return FALSE;
    }

  {
    int start;		/* physical address (?) */
    int width;		/* of root window in pixels */
    int banksize;	/* in bytes, usually video memory size */
    int memsize;	/* ? */

    if (!XF86DGAGetVideoLL (display, screen,
			    &start, &width, &banksize, &memsize))
      {
	printv ("XF86DGAGetVideoLL() failed\n");
	return FALSE;
      }

    if (X11STUFF_DGA_DEBUG)
      printv ("DGA start=%p width=%d banksize=%d "
	      "(0x%x) memsize=%d (0x%x)\n",
	      (void *) start, width, banksize, banksize,
	      memsize, memsize);

    par->base = (void *) start;
  }

  {
    Window root;
    XWindowAttributes wts;

    root = DefaultRootWindow (display);

    XGetWindowAttributes (display, root, &wts);

    if (X11STUFF_DGA_DEBUG)
      printv ("DGA root width=%u height=%u\n",
	      wts.width, wts.height);

    par->width  = wts.width;
    par->height = wts.height;
  }

  {
    XVisualInfo *info, templ;
    int i, nitems;

    templ.screen = screen;

    info = XGetVisualInfo (display, VisualScreenMask, &templ, &nitems);

    for (i = 0; i < nitems; i++)
      if (info[i].class == TrueColor && info[i].depth >= 15)
	{
	  if (X11STUFF_DGA_DEBUG)
	    printv ("DGA vi[%u] depth=%u\n", i, info[i].depth);

	  par->depth = info[i].depth;
	  break;
	}

    XFree (info);

    if (i >= nitems)
      {
	printv ("DGA: No appropriate X visual available\n");
	CLEAR (*par);
	return FALSE;
      }

    switch (bpp_hint)
      {
      case 16:
      case 24:
      case 32:
	par->bits_per_pixel = bpp_hint;
	break;

      default:
	{
	  XPixmapFormatValues *pf;
	  int i, count;

	  /* BPP heuristic */

	  par->bits_per_pixel = 0;

	  pf = XListPixmapFormats (display, &count);

	  for (i = 0; i < count; i++)
	    {
	      if (X11STUFF_DGA_DEBUG)
		printv ("DGA pf[%u]: depth=%u bpp=%u\n",
			i, pf[i].depth, pf[i].bits_per_pixel);

	      if (pf[i].depth == par->depth)
		{
		  par->bits_per_pixel = pf[i].bits_per_pixel;
		  break;
		}
	    }

	  XFree (pf);

	  if (i >= count)
	    {
	      printv ("DGA: Unknown frame buffer bits per pixel\n");
	      CLEAR (*par);
	      return FALSE;
	    }
	}
      }

    /* XImageByteOrder() ? */
  }

  par->bytes_per_line = par->width * par->bits_per_pixel;

  if (par->bytes_per_line & 7)
    {
      printv ("DGA: Unknown frame buffer bits per pixel\n");
      CLEAR (*par);
      return FALSE;
    }

  par->bytes_per_line >>= 3;

  par->size = par->height * par->bytes_per_line;

  return TRUE;
}

#else /* !HAVE_DGA_EXTENSION */

gboolean
x11_dga_query			(x11_dga_parameters *	par,
				 int			bpp_hint)
{
  printv ("DGA extension support not compiled in\n");

  if (par)
    CLEAR (*par);

  return FALSE;
}

#endif /* !HAVE_DGA_EXTENSION */
