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

/* $Id: control.h,v 1.4 2007-08-30 14:14:01 mschimek Exp $ */

#ifndef __ZTV_CONTROL_H__
#define __ZTV_CONTROL_H__

#include <assert.h>
#include "macros.h"
#include "callback.h"

TV_BEGIN_DECLS

/* Programmatically accessable controls. Other controls
   are anonymous, only the user knows what they do. Keep
   the list short. */
typedef enum {
	TV_CONTROL_ID_NONE,
	TV_CONTROL_ID_UNKNOWN = TV_CONTROL_ID_NONE,
	TV_CONTROL_ID_BRIGHTNESS,
	TV_CONTROL_ID_CONTRAST,
	TV_CONTROL_ID_SATURATION,
	TV_CONTROL_ID_HUE,
	TV_CONTROL_ID_MUTE,
	TV_CONTROL_ID_VOLUME,
	TV_CONTROL_ID_BASS,
	TV_CONTROL_ID_TREBLE,
	/* preliminary */
	TV_CONTROL_ID_AUDIO_MODE,
} tv_control_id;

typedef enum {
	TV_CONTROL_TYPE_NONE,
	TV_CONTROL_TYPE_INTEGER,	/* integer [min, max] */
	TV_CONTROL_TYPE_BOOLEAN,	/* integer [0, 1] */
	TV_CONTROL_TYPE_CHOICE,		/* multiple choice */
	TV_CONTROL_TYPE_ACTION,		/* setting has one-time effect */
	TV_CONTROL_TYPE_COLOR		/* RGB color entry */
} tv_control_type;

typedef struct _tv_control tv_control;

struct _tv_control {
	tv_control *		_next;	/* private, use tv_control_next() */

	void *			_parent;
	tv_callback *		_callback;

	char *			label;		/* localized */
	unsigned int		hash;

	tv_control_type		type;
	tv_control_id		id;

	char **			menu;	/* localized; last entry NULL */
#if 0
   add?	unsigned int		selectable;	/* menu item 1 << n */
   control enabled/disabled flag?
#endif
	int			minimum;
	int			maximum;
	int			step;
	int			reset;

	int			value;	/* last known, not current value */
};

tv_bool
tv_control_copy			(tv_control *		dst,
				 const tv_control *	src);
tv_control *
tv_control_dup			(const tv_control *	control);
void
tv_control_destroy		(tv_control *		control);
void
tv_control_delete		(tv_control *		control);

typedef void
tv_control_callback_fn		(tv_control *		control,
				 void *			user_data);

static __inline__ tv_callback *
tv_control_add_callback		(tv_control *		control,
				 tv_control_callback_fn	*notify,
				 tv_control_callback_fn	*destroy,
				 void *			user_data)
{
	assert (control != NULL);

	return tv_callback_add (&control->_callback,
				(tv_callback_fn *) notify,
				(tv_callback_fn *) destroy,
				user_data);
}

TV_END_DECLS

#endif /* __ZTV_CONTROL_H__ */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
