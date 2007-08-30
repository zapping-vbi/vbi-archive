/*
 *  Zapping TV viewer
 *
 *  Copyright (C) 2000-2002 Iñaki García Etxebarria
 *  Copyright (C) 2000-2005 Michael H. Schimek
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

/* $Id: main.h,v 1.2 2007-08-30 14:14:32 mschimek Exp $ */

#ifndef SUBTITLE_MAIN_H
#define SUBTITLE_MAIN_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

extern GList *			subtitle_views;

extern gboolean
plugin_get_symbol		(const gchar *		name,
				 gint			hash,
				 gpointer *		ptr);
extern gint
plugin_get_protocol		(void);

#endif /* SUBTITLE_MAIN_H */

/*
Local variables:
c-set-style: gnu
c-basic-offset: 2
End:
*/
