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

/* $Id: trigger.h,v 1.2.2.2 2006-05-07 06:04:59 mschimek Exp $ */

#ifndef TRIGGER_H
#define TRIGGER_H

#include <inttypes.h>		/* uint8_t */
#include "network.h"		/* vbi3_network */
#include "event-priv.h"		/* _vbi3_event_handler_list */

typedef struct _vbi3_trigger _vbi3_trigger;

extern void
_vbi3_trigger_destroy		(_vbi3_trigger *		t);
extern vbi3_bool
_vbi3_trigger_init		(_vbi3_trigger *		t,
				 const vbi3_network *	nk,
				 double			current_time);
extern void
_vbi3_trigger_delete		(_vbi3_trigger *		t);
extern void
_vbi3_trigger_list_delete	(_vbi3_trigger **	list);
extern unsigned int
_vbi3_trigger_list_fire		(_vbi3_trigger **	list,
				 _vbi3_event_handler_list *handlers,
				 double		current_time);
extern vbi3_bool
_vbi3_trigger_list_add_eacem	(_vbi3_trigger **	list,
				 _vbi3_event_handler_list *handlers,
				 const uint8_t *	s,
				 const vbi3_network *	nk,
				 double			current_time);
extern vbi3_bool
_vbi3_trigger_list_add_atvef	(_vbi3_trigger **	list,
				 _vbi3_event_handler_list *handlers,
				 const uint8_t *	s,
				 const vbi3_network *	nk,
				 double			current_time);

#endif /* TRIGGER_H */
