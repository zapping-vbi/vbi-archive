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

#include <stdlib.h>
#include <stdio.h>

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <common/math.h>
#include "csconvert.h"
#include "yuv2rgb.h"
#include "gen_conv.h"
#include "zmisc.h"
#include "cpu.h"
#include "globals.h"

#define MODE_RGB  0x1
#define MODE_BGR  0x2

#ifdef HAVE_GAS

static void*
yuv2rgb_init_swar (cpu_type cpu, int bpp, int mode)
{
  switch (cpu)
    {
    case CPU_PENTIUM_MMX:
    case CPU_PENTIUM_II:
    case CPU_CYRIX_MII:
    case CPU_CYRIX_III:
      switch (bpp)
        {
        case 15:
          return (mode == MODE_BGR ? mmx_yuv420_rgb5551 :
    	      mmx_yuv420_bgr5551);
        case 16:
          return (mode == MODE_BGR ? mmx_yuv420_rgb565 :
    	      mmx_yuv420_bgr565);
        case 24:
          return (mode == MODE_BGR ? mmx_yuv420_rgb24 :
    	      mmx_yuv420_bgr24);
        case 32:
          return (mode == MODE_BGR ? mmx_yuv420_rgb32 :
	      mmx_yuv420_bgr32);
        default:
          break;
        }
      break;

    case CPU_PENTIUM_III:
    case CPU_PENTIUM_4:
      switch (bpp)
        {
        case 15:
          return (mode == MODE_BGR ? sse_yuv420_rgb5551 :
    	      sse_yuv420_bgr5551);
        case 16:
          return (mode == MODE_BGR ? sse_yuv420_rgb565 :
    	      sse_yuv420_bgr565);
        case 24:
          return (mode == MODE_BGR ? sse_yuv420_rgb24 :
    	      sse_yuv420_bgr24);
        case 32:
          return (mode == MODE_BGR ? sse_yuv420_rgb32 :
	      sse_yuv420_bgr32);
        default:
          break;
        }
      break;

    case CPU_K6_2:
      switch (bpp)
        {
        case 15:
          return (mode == MODE_BGR ? _3dn_yuv420_rgb5551 :
    	      _3dn_yuv420_bgr5551);
        case 16:
          return (mode == MODE_BGR ? _3dn_yuv420_rgb565 :
    	      _3dn_yuv420_bgr565);
        case 24:
          return (mode == MODE_BGR ? _3dn_yuv420_rgb24 :
    	      _3dn_yuv420_bgr24);
        case 32:
          return (mode == MODE_BGR ? _3dn_yuv420_rgb32 :
	      _3dn_yuv420_bgr32);
        default:
          break;
        }
      break;

    case CPU_ATHLON:
      switch (bpp)
        {
        case 15:
          return (mode == MODE_BGR ? amd_yuv420_rgb5551 :
    	      amd_yuv420_bgr5551);
        case 16:
          return (mode == MODE_BGR ? amd_yuv420_rgb565 :
    	      amd_yuv420_bgr565);
        case 24:
          return (mode == MODE_BGR ? amd_yuv420_rgb24 :
    	      amd_yuv420_bgr24);
        case 32:
          return (mode == MODE_BGR ? amd_yuv420_rgb32 :
	      amd_yuv420_bgr32);
        default:
          break;
        }
      break;

    default:
      break;
    }

  return NULL; /* Fallback to C */
}

static void*
rgb2yuv_init_swar (cpu_type cpu, uint bpp, int mode)
{
  switch (cpu)
    {
    case CPU_PENTIUM_MMX:
    case CPU_PENTIUM_II:
    case CPU_CYRIX_MII:
    case CPU_CYRIX_III:
    case CPU_PENTIUM_III:
    case CPU_PENTIUM_4:
    case CPU_K6_2:
    case CPU_ATHLON:
      switch (bpp)
        {
        case 15:
          return (mode == MODE_BGR ? mmx_rgb5551_yuv420 :
    	      mmx_bgr5551_yuv420);
        case 16:
          return (mode == MODE_BGR ? mmx_rgb565_yuv420 :
    	      mmx_bgr565_yuv420);
        case 24:
          return (mode == MODE_BGR ? mmx_rgb24_yuv420 :
    	      mmx_bgr24_yuv420);
        case 32:
          return (mode == MODE_BGR ? mmx_rgb32_yuv420 :
	      mmx_bgr32_yuv420);
        default:
          break;
        }
      break;

    default:
      break;
    }

  return NULL; /* Fallback to C */
}

