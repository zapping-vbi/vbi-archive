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

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "options.h"
#include "fifo.h"
#include "mmx.h"
#include "rtepriv.h"
#include "video/video.h" /* fixme: video_unget_frame and friends */
#include "audio/audio.h" /* fixme: audio_read, audio_unget prots. */
#include "audio/mpeg.h"
// #include "convert_ry.h"

/* 1 if the tables contain useful data */
static int convert_started = 0;

/*
  These tables are used for fast conversion. For more info about the
  coefs. used, check the V4L2 Video Image Format Specification.
  These values are multiplied by 65536 for better precision.
*/
static int conv_ry[256];
static int conv_rcb[256];
static int conv_rcr[256];
static int conv_gy[256];
static int conv_gcb[256];
static int conv_gcr[256];
static int conv_by[256];
static int conv_bcb[256];
static int conv_bcr[256];

static void convert_init_tables( void )
{
	int i;
	int x;

	if (convert_started)
		return;

	for (i=0; i<256; i++)
	{
		x = i<<16;
		conv_ry[i] = 0.299*219*x/255;
		conv_rcb[i] = -0.1687*112*x/127;
		conv_rcr[i] = 0.5*112*x/127;

		conv_gy[i] = 0.567*219*x/255;
		conv_gcb[i] = -0.3313*112*x/127;
		conv_gcr[i] = -0.4187*112*x/127;

		conv_by[i] = 0.114*219*x/255;
		conv_bcb[i] = 0.5*112*x/127;
		conv_bcr[i] = -0.0813*112*x/127;
	}

	convert_started = 1;
}

/*
  Generic converter for RGB3, RGB4 to YCbCr420, being the YCbCr planes and
  RGB components in any order. Most of the real work is done here.
  jump: jump in bytes from a rgb component to the next (3 or 4)
  width: The width in pixels, it's assumed to be 4-multiplus
  height: The height in pixel, assumed to be even
  r, g, b: pointer to the first component in the image
  y, cb, cr: pointer to the places to store the data.
*/
static void convert_rgb_ycbcr(const char *_r, const char *_g,
			      const char *_b, int jump, int width,
			      int height, char *_y, char *_cb, char *_cr)
{
	int x, j;
	const char *r = _r, *g = _g, *b = _b;
	char *y = _y, *cb = _cb, *cr = _cr;
 
	CHECK("validate args", r != NULL);
	CHECK("validate args", g != NULL);
	CHECK("validate args", b != NULL);
	CHECK("validate args", y != NULL);
	CHECK("validate args", cb != NULL);
	CHECK("validate args", cr != NULL);
	CHECK("validate args", (width&~3) == 0);
	CHECK("validate args", (height&~1) == 0);

	convert_init_tables();

	/* We process 2*2 blocks, first the even lines */
	for (j=0; j<height; j+=2)
	{
		for (x=0;x<width; x+= 2)
		{
			register int R, G, B;
			R = *r;
			G = *g;
			B = *b;
			r += jump;
			g += jump;
			b += jump;
			*(y++) = ((conv_ry[R] + conv_gy[G] +
				   conv_by[B])>>16) + 16;
			*(cb) = conv_rcb[R] + conv_gcb[G] + conv_gcr[B];
			*(cr) = conv_rcr[R] + conv_gcr[G] + conv_bcr[B];
			R = *r;
			G = *g;
			B = *b;
			r += jump;
			g += jump;
			b += jump;
			*(y++) = ((conv_ry[R] + conv_gy[G] +
				   conv_by[B])>>16) + 16;
			*(cb++) += conv_rcb[R] + conv_gcb[G] + conv_gcr[B];
			*(cr++) += conv_rcr[R] + conv_gcr[G] + conv_bcr[B];
		}
		/* Skip the even line */
		y += width;
		r += width*jump;
		g += width*jump;
		b += width*jump;
	}

	/* Now go for the even lines */
	r = _r + width*jump;
	g = _g + width*jump;
	b = _b + width*jump;
	y = _y + width;
	cb = _cb;
	cr = _cr;

	for (j=1; j<height; j+=2)
	{
		for (x=0;x<width; x+= 2)
		{
			register int R, G, B;
			R = *r;
			G = *g;
			B = *b;
			r += jump;
			g += jump;
			b += jump;
			*(y++) = ((conv_ry[R] + conv_gy[G] +
				   conv_by[B])>>16) + 16;
			*(cb) += conv_rcb[R] + conv_gcb[G] + conv_gcr[B];
			*(cr) += conv_rcr[R] + conv_gcr[G] + conv_bcr[B];
			R = *r;
			G = *g;
			B = *b;
			r += jump;
			g += jump;
			b += jump;
			*(y++) = ((conv_ry[R] + conv_gy[G] +
				   conv_by[B])>>16) + 16;
			*(cb++) = ((*cb + conv_rcb[R] + conv_gcb[G] +
				    conv_gcr[B]) >> 16) + 128;
			*(cr++) = ((*cb + conv_rcr[R] + conv_gcr[G] +
				    conv_bcr[B]) >> 16) + 128;
		}
		/* Skip the odd line */
		y += width;
		r += width*jump;
		g += width*jump;
		b += width*jump;
	}
}

