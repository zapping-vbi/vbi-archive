#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include <gnome.h>
#include <glade/glade.h>

#include "callbacks.h"
#include "interface.h"

/* keep compiler happy, this is private */
void interface_destroy_callback (GtkWidget * widget, gpointer data);

/* The widget is being destroyed, destroy the GladeXML tree attached
   to it too */
void interface_destroy_callback (GtkWidget * widget,
				 gpointer data)
{
  gtk_object_destroy(GTK_OBJECT(glade_get_widget_tree(widget)));
}

/*
 * Tries to find a widget, that is accesible though parent, named
 * name. IMHO this should be called glade_lookup_widget and go into
 * libglade, but anyway...
 */
GtkWidget*
lookup_widget(GtkWidget * parent, const char * name)
{
  GtkWidget * widget = parent;
  GladeXML* tree;

  for (;;)
    {
      tree = glade_get_widget_tree(widget);
      if (tree)
	break; /* We foud a tree, success */

      /* try to go to the parent widget */
      if (GTK_IS_MENU(widget))
	widget = gtk_menu_get_attach_widget (GTK_MENU (widget) );
      else
	widget = widget -> parent;

      if (!widget)
	{
	  g_warning(_("Widget tree not found for %s"), name);
	  return NULL; /* No way, it could not be found */
	}
    }

  /* if we reach this, we have a tree */
  widget = glade_xml_get_widget (tree, name);
  if (!widget)
    {
      g_warning(_("Widget not found: %s"), name);
      return NULL;
    }

  return widget;
}

/*
 * Loads a GtkWidget from zapping.glade. All the memory is freed when
 * the object (widget) is destroyed. If name is NULL, all widgets are
 * loaded, but this is not recommended.
 */
GtkWidget*
build_widget(const char* name)
{
  GladeXML* xml = glade_xml_new(PACKAGE_DATA_DIR "/zapping.glade",
				 name);
  GtkWidget * widget;

  if ( !xml )
    return NULL;

  widget = glade_xml_get_widget(xml, name);
  if ( !widget )
    return NULL;

  glade_xml_signal_autoconnect(xml);

  /* Attach the callback to the object, so we know when we are
     destroyed */
  gtk_signal_connect( GTK_OBJECT (widget), "destroy",
		      GTK_SIGNAL_FUNC(interface_destroy_callback),
		      NULL);

  return widget;
}

GtkWidget*
create_zapping (void)
{
  return build_widget("zapping");
}

GtkWidget*
create_channel_window (void)
{
  return build_widget("channel_window");
}

GtkWidget*
create_zapping_properties (void)
{

  return build_widget("zapping_properties");
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
  return build_widget("plugin_properties");
}
