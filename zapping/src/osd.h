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

/* $Id: osd.h,v 1.2 2001-02-17 22:32:41 garetxe Exp $ */

#ifndef __OSD_H__
#define __OSD_H__

#include "../libvbi/format.h"

void startup_osd(void);
void shutdown_osd(void);

/* Starts showing the OSD window attached to the dest window */
void osd_on(GtkWidget * dest_window);
/* Hides the OSD window */
void osd_off(void);
/* FIXME: osd_geometry_update */

/* See libvbi/caption.c */
void osd_render(attr_char *buffer, int row);
void osd_clear(void);
void osd_roll_up(attr_char *buffer, int first_row, int last_row);

/*
 * Versions to be called by the CC decoder thread. Remember to use
 * these instead of osd_*, those can only be called from the main (aka
 * GTK) thread.
 */
void cc_render(attr_char *buffer, int row);
void cc_clear(void);
void cc_roll_up(attr_char *buffer, int first_row, int last_row);

#endif /* osd.h */
