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

/* $Id: control.c,v 1.1 2005-06-28 01:06:37 mschimek Exp $ */

#include <stdlib.h>		/* malloc() */
#include "misc.h"
#include "control.h"

static tv_bool
tv_control_copy			(tv_control *		dst,
				 const tv_control *	src)
{
	assert (NULL != dst);

	if (dst == src) {
		return TRUE;
	}

	CLEAR (*dst);

	if (!src) {
		return TRUE;
	}

	if (!(dst->label = strdup (src->label))) {
		return FALSE;
	}

	dst->hash	= src->hash;
	dst->type	= src->type;
	dst->id		= src->id;

	if (TV_CONTROL_TYPE_CHOICE == src->type) {
		unsigned int i;

		dst->menu = malloc ((src->maximum + 2) * sizeof (*dst->menu));
		if (!dst->menu) {
			tv_control_destroy (dst);
			return FALSE;
		}
	       
		for (i = 0; i <= (unsigned int) src->maximum; ++i) {
			assert (NULL != src->menu[i]);

			dst->menu[i] = strdup (src->menu[i]);
			if (!dst->menu[i]) {
				tv_control_destroy (dst);
				return FALSE;
			}
		}

		dst->menu[i] = NULL;
	}

	dst->minimum	= src->minimum;
	dst->maximum	= src->maximum;
	dst->step	= src->step;
	dst->reset	= src->reset;

	dst->value	= src->value;

	return TRUE;
}

tv_control *
tv_control_dup			(const tv_control *	c,
				 unsigned int		size)
{
	tv_control *nc;

	if (!(nc = malloc (MAX (sizeof (*nc), size)))) {
		return NULL;
	}

	if (!tv_control_copy (nc, c)) {
		free (nc);
		nc = NULL;
	}

	return nc;
}

void
tv_control_destroy		(tv_control *		c)
{
	assert (NULL != c);

	tv_callback_delete_all (c->_callback,
				/* notify: any */ NULL,
				/* destroy: any */ NULL,
				/* user_data: any */ NULL,
				/* object */ c);

	if (c->menu) {
		unsigned int i;

		for (i = 0; c->menu[i]; ++i) {
			free (c->menu[i]);
		}

		free (c->menu);
	}

	if (c->label)
		free (c->label);

	CLEAR (*c);
}

void
tv_control_delete		(tv_control *		c)
{
	if (c) {
		tv_control_destroy (c);
		free (c);
	}
}
