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

static struct {
  CSConverter		convert;
  gpointer		user_data;
} filters[TVENG_PIX_LAST][TVENG_PIX_LAST];

int lookup_csconvert(enum tveng_frame_pixformat src_fmt,
		     enum tveng_frame_pixformat dest_fmt)
{
  if (filters[src_fmt][dest_fmt].convert == NULL)
    return -1;

  return (((int)src_fmt)<<16) + dest_fmt;
}

void csconvert(int id, tveng_image_data *src, tveng_image_data *dest,
	       int width, int height)
{
  enum tveng_frame_pixformat src_fmt, dest_fmt;

  src_fmt = id>>16;
  dest_fmt = id&0xffff;

  g_assert (filters[src_fmt][dest_fmt].convert != NULL);

  filters[src_fmt][dest_fmt].convert
    (src, dest, width, height, filters[src_fmt][dest_fmt].user_data);
}

int register_converter (enum tveng_frame_pixformat src,
			enum tveng_frame_pixformat dest,
			CSConverter	converter,
			gpointer	user_data)
{
  if (lookup_csconvert (src, dest) > -1)
    return -1;
  
  filters[src][dest].convert = converter;
  filters[src][dest].user_data = user_data;

  return 0;
}

int register_converters (CSFilter	*converters,
			 int		num_converters)
{
  int i, count=0;

  for (i=0; i<num_converters; i++)
    if (lookup_csconvert (converters[i].src, converters[i].dest) == -1)
      {
	enum tveng_frame_pixformat src = converters[i].src,
	  dest = converters[i].dest;
	filters[src][dest].convert = converters[i].convert;
	filters[src][dest].user_data = converters[i].user_data;
	count ++;
      }

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
		 int width, int height,
		 gpointer	user_data)
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

  if (!strcmp ((char*)user_data, "bgr"))
    {
      for (y=height; y; y--, d += (dest_stride/2), dest = d,
	     s += src_stride, src = s)
	for (x=width; x; x--, dest++, src+=3)
	  *dest = b[src[0]] | g[src[1]] | r[src[2]];
    }
  else if (!strcmp ((char*)user_data, "rgb"))
    {
      for (y=height; y; y--, d += (dest_stride/2), dest = d,
	     s += src_stride, src = s)
	for (x=width; x; x--, dest++, src+=3)
	  *dest = r[src[0]] | g[src[1]] | b[src[2]];
    }
  else if (!strcmp ((char*)user_data, "bgra"))
    {
      for (y=height; y; y--, d += (dest_stride/2), dest = d,
	     s += src_stride, src = s)
	for (x=width; x; x--, dest++, src+=4)
	  *dest = b[src[0]] | g[src[1]] | r[src[2]];
    }
  else if (!strcmp ((char*)user_data, "rgba"))
    {
      for (y=height; y; y--, d += (dest_stride/2), dest = d,
	     s += src_stride, src = s)
	for (x=width; x; x--, dest++, src+=4)
	  *dest = r[src[0]] | g[src[1]] | b[src[2]];
    }
}

static void
rgb_rgb555	(tveng_image_data *_src, tveng_image_data *_dest,
		 int width, int height,
		 gpointer	user_data)
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

  if (!strcmp ((char*)user_data, "bgr"))
    {
      for (y=height; y; y--, d += (dest_stride/2), dest = d,
	     s += src_stride, src = s)
	for (x=width; x; x--, dest++, src+=3)
	  *dest = b[src[0]] | g[src[1]] | r[src[2]];
    }
  else if (!strcmp ((char*)user_data, "rgb"))
    {
      for (y=height; y; y--, d += (dest_stride/2), dest = d,
	     s += src_stride, src = s)
	for (x=width; x; x--, dest++, src+=3)
	  *dest = r[src[0]] | g[src[1]] | b[src[2]];
    }
  else if (!strcmp ((char*)user_data, "bgra"))
    {
      for (y=height; y; y--, d += (dest_stride/2), dest = d,
	     s += src_stride, src = s)
	for (x=width; x; x--, dest++, src+=4)
	  *dest = b[src[0]] | g[src[1]] | r[src[2]];
    }
  else if (!strcmp ((char*)user_data, "rgba"))
    {
      for (y=height; y; y--, d += (dest_stride/2), dest = d,
	     s += src_stride, src = s)
	for (x=width; x; x--, dest++, src+=4)
	  *dest = r[src[0]] | g[src[1]] | b[src[2]];
    }
}

