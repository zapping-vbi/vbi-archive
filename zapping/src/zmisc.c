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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gdk/gdkx.h>
#define ZCONF_DOMAIN "/zapping/options/main/"
#include "zmisc.h"
#include "zconf.h"
#include "tveng.h"
#include "interface.h"
#include "callbacks.h"
#include "zvbi.h"

extern tveng_device_info * main_info;
extern GtkWidget * main_window;
extern gboolean disable_preview; /* TRUE if preview won't work */
static GdkImage * zimage = NULL; /* The buffer that holds the capture */
static gint oldx=-1, oldy=-1, oldw=-1, oldh=-1; /* Last geometry of the
					    Zapping window */
static gint curx, cury, curw=-1, curh=-1;
/* current geometry of the window */
static gboolean obscured = FALSE;
gboolean ignore_next_expose = FALSE;
static guint timeout_id = 0;

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
  /* the border is because of the possible dword-alignings */
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

/* Force an expose event in the given area */
static 
void zmisc_clear_area(gint x, gint y, gint width, gint height);

static GtkWidget * clear_window = NULL;

static
void zmisc_clear_area(gint x, gint y, gint width, gint height)
{
  if (!clear_window)
    {
      clear_window = gtk_window_new( GTK_WINDOW_POPUP );

      gtk_widget_set_uposition(clear_window, x, y);
      gtk_widget_set_usize(clear_window, width, height);
    }

  gtk_widget_show(clear_window);
  gdk_window_set_decorations(clear_window->window, 0);
  gdk_window_move_resize(clear_window->window, x, y, width, height);

  gtk_widget_hide(clear_window);
}

static
gint zmisc_timeout_done (gpointer data);

static
gint zmisc_timeout_done (gpointer data)
{
  *((guint*)data) = 0;
  if (main_info->current_mode == TVENG_CAPTURE_WINDOW)
    {
      main_info->window.x = curx;
      main_info->window.y = cury;
      main_info->window.width = curw;
      main_info->window.height = curh;
      main_info->window.clipcount = 0;
      tveng_set_preview_window(main_info);
      zmisc_get_clips(); /* fills main_info->window.clips and calls
			    tveng_set_window */
      if (!obscured)
	tveng_set_preview_on(main_info);
    }

  return FALSE; /* destroy the timeout */
}

/* Announces that the tv_screen has moved. This routine refreshes the
   old placement of the window if neccesary */
void zmisc_refresh_tv_screen(gint x, gint y, gint w, gint h, gboolean
			     obscured_param)
{
  gint timeout_length = 100; /* 0.1 sec */

  curx = x; cury = y; curw = w; curh = h; obscured = obscured_param;

  /* Just do the update (exitting zapping) */
  if ((!w && !h) && (!x && !y))
    {
      zmisc_clear_area(0, 0, gdk_screen_width(), gdk_screen_height());
      if (timeout_id)
	gtk_timeout_remove(timeout_id);
      return;
    }
  
  if (timeout_id == 0) {
    if (main_info->current_mode == TVENG_CAPTURE_WINDOW)
      tveng_set_preview_off(main_info);

    if (main_info -> current_mode == TVENG_CAPTURE_WINDOW)
      {
	if (zcg_bool(NULL, "avoid_flicker"))
	  zmisc_clear_area(main_info->window.x, main_info->window.y,
			   main_info->window.width,
			   main_info->window.height);
	else
	  zmisc_clear_area(0, 0, gdk_screen_width(), gdk_screen_height());

	ignore_next_expose = TRUE;
      }
    timeout_id = gtk_timeout_add(timeout_length, zmisc_timeout_done,
				 &timeout_id);
  }
  else {
    gtk_timeout_remove(timeout_id);
    timeout_id = gtk_timeout_add(timeout_length, zmisc_timeout_done,
				 &timeout_id);
  }
  gdk_window_get_origin(main_window->window, &oldx, &oldy);
  gdk_window_get_size(main_window->window, &oldw, &oldh);
}

/* Clears any timers zmisc could use (the Zapping window is to be closed) */
void zmisc_clear_timers(void)
{
  if (timeout_id != 0)
    gtk_timeout_remove(timeout_id);

  timeout_id = 0;
}

/*
  does the mode switching. Since this requires more than just using
  tveng, a new routine is needed.
  Returns whatever tveng returns, but we print the message ourselves
  too, so no need to aknowledge it to the user.
  Side efects: Stops whatever mode was being used before.
*/
int
zmisc_switch_mode(enum tveng_capture_mode new_mode,
		  tveng_device_info * info)
{
  GtkWidget * tv_screen;
  int return_value = 0;
  GtkAllocation dummy_alloc;
  gint x, y, w, h;

  g_assert(info != NULL);
  g_assert(main_window != NULL);
  tv_screen = lookup_widget(main_window, "tv_screen");
  g_assert(tv_screen != NULL);

  if (info->current_mode == new_mode)
    return 0; /* success */

  gdk_window_get_size(tv_screen->window, &w, &h);
  gdk_window_get_origin(tv_screen->window, &x, &y);

  /* If we are fullscreen, something else needs to be done */
  if (info->current_mode == TVENG_CAPTURE_PREVIEW)
    {
      extern GtkWidget *black_window; /* from callbacks.c */
      gdk_keyboard_ungrab(GDK_CURRENT_TIME);
      gtk_widget_destroy(black_window);
    }
  tveng_stop_everything(info);

  switch (new_mode)
    {
    case TVENG_CAPTURE_READ:
      return_value = tveng_start_capturing(info);
      if (return_value != -1)
	{
	  dummy_alloc.width = w;
	  dummy_alloc.height = h;
	  on_tv_screen_size_allocate(tv_screen, &dummy_alloc, NULL);
	}
      else
	g_warning(info->error);
      break;
    case TVENG_CAPTURE_WINDOW:
      if (disable_preview) {
	g_warning("preview has been disabled");
	return -1;
      }
      info->window.x = x;
      info->window.y = y;
      info->window.width = w;
      info->window.height = h;
      info->window.clipcount = 0;
      tveng_set_preview_window(info);
      return_value = tveng_start_window(info);
      if (return_value != -1)
	zmisc_refresh_tv_screen(x, y, w, h, FALSE);
      else
	g_warning(info->error);
      break;
    case TVENG_CAPTURE_PREVIEW:
      if (disable_preview) {
	g_warning("preview has been disabled");
	return -1;
      }
      on_go_fullscreen1_activate(
       GTK_MENU_ITEM(lookup_widget(tv_screen, "go_fullscreen1")),
       NULL);
      if (main_info->current_mode != TVENG_CAPTURE_PREVIEW)
	{
	  g_warning(info->error);
	  return_value = -1;
	}
      break;
    default:
      break; /* TVENG_NO_CAPTURE */
    }

  return return_value;
}
