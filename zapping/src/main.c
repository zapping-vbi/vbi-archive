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
#  include "config.h"
#endif

#include <gnome.h>
#include <gdk/gdkx.h>
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
#include "libtv/cpu.h"
#include "overlay.h"
#include "capture.h"
#include "x11stuff.h"
#include "zimage.h"
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
#include "channel_editor.h"
#include "i18n.h"
#include "vdr.h"
#include "xawtv.h"
#include "subtitle.h"

#ifndef HAVE_PROGRAM_INVOCATION_NAME
char *program_invocation_name;
char *program_invocation_short_name;
#endif

Zapping *		zapping;

static GnomeClient *	session;

/*** END OF GLOBAL STUFF ***/

void shutdown_zapping(void);
static gboolean startup_zapping(gboolean load_plugins,
				tveng_device_info *info);

gboolean
on_zapping_key_press			(GtkWidget	*widget,
					 GdkEventKey	*event,
					 gpointer	*user_data);
gboolean
on_zapping_key_press			(GtkWidget	*widget,
					 GdkEventKey	*event,
					 gpointer	*user_data)
{
  return on_channel_enter (widget, event, user_data)
    || on_user_key_press (widget, event, user_data)
    || on_picture_size_key_press (widget, event, user_data)
    || on_channel_key_press (widget, event, user_data);
}


/* Start VBI services, and warn if we cannot */
static void
startup_teletext(void)
{
#ifdef HAVE_LIBZVBI
  if (NULL == _teletext_view_new /* have Teletext plugin */)
    gtk_action_group_set_visible (zapping->teletext_action_group, FALSE);
  if (NULL == _subtitle_view_new /* have Subtitle plugin */)
    gtk_action_group_set_visible (zapping->subtitle_action_group, FALSE);

  D();

  /* XXX still useful for channel names et al. */
  if (_teletext_view_new || _subtitle_view_new)
    {
      /* Error ignored. */
      zvbi_start ();
      D();
    }
#else
  disable_vbi = TRUE;

  gtk_action_group_set_visible (zapping->teletext_action_group, FALSE);
  gtk_action_group_set_visible (zapping->subtitle_action_group, FALSE);

  printv ("VBI disabled, removing GUI items\n");
      
  /* Set the capture mode to a default value and disable VBI */
  if (zcg_int (NULL, "capture_mode") == OLD_TVENG_TELETEXT)
    zcs_int (OLD_TVENG_CAPTURE_READ, "capture_mode");
#endif
}

static gboolean
session_save			(GnomeClient *		client,
				 gint			phase,
				 GnomeSaveStyle		save_style,
				 gboolean		shutting_down,
				 GnomeInteractStyle	interact_style,
				 gboolean		fast,
				 gpointer		user_data)
{
  GList *p;
  gchar **argv;
  guint argc;
  gboolean success;

  phase = phase;
  save_style = save_style;
  shutting_down = shutting_down;
  interact_style = interact_style;
  fast = fast;

  argv = g_malloc0 (4 * sizeof (gchar *));
  argc = 0;

  argv[argc++] = user_data; /* main() argv[0] */

  gnome_client_set_clone_command (client, argc, argv);
  gnome_client_set_restart_command (client, argc, argv);

  success = TRUE;

  for (p = g_list_first (plugin_list); p; p = p->next)
    {
      struct plugin_info *pi;

      /* Shutdown while recording etc no good. */ 
      pi = (struct plugin_info *) p->data;
      success &= plugin_running (pi);
    }

  /* libgnomeui 2.8 bug: ignores return value. */
  client->save_successfull = success;

  return success;
}

static void
session_die			(GnomeClient *		client,
				 gpointer		user_data)
{
  client = client;
  user_data = user_data;

  on_python_command1 (GTK_WIDGET (zapping), "zapping.quit()");
}

#include "pixmaps/brightness.h"
#include "pixmaps/contrast.h"
#include "pixmaps/saturation.h"
#include "pixmaps/hue.h"
#include "pixmaps/recordtb.h"
#include "pixmaps/mute.h"
#include "pixmaps/teletext.h"
#include "pixmaps/subtitle.h"
#include "pixmaps/video.h"
#include "pixmaps/screenshot.h"

