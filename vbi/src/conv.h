/*
 *  libzvbi - Unicode conversion helper functions
 *
 *  Copyright (C) 2003, 2004 Michael H. Schimek
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

/* $Id: conv.h,v 1.1.2.5 2006-05-07 06:04:58 mschimek Exp $ */

#ifndef __ZVBI3_CONV_H__
#define __ZVBI3_CONV_H__

#include <stdio.h>
#include <inttypes.h>		/* uint16_t */
#include <iconv.h>		/* iconv_t */
#include "lang.h"		/* vbi3_character_set */
#include "macros.h"

VBI3_BEGIN_DECLS

#define VBI3_NUL_TERMINATED -1L

extern void
vbi3_iconv_close		(iconv_t		cd);
extern iconv_t
vbi3_iconv_open			(const char *		dst_format,
				 char **		dst,
				 unsigned long		dst_size)
  __attribute__ ((_vbi3_nonnull (2)));
extern vbi3_bool
vbi3_iconv_ucs2			(iconv_t		cd,
				 char **		dst,
				 unsigned long		dst_size,
				 const uint16_t *	src,
				 long			src_size)
  __attribute__ ((_vbi3_nonnull (2)));
extern vbi3_bool
vbi3_iconv_unicode		(iconv_t		cd,
				 char **		dst,
				 unsigned long		dst_size,
				 unsigned int		unicode)
  __attribute__ ((_vbi3_nonnull (2)));

extern char *
vbi3_strndup_iconv_ucs2		(const char *		dst_format,
				 const uint16_t *	src,
				 long			src_size)
  __attribute__ ((_vbi3_alloc));

extern char *
vbi3_strndup_locale_utf8	(const char *		src,
				 long			src_size)
  __attribute__ ((_vbi3_alloc));

extern char *
vbi3_strndup_iconv_teletext	(const char *		dst_format,
				 const uint8_t *	src,
				 long			src_size,
				 const vbi3_character_set *cs)
  __attribute__ ((_vbi3_alloc,
		  _vbi3_nonnull (4)));






extern char *
vbi3_strndup_utf8_ucs2		(const uint16_t *	src,
				 long			src_size)
  __attribute__ ((_vbi3_alloc));


extern vbi3_bool
vbi3_fputs_cd_ucs2		(FILE *			fp,
				 iconv_t		cd,
				 const uint16_t *	src,
				 long			src_size)
  __attribute__ ((_vbi3_nonnull (1)));
extern vbi3_bool
vbi3_fputs_iconv_ucs2		(FILE *			fp,
				 const char *		dst_format,
				 const uint16_t *	src,
				 long			src_size)
  __attribute__ ((_vbi3_nonnull (1)));
extern vbi3_bool
vbi3_fputs_locale_utf8		(FILE *			fp,
				 const char *		src,
				 long			src_size)
  __attribute__ ((_vbi3_nonnull (1)));

extern const char *
vbi3_locale_codeset		(void);

/* Private */

extern char *
_vbi3_strdup_locale		(const char *		src)
  __attribute__ ((_vbi3_alloc));
extern uint16_t *
_vbi3_strdup_ucs2_utf8		(const char *		src)
  __attribute__ ((_vbi3_alloc));

VBI3_END_DECLS

#endif /* __ZVBI3_CONV_H__ */
