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

/**
 * XVideo backend.
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
#include "globals.h"

#ifdef USE_XV /* Real stuff */

static gboolean have_mitshm;

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

static unsigned int
xv_mode_id(char * fourcc)
{
  return ((((uint32_t)(fourcc[0])<<0)|
	   ((uint32_t)(fourcc[1])<<8)|
	   ((uint32_t)(fourcc[2])<<16)|
	   ((uint32_t)(fourcc[3])<<24)));
}

#define YV12 xv_mode_id("YV12") /* YVU420 (planar, 12 bits) */
#define UYVY xv_mode_id("UYVY") /* UYVY (packed, 16 bits) */
#define YUY2 xv_mode_id("YUY2") /* YUYV (packed, 16 bits) */

static XvPortID		xvport; /* Xv port we will use */

extern gint		disable_xv; /* TRUE if XV should be disabled */

/**
 * Create a new XV image with the given attributes, returns NULL on error.
 */
static xvzImage *
image_new(enum tveng_frame_pixformat pixformat, gint w, gint h)
{
  xvzImage *new_image = g_malloc0(sizeof(xvzImage));
  struct _xvzImagePrivate * pimage = new_image->priv =
    g_malloc0(sizeof(struct _xvzImagePrivate));
  void * image_data = NULL;
  unsigned int xvmode = (pixformat == TVENG_PIX_YUYV) ? YUY2 : YV12;
  double bpp = (pixformat == TVENG_PIX_YUYV) ? 2 : 1.5;

  pimage -> uses_shm = FALSE;

#ifdef USE_XV_SHM

  if (have_mitshm) /* just in case */
    {
      memset(&pimage->shminfo, 0, sizeof(XShmSegmentInfo));
      pimage->image = XvShmCreateImage(GDK_DISPLAY(), xvport,
	xvmode, NULL, w, h, &pimage->shminfo);

      if (pimage->image)
	{
	  pimage->uses_shm = TRUE;

	  pimage->shminfo.shmid =
	    shmget(IPC_PRIVATE, pimage->image->data_size,
		   IPC_CREAT | 0777);

	  if (pimage->shminfo.shmid == -1)
            {
	      goto shm_error;
	    }
	  else
	    {
	      pimage->shminfo.shmaddr =
		pimage->image->data = shmat(pimage->shminfo.shmid, 0, 0);

	      shmctl(pimage->shminfo.shmid, IPC_RMID, 0);
	      /* destroy when we terminate, now if shmat failed */

	      if (pimage->shminfo.shmaddr == (void *) -1)
	        goto shm_error;

	      pimage->shminfo.readOnly = False;

	      if (!XShmAttach(GDK_DISPLAY(), &pimage->shminfo))
	        {
		  g_assert(shmdt(pimage->shminfo.shmaddr) != -1);
 shm_error:
		  XFree(pimage->image);
		  pimage->image = NULL;
	          pimage->uses_shm = FALSE;
		}
	    }
	}
    }

#endif /* USE_XV_SHM */

  if (!pimage->image)
    {
      image_data = malloc(bpp*w*h);
      if (!image_data)
	{
	  g_warning("XV image data allocation failed");
	  goto error1;
	}
      pimage->image =
	XvCreateImage(GDK_DISPLAY(), xvport, xvmode,
		      image_data, w, h);
      if (!pimage->image)
        goto error2;
    }

  new_image->w = new_image->priv->image->width;
  new_image->h = new_image->priv->image->height;
  new_image->data = new_image->priv->image->data;
  new_image->data_size = new_image->priv->image->data_size;

  return new_image;

 error2:
  if (image_data)
    free(image_data);

 error1:
  g_free(new_image->priv);
  g_free(new_image);

  return NULL;
}

/**
 * Puts the image in the given drawable, scales to the drawable's size.
 */
