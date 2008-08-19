/*
 *  libzvbi
 *
 *  Copyright (C) 2004 Michael H. Schimek
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

/* $Id: intl.h,v 1.1.2.1 2008-08-19 10:56:05 mschimek Exp $ */

#ifndef __ZVBI_INTL_H__
#define __ZVBI_INTL_H__

#include "zvbi/macros.h"

VBI_BEGIN_DECLS

/**
 * @addtogroup Basic
 * @brief Libzvbi gettext domain.
 *
 * This is the domain name used by libzvbi for gettext translations.
 * Can be used for example with bind_textdomain_codeset() to specify
 * a character encoding different from the current locale.
 */
extern const char vbi_intl_domainname [];
 
VBI_END_DECLS

#endif /* __ZVBI_INTL_H__ */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
