/*
 *  libzvbi
 *
 *  Copyright (C) 2004 Michael H. Schimek
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

/* $Id: intl.h,v 1.1.2.2 2004-04-08 23:36:25 mschimek Exp $ */

#ifndef __ZVBI_INTL_H__
#define __ZVBI_INTL_H__

#include "macros.h"

VBI_BEGIN_DECLS

/**
 * @addtogroup Basic
 * @brief Libzvbi gettext domain.
 *
 * This is the domain name used by libzvbi for gettext translations.
 * Can be used for example with bind_textdomain_codeset() to specify
 * a character set different from the current locale.
 */
extern const char vbi_intl_domainname [];

VBI_END_DECLS

#endif /* __ZVBI_INTL_H__ */
