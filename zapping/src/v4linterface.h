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

#ifndef __V4LINTERFACE_H__
#define __V4LINTERFACE_H__

#include "zmodel.h"

/* 
   Creates a control box suited for setting up all the controls this
   device can have
*/
GtkWidget * create_control_box(tveng_device_info * info);

/*
  Rebuilds the control box if it's open. Call whenever the device
  controls change.
*/
void
update_control_box(tveng_device_info * info);

/* Notification of standard/input changes */
extern ZModel *z_input_model;

/**
 * Sets the given input, based on its hash.
 */
void
z_switch_input			(int hash, tveng_device_info *info);

/**
 * Sets the given standard, based on its hash.
 */
void
z_switch_standard		(int hash, tveng_device_info *info);

/* Do the startup/shutdown */
void startup_v4linterface	(tveng_device_info *info);
void shutdown_v4linterface	(void);

#endif
