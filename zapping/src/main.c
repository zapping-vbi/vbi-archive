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
#include "cpu.h"
#include "overlay.h"
#include "capture.h"
#include "x11stuff.h"
#include "ttxview.h"
#include "yuv2rgb.h"
#include "osd.h"
#include "remote.h"
#include "cmd.h"
#include "audio.h"
#include "csconvert.h"
#include "properties-handler.h"
#include "properties.h"
#include "mixer.h"
#include "keyboard.h"
#include "globals.h"
#include "plugin_properties.h"

#ifndef HAVE_PROGRAM_INVOCATION_NAME
char *program_invocation_name;
char *program_invocation_short_name;
#endif

/*** END OF GLOBAL STUFF ***/

static gboolean		disable_vbi = FALSE; /* TRUE for disabling VBI
						support */

static void shutdown_zapping(void);
static gboolean startup_zapping(gboolean load_plugins);

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
    gdk_window_get_geometry(main_window->window, NULL, NULL, &oldw,
			    &old_height, NULL);
}

/* Adjusts geometry */
static gint timeout_handler(gpointer unused)
{
  GdkGeometry geometry;
  GdkWindowHints hints = (GdkWindowHints) 0;
  GtkWidget *tv_screen;
  gint tvs_w, tvs_h, mw_w, mw_h = 0;
  double rw = 0, rh=0;

  if ((flag_exit_program) || (!main_window->window))
    return 0;

  if (main_info->current_mode != TVENG_CAPTURE_PREVIEW)
    {
      /* Set the geometry flags if needed */
      if (zcg_bool(NULL, "fixed_increments"))
	{
	  geometry.width_inc = 64;
	  geometry.height_inc = 48;
	  hints |= (GdkWindowHints) GDK_HINT_RESIZE_INC;
	}
      
      switch (zcg_int(NULL, "ratio")) {
      case 1:
	rw = 4;
	rh = 3;
	break;
      case 2:
	rw = 16;
	rh = 9;
	break;
#ifdef HAVE_LIBZVBI
      case 3:
	{
	  extern double zvbi_ratio;

	  rw = zvbi_ratio;
	  rh = 1;
	  break;
	}
#endif
      default:
	break;
      }

      if (rw)
	{
	  hints |= (GdkWindowHints) GDK_HINT_ASPECT;

	  /* toolbars correction */
	  tv_screen = lookup_widget(main_window, "tv_screen");
	  gdk_window_get_geometry(tv_screen->window, NULL, NULL,
				  &tvs_w, &tvs_h, NULL);
	  gdk_window_get_geometry(main_window->window, NULL, NULL,
				  &mw_w, &mw_h, NULL);

	  rw *= (((double)mw_w)/tvs_w);
	  rh *= (((double)mw_h)/tvs_h);

	  geometry.min_aspect = geometry.max_aspect = rw/rh;
	}
      
      gdk_window_set_geometry_hints(main_window->window, &geometry,
				    hints);
#ifdef HAVE_LIBZVBI
      {
	extern double zvbi_ratio;
	static double old_ratio = 0;

	if (old_ratio != zvbi_ratio &&
	    zcg_int(NULL, "ratio") == 3 &&
	    mw_h > 1 &&
	    geometry.min_aspect > 0.1)
	  {
	    /* ug, ugly */
	    gdk_window_get_geometry(main_window->window, NULL, NULL,
				    &mw_w, &mw_h, NULL);
	    gdk_window_resize(main_window->window,
			      (int)(mw_h * geometry.min_aspect), mw_h);
	    old_ratio = zvbi_ratio;
	  }
      }
#endif
    }

  return 1; /* Keep calling me */
}

gboolean
on_zapping_key_press			(GtkWidget	*widget,
					 GdkEventKey	*event,
					 gpointer	*user_data);
gboolean
on_zapping_key_press			(GtkWidget	*widget,
					 GdkEventKey	*event,
					 gpointer	*user_data)
{
  return on_user_key_press (widget, event, user_data)
    || on_channel_key_press (widget, event, user_data);
}

