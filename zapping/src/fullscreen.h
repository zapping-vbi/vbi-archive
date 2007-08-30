/* Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2000-2001 Iñaki García Etxebarria
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

#ifndef FULLSCREEN_H
#define FULLSCREEN_H

extern GdkPixbuf *
fullscreen_get_subtitle_image	(GdkRectangle *		expose,
				 guint			width,
				 guint			height);
extern gboolean
fullscreen_activate_subtitles	(gboolean		active);
extern gboolean
start_fullscreen		(display_mode		dmode,
				 capture_mode		cmode);
extern gboolean
stop_fullscreen			(void);

#endif /* FULLSCREEN_H */

/*
Local variables:
c-set-style: gnu
c-basic-offset: 2
End:
*/
