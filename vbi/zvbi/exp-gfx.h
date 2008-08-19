/*
 *  libzvbi - Closed Caption and Teletext rendering
 *
 *  Copyright (C) 2000-2008 Michael H. Schimek
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the 
 *  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, 
 *  Boston, MA  02110-1301  USA.
 */

/* $Id: exp-gfx.h,v 1.1.2.2 2008-08-19 16:36:50 mschimek Exp $ */

#ifndef __ZVBI_EXP_GFX_H__
#define __ZVBI_EXP_GFX_H__

#include <stdarg.h>
#include "zvbi/page.h"
#include "zvbi/image_format.h"

VBI_BEGIN_DECLS

/**
 */
typedef enum {
	VBI_TABLE = 0x32F54A01,
	VBI_RTL,
	VBI_SCALE,
	VBI_REVEAL,
	VBI_FLASH_ON,
	VBI_BRIGHTNESS,
	VBI_CONTRAST,
	VBI_ALPHA,
} vbi_export_option;

/**
 * @addtogroup Render
 * @{
 */
extern vbi_bool
vbi_ttx_page_draw_region_va	(const vbi_page *	pg,
				 void *			buffer,
				 const vbi_image_format *format,
				 unsigned int		x,
				 unsigned int		y,
				 unsigned int		column,
				 unsigned int		row,
				 unsigned int		n_columns,
				 unsigned int		n_rows,
				 va_list		options)
  _vbi_nonnull ((1, 2, 3));
extern vbi_bool
vbi_ttx_page_draw_region	(const vbi_page *	pg,
				 void *			buffer,
				 const vbi_image_format *format,
				 unsigned int		x,
				 unsigned int		y,
				 unsigned int		column,
				 unsigned int		row,
				 unsigned int		n_columns,
				 unsigned int		n_rows,
				 ...)
  _vbi_nonnull ((1, 2, 3)) _vbi_sentinel;
extern vbi_bool
vbi_ttx_page_draw_va		(const vbi_page *	pg,
				 void *			buffer,
				 const vbi_image_format *format,
				 va_list		options)
  _vbi_nonnull ((1, 2, 3));
extern vbi_bool
vbi_ttx_page_draw		(const vbi_page *	pg,
				 void *			buffer,
				 const vbi_image_format *format,
				 ...)
  _vbi_nonnull ((1, 2, 3)) _vbi_sentinel;
/** @} */

VBI_END_DECLS

#endif /* __ZVBI_EXP_GFX_H__ */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
