#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>
#include <glade/glade.h>

#include "interface.h"
#include "remote.h"
#include "zmisc.h"
#include "globals.h"

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
      g_warning ("%s [%s] couldn't be found, please contact the maintainer",
		 path, name);
      exit (1);
    }

  g_free (path);

  widget = glade_xml_get_widget (xml, name);

  if (!widget)
    {
      g_warning ("%s [%s] couldn't be loaded, please contact the maintainer",
		 path, name);
      exit (1);
    }

  glade_xml_signal_autoconnect (xml);

  return widget;
}

#define MENU_CMD(_name, _cmd)					\
  w = lookup_widget (widget, #_name);				\
  g_signal_connect (G_OBJECT (w), "activate",			\
		    (GCallback) on_remote_command1,		\
		    (gpointer)((const gchar *) _cmd));

GtkWidget*
create_zapping (void)
{
  GtkWidget *widget, *w;

  widget = build_widget ("zapping", NULL);

  /* Work around libglade bug */
  w = lookup_widget (widget, "tv_screen");
  gtk_widget_add_events (w, GDK_BUTTON_PRESS_MASK | GDK_EXPOSURE_MASK
			 | GDK_POINTER_MOTION_MASK |
			 GDK_VISIBILITY_NOTIFY_MASK |
			 GDK_KEY_PRESS_MASK);

  /* Menu remote commands, not possible with glade */
  MENU_CMD (quit1,		"zapping.quit()");
  MENU_CMD (go_fullscreen1,	"zapping.switch_mode('fullscreen')");
  MENU_CMD (go_previewing2,	"zapping.switch_mode('preview')");
  MENU_CMD (go_capturing2,	"zapping.switch_mode('capture')");
  MENU_CMD (videotext1,		"zapping.switch_mode('teletext')");
  MENU_CMD (new_ttxview,	"zapping.ttx_open_new()");
  MENU_CMD (mute2,		"zapping.mute()");
  MENU_CMD (about1,		"zapping.about()");
  MENU_CMD (propiedades1,	"zapping.properties()");
  MENU_CMD (hide_controls2,	"zapping.hide_controls()");
  MENU_CMD (plugins1,		"zapping.plugin_properties()");

  /* Toolbar commands */
  w = lookup_widget (widget, "tb-mute");
  g_signal_connect (G_OBJECT(w), "toggled",
		    (GCallback) on_remote_command1,
		    "zapping.mute()");

  w = lookup_widget (widget, "controls");
  g_signal_connect (G_OBJECT(w), "clicked",
		    (GCallback) on_remote_command1,
		    "zapping.control_box()");

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
 
  /* Menu remote commands, not possible with glade */
  MENU_CMD (go_fullscreen2,	"zapping.switch_mode('fullscreen')");
  MENU_CMD (go_previewing2,	"zapping.switch_mode('preview')");
  MENU_CMD (go_capturing2,	"zapping.switch_mode('capture')");
  MENU_CMD (videotext2,		"zapping.switch_mode('teletext')");
  MENU_CMD (new_ttxview2,	"zapping.ttx_open_new()");

  /* Fixme: These don't work, dunno why */
  MENU_CMD (hide_controls1,	"zapping.hide_controls()");
  MENU_CMD (pal_big,		"zapping.resize_screen(768, 576)");
  MENU_CMD (rec601_pal_big,	"zapping.resize_screen(720, 576)");
  MENU_CMD (ntsc_big,		"zapping.resize_screen(640, 480)");
  MENU_CMD (pal_small,		"zapping.resize_screen(384, 288)");
  MENU_CMD (rec601_pal_small,	"zapping.resize_screen(360, 288)");
  MENU_CMD (ntsc_small,		"zapping.resize_screen(320, 240)");

  return widget;
}
