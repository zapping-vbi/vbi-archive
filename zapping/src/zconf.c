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
  Rationale:
  Here are the routines to deal with configuration. It may seem a bit
  strange defining a new module for this, the usual thing is to manage
  the config directly.
  The thing is that i would like to port Zapping to use the new gconf
  engine, but it is not finished yet, so a compatibility layer must be
  created.
  This way the code looks much cleaner, and is easier to maintain,
  since i get rid of globals, and don't need hacks to update the
  config if needed
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>
#include <gnome-xml/tree.h>
#include <gnome-xml/parser.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include "zconf.h"
#include "zmisc.h" /* Misc common stuff */

/* Possible key types */
enum zconf_key_type
{
  ZCONF_TYPE_NONE, /* Indicates failure, the type should never be set
		      to this */
  ZCONF_TYPE_INTEGER,
  ZCONF_TYPE_STRING,
  ZCONF_TYPE_FLOAT,
  ZCONF_TYPE_BOOLEAN
};

/*
  This defines a key in the configuration tree.
*/
struct zconf_key
{
  enum zconf_key_type type;
  gchar * name; /* The name of the key */
  gchar * descriptioon; /* description for the key */
  gpointer content; /* Its content */
  GList * tree; /* A list to the children of the key */
};

/* Global values that control the library */
struct zconf_key * zconf_root = NULL; /* The root of the config tree
				       */
gboolean zconf_started = FALSE; /* indicates whether zconf has been
				   successfully started */

gchar * zconf_file = NULL;

gboolean zconf_we = FALSE; /* TRUE if the last call failed */

/*
  Configuration saving/loading functions.
*/
/* Inits the config saving module. Returns FALSE on error (probably
   because ENOMEM). Error here is critical (the calling program should
   exit) */
gboolean
zconf_init(const gchar * domain)
{
  /* Hold here the home dir */
  gchar * home_dir = g_get_home_dir();
  gchar * buffer = NULL; /* temporal storage */
  DIR * config_dir;
  xmlDocPtr doc;

  zconf_we = TRUE;

  /* We don't want to init zconf twice */
  if (zconf_started)
    return TRUE;

  if (home_dir == NULL)
    {
      buffer = g_strconcat(domain,
/* For translators: This will be precceeded by the domain (program) name */
			   _(" cannot determine your home dir"), NULL);
      ShowBox(buffer, GNOME_MESSAGE_BOX_ERROR);
      g_free(buffer);
      return FALSE;
    }

  /* OK, test if the config dir (.domain) exists */
  buffer = g_strconcat(homedir, "/.", domain, NULL);
  config_dir = opendir(buffer);
  if (!config_dir) /* No config dir, try to create one */
    {
      if (mkdir(buffer, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP |
		S_IXGRP | S_IROTH | S_XOTH) == -1)
	{
	  ShowBox(_("Cannot create config home dir, check your permissions"),
		  GNOME_MESSAGE_BOX_ERROR);
	  g_free(buffer);
	  return FALSE;
	}
      config_dir = opendir(buffer);
      if (!config_dir)
	{
	  ShowBox(_("Cannot open config home dir, this is weird"),
		  GNOME_MESSAGE_BOX_ERROR);
	  g_free(buffer);
	  return FALSE;
	}
    }

  /* Open the config doc, .domain/domain.conf */
  closedir(config_dir); /* Close the directory first */
  g_free(buffer);
  buffer = g_strconcat(homedir, "/.", domain, "/", domain, ".conf",
		       NULL);
  if (!buffer)
    return FALSE;

  /* This is blocking, and will freeze the GUI, but shouldn't take
     long */
  zconf_file = g_strdup(buffer); 
  doc = xmlParseFile(zconf_file);

  if (!doc)
    {
      /* We couldn't open the doc, create a new one */
      doc = xmlNewDoc("1.0");
      if (!doc)
	return FALSE;
      doc->root = xmlNewDocNode(doc, NULL, _("Zapping Configuration"),
				NULL);
    }

  /* Build the config tree from this entry */
  p_zconf_parse(xmlDocGetRootElement(doc), zconf_root);

  xmlFreeDoc(doc); /* Free the memory, we don't need it any more */
  gfree(buffer);
  gfree(home_dir);

  zconf_started = TRUE; /* Yes, we have started */
  zonf_we = FALSE;
  return TRUE;
}

/*
  Closes ZConf, returns FALSE if zconf could not be closed properly
  (usually because it couldn't write the config to disk)
*/
gboolean zconf_close()
{
  xmlDocPtr doc;
  doc = xmlNewDoc("1.0");

  zconf_we = TRUE;

  if (!doc)
    return FALSE; /* Error, we cannot create the doc*/

  /* We build the doc now */
  p_zconf_write(xmlDocGetRootElement(doc), zconf_root);

  /* and we destroy the zconf tree */
  zconf_root = p_zconf_cut_branch(zconf_root);

  if (xmlSaveFile(zconf_file, doc) == -1)
    {
      ShowBox(_("Zapping cannot save configuration to disk\n"
		"You should have write permission to your home dir..."),
	      GNOME_MESSAGE_BOX_ERROR);
      return FALSE; /* Error */
    }

  xmlFreeDoc(doc); /* Free memory */
  g_free(zconf_file);

  zconf_started = FALSE; /* We are ended now */

  zconf_we = FALSE;
  return TRUE;
}

/*
  Returns the last error zconf generated, 0 means the last operation
  was ok. It is useful for checking ambiguous return values:
  if ((!zconf_get_int(...)) && zconf_error())
     Show_Error_Message();
  else
     Show_Success_Message();
*/
gint zconf_error()
{
  /* currently there is only two error states, 0 and 1 */
  return (zconf_we);
}
