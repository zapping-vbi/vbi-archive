/*
 *  libzvbi - Triggers
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

/* $Id: trigger.h,v 1.2.2.1 2004-07-09 16:10:54 mschimek Exp $ */

#ifndef TRIGGER_H
#define TRIGGER_H

#include <inttypes.h>		/* uint8_t */
#include "network.h"		/* vbi_network */
#include "event.h"		/* _vbi_event_handler_list */

typedef struct _vbi_trigger _vbi_trigger;

extern void
_vbi_trigger_destroy		(_vbi_trigger *		t);
extern vbi_bool
_vbi_trigger_init		(_vbi_trigger *		t,
				 const vbi_network *	nk,
				 double			current_time);
extern void
_vbi_trigger_delete		(_vbi_trigger *		t);
extern void
_vbi_trigger_list_delete	(_vbi_trigger **	list);
extern unsigned int
_vbi_trigger_list_fire		(_vbi_trigger **	list,
				 _vbi_event_handler_list *handlers,
				 double		current_time);
extern vbi_bool
_vbi_trigger_list_add_eacem	(_vbi_trigger **	list,
				 _vbi_event_handler_list *handlers,
				 const uint8_t *	s,
				 const vbi_network *	nk,
				 double			current_time);
extern vbi_bool
_vbi_trigger_list_add_atvef	(_vbi_trigger **	list,
				 _vbi_event_handler_list *handlers,
				 const uint8_t *	s,
				 const vbi_network *	nk,
				 double			current_time);

#endif /* TRIGGER_H */
