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
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>
#include <signal.h>
#include <esd.h>
#include <pthread.h>

#define ZCONF_DOMAIN "/zapping/options/sound/"
#include "zmisc.h"
#include "interface.h"
#include "tveng.h"
#include "v4linterface.h"
#include "plugins.h"
#include "zconf.h"
#include "frequencies.h"
#include "sound.h"

struct SoundInfo
{
  pthread_mutex_t mutex; /* Allow multiple connections */
  pthread_t saving_thread; /* Sound recording thread */
  gboolean exit_thread; /* TRUE if shutdown_sound has been called */
  int esd_recording_socket; /* Socket used to communicate with esd */
  int esd_playing_socket; /* Socket used for playing */
};

static struct SoundInfo si;

/* This thread is the responsible of capturing the sound */
gpointer sound_capturing_thread( gpointer data );

/* Opens the esd capture stream, create the thread, and some other
   misc stuff. FALSE on error. */
gboolean startup_sound ( void )
{
  esd_format_t format;
  int rate = 44100;

  format = ESD_STREAM | ESD_RECORD | ESD_MONO | ESD_BITS16;

  si.esd_recording_socket =
    esd_record_stream_fallback(format, rate, NULL, NULL);

  /* I'm not sure which one is the error code */
  if ((si.esd_recording_socket == 0) || (si.esd_recording_socket == -1))
    {
      RunBox("Sound couldn't be opened.",
	     GNOME_MESSAGE_BOX_ERROR);
      return FALSE;
    }

  format = ESD_STREAM | ESD_PLAY | ESD_MONO | ESD_BITS16;

  si.esd_playing_socket =
    esd_play_stream_fallback(format, rate, NULL, NULL);

  /* I'm not sure which one is the error code */
  if ((si.esd_playing_socket == 0) || (si.esd_playing_socket == -1))
    {
      esd_close(si.esd_recording_socket);

      RunBox("Sound couldn't be opened.",
	     GNOME_MESSAGE_BOX_ERROR);
      return FALSE;
    }

  if (pthread_create(&si.saving_thread, NULL, sound_capturing_thread,
		      NULL))
    {
      esd_close(si.esd_recording_socket);
      esd_close(si.esd_playing_socket);

      RunBox("Cannot create a new thread", GNOME_MESSAGE_BOX_ERROR);
      return FALSE;
    }

  si.exit_thread = FALSE;
  pthread_mutex_init(&si.mutex, NULL);

  return TRUE;
}

/* Stops the sound related stuff */
void shutdown_sound ( void )
{
  gpointer returned_pointer;

  si.exit_thread = TRUE;

  /* the thread should exit now */
  pthread_join(si.saving_thread, &returned_pointer);

  pthread_mutex_destroy(&si.mutex);

  esd_close(si.esd_recording_socket);
  esd_close(si.esd_playing_socket);
}

/* This thread is the responsible of capturing the sound */
gpointer sound_capturing_thread( gpointer data )
{
  short buffer[ESD_BUF_SIZE/2];
  int bytes_read;
  fd_set rdset;
  struct timeval timeout;
  int n;

  while (!si.exit_thread)
    {
      FD_ZERO(&rdset);
      FD_SET(si.esd_recording_socket, &rdset);
      timeout.tv_sec = 0;
      timeout.tv_usec = 500000;
      n = select(si.esd_recording_socket+1, &rdset, NULL, NULL,
		 &timeout);
      if ((n == -1) || (n == 0))
	continue;

      bytes_read=read(si.esd_recording_socket, buffer, ESD_BUF_SIZE);

      /* FIXME: Play only if requested by a plugin */
      /* write(si.esd_playing_socket, buffer, bytes_read); */
    }

  return NULL;
}
