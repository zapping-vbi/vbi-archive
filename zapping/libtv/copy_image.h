/*
 *  Copyright (C) 2001-2004 Michael H. Schimek
 *  Copyright (C) 2000-2003 Iñaki García Etxebarria
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

/* $Id: copy_image.h,v 1.1 2006-04-12 01:48:15 mschimek Exp $ */

#ifndef __ZTV_COPY_IMAGE_H__
#define __ZTV_COPY_IMAGE_H__

#include "image_format.h"

TV_BEGIN_DECLS

extern void
tv_memcpy			(void *			dst,
				 const void *		src,
				 size_t			n_bytes);
extern tv_bool
tv_copy_image			(void *			dst_image,
				 const tv_image_format *dst_format,
				 const void *		src_image,
				 const tv_image_format *src_format);

TV_END_DECLS

#endif /* __ZTV_COPY_IMAGE_H__ */
