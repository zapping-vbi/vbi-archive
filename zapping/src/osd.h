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

/* $Id: osd.h,v 1.11 2001-08-20 17:46:49 mschimek Exp $ */

#ifndef __OSD_H__
#define __OSD_H__

#include "../libvbi/format.h"
#include "../libvbi/libvbi.h"
#include "zmodel.h"

void startup_osd(void);
void shutdown_osd(void);

/* Starts showing the OSD window attached to the dest window */
void osd_on(GtkWidget *dest_window, GtkWidget *parent);
/* Hides the OSD window */
void osd_off(void);
/* Sets the given window as the destination, overrides the coords */
void osd_set_window(GtkWidget *dest_window, GtkWidget *parent);
/* Forces the given coordinates, overrides the dest window */
void osd_set_coords(gint x, gint y, gint w, gint h);

/* See libvbi/caption.c */
void osd_render(void);
void osd_clear(void);
void osd_roll_up(attr_char *buffer, int first_row, int last_row);

void cc_event(vbi_event *ev, void *data);

extern ZModel *osd_model; /* used for notification of changes */

#endif /* osd.h */






