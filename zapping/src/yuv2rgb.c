/*
 * yuv2rgb.c, Software YUV to RGB coverter
 *
 *  Copyright (C) 1999, Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *  All Rights Reserved.
 *
 *  Functions broken out from display_x11.c and several new modes
 *  added by Håkan Hjort <d95hjort@dtek.chalmers.se>
 *
 *  15 & 16 bpp support by Franck Sicard <Franck.Sicard@solsoft.fr>
 *
 *  This file is part of mpeg2dec, a free MPEG-2 video decoder
 *
 *  mpeg2dec is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  mpeg2dec is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>

#include <glib.h>
#include "common/math.h"
#include "csconvert.h"
#include "yuv2rgb.h"
#include "gen_conv.h"
#include "zmisc.h"
#include "cpu.h"
#include "globals.h"

#define MODE_RGB  0x1
#define MODE_BGR  0x2

#ifdef HAVE_SSE

typedef void
planar2packed_fn		(uint8_t * image, uint8_t * py,
				 uint8_t * pu, uint8_t * pv,
				 int h_size, int v_size,
				 unsigned int rgb_stride,
				 unsigned int y_stride,
				 unsigned int uv_stride);
typedef void
packed2planar_fn		(uint8_t *py, uint8_t *pu, uint8_t *pv,
				 uint8_t *image, int h_size, int v_size,
				 unsigned int y_stride,
				 unsigned int uv_stride,
				 unsigned int rgb_stride);
typedef void
packed2packed_fn		(uint8_t *dest, uint8_t *src,
				 int h_size, int v_size,
				 unsigned int dest_stride,
				 unsigned int src_stride);

static void *
yuv420_to_rgb_function		(tv_pixfmt		pixfmt)
{
  if (cpu_features & CPU_FEATURE_SSE)
    switch (pixfmt)
      {
      case TV_PIXFMT_RGBA16_LE:	return sse_yuv420_rgb5551;
      case TV_PIXFMT_BGRA16_LE:	return sse_yuv420_bgr5551;
      case TV_PIXFMT_RGB16_LE:	return sse_yuv420_rgb565;
      case TV_PIXFMT_BGR16_LE:	return sse_yuv420_bgr565;
      case TV_PIXFMT_RGB24_LE:	return sse_yuv420_rgb24;
      case TV_PIXFMT_BGR24_LE:	return sse_yuv420_bgr24;
      case TV_PIXFMT_RGBA32_LE:	return sse_yuv420_rgb32;
      case TV_PIXFMT_BGRA32_LE:	return sse_yuv420_bgr32;
      default:			break;
      }

  if (cpu_features & CPU_FEATURE_AMD_MMX)
    switch (pixfmt)
      {
      case TV_PIXFMT_RGBA16_LE:	return amd_yuv420_rgb5551;
      case TV_PIXFMT_BGRA16_LE:	return amd_yuv420_bgr5551;
      case TV_PIXFMT_RGB16_LE:	return amd_yuv420_rgb565;
      case TV_PIXFMT_BGR16_LE:	return amd_yuv420_bgr565;
      case TV_PIXFMT_RGB24_LE:	return amd_yuv420_rgb24;
      case TV_PIXFMT_BGR24_LE:	return amd_yuv420_bgr24;
      case TV_PIXFMT_RGBA32_LE:	return amd_yuv420_rgb32;
      case TV_PIXFMT_BGRA32_LE:	return amd_yuv420_bgr32;
      default:			break;
      }

  if (cpu_features & CPU_FEATURE_3DNOW)
    switch (pixfmt)
      {
      case TV_PIXFMT_RGBA16_LE:	return _3dn_yuv420_rgb5551;
      case TV_PIXFMT_BGRA16_LE:	return _3dn_yuv420_bgr5551;
      case TV_PIXFMT_RGB16_LE:	return _3dn_yuv420_rgb565;
      case TV_PIXFMT_BGR16_LE:	return _3dn_yuv420_bgr565;
      case TV_PIXFMT_RGB24_LE:	return _3dn_yuv420_rgb24;
      case TV_PIXFMT_BGR24_LE:	return _3dn_yuv420_bgr24;
      case TV_PIXFMT_RGBA32_LE:	return _3dn_yuv420_rgb32;
      case TV_PIXFMT_BGRA32_LE:	return _3dn_yuv420_bgr32;
      default:			break;
      }

  if (cpu_features & CPU_FEATURE_MMX)
    switch (pixfmt)
      {
      case TV_PIXFMT_RGBA16_LE:	return mmx_yuv420_rgb5551;
      case TV_PIXFMT_BGRA16_LE:	return mmx_yuv420_bgr5551;
      case TV_PIXFMT_RGB16_LE:	return mmx_yuv420_rgb565;
      case TV_PIXFMT_BGR16_LE:	return mmx_yuv420_bgr565;
      case TV_PIXFMT_RGB24_LE:	return mmx_yuv420_rgb24;
      case TV_PIXFMT_BGR24_LE:	return mmx_yuv420_bgr24;
      case TV_PIXFMT_RGBA32_LE:	return mmx_yuv420_rgb32;
      case TV_PIXFMT_BGRA32_LE:	return mmx_yuv420_bgr32;
      default:			break;
      }

  return NULL;
}

static void *
rgb_to_yuv420_function		(tv_pixfmt		pixfmt)
{
  if (cpu_features & CPU_FEATURE_MMX)
    switch (pixfmt)
      {
      case TV_PIXFMT_RGBA16_LE:	return mmx_rgb5551_yuv420;
      case TV_PIXFMT_BGRA16_LE:	return mmx_bgr5551_yuv420;
      case TV_PIXFMT_RGB16_LE:	return mmx_rgb565_yuv420;
      case TV_PIXFMT_BGR16_LE:	return mmx_bgr565_yuv420;
      case TV_PIXFMT_RGB24_LE:	return mmx_rgb24_yuv420;
      case TV_PIXFMT_BGR24_LE:	return mmx_bgr24_yuv420;
      case TV_PIXFMT_RGBA32_LE:	return mmx_rgb32_yuv420;
      case TV_PIXFMT_BGRA32_LE:	return mmx_bgr32_yuv420;
      default:			break;
      }

  return NULL;
}

static void *
yuyv_to_rgb_function		(tv_pixfmt		pixfmt)
{
  if (cpu_features & CPU_FEATURE_SSE)
    switch (pixfmt)
      {
      case TV_PIXFMT_RGBA16_LE:	return sse_yuyv_rgb5551;
      case TV_PIXFMT_BGRA16_LE:	return sse_yuyv_bgr5551;
      case TV_PIXFMT_RGB16_LE:	return sse_yuyv_rgb565;
      case TV_PIXFMT_BGR16_LE:	return sse_yuyv_bgr565;
      case TV_PIXFMT_RGB24_LE:	return sse_yuyv_rgb24;
      case TV_PIXFMT_BGR24_LE:	return sse_yuyv_bgr24;
      case TV_PIXFMT_RGBA32_LE:	return sse_yuyv_rgb32;
      case TV_PIXFMT_BGRA32_LE:	return sse_yuyv_bgr32;
      default:			break;
      }
  
  if (cpu_features & CPU_FEATURE_AMD_MMX)
    switch (pixfmt)
      {
      case TV_PIXFMT_RGBA16_LE:	return amd_yuyv_rgb5551;
      case TV_PIXFMT_BGRA16_LE:	return amd_yuyv_bgr5551;
      case TV_PIXFMT_RGB16_LE:	return amd_yuyv_rgb565;
      case TV_PIXFMT_BGR16_LE:	return amd_yuyv_bgr565;
      case TV_PIXFMT_RGB24_LE:	return amd_yuyv_rgb24;
      case TV_PIXFMT_BGR24_LE:	return amd_yuyv_bgr24;
      case TV_PIXFMT_RGBA32_LE:	return amd_yuyv_rgb32;
      case TV_PIXFMT_BGRA32_LE:	return amd_yuyv_bgr32;
      default:			break;
      }
  
  if (cpu_features & CPU_FEATURE_3DNOW)
    switch (pixfmt)
      {
      case TV_PIXFMT_RGBA16_LE:	return _3dn_yuyv_rgb5551;
      case TV_PIXFMT_BGRA16_LE:	return _3dn_yuyv_bgr5551;
      case TV_PIXFMT_RGB16_LE:	return _3dn_yuyv_rgb565;
      case TV_PIXFMT_BGR16_LE:	return _3dn_yuyv_bgr565;
      case TV_PIXFMT_RGB24_LE:	return _3dn_yuyv_rgb24;
      case TV_PIXFMT_BGR24_LE:	return _3dn_yuyv_bgr24;
      case TV_PIXFMT_RGBA32_LE:	return _3dn_yuyv_rgb32;
      case TV_PIXFMT_BGRA32_LE:	return _3dn_yuyv_bgr32;
      default:			break;
      }
  
  if (cpu_features & CPU_FEATURE_MMX)
    switch (pixfmt)
      {
      case TV_PIXFMT_RGBA16_LE:	return mmx_yuyv_rgb5551;
      case TV_PIXFMT_BGRA16_LE:	return mmx_yuyv_bgr5551;
      case TV_PIXFMT_RGB16_LE:	return mmx_yuyv_rgb565;
      case TV_PIXFMT_BGR16_LE:	return mmx_yuyv_bgr565;
      case TV_PIXFMT_RGB24_LE:	return mmx_yuyv_rgb24;
      case TV_PIXFMT_BGR24_LE:	return mmx_yuyv_bgr24;
      case TV_PIXFMT_RGBA32_LE:	return mmx_yuyv_rgb32;
      case TV_PIXFMT_BGRA32_LE:	return mmx_yuyv_bgr32;
      default:			break;
      }
  
  return NULL;
}

static void *
rgb_to_yuyv_function		(tv_pixfmt		pixfmt)
{
  if (cpu_features & CPU_FEATURE_MMX)
    switch (pixfmt)
      {
      case TV_PIXFMT_RGBA16_LE:	return mmx_rgb5551_yuyv;
      case TV_PIXFMT_BGRA16_LE:	return mmx_bgr5551_yuyv;
      case TV_PIXFMT_RGB16_LE:	return mmx_rgb565_yuyv;
      case TV_PIXFMT_BGR16_LE:	return mmx_bgr565_yuyv;
      case TV_PIXFMT_RGB24_LE:	return mmx_rgb24_yuyv;
      case TV_PIXFMT_BGR24_LE:	return mmx_bgr24_yuyv;
      case TV_PIXFMT_RGBA32_LE:	return mmx_rgb32_yuyv;
      case TV_PIXFMT_BGRA32_LE:	return mmx_bgr32_yuyv;
      default:			break;
      }

  return NULL;
}

static void
planar2packed_proxy		(void *			dst_image,
				 const tv_image_format *dst_format,
				 const void *		src_image,
				 const tv_image_format *src_format,
				 const void *		user_data)
{
  planar2packed_fn *f = user_data;

  f ((uint8_t *) dst_image + dst_format->offset[0],
     (const uint8_t *) src_image + src_format->offset[0],
     (const uint8_t *) src_image + src_format->offset[1],
     (const uint8_t *) src_image + src_format->offset[2],
     MIN (dst_format->width, src_format->width),
     MIN (dst_format->height, src_format->height),
     dst_format->bytes_per_line[0],
     src_format->bytes_per_line[0],
     src_format->bytes_per_line[1]);
}

static void
packed2planar_proxy		(void *			dst_image,
				 const tv_image_format *dst_format,
				 const void *		src_image,
				 const tv_image_format *src_format,
				 const void *		user_data)
{
  packed2planar_fn *f = user_data;

  f ((uint8_t *) dst_image + dst_format->offset[0],
     (uint8_t *) dst_image + dst_format->offset[1],
     (uint8_t *) dst_image + dst_format->offset[2],
     (const uint8_t *) src_image + src_format->offset[0],
     MIN (dst_format->width, src_format->width),
     MIN (dst_format->height, src_format->height),
     dst_format->bytes_per_line[0],
     dst_format->bytes_per_line[1],
     src_format->bytes_per_line[0]);
}

static void
packed2packed_proxy		(void *			dst_image,
				 const tv_image_format *dst_format,
				 const void *		src_image,
				 const tv_image_format *src_format,
				 const void *		user_data)
{
  packed2packed_fn *f = user_data;

  f ((uint8_t *) dst_image + dst_format->offset[0],
     (const uint8_t *) src_image + src_format->offset[0],
     MIN (dst_format->width, src_format->width),
     MIN (dst_format->height, src_format->height),
     dst_format->bytes_per_line[0],
     src_format->bytes_per_line[0]);
}

static void
mmx_register_converters		(void)
{
  static tv_pixfmt pixfmts [] = {
    TV_PIXFMT_RGBA16_LE,
    TV_PIXFMT_BGRA16_LE,
    TV_PIXFMT_RGB16_LE,
    TV_PIXFMT_BGR16_LE,
    TV_PIXFMT_RGB24_LE,
    TV_PIXFMT_BGR24_LE,
    TV_PIXFMT_RGBA32_LE,
    TV_PIXFMT_BGRA32_LE,
  };
  unsigned int i;
  void *p;

  for (i = 0; i < N_ELEMENTS (pixfmts); ++i)
    {
      if ((p = yuv420_to_rgb_function (pixfmts[i])))
	{
	  register_converter ("yuv420-",
			      TV_PIXFMT_YUV420, pixfmts[i],
			      planar2packed_proxy, p);
	  register_converter ("yvu420-",
			      TV_PIXFMT_YVU420, pixfmts[i],
			      planar2packed_proxy, p);
	}

      if ((p = rgb_to_yuv420_function (pixfmts[i])))
	{
	  register_converter ("-yuv420",
			      pixfmts[i], TV_PIXFMT_YUV420,
			      packed2planar_proxy, p);
	  register_converter ("-yvu420",
			      pixfmts[i], TV_PIXFMT_YVU420,
			      packed2planar_proxy, p);
	}

      if ((p = yuyv_to_rgb_function (pixfmts[i])))
	register_converter ("yuyv-",
			    TV_PIXFMT_YUYV, pixfmts[i],
			    packed2packed_proxy, p);

      if ((p = rgb_to_yuyv_function (pixfmts[i])))
	register_converter ("-yuyv",
			    pixfmts[i], TV_PIXFMT_YUYV,
			    packed2packed_proxy, p);
    }
}

#else

static void
mmx_register_converters (void)
{
}

#endif

#define RGB(i)					\
	U = pu[i];				\
	V = pv[i];				\
	r = (void *) table_rV[V];		\
	g = (void *)(table_gU[U] + table_gV[V]);\
	b = (void *)table_bU[U];

#define DST1(i)					\
	Y = py_1[2*i];				\
	dst_1[2*i] = r[Y] + g[Y] + b[Y];	\
	Y = py_1[2*i+1];			\
	dst_1[2*i+1] = r[Y] + g[Y] + b[Y];

#define DST2(i)					\
	Y = py_2[2*i];				\
	dst_2[2*i] = r[Y] + g[Y] + b[Y];	\
	Y = py_2[2*i+1];			\
	dst_2[2*i+1] = r[Y] + g[Y] + b[Y];

#define DST1RGB(i)							\
	Y = py_1[2*i];							\
	dst_1[6*i] = r[Y]; dst_1[6*i+1] = g[Y]; dst_1[6*i+2] = b[Y];	\
	Y = py_1[2*i+1];						\
	dst_1[6*i+3] = r[Y]; dst_1[6*i+4] = g[Y]; dst_1[6*i+5] = b[Y];

#define DST2RGB(i)							\
	Y = py_2[2*i];							\
	dst_2[6*i] = r[Y]; dst_2[6*i+1] = g[Y]; dst_2[6*i+2] = b[Y];	\
	Y = py_2[2*i+1];						\
	dst_2[6*i+3] = r[Y]; dst_2[6*i+4] = g[Y]; dst_2[6*i+5] = b[Y];

#define DST1BGR(i)							\
	Y = py_1[2*i];							\
	dst_1[6*i] = b[Y]; dst_1[6*i+1] = g[Y]; dst_1[6*i+2] = r[Y];	\
	Y = py_1[2*i+1];						\
	dst_1[6*i+3] = b[Y]; dst_1[6*i+4] = g[Y]; dst_1[6*i+5] = r[Y];

#define DST2BGR(i)							\
	Y = py_2[2*i];							\
	dst_2[6*i] = b[Y]; dst_2[6*i+1] = g[Y]; dst_2[6*i+2] = r[Y];	\
	Y = py_2[2*i+1];						\
	dst_2[6*i+3] = b[Y]; dst_2[6*i+4] = g[Y]; dst_2[6*i+5] = r[Y];

static int div_round (int dividend, int divisor)
{
    if (dividend > 0)
	return (dividend + (divisor>>1)) / divisor;
    else
	return -((-dividend + (divisor>>1)) / divisor);
}

#define matrix_coefficients 6

static const int32_t Inverse_Table_6_9[8][4] = {
    {117504, 138453, 13954, 34903}, /* no sequence_display_extension */
    {117504, 138453, 13954, 34903}, /* ITU-R Rec. 709 (1990) */
    {104597, 132201, 25675, 53279}, /* unspecified */
    {104597, 132201, 25675, 53279}, /* reserved */
    {104448, 132798, 24759, 53109}, /* FCC */
    {104597, 132201, 25675, 53279}, /* ITU-R Rec. 624-4 System B, G */
    {104597, 132201, 25675, 53279}, /* SMPTE 170M */
    {117579, 136230, 16907, 35559}  /* SMPTE 240M (1987) */
};

