/*
 * yuv2rgb_mmx.c, Software YUV to RGB coverter with Intel MMX "technology"
 *
 * Copyright (C) 2000, Silicon Integrated System Corp.
 * All Rights Reserved.
 *
 * Author: Olie Lho <ollie@sis.com.tw>
 *
 * This file is part of mpeg2dec, a free MPEG-2 video decoder
 *
 * mpeg2dec is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * mpeg2dec is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Make; see the file COPYING. If not, write to
 * the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <stdlib.h>
#include <inttypes.h>
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifdef USE_MMX /* compile an empty file is mmx support is disabled */

#include "mmx.h"
#include "yuv2rgb.h"
#include "gen_conv.h"

yuv2rgb_fun yuv2rgb_init_mmx (int bpp, int mode)
{
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

    return NULL; // Fallback to C.
}

yuyv2rgb_fun yuyv2rgb_init_mmx (int bpp, int mode)
{
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

  return NULL; // Fallback to C.
}

#endif /* USE_MMX */