#define ADD_STOCK(name)							\
  item.stock_id = "zapping-" #name;					\
  gtk_stock_add (&item, 1);						\
  z_icon_factory_add_pixdata (item.stock_id, & name ## _png);

static void
init_zapping_stock		(void)
{
  static const GtkStockItem items [] = {
    { "zapping-mute",	  N_("Mute"),	   0, 0, NULL },
    { "zapping-teletext", N_("Teletext"),  0, 0, NULL },
    { "zapping-subtitle", N_("Subtitles"), 0, 0, NULL },
    { "zapping-video",	  N_("Video"),	   0, 0, NULL },
  };
  GtkStockItem item;

  CLEAR (item);

  ADD_STOCK (brightness);
  ADD_STOCK (contrast);
  ADD_STOCK (saturation);
  ADD_STOCK (hue);
  ADD_STOCK (recordtb);
  ADD_STOCK (screenshot);

  gtk_stock_add (items, G_N_ELEMENTS (items));

  z_icon_factory_add_pixdata ("zapping-mute", &mute_png);
  z_icon_factory_add_pixdata ("zapping-teletext", &teletext_png);
  z_icon_factory_add_pixdata ("zapping-subtitle", &subtitle_png);
  z_icon_factory_add_pixdata ("zapping-video", &video_png);
}

static void
restore_controls		(void)
{
  tveng_tuned_channel *tc;
  gboolean start_muted;

  D();

  tc = tveng_tuned_channel_new (/* copy of */ NULL);

  /* Error ignored. */
  zconf_get_controls (tc, "/zapping/options/main");

  start_muted = zcg_bool (NULL, "start_muted");

  if (start_muted)
    {
      tveng_tc_control *mute;

      if ((mute = tveng_tc_control_by_id (zapping->info,
					  tc->controls, tc->num_controls,
					  TV_CONTROL_ID_MUTE)))
	mute->value = 1;
    }

  load_control_values (zapping->info, tc->controls, tc->num_controls);

  tveng_tuned_channel_delete (tc);
  tc = NULL;

  set_mute (3 /* update */, /* controls */ TRUE, /* osd */ FALSE);

  D();

  /* Restore the input and the standard */

  /* XXX make this optional */

  if (1)
    zconf_get_sources (zapping->info, start_muted);
}

static void
restore_last_capture_mode		(void)
{
  /* Start the capture in the last mode */

  if (disable_overlay)
    {
      if (-1 == zmisc_switch_mode (DISPLAY_MODE_WINDOW,
				   CAPTURE_MODE_READ,
				   zapping->info, /* warnings */ FALSE))
	ShowBox (_("Capture mode couldn't be started:\n%s"),
		 GTK_MESSAGE_ERROR, tv_get_errstr (zapping->info));
    }
  else
    {
      gint cap_mode;
      display_mode dmode;
      capture_mode cmode;

      cap_mode = zcg_int (NULL, "capture_mode");
      from_old_tveng_capture_mode (&dmode, &cmode,
				   (enum old_tveng_capture_mode) cap_mode);

      if (-1 == zmisc_switch_mode (dmode, cmode, zapping->info, /* warnings */ FALSE))
	{
	  if (CAPTURE_MODE_READ != cmode)
	    {
	      if (0)
		ShowBox(_("Cannot restore previous mode, will try capture mode:\n%s"),
			GTK_MESSAGE_ERROR, tv_get_errstr (zapping->info));

	      if (-1 == zmisc_switch_mode (DISPLAY_MODE_WINDOW,
					   CAPTURE_MODE_READ, zapping->info,
					   /* warnings */ FALSE))
		{
		  if (0)
		    ShowBox(_("Capture mode couldn't be started either:\n%s"),
			    GTK_MESSAGE_ERROR, tv_get_errstr (zapping->info));
		  else
		    ShowBox(_("Cannot restore previous mode:\n%s"),
			    GTK_MESSAGE_ERROR, tv_get_errstr (zapping->info));
		}
	    }
	  else
	    {
	      ShowBox (_("Capture mode couldn't be started:\n%s"),
		       GTK_MESSAGE_ERROR, tv_get_errstr (zapping->info));
	    }
	}
      else
	{
	  last_dmode = DISPLAY_MODE_WINDOW;
	  last_cmode = CAPTURE_MODE_OVERLAY;
	}
    }
}


extern int zapzilla_main(int argc, char * argv[]);

/* Make sure the version appears in backtraces. */
#define _MAIN(version) main_ ## version
#define MAIN(version) _MAIN (version)

int
MAIN (PACKAGE_VERSION_ID)	(int			argc,
				 char **		argv);

int
MAIN (PACKAGE_VERSION_ID)	(int			argc,
				 char **		argv)
{
  GList * p;
  gint x_bpp = -1;
  gint dword_align = FALSE;
  gint disable_plugins = FALSE;
  gint dummy;
  char *video_device = NULL;
  char *command = NULL;
  char *yuv_format = NULL;
  char *norm = NULL;
  char *cpu_feature_str = NULL;
  gboolean mutable = TRUE;
  const gchar *display_name;
  tveng_device_info *info;
  /* Some other common options in case the standard one fails */
  const gchar *fallback_devices[] =
  {
#ifdef ENABLE_V4L
    "/dev/video",
    "/dev/video0",
    "/dev/v4l/video0",
    "/dev/v4l/video",
    "/dev/video1",
    "/dev/video2",
    "/dev/video3",
    "/dev/v4l/video1",
    "/dev/v4l/video2",
    "/dev/v4l/video3",
#endif
#ifdef ENABLE_BKTR
    "/dev/bktr",
#endif
  };
  gint num_fallbacks = sizeof(fallback_devices)/sizeof(char*);

  const struct poptOption options[] = {
    {
      "device",
      0,
      POPT_ARG_STRING,
      &video_device,
      0,
      N_("Kernel video device"),
      N_("FILENAME")
    },

#ifdef HAVE_XV_EXTENSION

    {
      "xv-video-port",
      0,
      POPT_ARG_INT,
      &xv_video_port,
      0,
      N_("XVideo video input port"),
      NULL
    },
    {
      "xv-image-port",
      0,
      POPT_ARG_INT,
      &xv_image_port,
      0,
      N_("XVideo image overlay port"),
      NULL
    },
    {
      "xv-port", /* for compatibility with Zapping 0.6 */
      0,
      POPT_ARG_INT,
      &xv_video_port,
      0,
      N_("XVideo video input port"),
      NULL
    },
    {
      "no-xv-video",
      0,
      POPT_ARG_NONE,
      &disable_xv_video,
      0,
      N_("Disable XVideo video input support"),
      NULL
    },
    {
      "no-xv-image",
      0,
      POPT_ARG_NONE,
      &disable_xv_image,
      0,
      N_("Disable XVideo image overlay support"),
      NULL
    },
    {
      "no-xv", /* for compatibility with Zapping 0.6 */
      'v',
      POPT_ARG_NONE,
      &disable_xv,
      0,
      N_("Disable XVideo extension support"),
      NULL
    },

#endif /* !HAVE_XV_EXTENSION */

    {
      "no-overlay",
      0,
      POPT_ARG_NONE,
      &disable_overlay,
      0,
      N_("Disable video overlay"),
      NULL
    },
    {
      "remote",
      0,
      POPT_ARG_NONE,
      &disable_overlay,
      0,
      N_("X display is remote, disable video overlay"),
      NULL
    },
    {
      "no-vbi",
      'i',
      POPT_ARG_NONE,
      &disable_vbi,
      0,
      N_("Disable VBI support"),
      NULL
    },
    {
      "no-plugins",
      'p',
      POPT_ARG_NONE,
      &disable_plugins,
      0,
      N_("Disable plugins"),
      NULL
    },
    {
      /* We used to call zapping_setup_fb on startup unless this
	 option was given. Now it is only called if necessary
	 before enabling V4L overlay. So the option is no longer
         used, but kept for compatibility. */
      "no-zsfb",
      'z',
      POPT_ARG_NONE,
      &dummy,
      0,
      /* TRANSLATORS: --no-zsfb command line option. */
      N_("Obsolete"),
      NULL
    },
    {
      "esd-out",
      0,
      POPT_ARG_NONE,
      &esd_output,
      0,
      "Copy recorded sound to sound daemon",
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
      N_("Print debug messages"),
      NULL
    },
    {
      "io-debug",
      0,
      POPT_ARG_NONE,
      &io_debug_msg,
      0,
      0, /* N_("Log driver accesses"), */
      NULL
    },
    {
      "dword-align",
      0,
      POPT_ARG_NONE,
      &dword_align,
      0,
      N_("Force dword alignment of the overlay window"),
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
      /* TRANSLATORS: --yuv-format command line option. */
      N_("Obsolete"),
      NULL
    },
    {
      "tunerless-norm",
      'n',
      POPT_ARG_STRING,
      &norm,
      0,
      /* TRANSLATORS: --tunerless-norm command line option. */
      N_("Obsolete"),
      NULL
    },
    {
      "cpu-features",
      0,
      POPT_ARG_STRING,
      &cpu_feature_str,
      0,
      N_("Override CPU detection"),
      NULL
    },
    {
      NULL,
      0,
      0,
      NULL,
      0,
      NULL,
      NULL
    } /* end the list */
  };

#if 0 /* L8ER */
  if (strlen(argv[0]) >= strlen("zapzilla") &&
      !(strcmp(&argv[0][strlen(argv[0])-strlen("zapzilla")], "zapzilla")))
#ifdef HAVE_LIBZVBI
    return zapzilla_main(argc, argv);
#else
    return EXIT_FAILURE;
#endif
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
		      GNOME_CLIENT_PARAM_SM_CONNECT, TRUE,
		      NULL);

#ifndef HAVE_PROGRAM_INVOCATION_NAME
  program_invocation_name = argv[0];
  program_invocation_short_name = g_get_prgname();
#endif

  session = gnome_master_client ();
  g_assert (NULL != session);

  gconf_client = gconf_client_get_default ();
  g_assert (NULL != gconf_client);

  gconf_client_add_dir (gconf_client, "/apps/zapping",
                        GCONF_CLIENT_PRELOAD_NONE, NULL);

  if (x11_get_bpp() < 15)
    {
      RunBox("The current depth (%i bpp) isn't supported by Zapping",
	     GTK_MESSAGE_ERROR, x11_get_bpp());
      return 0;
    }

  printv("%s\n%s %s, build date: %s\n",
	 "$Id: main.c,v 1.211 2006-03-11 13:15:01 mschimek Exp $",
	 "Zapping", VERSION, __DATE__);

  cpu_detection ();

  if (cpu_feature_str)
    {
      cpu_feature_set actual_features;

      actual_features = cpu_features;
      cpu_features &= cpu_feature_set_from_string (cpu_feature_str);

      printv ("CPU features 0x%x (actual 0x%x)\n",
    	      cpu_features, actual_features);
    }
  else
    {
      printv ("CPU features 0x%x\n", cpu_features);
    }

  D();
  glade_gnome_init();
  D();
  gnome_window_icon_set_default_from_file
    (PACKAGE_PIXMAPS_DIR "/gnome-television.png");
  D();
  init_zapping_stock ();
  D();
  if (!g_module_supported ())
    {
      RunBox(_("Sorry, but there is no module support in GLib"),
	     GTK_MESSAGE_ERROR);
      return 0;
    }
  D();

  if (0 && debug_msg)
    {
      fprintf (stderr, "X -version\n");
      system ("X -version 1>&2");
      fprintf (stderr, "metacity --version\n");
      system ("metacity --version 1>&2");    
    }

  have_wm_hints = wm_hints_detect ();

  switch (x_bpp)
    {
    case -1:
    case 24:
    case 32:
      break;

    default:
      if (debug_msg)
	fprintf (stderr, "Invalid bpp option %d (ignored). Expected "
		       "24 or 32.\n", x_bpp);
      x_bpp = -1;
      break;
    }

#if 0 /* Gtk 2.2 */
  display_name = gdk_display_get_name (gdk_display_get_default ());
#else
  display_name = x11_display_name ();
#endif
  screens = tv_screen_list_new (display_name, x_bpp);

  /* We should have at least on screen, even without Xinerama and DGA.
     The information is needed by fullscreen and overlay code. */
  if (!screens)
    {
      /* XXX localize */
      RunBox(("Cannot find X11 screens"),
	     GTK_MESSAGE_ERROR);
      return 0;
    }

  /* We need the display pixfmt to optimize the capture format.
     This info should be avaialable even without Xinerama and DGA. */
  if (TV_PIXFMT_NONE == screens->target.format.pixel_format->pixfmt)
    {
      /* XXX could use XVideo without. */
      /* XXX localize */
      RunBox(("Cannot determine display pixel format"),
	     GTK_MESSAGE_ERROR);
      return 0;
    }

  D();

  if (debug_msg)
    {
      const tv_screen *xs;

      for (xs = screens; xs; xs = xs->next)
	{
	  fprintf (stderr, "Screen %d:\n"
		   "  position               %u, %u - %u, %u\n"
		   "  frame buffer address   0x%lx\n"
		   "  frame buffer size      %ux%u pixels, 0x%lx bytes\n"
		   "  bytes per line         %lu bytes\n"
		   "  pixfmt                 %s\n",
		   xs->screen_number,
		   xs->x,
		   xs->y,
		   xs->x + xs->width,
		   xs->y + xs->height,
		   xs->target.base,
		   xs->target.format.width,
		   xs->target.format.height,
		   xs->target.format.size,
		   xs->target.format.bytes_per_line[0],
		   xs->target.format.pixel_format->name);
	}
    }

  /* XXX we need a list for preferences but actually each
     x11 screen may have its own set of vidmodes?? */
  vidmodes = x11_vidmode_list_new (NULL, -1);

  if (debug_msg)
    {
      x11_vidmode_info *v;
      unsigned int i = 0;

      fprintf (stderr, "VidModes:");

      for (v = vidmodes; v; v = v->next)
	{
	  fprintf (stderr, "%s%ux%u@%u",
		   (0 == i) ? "\n  " : " ",
		   v->width,
		   v->height,
		   (unsigned int)(v->vfreq + 0.5));
	  i = (i + 1) % 5;
	}

      fputc ('\n', stderr);
    }

  /* Determine size and pixfmt of the Display. */
  if (debug_msg)
    {
      /* If we have the XVideo extension, list adaptors, ports
         and image formats. */
      x11_xvideo_dump ();
    }

  x11_screensaver_init ();

  info = tveng_device_info_new(GDK_DISPLAY (), x_bpp);
  if (!info)
    {
      g_error(_("Cannot get device info struct"));
      return -1;
    }
  tveng_set_debug_level(debug_msg, info);
  tveng_set_xv_support(disable_xv || disable_xv_video, info);
  tveng_set_dword_align(dword_align, info);
  D();
  if (!startup_zapping(!disable_plugins, info))
    {
      RunBox(_("Zapping couldn't be started"), GTK_MESSAGE_ERROR);
      tveng_device_info_destroy(info);
      return 0;
    }
  D();
  if (yuv_format)
    {
      static const unsigned int TVENG_PIX_YVU420 = 6; /* obsolete */
      static const unsigned int TVENG_PIX_YUYV = 8;

      if (0 == strcasecmp (yuv_format, "YUYV"))
	zcs_int ((int) TVENG_PIX_YUYV, "yuv_format");
      else if (0 == strcasecmp (yuv_format, "YVU420"))
	zcs_int ((int) TVENG_PIX_YVU420, "yuv_format");
      else
	g_warning ("Unknown pixformat %s, must be YUYV or YVU420.\n"
		   "The current format is %s.",
		   yuv_format, (zcg_int (NULL, "yuv_format") ==
				(int) TVENG_PIX_YUYV) ? "YUYV" : "YVU420");
    }

  D();

  if (video_device)
    zcs_char(video_device, "video_device");

  if (!debug_msg)
#if 1
    tveng_set_zapping_setup_fb_verbosity(0, info);
#else
    tveng_set_zapping_setup_fb_verbosity(zcg_int(NULL, "zapping_setup_fb_verbosity"),
					 info);
#endif
  else
    tveng_set_zapping_setup_fb_verbosity(3, info);

  tv_set_filename (info, zcg_char(NULL, "video_device"));

  D();

  if (tveng_attach_device(zcg_char(NULL, "video_device"),
			  /* window */ None,
			  TVENG_ATTACH_XV,
			  info) == -1)
    {
      GtkWidget * question_box;
      gint i;

      question_box =
	gtk_message_dialog_new(NULL,
			       GTK_DIALOG_DESTROY_WITH_PARENT |
			       GTK_DIALOG_MODAL,
			       GTK_MESSAGE_QUESTION,
			       GTK_BUTTONS_YES_NO,
			       _("Couldn't open %s, try other devices?"),
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

	      tv_set_filename (info, fallback_devices[i]);
  
	      if (tveng_attach_device(fallback_devices[i],
				      /* window */ None,
				      TVENG_ATTACH_XV,
				      info) != -1)
		{
		  zcs_char(fallback_devices[i], "video_device");
		  ShowBox(_("%s suceeded, setting it as the new default"),
			  GTK_MESSAGE_INFO,
			  fallback_devices[i]);
		  goto device_ok;
		}

	      tv_set_filename (info, NULL);
	    }
	}

      RunBox(_("Sorry, but \"%s\" could not be opened:\n%s"),
	     GTK_MESSAGE_ERROR, zcg_char(NULL, "video_device"),
	     tv_get_errstr (info));

      return -1;
    }

 device_ok:

  if (tv_get_controller (info) == TVENG_CONTROLLER_XV)
    xv_present = TRUE;

  D();
  /* mute the device while we are starting up */
  /* FIXME */
  if (-1 == tv_mute_set (info, TRUE))
    mutable = FALSE;
  D();
  z_tooltips_active (zconf_get_boolean
		     (NULL, "/zapping/options/main/show_tooltips"));
  D();
  zconf_create_boolean (TRUE, NULL, "/zapping/options/main/disable_screensaver");
  x11_screensaver_control (zconf_get_boolean
			   (NULL, "/zapping/options/main/disable_screensaver"));
  D();
  startup_zvbi();
  D();
  startup_subtitle();
  D();
  zapping = ZAPPING (zapping_new ());
    {
      GnomeClientFlags flags;

      flags = gnome_client_get_flags (session);

      /* When started by the session manager we automatically get our
	 previous size and position. */
      if (!(flags & GNOME_CLIENT_RESTARTED))
	{
	  gint width;
	  gint height;

	  zconf_get_int (&width, "/zapping/internal/callbacks/w");
	  zconf_get_int (&height, "/zapping/internal/callbacks/h");

	  gtk_window_set_default_size (GTK_WINDOW (zapping), width, height);
	  D();
	}
    }
  gtk_widget_show(GTK_WIDGET (zapping));
  zapping->info = info;
  D();

  g_signal_connect(G_OBJECT(zapping),
		     "key-press-event",
		     G_CALLBACK(on_zapping_key_press), NULL);

  /* ensure that the main window is realized */
  gtk_widget_realize(GTK_WIDGET (zapping));
  while (!GTK_WIDGET (zapping->video)->window)
    z_update_gui();
  D();

  if (0 && !mutable)
    {
      GtkAction *action;

      /* FIXME the device we open initially might not be mutable,
         but could be later when we switch btw capture and overlay. */
      /* FIXME this can change at runtime, the mute button
         should update just like the controls box. */
      /* has no mute function */
      action = gtk_action_group_get_action (zapping->generic_action_group,
					    "Mute");
      z_action_set_visible (action, FALSE);
      D();
    }
  D();
  startup_capture();
  D();
  /* Add the plugins to the GUI */
  p = g_list_first(plugin_list);
  while (p)
    {
      plugin_add_gui(&zapping->app, (struct plugin_info*)p->data);
      p = p->next;
    }

  /* Disable preview if needed */
  if (disable_overlay)
    {
      GtkAction *action;

      printv("Preview disabled, removing GUI items\n");

      action = gtk_action_group_get_action (zapping->generic_action_group,
					    "Fullscreen");
      z_action_set_sensitive (action, FALSE);
      action = gtk_action_group_get_action (zapping->generic_action_group,
					    "Overlay");
      z_action_set_sensitive (action, FALSE);
    }
  D();
  startup_teletext();
  D();
  startup_vdr();
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
  startup_channel_editor ();
  D();
  osd_set_window(GTK_WIDGET (zapping->video));
  D();
  xawtv_ipc_init (GTK_WIDGET (zapping));
  D();
  mixer_setup ();
  D();
  restore_controls ();
  D();

  if (!command)
    {
      D();
      printv("switching to mode %d (%d)\n",
	     zcg_int (NULL, "capture_mode"), OLD_TVENG_CAPTURE_READ);
      D();
      restore_last_capture_mode ();
      D();
      printv("session manager\n");
      g_signal_connect (session, "save-yourself",
			G_CALLBACK (session_save), argv[0]); 
      g_signal_connect (session, "die",
			G_CALLBACK (session_die), NULL); 
      printv("going into main loop...\n");
      gtk_main();
    }
  else
    {
      printv("running command \"%s\"\n", command);
      python_command (NULL, command);
    }

  /* Closes all fd's, writes the config to HD, and that kind of things
     shutdown_zapping(); moved to zapping.c */

  /* Python. */
  shutdown_remote();

  return 0;
}

