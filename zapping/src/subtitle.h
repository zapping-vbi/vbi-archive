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

/* $Id: subtitle.h,v 1.4 2005-01-19 04:16:22 mschimek Exp $ */

#ifndef SUBTITLE_H
#define SUBTITLE_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

extern int zvbi_caption_pgno; /* page for subtitles */

GtkWidget *
subtitle_menu_new		(void);

extern void
shutdown_subtitle		(void);

extern void
startup_subtitle		(void);

G_END_DECLS

#endif /* SUBTITLE_H */