static void yuv2rgb_c_init (int bpp, int mode,
			    char * table_rV[256],
			    char * table_gU[256],
			    int table_gV[256],
			    char * table_bU[256])
{  
    int i;
    uint8_t table_Y[1024];
    uint32_t *table_32 = 0;
    uint16_t *table_16 = 0;
    uint8_t *table_8 = 0;
    uint32_t entry_size = 0;
    char *table_r = 0, *table_g = 0, *table_b = 0;

    int crv = Inverse_Table_6_9[matrix_coefficients][0];
    int cbu = Inverse_Table_6_9[matrix_coefficients][1];
    int cgu = -Inverse_Table_6_9[matrix_coefficients][2];
    int cgv = -Inverse_Table_6_9[matrix_coefficients][3];

    for (i = 0; i < 1024; i++) {
	int j;

	j = (76309 * (i - 384 - 16) + 32768) >> 16;
	j = (j < 0) ? 0 : ((j > 255) ? 255 : j);
	table_Y[i] = j;
    }

    switch (bpp) {
    case 32:
	table_32 = malloc ((197 + 2*682 + 256 + 132) * sizeof (uint32_t));

	entry_size = sizeof (uint32_t);
	table_r = (char *)(table_32 + 197);
	table_b = (char *)(table_32 + 197 + 685);
	table_g = (char *)(table_32 + 197 + 2*682);

	for (i = -197; i < 256+197; i++)
	    ((uint32_t *)table_r)[i] =
	      table_Y[i+384] << ((mode==MODE_RGB) ? 16 : 0);
	for (i = -132; i < 256+132; i++)
	    ((uint32_t *)table_g)[i] = table_Y[i+384] << 8;
	for (i = -232; i < 256+232; i++)
	    ((uint32_t *)table_b)[i] =
	      table_Y[i+384] << ((mode==MODE_RGB) ? 0 : 16);
	break;

    case 24:
	table_8 = malloc ((256 + 2*232) * sizeof (uint8_t));

	entry_size = sizeof (uint8_t);
	table_r = table_g = table_b = table_8 + 232;

	for (i = -232; i < 256+232; i++)
	    ((uint8_t * )table_b)[i] = table_Y[i+384];
	break;

    case 15:
    case 16:
	table_16 = malloc ((197 + 2*682 + 256 + 132) * sizeof (uint16_t));

	entry_size = sizeof (uint16_t);
	table_r = (char *)(table_16 + 197);
	table_b = (char *)(table_16 + 197 + 685);
	table_g = (char *)(table_16 + 197 + 2*682);

	for (i = -197; i < 256+197; i++) {
	    int j = table_Y[i+384] >> 3;

	    if (mode == MODE_RGB)
		j <<= ((bpp==16) ? 11 : 10);

	    ((uint16_t *)table_r)[i] = j;
	}
	for (i = -132; i < 256+132; i++) {
	    int j = table_Y[i+384] >> ((bpp==16) ? 2 : 3);

	    ((uint16_t *)table_g)[i] = j << 5;
	}
	for (i = -232; i < 256+232; i++) {
	    int j = table_Y[i+384] >> 3;

	    if (mode == MODE_BGR)
		j <<= ((bpp==16) ? 11 : 10);

	    ((uint16_t *)table_b)[i] = j;
	}
	break;

    default:
	printv ("%ibpp not supported by yuv2rgb\n", bpp);
	return;
    }

    for (i = 0; i < 256; i++) {
	table_rV[i] = table_r + entry_size * div_round (crv * (i-128), 76309);
	table_gU[i] = table_g + entry_size * div_round (cgu * (i-128), 76309);
	table_gV[i] = entry_size * div_round (cgv * (i-128), 76309);
	table_bU[i] = table_b + entry_size * div_round (cbu * (i-128), 76309);
    }
}

