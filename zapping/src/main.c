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
#include <gdk/gdkx.h>
#include <gnome-xml/tree.h>
#include <gnome-xml/parser.h>
#include <glade/glade.h>
#include <signal.h>
#define ZCONF_DOMAIN "/zapping/options/main/"
#include "zconf.h"
#include "zmisc.h"
#include "interface.h"
#include "tveng.h"
#include "v4linterface.h"
#include "plugins.h"
#include "frequencies.h"
#include "zvbi.h"
#include "mmx.h"
#include "overlay.h"
#include "capture.h"
#include "x11stuff.h"
#include "ttxview.h"

/* This comes from callbacks.c */
extern enum tveng_capture_mode restore_mode; /* the mode set when we went
						fullscreen */

/**** GLOBAL STUFF ****/

/* These are accessed by other modules as extern variables */
tveng_device_info	*main_info;
volatile gboolean	flag_exit_program = FALSE;
tveng_channels		*current_country = NULL;
GList			*plugin_list = NULL;
gboolean		disable_preview = FALSE;/* preview should be
						   disabled */
gboolean		disable_xv = FALSE; /* XVideo should be
					       disabled */
GtkWidget		*main_window;
gboolean		was_fullscreen=FALSE; /* will be TRUE if when
						 quitting we were
						 fullscreen */
tveng_tuned_channel	*global_channel_list=NULL;
/*** END OF GLOBAL STUFF ***/

static gboolean		disable_vbi = FALSE; /* TRUE for disabling VBI
						support */
static gint		newbttv = -1; /* Compatibility with old bttv
					 drivers */

static void shutdown_zapping(void);
static gboolean startup_zapping(void);

/*
 * This removes the bug when resizing toolbar makes the tv_screen have
 * 1 unit height.
*/
static gint old_height=-1;

static void
on_tv_screen_size_allocate	(GtkWidget	*widget,
				 GtkAllocation	*allocation,
				 gpointer	data)
{
  gint oldw;

  if (old_height == -1)
    old_height = gdk_screen_height()/2;

  if (!main_window->window)
    return;
  
  if (allocation->height == 1)
    gdk_window_resize(main_window->window,
		      main_window->allocation.width,
		      old_height);
  else
    gdk_window_get_size(main_window->window, &oldw, &old_height);
}

/* Adjusts geometry */
static gint timeout_handler(gpointer unused)
{
  GdkGeometry geometry;
  GdkWindowHints hints=0;

  if ((flag_exit_program) || (!main_window->window))
    return 0;

  if (main_info->current_mode != TVENG_CAPTURE_PREVIEW)
    {
      /* Set the geometry flags if needed */
      if (zcg_bool(NULL, "fixed_increments"))
	{
	  geometry.width_inc = 64;
	  geometry.height_inc = 48;
	  hints |= GDK_HINT_RESIZE_INC;
	}
      
      switch (zcg_int(NULL, "ratio")) {
      case 1:
	geometry.min_aspect = geometry.max_aspect = 4.0/3.0;
	hints |= GDK_HINT_ASPECT;
	break;
      case 2:
	geometry.min_aspect = geometry.max_aspect = 16.0/9.0;
	hints |= GDK_HINT_ASPECT;
	break;
      default:
	break;
      }
      
      gdk_window_set_geometry_hints(main_window->window, &geometry,
				    hints);
    }

  return 1; /* Keep calling me */
}

/* Start VBI services, and warn if we cannot */
static void
startup_teletext(void)
{
  /*
   * fixme: this appears to be a bug in the bttv2 driver, if we open
   * the vbi device and then the video device, the video device cannot
   * be opened (-EBUSY)
   * if we open video and then vbi it works.
   */
#ifdef HAVE_GDKPIXBUF
  if (disable_vbi)
    zconf_set_boolean(FALSE, "/zapping/options/vbi/enable_vbi");
  if ((!zvbi_open_device(newbttv)) &&
      (zconf_get_boolean(NULL, "/zapping/options/vbi/enable_vbi")))
    ShowBox(_("Sorry, but %s couldn't be opened:\n%s (%d)"),
	    GNOME_MESSAGE_BOX_ERROR, "/dev/vbi", strerror(errno), errno);
#else
  if (zconf_get_boolean(NULL, "/zapping/options/vbi/enable_vbi"))
    ShowBox(_("There's no GdkPixbuf support, VBI has been disabled"),
	    GNOME_MESSAGE_BOX_INFO);
  zconf_set_boolean(FALSE, "/zapping/options/vbi/enable_vbi");
#endif
  D();
}

