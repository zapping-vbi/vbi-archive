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

#ifndef __ZCONF_H__
#define __ZCONF_H__

#include <gnome.h> /* This file depends on Gnome and glib */

#include <stdio.h>

/* i18n support */
#include "support.h"

/* Possible key types */
enum zconf_type
{
  ZCONF_TYPE_NONE, /* Indicates failure, a key should never have this
		      type */
  ZCONF_TYPE_DIR, /* This key holds no actual value, just subdirs */
  ZCONF_TYPE_INTEGER,
  ZCONF_TYPE_STRING,
  ZCONF_TYPE_FLOAT,
  ZCONF_TYPE_BOOLEAN
};

/* ZConf initing and closing */
/* 
   Starts zconf, and returns FALSE on error.
   domain: Specifies the domain where the config will be saved under
   (usually it's equal to the program name).
*/
gboolean zconf_init(const gchar * domain);

/*
  Closes ZConf, returns FALSE if zconf could not be closed properly
  (usually because it couldn't write the config to disk)
*/
gboolean zconf_close(void);

/*
  Returns the last error zconf generated, 0 means the last operation
  was ok. It is useful for checking ambiguous return values:
  if ((!zconf_get_int(...)) && zconf_error())
     Show_Error_Message();
  else
     Show_Success_Message();
*/
gint zconf_error(void);

/* 
   In zconf there are four operations you can perform for each value:
   get: Gets the key value and fails if it doesn't exist.
   set: Sets a key value, and creates it (undocumented) if doesn't exist. 
   Never fails.
   create: If a key doesn't exist, it creates it with this value,
   if it already exists, it does nothing. Never fails.
   The paths are standard Unix filenames, relative to the domain or
   root dir:
   If the domain is "zapping", paths should be like
    plugins/png_save/interlaced
*/
/*
  Gets an integer value, returns 0 on error (ambiguous). If where
  is not NULL, the value is also stored in the location pointed to by
  where.
*/
gint zconf_get_integer(gint * where, gchar * path);
/*
  Sets an integer value
*/
void zconf_set_integer(gint new_value, gchar * path);
/*
  Sets a default integer value. desc is the description for the
  value. Set it to NULL for leaving it undocumented (a empty string is
  considered as a documented value)
*/
void zconf_create_integer(gint new_value, gchar * desc, gchar * path);

/*
  Gets a string value. The returned string is statically allocated,
  will only be valid until the next call to zconf, so use g_strdup()
  to duplicate it if needed.
  Returns NULL on error (not ambiguous, NULL is always an error).
  if where is not NULL, zconf will g_strdup the string itself, and
  place a pointer to it in where, that should be freed later with g_free.
*/
gchar * zconf_get_string(gchar ** where, gchar * path);

/*
  Sets an string value to the given string. Can fail if the string is
  so large that we cannot g_strdup it. Returns FALSE on failure.
*/
gboolean zconf_set_string(gchar * new_value, gchar * path);

/*
  Creates an string value. Sets desc to NULL to leave it
  undocumented. Can fail if the given string is too large. FALSE on error.
*/
gboolean zconf_create_string(gchar * value, gchar * desc, gchar * path);

/*
  Gets a boolean value. If where is not NULL, the value is also stored
  there. Returns FALSE on error (ambiguous, use zconf_error to check).
*/
gboolean zconf_get_boolean(gboolean * where, gchar * path);

/*
  Sets a boolean value.
*/
void zconf_set_boolean(gboolean new_value, gchar * path);

/*
  Creates a boolean key. Cannot fail.
*/
void zconf_create_boolean(gboolean new_value, gchar * desc, gchar * path);

/*
  Gets a float value. If where is not NULL, the value is also stored
  there. Returns 0.0 on error (ambiguous, use zconf_error to check).
*/
gfloat zconf_get_float(gfloat * where, gchar * path);

/*
  Sets a floating point number. Cannot fail.
*/
void zconf_set_float(gfloat new_value, gchar * path);

/*
  Creates a float key. Cannot fail.
*/
void zconf_create_float(gfloat new_value, gchar * desc, gchar * path);

/*
  Documentation functions.
*/
/*
  Returns the string that describes the given key, NULL if the key is
  undocumented (ambiguous, in case of the key not being found, NULL is
  returned, and zconf_error will return non-zero). If where is not
  NULL, the string will also be stored there (after g_strdup()'ing it)
*/
gchar * zconf_get_description(gchar ** where, gchar * path);

/*
  Sets the string that describes a key.
  Returns FALSE if the key could not be found.
*/
gboolean zconf_set_description(gchar * desc, gchar * path);

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
gchar * zconf_get_nth(gint index, gchar ** where, gchar * path);

/*
  Removes a key from the database. All the keys descendant from this
  one will be erased too. Fails if it cannot find the given key.
*/
gboolean zconf_delete(gchar * path);

/*
  Returns the type of the given key. It fails if the key doesn't
  exist. Failure is indicated by a return value of ZCONF_TYPE_NONE.
*/
enum zconf_type zconf_get_type(gchar * key);

/*
  zconf_set_type doesn't exist, if you wish to change the type of a
  key, you must zconf_set_[type] it, assigning thus a valid value to
  it. The old key, whatever its type and content was, is erased. Its
  description is kept untouched.
*/

#endif /* ZCONF.H */



