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
 * This code is used to communicate with the VBI device (usually
 * /dev/vbi), so multiple plugins can access to it simultaneously.
 * The code uses libvbi, a nearly verbatim copy of alevt.
 */

#ifndef __ZVBI_H__
#define __ZVBI_H__

#include <gnome.h>
#include <libvbi.h>

/* Open the given VBI device, FALSE on error */
gboolean
zvbi_open_device(gchar * device, gint finetune, gboolean erc);

/* Closes the VBI device */
void
zvbi_close_device(void);

/*
  Returns the global vbi object, or NULL if vbi isn't enabled or
  doesn't work. You can safely use this function to test if VBI works
  (it will return NULL if it doesn't).
*/
struct vbi *
zvbi_get_object(void);

/*
  Returns a pointer to the name of the Teletext provider, or NULL if
  this name is unknown. You must g_free the returned value.
*/
gchar*
zvbi_get_name(void);

/*
  Fills in the given pointers with the time as it appears in the
  header. The pointers can be NULL.
  If the time isn't known, -1 will be returned in all the fields
*/
void
zvbi_get_time(gint * hour, gint * min, gint * sec);

#endif /* zvbi.h */