static void yuv2rgb_c_32 (const uint8_t * py_1, const uint8_t * py_2,
			  const uint8_t * pu, const uint8_t * pv,
			  char * _dst_1, char * _dst_2, int h_size)
{
    int U, V, Y;
    uint32_t * r, * g, * b;
    uint32_t * dst_1, * dst_2;
    static char * table_rV[256];
    static char * table_gU[256];
    static int table_gV[256];
    static char * table_bU[256];
    static int inited = 0;

    if (!inited)
      {
	yuv2rgb_c_init (32, MODE_RGB, table_rV, table_gU, table_gV,
			table_bU);
	inited = 1;
      }

    h_size >>= 3;
    dst_1 = (void *)_dst_1;
    dst_2 = (void *)_dst_2;

    while (h_size--) {
	RGB(0);
	DST1(0);
	DST2(0);

	RGB(1);
	DST2(1);
	DST1(1);

	RGB(2);
	DST1(2);
	DST2(2);

	RGB(3);
	DST2(3);
	DST1(3);

	pu += 4;
	pv += 4;
	py_1 += 8;
	py_2 += 8;
	dst_1 += 8;
	dst_2 += 8;
    }
}

/* This is very near from the yuv2rgb_c_32 code*/
static void yuv2rgb_c_24_rgb (const uint8_t * py_1, const uint8_t * py_2,
			      const uint8_t * pu, const uint8_t * pv,
			      char * _dst_1, char * _dst_2, int h_size)
{
    int U, V, Y;
    uint8_t * r, * g, * b;
    uint8_t * dst_1, * dst_2;
    static char * table_rV[256];
    static char * table_gU[256];
    static int table_gV[256];
    static char * table_bU[256];
    static int inited = 0;

    if (!inited)
      {
	yuv2rgb_c_init (24, MODE_RGB, table_rV, table_gU, table_gV,
			table_bU);
	inited = 1;
      }

    h_size >>= 3;
    dst_1 = _dst_1;
    dst_2 = _dst_2;

    while (h_size--) {
	RGB(0);
	DST1RGB(0);
	DST2RGB(0);

	RGB(1);
	DST2RGB(1);
	DST1RGB(1);

	RGB(2);
	DST1RGB(2);
	DST2RGB(2);

	RGB(3);
	DST2RGB(3);
	DST1RGB(3);

	pu += 4;
	pv += 4;
	py_1 += 8;
	py_2 += 8;
	dst_1 += 24;
	dst_2 += 24;
    }
}