static void
image_put(xvzImage *image, GdkWindow *window, GdkGC *gc)
{
  gint w, h;
  struct _xvzImagePrivate *pimage = image->priv;

  g_assert(window != NULL);

  if (!pimage->image)
    {
      g_warning("Trying to put an empty XV image");
      return;
    }

  gdk_window_get_geometry(window, NULL, NULL, &w, &h, NULL);

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
static void
image_destroy(xvzImage *image)
{
  struct _xvzImagePrivate *pimage = image->priv;

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
    g_assert(shmdt(pimage->shminfo.shmaddr) != -1);
#endif

  g_free(image->priv);
  g_free(image);
}

static gboolean
grab(tveng_device_info *info)
{
  Display *dpy=GDK_DISPLAY();
  Window root_window = GDK_ROOT_WINDOW();
  int i, j=0, k=0;
  int nAdaptors;
  XvAdaptorInfo *pAdaptors, *pAdaptor;
  XvImageFormatValues *pImgFormats=NULL;
  int nImgFormats;

  if (Success != XvQueryAdaptors(dpy, root_window, &nAdaptors,
				 &pAdaptors))
    return FALSE;

  if (nAdaptors <= 0)
    return FALSE;

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

  return TRUE;

 error2:
  XvFreeAdaptorInfo(pAdaptors);

  return FALSE;
}

static void
ungrab(tveng_device_info *info)
{
  XvUngrabPort(GDK_DISPLAY(), xvport, CurrentTime);
  tveng_unset_xv_port(info);
}

static video_backend xv =
{
  name:			"XVideo Backend Scaler",
  grab:			grab,
  ungrab:		ungrab,
  image_new:		image_new,
  image_destroy:	image_destroy,
  image_put:		image_put
};

/**
 * Check whether XV is present and could work.
 */
gboolean add_backend_xv	(video_backend *backend);
gboolean add_backend_xv	(video_backend *backend)
{
  Display *dpy=GDK_DISPLAY();
  Window root_window = GDK_ROOT_WINDOW();
  unsigned int version, revision, major_opcode, event_base,
    error_base;
  int i, j=0, k=0, suitable_adaptor_found = 0;
  int nAdaptors;
  XvAdaptorInfo *pAdaptors, *pAdaptor;
  XvImageFormatValues *pImgFormats=NULL;
  int nImgFormats;

  if (disable_xv)
    return FALSE;

  if (Success != XvQueryExtension(dpy, &version, &revision,
				  &major_opcode, &event_base,
				  &error_base))
    return FALSE;

  if (version < 2 || (version == 2 && revision < 2))
    return FALSE;

  if (Success != XvQueryAdaptors(dpy, root_window, &nAdaptors,
				 &pAdaptors))
    return FALSE;

  if (nAdaptors <= 0)
    return FALSE;

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
		{
		  printv("		[%d] %c%c%c%c (0x%x)\n", k,
			 (char)(pImgFormats[k].id>>0)&0xff,
			 (char)(pImgFormats[k].id>>8)&0xff,
			 (char)(pImgFormats[k].id>>16)&0xff,
			 (char)(pImgFormats[k].id>>24)&0xff,
			 pImgFormats[k].id);
		  if (pImgFormats[k].id == YUY2 ||
		      pImgFormats[k].id == YV12)
		    suitable_adaptor_found++;
		}
	    }
	}
    }

  if (!suitable_adaptor_found)
    {
      XvFreeAdaptorInfo(pAdaptors);
      return FALSE;
    }

  printv("XV: %d suitable adaptor%s found.\n",
	 suitable_adaptor_found, suitable_adaptor_found == 1 ? "" : "s");

  /* XV could work, register the backend functions */
  memcpy(backend, &xv, sizeof(xv));

  XvFreeAdaptorInfo(pAdaptors);

#ifdef USE_XV_SHM
  have_mitshm = !!XShmQueryExtension(dpy);
#endif

  return TRUE;
}

#else /* !USE_XV, do not add the backend */

gboolean add_backend_xv (video_backend *backend);
gboolean add_backend_xv	(video_backend *backend)
{
  return FALSE;
}

#endif