static gint hide_pointer_tid = -1;
static gint cursor = GDK_LEFT_PTR;
#define HIDE_TIMEOUT /*ms*/ 1500
static gint
hide_pointer_timeout	(GtkWidget	*window)
{
  if (main_info->current_mode != TVENG_NO_CAPTURE) /* TTX mode */
    if (cursor)
      {
	z_set_cursor(window->window, 0);
	cursor = 0;
      }

  hide_pointer_tid = -1;
  return FALSE;
}

static gboolean
on_da_motion_notify			(GtkWidget	*widget,
					 GdkEventMotion	*motion,
					 gpointer	ignored)
{
  if (main_info->current_mode == TVENG_NO_CAPTURE) /* TTX mode */
    return FALSE;

  if (hide_pointer_tid>=0)
    gtk_timeout_remove(hide_pointer_tid);

  if (!cursor)
    z_set_cursor(widget->window, GDK_LEFT_PTR);
  cursor = GDK_LEFT_PTR;

  hide_pointer_tid = gtk_timeout_add(HIDE_TIMEOUT,
				     (GtkFunction)hide_pointer_timeout,
				     widget);

  return FALSE;
}

/* Start VBI services, and warn if we cannot */
static void
startup_teletext(void)
{
#ifdef HAVE_LIBZVBI
  startup_zvbi();

  if (disable_vbi)
    zconf_set_boolean(FALSE, "/zapping/options/vbi/enable_vbi");

  /* Make the vbi module open the device */
  D();
  zconf_touch("/zapping/options/vbi/enable_vbi");
  D();
#else
  zconf_set_boolean(FALSE, "/zapping/options/vbi/enable_vbi");
  vbi_gui_sensitive(FALSE);
#endif
}

/*
  Called 0.5s after the main window is created, should solve all the
  problems with geometry restoring.
*/
static
gint resize_timeout		( gpointer ignored )
{
  gint x, y, w, h; /* Saved geometry */

  zconf_get_integer(&x, "/zapping/internal/callbacks/x");
  zconf_get_integer(&y, "/zapping/internal/callbacks/y");
  zconf_get_integer(&w, "/zapping/internal/callbacks/w");
  zconf_get_integer(&h, "/zapping/internal/callbacks/h");
  printv("Restoring geometry: <%d,%d> <%d x %d>\n", x, y, w, h);
  gdk_window_move_resize(main_window->window, x, y, w, h);

  return FALSE;
}

extern int zapzilla_main(int argc, char * argv[]);

int main(int argc, char * argv[])
{
  GtkWidget * tv_screen;
  GList * p;
  gint x_bpp = -1;
  gint dword_align = FALSE;
  gint disable_zsfb = FALSE;
  gint disable_plugins = FALSE;
  char *video_device = NULL;
  char *command = NULL;
  char *yuv_format = NULL;
  gboolean xv_detected;
  gboolean unmutable = FALSE;
  /* Some other common options in case the standard one fails */
  char *fallback_devices[] =
  {
    "/dev/video",
    "/dev/video0",
    "/dev/v4l/video0",
    "/dev/v4l/video",
    "/dev/video1",
    "/dev/video2",
    "/dev/video3",
    "/dev/v4l/video1",
    "/dev/v4l/video2",
    "/dev/v4l/video3"
  };
  gint num_fallbacks = sizeof(fallback_devices)/sizeof(char*);

  const struct poptOption options[] = {
    {
      "device",
      0,
      POPT_ARG_STRING,
      &video_device,
      0,
      N_("Video device to use"),
      N_("DEVICE")
    },
    {
      "no-plugins",
      'p',
      POPT_ARG_NONE,
      &disable_plugins,
      0,
      N_("Disable plugins support"),
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
      "no-xv",
      'v',
      POPT_ARG_NONE,
      &disable_xv,
      0,
      N_("Disable XVideo extension support"),
      NULL
    },
    {
      "no-zsfb",
      'z',
      POPT_ARG_NONE,
      &disable_zsfb,
      0,
      N_("Do not call zapping_setup_fb on startup"),
      NULL
    },
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
      "dword-align",
      0,
      POPT_ARG_NONE,
      &dword_align,
      0,
      N_("Force dword aligning of the overlay window"),
      NULL
    },
    {
      "command",
      'c',
      POPT_ARG_STRING,
      &command,
      0,
      N_("Execute the given command and exit"),
      N_("CMD")
    },
    {
      "yuv-format",
      'y',
      POPT_ARG_STRING,
      &yuv_format,
      0,
      N_("Pixformat for XVideo capture mode [YUYV | YVU420]"),
      N_("PIXFORMAT")
    },
    {
      NULL,
    } /* end the list */
  };

  if (strlen(argv[0]) >= strlen("zapzilla") &&
      !(strcmp(&argv[0][strlen(argv[0])-strlen("zapzilla")], "zapzilla")))
