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

#ifdef HAVE_LIBJPEG

/* avoid redefinition warning */
#undef HAVE_STDLIB_H
#undef HAVE_STDDEF_H
#undef HAVE_PROTOTYPES
#include <jpeglib.h>
#if 0
#define PARENT(_ptr, _type, _member)					\
	({ char *_p = (char *)(_ptr); (_p != 0) ?			\
	  (_type *)(_p - offsetof (_type, _member)) : (_type *) 0; })
#endif

struct backend_private {
  struct jpeg_compress_struct	cinfo;	/* Compression parameters */
  struct jpeg_decompress_struct	dinfo;	/* Decompression parameters */
  struct jpeg_destination_mgr	dest;	/* Output handler */
  struct jpeg_source_mgr	src;	/* Input handler */
  struct jpeg_error_mgr		jerr;	/* Error handler */
};

static void
jpeg_mydest_init_destination (j_compress_ptr cinfo)
{
}

static boolean
jpeg_mydest_empty_output_buffer (j_compress_ptr cinfo)
{
  backend_private *priv = PARENT(cinfo, backend_private, cinfo);
  screenshot_data *data = PARENT(priv, screenshot_data, private);

  data->io_flush (data, data->io_buffer_size);

  priv->dest.next_output_byte = data->io_buffer;
  priv->dest.free_in_buffer = data->io_buffer_size;

  return TRUE;
} 

static void
jpeg_mydest_term_destination (j_compress_ptr cinfo)
{
  backend_private *priv = PARENT(cinfo, backend_private, cinfo);
  screenshot_data *data = PARENT(priv, screenshot_data, private);
  gint size = data->io_buffer_size - priv->dest.free_in_buffer;

  data->io_flush (data, size);
}

static void
jpeg_mysrc_init_source (j_decompress_ptr cinfo)
{
}

static boolean
jpeg_mysrc_fill_input_buffer (j_decompress_ptr cinfo)
{
  g_assert_not_reached ();
  return TRUE;
}

static void
jpeg_mysrc_skip_input_data (j_decompress_ptr cinfo, long num_bytes)
{
  backend_private *priv = PARENT(cinfo, backend_private, cinfo);
  screenshot_data *data = PARENT(priv, screenshot_data, private);

  g_assert (num_bytes < priv->src.bytes_in_buffer);

  priv->src.next_input_byte += num_bytes;
  priv->src.bytes_in_buffer -= num_bytes;
}

static void
jpeg_mysrc_term_source (j_decompress_ptr cinfo)
{
}

static gboolean
backend_init (screenshot_data *data, gboolean write,
	      gint quality)
{
  backend_private *priv = (backend_private *) &data->private;

  if (write)
    {
      /* Error handler XXX */
      priv->cinfo.err = jpeg_std_error (&priv->jerr);

      jpeg_create_compress(&priv->cinfo);

      /* Output handler */
      priv->dest.next_output_byte = data->io_buffer;
      priv->dest.free_in_buffer = data->io_buffer_size;
      priv->dest.init_destination = jpeg_mydest_init_destination;
      priv->dest.empty_output_buffer = jpeg_mydest_empty_output_buffer;
      priv->dest.term_destination = jpeg_mydest_term_destination;
      priv->cinfo.dest = &priv->dest;

      /* Compression parameters */
      priv->cinfo.image_width = data->format.width;
      priv->cinfo.image_height = data->format.height;
      priv->cinfo.input_components = 3;
      priv->cinfo.in_color_space = JCS_RGB;
      jpeg_set_defaults (&priv->cinfo);
      jpeg_set_quality (&priv->cinfo, quality, TRUE);

      jpeg_start_compress (&priv->cinfo, TRUE);
    }
  else /* read */
    {
      /* Error handler XXX */
      priv->dinfo.err = jpeg_std_error (&priv->jerr);

      jpeg_create_decompress (&priv->dinfo);

      /* Input handler */
      priv->src.next_input_byte = data->io_buffer;
      priv->src.bytes_in_buffer = data->io_buffer_used;
      priv->src.init_source = jpeg_mysrc_init_source;
      priv->src.fill_input_buffer = jpeg_mysrc_fill_input_buffer; 
      priv->src.skip_input_data = jpeg_mysrc_skip_input_data; 
      priv->src.resync_to_restart = jpeg_resync_to_restart;
      priv->src.term_source = jpeg_mysrc_term_source;
      priv->dinfo.src = &priv->src;

      /* Decompression parameters */
      jpeg_read_header (&priv->dinfo, TRUE);

      jpeg_start_decompress (&priv->dinfo);
    }

  return TRUE;
}

static void
backend_save (screenshot_data *data)
{
  backend_private *priv = (backend_private *) &data->private;
  gchar *pixels;
  gint rowstride;

  pixels = (gchar *) data->data.linear.data;
  rowstride = data->data.linear.stride;

  /* NB lines is evaluated by parent thread to update the progress bar */
  for (data->lines = 0; data->lines < data->format.height; data->lines++)
    {
      if (screenshot_close_everything || data->thread_abort)
	{
	  data->thread_abort = TRUE;
	  break;
	}

      jpeg_write_scanlines (&priv->cinfo, (JSAMPROW *) &pixels, 1);

      pixels += rowstride;
    }

  if (data->lines >= data->format.height)
    jpeg_finish_compress (&priv->cinfo);

  jpeg_destroy_compress (&priv->cinfo);
}

static void
backend_load (screenshot_data *data,
	      gchar *pixels, gint rowstride)
{
  struct backend_private *priv = (backend_private *) &data->private;

  for (data->lines = 0; data->lines < data->format.height; data->lines++)
    {
      jpeg_read_scanlines (&priv->dinfo, (JSAMPROW *) &pixels, 1);
      pixels += rowstride;
    }

  jpeg_finish_decompress (&priv->dinfo);
  jpeg_destroy_decompress (&priv->dinfo);
}

screenshot_backend
screenshot_backend_jpeg =
{
  .keyword		= "jpeg",
  .label		= "JPEG",
  .extension		= "jpeg",
  .sizeof_private	= sizeof (backend_private),
  .quality		= TRUE,
  .init			= backend_init,
  .save			= backend_save,
  .load			= backend_load,
};

#endif /* HAVE_LIBJPEG */
