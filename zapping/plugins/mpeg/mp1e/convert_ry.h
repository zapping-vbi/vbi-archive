/*
 * Some unoptimized filters to convert from the RGB colorspace to
 * YCbCr420
 * The formats byte order is assumed to be the same as the one
 * described in the V4L2 Video Image Format Specification
 *
 *  Copyright (C) 2000 Iñaki García Etxebarria
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

#ifndef __CONVERT_RY_H__
#define __CONVERT_RY_H__

/**
 * convert_rgb24_ycbcr420: Converts the given data from the rgb24 colorspace
 * to (planar) YCbCr420.
 * src: The original data
 * dest: A buffer to hold the converted data, must be of enough size
 * (width * height * 1.5 bytes )
 * width: Width of the image, must be 4-multiplus
 * height: Height of the image, must be 4-multiplus too
*/
void 
convert_rgb24_ycbcr420(const char * src, char * dest, int width, int
		       height);
void 
convert_rgb24_ycrcb420(const char * src, char * dest, int width, int
		       height);
void 
convert_bgr24_ycbcr420(const char * src, char * dest, int width, int
		       height);
void 
convert_bgr24_ycrcb420(const char * src, char * dest, int width, int
		       height);
void 
convert_rgb32_ycbcr420(const char * src, char * dest, int width, int
		       height);
void 
convert_rgb32_ycrcb420(const char * src, char * dest, int width, int
		       height);
void 
convert_bgr32_ycbcr420(const char * src, char * dest, int width, int
		       height);
void 
convert_bgr32_ycrcb420(const char * src, char * dest, int width, int
		       height);
void 
convert_rgb555_ycbcr420(const char * src, char * dest, int width, int
			height);
void 
convert_rgb555_ycrcb420(const char * src, char * dest, int width, int
			height);
void 
convert_rgb565_ycbcr420(const char * src, char * dest, int width, int
			height);
void 
convert_rgb565_ycrcb420(const char * src, char * dest, int width, int
			height);

/*
  Generic converter for RGB3, RGB4 to YCbCr420, being the YCbCr planes and
  RGB components in any order. Most of the real work is done here.
  jump: jump in bytes from a rgb component to the next (3 or 4)
  width: The width in pixels, it's assumed to be 4-multiplus
  height: The height in pixel, assumed to be even
  r, g, b: pointer to the first component in the image
  y, cb, cr: pointer to the places to store the data.
*/
void convert_rgb_ycbcr(const unsigned char *_r, const unsigned char *_g,
		       const unsigned char *_b, int jump, int width,
		       int height, unsigned char *_y, unsigned char *_cb, unsigned char *_cr);

/*
  Generic converter from RGB555 to YCbCr420, YCrCb420
*/
void convert_rgb555_ycbcr(const char *_src, int width, int
			  height, char *_y, char *_cb,
			  char *_cr);

/*
  Generic converter from RGB565 to YCbCr420, YCrCb420
*/
void convert_rgb565_ycbcr(const char *_src, int width, int
			  height, char *_y, char *_cb,
			  char *_cr);
#endif /* convert_ry.h */
