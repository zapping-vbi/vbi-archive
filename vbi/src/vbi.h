/*
 *  libzvbi - VBI decoding library
 *
 *  Copyright (C) 2000, 2001, 2002 Michael H. Schimek
 *  Copyright (C) 2000, 2001 Iñaki García Etxebarria
 *
 *  Based on AleVT 1.5.1
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

/* $Id: vbi.h,v 1.5.2.15 2004-07-09 16:10:54 mschimek Exp $ */

#ifndef VBI_H
#define VBI_H

#include <stdarg.h>
#include "macros.h"

VBI_BEGIN_DECLS

typedef enum {
	VBI_DEBUG = 7,
} vbi_log_level;

extern void
vbi_init			(void) __attribute__ ((constructor));
extern unsigned int
vbi_version			(unsigned int *		major,
				 unsigned int *		minor,
				 unsigned int *		micro);

typedef void vbi_log_fn		(vbi_log_level		level,
				 const char *		function,
				 const char *		message,
				 void *			user_data);
extern void
vbi_set_log_fn			(vbi_log_fn *		function,
				 void *			user_data);

/* Private */
extern void
_vbi_asprintf			(char **		errstr,
				 char *			templ,
				 ...);

VBI_END_DECLS

#endif /* VBI_H */