int main(int argc, char * argv[])
{
  GtkWidget * tv_screen;
  gchar * buffer;
  GList * p;
  gint x, y, w, h; /* Saved geometry */
  gint x_bpp = -1;
  char *default_norm = NULL;
  gboolean oldbttv = FALSE;

  const struct poptOption options[] = {
    {
      "bpp",
      'b',
      POPT_ARG_INT,
      &x_bpp,
      0,
      N_("Color depth of the X display"),
      N_("BPP")
    },
    {
      "debug",
      'd',
      POPT_ARG_NONE,
      &debug_msg,
      0,
      N_("Set debug messages on"),
      NULL
    },
    {
      "no-vbi",
      0,
      POPT_ARG_NONE,
      &disable_vbi,
      0,
      N_("Disable VBI support"),
      NULL
    },
    {
      "old-bttv",
      0,
      POPT_ARG_NONE,
      &oldbttv,
      0,
      N_("VBI support for old (<0.5.2) bttv drivers"),
      NULL
    },
    {
      "tunerless-norm",
      'n',
      POPT_ARG_STRING,
      &default_norm,
      0,
      N_("Set the default standard/norm for tunerless inputs"),
      N_("NORM")
    },
    {
      "no-xv",
      'v',
      POPT_ARG_NONE,
      &disable_xv,
      0,
      N_("Disable XVideo extension support"),
      NULL
    },
    {
      NULL,
    } /* end the list */
  };

#ifdef ENABLE_NLS
  bindtextdomain (PACKAGE, PACKAGE_LOCALE_DIR);
  textdomain (PACKAGE);
#endif
  /* Init gnome, libglade, modules and tveng */
  gnome_init_with_popt_table ("zapping", VERSION, argc, argv, options,
			      0, NULL);

  printv("oldbttv : %s\n", oldbttv ? "ON" : "OFF");

  if (oldbttv)
    newbttv = 0;

  printv("%s\n%s %s, build date: %s\n",
	 "$Id: main.c,v 1.84 2001-01-27 22:46:11 garetxe Exp $", "Zapping", VERSION, __DATE__);
  printv("Checking for MMX support... ");
  switch (mm_support())
    {
    case 1:
      printv("MMX enabled.\n");
      break;
    case 3:
      printv("Cyrix MMX / Extended MMX. MMX enabled.\n");
      break;
    case 5:
      printv("AMD MMX / 3DNow!. MMX enabled.\n");
      break;
    default:
      printv("MMX not detected. Using plain C.\n");
      break;
    }
  glade_gnome_init();
  D();
  if (!g_module_supported ())
    {
      RunBox(_("Sorry, but there is no module support"),
	     GNOME_MESSAGE_BOX_ERROR);
      return 0;
    }
  D();
  main_info = tveng_device_info_new( GDK_DISPLAY(), x_bpp, default_norm);
  if (!main_info)
    {
      g_error(_("Cannot get device info struct"));
      return -1;
    }
  tveng_set_debug_level(debug_msg, main_info);
  tveng_set_xv_support(disable_xv, main_info);
  D();
  if (!startup_zapping())
    {
      RunBox(_("Zapping couldn't be started"),
	      GNOME_MESSAGE_BOX_ERROR);
      tveng_device_info_destroy(main_info);
      return 0;
    }
  D();
  /* We must do this (running zapping_setup_fb) before attaching the
     device because V4L and V4L2 don't support multiple capture opens
  */
 open_device:

  main_info -> file_name = strdup(zcg_char(NULL, "video_device"));

  if (!main_info -> file_name)
    {
      perror("strdup");
      return 1;
    }
  D();
  tveng_set_zapping_setup_fb_verbosity(zcg_int(NULL,
					       "zapping_setup_fb_verbosity"),
				       main_info);
  D();
  /* try to run the auxiliary suid program */
  if (tveng_run_zapping_setup_fb(main_info) == -1)
    g_message("Error while executing zapping_setup_fb,\n"
	      "Overlay might not work:\n%s", main_info->error);
  D();
  free(main_info -> file_name);
  D();
  if (tveng_attach_device(zcg_char(NULL, "video_device"),
			  TVENG_ATTACH_XV,
			  main_info) == -1)
    {
      /* Check that the given device is /dev/video, if it isn't, try
	 it */
      if (strcmp(zcg_char(NULL, "video_device"), "/dev/video"))
	{
	  GtkWidget * question_box = gnome_message_box_new(
               _("The specified device isn't \"/dev/video\".\n"
		 "Should I try it?"),
	       GNOME_MESSAGE_BOX_QUESTION,
	       GNOME_STOCK_BUTTON_YES,
	       GNOME_STOCK_BUTTON_NO,
	       NULL);

	  gtk_window_set_title(GTK_WINDOW(question_box),
			       zcg_char(NULL, "video_device"));
	 
	  switch (gnome_dialog_run(GNOME_DIALOG(question_box)))
	    {
	    case 0: /* Retry */
	      zcs_char("/dev/video", "video_device");
	      goto open_device;
	    default:
	      break; /* Don't do anything */
	    }
	}

      buffer =
	g_strdup_printf(_("Sorry, but \"%s\" could not be opened:\n%s"),
			zcg_char(NULL, "video_device"),
			main_info->error);

      RunBox(buffer, GNOME_MESSAGE_BOX_ERROR);

      g_free(buffer);

      return -1;
    }
  D();
  /* mute the device while we are starting up */
  tveng_set_mute(1, main_info);
  D();
  main_window = create_zapping();
  D();
  tv_screen = lookup_widget(main_window, "tv_screen");
  /* Avoid dumb resizes to 1 pixel height */
  gtk_signal_connect(GTK_OBJECT(tv_screen), "size-allocate",
		     GTK_SIGNAL_FUNC(on_tv_screen_size_allocate), NULL);
  /* set periodically the geometry flags on the main window */
  gtk_timeout_add(100, (GtkFunction)timeout_handler, NULL);
  /* ensure that the main window is realized */
  gtk_widget_show(main_window);
  while (!tv_screen->window)
    z_update_gui();
  D();
  if (!startup_capture(tv_screen))
    {
      RunBox("The capture couldn't be started", GNOME_MESSAGE_BOX_ERROR);
      tveng_device_info_destroy(main_info);
      return 0;
    }
  D();
  /* Add the plugins to the GUI */
  p = g_list_first(plugin_list);
  while (p)
    {
      plugin_add_gui(GNOME_APP(main_window),
		     (struct plugin_info*)p->data);
      p = p->next;
    }
  /* Disable preview if needed */
  if (disable_preview)
    {
      printv("Preview disabled, removing GUI items\n");
      gtk_widget_set_sensitive(lookup_widget(main_window, "go_previewing2"),
			       FALSE);
      gtk_widget_hide(lookup_widget(main_window, "go_previewing2"));
      gtk_widget_set_sensitive(lookup_widget(main_window, "go_fullscreen1"),
			       FALSE);
      gtk_widget_hide(lookup_widget(main_window, "go_fullscreen1"));
    }
  /* Restore the input and the standard */
  tveng_set_input_by_index(zcg_int(NULL, "current_input"), main_info);
  D();
  tveng_set_standard_by_index(zcg_int(NULL, "current_standard"), main_info);
  D();
  update_standards_menu(main_window, main_info);
  D();
  startup_teletext();
  D();
  startup_ttxview();
  D();
  /* disable VBI if needed */
  if (!zvbi_get_object())
    {
      printv("VBI disabled, removing GUI items\n");
      gtk_widget_set_sensitive(lookup_widget(main_window, "separador5"),
			       FALSE);
      gtk_widget_hide(lookup_widget(main_window, "separador5"));
      gtk_widget_set_sensitive(lookup_widget(main_window, "videotext1"),
			       FALSE);
      gtk_widget_hide(lookup_widget(main_window, "videotext1"));
      gtk_widget_set_sensitive(lookup_widget(main_window, "videotext3"),
			       FALSE);
      gtk_widget_hide(lookup_widget(main_window, "videotext3"));
      gtk_widget_set_sensitive(lookup_widget(main_window, "new_ttxview"),
			       FALSE);
      gtk_widget_hide(lookup_widget(main_window, "new_ttxview"));
      /* Set the capture mode to a default value and disable VBI */
      if (zcg_int(NULL, "capture_mode") == TVENG_NO_CAPTURE)
	zcs_int(TVENG_CAPTURE_READ, "capture_mode");
    }
  D();
  /* Disable the View menu completely if it is redundant */
  if ((!zvbi_get_object()) && (disable_preview))
    {
      gtk_widget_set_sensitive(lookup_widget(main_window, "go_capturing2"),
			       FALSE);
      gtk_widget_hide(lookup_widget(main_window, "go_capturing2"));
      gtk_widget_set_sensitive(lookup_widget(main_window, "view1"),
			       FALSE);
      gtk_widget_hide(lookup_widget(main_window, "view1"));      
    }
  D();
  printv("switching to mode %d (%d)\n", zcg_int(NULL,
						"capture_mode"),
	 TVENG_CAPTURE_READ);
  /* Start the capture in the last mode */
  if (!disable_preview)
    {
      if (zmisc_switch_mode(zcg_int(NULL, "capture_mode"), main_info)
	  == -1)
	{
	  ShowBox(_("Cannot restore previous mode%s:\n%s"),
		  GNOME_MESSAGE_BOX_ERROR,
		  (zcg_int(NULL, "capture_mode") == TVENG_CAPTURE_READ) ? ""
		  : _(", I will try starting capture mode"),
		  main_info->error);
	  if ((zcg_int(NULL, "capture_mode") != TVENG_CAPTURE_READ) &&
	      (zmisc_switch_mode(TVENG_CAPTURE_READ, main_info) == -1))
	    ShowBox(_("Capture mode couldn't be started either:\n%s"),
		    GNOME_MESSAGE_BOX_ERROR, main_info->error);
	}
      else
	{
	  /* in callbacks.c */
	  extern enum tveng_capture_mode restore_mode;

	  restore_mode = TVENG_CAPTURE_WINDOW;
	}
    }
  else /* preview disabled */
      if (zmisc_switch_mode(TVENG_CAPTURE_READ, main_info) == -1)
	ShowBox(_("Capture mode couldn't be started:\n%s"),
		GNOME_MESSAGE_BOX_ERROR, main_info->error);
  D();
  /* hide toolbars and co. if necessary */
  if (zconf_get_boolean(NULL, "/zapping/internal/callbacks/hide_controls"))
    {
      gtk_widget_hide(lookup_widget(main_window, "dockitem1"));
      gtk_widget_hide(lookup_widget(main_window, "dockitem2"));
      gtk_widget_queue_resize(main_window);
    }
  if (zconf_get_boolean(NULL, "/zapping/internal/callbacks/hide_extra"))
    {
      gtk_widget_hide(lookup_widget(main_window, "Inputs"));
      gtk_widget_hide(lookup_widget(main_window, "Standards"));
      gtk_widget_queue_resize(main_window);
    }
  D();
  /* Sets the coords to the previous values, if the users wants to */
  if (zcg_bool(NULL, "keep_geometry"))
    {
      zconf_get_integer(&x, "/zapping/internal/callbacks/x");
      zconf_get_integer(&y, "/zapping/internal/callbacks/y");
      zconf_get_integer(&w, "/zapping/internal/callbacks/w");
      zconf_get_integer(&h, "/zapping/internal/callbacks/h");
      printv("Restoring geometry: <%d,%d> <%d x %d>\n", x, y, w, h);
      /* prevents the toolbar from resizing the window */
      z_update_gui();
      gdk_window_move_resize(main_window->window, x, y, w, h);
    }
  D();
  gdk_window_set_back_pixmap(tv_screen->window, NULL, FALSE);
  D();
  if (-1 == tveng_set_mute(zcg_bool(NULL, "start_muted"), main_info))
    printv("%s\n", main_info->error);
  D(); printv("going into main loop...\n");
  /* That's it, now go to the main loop */
  gtk_main();
  D();
  /* Closes all fd's, writes the config to HD, and that kind of things
   */
  shutdown_zapping();
  D();
  return 0;
}

