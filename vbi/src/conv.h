/*
 *  libzvbi - Unicode conversion helper functions
 *
 *  Copyright (C) 2003-2004 Michael H. Schimek
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

/* $Id: conv.h,v 1.1.2.4 2004-07-09 16:10:52 mschimek Exp $ */

#ifndef __ZVBI_CONV_H__
#define __ZVBI_CONV_H__

#include <stdio.h>
#include <inttypes.h>		/* uint16_t */
#include <iconv.h>		/* iconv_t */
#include "lang.h"		/* vbi_character_set */
#include "macros.h"

VBI_BEGIN_DECLS

extern iconv_t
vbi_iconv_ucs2_open		(const char *		dst_format,
				 char **		dst,
				 unsigned int		dst_size);
extern void
vbi_iconv_ucs2_close		(iconv_t		cd);
extern vbi_bool
vbi_iconv_ucs2			(iconv_t		cd,
				 char **		dst,
				 unsigned int		dst_size,
				 const uint16_t *	src,
				 unsigned int		src_size);
extern vbi_bool
vbi_iconv_unicode		(iconv_t		cd,
				 char **		dst,
				 unsigned int		dst_size,
				 unsigned int		unicode);
extern char *
vbi_strdup_iconv_ucs2		(const char *		dst_format,
				 const uint16_t *	src,
				 unsigned int		src_size);
extern vbi_bool
vbi_stdio_cd_ucs2		(FILE *			fp,
				 iconv_t		cd,
				 const uint16_t *	src,
				 unsigned int		src_size);
extern vbi_bool
vbi_stdio_iconv_ucs2		(FILE *			fp,
				 const char *		dst_format,
				 const uint16_t *	src,
				 unsigned int		src_size);

/* Private */

extern char *
_vbi_strdup_locale_ucs2		(const uint16_t *	src,
				 unsigned int		src_size);
extern char *
_vbi_strdup_locale_utf8		(const char *		src);
extern char *
_vbi_strndup_locale_utf8	(const char *		src,
				 unsigned int		src_size);
extern char *
_vbi_strdup_locale_teletext	(const uint8_t *	src,
				 unsigned int		src_size,
				 const vbi_character_set *cs);
extern uint16_t *
_vbi_strdup_ucs2_utf8		(const char *		src);

VBI_END_DECLS

#endif /* __ZVBI_CONV_H__ */
