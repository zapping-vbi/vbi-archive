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

#include <gnome.h>
#include <gnome-xml/tree.h>
#include <gnome-xml/parser.h>
#include <gdk/gdkx.h>

#include "interface.h"
#include "support.h"
#include "tveng.h"
#include "v4l2interface.h"
#include "io.h"
#include "plugins.h"

gboolean flag_exit_program = FALSE;
GdkImage * dummy_image = NULL;

/* Keep compiler happy */
gboolean
delete_event                (GtkWidget       *widget,
			     GdkEvent        *event,
			     gpointer         user_data);

gboolean
delete_event                (GtkWidget       *widget,
			     GdkEvent        *event,
			     gpointer         user_data)
{
  flag_exit_program = TRUE;
  
  return FALSE;
}

void
on_tv_screen_size_allocate             (GtkWidget       *widget,
                                        GtkAllocation   *allocation,
                                        gpointer         user_data)
{
  tveng_device_info * info = (tveng_device_info*) user_data;

  /* Delete dummy_image */
  /* We must have something to free here */
  ((GdkImagePrivate*)dummy_image)->ximage->data = malloc(16);      
  gdk_image_destroy(dummy_image);

  /* This way errors don't segfault */
  dummy_image = NULL;
  
  if (tveng_set_capture_size(allocation->width, allocation->height, 
			     info) == -1)
    {
      fprintf(stderr, "%s\n", info->error);
      return;
    }

  /* Reallocate a new image */
  dummy_image = gdk_image_new(GDK_IMAGE_NORMAL,
			      gdk_visual_get_system(),
			      info->format.width,
			      info->format.height);

  /* We don't need the actual data */
  XFree(((GdkImagePrivate*)dummy_image)->ximage->data);  
}

int ShowBox(const gchar * message,
	    const gchar * message_box_type)
{
  GtkWidget * dialog;
  
  dialog = gnome_message_box_new(message, message_box_type,
				 GNOME_STOCK_BUTTON_OK,
				 NULL);

  return(gnome_dialog_run(GNOME_DIALOG(dialog)));
}

int main(int argc, char * argv[])
{
  char * videodev = "/dev/video";
  int value;
  int num_channels = 3;
  GtkWidget * main_window;
  GtkWidget * tv_screen;
  int channels[] = 
  {
    703250,
    815250,
    583250
  };


  tveng_device_info * main_info;

#ifdef ENABLE_NLS
  bindtextdomain (PACKAGE, PACKAGE_LOCALE_DIR);
  textdomain (PACKAGE);
#endif

  gnome_init ("zapping", VERSION, argc, argv);

  main_info = tveng_device_info_new( GDK_DISPLAY() );

  if (!main_info)
    {
      fprintf(stderr, "Cannot get device info struct\n");
      return -1;
    }

  /* We must do this (running zapping_setup_fb) before attaching the
     device because V4L and V4L2 don't support multiple capture opens
  */
  main_info -> file_name = strdup(videodev);

  if (!main_info -> file_name)
    {
      perror("strdup");
      return 1;
    }

  /* try to run the auxiliary suid program */
  if (tveng_run_zapping_setup_fb(main_info) == -1)
    {
      fprintf(stderr, "tveng_run_zapping_setup_fb: %s\n",
	      main_info->error);
      fprintf(stderr, "FIXME: Preview should be disabled now\n");
    }

  free(main_info -> file_name);

  if (tveng_attach_device(videodev, O_RDONLY, main_info) == -1)
    {
      fprintf(stderr, "tveng_attach_device: %s\n", main_info ->
	      error);
      return -1;
    }

  fcntl(main_info->fd,F_SETFD,FD_CLOEXEC);

  value = tveng_set_mute(0, main_info);
  if (value == -1)
    fprintf(stderr, "%s\n", main_info->error);
  srand(time(NULL));
  value = tveng_tune_input(channels[rand()%num_channels], main_info);
  if (value == -1)
    fprintf(stderr, "%s\n", main_info->error);

  /* read some frames from the device */
  if (tveng_start_capturing(main_info) == -1)
    fprintf(stderr, "%s\n", main_info->error);

  main_window = gnome_app_new("Zapping", "Zapping test window");
  tv_screen = gtk_drawing_area_new();

  gtk_widget_show (tv_screen);

  gnome_app_set_contents(GNOME_APP(main_window), tv_screen);

  gtk_signal_connect (GTK_OBJECT (main_window), "delete_event",
		      GTK_SIGNAL_FUNC (delete_event),
		      NULL);

  gtk_widget_show(main_window);

  gdk_window_resize (main_window->window, main_info->format.width,
		     main_info->format.height);

  gtk_signal_connect (GTK_OBJECT (tv_screen), "size_allocate",
		      GTK_SIGNAL_FUNC (on_tv_screen_size_allocate),
		      main_info);

  dummy_image = gdk_image_new(GDK_IMAGE_NORMAL,
			      gdk_visual_get_system(),
			      main_info->format.width,
			      main_info->format.height);

  /* We don't need the actual data */
  XFree(((GdkImagePrivate*)dummy_image)->ximage->data);

  while (!flag_exit_program)
    {
      while (gtk_events_pending())
	gtk_main_iteration(); /* Check for UI changes */

      if (flag_exit_program)
	continue; /* Exit the loop if neccesary now */

      /* Do the image processing here */
      if (tveng_read_frame(50000, main_info) == -1)
	printf("read(): %s\n", main_info->error);
      ((GdkImagePrivate*)dummy_image)->ximage->data = 
	main_info->format.data;

      gdk_draw_image(tv_screen -> window,
		     tv_screen -> style -> white_gc,
		     dummy_image,
		     0, 0, 0, 0,
		     main_info->format.width,
		     main_info->format.height);
    }

  if (tveng_stop_capturing(main_info) == -1)
    fprintf(stderr, "%s\n", main_info -> error);

  /* Mute the device again and close the device */
  tveng_set_mute(1, main_info);
  tveng_device_info_destroy(main_info);

  if (dummy_image)
    {
      /* We must have something to free here */
      ((GdkImagePrivate*)dummy_image)->ximage->data = malloc(16);      
      gdk_image_destroy(dummy_image);
    }
  return 0;
}
