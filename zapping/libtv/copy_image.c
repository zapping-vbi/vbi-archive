/*
 *  Copyright (C) 2006 Michael H. Schimek
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

/* $Id: copy_image.c,v 1.1 2006-04-12 01:48:15 mschimek Exp $ */

#include "copy_image-priv.h"

extern void
_tv_memcpy_MMX			(void *			dst,
				 const void *		src,
				 size_t			n_bytes);
extern void
_tv_memcpy_SSE			(void *			dst,
				 const void *		src,
				 size_t			n_bytes);

#if SIMD == CPU_FEATURE_MMX

void
_tv_memcpy_MMX			(void *			dst,
				 const void *		src,
				 size_t			n_bytes)
{
	__m64 *d = (__m64 *) dst;
	const __m64 *s = (const __m64 *) src;
	unsigned int count;

	for (count = n_bytes / 64; count > 0; --count) {
		__m64 m0, m1, m2, m3, m4, m5, m6, m7;

		m0 = s[0];
		m1 = s[1];
		m2 = s[2];
		m3 = s[3];
		m4 = s[4];
		m5 = s[5];
		m6 = s[6];
		m7 = s[7];
		s += 8;

		d[0] = m0;
		d[1] = m1;
		d[2] = m2;
		d[3] = m3;
		d[4] = m4;
		d[5] = m5;
		d[6] = m6;
		d[7] = m7;
		d += 8;
	}

	count = n_bytes % 64;

	__asm__ __volatile__ (" cld\n"
			      " rep movsb\n"
			      : "+D" (d), "+S" (s), "+c" (count)
			      :: "cc", "memory");    
}

#elif SIMD == CPU_FEATURE_SSE_INT

void
_tv_memcpy_SSE			(void *			dst,
				 const void *		src,
				 size_t			n_bytes)
{
	__m128 *d = (__m128 *) dst;
	const __m128 *s	= (const __m128 *) src;
	unsigned int count;

	for (count = n_bytes / 128; count > 0; --count) {
		__m128 m0, m1, m2, m3, m4, m5, m6, m7;

		m0 = s[0];
		m1 = s[1];
		m2 = s[2];
		m3 = s[3];
		m4 = s[4];
		m5 = s[5];
		m6 = s[6];
		m7 = s[7];
		s += 8;

		_mm_stream_ps ((float *)(d + 0), m0);
		_mm_stream_ps ((float *)(d + 1), m1);
		_mm_stream_ps ((float *)(d + 2), m2);
		_mm_stream_ps ((float *)(d + 3), m3);
		_mm_stream_ps ((float *)(d + 4), m4);
		_mm_stream_ps ((float *)(d + 5), m5);
		_mm_stream_ps ((float *)(d + 6), m6);
		_mm_stream_ps ((float *)(d + 7), m7);
		d += 8;
	}

	_mm_sfence ();

	count = n_bytes % 128;

	__asm__ __volatile__ (" cld\n"
			      " rep movsb\n"
			      : "+D" (d), "+S" (s), "+c" (count)
			      :: "cc", "memory");    
}

#elif SIMD == 0

/**
 * Copies @a n_bytes from @a src to @a dst using vector instructions and
 * cache bypassing loads and stores, if available. The function works faster
 * if @a src and @a dst are multiples of 8 or 16.
 *
 * XXX unchecked
 */
void
tv_memcpy			(void *			dst,
				 const void *		src,
				 size_t			n_bytes)
{
	if (unlikely (dst == src))
		return;

#if 0
#ifdef CAN_COMPILE_SSE
	if (UNTESTED_SIMD
	    && (cpu_features & CPU_FEATURE_SSE_INT))
		if (0 == (((unsigned long) dst |
			   (unsigned long) src) & 15))
			return _tv_memcpy_SSE (dst, src, n_bytes);
#endif
#endif
#ifdef CAN_COMPILE_MMX
	/* Is this really faster? */
	if (cpu_features & CPU_FEATURE_MMX)
		return _tv_memcpy_MMX (dst, src, n_bytes);
#endif
	memcpy (dst, src, n_bytes);
}

#endif /* !SIMD */


#if SIMD

#  if SIMD == CPU_FEATURE_SSE_INT
     /* Use float (128 bit) instructions. */
#    define vu8 vf
#    undef vloadnt
#    define vloadnt vloadfnt
#    undef vstorent
#    define vstorent vstorefnt
#  endif

void
SIMD_NAME (_tv_copy_plane)	(uint8_t *		dst,
				 const uint8_t *	src,
				 unsigned int		width,
				 unsigned int		height,
				 unsigned long		dst_padding,
				 unsigned long		src_padding)
{
	for (; height > 0; --height) {
		vu8 m0, m1, m2, m3, m4, m5, m6, m7;
		const uint8_t *end1;
		const uint8_t *end2;

		end1 = src + (width & (-8 * sizeof (vu8)));
		end2 = src + width;

		while (src < end1) {
			m0 = vloadnt (src, 0 * sizeof (vu8));
			m1 = vloadnt (src, 1 * sizeof (vu8));
			m2 = vloadnt (src, 2 * sizeof (vu8));
			m3 = vloadnt (src, 3 * sizeof (vu8));
			m4 = vloadnt (src, 4 * sizeof (vu8));
			m5 = vloadnt (src, 5 * sizeof (vu8));
			m6 = vloadnt (src, 6 * sizeof (vu8));
			m7 = vloadnt (src, 7 * sizeof (vu8));
			src += 8 * sizeof (vu8);

			vstorent (dst, 0 * sizeof (vu8), m0);
			vstorent (dst, 1 * sizeof (vu8), m1);
			vstorent (dst, 2 * sizeof (vu8), m2);
			vstorent (dst, 3 * sizeof (vu8), m3);
			vstorent (dst, 4 * sizeof (vu8), m4);
			vstorent (dst, 5 * sizeof (vu8), m5);
			vstorent (dst, 6 * sizeof (vu8), m6);
			vstorent (dst, 7 * sizeof (vu8), m7);
			dst += 8 * sizeof (vu8);
		}

		while (src < end2) {
			m0 = vloadnt (src, 0);
			src += sizeof (vu8);
			vstorent (dst, 0, m0);
			dst += sizeof (vu8);
		}

		src += src_padding;
		dst += dst_padding;
	}

	sfence ();
	vempty ();
}