/* only trivial mods from yuv2rgb_c_24_rgb*/
static void yuv2rgb_c_24_bgr (const uint8_t * py_1, const uint8_t * py_2,
			      const uint8_t * pu, const uint8_t * pv,
			      char * _dst_1, char * _dst_2, int h_size)
{
    int U, V, Y;
    uint8_t * r, * g, * b;
    uint8_t * dst_1, * dst_2;
    static char * table_rV[256];
    static char * table_gU[256];
    static int table_gV[256];
    static char * table_bU[256];
    static int inited = 0;

    if (!inited)
      {
	yuv2rgb_c_init (24, MODE_BGR, table_rV, table_gU, table_gV,
			table_bU);
	inited = 1;
      }

    h_size >>= 3;
    dst_1 = _dst_1;
    dst_2 = _dst_2;

    while (h_size--) {
	RGB(0);
	DST1BGR(0);
	DST2BGR(0);

	RGB(1);
	DST2BGR(1);
	DST1BGR(1);

	RGB(2);
	DST1BGR(2);
	DST2BGR(2);

	RGB(3);
	DST2BGR(3);
	DST1BGR(3);

	pu += 4;
	pv += 4;
	py_1 += 8;
	py_2 += 8;
	dst_1 += 24;
	dst_2 += 24;
    }
}

