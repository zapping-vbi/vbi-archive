/*
 *  libzvbi - Events
 *
 *  Copyright (C) 2000, 2001, 2002, 2003, 2004 Michael H. Schimek
 *
 *  Based on code from AleVT 1.5.1
 *  Copyright (C) 1998,1999 Edgar Toernig (froese@gmx.de)
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

/* $Id: event-priv.h,v 1.1.2.1 2006-05-07 06:04:58 mschimek Exp $ */

#ifndef EVENT_PRIV_H
#define EVENT_PRIV_H

#include "event.h"

#ifndef EVENT_PRIV_LOG
#define EVENT_PRIV_LOG 0
#endif

/** @internal */
typedef struct _vbi3_event_handler vbi3_event_handler;

/** @internal */
struct _vbi3_event_handler {
	vbi3_event_handler *	next;
	vbi3_event_cb *		callback;
	void *			user_data;
	vbi3_event_mask		event_mask;
	unsigned int		blocked;
};

/** @internal */
typedef struct {
	vbi3_event_handler *	first;
	vbi3_event_handler *	current;
	vbi3_event_mask		event_mask;
} _vbi3_event_handler_list;

extern const char *
_vbi3_event_name			(vbi3_event_mask	event);

#if EVENT_PRIV_LOG
#define _vbi3_event_handler_list_send(es, ev)				\
do {									\
	fprintf (stderr, "%s:%u event %s\n",				\
		 __FILE__, __LINE__, _vbi3_event_name ((ev)->type));	\
        __vbi3_event_handler_list_send (es, ev);				\
} while (0)
#else
#define _vbi3_event_handler_list_send(es, ev)				\
	__vbi3_event_handler_list_send (es, ev)
#endif

extern void
__vbi3_event_handler_list_send	(_vbi3_event_handler_list *es,
				 const vbi3_event *	ev);
extern void
_vbi3_event_handler_list_remove_by_event
			    	(_vbi3_event_handler_list *es,
				 vbi3_event_mask	event_mask);
extern void
_vbi3_event_handler_list_remove_by_callback
				(_vbi3_event_handler_list *es,
				 vbi3_event_cb *		callback,
				 void *			user_data);
extern void
_vbi3_event_handler_list_remove	(_vbi3_event_handler_list *es,
				 vbi3_event_handler *	eh);
extern vbi3_event_handler *
_vbi3_event_handler_list_add	(_vbi3_event_handler_list *es,
				 vbi3_event_mask	event_mask,
				 vbi3_event_cb *		callback,
				 void *			user_data);
extern void
_vbi3_event_handler_list_destroy	(_vbi3_event_handler_list *es);
extern vbi3_bool
_vbi3_event_handler_list_init	(_vbi3_event_handler_list *es);

#endif /* EVENT_PRIV_H */
