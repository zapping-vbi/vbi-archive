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

#include <gdk/gdkx.h>
#include "zmisc.h"
#include "tveng.h"
#include "interface.h"

extern tveng_device_info * main_info;
extern GtkWidget * main_window;
GdkImage * zimage = NULL; /* The buffer that holds the capture */
gint oldx=-1, oldy=-1, oldw=-1, oldh=-1; /* Last geometry of the
					    Zapping window */

/*
  Prints a message box showing an error, with the location of the code
  that called the function. If run is TRUE, gnome_dialog_run will be
  called instead of just showing the dialog. Keep in mind that this
  will block the capture.
*/
GtkWidget * ShowBoxReal(const gchar * sourcefile,
			const gint line,
			const gchar * func,
			const gchar * message,
			const gchar * message_box_type,
			gboolean blocking, gboolean modal)
{
  GtkWidget * dialog;
  gchar * str;
  gchar buffer[256];

  buffer[255] = 0;

  g_snprintf(buffer, 255, " (%d)", line);

  str = g_strconcat(sourcefile, buffer, ": [", func, "]", NULL);

  if (!str)
    return NULL;

  dialog = gnome_message_box_new(message, message_box_type,
				 GNOME_STOCK_BUTTON_OK,
				 NULL);


  gtk_window_set_title(GTK_WINDOW (dialog), str);

  /*
    I know this isn't usual for a dialog box, but this way we can see
    all the title bar if we want to
  */
  gtk_window_set_policy(GTK_WINDOW (dialog), FALSE, TRUE, TRUE);

  g_free(str);

  if (blocking)
    {
      gnome_dialog_run_and_close(GNOME_DIALOG(dialog));
      return NULL;
    }

  gtk_window_set_modal(GTK_WINDOW (dialog), modal);
  gtk_widget_show(dialog);

  return dialog;
}

/*
  Resizes the image to a new size. If this is the same as the old
  size, nothing happens. Returns the newly allocated image on exit.
*/
GdkImage*
zimage_reallocate(int new_width, int new_height)
{
  GdkImage * new_zimage;
  gpointer new_data;
  gint old_size;
  gint new_size;

  if ((!zimage) || (zimage->width != new_width) || (zimage->height !=
						    new_height))
    {
      new_zimage = gdk_image_new(GDK_IMAGE_FASTEST,
				 gdk_visual_get_system(),
				 new_width,
				 new_height);
      if (!new_zimage)
	{
	  g_warning(_("Sorry, but a %dx%d image couldn't be "
		      "allocated, the image will not be changed."),
		    new_width, new_height);

	  return zimage;
	}
      /*
	A new image has been allocated, keep as much data as possible
      */
      if (zimage)
	{
	  old_size = zimage->height* zimage->bpl;
	  new_size = new_zimage->height * new_zimage->bpl;
	  new_data = ((GdkImagePrivate*)new_zimage) -> ximage-> data;
	  if (old_size > new_size)
	    memcpy(new_data, zimage_get_data(zimage), new_size);
	  else
	    memcpy(new_data, zimage_get_data(zimage), old_size);
	  
	  /* Destroy the old image, now it is useless */
	  gdk_image_destroy(zimage);
	}
      zimage = new_zimage;
    }

  return zimage;
}

/*
  Returns a pointer to the zimage
*/
GdkImage*
zimage_get(void)
{
  return zimage;
}

/*
  Returns a pointer to the image data
*/
gpointer
zimage_get_data( GdkImage * image)
{
  return (((GdkImagePrivate*)image) -> ximage-> data);
}

/*
  Destroys the image that holds the capture
*/
void
zimage_destroy(void)
{
  if (zimage)
    gdk_image_destroy( zimage );

  zimage = NULL;
}

/*
  Creates a GtkPixmapMenuEntry with the desired pixmap and the
  desired label.
*/
GtkWidget * z_gtk_pixmap_menu_item_new(const gchar * label,
				       const gchar * icon)
{
  GtkWidget * pixmap_menu_item;
  GtkWidget * accel_label;
  GtkWidget * pixmap;

  g_assert(label != NULL);

  pixmap_menu_item = gtk_pixmap_menu_item_new();
  accel_label = gtk_accel_label_new (label);
  gtk_misc_set_alignment(GTK_MISC(accel_label), 0.0, 0.5);
  gtk_container_add(GTK_CONTAINER (pixmap_menu_item), accel_label);
  gtk_accel_label_set_accel_widget (GTK_ACCEL_LABEL (accel_label),
				    pixmap_menu_item);
  gtk_widget_show (accel_label);

  if (icon)
    {
      /* if i don't specify the size, the pixmap is too big, but the
	 one libgnomeui creates isn't... why? */
      pixmap = gnome_stock_pixmap_widget_at_size (pixmap_menu_item,
						  icon, 16, 16);
      
      gtk_pixmap_menu_item_set_pixmap(GTK_PIXMAP_MENU_ITEM(pixmap_menu_item),
				      pixmap);
      gtk_widget_show(pixmap);
    }

  return (pixmap_menu_item);
}

/* adds a clip to the given struct, reallocating if needed */
static
void zmisc_add_clip(int x1, int y1, int x2, int y2,
		    struct tveng_clip ** clips, gint* num_clips);

static
void zmisc_add_clip(int x1, int y1, int x2, int y2,
		    struct tveng_clip ** clips, gint* num_clips)
{
  *clips = realloc(*clips, ((*num_clips)+1)*sizeof(struct tveng_clip));
  (*clips)[*num_clips].x = x1;
  (*clips)[*num_clips].y = y1;
  (*clips)[*num_clips].width = x2-x1;
  (*clips)[*num_clips].height = y2-y1;
  (*num_clips)++;
}

