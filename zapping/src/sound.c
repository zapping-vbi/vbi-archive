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

#define MAX_SOUND_MEM 1024*1024 /* each sound buffer should be 1M maximum */

struct soundbuffer
{
  pthread_mutex_t mutex; /* Mutex for this sound buffer */
  int buffer_size; /* Size allocated for the buffer (bytes) */
  int write_pointer; /* How much have we writen up to now */
  gpointer buffer; /* Pointer to the mem that contains the audio data
		    */
  struct timeval tv; /* When did capture of this buffer start */
};

struct SoundInfo
{
  pthread_mutex_t mutex; /* Allow multiple connections */
  pthread_t saving_thread; /* Sound recording thread */
  gboolean exit_thread; /* TRUE if shutdown_sound has been called */
  int esd_recording_socket; /* Socket used to communicate with esd */
  int esd_playing_socket; /* Socket used for playing */
  /* One of these buffers will be used for reading, the other for
     writing */
  struct soundbuffer sb[2];
  gint write_index; /* The buffer the thread is writing to */
  gboolean switch_buffers; /* Tell the capturing thread to switch
			      buffers */
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

  si.exit_thread = FALSE;
  si.write_index = 0; /* Start writing to the sound buffer */
  pthread_mutex_init(&si.mutex, NULL);

  pthread_mutex_init(&si.sb[0].mutex, NULL);
  si.sb[0].buffer = NULL;
  si.sb[0].write_pointer = 0;
  si.sb[0].buffer_size = 0;

  pthread_mutex_init(&si.sb[1].mutex, NULL);
  si.sb[1].buffer = NULL;
  si.sb[1].write_pointer = 0;
  si.sb[1].buffer_size = 0;
  si.switch_buffers = FALSE;

  if (pthread_create(&si.saving_thread, NULL, sound_capturing_thread,
		      NULL))
    {
      esd_close(si.esd_recording_socket);
      esd_close(si.esd_playing_socket);

      RunBox("Cannot create a new thread", GNOME_MESSAGE_BOX_ERROR);
      return FALSE;
    }

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
  pthread_mutex_destroy(&si.sb[0].mutex);
  pthread_mutex_destroy(&si.sb[1].mutex);

  g_free(si.sb[0].buffer);
  g_free(si.sb[1].buffer);

  esd_close(si.esd_recording_socket);
  esd_close(si.esd_playing_socket);
}

/* This thread is the responsible of capturing the sound */
gpointer sound_capturing_thread( gpointer data )
{
  int bytes_read;
  fd_set rdset;
  struct timeval timeout;
  int n;
  int index;

  /* Do the startup */
  index = si.write_index;
  pthread_mutex_lock(&si.sb[index].mutex);
  si.sb[index].write_pointer = 0;
  gettimeofday(&si.sb[index].tv, NULL);

  /* HOWTO: Make si.write_index = 1-si.write_index; si.switch_buffers
     = TRUE; put a lock in
     1-si.write_index and when pthread_mutex_lock returns you can
     start reading from the buffer */
  
  while (!si.exit_thread)
    {
      /* Check if a buffer switch has been requested */
      if (si.switch_buffers)
	{
	  pthread_mutex_lock(&si.sb[si.write_index].mutex);
	  si.sb[si.write_index].write_pointer = 0;
	  gettimeofday(&si.sb[si.write_index].tv, NULL);
	  si.switch_buffers = FALSE;
	  /* Remove the lock from the previous write buffer */
	  pthread_mutex_unlock(&si.sb[index].mutex);
	  index = si.write_index;
	}
      FD_ZERO(&rdset);
      FD_SET(si.esd_recording_socket, &rdset);
      timeout.tv_sec = 0;
      timeout.tv_usec = 500000;
      n = select(si.esd_recording_socket+1, &rdset, NULL, NULL,
		 &timeout);
      if ((n == -1) || (n == 0))
	continue;
      if (si.sb[index].write_pointer + ESD_BUF_SIZE >=
	  si.sb[index].buffer_size)
	{
	  si.sb[index].buffer_size += ESD_BUF_SIZE *4;
	  if (si.sb[index].buffer_size > MAX_SOUND_MEM)	 
	    {
	      si.sb[index].buffer_size -= ESD_BUF_SIZE *4;	      
	      si.sb[index].write_pointer -= ESD_BUF_SIZE;
	    }
	  else
	    si.sb[index].buffer = realloc(si.sb[index].buffer,
					  si.sb[index].buffer_size);
	}
      bytes_read =
	read(si.esd_recording_socket,
	     &(((char*)si.sb[index].buffer)[si.sb[index].write_pointer]),
	     ESD_BUF_SIZE);
      si.sb[index].write_pointer += bytes_read;

      /* FIXME: Play if requested by a plugin */
      /* write(si.esd_playing_socket, buffer, bytes_read); */
    }

  /* Unlock the mutex */
  pthread_mutex_unlock(&(si.sb[index].mutex));

  return NULL;
}

/* Create the struct needed for communicating with the sound thread */
struct soundinfo * sound_create_struct( void )
{
  struct soundinfo * returned_struct =
    (struct soundinfo *) malloc(sizeof(struct soundinfo));
  memset(returned_struct, 0, sizeof(struct soundinfo));

  returned_struct -> rate = 44100;
  returned_struct -> bits = 16;

  return (returned_struct);
}

/* Free all the mem the struct uses */
void sound_destroy_struct (struct soundinfo * info)
{
  g_free(info->buffer);
  g_free(info);
}

/*
  Read the data captured by the sound capturing thread into the si
  struct. Returns the number of available bytes in the struct, same as
  si.size
*/
gint sound_read_data(struct soundinfo * info)
{
  gint index;

  /* Tell the thread to switch buffers */
  si.write_index = 1 - si.write_index;
  si.switch_buffers = TRUE;
  index = 1 - si.write_index;
  pthread_mutex_lock(&(si.sb[index].mutex));

  /* Now it's safe to read the data */
  memcpy(&(info->tv), &(si.sb[index].tv), sizeof(struct timeval));
  if (si.sb[index].write_pointer > info->buffer_size)
    {
      info->buffer_size = si.sb[index].write_pointer * 2;
      info->buffer = realloc(info->buffer, info->buffer_size);
    }

  info->size = si.sb[index].write_pointer;
  memcpy(info->buffer, si.sb[index].buffer, info->size);

  pthread_mutex_unlock(&(si.sb[index].mutex));
  return (info->size);
}

