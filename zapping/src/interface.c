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

/**
 * Finds in the tree the given widget, returns a pointer to it or NULL
 * if not found
 */
GtkWidget*
find_widget(GtkWidget * parent, const char * name)
{
  GtkWidget * widget = parent;
  GtkWidget * result = NULL;
  GladeXML* tree;
  gchar *buf;

  buf = g_strdup_printf("registered-widget-%s", name);

  for (;;)
    {
      result = (GtkWidget*)gtk_object_get_data(GTK_OBJECT(widget), buf);
      if (result)
	break; /* found registered widget with that name */

      tree = glade_get_widget_tree(widget);
      if (tree)
      {
	result = glade_xml_get_widget (tree, name);
	if (result)
	  break; /* found glade widget with that name */
      }

      /* try to go to the parent widget */
      if (GTK_IS_MENU(widget))
	widget = gtk_menu_get_attach_widget (GTK_MENU (widget) );
      else
	widget = widget -> parent;

      if (!widget)
	break; /* Walked to the top of the tree, nothing found */
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
lookup_widget( GtkWidget *parent, const char *name)
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

/*
 * Loads a GtkWidget from zapping.glade. All the memory is freed when
 * the object (widget) is destroyed. If name is NULL, all widgets are
 * loaded, but this is not recommended.
 */
GtkWidget*
build_widget(const char* name, const char* glade_file)
{
  GladeXML* xml = glade_xml_new(glade_file, name);
  GtkWidget * widget;

  if ( !xml )
    {
      RunBox("%s [%s] couldn't be found, please contact the maintainer",
	     GNOME_MESSAGE_BOX_ERROR, glade_file, name);
      exit(1);
    }

  widget = glade_xml_get_widget(xml, name);

  if ( !widget )
    {
      RunBox("%s [%s] couldn't be loaded, please contact the maintainer",
	     GNOME_MESSAGE_BOX_ERROR, glade_file, name);
      exit(1);
    }

  glade_xml_signal_autoconnect(xml);

  return widget;
}

GtkWidget*
create_zapping (void)
{
  return build_widget("zapping", PACKAGE_DATA_DIR "/zapping.glade");
}

GtkWidget*
create_zapping_properties (void)
{
  return build_widget("zapping_properties", PACKAGE_DATA_DIR "/zapping.glade");
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
                        _("(C) Iñaki García Etxebarria"),
                        authors,
                        _("A TV viewer for the Gnome Desktop Environment"),
                        "zapping/logo.png");
  gtk_window_set_modal (GTK_WINDOW (about2), FALSE);

  return about2;
}

GtkWidget*
create_plugin_properties (void)
{
  return build_widget("plugin_properties", PACKAGE_DATA_DIR "/zapping.glade");
}

GtkWidget*
create_popup_menu1 (void)
{
  return build_widget("popup_menu1", PACKAGE_DATA_DIR "/zapping.glade");
}

GtkWidget*
create_searching (void)
{
  return build_widget("searching", PACKAGE_DATA_DIR "/zapping.glade");
}

GtkWidget*
create_txtcontrols (void)
{
  return build_widget("txtcontrols", PACKAGE_DATA_DIR "/zapping.glade");
}

GtkWidget*
create_ttxview (void)
{
  return build_widget("ttxview", PACKAGE_DATA_DIR "/zapping.glade");
}

GtkWidget*
create_ttxview_popup (void)
{
  return build_widget("ttxview_popup", PACKAGE_DATA_DIR "/zapping.glade");
}

GtkWidget*
create_prompt (void)
{
  return build_widget("prompt", PACKAGE_DATA_DIR "/zapping.glade");
}

GtkWidget*
create_widget (const gchar *name)
{
  return build_widget(name, PACKAGE_DATA_DIR "/zapping.glade");
}
