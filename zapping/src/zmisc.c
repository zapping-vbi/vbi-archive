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

#define ZCONF_DOMAIN "/zapping/options/main/"
#include "zmisc.h"
#include "zconf.h"
#include "tveng.h"
#include "interface.h"
#include "callbacks.h"
#include "zvbi.h"
#include "x11stuff.h"
#include "overlay.h"

extern tveng_device_info * main_info;
extern GtkWidget * main_window;
extern gboolean disable_preview; /* TRUE if preview won't work */
static GdkImage * zimage = NULL; /* The buffer that holds the capture */
gboolean debug_msg=FALSE; /* Debugging messages on or off */

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

/* Takes and hex number with only decimal digits in its
   representation, and returns the representation as if it was in 10,
   base. I.e. 0x123 becomes 123 (decimal) */
gint hex2dec(gint hex)
{
  int returned_value=0;
  gchar * representation = g_strdup_printf("%x", hex);
  sscanf(representation, "%d", &returned_value);
  g_free(representation);
  return returned_value;
}

/* The inverse of the above, converts 145d to 0x145 */
gint dec2hex(gint dec)
{
  int returned_value=0;
  gchar * representation = g_strdup_printf("%d", dec);
  sscanf(representation, "%x", &returned_value);
  g_free(representation);
  return returned_value;
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
	  new_data = zimage_get_data(new_zimage);
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
  return (x11_get_data(image));
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

/*
  Given a bpp (bites per pixel) and the endianess, returns the proper
  TVeng RGB mode.
  returns -1 if the mode is unknown.
*/
static enum tveng_frame_pixformat
zmisc_resolve_pixformat(int bpp, GdkByteOrder byte_order)
{
  switch (bpp)
    {
    case 15:
      return TVENG_PIX_RGB555;
      break;
    case 16:
      return TVENG_PIX_RGB565;
      break;
    case 24:
      if (byte_order == GDK_MSB_FIRST)
	return TVENG_PIX_RGB24;
      else
	return TVENG_PIX_BGR24;
      break;
    case 32:
      if (byte_order == GDK_MSB_FIRST)
	return TVENG_PIX_RGB32;
      else
	return TVENG_PIX_BGR32;
      break;
    default:
      g_warning("Unrecognized image bpp: %d",
		bpp);
      break;
    }
  return -1;
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
  enum tveng_frame_pixformat format;

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

  if (new_mode != TVENG_NO_CAPTURE)
    zvbi_set_mode(FALSE);

  switch (new_mode)
    {
    case TVENG_CAPTURE_READ:
      format = zmisc_resolve_pixformat(x11_get_bpp(),
				       x11_get_byte_order());

      if (format != -1)
	{
	  info->format.pixformat = format;
	  if (info->format.pixformat != format)
	    g_warning("Couldn't set format correctly: %s", info->error);
	}

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

      format = zmisc_resolve_pixformat(tveng_get_display_depth(info),
				       x11_get_byte_order());

      if (format != -1)
	{
	  info->format.pixformat = format;
	  tveng_set_capture_format(info);
	  if (info->format.pixformat != format)
	    g_warning("Couldn't set format correctly: %s", info->error);
	}

      info->window.x = x;
      info->window.y = y;
      info->window.width = w;
      info->window.height = h;
      info->window.clipcount = 0;
      tveng_set_preview_window(info);
      return_value = tveng_start_window(info);
      if (return_value != -1)
	overlay_sync(TRUE);
      else
	g_warning(info->error);
      break;
    case TVENG_CAPTURE_PREVIEW:
      if (disable_preview) {
	g_warning("preview has been disabled");
	return -1;
      }

      format = zmisc_resolve_pixformat(tveng_get_display_depth(info),
				       x11_get_byte_order());
      if (format != -1)
	{
	  info->format.pixformat = format;
	  tveng_set_capture_format(info);
	  if (info->format.pixformat != format)
	    g_warning("Couldn't set format correctly: %s", info->error);
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
