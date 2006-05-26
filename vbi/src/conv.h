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

/* $Id: conv.h,v 1.1.2.7 2006-05-26 00:43:05 mschimek Exp $ */

#ifndef __ZVBI3_CONV_H__
#define __ZVBI3_CONV_H__

#include <stdio.h>
#include <inttypes.h>		/* uint16_t */
#include "macros.h"
#include "lang.h"		/* vbi3_ttx_charset */
#include "version.h"

VBI3_BEGIN_DECLS

/* Public */

#define VBI3_NUL_TERMINATED -1

extern size_t
vbi3_strlen_ucs2		(const uint16_t *	src);
extern char *
vbi3_strndup_iconv		(const char *		dst_codeset,
				 const char *		src_codeset,
				 const char *		src,
				 ssize_t		src_size)
  __attribute__ ((_vbi3_alloc));
extern char *
vbi3_strndup_iconv_ucs2		(const char *		dst_codeset,
				 const uint16_t *	src,
				 ssize_t		src_size)
  __attribute__ ((_vbi3_alloc));
#if 3 == VBI_VERSION_MINOR
extern char *
vbi3_strndup_iconv_teletext	(const char *		dst_codeset,
				 const uint8_t *	src,
				 ssize_t		src_size,
				 const vbi3_ttx_charset *cs)
  __attribute__ ((_vbi3_alloc,
		  _vbi3_nonnull (4)));
#endif
extern vbi3_bool
vbi3_fputs_iconv		(FILE *			fp,
				 const char *		dst_codeset,
				 const char *		src_codeset,
				 const char *		src,
				 ssize_t		src_size)
  __attribute__ ((_vbi3_nonnull (2)));
extern vbi3_bool
vbi3_fputs_iconv_ucs2		(FILE *			fp,
				 const char *		dst_codeset,
				 const uint16_t *	src,
				 ssize_t		src_size)
  __attribute__ ((_vbi3_nonnull (2)));
extern const char *
vbi3_locale_codeset		(void);

/* Private */

typedef struct _vbi3_iconv_t vbi3_iconv_t;

extern vbi3_bool
_vbi3_iconv_ucs2		(vbi3_iconv_t *		cd,
				 char **		dst,
				 size_t			dst_size,
				 const uint16_t *	src,
				 ssize_t		src_size);
extern void
_vbi3_iconv_close		(vbi3_iconv_t *		cd);
extern vbi3_iconv_t *
_vbi3_iconv_open		(const char *		dst_codeset,
				 const char *		src_codeset,
				 char **		dst,
				 size_t			dst_size);

VBI3_END_DECLS

#endif /* __ZVBI3_CONV_H__ */
