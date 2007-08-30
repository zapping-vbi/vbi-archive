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
#  include "config.h"
#endif

#include <gnome.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <locale.h>
#include "zconf.h"
#include "zmisc.h" /* Misc common stuff */
#include "zmodel.h"
#include "globals.h"

/*
  This defines a key in the configuration tree.
*/
struct zconf_key
{
  enum zconf_type type;
  gchar * name; /* The name of the key */
  gchar * full_path; /* Full path to this key */
  gchar * description; /* description for the key */
  union
  {
    gint integer;
    gchar * string;
    gfloat floating;
    gboolean boolean;
  } contents; /* Its contents */
  struct zconf_key * parent; /* A pointer to the parent node */
  GList * tree; /* A list to the children of the key */
  ZModel * model; /* for hooks */
  GList * hooked; /* struct zconf_hook */
};

/*
  Connected to the key
*/
struct zconf_hook
{
  ZConfHook	callback;
  gchar		*key; /* Key this hook is attached to */
  gpointer	data;
};

/* Global values that control the library */
static struct zconf_key * zconf_root = NULL; /* The root of the config tree
				       */
static gboolean zconf_started = FALSE; /* indicates whether zconf has been
					  successfully started */

static gchar * zconf_file = NULL;

static gboolean zconf_we = FALSE; /* TRUE if the last call failed */

static gchar * zconf_buffer = NULL; /* A global buffer some functions share */

/*
  We should use namespaces, but the code in this file is for
  transitional purpouses only, gconf will be used when gconf comes.
  Don't relay on it too much, i don't plan to maintain it...
*/

/*
  List of functions that need to be implemented (privately)
*/
/*
  Translates a xml doc to a zconf_key value, creating subtrees as
  necessary. Allocates memory for key and modifies its value too.
*/
static void
p_zconf_parse(xmlNodePtr node, xmlDocPtr doc, struct zconf_key ** key,
	      struct zconf_key * parent);

/*
  Translates a zconf_key value with its children to a xml doc.
*/
static void
p_zconf_write(xmlNodePtr node, xmlDocPtr doc, struct zconf_key * key);

/*
  "Cuts a branch" of the zconf tree. Frees all the mem from the given
  key and all the mem of its children.
  NOTE: Not very fast, if I have spare time (:DDDDDDDDDD) i should
  speed this up a bit. Always returns NULL
*/
static gpointer
p_zconf_cut_branch(struct zconf_key * key);

/*
  Resolves the given path, returning the key associated to the (relative)
  path. It will return NULL if the key is not found
*/
static struct zconf_key*
p_zconf_resolve(const gchar * key, struct zconf_key * starting_dir);

/*
  Given a path, creates all the needed nodes that don't exist to make
  the path a valid (relative to starting_dir) one. Never fails, and
  returns the last node (if path is xxx/yyy/zzz, returns a pointer to zzz).
*/
static struct zconf_key*
p_zconf_create(const gchar * key, struct zconf_key * starting_dir);

/**
 * Sets the env vars describing the locale to "C", saves the results for
 * a env_restore later on. Workaround for a bug in glib:
 * printf uses the locale info to print float numbers (z.b., M_PI
 * printed under a spanish locale is 3,14159... not 3.14159...). atof
 * assumes the current locale is "C" when scanning floats.
 */
static gchar *old_locale = NULL;

static void
env_C(void)
{
  old_locale = g_strdup(setlocale(LC_ALL, NULL));

  setlocale(LC_ALL, "C");
}

static void
env_restore(void)
{
  if (!old_locale)
    return;

  setlocale(LC_ALL, old_locale);

  g_free(old_locale);
  old_locale = NULL;
}

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
  const gchar * home_dir = g_get_home_dir();
  gchar * buffer = NULL; /* temporal storage */
  DIR * config_dir;
  xmlDocPtr doc;
  xmlNodePtr root_node; /* The root node of the tree */
  xmlNodePtr new_node; /* The "General" node */

  zconf_we = TRUE;

  /* We don't want to init zconf twice */
  if (zconf_started)
    return TRUE;

  if (home_dir == NULL)
    {
      buffer = g_strconcat(domain,
			   " cannot determine your home dir", NULL);
      RunBox("%s", GTK_MESSAGE_ERROR, buffer);
      g_free(buffer);
      return FALSE;
    }

  /* OK, test if the config dir (.domain) exists */
  buffer = g_strconcat(home_dir, "/.", domain, NULL);
  config_dir = opendir(buffer);
  if (!config_dir) /* No config dir, try to create one */
    {
      if (mkdir(buffer, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP |
		S_IXGRP | S_IROTH | S_IXOTH) == -1)
	{
	  RunBox("Cannot create config home dir, check your permissions",
		  GTK_MESSAGE_ERROR);
	  g_free(buffer);
	  return FALSE;
	}
      config_dir = opendir(buffer);
      if (!config_dir)
	{
	  RunBox("Cannot open config home dir, this is weird",
		  GTK_MESSAGE_ERROR);
	  g_free(buffer);
	  return FALSE;
	}
    }

  /* Open the config doc, .domain/domain.conf */
  closedir(config_dir); /* Close the directory first */
  g_free(buffer);
  buffer = g_strconcat(home_dir, "/.", domain, "/", domain, ".conf",
		       NULL);
  if (!buffer)
    return FALSE;

  /* This is blocking, and will freeze the GUI, but shouldn't take
     long */
  zconf_file = g_strdup(buffer); 
  doc = NULL;
  if (g_file_test (zconf_file, G_FILE_TEST_EXISTS)) {
    /* Spits out an error message if the file does not exist, hence. */
    doc = xmlParseFile(zconf_file);
  }

  if (!doc) /* There isn't any doc to parse */
    {
      /* We couldn't open the doc, create a new one */
      doc = xmlNewDoc("1.0");
      if (!doc)
	{
	  g_warning("ZConf: xmlNewDoc failed");
	  return FALSE;
	}

      /* Add the main node to the tree, so the parser has something to
	 parse */
      root_node = xmlNewDocNode(doc, NULL, "Configuration", NULL);

      /* This will be the root node */
      xmlDocSetRootElement(doc, root_node);

      /* Add the "General entry" */
      new_node = xmlNewChild(root_node, NULL, "subtree", NULL);
      xmlSetProp(new_node, "label", domain);
      xmlSetProp(new_node, "type", "directory");
      xmlSetProp(new_node, "description", "Root Node");
    }

  /* Build the config tree from this entry */
  /* Examine the childs until we have a valid zconf_ root node */
  new_node = xmlDocGetRootElement(doc)-> children;
  env_C();
  while ((new_node) && (!zconf_root))
    {
      p_zconf_parse(new_node, doc,
		    &zconf_root, NULL);
      new_node = new_node -> next;
    }
  env_restore();

  /* No root node found, return error */
  if (zconf_root == NULL)
    {
      g_warning("ZConf: No root node found!");
      return FALSE;
    }

  if (doc)
    xmlFreeDoc(doc); /* Free the memory, we don't need it any more */

  g_free(buffer);

  zconf_buffer = NULL;

  zconf_started = TRUE; /* Yes, we have started */
  zconf_we = FALSE;
  return TRUE;
}

