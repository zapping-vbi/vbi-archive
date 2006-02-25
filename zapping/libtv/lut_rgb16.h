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

/* $Id: lut_rgb16.h,v 1.1 2006-02-25 17:37:42 mschimek Exp $ */

/* Look-up tables for image format conversion. */

#ifndef IMAGE_FORMAT_LUT_H
#define IMAGE_FORMAT_LUT_H

#include <inttypes.h>

extern const uint16_t		_tv_lut_rgb16[2][6][256];

#endif /* IMAGE_FORMAT_LUT_H */