/* This is exactly the same code as yuv2rgb_c_32 except for the types of
   r, g, b, dst_1, dst_2 */
static void yuv2rgb_c_16 (const uint8_t * py_1, const uint8_t * py_2,
			  const uint8_t * pu, const uint8_t * pv,
			  char * _dst_1, char * _dst_2, int h_size)
{
    int U, V, Y;
    uint16_t * r, * g, * b;
    uint16_t * dst_1, * dst_2;
    static char * table_rV[256];
    static char * table_gU[256];
    static int table_gV[256];
    static char * table_bU[256];
    static int inited = 0;

    if (!inited)
      {
	yuv2rgb_c_init (16, MODE_RGB, table_rV, table_gU, table_gV,
			table_bU);
	inited = 1;
      }

    h_size >>= 3;
    dst_1 = (void *)_dst_1;
    dst_2 = (void *)_dst_2;

    while (h_size--) {
	RGB(0);
	DST1(0);
	DST2(0);

	RGB(1);
	DST2(1);
	DST1(1);

	RGB(2);
	DST1(2);
	DST2(2);

	RGB(3);
	DST2(3);
	DST1(3);

	pu += 4;
	pv += 4;
	py_1 += 8;
	py_2 += 8;
	dst_1 += 8;
	dst_2 += 8;
    }
}

