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

/* $Id: clip_vector.h,v 1.2 2005-01-31 07:13:06 mschimek Exp $ */

#ifndef __ZTV_CLIP_VECTOR_H__
#define __ZTV_CLIP_VECTOR_H__

#include <inttypes.h>		/* uintX_t */
#include "macros.h"

TV_BEGIN_DECLS

/* Overlay clipping rectangle. These are regions you don't want
   overlaid, with clipping coordinates relative to the top, left
   corner of the overlay window (not the overlay buffer). */

typedef struct {
	uint16_t		x1;
	uint16_t		y1;
	uint16_t		x2;
	uint16_t		y2;
} tv_clip;

tv_inline tv_bool
tv_clip_equal			(const tv_clip *	clip1,
				 const tv_clip *	clip2)
{
	if (8 == sizeof (tv_clip))
		return ((* (const uint64_t *) clip1)
			== (* (const uint64_t *) clip2));
	else
		return (0 == ((clip1->x1 ^ clip2->x1) |
			      (clip1->y1 ^ clip2->y1) |
			      (clip1->x2 ^ clip2->x2) |
			      (clip1->y2 ^ clip2->y2)));
}

typedef struct {
	tv_clip *		vector;
	unsigned int		size;
	unsigned int		capacity;
} tv_clip_vector;

extern uint8_t *
tv_clip_vector_to_clip_mask	(tv_clip_vector *	vector,
				 unsigned int		width,
				 unsigned int		height)
  __attribute__ ((_tv_nonnull (1)));
extern tv_bool
_tv_clip_vector_add_clip_xy	(tv_clip_vector *	vector,
				 unsigned int		x1,
				 unsigned int		y1,
				 unsigned int		x2,
				 unsigned int		y2)
  __attribute__ ((_tv_nonnull (1)));

#define tv_clip_vector_add_clip_xy(v,x1,y1,x2,y2) \
  _tv_clip_vector_add_clip_xy(v,x1,y1,x2,y2)

tv_inline tv_bool
tv_clip_vector_add_clip_wh	(tv_clip_vector *	vector,
				 unsigned int		x,
				 unsigned int		y,
				 unsigned int		width,
				 unsigned int		height)
{
	return _tv_clip_vector_add_clip_xy (vector,
					   x, y, x + width, y + height);
}

extern tv_bool
tv_clip_vector_equal		(const tv_clip_vector *	vector1,
				 const tv_clip_vector *	vector2)
  __attribute__ ((_tv_nonnull (1, 2)));

tv_inline void
tv_clip_vector_clear		(tv_clip_vector *	vector)
{
	vector->size = 0;
}

extern tv_bool
tv_clip_vector_set		(tv_clip_vector *	dst,
				 const tv_clip_vector *	src)
  __attribute__ ((_tv_nonnull (1)));
extern tv_bool
tv_clip_vector_copy		(tv_clip_vector *	dst,
				 const tv_clip_vector *	src)
  __attribute__ ((_tv_nonnull (1)));
extern void
tv_clip_vector_destroy		(tv_clip_vector *	vector)
  __attribute__ ((_tv_nonnull (1)));
extern void
tv_clip_vector_init		(tv_clip_vector *	vector)
  __attribute__ ((_tv_nonnull (1)));

TV_END_DECLS

#endif /* __ZTV_CLIP_VECTOR_H__ */