/*
  Generic converter from RGB555 to YCbCr420, YCrCb420
*/
static void convert_rgb555_ycbcr(const char *_src, int width, int
				 height, char *_y, char *_cb,
				 char *_cr)
{
	int x, j;
	const short *src = (short*) _src;
	char *y = _y, *cb = _cb, *cr = _cr;
 
	CHECK("validate args", src != NULL);
	CHECK("validate args", y != NULL);
	CHECK("validate args", cb != NULL);
	CHECK("validate args", cr != NULL);
	CHECK("validate args", (width&~3) == 0);
	CHECK("validate args", (height&~1) == 0);

	convert_init_tables();

	/* We process 2*2 blocks, first the even lines */
	for (j=0; j<height; j+=2)
	{
		for (x=0;x<width; x+= 2)
		{
			register int R, G, B;
			R = ((*src)&31)<<3;
			G = (((*src)&(31<<5)))>>2;
			B = ((*(src++))&(31<<10))>>7;
			*(y++) = ((conv_ry[R] + conv_gy[G] +
				   conv_by[B])>>16) + 16;
			*(cb) = conv_rcb[R] + conv_gcb[G] + conv_gcr[B];
			*(cr) = conv_rcr[R] + conv_gcr[G] + conv_bcr[B];
			R = ((*src)&31)<<3;
			G = (((*src)&(31<<5)))>>2;
			B = ((*(src++))&(31<<10))>>7;
			*(y++) = ((conv_ry[R] + conv_gy[G] +
				   conv_by[B])>>16) + 16;
			*(cb++) += conv_rcb[R] + conv_gcb[G] + conv_gcr[B];
			*(cr++) += conv_rcr[R] + conv_gcr[G] + conv_bcr[B];
		}
		/* Skip the even line */
		y += width;
		src += (width<<1);
	}

	/* Now go for the even lines */
	src = ((short*)_src) + (width<<1);
	y = _y + width;
	cb = _cb;
	cr = _cr;

	for (j=1; j<height; j+=2)
	{
		for (x=0;x<width; x+= 2)
		{
			register int R, G, B;
			R = ((*src)&31)<<3;
			G = (((*src)&(31<<5)))>>2;
			B = ((*(src++))&(31<<10))>>7;
			*(y++) = ((conv_ry[R] + conv_gy[G] +
				   conv_by[B])>>16) + 16;
			*(cb) += conv_rcb[R] + conv_gcb[G] + conv_gcr[B];
			*(cr) += conv_rcr[R] + conv_gcr[G] + conv_bcr[B];
			R = ((*src)&31)<<3;
			G = (((*src)&(31<<5)))>>2;
			B = ((*(src++))&(31<<10))>>7;
			*(y++) = ((conv_ry[R] + conv_gy[G] +
				   conv_by[B])>>16) + 16;
			*(cb++) = ((*cb + conv_rcb[R] + conv_gcb[G] +
				    conv_gcr[B]) >> 16) + 128;
			*(cr++) = ((*cb + conv_rcr[R] + conv_gcr[G] +
				    conv_bcr[B]) >> 16) + 128;
		}
		/* Skip the odd line */
		y += width;
		src += width<<1;
	}
}

