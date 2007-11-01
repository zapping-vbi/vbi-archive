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

/* $Id: wss.h,v 1.1.1.1.2.6 2007-11-01 00:21:25 mschimek Exp $ */

#ifndef __ZVBI3_WSS_H__
#define __ZVBI3_WSS_H__

#include <inttypes.h>		/* uint8_t */
#include "macros.h"
#include "aspect_ratio.h"	/* vbi3_aspect_ratio */

VBI3_BEGIN_DECLS

/**
 * @addtogroup AspectRatio
 * @{
 */
extern vbi3_bool
vbi3_decode_wss_625		(vbi3_aspect_ratio *	ar,
				 const uint8_t		buffer[2]);
extern vbi3_bool
vbi3_encode_wss_625		(uint8_t		buffer[2],
				 const vbi3_aspect_ratio *ar);
extern vbi3_bool
vbi3_decode_wss_cpr1204		(vbi3_aspect_ratio *	ar,
				 const uint8_t		buffer[3]);
/** @} */

VBI3_END_DECLS

#endif /* __ZVBI3_WSS_H__ */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
