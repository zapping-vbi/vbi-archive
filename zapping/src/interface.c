#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>
#include <glade/glade.h>

#include "interface.h"
#include "remote.h"
#include "zmisc.h"
#include "globals.h"
#include "zconf.h"
#include "properties-handler.h"
#include "zvideo.h"
#include "callbacks.h"

extern gboolean have_wm_hints;

/**
 * Finds in the tree the given widget, returns a pointer to it or NULL
 * if not found
 */
GtkWidget *
find_widget				(GtkWidget *	parent,
					 const gchar *	name)
{
  GtkWidget *widget = parent;
  GtkWidget *result = NULL;
  GladeXML *tree;
  gchar *buf;

  buf = g_strdup_printf("registered-widget-%s", name);

  while (widget)
    {
      if ((result = (GtkWidget*) g_object_get_data (G_OBJECT(widget), buf)))
	break; /* found registered widget with that name */

      if ((tree = glade_get_widget_tree (widget)))
	if ((result = glade_xml_get_widget (tree, name)))
	  break; /* found glade widget with that name */

      if (0)
	fprintf (stderr, "found '%s'\n",
		 glade_get_widget_name(widget));

      /* try to go to the parent widget */
      if (GTK_IS_MENU (widget))
	widget = gtk_menu_get_attach_widget (GTK_MENU (widget));
      else
	widget = widget->parent;
    }

  g_free(buf);

  return result;
}

/*
 * Tries to find a widget, that is accesible though parent, named
 * name. IMHO this should be called glade_lookup_widget and go into
 * libglade, but anyway...
 * If the widget isn't found, a message is printed and the program
 * quits, it always returns a valid widget.
 */
GtkWidget *
lookup_widget				(GtkWidget *	parent,
					 const gchar *	name)
{
  GtkWidget *widget = find_widget(parent, name);

  if (!widget)
    {
      RunBox(_("Widget %s not found, please contact the maintainer"),
	     GTK_MESSAGE_ERROR, name);
      exit(1);
    }

  return widget;
}

void
register_widget(GtkWidget * widget, const char * name)
{
  GtkWidget * this_widget = widget;
  gchar *buf;

  /* Walk to the topmost level and register */
  for (;;)
    {
      if (!GTK_IS_MENU(widget) && !widget->parent)
	{
	  buf = g_strdup_printf("registered-widget-%s", name);
	  g_object_set_data(G_OBJECT(widget), buf, this_widget);
	  g_free(buf);
	  return;
	}

      /* try to go to the parent widget */
      if (GTK_IS_MENU(widget))
	widget = gtk_menu_get_attach_widget (GTK_MENU (widget) );
      else
	widget = widget -> parent;

      if (!widget)
	return; /* Toplevel not found */
    }
}

/**
 * build_widget:
 * @name: Name of the widget.
 * @file: Name of the Glade file. 
 * 
 * Loads a GtkWidget from a Glade file, when @file is %NULL from
 * zapping.glade2. All the memory is freed when the object (widget)
 * is destroyed. If name is %NULL, all widgets are loaded, but this
 * is not recommended.
 * 
 * Return value: 
 * Widget pointer, cannot fail.
 **/
GtkWidget *
build_widget			(const gchar *		name,
				 const gchar *		file)
{
  GladeXML *xml;
  GtkWidget *widget;
  gchar *path;

  if (!file)
    file = "zapping.glade2";

  path = g_strconcat (PACKAGE_DATA_DIR "/" PACKAGE "/", file, NULL);
  xml = glade_xml_new (path, name, NULL);

  if (!xml)
    {
      RunBox ("%s [%s] couldn't be found, please contact the maintainer",
	      GTK_MESSAGE_ERROR, path, name);
      exit (1);
    }

  g_free (path);

  widget = glade_xml_get_widget (xml, name);

  if (!widget)
    {
      RunBox ("%s [%s] couldn't be loaded, please contact the maintainer",
	      GTK_MESSAGE_ERROR, path, name);
      exit (1);
    }

  glade_xml_signal_autoconnect (xml);

  return widget;
}

