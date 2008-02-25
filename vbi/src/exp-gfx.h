/*
 *  libzvbi - Closed Caption and Teletext rendering
 *
 *  Copyright (C) 2000, 2001, 2002, 2004 Michael H. Schimek
 *
 *  Based on code from AleVT 1.5.1
 *  Copyright (C) 1998, 1999 Edgar Toernig <froese@gmx.de>
 *  Copyright (C) 1999 Paul Ortyl <ortylp@from.pl>
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

/* $Id: exp-gfx.h,v 1.2.2.8 2008-02-25 21:00:47 mschimek Exp $ */

#ifndef __ZVBI3_EXP_GFX_H__
#define __ZVBI3_EXP_GFX_H__

#include <stdarg.h>		/* va_list */
#include "macros.h"
#include "page.h"		/* vbi3_page */
#include "image_format.h"	/* vbi3_image_format */

VBI3_BEGIN_DECLS

/**
 * @addtogroup Render
 * @{
 */
extern vbi3_bool
vbi3_page_draw_teletext_region_va_list
				(const vbi3_page *	pg,
				 void *			buffer,
				 const vbi3_image_format *format,
				 unsigned int		x,
				 unsigned int		y,
				 unsigned int		column,
				 unsigned int		row,
				 unsigned int		n_columns,
				 unsigned int		n_rows,
				 va_list		export_options)
  _vbi3_nonnull ((1, 2, 3));
extern vbi3_bool
vbi3_page_draw_teletext_region	(const vbi3_page *	pg,
				 void *			buffer,
				 const vbi3_image_format *format,
				 unsigned int		x,
				 unsigned int		y,
				 unsigned int		column,
				 unsigned int		row,
				 unsigned int		n_columns,
				 unsigned int		n_rows,
				 ...)
  _vbi3_nonnull ((1, 2, 3)) _vbi3_sentinel;
extern vbi3_bool
vbi3_page_draw_teletext_va_list	(const vbi3_page *	pg,
				 void *			buffer,
				 const vbi3_image_format *format,
				 va_list		export_options)
  _vbi3_nonnull ((1, 2, 3));
extern vbi3_bool
vbi3_page_draw_teletext		(const vbi3_page *	pg,
				 void *			buffer,
				 const vbi3_image_format *format,
				 ...)
  _vbi3_nonnull ((1, 2, 3)) _vbi3_sentinel;
extern vbi3_bool
vbi3_page_draw_caption_region_va_list
				(const vbi3_page *	pg,
				 void *			buffer,
				 const vbi3_image_format *format,
				 unsigned int		x,
				 unsigned int		y,
				 unsigned int		column,
				 unsigned int		row,
				 unsigned int		n_columns,
				 unsigned int		n_rows,
				 va_list		export_options)
  _vbi3_nonnull ((1, 2, 3));
extern vbi3_bool
vbi3_page_draw_caption_region	(const vbi3_page *	pg,
				 void *			buffer,
				 const vbi3_image_format *format,
				 unsigned int		x,
				 unsigned int		y,
				 unsigned int		column,
				 unsigned int		row,
				 unsigned int		n_columns,
				 unsigned int		n_rows,
				 ...)
  _vbi3_nonnull ((1, 2, 3)) _vbi3_sentinel;
extern vbi3_bool
vbi3_page_draw_caption_va_list	(const vbi3_page *	pg,
				 void *			buffer,
				 const vbi3_image_format *format,
				 va_list		export_options)
  _vbi3_nonnull ((1, 2, 3));
extern vbi3_bool
vbi3_page_draw_caption		(const vbi3_page *	pg,
				 void *			buffer,
				 const vbi3_image_format *format,
				 ...)
  _vbi3_nonnull ((1, 2)) _vbi3_sentinel;
/** @} */

VBI3_END_DECLS

#endif /* __ZVBI3_EXP_GFX_H__ */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