/*
  Generic converter from RGB565 to YCbCr420, YCrCb420
*/
static void convert_rgb565_ycbcr(const char *_src, int width, int
				 height, char *_y, char *_cb,
				 char *_cr)
{
	int x, j;
	const short *src = (short*) _src;
	char *y = _y, *cb = _cb, *cr = _cr;
 
	CHECK("validate args", src != NULL);
	CHECK("validate args", y != NULL);
	CHECK("validate args", cb != NULL);
	CHECK("validate args", cr != NULL);
	CHECK("validate args", (width&~3) == 0);
	CHECK("validate args", (height&~1) == 0);

	convert_init_tables();

	/* We process 2*2 blocks, first the even lines */
	for (j=0; j<height; j+=2)
	{
		for (x=0;x<width; x+= 2)
		{
			register int R, G, B;
			R = ((*src)&31)<<3;
			G = (((*src)&(63<<5)))>>3;
			B = ((*(src++))&(31<<11))>>8;
			*(y++) = ((conv_ry[R] + conv_gy[G] +
				   conv_by[B])>>16) + 16;
			*(cb) = conv_rcb[R] + conv_gcb[G] + conv_gcr[B];
			*(cr) = conv_rcr[R] + conv_gcr[G] + conv_bcr[B];
			R = ((*src)&31)<<3;
			G = (((*src)&(63<<5)))>>3;
			B = ((*(src++))&(31<<11))>>8;
			*(y++) = ((conv_ry[R] + conv_gy[G] +
				   conv_by[B])>>16) + 16;
			*(cb++) += conv_rcb[R] + conv_gcb[G] + conv_gcr[B];
			*(cr++) += conv_rcr[R] + conv_gcr[G] + conv_bcr[B];
		}
		/* Skip the even line */
		y += width;
		src += (width<<1);
	}


	/* Now go for the even lines */
	src = ((short*)_src) + (width<<1);
	y = _y + width;
	cb = _cb;
	cr = _cr;

	for (j=1; j<height; j+=2)
	{
		for (x=0;x<width; x+= 2)
		{
			register int R, G, B;
			R = ((*src)&31)<<3;
			G = (((*src)&(63<<5)))>>3;
			B = ((*(src++))&(31<<11))>>8;
			*(y++) = ((conv_ry[R] + conv_gy[G] +
				   conv_by[B])>>16) + 16;
			*(cb) += conv_rcb[R] + conv_gcb[G] + conv_gcr[B];
			*(cr) += conv_rcr[R] + conv_gcr[G] + conv_bcr[B];
			R = ((*src)&31)<<3;
			G = (((*src)&(63<<5)))>>3;
			B = ((*(src++))&(31<<11))>>8;
			*(y++) = ((conv_ry[R] + conv_gy[G] +
				  conv_by[B])>>16) + 16;
			*(cb++) = ((*cb + conv_rcb[R] + conv_gcb[G] +
				    conv_gcr[B]) >> 16) + 128;
			*(cr++) = ((*cb + conv_rcr[R] + conv_gcr[G] +
				    conv_bcr[B]) >> 16) + 128;
		}
		/* Skip the odd line */
		y += width;
		src += width<<1;
	}
}

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
		       height)
{
	convert_rgb_ycbcr(src, src+1, src+2, 3, width, height, dest,
			  dest+(width*height), dest+((width*height)*5/4));
}

/**
 * convert_rgb24_ycrcb420: Converts the given data from the rgb24 colorspace
 * to (planar) YCrCb420.
 * src: The original data
 * dest: A buffer to hold the converted data, must be of enough size
 * (width * height * 1.5 bytes )
 * width: Width of the image, must be 4-multiplus
 * height: Height of the image, must be 4-multiplus too
*/
void 
convert_rgb24_ycrcb420(const char * src, char * dest, int width, int
		       height)
{
	convert_rgb_ycbcr(src, src+1, src+2, 3, width, height, dest,
			  dest+((width*height)*5/4), dest + (width*height));
}

/**
 * convert_bgr24_ycbcr420: Converts the given data from the bgr24 colorspace
 * to (planar) YCbCr420.
 * src: The original data
 * dest: A buffer to hold the converted data, must be of enough size
 * (width * height * 1.5 bytes )
 * width: Width of the image, must be 4-multiplus
 * height: Height of the image, must be 4-multiplus too
*/
void 
convert_bgr24_ycbcr420(const char * src, char * dest, int width, int
		       height)
{
	convert_rgb_ycbcr(src+2, src+1, src, 3, width, height, dest,
			  dest+(width*height), dest+((width*height)*5/4));

}

/**
 * convert_bgr24_ycrcb420: Converts the given data from the bgr24 colorspace
 * to (planar) YCrCb420.
 * src: The original data
 * dest: A buffer to hold the converted data, must be of enough size
 * (width * height * 1.5 bytes )
 * width: Width of the image, must be 4-multiplus
 * height: Height of the image, must be 4-multiplus too
*/
void 
convert_bgr24_ycrcb420(const char * src, char * dest, int width, int
		       height)
{
	convert_rgb_ycbcr(src+2, src+1, src, 3, width, height, dest,
			  dest+((width*height)*5/4), dest+(width*height));

}

