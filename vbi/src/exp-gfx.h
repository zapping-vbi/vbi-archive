/*
 *  libzvbi - Closed Caption and Teletext rendering
 *
 *  Copyright (C) 2000, 2001, 2002 Michael H. Schimek
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

/* $Id: exp-gfx.h,v 1.2.2.5 2004-04-08 23:36:25 mschimek Exp $ */

#ifndef __ZVBI_EXP_GFX_H__
#define __ZVBI_EXP_GFX_H__

#include <stdarg.h>		/* va_list */
#include "macros.h"
#include "format.h"		/* vbi_page */
#include "graphics.h"		/* vbi_image_format */

VBI_BEGIN_DECLS

/**
 * @addtogroup Render
 * @{
 */
extern vbi_bool
vbi_draw_vt_page_region_va_list	(const vbi_page *	pg,
				 void *			buffer,
				 const vbi_image_format *format,
				 unsigned int		column,
				 unsigned int		row,
				 unsigned int		width,
				 unsigned int		height,
				 va_list		export_options);
extern vbi_bool
vbi_draw_vt_page_region		(const vbi_page *	pg,
				 void *			buffer,
				 const vbi_image_format *format,
				 unsigned int		column,
				 unsigned int		row,
				 unsigned int		width,
				 unsigned int		height,
				 ...);
extern vbi_bool
vbi_draw_vt_page_va_list	(const vbi_page *	pg,
				 void *			buffer,
				 const vbi_image_format *format,
				 va_list		export_options);
extern vbi_bool
vbi_draw_vt_page		(const vbi_page *	pg,
				 void *			buffer,
				 const vbi_image_format *format,
				 ...);
extern vbi_bool
vbi_draw_cc_page_region_va_list	(const vbi_page *	pg,
				 void *			buffer,
				 const vbi_image_format *format,
				 unsigned int		column,
				 unsigned int		row,
				 unsigned int		width,
				 unsigned int		height,
				 va_list		export_options);
extern vbi_bool
vbi_draw_cc_page_region		(const vbi_page *	pg,
				 void *			buffer,
				 const vbi_image_format *format,
				 unsigned int		column,
				 unsigned int		row,
				 unsigned int		width,
				 unsigned int		height,
				 ...);
extern vbi_bool
vbi_draw_cc_page_va_list	(const vbi_page *	pg,
				 void *			buffer,
				 const vbi_image_format *format,
				 va_list		export_options);
extern vbi_bool
vbi_draw_cc_page		(const vbi_page *	pg,
				 void *			buffer,
				 const vbi_image_format *format,
				 ...);
/** @} */

VBI_END_DECLS

#endif /* __ZVBI_EXP_GFX_H__ */
