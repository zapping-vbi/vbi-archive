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

/* $Id: clip_vector.c,v 1.2 2005-06-28 01:03:11 mschimek Exp $ */

#include <stdlib.h>		/* malloc() */
#include "misc.h"
#include "clip_vector.h"

tv_bool
tv_clip_vector_equal		(const tv_clip_vector *	vector1,
				 const tv_clip_vector *	vector2)
{
	unsigned int i;

	assert (NULL != vector1);
	assert (NULL != vector2);

	if (vector1 == vector2)
		return TRUE;

	if (vector1->size != vector2->size)
		return FALSE;

	for (i = 0; i < vector1->size; ++i)
		if (!tv_clip_equal (vector1->vector + i,
				    vector2->vector + i))
			return FALSE;

	return TRUE;
}

tv_bool
tv_clip_vector_set		(tv_clip_vector *	dst,
				 const tv_clip_vector *	src)
{
	assert (NULL != dst);

	if (dst == src)
		return TRUE;

	if (src) {
		if (src->size > dst->capacity) {
                        tv_clip *clip_vector;
			unsigned long size;

                        assert (src->capacity >= src->size);

			size = src->capacity * sizeof (*clip_vector);
                        if (!(clip_vector = realloc (dst->vector, size)))
                                return FALSE;

                        dst->vector = clip_vector;
                        dst->capacity = src->capacity;
                }

                memcpy (dst->vector, src->vector,
                        sizeof (*src->vector) * src->size);

                dst->size = src->size;             
	} else {
		dst->size = 0;
	}

	return TRUE;
}

tv_bool
tv_clip_vector_copy		(tv_clip_vector *	dst,
				 const tv_clip_vector *	src)
{
	assert (NULL != dst);

	if (dst == src)
		return TRUE;

	if (src) {
		tv_clip *clip_vector;
		unsigned long size;

		size = src->capacity * sizeof (*clip_vector);
		if (!(clip_vector = malloc (size)))
			return FALSE;

		dst->vector = clip_vector;
		dst->capacity = src->capacity;
		dst->size = src->size;

		memcpy (dst->vector, src->vector,
			sizeof (*src->vector) * src->size);
	} else {
		CLEAR (*dst);
	}

	return TRUE;
}

