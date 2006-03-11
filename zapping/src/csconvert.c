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
#include "libtv/yuv2rgb.h"
#include "libtv/yuv2yuv.h"

#define TEST 0

static struct {
  CSConverter_fn *	convert;
  const void *		user_data;
} filters[/* src */ TV_MAX_PIXFMTS][/* dst */ TV_MAX_PIXFMTS];

static int initialized = 0;

void startup_csconvert(void)
{
#define NV12 TV_PIXFMT_SET (TV_PIXFMT_NV12)
#define SBGGR TV_PIXFMT_SET (TV_PIXFMT_SBGGR)
#define YUV420 (TV_PIXFMT_SET (TV_PIXFMT_YUV420) |			\
		TV_PIXFMT_SET (TV_PIXFMT_YVU420))
#define YUYV (TV_PIXFMT_SET (TV_PIXFMT_YUYV) |				\
	      TV_PIXFMT_SET (TV_PIXFMT_UYVY) |				\
	      TV_PIXFMT_SET (TV_PIXFMT_YVYU) |				\
	      TV_PIXFMT_SET (TV_PIXFMT_VYUY))
#define RGB32 (TV_PIXFMT_SET (TV_PIXFMT_RGBA32_LE) |			\
	       TV_PIXFMT_SET (TV_PIXFMT_RGBA32_BE) |			\
	       TV_PIXFMT_SET (TV_PIXFMT_BGRA32_LE) |			\
	       TV_PIXFMT_SET (TV_PIXFMT_BGRA32_BE) |			\
	       TV_PIXFMT_SET (TV_PIXFMT_RGB24_LE) |			\
	       TV_PIXFMT_SET (TV_PIXFMT_RGB24_BE))
#define RGB16 (TV_PIXFMT_SET (TV_PIXFMT_BGR16_LE) |			\
	       TV_PIXFMT_SET (TV_PIXFMT_BGR16_BE) |			\
	       TV_PIXFMT_SET (TV_PIXFMT_BGRA16_LE) |			\
	       TV_PIXFMT_SET (TV_PIXFMT_BGRA16_BE))
  static const CSFilters table [] = {
    { NV12,	YUV420,		(CSConverter_fn *) _tv_nv_to_yuv420, 0 },
    { NV12,	YUYV,		(CSConverter_fn *) _tv_nv_to_yuyv, 0 },
    { NV12,	RGB16 | RGB32,	(CSConverter_fn *) _tv_nv_to_rgb, 0 },
    { YUV420,	YUV420,		(CSConverter_fn *) _tv_yuv420_to_yuv420, 0 },
    { YUV420,	YUYV,		(CSConverter_fn *) _tv_yuv420_to_yuyv, 0 },
    { YUV420,	RGB16 | RGB32,	(CSConverter_fn *) _tv_yuv420_to_rgb, 0 },
    { YUYV,	YUV420,		(CSConverter_fn *) _tv_yuyv_to_yuv420, 0 },	
    { YUYV,	YUYV,		(CSConverter_fn *) _tv_yuyv_to_yuyv, 0 },
    { YUYV,	RGB16 | RGB32,	(CSConverter_fn *) _tv_yuyv_to_rgb, 0 },
    { RGB32,	RGB32,		(CSConverter_fn *) _tv_rgb32_to_rgb32, 0 },
    { RGB32,	RGB16,		(CSConverter_fn *) _tv_rgb32_to_rgb16, 0 },
    { SBGGR,	RGB16 | RGB32,	(CSConverter_fn *) _tv_sbggr_to_rgb, 0 },
  };
  unsigned int i, j, k;

  if (initialized)
    return;

  CLEAR (filters);

  for (i = 0; i < G_N_ELEMENTS (table); ++i)
    for (j = 0; j < TV_MAX_PIXFMTS; ++j)
      for (k = 0; k < TV_MAX_PIXFMTS; ++k)
	if (table[i].src_pixfmt_set & TV_PIXFMT_SET (j)
	    && table[i].dst_pixfmt_set & TV_PIXFMT_SET (k))
	  {
	    if (TEST)
	      fprintf (stderr, "register %s -> %s\n",
		       tv_pixfmt_name (j),
		       tv_pixfmt_name (k));

	    filters[j][k].convert = table[i].convert;
	  }

  /* Register the YUV <- RGB filters */
  startup_yuv2rgb ();

  initialized = 1;
}

int lookup_csconvert(tv_pixfmt src_pixfmt,
		     tv_pixfmt dst_pixfmt)
{
  if (!initialized)
    startup_csconvert ();

  if (TEST)
    fprintf (stderr, "%p = %s %s -> %s\n",
	     filters[src_pixfmt][dst_pixfmt].convert,
	     __FUNCTION__,
	     tv_pixfmt_name (src_pixfmt),
	     tv_pixfmt_name (dst_pixfmt));

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

  if (!initialized)
    startup_csconvert ();

  if (TEST)
    fprintf (stderr, "%s %s -> %s\n",
	     __FUNCTION__,
	     tv_pixfmt_name (src_format->pixel_format->pixfmt),
	     tv_pixfmt_name (dst_format->pixel_format->pixfmt));

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
  if (filters[src_pixfmt][dst_pixfmt].convert)
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

void shutdown_csconvert(void)
{
  shutdown_yuv2rgb ();

  CLEAR (filters);
}
