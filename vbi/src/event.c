/*
 *  libzvbi - Events
 *
 *  Copyright (C) 2000-2004 Michael H. Schimek
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

/* $Id: event.c,v 1.1.2.2 2004-04-17 05:52:24 mschimek Exp $ */

#include <assert.h>
#include <stdlib.h>		/* malloc() */
#include "misc.h"		/* CLEAR() */
#include "event.h"

/**
 * @internal
 * @param es Event handler list.
 * @param ev The event to send.
 * 
 * Traverses the list of event handlers and calls each handler waiting
 * for this @a ev->type of event, passing @a ev as parameter.
 */
void
_vbi_event_handler_list_send	(_vbi_event_handler_list *es,
				 const vbi_event *	ev)
{
	_vbi_event_handler *eh;
	_vbi_event_handler *current;
	
	assert (NULL != es);
	assert (NULL != ev);

	current = es->current;

	eh = es->first;

	while (eh) {
		if (eh->callback && 0 == eh->blocked) {
			es->current = eh;
			eh->blocked = 1;

			if (eh->event_mask & ev->type)
				eh->callback (ev, eh->user_data);

			if (es->current == eh) {
				--eh->blocked;
				eh = eh->next;
			} else {
				/* eh removed itself in callback. */
				eh = es->current;
			}
		} else {
			eh = eh->next;
		}
	}

	es->current = current;
}

/**
 * @internal
 * @param es Event handler list.
 * @param event_mask Event mask.
 *
 * Removes all handlers from the list which handle
 * only events given in the @a event_mask.
 */
void
_vbi_event_handler_list_remove_by_event
			    	(_vbi_event_handler_list *es,
				 unsigned int		event_mask)
{
	_vbi_event_handler *eh, **ehp;
	unsigned int clear_mask;

	assert (NULL != es);

	clear_mask = ~event_mask;

	ehp = &es->first;

	while ((eh = *ehp)) {
		if (0 == (eh->event_mask &= clear_mask)) {
			/* Remove handler. */

			*ehp = eh->next;

			if (es->current == eh)
				es->current = eh->next;

			free (eh);
		} else {
			ehp = &eh->next;
		}
	}

	es->event_mask &= clear_mask;
}

/**
 * @param es Event handler list.
 * @param callback Event handler function.
 * @param user_data Pointer passed to the handler.
 * 
 * Removes an event handler from the list. Safe to call from a handler
 * removing itself or another handler. Does nothing if @a callback
 * is not in the list.
 */
void
_vbi_event_handler_list_remove	(_vbi_event_handler_list *es,
				 vbi_event_cb *		callback,
				 void *			user_data)
{
	_vbi_event_handler_list_add (es, 0, callback, user_data);
}

/**
 * @param es Event handler list.
 * @param event_mask Events the handler is waiting for.
 * @param callback Event handler function.
 * @param user_data Pointer passed to the handler.
 * 
 * Adds a new event handler to the list. @a event_mask can be any set of
 * VBI_EVENT_ symbols, -1 for all events and 0 for none. When the @a callback
 * with @a user_data is already registered, its event_mask will be changed. Any
 * number of handlers can be added, also different handlers for the same
 * event which will be called in registration order.
 * 
 * This function can be safely called at any time, even from a handler.
 * 
 * @return
 * @c FALSE on failure.
 */
extern vbi_bool
_vbi_event_handler_list_add	(_vbi_event_handler_list *es,
				 unsigned int		event_mask,
				 vbi_event_cb *		callback,
				 void *			user_data)
{
	_vbi_event_handler *eh, **ehp;
	unsigned int event_union;
	vbi_bool found;

	assert (NULL != es);

	ehp = &es->first;

	event_union = 0;

	found = FALSE;

	while ((eh = *ehp)) {
		if (eh->callback == callback
		    && eh->user_data == user_data) {
			found = TRUE;

			if (0 == event_mask) {
				/* Remove handler. */

				*ehp = eh->next;

				if (es->current == eh)
					es->current = eh->next;

				free (eh);

				continue;
			} else {
				eh->event_mask = event_mask;
			}
		}

		event_union |= eh->event_mask;
		ehp = &eh->next;
	}

	if (!found && event_mask) {
		/* Add handler. */

		if (!(eh = (_vbi_event_handler *) malloc (sizeof (*eh))))
			return FALSE;

		eh->next	= NULL;
		eh->event_mask	= event_mask;

		eh->callback	= callback;
		eh->user_data	= user_data;

		eh->blocked	= 0;

		event_union |= event_mask;

		*ehp = eh;
	}

	es->event_mask = event_union;

	return TRUE;
}

void
_vbi_event_handler_list_destroy	(_vbi_event_handler_list *es)
{
	assert (NULL != es);

	_vbi_event_handler_list_remove_by_event (es, (unsigned int) -1);

	CLEAR (*es);
}

vbi_bool
_vbi_event_handler_list_init	(_vbi_event_handler_list *es)
{
	assert (NULL != es);

	CLEAR (*es);

	return TRUE;
}