static void*
yuyv2rgb_init_swar (cpu_type cpu, uint bpp, int mode)
{
  switch (cpu)
    {
    case CPU_PENTIUM_MMX:
    case CPU_PENTIUM_II:
    case CPU_CYRIX_MII:
    case CPU_CYRIX_III:
      switch (bpp)
        {
        case 15:
          return (mode == MODE_BGR ? mmx_yuyv_rgb5551 :
    	      mmx_yuyv_bgr5551);
        case 16:
          return (mode == MODE_BGR ? mmx_yuyv_rgb565 :
    	      mmx_yuyv_bgr565);
        case 24:
          return (mode == MODE_BGR ? mmx_yuyv_rgb24 :
    	      mmx_yuyv_bgr24);
        case 32:
          return (mode == MODE_BGR ? mmx_yuyv_rgb32 :
	      mmx_yuyv_bgr32);
        default:
          break;
        }
      break;

    case CPU_PENTIUM_III:
    case CPU_PENTIUM_4:
      switch (bpp)
        {
        case 15:
          return (mode == MODE_BGR ? sse_yuyv_rgb5551 :
    	      sse_yuyv_bgr5551);
        case 16:
          return (mode == MODE_BGR ? sse_yuyv_rgb565 :
    	      sse_yuyv_bgr565);
        case 24:
          return (mode == MODE_BGR ? sse_yuyv_rgb24 :
    	      sse_yuyv_bgr24);
        case 32:
          return (mode == MODE_BGR ? sse_yuyv_rgb32 :
	      sse_yuyv_bgr32);
        default:
          break;
        }
      break;

    case CPU_K6_2:
      switch (bpp)
        {
        case 15:
          return (mode == MODE_BGR ? _3dn_yuyv_rgb5551 :
    	      _3dn_yuyv_bgr5551);
        case 16:
          return (mode == MODE_BGR ? _3dn_yuyv_rgb565 :
    	      _3dn_yuyv_bgr565);
        case 24:
          return (mode == MODE_BGR ? _3dn_yuyv_rgb24 :
    	      _3dn_yuyv_bgr24);
        case 32:
          return (mode == MODE_BGR ? _3dn_yuyv_rgb32 :
	      _3dn_yuyv_bgr32);
        default:
          break;
        }
      break;

    case CPU_ATHLON:
      switch (bpp)
        {
        case 15:
          return (mode == MODE_BGR ? amd_yuyv_rgb5551 :
    	      amd_yuyv_bgr5551);
        case 16:
          return (mode == MODE_BGR ? amd_yuyv_rgb565 :
    	      amd_yuyv_bgr565);
        case 24:
          return (mode == MODE_BGR ? amd_yuyv_rgb24 :
    	      amd_yuyv_bgr24);
        case 32:
          return (mode == MODE_BGR ? amd_yuyv_rgb32 :
	      amd_yuyv_bgr32);
        default:
          break;
        }
      break;

    default:
      break;
    }

  return NULL; /* Fallback to C */
}

static void*
rgb2yuyv_init_swar (cpu_type cpu, uint bpp, int mode)
{
  switch (cpu)
    {
    case CPU_PENTIUM_MMX:
    case CPU_PENTIUM_II:
    case CPU_CYRIX_MII:
    case CPU_CYRIX_III:
    case CPU_PENTIUM_III:
    case CPU_PENTIUM_4:
    case CPU_K6_2:
    case CPU_ATHLON:
      switch (bpp)
        {
        case 15:
          return (mode == MODE_BGR ? mmx_rgb5551_yuyv :
    	      mmx_bgr5551_yuyv);
        case 16:
          return (mode == MODE_BGR ? mmx_rgb565_yuyv :
    	      mmx_bgr565_yuyv);
        case 24:
          return (mode == MODE_BGR ? mmx_rgb24_yuyv :
    	      mmx_bgr24_yuyv);
        case 32:
          return (mode == MODE_BGR ? mmx_rgb32_yuyv :
	      mmx_bgr32_yuyv);
        default:
          break;
        }
      break;

    default:
      break;
    }

  return NULL; /* Fallback to C */
}

typedef void (* yuv2rgb_fun) (uint8_t * image, uint8_t * py,
			      uint8_t * pu, uint8_t * pv,
			      int h_size, int v_size,
			      int rgb_stride, int y_stride, int uv_stride);

