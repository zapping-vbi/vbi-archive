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

/* $Id: osd.h,v 1.1 2001-02-17 15:23:02 garetxe Exp $ */

#ifndef __OSD_H__
#define __OSD_H__

#include "../libvbi/format.h"

void startup_osd(void);
void shutdown_osd(void);

void osd_on(GtkWidget * dest_window);
void osd_off(void);

/* See libvbi/caption.c */
void osd_render(attr_char *buffer, int row);
void osd_clear(void);
void osd_roll_up(attr_char *buffer, int first_row, int last_row);

#endif /* osd.h */
