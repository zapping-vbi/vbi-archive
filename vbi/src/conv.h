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

/* $Id: conv.h,v 1.1.2.1 2004-02-25 17:35:27 mschimek Exp $ */

#ifndef CONV_H
#define CONV_H

#include <stdio.h>
#include <inttypes.h>
#include <iconv.h>

#include "misc.h"

/* Public */

/**
 * This is the domain name used by libzvbi for gettext translations.
 * Can be used for example with bind_textdomain_codeset() to specify
 * a character set different from the current locale.
 */
extern const char vbi_intl_domainname [];

/* Private */

#ifndef _
#  ifdef ENABLE_NLS
#    include <libintl.h>
#    include <locale.h>
#    define _(String) dgettext (vbi_intl_domainname, String)
#    ifdef gettext_noop
#      define N_(String) gettext_noop (String)
#    else
#      define N_(String) (String)
#    endif
#  else /* Stubs that do something close enough.  */
#    define gettext(Msgid) ((const char *) (Msgid))
#    define dgettext(Domainname, Msgid) ((const char *) (Msgid))
#    define dcgettext(Domainname, Msgid, Category) ((const char *) (Msgid))
#    define ngettext(Msgid1, Msgid2, N) \
       ((N) == 1 ? (const char *) (Msgid1) : (const char *) (Msgid2))
#    define dngettext(Domainname, Msgid1, Msgid2, N) \
       ((N) == 1 ? (const char *) (Msgid1) : (const char *) (Msgid2))
#    define dcngettext(Domainname, Msgid1, Msgid2, N, Category) \
       ((N) == 1 ? (const char *) (Msgid1) : (const char *) (Msgid2))
#    define textdomain(Domainname) ((const char *) (Domainname))
#    define bindtextdomain(Domainname, Dirname) ((const char *) (Dirname))
#    define bind_textdomain_codeset(Domainname, Codeset) \
       ((const char *) (Codeset))
#    define _(String) (String)
#    define N_(String) (String)
#  endif
#endif

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
extern char *
vbi_strdup_locale_ucs2		(const uint16_t *	src,
				 unsigned int		src_size);
extern char *
vbi_strdup_locale_utf8		(const char *		src);
extern uint16_t *
vbi_strdup_ucs2_utf8		(const char *		src);
extern vbi_bool
vbi_stdio_iconv_ucs2		(FILE *			fp,
				 const char *		dst_format,
				 const uint16_t *	src,
				 unsigned int		src_size);

#endif /* CONV_H */
