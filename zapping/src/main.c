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
#include <signal.h>

/* FIXME: needed just for testing zconf */
#include <string.h>

#include "interface.h"
#include "support.h"
#include "tveng.h"
#include "v4l2interface.h"
#include "io.h"
#include "plugins.h"
#include "zconf.h"
#include "zmisc.h"

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
  dummy_image = gdk_image_new(GDK_IMAGE_FASTEST,
			      gdk_visual_get_system(),
			      info->format.width,
			      info->format.height);

}

int main(int argc, char * argv[])
{
  char * videodev = "/dev/video";
  int value;
  int num_channels = 3;
  char * string_test_1 = "/zapping/tests/string/one";
  char * float_test_1 = "/zapping/tests/float/one";
  char * boolean_test_1 = "/zapping/tests/boolean/one";
  char * integer_test_1 = "/zapping/tests/integer/one";

  gchar * string_1;
  gfloat float_1;
  gboolean boolean_1;
  gint integer_1;

  /* Default values */
  gchar * d_string_1 = "Something is cooking in the oven";
  gfloat d_float_1 = -5.7e-13;
  gfloat d_boolean_1 = TRUE;
  gfloat d_integer_1 = -42;

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

  if (!zconf_init("zapping"))
    {
      g_error(_("Sorry, Zapping is unable to create the config tree"));
      return 0;
    }

  /* Do some tests on the configuration */
  /* Query values */
  if ((!zconf_get_string(&string_1, string_test_1)) &&
      (zconf_error()))
    g_warning("%s didn't exist", string_test_1);
  if ((!zconf_get_float(&float_1, float_test_1)) &&
      (zconf_error()))
    g_warning("%s didn't exist", float_test_1);
  if ((!zconf_get_boolean(&boolean_1, boolean_test_1)) &&
      (zconf_error()))
    g_warning("%s didn't exist", boolean_test_1);
  if ((!zconf_get_integer(&integer_1, integer_test_1)) &&
      (zconf_error()))
    g_warning("%s didn't exist", integer_test_1);

  /* Set default values for some of them */
  zconf_create_string(d_string_1, "String test 1", string_test_1);
  zconf_create_float(d_float_1, "Float test 1", float_test_1);
  zconf_create_boolean(d_boolean_1, "Boolean test 1", boolean_test_1);
  zconf_create_integer(d_integer_1, "Integer test 1", integer_test_1);

  /* Query new values */
  if ((!zconf_get_string(&string_1, string_test_1)) &&
      (zconf_error()))
    g_warning("%s cannot be queried", string_test_1);
  if ((!zconf_get_float(&float_1, float_test_1)) &&
      (zconf_error()))
    g_warning("%s cannot be queried", float_test_1);
  if ((!zconf_get_boolean(&boolean_1, boolean_test_1)) &&
      (zconf_error()))
    g_warning("%s cannot be queried", boolean_test_1);
  if ((!zconf_get_integer(&integer_1, integer_test_1)) &&
      (zconf_error()))
    g_warning("%s cannot be queried", integer_test_1);

  /* Change the current values a bit */
  string_1 = strfry(string_1);
  float_1 = float_1 * M_PI * M_PI;
  boolean_1 = !boolean_1;
  srand(time(NULL));
  integer_1 = integer_1 + rand();

  /* Set the new values */
  zconf_set_string(string_1, string_test_1);
  zconf_set_float(float_1, float_test_1);
  zconf_set_boolean(boolean_1, boolean_test_1);
  zconf_set_integer(integer_1, integer_test_1);

  /* Print the values with the descriptions */
  printf("%s: %s\n", zconf_get_description(NULL, string_test_1), string_1);
  printf("%s: %g\n", zconf_get_description(NULL, float_test_1), float_1);
  printf("%s: %s\n", zconf_get_description(NULL, boolean_test_1),
	 boolean_1 ? "TRUE" : "FALSE");
  printf("%s: %d\n", zconf_get_description(NULL, integer_test_1), integer_1);

  g_free(string_1);

  zconf_close();

  main_info = tveng_device_info_new( GDK_DISPLAY() );

  /* Ignore alarms */
  signal(SIGALRM, SIG_IGN);

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

  dummy_image = gdk_image_new(GDK_IMAGE_FASTEST,
			      gdk_visual_get_system(),
			      main_info->format.width,
			      main_info->format.height);

  while (!flag_exit_program)
    {
      while (gtk_events_pending())
	gtk_main_iteration(); /* Check for UI changes */

      if (flag_exit_program)
	continue; /* Exit the loop if neccesary now */

      /* Do the image processing here */
      if (tveng_read_frame(((GdkImagePrivate*)dummy_image)-> ximage->
			   data,
			   ((int)dummy_image->bpl)*dummy_image->height,
			  50, main_info) == -1)
	{
	  printf("read(): %s\n", main_info->error);
	  continue;
	}

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
    gdk_image_destroy(dummy_image);

  return 0;
}
