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

#ifndef __OVERLAY_H__
#define __OVERLAY_H__

/*
 * These routines handle the Windowed overlay mode.
 */

/*
 * Inits the overlay engine.
 * window: The window we will be overlaying to.
 * main_window: Its toplevel parent
 */
void
startup_overlay(GtkWidget * window, GtkWidget * main_window,
		tveng_device_info * info);

/*
 * Stops the overlay engine.
 */
void
overlay_stop(tveng_device_info *info);

/*
 * Shuts down the overlay engine
 */
void
shutdown_overlay(void);

/*
 * Tells the overlay engine to sync the overlay with the window.
 * clean_screen: TRUE means that we should refresh the whole screeen.
 */
void
overlay_sync(gboolean clean_screen);

#endif /* overlay.h */
