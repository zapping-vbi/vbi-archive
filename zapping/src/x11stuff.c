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
 * the routines contained here are all the X-specific stuff in Zapping
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#include <tveng.h>
#include "x11stuff.h"

/*
 * Returns a pointer to the data contained in the given GdkImage
 */
gpointer
x11_get_data(GdkImage * image)
{
  return (((GdkImagePrivate*)image) -> ximage -> data);
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
  GdkImage * tmp_image = gdk_image_new(GDK_IMAGE_FASTEST,
				       gdk_visual_get_system(),
				       16, 16);
  gint result;

  if (!tmp_image)
    return -1;

  result = tmp_image->bpp << 3;

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
  *clips = realloc(*clips, ((*num_clips)+1)*sizeof(struct tveng_clip));
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

/*
 * Returns a pointer to a clips struct (that you need to free()
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
  for (;;) {
    XQueryTree(dpy, me, &rroot, &parent, &children, &nchildren);
    XFree((char *) children);
    if (root == parent)
      break;
    me = parent;
  }
  XQueryTree(dpy, root, &rroot, &parent, &children, &nchildren);
    
  for (i = 0; i < nchildren; i++)
    if (children[i]==me)
      break;
  
  /* enter error-ignore mode */
  olderror = XSetErrorHandler(xerror);
  for (i++; i<nchildren; i++) {
    XGetWindowAttributes(dpy, children[i], &wts);
    if (!(wts.map_state & IsViewable))
      continue;
    
    x1=wts.x-wx;
    y1=wts.y-wy;
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
  XFree((char *) children);
  XSetErrorHandler(olderror);
  /* leave error-ignore mode */

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
