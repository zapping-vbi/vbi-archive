/*
 *  Zapping (TV viewer for the Gnome Desktop)
 *
 * Copyright (C) 2001 Iñaki García Etxebarria
 * Copyright (C) 2003 Michael H. Schimek
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

/* $Id: subtitle.h,v 1.5 2005-09-01 01:31:09 mschimek Exp $ */

#ifndef SUBTITLE_H
#define SUBTITLE_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <gtk/gtk.h>
#include "libvbi/exp-gfx.h"
#include "plugins/subtitle/view.h"

G_BEGIN_DECLS

extern int zvbi_caption_pgno; /* page for subtitles */

extern void
subt_store_position_in_config	(SubtitleView *		view,
				 const gchar *		path);
extern void
subt_set_position_from_config	(SubtitleView *		view,
				 const gchar *		path);

extern guint
zvbi_menu_shell_insert_active_subtitle_pages
				(GtkMenuShell *		menu,
				 gint			position,
				 vbi3_pgno		curr_pgno,
				 gboolean		separator_above,
				 gboolean		separator_below);
extern GtkWidget *
zvbi_subtitle_menu_new		(vbi3_pgno		curr_pgno);

extern void
shutdown_subtitle		(void);

extern void
startup_subtitle		(void);

G_END_DECLS

#endif /* SUBTITLE_H */
