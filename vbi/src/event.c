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

/* $Id: event.c,v 1.1.2.7 2006-05-18 16:49:19 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "misc.h"		/* CLEAR() */
#include "event-priv.h"

/**
 * @addtogroup Event Events
 * @ingroup Service
 *
 * Typically the transmission of VBI data like a Teletext or Closed
 * Caption page spans several VBI lines or even video frames. So internally
 * the data service decoder maintains caches accumulating data. When a page
 * or other object is complete it calls the respective event handler to
 * notify the application.
 *
 * Clients can register any number of handlers needed, also different
 * handlers for the same event. They will be called in the order registered
 * from the vbi3_decode() function. Since they block decoding, they should
 * return as soon as possible. The event structure and all data
 * pointed to from there must be read only. The data is only valid until
 * the handler returns.
 */

/** @internal */
const char *
_vbi3_event_name			(vbi3_event_mask	event)
{
	switch (event) {

#undef CASE
#define CASE(s) case VBI3_EVENT_##s : return #s ;

	CASE (NONE)
	CASE (CLOSE)
	CASE (RESET)
	CASE (TTX_PAGE)
	CASE (CC_PAGE)
	CASE (NETWORK)
	CASE (TRIGGER)
	CASE (ASPECT)
	CASE (PROG_INFO)
	CASE (PAGE_TYPE)
	CASE (TOP_CHANGE)
	CASE (LOCAL_TIME)
	CASE (PROG_ID)
	CASE (CC_RAW)
	}

	return NULL;
}

/**
 * @internal
 * @param es Event handler list.
 * @param ev The event to send.
 * 
 * Traverses the list of event handlers and calls each handler waiting
 * for this @a ev->type of event, passing @a ev as parameter.
 */
void
__vbi3_event_handler_list_send	(_vbi3_event_handler_list *es,
				 const vbi3_event *	ev)
{
	vbi3_event_handler *eh;
	vbi3_event_handler *current;

	assert (NULL != es);
	assert (NULL != ev);

	if (0 == (es->event_mask & ev->type))
		return;

	current = es->current;

	eh = es->first;

	while (eh) {
		if ((eh->event_mask & ev->type)
		    && eh->callback
		    && 0 == eh->blocked) {
			vbi3_bool done;

			es->current = eh;
			eh->blocked = 1;

			done = eh->callback (ev, eh->user_data);

			if (es->current == eh) {
				eh->blocked = 0;
				eh = eh->next;
			} else {
				/* eh removed itself in callback. */
				eh = es->current;
			}

			if (done)
				break;
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
_vbi3_event_handler_list_remove_by_event
			    	(_vbi3_event_handler_list *es,
				 vbi3_event_mask	event_mask)
{
	vbi3_event_handler *eh, **ehp;
	vbi3_event_mask clear_mask;

	assert (NULL != es);

	clear_mask = ~event_mask;

	ehp = &es->first;

	while ((eh = *ehp)) {
		if (0 == (eh->event_mask &= clear_mask)) {
			/* Remove handler. */

			*ehp = eh->next;

			if (es->current == eh)
				es->current = eh->next;

			vbi3_free (eh);
		} else {
			ehp = &eh->next;
		}
	}

	es->event_mask &= clear_mask;
}

/**
 * @param es Event handler list.
 * @param callback Function to be called on events.
 * @param user_data User pointer passed through to the @a callback function.
 * 
 * Removes all event handlers from the list with this @a callback and
 * @a user_data. You can safely call this function from a handler removing
 * itself or another handler.
 */
void
_vbi3_event_handler_list_remove_by_callback
				(_vbi3_event_handler_list *es,
				 vbi3_event_cb *		callback,
				 void *			user_data)
{
	_vbi3_event_handler_list_add (es, 0, callback, user_data);
}

/**
 * @param es Event handler list.
 * @param eh Event handler.
 * 
 * Removes event handler @a eh if member of the list @a es. You can
 * safely call this function from a handler removing itself or another
 * handler.
 */
void
_vbi3_event_handler_list_remove	(_vbi3_event_handler_list *es,
				 vbi3_event_handler *	eh)
{
	vbi3_event_handler *eh1, **ehp;
	vbi3_event_mask event_union;

	assert (NULL != es);
	assert (NULL != eh);

	ehp = &es->first;

	event_union = 0;

	while ((eh1 = *ehp)) {
		if (eh == eh1) {
			/* Remove handler. */

			*ehp = eh->next;

			if (es->current == eh)
				es->current = eh->next;

			vbi3_free (eh);
		} else {
			event_union |= eh1->event_mask;
			ehp = &eh1->next;
		}
	}

	es->event_mask = event_union;
}

/**
 * @param es Event handler list.
 * @param event_mask Set of events (@c VBI3_EVENT_) the handler is waiting
 *   for, can be -1 for all and 0 for none.
 * @param callback Function to be called on events.
 * @param user_data User pointer passed through to the @a callback function.
 * 
 * Adds a new event handler to the list. When the @a callback with @a
 * user_data is already registered the function merely changes the set
 * of events it will receive in the future. When the @a event_mask is
 * empty the function does nothing or removes an already registered event
 * handler. You can safely call this function from an event handler.
 *
 * Any number of handlers can be added, also different handlers for the
 * same event which will be called in registration order. Handlers are
 * not recursively called when they trigger events.
 *
 * @return
 * Pointer to opaque vbi3_event_handler object, @c NULL on failure or if
 * no handler has been added.
 */
vbi3_event_handler *
_vbi3_event_handler_list_add	(_vbi3_event_handler_list *es,
				 vbi3_event_mask	event_mask,
				 vbi3_event_cb *		callback,
				 void *			user_data)
{
	vbi3_event_handler *eh, **ehp, *found;
	vbi3_event_mask event_union;

	assert (NULL != es);

	ehp = &es->first;

	event_union = 0;

	found = NULL;

	while ((eh = *ehp)) {
		if (eh->callback == callback
		    && eh->user_data == user_data) {
			found = eh;

			if (0 == event_mask) {
				/* Remove handler. */

				*ehp = eh->next;

				if (es->current == eh)
					es->current = eh->next;

				vbi3_free (eh);

				continue;
			} else {
				eh->event_mask = event_mask;
			}
		}

		event_union |= eh->event_mask;
		ehp = &eh->next;
	}

	if (NULL == found && event_mask) {
		/* Add handler. */

		if ((found = vbi3_malloc (sizeof (*found)))) {
			CLEAR (*found);

			found->next		= NULL;
			found->event_mask	= event_mask;

			found->callback		= callback;
			found->user_data	= user_data;

			/* Whoops. Remalloc'ed ourselves? */
			found->blocked = (es->current == found);

			event_union |= event_mask;

			*ehp = found;
		}
	}

	es->event_mask = event_union;

	return found;
}

void
_vbi3_event_handler_list_destroy	(_vbi3_event_handler_list *es)
{
	assert (NULL != es);

	_vbi3_event_handler_list_remove_by_event (es, (vbi3_event_mask) -1);

	CLEAR (*es);
}

vbi3_bool
_vbi3_event_handler_list_init	(_vbi3_event_handler_list *es)
{
	assert (NULL != es);

	CLEAR (*es);

	return TRUE;
}
