/*
 *  libzvbi - Text export functions
 *
 *  Copyright (C) 2001, 2002, 2003, 2004 Michael H. Schimek
 *
 *  Based on code from AleVT 1.5.1
 *  Copyright (C) 1998, 1999 Edgar Toernig <froese@gmx.de>
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

/* $Id: exp-txt.h,v 1.2 2005-01-08 14:54:20 mschimek Exp $ */

#ifndef __ZVBI3_EXP_TXT_H__
#define __ZVBI3_EXP_TXT_H__

#include <stdarg.h>		/* va_list */
#include "macros.h"
#include "page.h"		/* vbi3_page */

VBI3_BEGIN_DECLS

/**
 * @addtogroup Render
 * @{
 */
extern unsigned int
vbi3_print_page_region_va_list	(vbi3_page *		pg,
				 char *			buffer,
				 unsigned int		buffer_size,
				 const char *		format,
				 const char *		separator,
				 unsigned int		separator_size,
				 unsigned int		column,
				 unsigned int		row,
				 unsigned int		width,
				 unsigned int		height,
				 va_list		export_options);
extern unsigned int
vbi3_print_page_region		(vbi3_page *		pg,
				 char *			buffer,
				 unsigned int		buffer_size,
				 const char *		format,
				 const char *		separator,
				 unsigned int		separator_size,
				 unsigned int		column,
				 unsigned int		row,
				 unsigned int		width,
				 unsigned int		height,
				 ...);
extern unsigned int
vbi3_print_page_va_list		(vbi3_page *		pg,
				 char *			buffer,
				 unsigned int		buffer_size,
				 const char *		format,
				 va_list		export_options);
extern unsigned int
vbi3_print_page			(vbi3_page *		pg,
				 char *			buffer,
				 unsigned int		buffer_size,
				 const char *		format,
				 ...);
/** @} */

VBI3_END_DECLS

#endif /* __ZVBI3_EXP_TXT_H__ */