/**
 * convert_rgb32_ycbcr420: Converts the given data from the rgb32 colorspace
 * to (planar) YCbCr420.
 * src: The original data
 * dest: A buffer to hold the converted data, must be of enough size
 * (width * height * 1.5 bytes )
 * width: Width of the image, must be 4-multiplus
 * height: Height of the image, must be 4-multiplus too
*/
void 
convert_rgb32_ycbcr420(const char * src, char * dest, int width, int
		       height)
{
	convert_rgb_ycbcr(src, src+1, src+2, 4, width, height, dest,
			  dest+(width*height), dest+((width*height)*5/4));
}

/**
 * convert_rgb32_ycrcb420: Converts the given data from the rgb32 colorspace
 * to (planar) YCrCb420.
 * src: The original data
 * dest: A buffer to hold the converted data, must be of enough size
 * (width * height * 1.5 bytes )
 * width: Width of the image, must be 4-multiplus
 * height: Height of the image, must be 4-multiplus too
*/
void 
convert_rgb32_ycrcb420(const char * src, char * dest, int width, int
		       height)
{
	convert_rgb_ycbcr(src, src+1, src+2, 4, width, height, dest,
			  dest+((width*height)*5/4), dest + (width*height));
}

/**
 * convert_bgr32_ycbcr420: Converts the given data from the bgr32 colorspace
 * to (planar) YCbCr420.
 * src: The original data
 * dest: A buffer to hold the converted data, must be of enough size
 * (width * height * 1.5 bytes )
 * width: Width of the image, must be 4-multiplus
 * height: Height of the image, must be 4-multiplus too
*/
void 
convert_bgr32_ycbcr420(const char * src, char * dest, int width, int
		       height)
{
	convert_rgb_ycbcr(src+2, src+1, src, 4, width, height, dest,
			  dest+(width*height), dest+((width*height)*5/4));

}

/**
 * convert_bgr32_ycrcb420: Converts the given data from the bgr32 colorspace
 * to (planar) YCrCb420.
 * src: The original data
 * dest: A buffer to hold the converted data, must be of enough size
 * (width * height * 1.5 bytes )
 * width: Width of the image, must be 4-multiplus
 * height: Height of the image, must be 4-multiplus too
*/
void 
convert_bgr32_ycrcb420(const char * src, char * dest, int width, int
		       height)
{
	convert_rgb_ycbcr(src+2, src+1, src, 4, width, height, dest,
			  dest+((width*height)*5/4), dest+(width*height));

}

/**
 * convert_rgb555_ycbcr420: Converts the given data from the rgb555 colorspace
 * to (planar) YCbCr420.
 * src: The original data
 * dest: A buffer to hold the converted data, must be of enough size
 * (width * height * 1.5 bytes )
 * width: Width of the image, must be 4-multiplus
 * height: Height of the image, must be 4-multiplus too
*/
void 
convert_rgb555_ycbcr420(const char * src, char * dest, int width, int
			height)
{
	convert_rgb555_ycbcr(src, width, height, dest,
			     dest+(width*height), dest+((width*height)*5/4));
}

/**
 * convert_rgb555_ycrcb420: Converts the given data from the rgb555 colorspace
 * to (planar) YCrCb420.
 * src: The original data
 * dest: A buffer to hold the converted data, must be of enough size
 * (width * height * 1.5 bytes )
 * width: Width of the image, must be 4-multiplus
 * height: Height of the image, must be 4-multiplus too
*/
void 
convert_rgb555_ycrcb420(const char * src, char * dest, int width, int
			height)
{
	convert_rgb555_ycbcr(src, width, height, dest,
			     dest+((width*height)*5/4), dest+(width*height));
}

/**
 * convert_rgb565_ycbcr420: Converts the given data from the rgb555 colorspace
 * to (planar) YCbCr420.
 * src: The original data
 * dest: A buffer to hold the converted data, must be of enough size
 * (width * height * 1.5 bytes )
 * width: Width of the image, must be 4-multiplus
 * height: Height of the image, must be 4-multiplus too
*/
void 
convert_rgb565_ycbcr420(const char * src, char * dest, int width, int
			height)
{
	convert_rgb565_ycbcr(src, width, height, dest,
			     dest+(width*height), dest+((width*height)*5/4));
}

/**
 * convert_rgb565_ycrcb420: Converts the given data from the rgb555 colorspace
 * to (planar) YCrCb420.
 * src: The original data
 * dest: A buffer to hold the converted data, must be of enough size
 * (width * height * 1.5 bytes )
 * width: Width of the image, must be 4-multiplus
 * height: Height of the image, must be 4-multiplus too
*/
void 
convert_rgb565_ycrcb420(const char * src, char * dest, int width, int
			height)
{
	convert_rgb565_ycbcr(src, width, height, dest,
			     dest+((width*height)*5/4), dest+(width*height));
}
