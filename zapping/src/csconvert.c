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

/**
 * Color conversion routines. Here we have the RGB->RGB conversion
 * routines, the YUV <-> RGB routines are in yuv2rgb.c
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include "csconvert.h"
#include "yuv2rgb.h"
#include "zmisc.h"
#include "libtv/rgb2rgb.h"

static struct {
  CSConverter_fn *	convert;
  const void *		user_data;
} filters[TV_MAX_PIXFMTS][TV_MAX_PIXFMTS];

int lookup_csconvert(tv_pixfmt src_pixfmt,
		     tv_pixfmt dst_pixfmt)
{
  if (filters[src_pixfmt][dst_pixfmt].convert == NULL)
    return -1;

  return (((int)src_pixfmt)<<16) + dst_pixfmt;
}

gboolean
csconvert			(void *			dst_image,
				 const tv_image_format *dst_format,
				 const void *		src_image,
				 const tv_image_format *src_format)
{
  CSConverter_fn *convert;
  const void *user_data;

  if (dst_format->pixel_format == src_format->pixel_format)
    return tv_copy_image (dst_image, dst_format, src_image, src_format);

  convert = filters[src_format->pixel_format->pixfmt]
    [dst_format->pixel_format->pixfmt].convert;

  if (!convert)
    return FALSE;

  user_data = filters[src_format->pixel_format->pixfmt]
    [dst_format->pixel_format->pixfmt].user_data;

  convert (dst_image, dst_format, src_image, src_format, user_data);

  return TRUE;
}

int register_converter (const char *	name _unused_,
			tv_pixfmt src_pixfmt,
			tv_pixfmt dst_pixfmt,
			CSConverter_fn *	converter,
			const void *		user_data)
{
  if (-1 != lookup_csconvert (src_pixfmt, dst_pixfmt))
    return -1; /* already registered */

  filters[src_pixfmt][dst_pixfmt].convert = converter;
  filters[src_pixfmt][dst_pixfmt].user_data = user_data;

  return 0;
}

int register_converters (const char *	name,
			 CSFilter	*converters,
			 int		num_converters)
{
  int i, count=0;

  for (i = 0; i < num_converters; ++i)
    if (0 == register_converter (name,
				 converters[i].src_pixfmt,
				 converters[i].dst_pixfmt,
				 converters[i].convert,
				 converters[i].user_data))
      ++count;

  return count;
}

/* ------------------------------------------------------------------------ */

static void
nv12_yuv420			(void *			dst_image,
				 const tv_image_format *dst_format,
				 const void *		src_image,
				 const tv_image_format *src_format,
				 const void *		user_data _unused_)
{
	uint8_t *dst;
	uint8_t *udst;
	uint8_t *vdst;
	const uint8_t *src;
	const uint8_t *end;
	unsigned int width;
	unsigned int height;
	unsigned long dst_bpl;
	unsigned long src_bpl;
	unsigned long udst_padding;
	unsigned long vdst_padding;
	unsigned long src_padding;

	dst = (uint8_t *) dst_image + dst_format->offset[0];
	src = (const uint8_t *) src_image + src_format->offset[0];

	width = MIN (dst_format->width, src_format->width);
	height = MIN (dst_format->height, src_format->height);

	assert (0 == ((width | height) % 2));

	dst_bpl = dst_format->bytes_per_line[0];
	src_bpl = src_format->bytes_per_line[0];

	if (likely (dst_bpl == width && src_bpl == width)) {
		memcpy (dst, src, width * height);
	} else {
		end = src + height * src_bpl;

		while (src < end) {
			memcpy (dst, src, width);
			dst += dst_bpl;
			src += src_bpl;
		}
	}

	udst = (uint8_t *) dst_image + dst_format->offset[1];
	vdst = (uint8_t *) dst_image + dst_format->offset[2];
	src = (const uint8_t *) src_image + src_format->offset[1];

	udst_padding = dst_format->bytes_per_line[1] - (width >> 1);
	vdst_padding = dst_format->bytes_per_line[2] - (width >> 1);
	src_padding = src_format->bytes_per_line[1] - width;

	for (height >>= 1; height > 0; --height) {
		end = src + width;

		while (src < end) {
			*udst++ = src[0];
			*vdst++ = src[1];
			src += 2;
		}

		udst += udst_padding;
		vdst += vdst_padding;
		src += src_padding;
	}
}

void startup_csconvert(void)
{
  CSFilter rgb_filters [] = {
    /* We lack rgb5.5 -> rgb.* filters, but those are easy too in case
       we need them. They are rarely used, tho. */
    { TV_PIXFMT_NV12, TV_PIXFMT_YUV420, nv12_yuv420, "nv12->yuv420" }
  };
  static const tv_pixfmt src_formats [] =
    {
      TV_PIXFMT_RGBA32_LE,
      TV_PIXFMT_RGBA32_BE,
      TV_PIXFMT_BGRA32_LE,
      TV_PIXFMT_BGRA32_BE,
      TV_PIXFMT_RGB24_LE,
      TV_PIXFMT_RGB24_BE,
      TV_PIXFMT_SBGGR,
    };
  static const tv_pixfmt dst_formats [] =
    {
      TV_PIXFMT_RGBA32_LE,
      TV_PIXFMT_RGBA32_BE,
      TV_PIXFMT_BGRA32_LE,
      TV_PIXFMT_BGRA32_BE,
      TV_PIXFMT_RGB24_LE,
      TV_PIXFMT_RGB24_BE,
      TV_PIXFMT_BGR16_LE,
      TV_PIXFMT_BGR16_BE,
      TV_PIXFMT_BGRA16_LE,
      TV_PIXFMT_BGRA16_BE,
    };
  unsigned int i;
  unsigned int j;

  CLEAR (filters);

  for (i = 0; i < G_N_ELEMENTS (src_formats); ++i)
    {
      for (j = 0; j < G_N_ELEMENTS (dst_formats); ++j)
	{
	  if (TV_PIXFMT_SBGGR == src_formats[i])
	    {
	      register_converter ("_tv_sbggr_to_rgb",
				  src_formats[i], dst_formats[j],
				  (CSConverter_fn *) _tv_sbggr_to_rgb,
				  /* user_data */ NULL);
	    }
	  else if (2 == TV_PIXFMT_BYTES_PER_PIXEL (dst_formats[j]))
	    {
	      register_converter ("_tv_rgb32_to_rgb16",
				  src_formats[i], dst_formats[j],
				  (CSConverter_fn *) _tv_rgb32_to_rgb16,
				  /* user_data */ NULL);
	    }
	  else
	    {
	      register_converter ("_tv_rgb32_to_rgb32",
				  src_formats[i], dst_formats[j],
				  (CSConverter_fn *) _tv_rgb32_to_rgb32,
				  /* user_data */ NULL);
	    }
	}
    }

  register_converters ("c", rgb_filters, N_ELEMENTS (rgb_filters));

  /* Load the YUV <-> RGB filters */
  startup_yuv2rgb ();
}

void shutdown_csconvert(void)
{
  shutdown_yuv2rgb ();

  CLEAR (filters);
}