#else /* !SIMD */

#define IMPL (SCALAR |							\
	      CPU_FEATURE_MMX |						\
	      CPU_FEATURE_SSE_FLT |					\
	      CPU_FEATURE_ALTIVEC)

void
_tv_copy_plane_SCALAR		(uint8_t *		dst,
				 const uint8_t *	src,
				 unsigned int		width,
				 unsigned int		height,
				 unsigned long		dst_padding,
				 unsigned long		src_padding)
{
	for (; height > 0; --height) {
		memcpy (dst, src, width);

		dst += width + dst_padding;
		src += width + src_padding;
	}
}

/*
 * XXX unchecked
 */
tv_bool
tv_copy_image			(void *			dst_image,
				 const tv_image_format *dst_format,
				 const void *		src_image,
				 const tv_image_format *src_format)
{
	const tv_pixel_format *pf;
	copy_plane_fn *copy_plane;
	uint8_t *dst;
	const uint8_t *src;
	unsigned int width;
	unsigned int height;
	unsigned long dst_padding;
	unsigned long src_padding;
	unsigned long align;

	if (unlikely (NULL == src_image))
		return tv_clear_image (dst_image, dst_format);

	assert (NULL != dst_image);

	if (unlikely (dst_image == src_image))
		return TRUE;

	assert (NULL != dst_format);
	assert (NULL != src_format);

	if (unlikely (dst_format->pixel_format
		      != src_format->pixel_format))
		return FALSE;

	width = MIN (dst_format->width, src_format->width);
	height = MIN (dst_format->height, src_format->height);

	if (0 == width || 0 == height)
		return FALSE;

	pf = dst_format->pixel_format;

	if (0 != ((width & pf->hmask) | (height & pf->vmask)))
		return FALSE;

	width = (width * pf->bits_per_pixel) >> 3;

	dst_padding = dst_format->bytes_per_line[0] - width;
	src_padding = src_format->bytes_per_line[0] - width;

	if (unlikely ((long)(dst_padding | src_padding) < 0))
		return FALSE;

	if (pf->n_planes > 1) {
		unsigned int uv_width;
		unsigned int uv_height;
		unsigned int block;
		unsigned long udst_padding;
		unsigned long usrc_padding;

		uv_width = width >> pf->uv_hshift;
		uv_height = height >> pf->uv_vshift;

		block = uv_width * uv_height;

		udst_padding = dst_format->bytes_per_line[1] - uv_width;
		usrc_padding = src_format->bytes_per_line[1] - uv_width;

		if (pf->n_planes > 2) {
			unsigned int v_width;
			unsigned int v_height;
			unsigned long vdst_padding;
			unsigned long vsrc_padding;

			vdst_padding = dst_format->bytes_per_line[2]
				- uv_width;
			vsrc_padding = src_format->bytes_per_line[2]
				- uv_width;

			align = vdst_padding | vsrc_padding;

			if (unlikely ((long)(udst_padding |
					     usrc_padding |
					     align) < 0))
				return FALSE;

			if (likely (0 == align)) {
				v_width = block;
				v_height = 1;
			} else {
				v_width = uv_width;
				v_height = uv_height;
			}

			align |= v_width;

			dst = (uint8_t *) dst_image + dst_format->offset[2];
			src = (const uint8_t *) src_image
				+ src_format->offset[2];

			align |= (unsigned long) dst;
			align |= (unsigned long) src;

			copy_plane = SIMD_FN_ALIGNED_SELECT (_tv_copy_plane,
							     align, IMPL);

			copy_plane (dst, src, v_width, v_height,
				    vdst_padding, vsrc_padding);
		} else {
			if (unlikely ((long)(udst_padding |
					     usrc_padding) < 0))
				return FALSE;
		}

		align = udst_padding | usrc_padding;

		if (likely (0 == align)) {
			uv_width = block;
			uv_height = 1;
		}

		align |= uv_width;

		dst = (uint8_t *) dst_image + dst_format->offset[1];
		src = (const uint8_t *) src_image + src_format->offset[1];

		align |= (unsigned long) dst;
		align |= (unsigned long) src;

		copy_plane = SIMD_FN_ALIGNED_SELECT (_tv_copy_plane,
						     align, IMPL);

		copy_plane (dst, src, uv_width, uv_height,
			    udst_padding, usrc_padding);
	}

	align = dst_padding | src_padding;

	if (likely (0 == align)) {
		width *= height;
		height = 1;
	}

	align |= width;

	dst = (uint8_t *) dst_image + dst_format->offset[0];
	src = (const uint8_t *) src_image + src_format->offset[0];

	align |= (unsigned long) dst;
	align |= (unsigned long) src;

	copy_plane = SIMD_FN_ALIGNED_SELECT (_tv_copy_plane, align, IMPL);

	copy_plane (dst, src, width, height, dst_padding, src_padding);

	return TRUE;
}

#endif /* !SIMD */
