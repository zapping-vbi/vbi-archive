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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef HAVE_ESD

#include <gnome.h>
#include <math.h>
#include <unistd.h>

#include "audio.h"
#define ZCONF_DOMAIN "/zapping/options/audio/"
#include "zconf.h"
#include "interface.h"
#include "zmisc.h"

#include <esd.h>

typedef struct {
  int		socket;
  int		stereo;
  int		sampling_rate;
  double	time;
  double	buffer_period_near;
  double	buffer_period_far;
  gboolean	write;
} esd_handle;

/** ESD backend ***/
static gpointer
_open				(gboolean		stereo,
				 guint			sampling_rate,
				 enum audio_format	format,
				 gboolean		write)
{
  esd_format_t fmt;
  esd_handle *h;

  /* FIXME: Improve error reporting */

  if (format != AUDIO_FORMAT_S16_LE)
    {
      g_warning("Requested audio format won't work");
      return NULL;
    }

  h = (esd_handle *) g_malloc0(sizeof(esd_handle));

  if (write)
    {
      GnomeProgram *program;
      GValue espeaker;

      fmt = ESD_STREAM | ESD_PLAY | ESD_BITS16
	| (stereo ? ESD_STEREO : ESD_MONO);

      /* Use --espeaker parameter or NULL for
	 default host ($DISPLAY, localhost). */
      program = gnome_program_get ();
      CLEAR (espeaker);
      g_value_init (&espeaker, G_TYPE_STRING);
      g_object_get_property (G_OBJECT (program),
			     GNOME_PARAM_ESPEAKER, &espeaker);

      h->socket = esd_play_stream_fallback (fmt, (int) sampling_rate,
					    g_value_get_string (&espeaker),
					    /* name */ NULL);
      g_value_unset (&espeaker);
    }
  else
    {
      fmt = ESD_STREAM | ESD_RECORD | ESD_BITS16
	| (stereo ? ESD_STEREO : ESD_MONO);

      h->socket = esd_record_stream_fallback (fmt, (int) sampling_rate,
					      /* host: default */ NULL,
					      /* name */ NULL);
    }

  if (h->socket < 0)
    {
      g_warning("Cannot open ESD play/record socket");
      g_free(h);
      return NULL;
    }

  h->sampling_rate = sampling_rate;
  h->stereo = stereo;
  h->write = write;

  h->time = 0.0;

  return h;
}

static void
_close (gpointer handle)
{
  esd_handle *h = (esd_handle *) handle;

  close(h->socket);

  g_free(handle);
}

static gboolean
_read (gpointer handle, gpointer dest, guint num_bytes,
       double *timestamp)
{
  esd_handle *h = (esd_handle *) handle;
  struct timeval tv;
  unsigned char *p;
  ssize_t r, n;
  double now;

  g_assert (!h->write);

  for (p = (unsigned char *) dest, n = num_bytes; n > 0;)
    {
      fd_set rdset;
      
      FD_ZERO(&rdset);
      FD_SET(h->socket, &rdset);
      tv.tv_sec = 2;
      tv.tv_usec = 0;
      r = select(h->socket+1, &rdset,
		 NULL, NULL, &tv);

      /* FIXME */
      if (r == 0)
	g_error("ESD read timeout");
      else if (r < 0)
	g_error("ESD select error %d (%s)",
		errno, strerror(errno));

      r = read(h->socket, p, (size_t) n);
      
      if (r < 0)
	{
	  g_assert(errno == EINTR);
	  continue;
	}

      p += r;
      n -= r;
    }

  gettimeofday(&tv, NULL);
  now = tv.tv_sec + tv.tv_usec * (1 / 1e6);

  if (h->time > 0.0)
    {
      double dt = now - h->time;
      double ddt = h->buffer_period_far - dt;

      if (fabs(h->buffer_period_near)
	  < h->buffer_period_far * 1.5)
	{
	  h->buffer_period_near =
	    (h->buffer_period_near - dt) * 0.8 + dt;
	  h->buffer_period_far = ddt * 0.9999 + dt;
	  *timestamp = h->time += h->buffer_period_far;
	} 
      else 
	{
	  h->buffer_period_near = h->buffer_period_far;
	  *timestamp = h->time = now;
	}
    } 
  else 
    {
      *timestamp = h->time = now;
      
      /* XXX assuming num_bytes won't change */
      h->buffer_period_near =
	h->buffer_period_far =
          num_bytes / (double)(h->sampling_rate * 2 << h->stereo);
    }

  return TRUE;
}

static gboolean
_write (gpointer handle, gpointer src, guint num_bytes,
	double timestamp)
{
  esd_handle *h = (esd_handle *) handle;
  unsigned char *p;
  ssize_t r, n;

  timestamp = timestamp;

  g_assert (h->write);

  p = src;
  n = num_bytes;

  while (n > 0)
    {
      r = write (h->socket, p, (size_t) n);

      if (r < 0)
	{
	  int saved_errno = errno;

	  if (EINTR == saved_errno)
	    continue;

	  g_error("ESD write error %d (%s)",
		  saved_errno, strerror (saved_errno));

	  return FALSE;
	}

      p += r;
      n -= r;
    }

  return TRUE;
}

const audio_backend_info esd_backend =
{
  name:		"Enlightened Sound Daemon",
  open:		_open,
  close:	_close,
  read:		_read,
  write:	_write,
};

#endif /* HAVE_ESD */
