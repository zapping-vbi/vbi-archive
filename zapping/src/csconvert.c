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
#  include <config.h>
#endif

#include <glib.h>
#include "csconvert.h"
#include "yuv2rgb.h"
#include "zmisc.h"

static struct {
  CSConverter_fn *	convert;
  const char *		user_data;
} filters[TV_MAX_PIXFMTS][TV_MAX_PIXFMTS];

int lookup_csconvert(tv_pixfmt src_pixfmt,
		     tv_pixfmt dst_pixfmt)
{
  if (filters[src_pixfmt][dst_pixfmt].convert == NULL)
    return -1;

  return (((int)src_pixfmt)<<16) + dst_pixfmt;
}

void csconvert(int id, tveng_image_data *src, tveng_image_data *dest,
	       unsigned int width, unsigned int height)
{
  tv_pixfmt src_pixfmt, dst_pixfmt;

  src_pixfmt = id>>16;
  dst_pixfmt = id&0xffff;

  g_assert (filters[src_pixfmt][dst_pixfmt].convert != NULL);

  filters[src_pixfmt][dst_pixfmt].convert
    (src, dest, width, height, filters[src_pixfmt][dst_pixfmt].user_data);


}

int register_converter (const char *	name _unused_,
			tv_pixfmt src_pixfmt,
			tv_pixfmt dst_pixfmt,
			CSConverter_fn *	converter,
			const char *		user_data)
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

static void build_csconvert_tables(unsigned int *r,
				   unsigned int *g,
				   unsigned int *b,
				   int rmask, int rshift, int rprec,
				   int gmask, int gshift, int gprec,
				   int bmask, int bshift, int bprec)
{
  unsigned int i;

  for (i=0; i<256; i++)
    {
      r[i] = ((i >> (8-rprec))<<rshift)&rmask;
      g[i] = ((i >> (8-gprec))<<gshift)&gmask;
      b[i] = ((i >> (8-bprec))<<bshift)&bmask;
    }
}

static void
rgb_rgb565	(tveng_image_data *_src, tveng_image_data *_dest,
		 unsigned int width, unsigned int height,
		 const gchar *	user_data)
{
  const unsigned char *src=_src->linear.data, *s = src;
  int src_stride = _src->linear.stride;
  int dest_stride = _dest->linear.stride;
  unsigned short *dest = (unsigned short *)_dest->linear.data, *d = dest;
  int x, y;
  static int tables = 0;
  static int r[256], g[256], b[256];

  if (!tables)
    {
      build_csconvert_tables (r, g, b,
			      0xf800, 11, 5, /* red */
			      0x7e0, 5, 6, /* green */
			      0x1f, 0, 5 /* blue */);
      tables = 1;
    }

  if (!strcmp (user_data, "bgr"))
    {
      for (y=height; y; y--, d += (dest_stride/2), dest = d,
	     s += src_stride, src = s)
	for (x=width; x; x--, dest++, src+=3)
	  *dest = b[src[0]] | g[src[1]] | r[src[2]];
    }
  else if (!strcmp (user_data, "rgb"))
    {
      for (y=height; y; y--, d += (dest_stride/2), dest = d,
	     s += src_stride, src = s)
	for (x=width; x; x--, dest++, src+=3)
	  *dest = r[src[0]] | g[src[1]] | b[src[2]];
    }
  else if (!strcmp (user_data, "bgra"))
    {
      for (y=height; y; y--, d += (dest_stride/2), dest = d,
	     s += src_stride, src = s)
	for (x=width; x; x--, dest++, src+=4)
	  *dest = b[src[0]] | g[src[1]] | r[src[2]];
    }
  else if (!strcmp (user_data, "rgba"))
    {
      for (y=height; y; y--, d += (dest_stride/2), dest = d,
	     s += src_stride, src = s)
	for (x=width; x; x--, dest++, src+=4)
	  *dest = r[src[0]] | g[src[1]] | b[src[2]];
    }
}

