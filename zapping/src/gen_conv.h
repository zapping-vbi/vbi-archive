/*
 *  Zapping (TV viewer for the Gnome Desktop)
 *  SWAR color conversion routines
 *
 *  Copyright (C) 2001 Michael H. Schimek <mschimek@users.sf.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
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
extern void mmx_yuv420_rgb32(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void mmx_yuyv_bgr32(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void mmx_yuv420_bgr32(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void mmx_yuyv_rgb24(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void mmx_yuv420_rgb24(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void mmx_yuyv_bgr24(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void mmx_yuv420_bgr24(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void mmx_yuyv_rgb565(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void mmx_yuv420_rgb565(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void mmx_yuyv_bgr565(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void mmx_yuv420_bgr565(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void mmx_yuyv_rgb5551(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void mmx_yuv420_rgb5551(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void mmx_yuyv_bgr5551(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void mmx_yuv420_bgr5551(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void mmx_rgb32_yuyv(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void mmx_bgr32_yuyv(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void mmx_rgb24_yuyv(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void mmx_bgr24_yuyv(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void mmx_rgb565_yuyv(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void mmx_bgr565_yuyv(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void mmx_rgb5551_yuyv(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void mmx_bgr5551_yuyv(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void mmx_rgb32_yuv420(unsigned char *d, unsigned char *d_u, unsigned char *d_v, unsigned char *s, int w, int h, int d_stride, int d_uv_stride, int s_stride);
extern void mmx_bgr32_yuv420(unsigned char *d, unsigned char *d_u, unsigned char *d_v, unsigned char *s, int w, int h, int d_stride, int d_uv_stride, int s_stride);
extern void mmx_rgb24_yuv420(unsigned char *d, unsigned char *d_u, unsigned char *d_v, unsigned char *s, int w, int h, int d_stride, int d_uv_stride, int s_stride);
extern void mmx_bgr24_yuv420(unsigned char *d, unsigned char *d_u, unsigned char *d_v, unsigned char *s, int w, int h, int d_stride, int d_uv_stride, int s_stride);
extern void mmx_rgb565_yuv420(unsigned char *d, unsigned char *d_u, unsigned char *d_v, unsigned char *s, int w, int h, int d_stride, int d_uv_stride, int s_stride);
extern void mmx_bgr565_yuv420(unsigned char *d, unsigned char *d_u, unsigned char *d_v, unsigned char *s, int w, int h, int d_stride, int d_uv_stride, int s_stride);
extern void mmx_rgb5551_yuv420(unsigned char *d, unsigned char *d_u, unsigned char *d_v, unsigned char *s, int w, int h, int d_stride, int d_uv_stride, int s_stride);
extern void mmx_bgr5551_yuv420(unsigned char *d, unsigned char *d_u, unsigned char *d_v, unsigned char *s, int w, int h, int d_stride, int d_uv_stride, int s_stride);
extern void sse_yuyv_rgb32(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void sse_yuv420_rgb32(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void sse_yuyv_bgr32(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void sse_yuv420_bgr32(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void sse_yuyv_rgb24(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void sse_yuv420_rgb24(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void sse_yuyv_bgr24(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void sse_yuv420_bgr24(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void sse_yuyv_rgb565(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void sse_yuv420_rgb565(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void sse_yuyv_bgr565(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void sse_yuv420_bgr565(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void sse_yuyv_rgb5551(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void sse_yuv420_rgb5551(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void sse_yuyv_bgr5551(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void sse_yuv420_bgr5551(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void _3dn_yuyv_rgb32(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void _3dn_yuv420_rgb32(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void _3dn_yuyv_bgr32(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void _3dn_yuv420_bgr32(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void _3dn_yuyv_rgb24(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void _3dn_yuv420_rgb24(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void _3dn_yuyv_bgr24(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void _3dn_yuv420_bgr24(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void _3dn_yuyv_rgb565(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void _3dn_yuv420_rgb565(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void _3dn_yuyv_bgr565(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void _3dn_yuv420_bgr565(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void _3dn_yuyv_rgb5551(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void _3dn_yuv420_rgb5551(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void _3dn_yuyv_bgr5551(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void _3dn_yuv420_bgr5551(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void amd_yuyv_rgb32(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void amd_yuv420_rgb32(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void amd_yuyv_bgr32(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void amd_yuv420_bgr32(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void amd_yuyv_rgb24(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void amd_yuv420_rgb24(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void amd_yuyv_bgr24(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void amd_yuv420_bgr24(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void amd_yuyv_rgb565(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void amd_yuv420_rgb565(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void amd_yuyv_bgr565(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void amd_yuv420_bgr565(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void amd_yuyv_rgb5551(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void amd_yuv420_rgb5551(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);
extern void amd_yuyv_bgr5551(unsigned char *d, unsigned char *s, int w, int h, int d_stride, int s_stride);
extern void amd_yuv420_bgr5551(unsigned char *d, unsigned char *s, unsigned char *s_u, unsigned char *s_v, int w, int h, int d_stride, int s_stride, int s_uv_stride);

#endif /* _GEN_CONV_H__ */