#ifdef HAVE_LIBZVBI
    return zapzilla_main(argc, argv);
#else
    return EXIT_FAILURE;
#endif

#ifdef ENABLE_NLS
  bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);
#endif

  gnome_program_init (PACKAGE, VERSION, LIBGNOMEUI_MODULE,
                      argc, argv,
                      GNOME_PARAM_APP_DATADIR, PACKAGE_DATA_DIR,
                      GNOME_PARAM_POPT_TABLE, options,
		      NULL);

#ifndef HAVE_PROGRAM_INVOCATION_NAME
  program_invocation_name = argv[0];
  program_invocation_short_name = g_get_prgname();
#endif

  if (x11_get_bpp() < 15)
    {
      RunBox("The current depth (%i bpp) isn't supported by Zapping",
	     GTK_MESSAGE_ERROR, x11_get_bpp());
      return 0;
    }

  printv("%s\n%s %s, build date: %s\n",
	 "$Id: main.c,v 1.165.2.1 2002-07-19 20:53:47 garetxe Exp $",
	 "Zapping", VERSION, __DATE__);
  printv("Checking for CPU... ");
  switch (cpu_detection())
    {
    case CPU_PENTIUM_MMX:
    case CPU_PENTIUM_II:
      printv("Intel Pentium MMX / Pentium II. MMX support enabled.\n");
      break;

    case CPU_PENTIUM_III:
    case CPU_PENTIUM_4:
      printv("Intel Pentium III / Pentium 4. SSE support enabled.\n");
      break;

    case CPU_K6_2:
      printv("AMD K6-2 / K6-III. 3DNow support enabled.\n");
      break;

    case CPU_CYRIX_MII:
    case CPU_CYRIX_III:
      printv("Cyrix MII / Cyrix III. MMX support enabled.\n");
      break;

    case CPU_ATHLON:
      printv("AMD Athlon. MMX/3DNow/SSE support enabled.\n");
      break;

    default:
      printv("unknow type. Using plain C.\n");
      break;
    }
  D();
  /* init the conversion routines */
  yuv2rgb_init(x11_get_bpp(),
	       (x11_get_byte_order()==GDK_MSB_FIRST)?MODE_BGR:MODE_RGB);
  yuyv2rgb_init(x11_get_bpp(),
	       (x11_get_byte_order()==GDK_MSB_FIRST)?MODE_BGR:MODE_RGB);

  D();
  glade_gnome_init();
  D();
  gnome_window_icon_set_default_from_file(PACKAGE_PIXMAPS_DIR "/gnome-television.png");
  D();
  if (!g_module_supported ())
    {
      RunBox(_("Sorry, but there is no module support in GLib"),
	     GTK_MESSAGE_ERROR);
      return 0;
    }
  D();
  have_wmhooks = (wm_detect () != -1);
  D();
  main_info = tveng_device_info_new( GDK_DISPLAY(), x_bpp);
  if (!main_info)
    {
      g_error(_("Cannot get device info struct"));
      return -1;
    }
  tveng_set_debug_level(debug_msg, main_info);
  tveng_set_xv_support(disable_xv, main_info);
  tveng_set_dword_align(dword_align, main_info);
  D();
  if (!startup_zapping(!disable_plugins))
    {
      RunBox(_("Zapping couldn't be started"), GTK_MESSAGE_ERROR);
      tveng_device_info_destroy(main_info);
      return 0;
    }
  D();

  if (yuv_format)
    {
      if (!strcasecmp(yuv_format, "YUYV"))
	zcs_int(TVENG_PIX_YUYV, "yuv_format");
      else if (!strcasecmp(yuv_format, "YVU420"))
	zcs_int(TVENG_PIX_YVU420, "yuv_format");
      else
	g_warning("Unknown pixformat %s: Must be one of (YUYV | YVU420)\n"
		  "The current format is %s",
		  yuv_format, zcg_int(NULL, "yuv_format") ==
		  TVENG_PIX_YUYV ? "YUYV" : "YVU420");
    }

  D();

  if (video_device)
    zcs_char(video_device, "video_device");

  if (!debug_msg)
    tveng_set_zapping_setup_fb_verbosity(zcg_int(NULL,
						 "zapping_setup_fb_verbosity"),
					 main_info);
  else
    tveng_set_zapping_setup_fb_verbosity(3, main_info);

  main_info -> file_name = strdup(zcg_char(NULL, "video_device"));

  if (!main_info -> file_name)
    {
      perror("strdup");
      return 1;
    }
  D();

  xv_detected = tveng_detect_xv_overlay (main_info);
  printv("XV overlay detection: %s\n", xv_detected ? "OK" : "Failed");

  /* try to run the auxiliary suid program */
  if (!disable_zsfb &&
      !xv_detected &&
      tveng_run_zapping_setup_fb(main_info) == -1)
    g_message("Error while executing zapping_setup_fb,\n"
	      "Previewing might not work:\n%s", main_info->error);
  D();
  free(main_info -> file_name);

  D();

  if (tveng_attach_device(zcg_char(NULL, "video_device"),
			  TVENG_ATTACH_XV,
			  main_info) == -1)
    {
      GtkWidget * question_box;
      gint i;

      question_box =
	gtk_message_dialog_new(NULL,
			       GTK_DIALOG_DESTROY_WITH_PARENT |
			       GTK_DIALOG_MODAL,
			       GTK_MESSAGE_QUESTION,
			       GTK_BUTTONS_YES_NO,
			       _("Couldn't open %s, should I try "
				 "some common options?"),
			       zcg_char(NULL, "video_device"));
#if 0
      /* Destroy the dialog when the user responds to it */
      /* (e.g. clicks a button) */
      g_signal_connect_swapped (G_OBJECT (question_box), "response",
				G_CALLBACK (gtk_widget_destroy),
				question_box);
#endif

      gtk_dialog_set_default_response (GTK_DIALOG (question_box),
				       GTK_RESPONSE_YES);

      if (gtk_dialog_run(GTK_DIALOG(question_box)) == GTK_RESPONSE_YES)
	{ /* retry */
	  for (i = 0; i<num_fallbacks; i++)
	    {
	      printf("trying device: %s\n", fallback_devices[i]);
	      main_info -> file_name = strdup(fallback_devices[i]);
  
	      D();
	      /* try to run the auxiliary suid program */
	      if (tveng_run_zapping_setup_fb(main_info) == -1)
		g_message("Error while executing zapping_setup_fb,\n"
			  "Previewing might not work:\n%s", main_info->error);
	      D();
	      free(main_info -> file_name);

	      if (tveng_attach_device(fallback_devices[i],
				      TVENG_ATTACH_XV,
				      main_info) != -1)
		{
		  zcs_char(fallback_devices[i], "video_device");
		  ShowBox(_("%s suceeded, setting it as the new default"),
			  GTK_MESSAGE_INFO,
			  fallback_devices[i]);
		  goto device_ok;
		}
	    }
	}

      RunBox(_("Sorry, but \"%s\" could not be opened:\n%s"),
	     GTK_MESSAGE_ERROR, zcg_char(NULL, "video_device"),
	     main_info->error);

      return -1;
    }

 device_ok:
  if (main_info->current_controller == TVENG_CONTROLLER_XV)
    xv_present = TRUE;

  D();
  /* mute the device while we are starting up */
  if (tveng_set_mute(1, main_info) < 0)
    unmutable = TRUE;
  D();
  z_tooltips_active (zconf_get_boolean
		     (NULL, "/zapping/options/main/show_tooltips"));
  D();
  main_window = create_zapping();
  D();
  tv_screen = lookup_widget(main_window, "tv_screen");
  /* Avoid dumb resizes to 1 pixel height */
  g_signal_connect(G_OBJECT(tv_screen), "size-allocate",
		     G_CALLBACK(on_tv_screen_size_allocate),
		     NULL);
  g_signal_connect(G_OBJECT(main_window),
		     "key-press-event",
		     G_CALLBACK(on_zapping_key_press), NULL);
  /* set periodically the geometry flags on the main window */
  gtk_timeout_add(100, (GtkFunction)timeout_handler, NULL);
  /* ensure that the main window is realized */
  gtk_widget_realize(main_window);
  gtk_widget_realize(tv_screen);
  while (!tv_screen->window)
    z_update_gui();
  D();
  g_signal_connect(G_OBJECT(tv_screen), "motion-notify-event",
		     G_CALLBACK(on_da_motion_notify), NULL);
  hide_pointer_tid = gtk_timeout_add(HIDE_TIMEOUT,
				     (GtkFunction)hide_pointer_timeout,
				     tv_screen);

  D();
  window_on_top (main_window, zconf_get_boolean
		 (NULL, "/zapping/options/main/keep_on_top"));
  D();
  if (unmutable)
    {
      /* has no mute function */
      gtk_widget_hide(lookup_widget(main_window, "tb-mute"));
      D();
    }
  D();
  if (!startup_capture(tv_screen))
    {
      RunBox("The capture couldn't be started", GTK_MESSAGE_ERROR);
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
  D();
  startup_teletext();
  D();
#ifdef HAVE_LIBZVBI
  startup_ttxview();
#endif
  D();
  startup_osd();
  D();
  startup_audio();
  D();
  startup_keyboard();
  D();
  startup_csconvert();
  D();
  startup_properties_handler ();
  D();
  startup_plugin_properties ();
  D();
  osd_set_window(tv_screen);
#ifdef HAVE_LIBZVBI
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM
       (lookup_widget(main_window, "closed_caption1")),
       zconf_get_boolean(NULL, "/zapping/internal/callbacks/closed_caption"));
  D();
