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

/* $Id: subtitle.h,v 1.1 2004-09-22 21:29:15 mschimek Exp $ */

#ifndef SUBTITLE_H
#define SUBTITLE_H

#include "config.h"

#include <gtk/gtk.h>
#include <libzvbi.h>

G_BEGIN_DECLS

GtkWidget *
subtitles_menu_new		(void);

#ifdef HAVE_LIBZVBI
extern vbi_pgno
find_subtitle_page		(void);
#endif

G_END_DECLS

#endif /* SUBTITLE_H */
