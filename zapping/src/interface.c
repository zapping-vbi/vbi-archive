/*
 *  Zapping (TV viewer for the Gnome Desktop)
 *
 * Copyright (C) 2001 Iñaki García Etxebarria
 * Copyright (C) 2003 Michael H. Schimek
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

/* $Id: interface.c,v 1.32 2005-01-08 14:54:28 mschimek Exp $ */

/* XXX gtk+ 2.3 toolbar changes */
#undef GTK_DISABLE_DEPRECATED

#include "config.h"

#include <gnome.h>
#include <glade/glade.h>

#include "interface.h"
#include "zmisc.h"
#include "globals.h"
#include "zconf.h"
#include "properties-handler.h"
#include "zvideo.h"
#include "v4linterface.h"
#include "plugins.h"
//#include "zvbi.h"
#include "remote.h"

extern gboolean have_wm_hints;

/* Finds the named widget in the tree parent belongs to.
   Returns a pointer to it or NULL if not found. */
GtkWidget *
find_widget			(GtkWidget *		parent,
				 const gchar *		name)
{
  GtkWidget *widget;
  GtkWidget *result;
  GladeXML *tree;
  gchar *buf;

  widget = parent;
  result = NULL;

  buf = g_strconcat ("registered-widget-", name, NULL);

  while (widget)
    {
      if ((result = (GtkWidget*) g_object_get_data (G_OBJECT (widget), buf)))
	break; /* found registered widget with that name */

      if ((tree = glade_get_widget_tree (widget)))
	if ((result = glade_xml_get_widget (tree, name)))
	  break; /* found glade widget with that name */

      if (0)
	fprintf (stderr, "found '%s'\n",
		 glade_get_widget_name (widget));

      if (GTK_IS_MENU (widget))
	widget = gtk_menu_get_attach_widget (GTK_MENU (widget));
      else
	widget = widget->parent;
    }

  g_free (buf);

  return result;
}

/* Tries to find a widget that is accesible though parent, named
   name. IMHO this should be called glade_lookup_widget and go into
   libglade, but anyway... If the widget isn't found, a message is
   printed and the program quits, it always returns a valid widget. */
GtkWidget *
lookup_widget			(GtkWidget *		parent,
				 const gchar *		name)
{
  GtkWidget *widget = find_widget (parent, name);

  if (!widget)
    {
      RunBox ("Widget %s not found, please contact the maintainer",
	      GTK_MESSAGE_ERROR, name);
      exit (EXIT_FAILURE);
    }

  return widget;
}

/* Register a widget to be found with find_widget() or lookup_widget().
   The information is attached to the toplevel ancestor of parent,
   or when NULL of the widget. */
void
register_widget			(GtkWidget *		parent,
				 GtkWidget *		widget,
				 const char *		name)
{
  if (!parent)
    parent = widget;

  for (;;)
    {
      if (!GTK_IS_MENU (parent) && !parent->parent)
	{
	  gchar * buf;

	  buf = g_strconcat ("registered-widget-", name, NULL);
	  g_object_set_data (G_OBJECT (parent), buf, widget);
	  g_free (buf);
	  return;
	}

      if (GTK_IS_MENU (parent))
	parent = gtk_menu_get_attach_widget (GTK_MENU (parent));
      else
	parent = parent->parent;

      if (!parent)
	return; /* toplevel not found */
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
      RunBox ("File %s [%s] not found, please contact the maintainer",
	      GTK_MESSAGE_ERROR, path, name);
      exit (EXIT_FAILURE);
    }

  g_free (path);

  widget = glade_xml_get_widget (xml, name);

  if (!widget)
    {
      RunBox ("Widget %s not found in %s, please contact the maintainer",
	      GTK_MESSAGE_ERROR, name, path);
      exit (EXIT_FAILURE);
    }

  glade_xml_signal_autoconnect (xml);

  return widget;
}