static void
rgb_rgb555	(tveng_image_data *_src, tveng_image_data *_dest,
		 unsigned int width, unsigned int height,
		 const gchar *	user_data)
{
  const unsigned char *src=_src->linear.data, *s = src;
  int src_stride = _src->linear.stride;
  int dest_stride = _dest->linear.stride;
  unsigned short *dest = (unsigned short *)_dest->linear.data, *d = dest;
  int x, y;
  static int tables = 0;
  static int r[256], g[256], b[256];

  if (!tables)
    {
      build_csconvert_tables (r, g, b,
			      0x7c00, 10, 5, /* red */
			      0x3e0, 5, 5, /* green */
			      0x1f, 0, 5 /* blue */);
      tables = 1;
    }

  if (!strcmp (user_data, "bgr"))
    {
      for (y=height; y; y--, d += (dest_stride/2), dest = d,
	     s += src_stride, src = s)
	for (x=width; x; x--, dest++, src+=3)
	  *dest = b[src[0]] | g[src[1]] | r[src[2]];
    }
  else if (!strcmp (user_data, "rgb"))
    {
      for (y=height; y; y--, d += (dest_stride/2), dest = d,
	     s += src_stride, src = s)
	for (x=width; x; x--, dest++, src+=3)
	  *dest = r[src[0]] | g[src[1]] | b[src[2]];
    }
  else if (!strcmp (user_data, "bgra"))
    {
      for (y=height; y; y--, d += (dest_stride/2), dest = d,
	     s += src_stride, src = s)
	for (x=width; x; x--, dest++, src+=4)
	  *dest = b[src[0]] | g[src[1]] | r[src[2]];
    }
  else if (!strcmp (user_data, "rgba"))
    {
      for (y=height; y; y--, d += (dest_stride/2), dest = d,
	     s += src_stride, src = s)
	for (x=width; x; x--, dest++, src+=4)
	  *dest = r[src[0]] | g[src[1]] | b[src[2]];
    }
}

static void
rgb_bgr		(tveng_image_data	*_src, tveng_image_data *_dest,
		 unsigned int width, unsigned int height, const gchar * user_data)
{
  unsigned char *src = _src->linear.data, *s = src;
  unsigned char *dest = _dest->linear.data, *d = dest;
  int src_stride = _src->linear.stride;
  int dest_stride = _dest->linear.stride;
  int x, y;
  int src_size = 0, dest_size = 0;

  if (!strcmp (user_data, "bgra<->rgba"))
    {
      src_size = 4;
      dest_size = 4;
    }
  else if (!strcmp (user_data, "rgb->bgra"))
    {
      src_size = 3;
      dest_size = 4;
    }
  else if (!strcmp (user_data, "bgra->rgb"))
    {
      src_size = 4;
      dest_size = 3;
    }
  else if (!strcmp (user_data, "bgr<->rgb"))
    {
      src_size = 3;
      dest_size = 3;
    }
  else if (!strcmp (user_data, "rgb<->bgr"))
    {
      src_size = 3;
      dest_size = 3;
    }
  else
    g_assert_not_reached ();

  for (y=height; y; y--, d += (dest_stride), dest = d,
	 s += src_stride, src = s)
    for (x=width; x; x--, dest += dest_size, src += src_size)
      {
	dest[0] = src[2];
	dest[1] = src[1];
	dest[2] = src[0];
      }
}

static void
bgra_bgr (tveng_image_data *_src, tveng_image_data *_dest,
	  unsigned int width, unsigned int height,
	  const gchar * user_data)
{
  unsigned char *src = _src->linear.data, *s = src;
  unsigned char *dest = _dest->linear.data, *d = dest;
  int src_stride = _src->linear.stride;
  int dest_stride = _dest->linear.stride;
  int x, y;
  int src_size = 0, dest_size = 0;

  if (!strcmp (user_data, "bgra->bgr"))
    {
      src_size = 4;
      dest_size = 3;
    }
  else if (!strcmp (user_data, "bgr->bgra"))
    {
      src_size = 3;
      dest_size = 4;
    }
  else
    g_assert_not_reached ();

  for (y=height; y; y--, d += (dest_stride), dest = d,
	 s += src_stride, src = s)
    for (x=width; x; x--, dest += dest_size, src += src_size)
      {
	dest[0] = src[0];
	dest[1] = src[1];
	dest[2] = src[2];
      }
}

