/*
 * Screenshot saving plugin for Zapping
 * Copyright (C) 2000-2001 Iñaki García Etxebarria
 * Copyright (C) 2001 Michael H. Schimek
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

#include "screenshot.h"

static gboolean
backend_init (screenshot_data *data, gboolean write, gint quality)
{
  return TRUE;
}

static void
backend_save (screenshot_data *data)
{
  gchar *src, *dest;
  gint src_bpl, dest_bpl, free, n;

  src = (gchar *) data->data;
  src_bpl = data->format.bytesperline;
  dest = data->io_buffer;
  free = data->io_buffer_size;
  dest_bpl = data->format.width * 3;

  g_assert (free > 80);

  n = snprintf (dest, 80, "P6 %d %d 255\n",
		data->format.width, data->format.height);
  dest += n;
  free -= n;

  /* NB lines is evaluated by parent thread to update the progress bar */
  for (data->lines = 0; data->lines < data->format.height; data->lines++)
    {
      if (free < dest_bpl)
	{
	  data->io_flush (data, data->io_buffer_size - free);
	  dest = data->io_buffer;
	  free = data->io_buffer_size;
	}

      if (screenshot_close_everything || data->thread_abort)
	{
	  data->thread_abort = TRUE;
	  break;
	}

      (data->Converter)(data->format.width, src, dest);

      src += src_bpl;
      dest += dest_bpl;
      free -= dest_bpl;
    }

  if (!data->thread_abort && free < data->io_buffer_size)
    data->io_flush (data, data->io_buffer_size - free);
}

screenshot_backend
screenshot_backend_ppm =
{
  .keyword		= "ppm",
  .label		= N_("PPM"),
  .extension		= "ppm",
  .quality		= FALSE,
  .bpp_est		= 3.0,
  .init			= backend_init,
  .save			= backend_save,
};
