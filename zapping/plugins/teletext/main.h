/*
 *  Zapping TV viewer
 *
 *  Copyright (C) 2000, 2001, 2002 Iñaki García Etxebarria
 *  Copyright (C) 2000, 2001, 2002, 2003, 2004 Michael H. Schimek
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* $Id: main.h,v 1.5 2007-08-30 14:14:33 mschimek Exp $ */

#ifndef TELETEXT_MAIN_H
#define TELETEXT_MAIN_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <gtk/gtk.h>
#include "libvbi/teletext_decoder.h"
#include "bookmark.h"

extern vbi3_network		anonymous_network;
extern bookmark_list		bookmarks;
extern BookmarkEditor *		bookmarks_dialog;
extern GtkActionGroup *		teletext_action_group;
extern GList *			teletext_windows;
extern GList *			teletext_views;

#endif /* TELETEXT_MAIN_H */

/*
Local variables:
c-set-style: gnu
c-basic-offset: 2
End:
*/