static void
yuv420_rgb_proxy (tveng_image_data *src, tveng_image_data *dest,
		  int width, int height, void *user_data)
{
  yuv2rgb_fun f = (yuv2rgb_fun)user_data;
  f (dest->linear.data, src->planar.y, src->planar.u,
     src->planar.v, width, height, dest->linear.stride,
     src->planar.y_stride, src->planar.uv_stride);
}

static void
yvu420_rgb_proxy (tveng_image_data *src, tveng_image_data *dest,
		  int width, int height, void *user_data)
{
  yuv2rgb_fun f = (yuv2rgb_fun)user_data;
  f (dest->linear.data, src->planar.y, src->planar.v,
     src->planar.u, width, height, dest->linear.stride,
     src->planar.y_stride, src->planar.uv_stride);
}

typedef void (* rgb2yuv_fun) (uint8_t *py, uint8_t *pu, uint8_t *pv,
			      uint8_t *image, int h_size, int v_size,
			      int y_stride, int uv_stride, int rgb_stride);

static void rgb_yuv420_proxy (tveng_image_data *src, tveng_image_data *dest,
			      int width, int height, void *user_data)
{
  rgb2yuv_fun f = (rgb2yuv_fun)user_data;
  f (dest->planar.y, dest->planar.u, dest->planar.v, src->linear.data,
     width, height, dest->planar.y_stride, dest->planar.uv_stride,
     src->linear.stride);
}

static void rgb_yvu420_proxy (tveng_image_data *src, tveng_image_data *dest,
			      int width, int height, void *user_data)
{
  rgb2yuv_fun f = (rgb2yuv_fun)user_data;
  f (dest->planar.y, dest->planar.v, dest->planar.u, src->linear.data,
     width, height, dest->planar.y_stride, dest->planar.uv_stride,
     src->linear.stride);
}

typedef void (* yuyv2rgb_fun) (uint8_t *dest, uint8_t *src,
			       int h_size, int v_size,
			       int dest_stride, int src_stride);

static void
yuyv_rgb_proxy (tveng_image_data *src, tveng_image_data *dest,
		int width, int height, void *user_data)

{
  yuyv2rgb_fun f = (yuyv2rgb_fun)user_data;
  f (dest->linear.data, src->linear.data, width, height,
     dest->linear.stride, src->linear.stride);
}

static void mmx_register_converters (void)
{
  cpu_type cpu = cpu_detection ();
  /* keep in sync with tveng.h */
  int depth [] =
    {
      15, 16, 24, 24, 32, 32
    };
  /* ditto */
  int mode [] =
    {
      MODE_RGB, MODE_RGB, MODE_RGB, MODE_BGR, MODE_RGB, MODE_BGR
    };
  int i;
  void *p;

  /* YUV420, YVU420 -> RGB* */
  for (i=TVENG_PIX_FIRST; i<=TVENG_PIX_BGR32; i++)
    if ((p = yuv2rgb_init_swar (cpu, depth[i], mode[i])))
      {
	register_converter (TVENG_PIX_YUV420, i, yuv420_rgb_proxy, p);
	register_converter (TVENG_PIX_YVU420, i, yvu420_rgb_proxy, p);
      }

  /* RGB* -> YUV420, YVU420 */
  for (i=TVENG_PIX_FIRST; i<=TVENG_PIX_BGR32; i++)
    if ((p = rgb2yuv_init_swar (cpu, depth[i], mode[i])))
      {
	register_converter (i, TVENG_PIX_YUV420, rgb_yuv420_proxy, p);
	register_converter (i, TVENG_PIX_YVU420, rgb_yvu420_proxy, p);
      }

  /* YUYV -> RGB* */
  for (i=TVENG_PIX_FIRST; i<=TVENG_PIX_BGR32; i++)
    if ((p = yuyv2rgb_init_swar (cpu, depth[i], mode[i])))
      register_converter (TVENG_PIX_YUYV, i, yuyv_rgb_proxy, p);

  /* RGB* -> YUYV */
  for (i=TVENG_PIX_FIRST; i<=TVENG_PIX_BGR32; i++)
    if ((p = rgb2yuyv_init_swar (cpu, depth[i], mode[i])))
      register_converter (i, TVENG_PIX_YUYV, yuyv_rgb_proxy, p);
}

#else /* !HAVE_GAS */
static void mmx_register_converters (void)
{
}
#endif /* !HAVE_GAS */

