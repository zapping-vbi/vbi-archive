/* Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2000 Iñaki García Etxebarria
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

/*
 * These routines handle the capture mode and the Xv stuff.
 */
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include <tveng.h>

/*
 * Inits the capture, setting the given widget as a destination.
 * Note that this only sets up the structs.
 * Returns FALSE on error
 */
gboolean
startup_capture(GtkWidget * widget);

/*
 * Releases the capture structs.
 */
void
shutdown_capture(tveng_device_info * info);

/*
 * Starts capturing to the given widget, returns -1 on error
 */
gint
capture_start(GtkWidget *widget, tveng_device_info * info);

/*
 * Stops capturing
 */
void
capture_stop(tveng_device_info * info);

/*
 * Get one frame, pass it to the plugins, and render it into the
 * widget, if needed.
 */
void
capture_process_frame(GtkWidget * widget, tveng_device_info * info);
