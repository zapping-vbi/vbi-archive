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
  author. Declaring things as static is required, otherwise the
  dinamic linker and the compiler will go nuts.
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

static void yoyo_dado ( void );

/*
  Declaration of the static symbols of the plugin. Refer to the docs
  to know what does each of these functions do
*/
gint plugin_get_protocol ( void )
{
  /* You don't need to modify this function */
  return PLUGIN_PROTOCOL;
}

/* Return FALSE if we aren't able to access a symbol, you should only
   need to edit the pointer table, not the code */
gboolean plugin_get_symbol(gchar * name, gint hash, gpointer * ptr)
{
  /* Usually this table is the only thing you will need to change */
  struct plugin_exported_symbol table_of_symbols[] =
  {
    SYMBOL(plugin_get_info, 0x1234),
    SYMBOL(plugin_init, 0x1234),
    SYMBOL(plugin_close, 0x1234),
    SYMBOL(plugin_start, 0x1234),
    SYMBOL(plugin_stop, 0x1234),
    SYMBOL(plugin_load_config, 0x1234),
    SYMBOL(plugin_save_config, 0x1234),
    SYMBOL(plugin_running, 0x1234),
    SYMBOL(plugin_process_sample, 0x1234),
    SYMBOL(plugin_get_public_info, 0x1234),
    /* These three shouldn't be exported, since there are no
       configuration options */
    /*    SYMBOL(plugin_add_properties, 0x1234),
    SYMBOL(plugin_activate_properties, 0x1234),
    SYMBOL(plugin_help_properties, 0x1234),*/
    SYMBOL(plugin_add_gui, 0x1234),
    SYMBOL(plugin_remove_gui, 0x1234),
    SYMBOL(plugin_get_misc_info, 0x1234)
  };
  gint num_exported_symbols =
    sizeof(table_of_symbols)/sizeof(struct plugin_exported_symbol);
  gint i;

  /* Try to find the given symbol in the table of exported symbols
   of the plugin */
  for (i=0; i<num_exported_symbols; i++)
    if (!strcmp(table_of_symbols[i].symbol, name))
      {
	if (table_of_symbols[i].hash != hash)
	  {
	    if (ptr)
	      *ptr = GINT_TO_POINTER(0x3); /* hash collision code */
	    /* Warn */
	    g_warning(_("Check error: \"%s\" in plugin %s"
		       "has hash 0x%x vs. 0x%x"), name,
		      str_canonical_name, 
		      table_of_symbols[i].hash,
		      hash);
	    return FALSE;
	  }
	if (ptr)
	  *ptr = table_of_symbols[i].ptr;
	return TRUE; /* Success */
      }

  if (ptr)
    *ptr = GINT_TO_POINTER(0x2); /* Symbol not found in the plugin */
  return FALSE;
}

static
gboolean plugin_running (void)
{
  /* This will usually be like this too */
  return active;
}

static
void plugin_get_info (gchar ** canonical_name, gchar **
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

static
gboolean plugin_init (PluginBridge bridge, tveng_device_info * info)
{
  /* Do any startup you need here, and return FALSE on error */

  /* If this is set, autostarting is on (we should start now) */
  if (active)
    return plugin_start();
  return TRUE;
}

static
void plugin_close(void)
{
  /* If we were working, stop the work */
  if (active)
    plugin_stop();

  /* Any cleanups would go here (closing fd's and so on) */
}

static
gboolean plugin_start (void)
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

static
void plugin_stop(void)
{
  /* Most times we cannot be stopped while we are stopped */
  if (!active)
    return;

  /* Stop anything the plugin is doing and set the flag */
  active = FALSE;
}

static
void plugin_load_config (gchar * root_key)
{
  gchar * buffer;

  /* The autostart config value is compulsory, you shouldn't need to
     change the following */
  buffer = g_strconcat(root_key, "autostart", NULL);
  /* Create sets a default value for a key, check src/zconf.h */
  zconf_create_boolean(FALSE,
		       "Whether the plugin should start"
		       " automatically when opening Zapping", buffer);
  active = zconf_get_boolean(NULL, buffer);
  g_free(buffer);

  /* Load here any other config key */
}

static
void plugin_save_config (gchar * root_key)
{
  gchar * buffer;

  /* This one is compulsory, you won't need to change it */
  buffer = g_strconcat(root_key, "autostart", NULL);
  zconf_set_boolean(active, buffer);
  g_free(buffer);

  /* Save here any other config keys you need to save */
}

static
void plugin_process_sample(plugin_sample * sample)
{
  /* If the plugin isn't active, it shouldn't do anything */
  if (!active)
    return;

  /* Do any changes to the image here and update the struct
     accordingly */
}

static
void yoyo_dado ( void )
{
  ShowBox("Yoyo-Dado", GNOME_MESSAGE_BOX_INFO);
}


static
gboolean plugin_get_public_info (gint index, gpointer * ptr, gchar **
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

static
void plugin_add_properties ( GnomePropertyBox * gpb )
{
  /* Here you would add a page to the property box. Define this
     function only if you are going to add something to the box */
}

static
gboolean plugin_activate_properties ( GnomePropertyBox * gpb, gint page )
{
  /* Return TRUE only if the given page have been builded by this
     plugin, and apply any config changes here */
  return FALSE;
}

static
gboolean plugin_help_properties ( GnomePropertyBox * gpb, gint page )
{
  /*
    Return TRUE only if the given page have been builded by this
    plugin, and show some help (or at least sth like ShowBox
    "Sorry, but the template plugin doesn't help you").
  */
  return FALSE;
}

static
void plugin_add_gui (GnomeApp * app)
{
  /*
    Define this function only if you are going to do something to the
    main Zapping window.
  */
}

static
void plugin_remove_gui (GnomeApp * app)
{
  /*
    Define this function if you have defined previously plugin_add_gui
   */
}

static
struct plugin_misc_info * plugin_get_misc_info (void)
{
  static struct plugin_misc_info returned_struct =
  {
    sizeof(struct plugin_misc_info), /* size of this struct */
    6, /* plugin priority, this is just an example */
    0 /* Cathegory */
  };

  /*
    Tell that the template plugin should be run with a somewhat high
    priority (just to put an example)
  */
  return (&returned_struct);
}
