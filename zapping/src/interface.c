#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include <gnome.h>
#include <glade/glade.h>

#include "interface.h"
#include "zmisc.h"
#include "zconf.h"
#include "remote.h"

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
  gchar *long_name;
  gchar *buf;

  buf = g_strdup_printf("registered-widget-%s", name);

  while (widget)
    {
      if ((result = (GtkWidget *) gtk_object_get_data (GTK_OBJECT (widget), buf)))
	break; /* found registered widget with that name */

      if ((tree = glade_get_widget_tree (widget)))
	if ((result = glade_xml_get_widget (tree, name)))
	  break; /* found glade widget with that name */

      if (0)
	if ((long_name = (gchar *) glade_get_widget_long_name (widget)))
	  {
	    fprintf (stderr, "found '%s'\n", long_name);
	    g_free (long_name);
	  }

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
      RunBox("Widget %s not found, please contact the maintainer",
	     GNOME_MESSAGE_BOX_ERROR, name);
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
	  gtk_object_set_data(GTK_OBJECT(widget), buf, this_widget);
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
 * zapping.glade. All the memory is freed when the object (widget)
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
    file = "zapping.glade";

  path = g_strconcat (PACKAGE_DATA_DIR, "/", file, NULL);

  xml = glade_xml_new (path, name);

  if (!xml)
    {
      RunBox ("%s [%s] couldn't be found, please contact the maintainer",
	      GNOME_MESSAGE_BOX_ERROR, path, name);
      exit (1);
    }

  widget = glade_xml_get_widget (xml, name);

  if (!widget)
    {
      RunBox ("%s [%s] couldn't be loaded, please contact the maintainer",
	      GNOME_MESSAGE_BOX_ERROR, path, name);
      exit (1);
    }

  glade_xml_signal_autoconnect (xml);

  g_free (path);

  return widget;
}

void
change_toolbar_style (GtkWidget *widget, int style)
{
  extern GtkWidget *main_window;
  static GtkToolbarStyle gts[] = {
    GTK_TOOLBAR_ICONS,
    GTK_TOOLBAR_TEXT,
    GTK_TOOLBAR_BOTH
  };

  if (!widget)
    widget = main_window;

  if (!widget || style < 0 || style > 2)
    return;

  widget = lookup_widget (widget, "dockitem2"); /* main window */
  widget = gnome_dock_item_get_child (GNOME_DOCK_ITEM (widget));

  if (!widget)
    return;

  gtk_toolbar_set_style (GTK_TOOLBAR (widget), gts[style]);
}

#define MENU_CMD(_name, _cmd)						\
  w = lookup_widget (widget, #_name);					\
  gtk_signal_connect (GTK_OBJECT (w), "activate",			\
		      (GtkSignalFunc) on_remote_command1,		\
		      (gpointer)((const gchar *) _cmd));

GtkWidget*
create_zapping (void)
{
  GtkWidget *widget;
  GtkWidget *w;

  widget = build_widget("zapping", NULL);

  /* Change the pixmaps, work around glade bug */
  set_stock_pixmap (w = lookup_widget (widget, "channel_up"),
		    GNOME_STOCK_PIXMAP_UP);
  set_stock_pixmap (w = lookup_widget (widget, "channel_down"),
		    GNOME_STOCK_PIXMAP_DOWN);

  /* Menu remote commands, not possible with glade */
  MENU_CMD (quit1,		"quit");
  MENU_CMD (go_fullscreen1,	"switch_mode fullscreen");
  MENU_CMD (go_previewing2,	"switch_mode preview");
  MENU_CMD (go_capturing2,	"switch_mode capture");
  MENU_CMD (videotext1,		"switch_mode teletext");
  MENU_CMD (new_ttxview,	"ttx_open_new");
  MENU_CMD (mute2,		"mute");

  w = gtk_toolbar_insert_element (GTK_TOOLBAR (lookup_widget (widget, "toolbar1")),
                                  GTK_TOOLBAR_CHILD_TOGGLEBUTTON,
                                  NULL,
                                  _("Mute"),
                                  _("Switch audio on and off"),
				  NULL,
                                  z_load_pixmap ("mute.png"),
				  NULL, NULL, 3);
  gtk_object_set_data (GTK_OBJECT (widget), "registered-widget-tb-mute", w);
  gtk_widget_show (w);

  gtk_signal_connect (GTK_OBJECT (w), "toggled",
		      (GtkSignalFunc) on_remote_command1,
		      (gpointer)((const gchar *) "mute"));

  propagate_toolbar_changes (lookup_widget (widget, "toolbar1"));

  zconf_create_integer (2, "Display icons, text or both",
		        "/zapping/options/main/toolbar_style");
  change_toolbar_style (widget, zconf_get_integer (NULL,
		        "/zapping/options/main/toolbar_style"));

  return widget;
}

/*
 * This one doesn't use libglade, because libglade cannot properly
 * display the logo.
 */
GtkWidget*
create_about2 (void)
{
  const gchar *authors[] = {
    "Iñaki García Etxebarria <garetxe@users.sourceforge.net>",
    NULL
  };
  GtkWidget *about2;

  about2 = gnome_about_new ("Zapping", VERSION,
                        "(C) Iñaki García Etxebarria",
                        authors,
                        _("A TV viewer for the Gnome Desktop Environment"),
                        "zapping/logo.png");
  gtk_window_set_modal (GTK_WINDOW (about2), FALSE);

  return about2;
}

GtkWidget*
create_popup_menu1 (void)
{
  GtkWidget *widget;
  GtkWidget *w;

  widget = build_widget ("popup_menu1", NULL);

  /* Menu remote commands, not possible with glade */
  MENU_CMD (go_fullscreen2,	"switch_mode fullscreen");
  MENU_CMD (go_previewing2,	"switch_mode preview");
  MENU_CMD (go_capturing2,	"switch_mode capture");
  MENU_CMD (videotext2,		"switch_mode teletext");
  MENU_CMD (new_ttxview2,	"ttx_open_new");

  return widget;
}