#define PY_CMD(_name, _signal, _cmd)				\
  w = lookup_widget (widget, #_name);				\
  g_signal_connect (G_OBJECT (w), _signal,			\
		    (GCallback) on_remote_command1,		\
		    (gpointer)((const gchar *) _cmd));

GtkWidget*
create_zapping (void)
{
  GdkColor col = { 0, 0, 0, 0 };
  GtkWidget *widget, *w;
  GtkWidget *box;
  GtkWidget *tv_screen;

  widget = build_widget ("zapping", NULL);
  gtk_widget_modify_bg (widget, GTK_STATE_NORMAL, &col);

  box = lookup_widget (widget, "tv_screen_box");

  tv_screen = z_video_new ();
  gtk_container_add (GTK_CONTAINER (box), tv_screen);
  register_widget (tv_screen, "tv-screen");

  // XXX free, 4:3, 16:9
  if (zconf_get_boolean (NULL, "/zapping/options/main/fixed_increments"))
    z_video_set_size_inc (Z_VIDEO (tv_screen), 64, 64 * 3 / 4);

  gtk_widget_add_events (tv_screen,
			 GDK_BUTTON_PRESS_MASK |
			 GDK_BUTTON_RELEASE_MASK |
			 GDK_EXPOSURE_MASK |
			 GDK_POINTER_MOTION_MASK |
			 GDK_VISIBILITY_NOTIFY_MASK |
			 GDK_KEY_PRESS_MASK);

  g_signal_connect (G_OBJECT (tv_screen), "button_press_event",
		    (GCallback) on_tv_screen_button_press_event,
		    (gpointer) NULL);

  gtk_widget_show (tv_screen);

  w = lookup_widget (widget, "keep_window_on_top2");
  gtk_widget_set_sensitive (w, !!have_wm_hints);
  /* XXX tell why not */
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (w),
    zconf_get_boolean (NULL, "/zapping/options/main/keep_on_top")
    && !!have_wm_hints);

  PY_CMD (quit1,		"activate",	"zapping.quit()");
  PY_CMD (go_fullscreen1,	"activate",	"zapping.switch_mode('fullscreen')");
  PY_CMD (go_previewing2,	"activate",	"zapping.switch_mode('preview')");
  PY_CMD (go_capturing2,	"activate",	"zapping.switch_mode('capture')");
  PY_CMD (videotext1,		"activate",	"zapping.switch_mode('teletext')");
  PY_CMD (new_ttxview,		"activate",	"zapping.ttx_open_new()");
  PY_CMD (mute2,		"activate",	"zapping.mute()");
  PY_CMD (about1,		"activate",	"zapping.about()");
  PY_CMD (propiedades1,		"activate",	"zapping.properties()");
  PY_CMD (hide_controls2,	"activate",	"zapping.hide_controls()");
  PY_CMD (keep_window_on_top2,	"activate",	"zapping.keep_on_top()");
  PY_CMD (plugins1,		"activate",	"zapping.plugin_properties()");
  PY_CMD (main_help1,		"activate",	"zapping.help()");
  PY_CMD (vbi_info1,		"activate",	"zapping.network_info()");
  PY_CMD (program_info1,	"activate",	"zapping.program_info()");
  PY_CMD (closed_caption1,	"activate",	"zapping.closed_caption()");
  PY_CMD (channels1,		"activate",	"zapping.channel_editor()");
  PY_CMD (channel_up,		"clicked",	"zapping.channel_up()");
  PY_CMD (channel_down,		"clicked",	"zapping.channel_down()");
  PY_CMD (tb-mute,		"toggled",	"zapping.mute()");
  PY_CMD (controls,		"clicked",	"zapping.control_box()");

  propagate_toolbar_changes (lookup_widget (widget, "toolbar1"));

#if 0
  zconf_create_integer (2, "Display icons, text or both",
		        "/zapping/options/main/toolbar_style");
  change_toolbar_style (widget, zconf_get_integer (NULL,
		        "/zapping/options/main/toolbar_style"));
#endif

  return widget;
}

GtkWidget*
create_popup_menu1 (void)
{
  GtkWidget *widget;
  GtkWidget *w;

  widget = build_widget ("popup_menu1", NULL);

  w = lookup_widget (widget, "keep_window_on_top1");
  gtk_widget_set_sensitive (w, !!have_wm_hints);
  /* XXX tell why not */
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (w),
				  zconf_get_boolean
				  (NULL, "/zapping/options/main/keep_on_top")
				  && !!have_wm_hints);

  PY_CMD (go_fullscreen2,	"activate", "zapping.switch_mode('fullscreen')");
  PY_CMD (go_previewing2,	"activate", "zapping.switch_mode('preview')");
  PY_CMD (go_capturing2,	"activate", "zapping.switch_mode('capture')");
  PY_CMD (videotext2,		"activate", "zapping.switch_mode('teletext')");
  PY_CMD (new_ttxview2,		"activate", "zapping.ttx_open_new()");
  PY_CMD (hide_controls1,	"activate", "zapping.hide_controls()");
  PY_CMD (keep_window_on_top1,	"activate", "zapping.keep_on_top()");

  w = lookup_widget (widget, "appearance1_menu");
  picture_sizes_append_menu (GTK_MENU_SHELL (w));

  return widget;
}