/* Gets all the clips */
void zmisc_get_clips(void);

void zmisc_get_clips(void)
{
  struct tveng_clip * clips = NULL;
  int x1,y1,x2,y2;
  Display *dpy = GDK_DISPLAY();
  XWindowAttributes wts;
  Window root, me, rroot, parent, *children;
  uint nchildren, i;
  gint num_clips=0;
  int wx, wy, wwidth, wheight, swidth, sheight;

  wx = main_info -> window.x;
  wy = main_info -> window.y;
  wwidth = wx + main_info -> window.width;
  wheight = wy + main_info -> window.height;
  swidth = gdk_screen_width();
  sheight = gdk_screen_height();
  if (wx<0)
    zmisc_add_clip(0, 0, (uint)(-wx), wheight, &clips, &num_clips);
  if (wy<0)
    zmisc_add_clip(0, 0, wwidth, (uint)(-wy), &clips, &num_clips);
  if ((wx+wwidth) > swidth)
    zmisc_add_clip(swidth-wx, 0, wwidth, wheight, &clips,
		   &num_clips);
  if ((wy+wheight) > sheight)
    zmisc_add_clip(0, sheight-wy, wwidth, wheight, &clips, &num_clips);
  
  root=GDK_ROOT_WINDOW();
  me=GDK_WINDOW_XWINDOW(lookup_widget(main_window, "tv_screen")->window);
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
  
  for (i++; i<nchildren; i++) {
    XGetWindowAttributes(dpy, children[i], &wts);
    if (!(wts.map_state & IsViewable))
      continue;
    
    x1=wts.x-main_info->window.x;
    y1=wts.y-main_info->window.y;
    x2=x1+wts.width+2*wts.border_width;
    y2=y1+wts.height+2*wts.border_width;
    if ((x2 < 0) || (x1 > (int)wwidth) || (y2 < 0) || (y1 > (int)wheight))
      continue;
    
    if (x1<0)      	     x1=0;
    if (y1<0)            y1=0;
    if (x2>(int)wwidth)  x2=wwidth;
    if (y2>(int)wheight) y2=wheight;
    zmisc_add_clip(x1, y1, x2, y2, &clips, &num_clips);
  }
  XFree((char *) children);
  
  main_info->window.clipcount = num_clips;
  main_info->window.clips = clips;
  tveng_set_preview_window(main_info);

  if (clips)
    free(clips);
}


static
gint zmisc_timeout_done (gpointer data);

static
gint zmisc_timeout_done (gpointer data)
{
  *((guint*)data) = 0;
  if (main_info->current_mode == TVENG_CAPTURE_PREVIEW)
      zmisc_get_clips(), tveng_set_preview_on(main_info);

  return FALSE; /* destroy the timeout */
}

/* Announces that the tv_screen has moved. This routine refreshes the
   old placement of the window if neccesary */
void zmisc_refresh_tv_screen(gint x, gint y, gint w, gint h)
{
  /* I'm using X directly here because the special properties the
     window should have. Probably just a popup window would do too,
     but this code is "borrowed" from xawtv */
  Window   win = GDK_ROOT_WINDOW();
  Display *dpy = GDK_DISPLAY();
  XSetWindowAttributes xswa;
  unsigned long mask;
  Window   tmp;
  static guint timeout_id = 0;
  gint timeout_length = 100; /* 0.1 sec */
  
  /* Just do the update (exitting zapping) */
  if ((!w && !h) && (!x && !y))
    {
      xswa.override_redirect = True;
      xswa.backing_store = NotUseful;
      xswa.save_under = False;
      mask = (CWSaveUnder | CWBackingStore| CWOverrideRedirect );
      tmp = XCreateWindow(dpy,win, 0, 0,
			  gdk_screen_width(), gdk_screen_height(), 0,
			  CopyFromParent, InputOutput, CopyFromParent,
			  mask, &xswa);
      XMapWindow(dpy, tmp);
      XUnmapWindow(dpy, tmp);
      XDestroyWindow(dpy, tmp);
      return;
    }
  
  if (timeout_id == 0) {
    if (main_info->current_mode == TVENG_CAPTURE_PREVIEW)
      tveng_set_preview_off(main_info);

    if ((main_info -> current_mode == TVENG_CAPTURE_PREVIEW) && (oldw != -1))
      {
	xswa.override_redirect = True;
	xswa.backing_store = NotUseful;
	xswa.save_under = False;
	mask = (CWSaveUnder | CWBackingStore| CWOverrideRedirect );
	tmp = XCreateWindow(dpy,win,
			    main_info->window.x,
			    main_info->window.y,
			    main_info->window.width,
			    main_info->window.height,
			    0, CopyFromParent, InputOutput,
			    CopyFromParent, mask, &xswa);
	XMapWindow(dpy, tmp);
	XUnmapWindow(dpy, tmp);
	XDestroyWindow(dpy, tmp);
      }
    timeout_id = gtk_timeout_add(timeout_length, zmisc_timeout_done,
				 &timeout_id);
  }
  else {
    gtk_timeout_remove(timeout_id);
    timeout_id = gtk_timeout_add(timeout_length, zmisc_timeout_done,
				 &timeout_id);
  }
  oldx = x;
  oldy = y;
  oldw = w;
  oldh = h;
}