#endif
  printv("switching to mode %d (%d)\n", zcg_int(NULL,
						"capture_mode"),
	 TVENG_CAPTURE_READ);
  /* Start the capture in the last mode */
  if (!disable_preview)
    {
      if (zmisc_switch_mode((enum tveng_capture_mode)
			    zcg_int(NULL, "capture_mode"), main_info)
	  == -1)
	{
	  ShowBox(_("Cannot restore previous mode%s:\n%s"),
		  GTK_MESSAGE_ERROR,
		  (zcg_int(NULL, "capture_mode") == TVENG_CAPTURE_READ) ? ""
		  : _(", I will try starting capture mode"),
		  main_info->error);
	  if ((zcg_int(NULL, "capture_mode") != TVENG_CAPTURE_READ) &&
	      (zmisc_switch_mode(TVENG_CAPTURE_READ, main_info) == -1))
	    ShowBox(_("Capture mode couldn't be started either:\n%s"),
		    GTK_MESSAGE_ERROR, main_info->error);
	}
      else
	{
	  /* in callbacks.c */
	  extern enum tveng_capture_mode last_mode;

	  last_mode = TVENG_CAPTURE_WINDOW;
	}
    }
  else /* preview disabled */
      if (zmisc_switch_mode(TVENG_CAPTURE_READ, main_info) == -1)
	ShowBox(_("Capture mode couldn't be started:\n%s"),
		GTK_MESSAGE_ERROR, main_info->error);
  D();
  if (-1 == tveng_set_mute(zcg_bool(NULL, "start_muted"), main_info))
    printv("%s\n", main_info->error);
  else
    set_mute1(3, TRUE, FALSE);
  D();
  /* Restore the input and the standard */
  if (zcg_int(NULL, "current_input"))
    z_switch_input(zcg_int(NULL, "current_input"), main_info);
  if (zcg_int(NULL, "current_standard"))
    z_switch_standard(zcg_int(NULL, "current_standard"), main_info);
  /* FIXME: Figure out what to do regarding callbacks.c */
  cur_tuned_channel = zcg_int(NULL, "cur_tuned_channel");
  z_switch_channel(tveng_retrieve_tuned_channel_by_index(cur_tuned_channel,
							 global_channel_list),
		   main_info);
  if (!command)
    {
      gtk_widget_show(main_window);
      resize_timeout(NULL);
      /* hide toolbars and co. if necessary */
      if (zconf_get_boolean(NULL, "/zapping/internal/callbacks/hide_controls"))
	cmd_run ("zapping.hide_controls(1)");
#ifdef HAVE_LIBZVBI
      /* setup subtitles page button */
      zconf_get_integer(&zvbi_page,
			"/zapping/internal/callbacks/zvbi_page");
      D();
#endif
      /* Sets the coords to the previous values, if the users wants to */
      if (zcg_bool(NULL, "keep_geometry"))
	gtk_timeout_add(500, resize_timeout, NULL);
      D(); printv("going into main loop...\n");
      gtk_main();
    }
  else
    {
      D(); printv("running command \"%s\"\n", command);
      cmd_run(command);
    }
  /* Closes all fd's, writes the config to HD, and that kind of things
   */
  shutdown_zapping();
  return 0;
}

