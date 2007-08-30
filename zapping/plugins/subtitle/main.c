/*
 *  Zapping TV viewer
 *
 *  Copyright (C) 2000-2002 Iñaki García Etxebarria
 *  Copyright (C) 2000-2005 Michael H. Schimek
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* $Id: main.c,v 1.2 2007-08-30 14:14:32 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "src/plugin_common.h"
#include "src/properties.h"
#include "preferences.h"
#include "main.h"
#include "view.h"

GList *				subtitle_views;

static void
plugin_close			(void)
{
  while (subtitle_views)
    gtk_widget_destroy (GTK_WIDGET (subtitle_views->data));
}

static void
properties_add			(GtkDialog *		dialog)
{
  static SidebarEntry se = {
    .label		= N_("Subtitles"),
    .icon_name		= "subtitle48.png",
    .create		= subtitle_prefs_new,
    .apply		= (void (*)(GtkWidget *)) subtitle_prefs_apply,
    .cancel		= (void (*)(GtkWidget *)) subtitle_prefs_cancel,
    .help_link_id	= "zapping-settings-vbi",
  };
  static const SidebarGroup sg = {
    N_("Plugins"), &se, 1
  };

  standard_properties_add (dialog, &sg, 1, /* glade */ NULL);
}

static gboolean
plugin_init			(PluginBridge		bridge _unused_,
				 tveng_device_info *	info _unused_)
{
  static const property_handler ph = {
    .add = properties_add,
  };

  D();

  append_property_handler (&ph);

  D();

  return TRUE;
}

static void
plugin_get_info			(const gchar **		canonical_name,
				 const gchar **		descriptive_name,
				 const gchar **		description,
				 const gchar **		short_description,
				 const gchar **		author,
				 const gchar **		version)
{
  if (canonical_name)
    *canonical_name = "subtitle";
  if (descriptive_name)
    *descriptive_name = N_("Subtitle plugin");
  if (description)
    *description = "";
  if (short_description)
    *short_description = "";
  if (author)
    *author = "";
  if (version)
    *version = "1.0";
}

static struct plugin_misc_info *
plugin_get_misc_info		(void)
{
  static struct plugin_misc_info returned_struct = {
    sizeof (struct plugin_misc_info),
    6, /* plugin priority */
    0 /* category */
  };

  return &returned_struct;
}

gboolean
plugin_get_symbol		(const gchar *		name,
				 gint			hash,
				 gpointer *		ptr)
{
  static const struct plugin_exported_symbol symbols [] = {
    SYMBOL (plugin_close, 0x1234),
    SYMBOL (plugin_get_info, 0x1234),
    SYMBOL (plugin_get_misc_info, 0x1234),
    SYMBOL (plugin_init, 0x1234),
    SYMBOL2 (subtitle, view_new),
  };
  guint i;

  for (i = 0; i < N_ELEMENTS (symbols); ++i)
    if (0 == strcmp (symbols[i].symbol, name))
      {
	if (symbols[i].hash != hash)
	  {
	    if (ptr)
	      *ptr = GINT_TO_POINTER(0x3); /* hash collision code */
	    g_warning("Check error: \"%s\" in plugin %s "
		      "has hash 0x%x vs. 0x%x",
		      name, "teletext", symbols[i].hash, hash);
	    return FALSE;
	  }
	if (ptr)
	  *ptr = symbols[i].ptr;
	return TRUE;
      }

  if (ptr)
    *ptr = GINT_TO_POINTER(0x2);
  return FALSE;
}

gint
plugin_get_protocol		(void)
{
  return PLUGIN_PROTOCOL;
}

/*
Local variables:
c-set-style: gnu
c-basic-offset: 2
End:
*/
