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

#if HAVE_ARTS

/** aRts (KDE sound server) backend **/

#include <gnome.h>
#include <math.h>
#include <unistd.h>

#include "audio.h"
#define ZCONF_DOMAIN "/zapping/options/audio/"
#include "zconf.h"
#include "zmisc.h"

/** artsc, aka "porting to arts for dummies" :-) **/
#include <artsc.h>

typedef struct {
  arts_stream_t		stream;
  int			stereo;
  int			sampling_rate;
  double		time;
  double		buffer_period_near;
  double		buffer_period_far;
} arts_handle;

static gpointer
arts_open (gboolean stereo, guint rate, enum audio_format format)
{
  arts_handle *h;
  arts_stream_t stream;
  gint errcode;

  /* FIXME: Improve error reporting */

  if (format != AUDIO_FORMAT_S16_LE)
    {
      g_warning("Requested audio format won't work");
      return NULL;
    }

  if ((errcode = arts_init()))
    {
      /* FIXME: memleak? */
      g_warning("arts_init: %s", arts_error_text(errcode));
      return NULL;
    }

  stream = arts_record_stream ((int) rate, 16, (!!stereo)+1, "Zapping");

  /* FIXME: can this really fail? */
  if (!stream)
    {
      g_warning("Cannot open recording stream");
      goto fail;
    }

  h = g_malloc0(sizeof(arts_handle));

  h->stream = stream;
  h->stereo = !!stereo;
  h->sampling_rate = rate;

  h->time = 0.0;

  return h;

 fail:
  arts_free();
  return NULL;
}

static void
arts_close (gpointer handle)
{
  arts_handle *h = handle;

  arts_close_stream(h->stream);
  arts_free();

  g_free(handle);
}

static void
_arts_read (gpointer handle, gpointer dest, guint num_bytes,
	    double *timestamp)
{
  arts_handle *h = handle;
  unsigned char *p;
  ssize_t r, n;
  struct timeval tv;
  double now;

  for (p = dest, n = num_bytes; n > 0;)
    {
      r = arts_read(h->stream, p, n);
      
      if (r < 0)
	{
	  g_warning("ARTS: READ ERROR, quitting: %s",
		    arts_error_text(r));
	  memset(p, 0, (unsigned int) n);
	  break;
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
}

const audio_backend_info arts_backend =
{
  name:		"KDE Sound Server (aRts)",
  open:		arts_open,
  close:	arts_close,
  read:		_arts_read,
};

#endif /* HAVE_ARTS */