#define RGB(i)					\
	U = pu[i];				\
	V = pv[i];				\
	r = table_rV[V];			\
	g = table_gU[U] + table_gV[V];		\
	b = table_bU[U];

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
			    void * table_rV[256],
			    void * table_gU[256],
			    int table_gV[256],
			    void * table_bU[256])
{  
    int i;
    uint8_t table_Y[1024];
    uint32_t *table_32 = 0;
    uint16_t *table_16 = 0;
    uint8_t *table_8 = 0;
    uint32_t entry_size = 0;
    void *table_r = 0, *table_g = 0, *table_b = 0;

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
	table_r = table_32 + 197;
	table_b = table_32 + 197 + 685;
	table_g = table_32 + 197 + 2*682;

	for (i = -197; i < 256+197; i++)
	    ((uint32_t *)table_r)[i] = table_Y[i+384] << ((mode==MODE_RGB) ? 16 : 0);
	for (i = -132; i < 256+132; i++)
	    ((uint32_t *)table_g)[i] = table_Y[i+384] << 8;
	for (i = -232; i < 256+232; i++)
	    ((uint32_t *)table_b)[i] = table_Y[i+384] << ((mode==MODE_RGB) ? 0 : 16);
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
	table_r = table_16 + 197;
	table_b = table_16 + 197 + 685;
	table_g = table_16 + 197 + 2*682;

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

static void yuv2rgb_c_32 (uint8_t * py_1, uint8_t * py_2,
			  uint8_t * pu, uint8_t * pv,
			  void * _dst_1, void * _dst_2, int h_size)
{
    int U, V, Y;
    uint32_t * r, * g, * b;
    uint32_t * dst_1, * dst_2;
    static void * table_rV[256];
    static void * table_gU[256];
    static int table_gV[256];
    static void * table_bU[256];
    static int inited = 0;

    if (!inited)
      {
	yuv2rgb_c_init (32, MODE_RGB, table_rV, table_gU, table_gV,
			table_bU);
	inited = 1;
      }

    h_size >>= 3;
    dst_1 = _dst_1;
    dst_2 = _dst_2;

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

// This is very near from the yuv2rgb_c_32 code
static void yuv2rgb_c_24_rgb (uint8_t * py_1, uint8_t * py_2,
			      uint8_t * pu, uint8_t * pv,
			      void * _dst_1, void * _dst_2, int h_size)
{
    int U, V, Y;
    uint8_t * r, * g, * b;
    uint8_t * dst_1, * dst_2;
    static void * table_rV[256];
    static void * table_gU[256];
    static int table_gV[256];
    static void * table_bU[256];
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

// only trivial mods from yuv2rgb_c_24_rgb
static void yuv2rgb_c_24_bgr (uint8_t * py_1, uint8_t * py_2,
			      uint8_t * pu, uint8_t * pv,
			      void * _dst_1, void * _dst_2, int h_size)
{
    int U, V, Y;
    uint8_t * r, * g, * b;
    uint8_t * dst_1, * dst_2;
    static void * table_rV[256];
    static void * table_gU[256];
    static int table_gV[256];
    static void * table_bU[256];
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

// This is exactly the same code as yuv2rgb_c_32 except for the types of
// r, g, b, dst_1, dst_2
static void yuv2rgb_c_16 (uint8_t * py_1, uint8_t * py_2,
			  uint8_t * pu, uint8_t * pv,
			  void * _dst_1, void * _dst_2, int h_size)
{
    int U, V, Y;
    uint16_t * r, * g, * b;
    uint16_t * dst_1, * dst_2;
    static void * table_rV[256];
    static void * table_gU[256];
    static int table_gV[256];
    static void * table_bU[256];
    static int inited = 0;

    if (!inited)
      {
	yuv2rgb_c_init (16, MODE_RGB, table_rV, table_gU, table_gV,
			table_bU);
	inited = 1;
      }

    h_size >>= 3;
    dst_1 = _dst_1;
    dst_2 = _dst_2;

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

static void yuv2rgb_c_15 (uint8_t * py_1, uint8_t * py_2,
			  uint8_t * pu, uint8_t * pv,
			  void * _dst_1, void * _dst_2, int h_size)
{
    int U, V, Y;
    uint16_t * r, * g, * b;
    uint16_t * dst_1, * dst_2;
    static void * table_rV[256];
    static void * table_gU[256];
    static int table_gV[256];
    static void * table_bU[256];
    static int inited = 0;

    if (!inited)
      {
	yuv2rgb_c_init (15, MODE_RGB, table_rV, table_gU, table_gV,
			table_bU);
	inited = 1;
      }

    h_size >>= 3;
    dst_1 = _dst_1;
    dst_2 = _dst_2;

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

typedef void (* yuv2rgb_c_internal_fun) (uint8_t *, uint8_t *,
					 uint8_t *, uint8_t *,
					 void *, void *, int);
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
yuv2rgb_c_proxy (tveng_image_data *_src, tveng_image_data *_dest, int
		 h_size, int v_size, gpointer user_data)
{
  yuv2rgb_c_internal_fun yuv2rgb_c_internal = NULL;
  void *dst = _dest->linear.data;
  int rgb_stride = _dest->linear.stride;
  uint8_t *py = _src->planar.y, *pu = _src->planar.u,
    *pv = _src->planar.v;
  int y_stride = _src->planar.y_stride;
  int uv_stride = _src->planar.uv_stride;

  switch ((int)user_data)
    {
    case YUV420_RGB555:
      yuv2rgb_c_internal = yuv2rgb_c_15;
      break;
    case YVU420_RGB555:
      yuv2rgb_c_internal = yuv2rgb_c_15;
      swap (pu, pv);
      break;
    case YUV420_RGB565:
      yuv2rgb_c_internal = yuv2rgb_c_16;
      break;
    case YVU420_RGB565:
      yuv2rgb_c_internal = yuv2rgb_c_16;
      swap (pu, pv);
      break;
    case YUV420_RGB24:
      yuv2rgb_c_internal = yuv2rgb_c_24_rgb;
      break;
    case YVU420_RGB24:
      yuv2rgb_c_internal = yuv2rgb_c_24_rgb;
      swap (pu, pv);
      break;
    case YUV420_BGR24:
      yuv2rgb_c_internal = yuv2rgb_c_24_bgr;
      break;
    case YVU420_BGR24:
      yuv2rgb_c_internal = yuv2rgb_c_24_bgr;
      swap (pu, pv);
      break;
    case YUV420_RGB32:
      yuv2rgb_c_internal = yuv2rgb_c_32;
      break;
    case YVU420_RGB32:
      yuv2rgb_c_internal = yuv2rgb_c_32;
      swap (pu, pv);
    default:
      g_assert_not_reached ();
      break;
    }

  v_size >>= 1;
  
  while (v_size--) {
    yuv2rgb_c_internal (py, py + y_stride, pu, pv, dst, dst + rgb_stride,
			h_size);
    
    py += 2 * y_stride;
    pu += uv_stride;
    pv += uv_stride;
    dst += 2 * rgb_stride;
  }
}

void startup_yuv2rgb (void) 
{
  CSFilter cfuncs[] =
    {
      {TVENG_PIX_YUV420, TVENG_PIX_RGB555, yuv2rgb_c_proxy,
       (void*)YUV420_RGB555},
      {TVENG_PIX_YVU420, TVENG_PIX_RGB555, yuv2rgb_c_proxy,
       (void*)YVU420_RGB555},
      {TVENG_PIX_YUV420, TVENG_PIX_RGB565, yuv2rgb_c_proxy,
       (void*)YUV420_RGB565},
      {TVENG_PIX_YVU420, TVENG_PIX_RGB565, yuv2rgb_c_proxy,
       (void*)YVU420_RGB565},
      {TVENG_PIX_YUV420, TVENG_PIX_RGB24, yuv2rgb_c_proxy,
       (void*)YUV420_RGB24},
      {TVENG_PIX_YVU420, TVENG_PIX_RGB24, yuv2rgb_c_proxy,
       (void*)YVU420_RGB24},
      {TVENG_PIX_YUV420, TVENG_PIX_BGR24, yuv2rgb_c_proxy,
       (void*)YUV420_BGR24},
      {TVENG_PIX_YVU420, TVENG_PIX_BGR24, yuv2rgb_c_proxy,
       (void*)YVU420_BGR24},
      {TVENG_PIX_YUV420, TVENG_PIX_RGB32, yuv2rgb_c_proxy,
       (void*)YUV420_RGB32},
      {TVENG_PIX_YVU420, TVENG_PIX_RGB32, yuv2rgb_c_proxy,
       (void*)YVU420_RGB32}
    };

  /* Try first the MMX versions of the functions */
  mmx_register_converters ();

  /* Register the C version of the converters when we don't have
     MMX versions registered */
  register_converters (cfuncs, sizeof(cfuncs)/sizeof(cfuncs[0]));
}

void shutdown_yuv2rgb (void)
{
}
