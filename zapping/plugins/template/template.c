/* Screenshot saving plugin for Zapping
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
#include "plugin_common.h"
/*
  You can use this code as the template for your own plugins. If you
  have any doubts after reading this and the docs, please contact the
  author. Declaring things as static isn't needed, it is just to make
  clear what is to be exported and what isn't.
*/

/* This is the description of the plugin, change as appropiate */
static const gchar str_canonical_name[] = "template";
static const gchar str_descriptive_name[] =
N_("Template plugin");
static const gchar str_description[] =
N_("This plugin is just a template, it does nothing.");
static const gchar str_short_description[] = 
N_("This plugin does nothing useful.");
static const gchar str_author[] = "Iñaki García Etxebarria";
/* The format of the version string must be
   %d[[.%d[.%d]][other_things]], where the things between [] aren't
   needed, and if not present, 0 will be assumed */
static const gchar str_version[] = "1.0gamma";

/* Active status of the plugin */
static gboolean active = FALSE;

gint zp_protocol ( void )
{
  /* You don't need to modify this function */
  return PLUGIN_PROTOCOL;
}

gboolean zp_running (void)
{
  /* This will usually be like this too */
  return active;
}

void zp_get_info (gchar ** canonical_name, gchar **
		  descriptive_name, gchar ** description, gchar **
		  short_description, gchar ** author, gchar **
		  version)
{
  /* Usually, this one doesn't need modification either */
  if (canonical_name)
    *canonical_name = _(str_canonical_name);
  if (descriptive_name)
    *descriptive_name = _(str_descriptive_name);
  if (description)
    *description = _(str_description);
  if (short_description)
    *short_description = _(str_short_description);
  if (author)
    *author = _(str_author);
  if (version)
    *version = _(str_version);
}

gboolean zp_init (PluginBridge bridge, tveng_device_info * info)
{
  /* Do any startup you need here, and return FALSE on error */

  /* If this is set, autostarting is on (we should start now) */
  if (active)
    return zp_start();

  return TRUE;
}

void zp_close(void)
{
  /* If we were working, stop the work */
  if (active)
    zp_stop();

  /* Any cleanups would go here (closing fd's and so on) */
}

gboolean zp_start (void)
{
  /* In most plugins, you don't want to be started twice */
  if (active)
    return TRUE;

  /* Do any neccessary work to start the plugin here */

  /* If everything has been ok, set the active flags and return TRUE
   */
  active = TRUE;
  return TRUE;
}

void zp_stop(void)
{
  /* Most times we cannot be stopped while we are stopped */
  if (!active)
    return;

  /* Stop anything the plugin is doing and set the flag */
  active = FALSE;
}

void zp_load_config (gchar * root_key)
{
  gchar * buffer;
  /* The autostart config value is compulsory, you shouldn't need to
     change the following */
  buffer = g_strconcat(root_key, "autostart", NULL);
  /* Create sets a default value for a key, check src/zconf.h */
  zconf_create_boolean(FALSE,
		       _("Whether the plugin should start"
			 " automatically when opening Zapping"), buffer);
  active = zconf_get_boolean(NULL, buffer);
  g_free(buffer);

  /* Load here any other config key */
}

void zp_save_config (gchar * root_key)
{
  gchar * buffer;

  /* This one is compulsory, you won't need to change it */
  buffer = g_strconcat(root_key, "autostart", NULL);
  zconf_set_boolean(active, buffer);
  g_free(buffer);

  /* Save here any other config keys you need to save*/
}

gpointer zp_process_frame(gpointer data, struct tveng_frame_format *
			  format)
{
  /* If the plugin isn't active, it shouldn't do anything */
  if (!active)
    return data;

  /*
    Return the modified data (the same as the supplied on, in this
    case).
  */
  return data;
}

void yoyo_dado ( void );
void yoyo_dado ( void )
{
  /* What a incredible waste of resources, isn't it? ;-DDD */
  ShowBox("Yoyo-Dado !!", GNOME_MESSAGE_BOX_INFO);
}


gboolean zp_get_public_info (gint index, gpointer * ptr, gchar **
			     symbol, gchar ** description, gchar **
			     type, gint * hash)
{
  /*
    This plugin exports a dummy function that basically does
    nothing. You don't need to export anything if you don't want to.
    The plugin_exported_symbol is defined for your convenience, you
    don't need to use if you don't want to.
   */
  struct plugin_exported_symbol symbols[] =
  {
    {
      /* Note that in this symbol the returned symbol name and the
	 "real" name are different, this is perfectly legal, but it
	 isn't a good practice. */
      yoyo_dado, "yoyo-dado",
      N_("Example of an exported symbol, does nothing useful"),
      "void yoyo-dado ( void );", 0x1234
    }
  };
  gint num_exported_symbols =
    sizeof(symbols)/sizeof(struct plugin_exported_symbol);

  if ((index >= num_exported_symbols) || (index < 0))
    return FALSE;

  if (ptr)
    *ptr = symbols[index].ptr;
  if (symbol)
    *symbol = symbols[index].symbol;
  if (description)
    *description = _(symbols[index].description);
  if (type)
    *type = symbols[index].type;
  if (hash)
    *hash = symbols[index].hash;

  return TRUE; /* Exported */
}

void zp_add_properties ( GnomePropertyBox * gpb )
{
  /* Here you would add a page to the property box. Define this
     function only if you are going to add something to the box */
}

gboolean zp_activate_properties ( GnomePropertyBox * gpb, gint page )
{
  /* Return TRUE only if the given page have been builded by this
     plugin, and apply any config changes here */
  return FALSE;
}

gboolean zp_help_properties ( GnomePropertyBox * gpb, gint page )
{
  /*
    Return TRUE only if the given page have been builded by this
    plugin, and show some help (or at least sth like ShowBox
    "Sorry, but the template plugin doesn't help you").
  */
  return FALSE;
}

void zp_add_gui (GnomeApp * app)
{
  /*
    Define this function only if you are going to do something to the
    main Zapping window.
  */
}

void zp_remove_gui (GnomeApp * app)
{
  /*
    Define this function if you have defined previously zp_add_gui
   */
}

gint zp_get_priority (void)
{
  /*
    Tell that the template plugin should be run with a high priority
    (just to put an example)
  */
  return 5;
}