/* Note: width = x2 - x1, height = y2 - y1. */
tv_bool
_tv_clip_vector_add_clip_xy	(tv_clip_vector *	vector,
				 unsigned int		x1,
				 unsigned int		y1,
				 unsigned int		x2,
				 unsigned int		y2)
{
	tv_clip *clip;
	tv_clip *end;

	assert (NULL != vector);

	if (x1 >= x2 || y1 >= y2)
		return TRUE;

 restart:
	clip = vector->vector;
	end = vector->vector + vector->size;

	/* Clips are sorted in ascending order, first by x1, second y1.
	   We split into horizontal bands, merging clips where possible.
	   Vertically adjacent clips or bands are not merged.

	   a #
	   b ##
	   c #####
           d ######
	   e  ##
           f  clip
           g  #####
	   h   ##
	   i    ##
	   j     ##
	   k      #
	*/

	for (;;) {
		if (clip >= end /* k */ || y2 <= clip->y1 /* a */)
			goto insert;

		if (y1 < clip->y1 /* bcd */) {
			/* Insert bottom half (efg). */
			if (!tv_clip_vector_add_clip_xy
			    (vector, x1, clip->y1, x2, y2))
				return FALSE;

			/* Top half (a). */
			y2 = clip->y1;
			goto restart;
		}

		if (y1 < clip->y2 /* efghij */) {
			if (y2 > clip->y2 /* gj */) {
				/* Insert bottom half (k). */
				if (!tv_clip_vector_add_clip_xy
				    (vector, x1, clip->y2, x2, y2))
					return FALSE;

				/* Top half (fi). */
				y2 = clip->y2;
				goto restart;
			}

			break;
		}

		++clip;
	}

	/* efhi */

	if (y1 != clip->y1 || y2 != clip->y2) {
		unsigned int ys;
		unsigned int n;
		unsigned int i;

		/* Split band at ys. */

		if (y1 > clip->y1 /* hi */) {
			ys = y1;
		} else { /* e */
			ys = y2; 
		}

		/* n = band size. */
		for (n = 1; clip + n < end; ++n)
			if (clip[0].y1 != clip[n].y1)
				break;

		if (vector->size + n >= vector->capacity) {
			tv_clip *new_vector;
			unsigned int new_capacity;
			unsigned long size;

			/* 0 < n <= size <= capacity */
			new_capacity = vector->capacity * 2;

			size = new_capacity * sizeof (*new_vector);
			if (!(new_vector = realloc (vector->vector, size)))
				return FALSE;

			clip = new_vector + (clip - vector->vector);
			end = new_vector + vector->size;

			vector->vector = new_vector;
			vector->capacity = new_capacity;
		}

		memmove (clip + n, clip, sizeof (tv_clip) * (end - clip));

		vector->size += n;
		end += n;

		for (i = 0; i < n; ++i) {
			clip[i + 0].y2 = ys;
			clip[i + n].y1 = ys;
		}

		goto restart; /* ef */
	}

	/* f */

	do {
		if (x2 < clip->x1)
			break; /* insert before */

		if (x1 < clip->x2) {
			unsigned int n;

			/* Merge clips. */

			x1 = MIN (x1, (unsigned int) clip->x1);

			for (n = 1; clip + n < end; ++n)
				if (x2 < clip[n].x1 || y1 != clip[n].y1)
					break;
			--n;

			x2 = MAX (x2, (unsigned int) clip[n].x2);

			if (n > 0) {
				memmove (clip, clip + n,
					 sizeof (tv_clip)
					 * (end - (clip + n)));
				vector->size -= n;
			}

			goto store;
		}

		++clip;
	} while (clip < end && y1 == clip->y1);

 insert:
	if (vector->size == vector->capacity) {
		tv_clip *new_vector;
		unsigned int new_capacity;
		unsigned long size;

		new_capacity = (vector->capacity < 16) ? 16
			: vector->capacity * 2;

		size = new_capacity * sizeof (*new_vector);
		if (!(new_vector = realloc (vector->vector, size)))
			return FALSE;

		clip = new_vector + (clip - vector->vector);
		end = new_vector + vector->size;

		vector->vector = new_vector;
		vector->capacity = new_capacity;
	}

	if (clip < end) {
		memmove (clip + 1, clip, sizeof (tv_clip) * (end - clip));
	}

	++vector->size;

 store:
	assert (clip >= vector->vector
		&& clip < vector->vector + vector->size);

	clip->x1 = x1;
	clip->y1 = y1;
	clip->x2 = x2;
	clip->y2 = y2;

	if (0) {
		unsigned int i;

		clip = vector->vector;

		for (i = 0; i < vector->size; ++i, ++clip)
			fprintf (stderr, "%3u: %3u,%3u - %3u,%3u\n",
				 i, clip->x1, clip->y1, clip->x2, clip->y2);
	}

	return TRUE;
}

/* XXX untested. Should create another mask_to_vector to test if
   the clips are equivalent. */
uint8_t *
tv_clip_vector_to_clip_mask	(tv_clip_vector *	vector,
				 unsigned int		width,
				 unsigned int		height)
{
	uint8_t *mask;
	unsigned long bpl;
	tv_clip *clip;
	unsigned int count;

	assert (NULL != vector);

	bpl = (width + 7) & (unsigned long) -8; 

	if (!(mask = calloc (1, height * bpl)))
		return NULL;

	clip = vector->vector;

	for (count = vector->size; count > 0; --count) {
		unsigned int x7;
		unsigned int xw;
		uint8_t *start;
		uint8_t lmask;
		uint8_t rmask;
		unsigned int length;
		unsigned int n;

		if (clip->x2 > width)
			goto failure;

		if (clip->y2 > height)
			goto failure;

		start = mask + clip->y1 * bpl + (clip->x1 >> 3);

		x7 = clip->x1 & 7;
		xw = x7 + clip->x2 - clip->x1;

		lmask = ~0 << x7;
		rmask = ~(~0 << (xw & 7));

		length = (xw - 1) >> 3;

		if (0 == length) {
			lmask &= rmask;

			for (n = clip->y2 - clip->y1; n > 0; --n) {
				*start |= lmask;
				start += bpl;
			}
		} else {
			for (n = clip->y2 - clip->y1; n > 0; --n) {
				unsigned int i;

				start[0] |= lmask;

				for (i = 1; i < length; ++i)
					start[i] = 0xFF;

				start[length] |= rmask;

				start += bpl;
			}
		}

		++clip;
	}

	return mask;

 failure:
	free (mask);
	return NULL;
}

void
tv_clip_vector_destroy		(tv_clip_vector *	vector)
{
	assert (NULL != vector);

	free (vector->vector);
	CLEAR (*vector);
}

void
tv_clip_vector_init		(tv_clip_vector *	vector)
{
	assert (NULL != vector);

	CLEAR (*vector);
}