static void shutdown_zapping(void)
{
  int i = 0;
  gchar * buffer = NULL;
  tveng_tuned_channel * channel;
  gboolean do_screen_cleanup = FALSE;

  /* Stops any capture currently active */
  if (main_info->current_mode == TVENG_CAPTURE_WINDOW)
    do_screen_cleanup = TRUE;

  if (was_fullscreen)
    zcs_int(TVENG_CAPTURE_PREVIEW, "capture_mode");

  tveng_set_mute(1, main_info);
  
  /* Unloads all plugins, this tells them to save their config too */
  plugin_unload_plugins(plugin_list);
  plugin_list = NULL;

  /* Write the currently tuned channels */
  zconf_delete(ZCONF_DOMAIN "tuned_channels");
  while ((channel = tveng_retrieve_tuned_channel_by_index(i,
							  global_channel_list))
	 != NULL)
    {
      buffer = g_strdup_printf(ZCONF_DOMAIN "tuned_channels/%d/name",
			       i);
      zconf_create_string(channel->name, "Channel name", buffer);
      g_free(buffer);
      buffer = g_strdup_printf(ZCONF_DOMAIN "tuned_channels/%d/freq",
			       i);
      zconf_create_integer((int)channel->freq, "Tuning frequence", buffer);
      g_free(buffer);
      buffer = g_strdup_printf(ZCONF_DOMAIN "tuned_channels/%d/accel_key",
			       i);
      zconf_create_integer((int)channel->accel_key, "Accelator key",
			   buffer);
      g_free(buffer);
      buffer = g_strdup_printf(ZCONF_DOMAIN "tuned_channels/%d/accel_mask",
			       i);
      zconf_create_integer((int)channel->accel_mask, "Accelerator mask",
			   buffer);
      g_free(buffer);
      buffer = g_strdup_printf(ZCONF_DOMAIN "tuned_channels/%d/real_name",
			       i);
      zconf_create_string(channel->real_name, "Real channel name", buffer);
      g_free(buffer);
      buffer = g_strdup_printf(ZCONF_DOMAIN "tuned_channels/%d/country",
			       i);
      zconf_create_string(channel->country, 
			  "Country the channel is in", buffer);
      g_free(buffer);
      i++;
    }
  global_channel_list = tveng_clear_tuned_channel(global_channel_list);

  zcs_char(current_country -> name, "current_country");
  if (main_info->num_standards)
    zcs_int(main_info -> cur_standard, "current_standard");
  if (main_info->num_inputs)
    zcs_int(main_info -> cur_input, "current_input");

  /* Shutdown all other modules */
  shutdown_callbacks();

  /* Shut down vbi */
  zvbi_close_device();

  /*
   * Shuts down the teletext view
   */
  shutdown_ttxview();

  /* Save the config and show an error if something failed */
  if (!zconf_close())
    ShowBox(_("ZConf could not be closed properly , your\n"
	      "configuration will be lost.\n"
	      "Possible causes for this are:\n"
	      "   - There is not enough free memory\n"
	      "   - You do not have permissions to write to $HOME/.zapping\n"
	      "   - libxml is non-functional (?)\n"
	      "   - or, more probably, you have found a bug in\n"
	      "     Zapping. Please contact the author.\n"
	      ), GNOME_MESSAGE_BOX_ERROR);

  /*
   * Tell the overlay engine to shut down and to do a cleanup if necessary
   */
  shutdown_overlay(do_screen_cleanup);
  /*
   * Shuts down the capture engine
   */
  shutdown_capture(main_info);

  /* Close */
  tveng_device_info_destroy(main_info);
}