void startup_csconvert(void)
{
  CSFilter rgb_filters [] = {
    /* We lack rgb5.5 -> rgb.* filters, but those are easy too in case
       we need them. They are rarely used, tho. */
    { TV_PIXFMT_BGR24_LE,  TV_PIXFMT_BGRA16_LE, rgb_rgb555, "rgb" },
    { TV_PIXFMT_BGR24_BE,  TV_PIXFMT_BGRA16_LE, rgb_rgb555, "bgr" },
    { TV_PIXFMT_BGRA32_LE, TV_PIXFMT_BGRA16_LE, rgb_rgb555, "rgba" },
    { TV_PIXFMT_BGRA32_BE, TV_PIXFMT_BGRA16_LE, rgb_rgb555, "bgra" },
    
    { TV_PIXFMT_BGR24_LE,  TV_PIXFMT_BGR16_LE,  rgb_rgb565, "rgb" },
    { TV_PIXFMT_BGR24_BE,  TV_PIXFMT_BGR16_LE,  rgb_rgb565, "bgr" },
    { TV_PIXFMT_BGRA32_LE, TV_PIXFMT_BGR16_LE,  rgb_rgb565, "rgba" },
    { TV_PIXFMT_BGRA32_BE, TV_PIXFMT_BGR16_LE,  rgb_rgb565, "bgra" },

    { TV_PIXFMT_BGR24_LE,  TV_PIXFMT_BGR24_BE,  rgb_bgr, "rgb<->bgr" },
    { TV_PIXFMT_BGR24_LE,  TV_PIXFMT_BGRA32_BE, rgb_bgr, "rgb->bgra" },
    { TV_PIXFMT_BGR24_BE,  TV_PIXFMT_BGR24_LE,  rgb_bgr, "rgb<->bgr" },
    { TV_PIXFMT_BGR24_BE,  TV_PIXFMT_BGRA32_LE, rgb_bgr, "rgb->bgra" },
    { TV_PIXFMT_BGRA32_LE, TV_PIXFMT_BGR24_BE,  rgb_bgr, "bgra->rgb" },
    { TV_PIXFMT_BGRA32_LE, TV_PIXFMT_BGRA32_BE, rgb_bgr, "bgra<->rgba" },
    { TV_PIXFMT_BGRA32_BE, TV_PIXFMT_BGR24_LE,  rgb_bgr, "bgra->rgb" },
    { TV_PIXFMT_BGRA32_BE, TV_PIXFMT_BGRA32_LE, rgb_bgr, "bgra<->rgba" },

    { TV_PIXFMT_BGR24_LE,  TV_PIXFMT_BGRA32_LE, bgra_bgr, "bgr->bgra" },
    { TV_PIXFMT_BGR24_BE,  TV_PIXFMT_BGRA32_BE, bgra_bgr, "bgr->bgra" },
    { TV_PIXFMT_BGRA32_LE, TV_PIXFMT_BGR24_LE,  bgra_bgr, "bgra->bgr" },
    { TV_PIXFMT_BGRA32_BE, TV_PIXFMT_BGR24_BE,  bgra_bgr, "bgra->bgr" }
  };

  CLEAR (filters);

  register_converters ("c", rgb_filters, N_ELEMENTS (rgb_filters));

  /* Load the YUV <-> RGB filters */
  startup_yuv2rgb ();
}

void shutdown_csconvert(void)
{
  shutdown_yuv2rgb ();

  CLEAR (filters);
}
