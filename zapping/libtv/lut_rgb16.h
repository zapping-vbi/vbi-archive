/*
 *  Copyright (C) 2006 Michael H. Schimek
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

/* $Id: lut_rgb16.h,v 1.3 2007-08-30 14:14:09 mschimek Exp $ */

/* Look-up tables for image format conversion. */

#ifndef LUT_RGB16_H
#define LUT_RGB16_H

#include <inttypes.h>

extern const uint16_t		_tv_lut_rgb16 [2][6][256];

#endif /* LUT_RGB16_H */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