static gboolean startup_zapping()
{
  int i = 0;
  gchar * buffer = NULL;
  gchar * buffer2 = NULL;
  tveng_tuned_channel new_channel;
  GList * p;
  D();
  /* Starts the configuration engine */
  if (!zconf_init("zapping"))
    {
      g_error(_("Sorry, Zapping is unable to create the config tree"));
      return FALSE;
    }
  D();
  /* Sets defaults for zconf */
  zcc_bool(TRUE, "Save and restore zapping geometry (non ICCM compliant)", 
	   "keep_geometry");
  zcc_bool(FALSE, "Resize by fixed increments", "fixed_increments");
  zcc_char(tveng_get_country_tune_by_id(0)->name,
	     "The country you are currently in", "current_country");
  current_country = 
    tveng_get_country_tune_by_name(zcg_char(NULL, "current_country"));
  zcc_char("/dev/video", "The device file to open on startup",
	   "video_device");
  zcc_bool(FALSE, "TRUE if Zapping should be started without sound",
	   "start_muted");
  zcc_bool(TRUE, "TRUE if some flickering should be avoided in preview mode",
	   "avoid_flicker");
  zcc_int(0, "Verbosity value given to zapping_setup_fb",
	  "zapping_setup_fb_verbosity");
  zcc_int(0, "Ratio mode", "ratio");
  zcc_int(0, "Change the video mode when going fullscreen", "change_mode");
  zcc_int(0, "Current standard", "current_standard");
  zcc_int(0, "Current input", "current_input");
  zcc_int(TVENG_CAPTURE_WINDOW, "Current capture mode", "capture_mode");
  zcc_bool(FALSE, "In videotext mode", "videotext_mode");
  zconf_create_boolean(FALSE, "Hide controls",
		       "/zapping/internal/callbacks/hide_controls");
  D();
  /* Loads all the tuned channels */
  while (zconf_get_nth(i, &buffer, ZCONF_DOMAIN "tuned_channels") !=
	 NULL)
    {
      g_assert(strlen(buffer) > 0);

      if (buffer[strlen(buffer)-1] == '/')
	buffer[strlen(buffer)-1] = 0;

      /* Get all the items from here  */
      buffer2 = g_strconcat(buffer, "/name", NULL);
      zconf_get_string(&new_channel.name, buffer2);
      g_free(buffer2);
      buffer2 = g_strconcat(buffer, "/real_name", NULL);
      zconf_get_string(&new_channel.real_name, buffer2);
      g_free(buffer2);
      buffer2 = g_strconcat(buffer, "/freq", NULL);
      zconf_get_integer(&new_channel.freq, buffer2);
      g_free(buffer2);
      buffer2 = g_strconcat(buffer, "/accel_key", NULL);
      zconf_get_integer(&new_channel.accel_key, buffer2);
      g_free(buffer2);
      buffer2 = g_strconcat(buffer, "/accel_mask", NULL);
      zconf_get_integer(&new_channel.accel_mask, buffer2);
      g_free(buffer2);
      buffer2 = g_strconcat(buffer, "/country", NULL);
      zconf_get_string(&new_channel.country, buffer2);
      g_free(buffer2);

      new_channel.index = 0;
      global_channel_list =
	tveng_insert_tuned_channel(&new_channel, global_channel_list);

      /* Free the previously allocated mem */
      g_free(new_channel.name);
      g_free(new_channel.real_name);
      g_free(new_channel.country);

      g_free(buffer);
      i++;
    }
  D();
  /* Starts all modules */
  if (!startup_callbacks())
    return FALSE;
  D();
  /* Loads the modules */
  plugin_list = plugin_load_plugins();
  D();
  /* init them, and remove the ones that couldn't be inited */
 restart_loop:

  p = g_list_first(plugin_list);
  while (p)
    {
      plugin_load_config((struct plugin_info*)p->data);
      if (!plugin_init(main_info, (struct plugin_info*)p->data))
	{
	  plugin_unload((struct plugin_info*)p->data);
	  plugin_list = g_list_remove_link(plugin_list, p);
	  g_list_free_1(p);
	  goto restart_loop;
	}
      p = p->next;
    }
  D();
  return TRUE;
}
