/*
 *  Zapping (TV viewer for the Gnome Desktop)
 *  MMX color conversion routines
 *
 *  Copyright (C) 2001 Michael H. Schimek <mschimek@users.sf.net>
 *
 *  Contains code from mpeg2dec yuv2rgb_mmx.c,
 *  Software YUV to RGB converter with Intel MMX "technology"
 *
 *  Copyright (C) 2000, Silicon Integrated System Corp.
 *  All Rights Reserved.
 *
 *  Author: Olie Lho <ollie@sis.com.tw>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* Generated file - do not edit */

#ifndef _GEN_CONV_H__
#define _GEN_CONV_H__

#include <stdio.h>
#include <stdlib.h>

extern void mmx_yuyv_rgb32(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void mmx_yvyu_rgb32(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void mmx_uyvy_rgb32(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void mmx_vyuy_rgb32(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void mmx_yuv420_rgb32(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void mmx_yuyv_bgr32(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void mmx_yvyu_bgr32(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void mmx_uyvy_bgr32(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void mmx_vyuy_bgr32(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void mmx_yuv420_bgr32(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void mmx_yuyv_rgb24(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void mmx_yvyu_rgb24(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void mmx_uyvy_rgb24(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void mmx_vyuy_rgb24(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void mmx_yuv420_rgb24(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void mmx_yuyv_bgr24(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void mmx_yvyu_bgr24(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void mmx_uyvy_bgr24(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void mmx_vyuy_bgr24(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void mmx_yuv420_bgr24(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void mmx_yuyv_rgb565(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void mmx_yvyu_rgb565(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void mmx_uyvy_rgb565(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void mmx_vyuy_rgb565(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void mmx_yuv420_rgb565(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void mmx_yuyv_bgr565(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void mmx_yvyu_bgr565(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void mmx_uyvy_bgr565(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void mmx_vyuy_bgr565(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void mmx_yuv420_bgr565(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void mmx_yuyv_rgb5551(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void mmx_yvyu_rgb5551(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void mmx_uyvy_rgb5551(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void mmx_vyuy_rgb5551(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void mmx_yuv420_rgb5551(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void mmx_yuyv_bgr5551(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void mmx_yvyu_bgr5551(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void mmx_uyvy_bgr5551(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void mmx_vyuy_bgr5551(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void mmx_yuv420_bgr5551(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void sse_yuyv_rgb32(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void sse_yvyu_rgb32(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void sse_uyvy_rgb32(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void sse_vyuy_rgb32(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void sse_yuv420_rgb32(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void sse_yuyv_bgr32(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void sse_yvyu_bgr32(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void sse_uyvy_bgr32(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void sse_vyuy_bgr32(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void sse_yuv420_bgr32(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void sse_yuyv_rgb24(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void sse_yvyu_rgb24(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void sse_uyvy_rgb24(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void sse_vyuy_rgb24(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void sse_yuv420_rgb24(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void sse_yuyv_bgr24(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void sse_yvyu_bgr24(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void sse_uyvy_bgr24(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void sse_vyuy_bgr24(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void sse_yuv420_bgr24(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void sse_yuyv_rgb565(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void sse_yvyu_rgb565(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void sse_uyvy_rgb565(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void sse_vyuy_rgb565(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void sse_yuv420_rgb565(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void sse_yuyv_bgr565(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void sse_yvyu_bgr565(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void sse_uyvy_bgr565(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void sse_vyuy_bgr565(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void sse_yuv420_bgr565(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void sse_yuyv_rgb5551(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void sse_yvyu_rgb5551(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void sse_uyvy_rgb5551(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void sse_vyuy_rgb5551(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void sse_yuv420_rgb5551(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void sse_yuyv_bgr5551(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void sse_yvyu_bgr5551(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void sse_uyvy_bgr5551(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void sse_vyuy_bgr5551(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void sse_yuv420_bgr5551(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void _3dnow_yuyv_rgb32(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void _3dnow_yvyu_rgb32(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void _3dnow_uyvy_rgb32(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void _3dnow_vyuy_rgb32(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void _3dnow_yuv420_rgb32(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void _3dnow_yuyv_bgr32(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void _3dnow_yvyu_bgr32(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void _3dnow_uyvy_bgr32(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void _3dnow_vyuy_bgr32(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void _3dnow_yuv420_bgr32(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void _3dnow_yuyv_rgb24(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void _3dnow_yvyu_rgb24(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void _3dnow_uyvy_rgb24(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void _3dnow_vyuy_rgb24(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void _3dnow_yuv420_rgb24(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void _3dnow_yuyv_bgr24(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void _3dnow_yvyu_bgr24(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void _3dnow_uyvy_bgr24(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void _3dnow_vyuy_bgr24(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void _3dnow_yuv420_bgr24(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void _3dnow_yuyv_rgb565(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void _3dnow_yvyu_rgb565(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void _3dnow_uyvy_rgb565(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void _3dnow_vyuy_rgb565(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void _3dnow_yuv420_rgb565(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void _3dnow_yuyv_bgr565(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void _3dnow_yvyu_bgr565(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void _3dnow_uyvy_bgr565(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void _3dnow_vyuy_bgr565(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void _3dnow_yuv420_bgr565(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void _3dnow_yuyv_rgb5551(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void _3dnow_yvyu_rgb5551(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void _3dnow_uyvy_rgb5551(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void _3dnow_vyuy_rgb5551(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void _3dnow_yuv420_rgb5551(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void _3dnow_yuyv_bgr5551(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void _3dnow_yvyu_bgr5551(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void _3dnow_uyvy_bgr5551(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void _3dnow_vyuy_bgr5551(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void _3dnow_yuv420_bgr5551(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void amd_yuyv_rgb32(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void amd_yvyu_rgb32(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void amd_uyvy_rgb32(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void amd_vyuy_rgb32(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void amd_yuv420_rgb32(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void amd_yuyv_bgr32(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void amd_yvyu_bgr32(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void amd_uyvy_bgr32(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void amd_vyuy_bgr32(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void amd_yuv420_bgr32(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void amd_yuyv_rgb24(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void amd_yvyu_rgb24(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void amd_uyvy_rgb24(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void amd_vyuy_rgb24(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void amd_yuv420_rgb24(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void amd_yuyv_bgr24(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void amd_yvyu_bgr24(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void amd_uyvy_bgr24(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void amd_vyuy_bgr24(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void amd_yuv420_bgr24(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void amd_yuyv_rgb565(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void amd_yvyu_rgb565(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void amd_uyvy_rgb565(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void amd_vyuy_rgb565(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void amd_yuv420_rgb565(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void amd_yuyv_bgr565(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void amd_yvyu_bgr565(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void amd_uyvy_bgr565(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void amd_vyuy_bgr565(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void amd_yuv420_bgr565(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void amd_yuyv_rgb5551(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void amd_yvyu_rgb5551(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void amd_uyvy_rgb5551(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void amd_vyuy_rgb5551(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void amd_yuv420_rgb5551(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void amd_yuyv_bgr5551(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void amd_yvyu_bgr5551(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void amd_uyvy_bgr5551(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void amd_vyuy_bgr5551(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void amd_yuv420_bgr5551(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);

#endif /* _GEN_CONV_H__ */