static void
rgb_bgr		(tveng_image_data	*_src, tveng_image_data *_dest,
		 int width, int height, gpointer user_data)
{
  unsigned char *src = _src->linear.data, *s = src;
  unsigned char *dest = _dest->linear.data, *d = dest;
  int src_stride = _src->linear.stride;
  int dest_stride = _dest->linear.stride;
  int x, y;
  int src_size = 0, dest_size = 0;

  if (!strcmp ((char*)user_data, "bgra<->rgba"))
    {
      src_size = 4;
      dest_size = 4;
    }
  else if (!strcmp ((char*)user_data, "rgb->bgra"))
    {
      src_size = 3;
      dest_size = 4;
    }
  else if (!strcmp ((char*)user_data, "bgra->rgb"))
    {
      src_size = 4;
      dest_size = 3;
    }
  else if (!strcmp ((char*)user_data, "bgr<->rgb"))
    {
      src_size = 3;
      dest_size = 3;
    }
  else if (!strcmp ((char*)user_data, "rgb<->bgr"))
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
bgra_bgr (tveng_image_data *_src, tveng_image_data *_dest, int width, int height,
	  gpointer user_data)
{
  unsigned char *src = _src->linear.data, *s = src;
  unsigned char *dest = _dest->linear.data, *d = dest;
  int src_stride = _src->linear.stride;
  int dest_stride = _dest->linear.stride;
  int x, y;
  int src_size = 0, dest_size = 0;

  if (!strcmp ((char*)user_data, "bgra->bgr"))
    {
      src_size = 4;
      dest_size = 3;
    }
  else if (!strcmp ((char*)user_data, "bgr->bgra"))
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
    { TVENG_PIX_RGB24, TVENG_PIX_RGB555, rgb_rgb555, "rgb" },
    { TVENG_PIX_BGR24, TVENG_PIX_RGB555, rgb_rgb555, "bgr" },
    { TVENG_PIX_RGB32, TVENG_PIX_RGB555, rgb_rgb555, "rgba" },
    { TVENG_PIX_BGR32, TVENG_PIX_RGB555, rgb_rgb555, "bgra" },
    
    { TVENG_PIX_RGB24, TVENG_PIX_RGB565, rgb_rgb565, "rgb" },
    { TVENG_PIX_BGR24, TVENG_PIX_RGB565, rgb_rgb565, "bgr" },
    { TVENG_PIX_RGB32, TVENG_PIX_RGB565, rgb_rgb565, "rgba" },
    { TVENG_PIX_BGR32, TVENG_PIX_RGB565, rgb_rgb565, "bgra" },
    
    { TVENG_PIX_RGB24, TVENG_PIX_BGR24, rgb_bgr, "rgb<->bgr" },
    { TVENG_PIX_RGB24, TVENG_PIX_BGR32, rgb_bgr, "rgb->bgra" },
    { TVENG_PIX_BGR24, TVENG_PIX_RGB24, rgb_bgr, "rgb<->bgr" },
    { TVENG_PIX_BGR24, TVENG_PIX_RGB32, rgb_bgr, "rgb->bgra" },
    { TVENG_PIX_RGB32, TVENG_PIX_BGR24, rgb_bgr, "bgra->rgb" },
    { TVENG_PIX_RGB32, TVENG_PIX_BGR32, rgb_bgr, "bgra<->rgba" },
    { TVENG_PIX_BGR32, TVENG_PIX_RGB24, rgb_bgr, "bgra->rgb" },
    { TVENG_PIX_BGR32, TVENG_PIX_RGB32, rgb_bgr, "bgra<->rgba" },
    
    { TVENG_PIX_RGB24, TVENG_PIX_RGB32, bgra_bgr, "bgr->bgra" },
    { TVENG_PIX_BGR24, TVENG_PIX_BGR32, bgra_bgr, "bgr->bgra" },
    { TVENG_PIX_RGB32, TVENG_PIX_RGB24, bgra_bgr, "bgra->bgr" },
    { TVENG_PIX_BGR32, TVENG_PIX_BGR32, bgra_bgr, "bgra->bgr" }
  };
  int num_filters = sizeof (rgb_filters) / sizeof(rgb_filters[0]);

  memset (filters, 0, sizeof (filters));

  register_converters (rgb_filters, num_filters);

  /* Load the YUV <-> RGB filters */
  startup_yuv2rgb ();
}

void shutdown_csconvert(void)
{
  shutdown_yuv2rgb ();

  memset (filters, 0, sizeof (filters));
}
