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
/*
  Structures shared by Zapping and the plugins
*/
#ifndef __PLUGINS_COMMON_H__
#define __PLUGINS_COMMON_H__

#ifdef HAVE_CONFIG_H
#  include <config.h> /* VERSION */
#endif

#include <gnome.h>

#include "tveng.h"
#include "zmisc.h"
#include "zconf.h"
#include "interface.h"

/* The plugin protocol we are able to understand */
#define PLUGIN_PROTOCOL 2

/* The definition of a PluginBrigde */
typedef gboolean (*PluginBridge) ( gpointer * ptr, gchar * plugin,
				   gchar * symbol, gchar * type,
				   gint hash );

/* This structure holds info about one of the exported symbols of the
   plugin */
struct plugin_exported_symbol
{
  gpointer ptr; /* The resolved symbol */
  gchar * symbol; /* The name of the symbol */
  gchar * description; /* A brief description for the symbol */
  gchar * type; /* Symbol type */
  gint hash; /* Symbol hash */
};

#ifndef ZAPPING /* If this is being included from a plugin, give them
		   the correct prototypes for public symbols ( so
		   compiling will give an error if defined differently) */

/* These are not commented here because they are in the plugin docs */
/* This is the declaration of the two public symbols, the rest is
   static and will give a warning if declared and not implemented */
gint plugin_get_protocol (void);
gboolean plugin_get_symbol(gchar * name, gint hash, gpointer * ptr);

static gboolean plugin_running ( void );
static
void plugin_get_info ( gchar ** canonical_name, gchar **
		       descriptive_name, gchar ** description, gchar **
		       short_description, gchar ** author, gchar **
		       version );
static
gboolean plugin_init ( PluginBridge bridge, tveng_device_info * info );
static void plugin_close( void );
static gboolean plugin_start ( void );
static void plugin_stop( void );
static void plugin_load_config ( gchar * root_key );
static void plugin_save_config ( gchar * root_key );
static GdkImage * plugin_process_frame(GdkImage * image, gpointer data,
				       struct tveng_frame_format * format);
static
gboolean plugin_get_public_info (gint index, gpointer * ptr, gchar **
				 symbol, gchar ** description, gchar **
				 type, gint * hash);
static void plugin_add_properties ( GnomePropertyBox * gpb );
static
gboolean plugin_activate_properties ( GnomePropertyBox * gpb, gint page );
static
gboolean plugin_help_properties ( GnomePropertyBox * gpb, gint page );
static void plugin_add_gui ( GnomeApp * app );
static void plugin_remove_gui ( GnomeApp * app );
static gint plugin_get_priority ( void );

/* This macro if for your convenience, it symplifies adding symbols */
#define SYMBOL(symbol, hash) \
{symbol, #symbol, NULL, NULL, hash}
#endif

#endif /* PLUGINS_COMMON */