static void yuv2rgb_c_15 (const uint8_t * py_1, const uint8_t * py_2,
			  const uint8_t * pu, const uint8_t * pv,
			  char * _dst_1, char * _dst_2, int h_size)
{
    int U, V, Y;
    uint16_t * r, * g, * b;
    uint16_t * dst_1, * dst_2;
    static char * table_rV[256];
    static char * table_gU[256];
    static int table_gV[256];
    static char * table_bU[256];
    static int inited = 0;

    if (!inited)
      {
	yuv2rgb_c_init (15, MODE_RGB, table_rV, table_gU, table_gV,
			table_bU);
	inited = 1;
      }

    h_size >>= 3;
    dst_1 = (void *)_dst_1;
    dst_2 = (void *)_dst_2;

    while (h_size--) {
	RGB(0);
	DST1(0);
	DST2(0);

	RGB(1);
	DST2(1);
	DST1(1);

	RGB(2);
	DST1(2);
	DST2(2);

	RGB(3);
	DST2(3);
	DST1(3);

	pu += 4;
	pv += 4;
	py_1 += 8;
	py_2 += 8;
	dst_1 += 8;
	dst_2 += 8;
    }
}

typedef void (* yuv2rgb_c_internal_fun) (const uint8_t *, const uint8_t *,
					 const uint8_t *, const uint8_t *,
					 char *, char *, int);
