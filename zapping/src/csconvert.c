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
 * Unaccelerated colorspace conversions, see yuv2rgb for the MMX
 * YUV->RGB filters.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "csconvert.h"

static unsigned int r[256], g[256], b[256];

static void
bgr24_short	(const char *_src, char *_dest,
		 int src_stride, int dest_stride,
		 int width, int height)
{
  const unsigned char *src=_src, *s = src;
  unsigned short *dest = (unsigned short *)_dest, *d = dest;
  int x, y;

  for (y=height; y; y--, d += (dest_stride/2), dest = d,
	 s += src_stride, src = s)
    for (x=width; x; x--, dest++, src+=3)
      *dest = b[src[0]] | g[src[1]] | r[src[2]];
}

static void
bgr24_long	(const char *_src, char *_dest,
		 int src_stride, int dest_stride,
		 int width, int height)
{
  const unsigned char *src=_src, *s = src;
  unsigned int *dest = (unsigned int *)_dest, *d = dest;
  int x, y;

  for (y=height; y; y--, d += (dest_stride/4), dest = d,
	 s += src_stride, src = s)
    for (x=width; x; x--, dest++, src+=3)
      *dest = b[src[0]] | g[src[1]] | r[src[2]];
}

static void
bgr32_long	(const char *_src, char *_dest,
		 int src_stride, int dest_stride,
		 int width, int height)
{
  const unsigned char *src=_src, *s = src;
  unsigned int *dest = (unsigned int *)_dest, *d = dest;
  int x, y;

  for (y=height; y; y--, d += (dest_stride/4), dest = d,
	 s += src_stride, src = s)
    for (x=width; x; x--, dest++, src+=4)
      *dest = b[src[0]] | g[src[1]] | r[src[2]];
}

static void
rgb32_long	(const char *_src, char *_dest,
		 int src_stride, int dest_stride,
		 int width, int height)
{
  const unsigned char *src=_src, *s = src;
  unsigned int *dest = (unsigned int *)_dest, *d = dest;
  int x, y;

  for (y=height; y; y--, d += (dest_stride/4), dest = d,
	 s += src_stride, src = s)
    for (x=width; x; x--, dest++, src+=4)
      *dest = r[src[0]] | g[src[1]] | b[src[2]];
}

static struct {
  enum tveng_frame_pixformat	src;
  enum tveng_frame_pixformat	dest;
  void		(*convert)	(const char *src, char *dest,
				 int src_stride, int dest_stride,
				 int width, int height);
} filters[] =
{
  {
    TVENG_PIX_BGR24, TVENG_PIX_RGB565, bgr24_short
  },
  {
    TVENG_PIX_BGR24, TVENG_PIX_RGB555, bgr24_short
  },
  {
    TVENG_PIX_BGR24, TVENG_PIX_BGR32, bgr24_long
  },
  {
    TVENG_PIX_BGR32, TVENG_PIX_RGB32, bgr32_long
  },
  {
    TVENG_PIX_RGB32, TVENG_PIX_BGR32, rgb32_long
  }
};

#define num_filters (sizeof(filters)/sizeof(filters[0]))

int lookup_csconvert(enum tveng_frame_pixformat src_fmt,
		     enum tveng_frame_pixformat dest_fmt)
{
  unsigned int i;

  for (i=0; i<num_filters; i++)
    if (filters[i].src == src_fmt &&
	filters[i].dest == dest_fmt)
      return i;

  return -1;
}

void build_csconvert_tables(int rmask, int rshift, int rprec,
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

void csconvert(int id, const char *src, char *dest,
	       int src_stride, int dest_stride,
	       int width, int height)
{
  filters[id].convert(src, dest, src_stride, dest_stride, width, height);
}

void startup_csconvert(void)
{
}

void shutdown_csconvert(void)
{
}
