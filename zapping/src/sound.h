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
  This module adds sound support for Zapping. It uses esd's
  capabilities.
*/
#ifndef __SOUND_H__
#define __SOUND_H__

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>

/* This struct holds some info about a sound sample */
struct soundinfo
{
  gint buffer_size; /* Actual size of the buffer */
  gint size; /* Number of available bytes in the buffer struct */
  gpointer buffer; /* pointer to the data */
  struct timeval tv; /* When was captured this sample */
  gint rate; /* Sound sampling frequence, usually 44.1 kHz */
  gint bits; /* Bits per sample (usually 16) */
};

/* Start the sound, return FALSE on error */
gboolean startup_sound( void );

/* Shutdown all sound */
void shutdown_sound ( void);

/* Set the timestamps relative to the current time value */
void sound_start_timer ( void );

/* Create the struct needed for communicating with the sound thread */
struct soundinfo * sound_create_struct( void );

/* Free all the mem the struct uses */
void sound_destroy_struct (struct soundinfo * si);

/*
  Read the data captured by the sound capturing thread into the si
  struct. Returns the number of available bytes in the struct, same as
  si.size
*/
gint sound_read_data(struct soundinfo * si);

#endif /* SOUND.H */