enum {
  YUV420_RGB555,
  YVU420_RGB555,
  YUV420_RGB565,
  YVU420_RGB565,
  YUV420_RGB24,
  YVU420_RGB24,
  YUV420_BGR24,
  YVU420_BGR24,
  YUV420_RGB32,
  YVU420_RGB32,
};

static void
c_proxy				(void *			dst_image,
				 const tv_image_format *dst_format,
				 const void *		src_image,
				 const tv_image_format *src_format,
				 const void *		user_data)
{
  yuv2rgb_c_internal_fun yuv2rgb_c_internal = NULL;
  char *dst = (char *) dst_image + dst_format->offset[0];
  const uint8_t *py = (const uint8_t *) src_image + src_format->offset[0];
  const uint8_t *pu = (const uint8_t *) src_image + src_format->offset[1];
  const uint8_t *pv = (const uint8_t *) src_image + src_format->offset[2];
  int v_size;
  int h_size;

  switch ((int) user_data)
    {
    case YUV420_RGB555:
    case YVU420_RGB555:
      yuv2rgb_c_internal = yuv2rgb_c_15;
      break;

    case YUV420_RGB565:
    case YVU420_RGB565:
      yuv2rgb_c_internal = yuv2rgb_c_16;
      break;

    case YUV420_RGB24:
    case YVU420_RGB24:
      yuv2rgb_c_internal = yuv2rgb_c_24_rgb;
      break;

    case YUV420_BGR24:
    case YVU420_BGR24:
      yuv2rgb_c_internal = yuv2rgb_c_24_bgr;
      break;

    case YUV420_RGB32:
    case YVU420_RGB32:
      yuv2rgb_c_internal = yuv2rgb_c_32;
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  v_size = MIN (dst_format->height, src_format->height) >> 1;
  h_size = MIN (dst_format->width, src_format->width);

  while (v_size-- > 0) {
    yuv2rgb_c_internal (py, py + src_format->bytes_per_line[0], pu, pv,
			dst, dst + dst_format->bytes_per_line[0],
			h_size);

    py += 2 * src_format->bytes_per_line[0];
    pu += src_format->bytes_per_line[1];
    pv += src_format->bytes_per_line[1];
    dst += 2 * dst_format->bytes_per_line[0];
  }
}

void startup_yuv2rgb (void) 
{
  CSFilter cfuncs[] = {
    {TV_PIXFMT_YUV420, TV_PIXFMT_BGRA16_LE, c_proxy, (void*)YUV420_RGB555},
    {TV_PIXFMT_YVU420, TV_PIXFMT_BGRA16_LE, c_proxy, (void*)YVU420_RGB555},
    {TV_PIXFMT_YUV420, TV_PIXFMT_BGR16_LE, c_proxy, (void*)YUV420_RGB565},
    {TV_PIXFMT_YVU420, TV_PIXFMT_BGR16_LE, c_proxy, (void*)YVU420_RGB565},
    {TV_PIXFMT_YUV420, TV_PIXFMT_RGB24_LE, c_proxy, (void*)YUV420_RGB24},
    {TV_PIXFMT_YVU420, TV_PIXFMT_RGB24_LE, c_proxy, (void*)YVU420_RGB24},
    {TV_PIXFMT_YUV420, TV_PIXFMT_BGR24_LE, c_proxy, (void*)YUV420_BGR24},
    {TV_PIXFMT_YVU420, TV_PIXFMT_BGR24_LE, c_proxy, (void*)YVU420_BGR24},
    {TV_PIXFMT_YUV420, TV_PIXFMT_BGRA32_LE, c_proxy, (void*)YUV420_RGB32},
    {TV_PIXFMT_YVU420, TV_PIXFMT_BGRA32_LE, c_proxy, (void*)YVU420_RGB32}
  };

  /* Try first the MMX versions of the functions */
  mmx_register_converters ();

  /* Register the C version of the converters when we don't have
     MMX versions registered */
  register_converters ("yuv420-", cfuncs, N_ELEMENTS (cfuncs));
}

void shutdown_yuv2rgb (void)
{
}
