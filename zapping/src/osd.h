/* Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2001 Iñaki García Etxebarria
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

/* $Id: osd.h,v 1.17.2.1 2002-12-31 00:15:48 garetxe Exp $ */

#ifndef __OSD_H__
#define __OSD_H__

#include "zmodel.h"

void startup_osd(void);
void shutdown_osd(void);

/* Sets the given window as the destination */
void osd_set_window(GtkWidget *dest_window);
/**
 * Like set_window, but lets you specify a subrectangle to use
 */
void osd_set_coords(GtkWidget *dest_window, gint x, gint y, gint w, gint h);
/**
 * Call this when the osd window you've set is going to be destroyed.
 */
void osd_unset_window(void);

/* Clears any OSD text in the window */
void osd_clear(void);

/**
 * Formats and renders the given string that might contain pango
 * markup.
 * @timeout_cb: When given and osd timed out, called with TRUE, when
 *   when given and error or replaced, called with FALSE. 
 * @string: Chars to draw.
 */
void
osd_render_markup	(void (* timeout_cb)(gboolean),
			 const char *string, ...);

/**
 * Like osd_render_pango_markup but no markup parsing is done, the
 * string is printed as is.
 */
void
osd_render		(void (* timeout_cb)(gboolean),
			 const char *string, ...);

extern ZModel *osd_model; /* used for notification of changes */

#endif /* osd.h */