static void shutdown_zapping(void)
{
  int i = 0, j = 0;
  gchar * buffer = NULL;
  tveng_tuned_channel * channel;

  printv("Shutting down the beast:\n");

  if (was_fullscreen)
    zcs_int(TVENG_CAPTURE_PREVIEW, "capture_mode");

  /* Unloads all plugins, this tells them to save their config too */
  printv("plugins");
  plugin_unload_plugins(plugin_list);
  plugin_list = NULL;

/* Temporarily moved up here for a test */
#ifdef HAVE_LIBZVBI
  /*
   * Shuts down the teletext view
   */
  printv(" ttxview");
  shutdown_ttxview();
  /* Shut down vbi */
  printv(" vbi\n");
  shutdown_zvbi();
#endif

  /* Write the currently tuned channels */
  printv(" channels");
  zconf_delete(ZCONF_DOMAIN "tuned_channels");
  while ((channel = tveng_retrieve_tuned_channel_by_index(i,
							  global_channel_list))
	 != NULL)
    {
      if ((i == cur_tuned_channel) &&
	  !ChannelWindow) /* Having the channel editor open screws this
			   logic up, do not save controls in this case */
	{
	  g_free(channel->controls);
	  store_control_values(&channel->num_controls,
			       &channel->controls, main_info);
	}

#define SAVE_CONFIG(_type, _name, _cname, _descr)			\
  buffer = g_strdup_printf (ZCONF_DOMAIN "tuned_channels/%d/" #_cname, i); \
  zconf_create_##_type (channel-> _name, _descr, buffer);		\
  g_free (buffer);

      SAVE_CONFIG (string,  name,         name,         "Station name");
      SAVE_CONFIG (integer, freq,         freq,         "Tuning frequency");
      SAVE_CONFIG (z_key,   accel,        accel,        "Accelerator key");
      /* historic "real_name", changed to less confusing rf_name */
      SAVE_CONFIG (string,  rf_name,      real_name,    "RF channel name");
      SAVE_CONFIG (string,  country,      country,      "Country the channel is in");
      SAVE_CONFIG (integer, input,        input,        "Attached input");
      SAVE_CONFIG (integer, standard,     standard,     "Attached standard");
      SAVE_CONFIG (integer, num_controls, num_controls, "Saved controls");

      for (j = 0; j<channel->num_controls; j++)
	{
	  buffer = g_strdup_printf(ZCONF_DOMAIN
				   "tuned_channels/%d/controls/%d/name",
				   i, j);
	  zconf_create_string (channel->controls[j].name, "Control name", buffer);
	  g_free(buffer);
	  buffer = g_strdup_printf(ZCONF_DOMAIN
				   "tuned_channels/%d/controls/%d/value",
				   i, j);
	  zconf_create_float(channel->controls[j].value, "Control value", buffer);
	  g_free(buffer);
	}

      i++;
    }

  global_channel_list = tveng_clear_tuned_channel(global_channel_list);

  if (current_country)
    zcs_char(current_country -> name, "current_country");

  if (main_info->num_standards)
    zcs_int(main_info->standards[main_info -> cur_standard].hash,
	    "current_standard");
  else
    zcs_int(0, "current_standard");

  if (main_info->num_inputs)
    zcs_int(main_info->inputs[main_info->cur_input].hash,
	    "current_input");
  else
    zcs_int(0, "current_input");

  tveng_set_mute(1, main_info);

#if GNOME2_PORT_COMPLETE
  /* Shutdown all other modules */
  printv(" callbacks");
  shutdown_callbacks();
#endif

  /* inputs, standards handling */
  printv("\n v4linterface");
  shutdown_v4linterface();

  /*
   * Tell the overlay engine to shut down and to do a cleanup if necessary
   */
  printv(" overlay");
  shutdown_overlay();

  /*
   * Shuts down the OSD info
   */
  printv(" osd");
  shutdown_osd();

  /*
   * Shuts down the capture engine
   */
  printv(" capture");
  shutdown_capture();

  /*
   * Keyboard
   */
  printv(" kbd");
  shutdown_keyboard();

  /*
   * The audio config
   */
  printv(" audio");
  shutdown_audio();

  /*
   * The video output backends.
   */
  printv(" xvz");
  shutdown_xvz();

  /*
   * The colorspace conversions.
   */
  printv(" csconvert");
  shutdown_csconvert();

  /*
   * The mixer code.
   */
  printv(" mixer");
  shutdown_mixer();

  /*
   * The plugin properties dialog.
   */
  printv(" pp");
  shutdown_plugin_properties();

  /*
   * The properties handler.
   */
  printv(" ph");
  shutdown_properties_handler();
  shutdown_properties();
   
  /* Close */
  printv(" video device");
  tveng_device_info_destroy(main_info);

  /* Save the config and show an error if something failed */
  printv(" config");
  if (!zconf_close())
    ShowBox(_("ZConf could not be closed properly , your\n"
	      "configuration will be lost.\n"
	      "Possible causes for this are:\n"
	      "   - There is not enough free memory\n"
	      "   - You do not have permissions to write to $HOME/.zapping\n"
	      "   - libxml is non-functional (?)\n"
	      "   - or, more probably, you have found a bug in\n"
	      "     %s. Please contact the author.\n"
	      ), GTK_MESSAGE_ERROR, "Zapping");

  printv(" cmd");
  shutdown_cmd ();
  shutdown_remote();

  printv(".\nShutdown complete, goodbye.\n");
}

static gboolean startup_zapping(gboolean load_plugins)
{
  int i = 0, j;
  gchar * buffer = NULL;
  gchar * buffer2 = NULL;
  gchar * buffer3 = NULL;
  gchar * buffer4 = NULL;
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
  startup_remote ();
  startup_cmd ();
#if GNOME2_PORT_COMPLETE
  cmd_register ("subtitle_overlay", subtitle_overlay_cmd, NULL);
#endif
  startup_properties();
  D();

  /* Sets defaults for zconf */
  zcc_bool(TRUE, "Save and restore zapping geometry (non ICCM compliant)", 
	   "keep_geometry");
  zcc_bool(FALSE, "Keep main window on top", "keep_on_top");
  zcc_bool(TRUE, "Show tooltips", "show_tooltips");
  zcc_bool(TRUE, "Resize by fixed increments", "fixed_increments");
  zcc_char(tveng_get_country_tune_by_id(0)->name,
	     "The country you are currently in", "current_country");
  current_country = 
    tveng_get_country_tune_by_name(zcg_char(NULL, "current_country"));
  if (!current_country)
    current_country = tveng_get_country_tune_by_id(0);
  zcc_char("/dev/video0", "The device file to open on startup",
	   "video_device");
  zcc_bool(FALSE, "TRUE if Zapping should be started without sound",
	   "start_muted");
  zcc_bool(TRUE, "TRUE if some flickering should be avoided in preview mode",
	   "avoid_flicker");
  zcc_bool(FALSE, "TRUE if the controls info should be saved with each "
	   "channel", "save_controls");
  zcc_int(0, "Verbosity value given to zapping_setup_fb",
	  "zapping_setup_fb_verbosity");
  zcc_int(0, "Ratio mode", "ratio");
  zcc_int(0, "Change the video mode when going fullscreen", "change_mode");
  zcc_int(0, "Current standard", "current_standard");
  zcc_int(0, "Current input", "current_input");
  zcc_int(TVENG_CAPTURE_WINDOW, "Current capture mode", "capture_mode");
  zcc_int(TVENG_CAPTURE_WINDOW, "Previous capture mode", "previous_mode");
  zcc_int(TVENG_PIX_YUYV, "Pixformat used with XVideo capture",
	  "yuv_format");
  zcc_bool(FALSE, "In videotext mode", "videotext_mode");
  zconf_create_boolean(FALSE, "Hide controls",
		       "/zapping/internal/callbacks/hide_controls");
  D();
  /* Loads all the tuned channels */
  while (zconf_get_nth(i, &buffer, ZCONF_DOMAIN "tuned_channels") !=
	 NULL)
    {
      g_assert(strlen(buffer) > 0);

      memset(&new_channel, 0, sizeof(new_channel));

      if (buffer[strlen(buffer)-1] == '/')
	buffer[strlen(buffer)-1] = 0;

      /* Get all the items from here  */

#define LOAD_CONFIG(_type, _name, _cname)				\
  buffer2 = g_strconcat (buffer, "/" #_cname, NULL);			\
  zconf_get_##_type (&new_channel. _name, buffer2);			\
  g_free (buffer2);

      LOAD_CONFIG (string,  name,         name);
      LOAD_CONFIG (string,  rf_name,      real_name);
      LOAD_CONFIG (integer, freq,         freq);
      LOAD_CONFIG (z_key,   accel,        accel);
      LOAD_CONFIG (string,  country,      country);
      LOAD_CONFIG (integer, input,        input);
      LOAD_CONFIG (integer, standard,     standard);
      LOAD_CONFIG (integer, num_controls, num_controls);

      buffer2 = g_strconcat(buffer, "/controls", NULL);
      if (new_channel.num_controls)
	new_channel.controls = (tveng_tc_control *)
	  g_malloc0(sizeof(tveng_tc_control)
		    * new_channel.num_controls);
      for (j = 0; j<new_channel.num_controls; j++)
	{
	  if (!zconf_get_nth(j, &buffer3, buffer2))
	    {
	      g_warning("Control %d of channel %d [%s] is malformed, skipping",
			j, i, new_channel.name);
	      continue;
	    }
	  buffer4 = g_strconcat(buffer3, "/name", NULL);
	  strncpy(new_channel.controls[j].name,
		  zconf_get_string(NULL, buffer4), 32);
	  g_free(buffer4);
	  buffer4 = g_strconcat(buffer3, "/value", NULL);
	  zconf_get_float(&new_channel.controls[j].value, buffer4);
	  g_free(buffer4);
	  g_free(buffer3);
	}
      g_free(buffer2);

      new_channel.index = 0;
      global_channel_list =
	tveng_append_tuned_channel(&new_channel, global_channel_list);

      /* Free the previously allocated mem */
      g_free(new_channel.name);
      g_free(new_channel.rf_name);
      g_free(new_channel.country);
      g_free(new_channel.controls);

      g_free(buffer);
      i++;
    }
  D();
  /* Starts all modules */
  startup_v4linterface(main_info);
  D();
  startup_mixer();
  D();
  startup_xvz();
  D();
#if GNOME2_PORT_COMPLETE
  if (!startup_callbacks())
    return FALSE;
#endif
  D();
  /* Loads the plugins */
  if (load_plugins)
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
