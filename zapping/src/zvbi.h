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

/*
  Returns the specified page. Use ANY_SUB for accessing the most
  recent subpage, its number otherwise.
  Returns the page on success, NULL otherwise
  The returned struct doesn't need to be freed
*/
struct vt_page *
zvbi_get_page(gint page, gint subpage);

/*
  Stores in the given location the formatted version of the given
  page. use ANY_SUB for accessing the most recent subpage, its
  number otherwise.
  Returns TRUE if the page has been rendered properly, FALSE if it
  wasn't in the cache.
  Reveal specifies whether to render hidden text or not, usually FALSE
*/
gboolean
zvbi_format_page(gint page, gint subpage, gboolean reveal,
		 struct fmt_page *pg);

/*
  Renders the given formatted page into a paletted buffer. Each byte
  in the buffer represents 1-byte, and the palette is as follows:
                      {  0,  0,  0},
		      {255,  0,  0},
		      {  0,255,  0},
		      {255,255,  0},
		      {  0,  0,255},
		      {255,  0,255},
		      {  0,255,255},
		      {255,255,255}
   width and height are pointers where the dimensions of the rendered
   buffer will be stored. They cannot be NULL.
   Returns the allocated buffer (width*height bytes), or NULL on
   error.
   The allocated buffer should be free'd, not g_free'd, since it's
   allocated with malloc. NULL on error.
*/
unsigned char*
zvbi_render_page(struct fmt_page *pg, gint *width, gint *height);

/*
  Renders the given formatted page into a RGBA buffer.
  alpha is a 8-entry unsigned char array specifying the alpha values
  for the 8 different colours.
  The same is just like in zvbi_render_page.
*/
unsigned char*
zvbi_render_page_rgba(struct fmt_page *pg, gint *width, gint *height,
		      unsigned char * alpha);
#endif /* zvbi.h */
