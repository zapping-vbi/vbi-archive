/*
 *  libzvbi - Wide Screen Signalling
 *
 *  Copyright (C) 2001-2004 Michael H. Schimek
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

/* $Id: wss.h,v 1.1.1.1.2.2 2004-04-03 02:04:48 mschimek Exp $ */

#ifndef __ZVBI_WSS_H__
#define __ZVBI_WSS_H__

#include <inttypes.h>		/* uint8_t */
#include "macros.h"
#include "aspect_ratio.h"	/* vbi_aspect_ratio */

/* Public */

VBI_BEGIN_DECLS

extern vbi_bool
vbi_decode_wss_625		(vbi_aspect_ratio *	r,
				 const uint8_t		buffer[2]);
extern vbi_bool
vbi_decode_wss_cpr1204		(vbi_aspect_ratio *	r,
				 const uint8_t		buffer[3]);

VBI_END_DECLS

/* Private */

#endif /* __ZVBI_WSS_H__ */
