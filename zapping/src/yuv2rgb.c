/*
 * yuv2rgb.c, Software YUV <-> RGB coverter
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
#include "libtv/cpu.h"
#include "globals.h"

#define MODE_RGB  0x1
#define MODE_BGR  0x2

/* Won't run on x86-64. */
#if defined (HAVE_X86) && defined (CAN_COMPILE_SSE)

typedef void
packed2planar_fn		(uint8_t *py,
				 uint8_t *pu,
				 uint8_t *pv,
				 const uint8_t *image,
				 int h_size,
				 int v_size,
				 unsigned int y_stride,
				 unsigned int uv_stride,
				 unsigned int rgb_stride);
typedef void
packed2packed_fn		(uint8_t *dst,
				 const uint8_t *src,
				 int h_size,
				 int v_size,
				 unsigned int dest_stride,
				 unsigned int src_stride);

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
      if ((p = rgb_to_yuv420_function (pixfmts[i])))
	{
	  register_converter ("-yuv420",
			      pixfmts[i], TV_PIXFMT_YUV420,
			      packed2planar_proxy, p);
	  register_converter ("-yvu420",
			      pixfmts[i], TV_PIXFMT_YVU420,
			      packed2planar_proxy, p);
	}

      if ((p = rgb_to_yuyv_function (pixfmts[i])))
	register_converter ("-yuyv",
			    pixfmts[i], TV_PIXFMT_YUYV,
			    packed2packed_proxy, p);
    }
}

#else /* !(HAVE_X86 && CAN_COMPILE_SSE) */

static void
mmx_register_converters (void)
{
}

#endif /* !(HAVE_X86 && CAN_COMPILE_SSE) */

void startup_yuv2rgb (void) 
{
  mmx_register_converters ();
}

void shutdown_yuv2rgb (void)
{
}

/*
Local variables:
c-set-style: gnu
c-basic-offset: 2
End:
*/
