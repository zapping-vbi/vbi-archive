/* Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2000-2001 Iñaki García Etxebarria
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
    if (children)
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
  XSetErrorHandler(olderror);
  /* leave error-ignore mode */

  if (children)
    XFree((char *) children);

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

  if (on) {
    if (timeout == -2) {
      g_warning("cannot activate screensaver before deactivating");
      return;
    }
    XSetScreenSaver(GDK_DISPLAY(), timeout, interval, prefer_blanking,
		    allow_exposures);
#ifdef USE_XDPMS
    if ( (DPMSQueryExtension(GDK_DISPLAY(), &dummy, &dummy) ) &&
	 (DPMSCapable(GDK_DISPLAY())) && (dpms_was_on) )
      DPMSEnable(GDK_DISPLAY());
#endif
  } else {
    XGetScreenSaver(GDK_DISPLAY(), &timeout, &interval,
		    &prefer_blanking, &allow_exposures);
    /* FIXME: this doesn't appear to work yet (it should, according to
       man, what am i missing?) */
    XSetScreenSaver(GDK_DISPLAY(), 0, interval, prefer_blanking,
		    allow_exposures);
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
 * XvImage handling.
 */

/*
  Comment out if you have problems with the Shm extension
  (you keep getting a Gdk-error request_code:14x, minor_code:19)
*/
#define USE_XV_SHM 1

struct _xvzImagePrivate {
#ifdef USE_XV
  gboolean		uses_shm;
  XvImage		*image;
#ifdef USE_XV_SHM
  XShmSegmentInfo	shminfo; /* shared mem info for the xvimage */
#endif
#endif  
};

#ifdef USE_XV /* Real stuff */

static unsigned int
xv_mode_id(char * fourcc)
{
  return ((((__u32)(fourcc[0])<<0)|
	   ((__u32)(fourcc[1])<<8)|
	   ((__u32)(fourcc[2])<<16)|
	   ((__u32)(fourcc[3])<<24)));
}

#define YV12 xv_mode_id("YV12") /* YVU420 (planar, 12 bits) */
#define UYVY xv_mode_id("UYVY") /* UYVY (packed, 16 bits) */
#define YUY2 xv_mode_id("YUY2") /* YUYV (packed, 16 bits) */

static XvPortID		xvport; /* Xv port we will use */
static gboolean		port_grabbed = FALSE; /* We own a port */

extern gint		disable_xv; /* TRUE if XV should be disabled */

/**
 * Create a new XV image with the given attributes, returns NULL on error.
 */
xvzImage * xvzImage_new(enum tveng_frame_pixformat pixformat,
			gint w, gint h)
{
  xvzImage *new_image = g_malloc0(sizeof(xvzImage));
  struct _xvzImagePrivate * pimage = new_image->private =
    g_malloc0(sizeof(struct _xvzImagePrivate));
  void * image_data = NULL;
  unsigned int xvmode = (pixformat == TVENG_PIX_YUYV) ? YUY2 : YV12;

  if (!port_grabbed)
    {
      g_warning("XVPort not grabbed!");
      g_free(new_image->private);
      g_free(new_image);
      return NULL;
    }

  pimage -> uses_shm = FALSE;

#ifdef USE_XV_SHM
  memset(&pimage->shminfo, 0, sizeof(XShmSegmentInfo));
  pimage->image = XvShmCreateImage(GDK_DISPLAY(), xvport, xvmode, NULL,
				   w, h, &pimage->shminfo);
  if (pimage->image)
    pimage->uses_shm = TRUE;
#endif

  if (!pimage->image)
    {
      image_data = malloc(16*w*h);
      if (!image_data)
	{
	  g_warning("XV image data allocation failed");
	  g_free(new_image->private);
	  g_free(new_image);
	  return NULL;
	}
      pimage->image =
	XvCreateImage(GDK_DISPLAY(), xvport, xvmode,
		      image_data, w, h);
    }

  if (!pimage->image)
    {
      g_free(new_image->private);
      g_free(new_image);
      return NULL;
    }

#ifdef USE_XV_SHM
  if (pimage->uses_shm)
    {
      pimage->shminfo.shmid =
	shmget(IPC_PRIVATE, pimage->image->data_size,
	       IPC_CREAT | 0777);
      pimage->shminfo.shmaddr =
	pimage->image->data = shmat(pimage->shminfo.shmid, 0, 0);
      shmctl(pimage->shminfo.shmid, IPC_RMID, 0); /* remove when we
							terminate */

      pimage->shminfo.readOnly = False;

      XShmAttach(GDK_DISPLAY(), &pimage->shminfo);
    }
#endif

  XSync(GDK_DISPLAY(), False);

  new_image->w = new_image->private->image->width;
  new_image->h = new_image->private->image->height;
  new_image->data = new_image->private->image->data;
  new_image->data_size = new_image->private->image->data_size;

  return new_image;
}

/**
 * Puts the image in the given drawable, scales to the drawable's size.
 */
void xvzImage_put(xvzImage *image, GdkWindow *window, GdkGC *gc)
{
  gint w, h;
  struct _xvzImagePrivate *pimage = image->private;

  g_assert(window != NULL);

  if (!port_grabbed)
    {
      g_warning("XVPort not grabbed!");
      return;
    }

  if (!pimage->image)
    {
      g_warning("Trying to put an empty XV image");
      return;
    }

  gdk_window_get_size(window, &w, &h);

#ifdef USE_XV_SHM
  if (pimage->uses_shm)
    XvShmPutImage(GDK_DISPLAY(), xvport,
		  GDK_WINDOW_XWINDOW(window),
		  GDK_GC_XGC(gc), pimage->image,
		  0, 0, image->w, image->h, /* source */
		  0, 0, w, h, /* dest */
		  True);
#endif

  if (!pimage->uses_shm)
    XvPutImage(GDK_DISPLAY(), xvport,
	       GDK_WINDOW_XWINDOW(window),
	       GDK_GC_XGC(gc), pimage->image,
	       0, 0, image->w, image->h, /* source */
	       0, 0, w, h /* dest */);
}

/**
 * Frees the data associated with the image
 */
void xvzImage_destroy(xvzImage *image)
{
  struct _xvzImagePrivate *pimage = image->private;

  g_assert(image != NULL);

#ifdef USE_XV_SHM
  if (pimage->uses_shm)
    XShmDetach(GDK_DISPLAY(), &pimage->shminfo);
#endif
  if (!pimage->uses_shm)
    free(pimage->image->data);

  XFree(pimage->image);

#ifdef USE_XV_SHM
  if (pimage->uses_shm)
    shmdt(pimage->shminfo.shmaddr);
#endif

  g_free(image->private);
  g_free(image);
}

gboolean xvz_grab_port(tveng_device_info *info)
{
  Display *dpy=GDK_DISPLAY();
  Window root_window = GDK_ROOT_WINDOW();
  unsigned int version, revision, major_opcode, event_base,
    error_base;
  int i, j=0, k=0;
  int nAdaptors;
  XvAdaptorInfo *pAdaptors, *pAdaptor;
  XvImageFormatValues *pImgFormats=NULL;
  int nImgFormats;

  if (disable_xv)
    return FALSE;
  if (port_grabbed)
    return TRUE;

  if (Success != XvQueryExtension(dpy, &version, &revision,
				  &major_opcode, &event_base,
				  &error_base))
    goto error1;

  if (version < 2 || (version == 2 && revision < 2))
    goto error1;

  if (Success != XvQueryAdaptors(dpy, root_window, &nAdaptors,
				 &pAdaptors))
    goto error1;
  if (nAdaptors <= 0)
    goto error1;

  /* Just for debugging, can be useful */
  for (i=0; i<nAdaptors; i++)
    {
      pAdaptor = pAdaptors + i;
      /* print some info about this adaptor */
      printv("%d) Adaptor info:\n"
	     "	- Base port id:		0x%x\n"
	     "	- Number of ports:	%d\n"
	     "	- Type:			%d\n"
	     "	- Name:			%s\n"
	     "	- Number of formats:	%d\n",
	     i, (int)pAdaptor->base_id, (int) pAdaptor->num_ports,
	     (int)pAdaptor->type, pAdaptor->name,
	     (int)pAdaptor->num_formats);

      if ((pAdaptor->type & XvInputMask) &&
	  (pAdaptor->type & XvImageMask))
	{ /* Image adaptor, check if some port fits our needs */
	  for (j=0; j<pAdaptor->num_ports;j++)
	    {
	      xvport = pAdaptor->base_id + j;
	      pImgFormats = XvListImageFormats(dpy, xvport,
					       &nImgFormats);
	      if (!pImgFormats || !nImgFormats)
		continue;

	      for (k=0; k<nImgFormats; k++)
		printv("		[%d] %c%c%c%c (0x%x)\n", k,
		       (char)(pImgFormats[k].id>>0)&0xff,
		       (char)(pImgFormats[k].id>>8)&0xff,
		       (char)(pImgFormats[k].id>>16)&0xff,
		       (char)(pImgFormats[k].id>>24)&0xff,
		       pImgFormats[k].id);
	    }
	}
    }

  /* The real thing */
  for (i=0; i<nAdaptors; i++)
    {
      pAdaptor = pAdaptors + i;

      if ((pAdaptor->type & XvInputMask) &&
	  (pAdaptor->type & XvImageMask))
	{ /* Image adaptor, check if some port fits our needs */
	  for (j=0; j<pAdaptor->num_ports;j++)
	    {
	      xvport = pAdaptor->base_id + j;
	      pImgFormats = XvListImageFormats(dpy, xvport,
					       &nImgFormats);
	      if (!pImgFormats || !nImgFormats)
		continue;

	      if (Success != XvGrabPort(dpy, xvport, CurrentTime))
		continue;

	      for (k=0; k<nImgFormats; k++)
		if (pImgFormats[k].id == YUY2 ||
		    pImgFormats[k].id == YV12)
		  goto adaptor_found;

	      XvUngrabPort(dpy, xvport, CurrentTime);
	    }
	}
    }

  if (i == nAdaptors)
    goto error2;

  /* success */
 adaptor_found:
  printv("Adaptor #%d, image format #%d (0x%x), port #%d chosen\n",
	 i, k, pImgFormats[k].id, j);
  XvFreeAdaptorInfo(pAdaptors);

  tveng_set_xv_port(xvport, info);
  port_grabbed = TRUE;
  return TRUE;

 error2:
  XvFreeAdaptorInfo(pAdaptors);

 error1:
  return FALSE;
}

void xvz_ungrab_port(tveng_device_info *info)
{
  if (!port_grabbed)
    return;

  XvUngrabPort(GDK_DISPLAY(), xvport, CurrentTime);
  port_grabbed = FALSE;
  tveng_unset_xv_port(info);
}

#else /* !USE_XV, useless stubs */

xvzImage * xvzImage_new(gint width, gint height)
{
  return NULL;
}

void xvzImage_put(xvzImage *image, GdkWindow *window)
{
}

void xvzImage_destroy(xvzImage *image)
{
}

gboolean xvz_grab_port(tveng_device_info *info)
{
  return FALSE;
}

void xvz_ungrab_port(tveng_device_info *info)
{
}
#endif