/*
  Closes ZConf, returns FALSE if zconf could not be closed properly
  (usually because it couldn't write the config to disk)
*/
gboolean zconf_close(void)
{
  xmlDocPtr doc;
  xmlNodePtr root_node;

  zconf_we = TRUE;

  doc = xmlNewDoc("1.0");
  if (!doc)
    return FALSE;

  root_node = xmlNewDocNode(doc, NULL, "Configuration", NULL);
  xmlDocSetRootElement(doc, root_node);
  
  /* We build the doc now */
  env_C();
  p_zconf_write(xmlDocGetRootElement(doc), doc, zconf_root);
  env_restore();

  /* and we destroy the zconf tree */
  zconf_root = p_zconf_cut_branch(zconf_root);

  if (xmlSaveFormatFile(zconf_file, doc, TRUE) == -1)
    {
      ShowBox("Zapping cannot save configuration to disk\n"
	      "You should have write permissions to your home dir...",
	      GTK_MESSAGE_ERROR);
      return FALSE; /* Error */
    }

  xmlFreeDoc(doc); /* Free memory */
  g_free(zconf_file);

  if (zconf_buffer) /* free this mem too */
    g_free(zconf_buffer);

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
gint zconf_error(void)
{
  /* currently there is only two error states, 0 and 1 */
  return (zconf_we);
}

static gboolean
p_zconf_copy			(const gchar *		dst_path,
				 struct zconf_key *	src_key)
{
  struct zconf_key *dst_key;
  gchar *new_path;

  new_path = g_strconcat (dst_path, "/", src_key->name, NULL);

  dst_key = p_zconf_resolve (new_path, zconf_root);

  if (NULL == dst_key)
    {
      /* The value doesn't exist yet, create it */
      dst_key = p_zconf_create (new_path, zconf_root);
      dst_key->type = src_key->type;
    }
  else
    {
      g_assert (dst_key->type == src_key->type);
    }

  switch (src_key->type)
    {
      GList *p;

    case ZCONF_TYPE_DIR:
      for (p = src_key->tree; p; p = p->next)
	{
	  struct zconf_key *child_key = (struct zconf_key *) p->data;

	  if (!p_zconf_copy (new_path, child_key))
	    goto failure;
	}
      break;

    case ZCONF_TYPE_INTEGER:
      if (dst_key->contents.integer != src_key->contents.integer)
	{
	  dst_key->contents.integer = src_key->contents.integer;

	  if (dst_key->model)
	    zmodel_changed (dst_key->model);
	}
      break;

    case ZCONF_TYPE_STRING:
      if (NULL == dst_key->contents.string)
	{
	  /* Is a new key. */
	  dst_key->contents.string = g_strdup (src_key->contents.string);
	}
      else if (0 != strcmp (dst_key->contents.string,
			    src_key->contents.string))
	{
	  g_free (dst_key->contents.string);

	  dst_key->contents.string = g_strdup (src_key->contents.string);

	  if (dst_key->model)
	    zmodel_changed (dst_key->model);
	}
      break;

    case ZCONF_TYPE_FLOAT:
      if (dst_key->contents.floating != src_key->contents.floating)
	{
	  dst_key->contents.floating = src_key->contents.floating;

	  if (dst_key->model)
	    zmodel_changed (dst_key->model);
	}
      break;

    case ZCONF_TYPE_BOOLEAN:
      if (dst_key->contents.boolean != src_key->contents.boolean)
	{
	  dst_key->contents.boolean = src_key->contents.boolean;

	  if (dst_key->model)
	    zmodel_changed (dst_key->model);
	}
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  g_free (new_path);
  new_path = NULL;

  return TRUE;

 failure:
  g_free (new_path);
  new_path = NULL;

  return FALSE;
}

/* Copies the key src into dir dst. Copies recursively if src is
   a dir. dst must be a dir, if it exists. */
gboolean
zconf_copy			(const gchar *		dst_path,
				 const gchar *		src_path)
{
  struct zconf_key *dst_key;
  struct zconf_key *src_key;

  g_assert (NULL != dst_path);
  g_assert (NULL != src_path);

  dst_key = p_zconf_resolve (dst_path, zconf_root);

  if (NULL == dst_key)
    {
      /* The value doesn't exist yet, create it */
      dst_key = p_zconf_create (dst_path, zconf_root);
      dst_key->type = ZCONF_TYPE_DIR;
    }
  else
    {
      g_assert (ZCONF_TYPE_DIR == dst_key->type);
    }

  src_key = p_zconf_resolve (src_path, zconf_root);

  if (NULL == src_key)
    {
      /* Doesn't exist. */
      zconf_we = FALSE;
    }
  else
    {
      zconf_we = !p_zconf_copy (dst_path, src_key);
    }

  return !zconf_we;
}

#define GET_VALUE(_type, _field, _nothing)				\
  struct zconf_key *key;						\
  zconf_we = TRUE; /* start with an error */				\
  g_assert (NULL != path);						\
  g_assert (zconf_started);						\
  g_assert (NULL != zconf_root);					\
  key = p_zconf_resolve (path, zconf_root);				\
  if (where)								\
    *where = _nothing;							\
  if (!key)								\
    return _nothing;							\
  if (_type != key->type)						\
    return _nothing;							\
  if (where)								\
    *where = key->contents._field;					\
  zconf_we = FALSE;							\
  return key->contents._field;

#define SET_VALUE(_type, _field)					\
  struct zconf_key *key;						\
  zconf_we = TRUE; /* start with an error */				\
  g_assert (NULL != path);						\
  g_assert (zconf_started);						\
  g_assert (NULL != zconf_root);					\
  if (!(key = p_zconf_resolve (path, zconf_root)))			\
    {									\
      key = p_zconf_create (path, zconf_root);				\
      key->type = _type;						\
    }									\
  else if (_type != key->type)						\
    {									\
      if (ZCONF_TYPE_DIR != key->type)					\
	g_warning ("Changing the type of %s from %s to %s.",		\
		   path, zconf_type_string (key->type),			\
		   zconf_type_string (_type));				\
      if (ZCONF_TYPE_STRING == key->type)				\
	{								\
	  g_free (key->contents.string);				\
	  key->contents.string = NULL;					\
	}								\
      key->type = _type;						\
    }									\
  if (ZCONF_TYPE_STRING != _type					\
      && new_value != key->contents._field)				\
    {									\
      key->contents._field = new_value;					\
      if (key->model)							\
	zmodel_changed (key->model);					\
    }									\
  zconf_we = FALSE;

#define CREATE_VALUE(_type, _field)					\
  struct zconf_key *key;						\
  zconf_we = TRUE; /* start with an error */				\
  g_assert (NULL != path);						\
  g_assert (zconf_started);						\
  g_assert (NULL != zconf_root);					\
  if (!(key = p_zconf_resolve (path, zconf_root)))			\
    {									\
      key = p_zconf_create (path, zconf_root);				\
      key->type = _type;						\
      key->contents._field = new_value;					\
    }									\
  else if (_type != key->type)						\
    {									\
      if (ZCONF_TYPE_DIR != key->type)					\
	g_warning ("Changing the type of %s from %s to %s.",		\
		   path, zconf_type_string (key->type),			\
		   zconf_type_string (_type));				\
      if (ZCONF_TYPE_STRING == key->type)				\
	g_free (key->contents.string);					\
      key->type = _type;						\
      key->contents._field = new_value;					\
    }									\
  if (desc && !key->description)					\
    key->description = g_strdup (desc);					\
  zconf_we = FALSE;

/*
  Gets an integer value, returns 0 on error (ambiguous). If where
  is not NULL, the value is also stored in the location pointed to by
  where.
*/
gint
zconf_get_int			(gint *			where,
				 const gchar *		path)
{
  GET_VALUE (ZCONF_TYPE_INTEGER, integer, 0);
}

/*
  Sets an integer value, creating the path to it if needed.
*/
void
zconf_set_int			(gint			new_value,
				 const gchar *		path)
{
  SET_VALUE (ZCONF_TYPE_INTEGER, integer);
}

/*
  Sets a default integer value. desc is the description for the
  value. Set it to NULL for leaving it undocumented (a empty string is
  considered as a documented value)
*/
void zconf_create_int(gint new_value, const gchar * desc,
			  const gchar * path)
{
  CREATE_VALUE (ZCONF_TYPE_INTEGER, integer);
}

guint zconf_get_uint(guint * where, const gchar * path)
{
  return (guint) zconf_get_int ((gint *) where, path);
}

void zconf_set_uint(guint new_value, const gchar * path)
{
  zconf_set_int ((gint) new_value, path);
}

void zconf_create_uint(guint new_value, const gchar * desc,
		       const gchar * path)
{
  zconf_create_int ((gint) new_value, desc, path);
}

/*
  Gets a string value. The returned string is statically allocated,
  will only be valid until the next call to zconf, so use g_strdup()
  to duplicate it if needed.
  Returns NULL on error (not ambiguous, NULL is always an error).
  if where is not NULL, zconf will g_strdup the string itself, and
  place a pointer to it in where, that should be freed later with g_free.
*/
const gchar *
zconf_get_string		(gchar **		where,
				 const gchar *		path)
{
  struct zconf_key *key;						

  zconf_we = TRUE; /* start with an error */				

  g_assert (NULL != path);						
  g_assert (zconf_started);						
  g_assert (NULL != zconf_root);					

  key = p_zconf_resolve (path, zconf_root);				

  if (where)								
    *where = NULL;

  if (!key)								
    return NULL;

  if (ZCONF_TYPE_STRING != key->type)
    return NULL;

  if (where)								
    *where = g_strdup (key->contents.string);

  zconf_we = FALSE;							

  return key->contents.string;
}

/*
  Sets an string value to the given string.
*/
void
zconf_set_string		(const gchar *		new_value,
				 const gchar *		path)
{
  struct zconf_key *key;						

  zconf_we = TRUE; /* start with an error */				

  g_assert (NULL != path);						
  g_assert (zconf_started);						
  g_assert (NULL != zconf_root);					

  if (!(key = p_zconf_resolve (path, zconf_root)))			
    {									
      key = p_zconf_create (path, zconf_root);				
      key->type = ZCONF_TYPE_STRING;
    }									
  else if (ZCONF_TYPE_STRING != key->type)
    {									
      if (ZCONF_TYPE_DIR != key->type)					
	g_warning ("Changing the type of %s from %s to %s.",		
		   path, zconf_type_string (key->type),			
		   zconf_type_string (ZCONF_TYPE_STRING));

      key->type = ZCONF_TYPE_STRING;
      key->contents.string = NULL;
    }									

  if (NULL == key->contents.string /* type changed to string */	
      || 0 != strcmp (key->contents.string, new_value))		
    {								
      g_free (key->contents.string);				
      key->contents.string = g_strdup (new_value ? new_value : "");	
	
      if (key->model)						
	zmodel_changed (key->model);				
    }

  zconf_we = FALSE;
}

/*
  Creates an string value. Sets desc to NULL to leave it
  undocumented. Can fail if the given string is too large. FALSE on error.
*/
void
zconf_create_string		(const gchar *		new_value,
				 const gchar *		desc,
				 const gchar *		path)
{
  struct zconf_key *key;						

  zconf_we = TRUE; /* start with an error */				

  g_assert (NULL != path);						
  g_assert (zconf_started);						
  g_assert (NULL != zconf_root);					

  if (!(key = p_zconf_resolve (path, zconf_root)))			
    {									
      key = p_zconf_create (path, zconf_root);				

      key->type = ZCONF_TYPE_STRING;
      key->contents.string = g_strdup (new_value);			
    }									
  else if (ZCONF_TYPE_STRING != key->type)
    {									
      if (ZCONF_TYPE_DIR != key->type)					
	g_warning ("Changing the type of %s from %s to %s.",		
		   path, zconf_type_string (key->type),			
		   zconf_type_string (ZCONF_TYPE_STRING));

      key->type = ZCONF_TYPE_STRING;
      key->contents.string = g_strdup (new_value);			
    }

  if (desc && !key->description)					
    key->description = g_strdup (desc);					

  zconf_we = FALSE;
}

/*
  Gets a boolean value. If where is not NULL, the value is also stored
  there. Returns FALSE on error (ambiguous, use zconf_error to check).
*/
gboolean
zconf_get_boolean		(gboolean *		where,
				 const gchar *		path)
{
  GET_VALUE (ZCONF_TYPE_BOOLEAN, boolean, FALSE);
}

/*
  Sets a boolean value.
*/
void
zconf_set_boolean		(gboolean		new_value,
				 const gchar *		path)
{
  SET_VALUE (ZCONF_TYPE_BOOLEAN, boolean);
}

/*
  Creates a boolean key. Cannot fail.
*/
void zconf_create_boolean(gboolean new_value, const gchar * desc,
			  const gchar * path)
{
  CREATE_VALUE (ZCONF_TYPE_BOOLEAN, boolean);
}

/*
  Gets a float value. If where is not NULL, the value is also stored
  there. Returns 0.0 on error (ambiguous, use zconf_error to check).
*/
gfloat
zconf_get_float			(gfloat *		where,
				 const gchar *		path)
{
  GET_VALUE (ZCONF_TYPE_FLOAT, floating, 0.0);
}

/*
  Sets a floating point number. Cannot fail.
*/
void zconf_set_float(gfloat new_value, const gchar * path)
{
  SET_VALUE (ZCONF_TYPE_FLOAT, floating);
}

/*
  Creates a float key. Cannot fail.
*/
void zconf_create_float(gfloat new_value, const gchar * desc,
			const gchar * path)
{
  CREATE_VALUE (ZCONF_TYPE_FLOAT, floating);
}

/*
  Documentation functions.
*/
/*
  Returns the string that describes the given key, NULL if the key is
  undocumented (ambiguous, in case of the key not being found, NULL is
  returned, and zconf_error will return non-zero). If where is not
  NULL, the string will also be stored there (after g_strdup()'ing it)
*/
const gchar *
zconf_get_description		(gchar **		where,
				 const gchar *		path)
{
  struct zconf_key * key;
  zconf_we = TRUE; /* Start with an error */

  g_assert(path != NULL);
  g_assert(zconf_started == TRUE);
  g_assert(zconf_root != NULL);

  key = p_zconf_resolve(path, zconf_root);

  if (!key)
    return NULL;

  if (where)
    {
      *where = NULL;

      if (NULL != key->description)
	*where = g_strdup (key->description);
    }

  zconf_we = FALSE;

  return key->description;
}

/*
  Sets the string that describes a key. Returns FALSE if the key could
  not be found.
*/
gboolean zconf_set_description(const gchar * desc, const gchar * path)
{
  struct zconf_key * key;
  zconf_we = TRUE; /* Start with an error */

  g_assert(path != NULL);
  g_assert(zconf_started == TRUE);
  g_assert(zconf_root != NULL);
  g_assert(desc != NULL);

  key = p_zconf_resolve(path, zconf_root);

  if (!key)
    return FALSE;

  /* If there was any previous description erase it */
  if (key -> description)
    g_free(key->description);

  key->description = g_strdup(desc);

  if (key->description == NULL)
    return FALSE;

  zconf_we = FALSE;
  return TRUE;
}

/*
  Value and type querying and erasing functions.
*/
/*
  Gets the name of the nth entry in the subdirectory. Call it starting
  from 0 and incrementing one by one until it gives an error (returns
  NULL) to query the contents of a key (a key is a directory AND a
  file). The returned pointer is a pointer to a statically allocated
  string, and it will only be valid until the next zconf call. if
  where is not NULL, a new copy will be g_strdup()'ed there.
*/
const gchar *
zconf_get_nth			(guint			index,
				 gchar **		where,
				 const gchar *		path)
{
  struct zconf_key * key;
  struct zconf_key * subkey; /* The found key */

  zconf_we = TRUE; /* Start with an error */

  g_assert(path != NULL);
  g_assert(*path != 0);
  g_assert(zconf_started == TRUE);
  g_assert(zconf_root != NULL);

  key = p_zconf_resolve(path, zconf_root);

  if (!key)
    return NULL;

  if (zconf_buffer)
    {
      g_free(zconf_buffer);
      zconf_buffer = NULL;
    }

  subkey = (struct zconf_key *) g_list_nth_data(key->tree, index);

  /* Check that we have something to access */
  if (!subkey)
    return NULL;

  if (path[strlen(path)-1] != '/')
    zconf_buffer = g_strconcat(path, "/", subkey->name, NULL);
  else
    zconf_buffer = g_strconcat(path, subkey->name, NULL);

  if (!zconf_buffer)
    return NULL;

  if (where != NULL)
    *where = g_strdup(zconf_buffer);

  zconf_we = FALSE;
  return zconf_buffer;
}

/*
  Removes a key from the database. All the keys descendant from this
  one will be erased too. Fails if it cannot find the given key.
*/
gboolean zconf_delete(const gchar * path)
{
  struct zconf_key * key;
  zconf_we = TRUE; /* Start with an error */

  g_assert(path != NULL);
  g_assert(zconf_started == TRUE);
  g_assert(zconf_root != NULL);

  key = p_zconf_resolve(path, zconf_root);

  if (!key)
    return FALSE;

  /* Cut the brach */
  p_zconf_cut_branch(key);

  zconf_we = FALSE;
  return TRUE;
}

/*
  Returns the type of the given key. It fails if the key doesn't
  exist. Failure is indicated by a return value of ZCONF_TYPE_NONE.
*/
enum zconf_type zconf_get_type(const gchar * path)
{
  struct zconf_key * key;
  zconf_we = TRUE; /* Start with an error */

  g_assert(path != NULL);
  g_assert(zconf_started == TRUE);
  g_assert(zconf_root != NULL);

  key = p_zconf_resolve(path, zconf_root);

  if (!key)
    return ZCONF_TYPE_NONE;

  zconf_we = FALSE;
  return key->type;
}

/*
  Translates the given key type to a string. This string is statically
  allocated, it may be overwritten the next time you call any zconf
  function. Always succeeds (returns [Unknown] if the type is unknown :-)
*/
const gchar *
zconf_type_string(enum zconf_type type)
{
  switch (type)
    {
    case ZCONF_TYPE_INTEGER:
      return "Integer";
    case ZCONF_TYPE_STRING:
      return "String";
    case ZCONF_TYPE_FLOAT:
      return "Float";
    case ZCONF_TYPE_BOOLEAN:
      return "Boolean";
    case ZCONF_TYPE_DIR:
      return "Directory";
    case ZCONF_TYPE_NONE:
      return "Undefined";
    default:
      return "Unknown";
    }
}

/*
  zconf_set_type doesn't exist, if you wish to change the type of a
  key, you must zconf_set_[type] it, assigning thus a valid value to
  it. The old key, whatever its type and content was, is erased. Its
  description is kept untouched.
*/

/*
  PRIVATE FUNCTIONS, JUST FOR THIS IMPLEMENTATION.
*/
/*
  Translates a xml doc to a zconf_key value, creating subtrees as
  necessary. Allocates memory for key and modifies its value too.
*/
static void
p_zconf_parse(xmlNodePtr node, xmlDocPtr doc, struct zconf_key ** skey,
	      struct zconf_key * parent)
{
  struct zconf_key * new_key; /* The new key we are creating */
  xmlNodePtr p; /* For iterating */
  xmlChar * name; /* The name of this node */
  xmlChar * type; /* The type of the key */
  xmlChar * description; /* The description for the key */
  xmlChar * node_string; /* The string content of this node */
  gchar * full_name;
  enum zconf_type key_type;

  g_assert(node != NULL);
  g_assert(doc != NULL);

  /* Get the (string) contents of the node */
  node_string = xmlNodeListGetString(doc, node->children, 1);

  name = xmlGetProp(node, "label");
  if (!name)
    {
      xmlFree(node_string);
      return; /* Doesn't have a name, it is not valid */
    }

  /* Create the full path to this key */
  if (parent)
    full_name=g_strconcat(parent->full_path, "/", name, NULL);
  else
    full_name=g_strconcat("/", name, NULL);

  description = xmlGetProp(node, "description");

  type = xmlGetProp(node, "type");
  if (!type)
    {
      xmlFree(name);
      xmlFree(node_string);
      xmlFree(description);
      g_free(full_name);
      return;
    }

  key_type = ZCONF_TYPE_NONE;

  if (!strcasecmp(type, "Directory"))
    key_type = ZCONF_TYPE_DIR;
  else if (!strcasecmp(type, "Integer"))
    key_type = ZCONF_TYPE_INTEGER;
  else if (!strcasecmp(type, "Float"))
    key_type = ZCONF_TYPE_FLOAT;
  else if (!strcasecmp(type, "Boolean"))
    key_type = ZCONF_TYPE_BOOLEAN;
  else if (!strcasecmp(type, "String"))
    key_type = ZCONF_TYPE_STRING;
  else /* Error */
    {
      xmlFree(type);
      g_free(full_name);
      xmlFree(name);
      xmlFree(node_string);
      xmlFree(description);
      g_warning("The specified type \"%s\" is unknown",
		type);
      return;
    }

  xmlFree(type);

  /* Check if that node already exists */
  if (p_zconf_resolve(full_name, zconf_root))
    {
      /* It already exists, warn */
      g_warning("Duplicated node in the config tree: %s",
		full_name);
      xmlFree(name);
      g_free(full_name);
      xmlFree(node_string);
      xmlFree(description);
      return;
    }

  /* Create and clear the struct */
  new_key = g_malloc0(sizeof(*new_key));

  new_key -> parent = parent;
  new_key -> full_path = full_name;
  new_key -> name = g_strdup(name);
  xmlFree(name);
  new_key -> type = key_type;
  if (description)
    {
      new_key -> description = g_strdup(description);
      xmlFree(description);
    }

  if ((!node_string) && (key_type != ZCONF_TYPE_DIR))
    {
      g_free(new_key->description);
      g_free(new_key->full_path);
      g_free(new_key->name);
      g_free(new_key);
      return;
    }

  switch (key_type) /* Get the value */
    {
    case ZCONF_TYPE_BOOLEAN:
      new_key->contents.boolean = atoi(node_string);
      break;
    case ZCONF_TYPE_INTEGER:
      new_key->contents.integer = atoi(node_string);
      break;
    case ZCONF_TYPE_FLOAT:
      new_key->contents.floating = atof(node_string);
      break;
    case ZCONF_TYPE_STRING:
      new_key->contents.string = g_strdup(node_string);
      break;
    case ZCONF_TYPE_DIR: /* Nothing to be done here */
      break;
    default:
      g_assert_not_reached();
      break;
    }

  /* Lets attach the new node to the parent */
  if (parent)
    parent -> tree = g_list_append(parent->tree, new_key);

  /* We leave the rest of the fields empty (NULL) */
  p = node->children;
  while (p) /* Iterate through all the nodes, creating subtrees */
    {
      if (!strcasecmp(p->name, "subtree"))
	p_zconf_parse(p, doc, NULL, new_key); /* Parse this one */

      p = p->next;
    }

  if (skey) /* store the created key */
    *skey = new_key;

  /* Free the used mem */
  xmlFree(node_string);
}

/*
  Translates a zconf_key value with its children to a xml doc.
*/
static void
p_zconf_write(xmlNodePtr node, xmlDocPtr doc, struct zconf_key * key)
{
  GList * sub_key;
  xmlNodePtr new_node; /* The node we are about to add */
  gchar * str_value = NULL; /* Value in plain text */
  const gchar * str_type = NULL; /* String describing the type */

  g_assert(key != NULL);
  g_assert(doc != NULL);
  g_assert(node != NULL);
  g_assert(key -> name != NULL);

  switch (key -> type)
    {
    case ZCONF_TYPE_DIR:
      str_type = "directory";
      break;
    case ZCONF_TYPE_INTEGER:
      str_type = "integer";
      str_value = g_strdup_printf("%d", key->contents.integer);
      break;
    case ZCONF_TYPE_BOOLEAN:
      str_type = "boolean";
      str_value = g_strdup_printf("%d", (gint) key->contents.boolean);
      break;
    case ZCONF_TYPE_FLOAT:
      str_type = "float";
      str_value = g_strdup_printf("%g", key->contents.floating);
      break;
    case ZCONF_TYPE_STRING:
      str_type = "string";
      str_value = g_strdup(key->contents.string);
      break;
    default:
      g_assert_not_reached();
      break;
    }

  /* Write this node to the XML tree and go recursive */
  new_node = xmlNewChild(node, NULL, "subtree", str_value);
  xmlSetProp(new_node, "label", key -> name);
  xmlSetProp(new_node, "type", str_type);
  if (key -> description)
    xmlSetProp(new_node, "description", key -> description);

  sub_key = g_list_first(key -> tree);

  /* Now go recursive for the rest of the children */
  while (sub_key)
    {
      p_zconf_write(new_node, doc, (struct zconf_key*) sub_key -> data);
      sub_key = sub_key -> next;
    }

  /* Free the mem */
  g_free(str_value);
}

/*
  "Cuts a branch" of the zconf tree. Frees all the mem from the given
  key and all the mem of its children.
  NOTE: Not very fast, if I have spare time (:DDDDDDDDDD) i should
  speed this up a bit. Always returns NULL
*/
static gpointer
p_zconf_cut_branch(struct zconf_key * key)
{
  GList * p;

  g_assert(key != NULL);
  g_assert(key -> name != NULL);
  g_assert(key -> full_path != NULL);

  g_free (key->name);
  g_free (key->full_path);

  if (ZCONF_TYPE_STRING == key->type)
    g_free (key->contents.string);

  g_free (key->description);

  if (key->model)
    g_object_unref(G_OBJECT(key->model));

  for (p = g_list_first(key->hooked); p; p = p->next)
    {
      struct zconf_hook *hook = p->data;
      g_free(hook->key);
      g_free(hook);
    }

  g_list_free(key->hooked);
  
  /* Call this recursively for our children */
  do
    {
      p = g_list_first(key -> tree );
      if (p)
	p_zconf_cut_branch((struct zconf_key*) p->data);
    } while (p);

  /* Remove ourselves from our parent */
  if (key->parent)
    key->parent->tree = g_list_remove(key->parent->tree, key);

  g_list_free(key -> tree);

  /* Free the key itself */
  g_free(key);
  
  return NULL; /* This simplifies a bit programming */
}

/*
  Resolves the given path, returning the key associated to the (relative)
  path. It will return NULL if the key is not found
*/
static struct zconf_key*
p_zconf_resolve(const gchar * key, struct zconf_key * starting_dir)
{
  gchar key_name[256]; /* The key name */
  GList * p; /* For getting the name */
  struct zconf_key * sub_key; /* For traversing */
  gint i = 0;
  gboolean last_item=FALSE; /* We have got the last item in the path */

  g_assert(key != NULL);

  /* We cannot walk on the ether */
  if (starting_dir == NULL)
    return NULL;

  key_name[255] = 0;

  /* Remove starting slashes, but warn */
  while (key[0] == '/')
    {
      if (i > 0)
	g_warning("Removing %d consecutive slashes, this shouldn't happen",
		  i+1);
      i++;
      key++;
    }

  i = 0;

  /* Get the name */
  while ((key[i] != '/') && (key[i] != 0) && (i < 255))
    {
      key_name[i] = key[i];
      i++;
    }

  if (i == 255)
    {
      g_warning("Path item too long: %s", key);
      return NULL;
    }

  /* Finish the string */
  key_name[i] = 0;

  /* Check whether this is the last item in the list */
  if (((key[i] == '/') && (key[i+1] == 0)) || (key[i] == 0))
    last_item = TRUE;

  /* Check that the current item has the correct name */
  if (strcasecmp(starting_dir -> name, key_name))
    return NULL; /* This is not the correct way */

  /* We found it */
  if (last_item)
    return starting_dir;

  /* It is not the last one, check the childs */
  p = g_list_first(starting_dir -> tree);
  g_assert(key[i] == '/');

  while (p)
    {
      sub_key = p_zconf_resolve(&(key[i+1]), (struct zconf_key *)
				p->data);
      if (sub_key) /* We got it */
	return sub_key;

      /* Continue searching */
      p = p->next;
    }

  return NULL;
}

/*
  Given a path, creates all the needed nodes that don't exist to make
  the path a valid (relative to starting_dir) one. Never fails, and
  returns the last node (if path is xxx/yyy/zzz, returns a pointer to zzz).
*/
static struct zconf_key *
p_zconf_create(const gchar * key, struct zconf_key * starting_dir)
{
  gchar key_name[256], key_name_2[256]; /* The key name */
  struct zconf_key * sub_key; /* For traversing */
  GList * p; /* For traversing */
  gint i = 0, j;
  gboolean last_item=FALSE; /* We have got the last item in the path */
  gchar * ptr; /* A pointer to some text, it isn't really neccessary */

  g_assert(key != NULL);
  g_assert(starting_dir != NULL);

  key_name[255] = key_name_2[255] = 0;

  /* Remove starting slashes, but warn */
  while (key[0] == '/')
    {
      if (i > 0)
	g_warning("Removing %d consecutive slashes, this shouldn't happen", 
		  i+1);
      i++;
      key++;
    }

  i = 0; /* Start again */

  /* Get the name */
  while ((key[i] != '/') && (key[i] != 0) && (i < 255))
    {
      key_name[i] = key[i];
      i++;
    }

  if (i == 255)
    {
      g_warning("Path item too long: %s", key);
      return NULL;
    }

  /* Finish the string */
  key_name[i] = 0;

  /* Remove the slash if it is present */
  if (((key[i] == '/') && (key[i+1] == 0)) || (key[i] == 0))
    last_item = TRUE;

  /* The name of the starting dir should match the given one */
  if (strcasecmp(starting_dir->name, key_name))
    {
      g_warning("%s isn't your domain name, zconf will exit now",
		key_name);
      g_assert_not_reached();
    }

  if (last_item)
    return starting_dir; /* We are done */

  /* Get the next item in the path */
  i++;
  j = i; /* Save a copy of i */
  while (key[j] == '/')
    j++;
  i = 0;

  /* Get the name */
  while ((key[i+j] != '/') && (key[i+j] != 0) && (i < 255))
    {
      key_name_2[i] = key[i+j];
      i++;
    }

  if (i == 255)
    {
      g_warning("Path item too long: %s", key);
      return NULL;
    }

  /* Finish the string */
  key_name_2[i] = 0;  

  /* Search in the childs of this node for the next item in the path
   */
  p = starting_dir -> tree;
  while (p)
    {
      sub_key = (struct zconf_key*) p->data;
      if (!strcasecmp(sub_key->name, key_name_2))
	{
	  /* OK, we don't need to create this one */
	  return p_zconf_create(&(key[j]), sub_key);
	}
      p = p->next;
    }

  /* The child didn't exist, create it */
  sub_key = g_malloc0(sizeof(*sub_key));

  sub_key -> name = g_strdup(key_name_2);
  ptr = starting_dir -> full_path;
  g_assert(ptr != NULL);
  if (strlen(ptr) == 0)
    sub_key -> full_path = g_strconcat("/", key_name_2, NULL);
  else if (ptr[strlen(ptr)-1] == '/')
    sub_key -> full_path = g_strconcat(ptr, key_name_2, NULL);
  else
    sub_key -> full_path = g_strconcat(ptr, "/", key_name_2, NULL);

  sub_key -> type = ZCONF_TYPE_DIR;
  /* Set the parent <-> child relation */
  sub_key -> parent = starting_dir;
  starting_dir -> tree = g_list_append(starting_dir -> tree, sub_key);

  /* Go on with the recursion */
  return p_zconf_create(&(key[j]), sub_key);
}

/* HOOKS HANDLING */
static
void on_key_model_changed		(GObject	*model,
					 struct	zconf_hook *hook)
{
  struct zconf_key *key = g_object_get_data(model, "key");

  g_assert(key != NULL);
  g_assert(hook != NULL);
  g_assert(hook->callback != NULL);

  if (ZCONF_TYPE_STRING == key->type)
    hook->callback (key->full_path, key->contents.string, hook->data);
  else
    hook->callback (key->full_path, &key->contents, hook->data);
}

static struct zconf_hook*
real_add_hook(const gchar * key_name, ZConfHook callback, gpointer data,
	      gboolean wakeup)
{
  struct zconf_key *key;
  struct zconf_hook *hook;

  g_assert(key_name != NULL);
  g_assert(callback != NULL);

  key = p_zconf_resolve(key_name, zconf_root);

  if (!key)
    {
      g_warning("\"%s\" not found while trying to add hook for it", key_name);
      return NULL;
    }

  if (!key->model)
    {
      key->model = ZMODEL(zmodel_new());
      g_object_set_data(G_OBJECT(key->model), "key", key);
    }

  hook = g_malloc(sizeof(struct zconf_hook));

  hook->callback = callback;
  hook->data = data;
  hook->key = g_strdup(key_name);

  key->hooked = g_list_append(key->hooked, hook);

  g_signal_connect(G_OBJECT(key->model), "changed",
		   G_CALLBACK(on_key_model_changed),
		   hook);

  if (wakeup)
    {
      if (ZCONF_TYPE_STRING == key->type)
	callback (key->full_path, key->contents.string, data);
      else
	callback (key->full_path, &key->contents, data);
    }

  return hook;
}

void
zconf_add_hook(const gchar * key_name, ZConfHook callback, gpointer data)
{
  real_add_hook(key_name, callback, data, FALSE);
}

static void
on_object_hook_destroyed	(GObject	*object,
				 struct zconf_hook *hook)
{
  struct zconf_key *key;

  key = p_zconf_resolve(hook->key, zconf_root);

  if (!key)
    {
      g_warning("Destroying %p: Cannot find key for hook %p: [%s]",
		object, hook, hook->key);
      return;
    }

  zconf_remove_hook(hook->key, hook->callback, hook->data);
}

void
zconf_add_hook_while_alive(GObject *object,
			   const gchar * key_name,
			   ZConfHook callback,
			   gpointer data,
			   gboolean wakeup)
{
  struct zconf_hook *hook =
    real_add_hook(key_name, callback, data, wakeup);

  if (!hook)
    return;

  g_signal_connect(object, "destroy",
		   G_CALLBACK(on_object_hook_destroyed),
		   hook);
}

void
zconf_remove_hook(const gchar * key_name, ZConfHook callback, gpointer data)
{
  struct zconf_key *key;
  struct zconf_hook *hook = NULL;
  GList *p;

  g_assert(key_name != NULL);
  g_assert(callback != NULL);

  key = p_zconf_resolve(key_name, zconf_root);

  if (!key)
    {
      g_warning("\"%s\" not found while trying to remove hook from it",
		key_name);
      return;
    }

  if (!key->model)
    {
      g_warning("While removing \"%s\": no hooks for that", key_name);
      return;
    }

  for (p = g_list_first(key->hooked); p; p = p->next)
    {
      if ((((struct zconf_hook*)p->data)->callback == callback) &&
	  (((struct zconf_hook*)p->data)->data == data))
	hook = p->data;
    }

  if (!hook)
    {
      g_warning("Cannot find hook for %p in %s", callback, key_name);
      return;
    }

  g_free(hook->key);
  key->hooked = g_list_remove(key->hooked, hook);

  g_signal_handlers_disconnect_matched (key->model,
					G_SIGNAL_MATCH_FUNC |
					G_SIGNAL_MATCH_DATA,
					0, 0, NULL,
					on_key_model_changed,
					hook);

  g_free(hook);
}

void
zconf_touch(const gchar * key_name)
{
  struct zconf_key *key;

  g_assert(key_name != NULL);

  key = p_zconf_resolve(key_name, zconf_root);

  if (!key)
    {
      g_warning("\"%s\" not found while trying to touch it",
		key_name);
      return;
    }

  if (!key->model)
    return;

  zmodel_changed(key->model);
}

/*
 *  Generic hooks
 */

void
zconf_hook_widget_show		(const gchar *		key _unused_,
				 gpointer		new_value_ptr,
				 gpointer		user_data)
{
  gboolean show = * (gboolean *) new_value_ptr;
  GtkWidget *widget = user_data;

  if (show)
    gtk_widget_show (widget);
  else
    gtk_widget_hide (widget);
}

void
zconf_hook_toggle_button	(const gchar *		key _unused_,
				 gpointer		new_value_ptr,
				 gpointer		user_data)
{
  gboolean active = * (gboolean *) new_value_ptr;
  GtkToggleButton *toggle_button = user_data;

  if (active != gtk_toggle_button_get_active (toggle_button))
    gtk_toggle_button_set_active (toggle_button, active);
}

void
zconf_hook_check_menu		(const gchar *		key _unused_,
				 gpointer		new_value_ptr,
				 gpointer		user_data)
{
  gboolean active = * (gboolean *) new_value_ptr;
  GtkCheckMenuItem *item = user_data;

  if (active != item->active)
    gtk_check_menu_item_set_active (item, active);
}

/*
Local variables:
c-set-style: gnu
c-basic-offset: 2
End:
*/