int main(int argc, char * argv[])
{
  return MAIN (PACKAGE_VERSION_ID) (argc, argv);
}

void shutdown_zapping(void)
{
  guint i = 0;
  gchar * buffer = NULL;
  tveng_tuned_channel * channel;

  printv("Shutting down the beast:\n");

  if (was_fullscreen)
    zcs_int(OLD_TVENG_CAPTURE_PREVIEW, "capture_mode");

  /* Unloads all plugins, this tells them to save their config too */
  printv("plugins");
  plugin_unload_plugins(plugin_list);
  plugin_list = NULL;

  /* Shut down vbi */

  printv(" subtitles\n");
  shutdown_subtitle();

  printv(" vbi\n");
  shutdown_zvbi();

  {
    tveng_tc_control *controls;
    guint n_controls;

    /* Global controls (preliminary) */

    printv(" controls");

    store_control_values (zapping->info, &controls, &n_controls);
    zconf_delete (ZCONF_DOMAIN "num_controls");
    zconf_delete ("/zapping/options/main/controls");
    zconf_set_uint (n_controls, ZCONF_DOMAIN "num_controls");
    zconf_set_description ("Saved controls", ZCONF_DOMAIN "num_controls");
    zconf_create_controls (controls, n_controls, "/zapping/options/main");
  }

  /* Write the currently tuned channels */
  printv(" channels");

  zconf_delete (ZCONF_DOMAIN "tuned_channels");

  i = 0;

  while ((channel = tveng_tuned_channel_nth (global_channel_list, i)) != NULL)
    {
      if ((i == (guint) cur_tuned_channel) &&
	  !ChannelWindow) /* Having the channel editor open screws this
			   logic up, do not save controls in this case */
	{
	  g_free (channel->controls);
	  store_control_values (zapping->info,
				&channel->controls,
				&channel->num_controls);
	}

#define SAVE_CONFIG(_type, _name, _cname, _descr)			\
  buffer = g_strdup_printf (ZCONF_DOMAIN "tuned_channels/%d/" #_cname, i); \
  zconf_set_##_type (channel->_name, buffer);				\
  zconf_set_description (_descr, buffer);				\
  g_free (buffer);

      SAVE_CONFIG (string,  name,         name,         "Station name");

      buffer = g_strdup_printf (ZCONF_DOMAIN "tuned_channels/%d/freq", i);
      zconf_set_uint (channel->frequ / 1000, buffer);
      zconf_set_description ("Tuning frequency", buffer);
      g_free (buffer);

      SAVE_CONFIG (z_key,   accel,        accel,        "Accelerator key");
      /* historic "real_name", changed to less confusing rf_name */
      SAVE_CONFIG (string,  rf_name,      real_name,    "RF channel name");
      SAVE_CONFIG (string,  rf_table,     country,      "RF channel table");
      SAVE_CONFIG (uint,    input,        input,        "Attached input");
      SAVE_CONFIG (uint,    standard,     standard,     "Attached standard");
      SAVE_CONFIG (uint,    num_controls, num_controls, "Saved controls");

      if (channel->num_controls > 0)
	{
	  buffer = g_strdup_printf (ZCONF_DOMAIN "tuned_channels/%d", i);
	  zconf_create_controls (channel->controls,
				 channel->num_controls, buffer);
	  g_free (buffer);
	}

      SAVE_CONFIG (int, caption_pgno, caption_pgno, "Default subtitle page");

#ifdef HAVE_LIBZVBI
      SAVE_CONFIG (uint,    num_ttx_encodings, num_ttx_encodings,
		   "Saved Teletext page encodings");

      if (channel->num_ttx_encodings > 0)
	{
	  buffer = g_strdup_printf (ZCONF_DOMAIN "tuned_channels/%d", i);
	  zconf_create_ttx_encodings (channel->ttx_encodings,
				      channel->num_ttx_encodings, buffer);
	  g_free (buffer);
	}
#endif

      i++;
    }

  zconf_set_sources (zapping->info);

  tveng_tuned_channel_list_delete (&global_channel_list);

  /* inputs, standards handling */
  printv("\n v4linterface");
  shutdown_v4linterface();

  /*
   * Tell the overlay engine to shut down and to do a cleanup if necessary
   */
  printv(" overlay");
  /* zilch. */

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
   * VDR
   */
  printv(" vdr");
  shutdown_vdr();

  /*
   * The audio config
   */
  printv(" audio");
  shutdown_audio();

  /*
   * The video output backends.
   */
  printv(" zimage");
  shutdown_zimage();

  /*
   * The colorspace conversions.
   */
  printv(" csconvert");
  shutdown_csconvert();

  /*
   * The mixer code.
   */
  printv(" mixer");
  shutdown_mixer(zapping->info);

  /*
   * The plugin properties dialog.
   */
  printv(" pp");
  shutdown_plugin_properties();

  /*
   * The channel editor.
   */
  printv (" ce");
  shutdown_channel_editor ();

  /*
   * The properties handler.
   */
  printv(" ph");
  shutdown_properties_handler();
  shutdown_properties();

  /* Close */
  printv(" video device");
  tveng_device_info_destroy(zapping->info);

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

  printv(".\nShutdown complete, goodbye.\n");
}

static gboolean startup_zapping(gboolean load_plugins,
				tveng_device_info *info)
{
  guint i = 0;
  gchar * buffer = NULL;
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
  startup_properties();
  D();

  zcc_bool(TRUE, "Show tooltips", "show_tooltips");
  zcc_bool(TRUE, "Resize by fixed increments", "fixed_increments");

#if 0
  zcc_char(tveng_get_country_tune_by_id(0)->name,
	     "The country you are currently in", "current_country");
  current_country = 
    tveng_get_country_tune_by_name(zcg_char(NULL, "current_country"));
  if (!current_country)
    current_country = tveng_get_country_tune_by_id(0);
#else
  /* last selected frequency table */
  zcc_char ("", "The country you are currently in", "current_country");
#endif

#if defined(ENABLE_V4L)
  zcc_char("/dev/video0", "The device file to open on startup",
	   "video_device");
#elif defined(ENABLE_BKTR)
  zcc_char("/dev/bktr", "The device file to open on startup",
	   "video_device");
#else
  zcc_char("", "The device file to open on startup", "video_device");
#endif

  zcc_bool(FALSE, "TRUE if the controls info should be saved with each "
	   "channel", "save_controls");
  zcc_int(0, "Verbosity value given to zapping_setup_fb",
	  "zapping_setup_fb_verbosity");
  zcc_int(0, "Ratio mode", "ratio");
  zcc_int(0, "Change the video mode when going fullscreen", "change_mode");
  zcc_int(0, "Current standard", "current_standard");
  zcc_int(0, "Current video input", "current_input");
  zcc_int(0, "Current audio input", "current_audio_input");
  zcc_int(OLD_TVENG_CAPTURE_WINDOW, "Current capture mode", "capture_mode");
  zcc_int(OLD_TVENG_CAPTURE_WINDOW, "Previous capture mode", "previous_mode");
  zcc_int(8 /* TVENG_PIX_YUYV */, "Pixformat used with XVideo capture",
	  "yuv_format");
  zcc_bool(FALSE, "In videotext mode", "videotext_mode");
  zcc_bool(FALSE, "Keep main window above other windows", "keep_on_top");
  zcc_int(-1, "Icons, text, text below, text beside", "toolbar_style");

  D();

  /* Loads all the tuned channels */

  global_channel_list = NULL;

  i = 0;

  while (zconf_get_nth (i, &buffer, ZCONF_DOMAIN "tuned_channels") != NULL)
    {
      tveng_tuned_channel *tc;
      gchar *buffer2;
      guint len;

      tc = tveng_tuned_channel_new (/* copy of */ NULL);

      len = strlen (buffer);
      while (len > 0 && '/' == buffer[len - 1])
	buffer[--len] = 0;

      /* Get all the items from here */

#define LOAD_CONFIG(_type, _name, _cname)				\
  buffer2 = g_strconcat (buffer, "/" #_cname, NULL);			\
  zconf_get_##_type (&tc->_name, buffer2);				\
  g_free (buffer2);

      LOAD_CONFIG (string,  name,         name);
      LOAD_CONFIG (string,  rf_name,      real_name);
      LOAD_CONFIG (uint,    frequ,        freq);
      tc->frequ *= 1000 /* Hz */;
      LOAD_CONFIG (z_key,   accel,        accel);
      LOAD_CONFIG (string,  rf_table,     country);
      LOAD_CONFIG (uint,    input,        input);
      LOAD_CONFIG (uint,    standard,     standard);
      LOAD_CONFIG (int,	    caption_pgno, caption_pgno);

      /* Error ignored. */
      zconf_get_controls (tc, buffer);

#ifdef HAVE_LIBZVBI
      /* Error ignored. */
      zconf_get_ttx_encodings (tc, buffer);
#endif

      tveng_tuned_channel_insert (&global_channel_list, tc,
				  /* position */ G_MAXINT);

      g_free (buffer);

      i++;
    }

  D();
  /* Starts all modules */
  startup_v4linterface(info);
  D();
  startup_mixer(info);
  D();
  startup_zimage();
  D();

  /* Loads the plugins */
  if (load_plugins)
    plugin_list = plugin_load_plugins();
  /* init them, and remove the ones that couldn't be inited */
 restart_loop:
  D();
  p = g_list_first(plugin_list);
  while (p)
    {
      D();
      plugin_load_config((struct plugin_info*)p->data);
      D();
      if (!plugin_init(info, (struct plugin_info*)p->data))
	{
	  D();
	  plugin_unload((struct plugin_info*)p->data);
	  plugin_list = g_list_remove_link(plugin_list, p);
	  g_list_free_1(p);
	  goto restart_loop;
	}
      p = p->next;
    }
  D();

#ifdef HAVE_LIBZVBI

  {
    struct plugin_info *info;

    if ((info = plugin_by_name ("teletext")))
      {
	_teletext_view_new = plugin_symbol (info, "view_new");
	_teletext_view_from_widget = plugin_symbol (info, "view_from_widget");
	_teletext_toolbar_new = plugin_symbol (info, "toolbar_new");
      }

    if ((info = plugin_by_name ("subtitle")))
      {
	_subtitle_view_new = plugin_symbol (info, "view_new");
      }
  }

#endif

  return TRUE;
}
