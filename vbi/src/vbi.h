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

/* $Id: vbi.h,v 1.5.2.18 2006-05-07 06:04:59 mschimek Exp $ */

#ifndef VBI3_H
#define VBI3_H

#include <stdarg.h>
#include "macros.h"

VBI3_BEGIN_DECLS

extern void
vbi3_init			(void) __attribute__ ((constructor));
extern unsigned int
vbi3_version			(unsigned int *		major,
				 unsigned int *		minor,
				 unsigned int *		micro);

VBI3_END_DECLS

#endif /* VBI3_H */
