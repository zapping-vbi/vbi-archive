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

GdkImage * zimage = NULL; /* The buffer that holds the capture */

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
