#if 0

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
_vbi_event_handlers_send	(_vbi_event_handlers *	es,
				 const vbi_event *	ev)
{
	_vbi_event_handler *eh;
	_vbi_event_handler *current;
	
	assert (NULL != es);
	assert (NULL != ev);

	eh = es->first;
	current = es->current;

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
				/* eh unexists. */
				eh = es->current;
			}
		} else {
			eh = eh->next;
		}
	}

	es->current = current;
}

void
_vbi_event_handlers_remove_by_event
			    	(_vbi_event_handlers *	es,
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
 * Removes an event handler from the list.
 *
 * Apart of removing a handler this function also disables decoding
 * of data services when no handler is registered to consume the
 * respective data. Removing the last @c VBI_EVENT_TTX_PAGE handler for
 * example disables Teletext decoding.
 * 
 * This function can be safely called at any time, even from a handler
 * removing itself or another handler, and regardless if the @a handler
 * has been successfully registered.
 */
void
_vbi_event_handlers_remove	(_vbi_event_handlers *	es,
				 vbi_event_cb *		callback,
				 void *			user_data)
{
	_vbi_event_handlers_add (es, 0, callback, user_data);
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
 * Apart of adding handlers this function also enables and disables decoding
 * of data services depending on the presence of at least one handler for the
 * respective data. A @c VBI_EVENT_TTX_PAGE handler for example enables
 * Teletext decoding.
 * 
 * This function can be safely called at any time, even from a handler.
 * 
 * @return
 * @c FALSE on failure.
 */
extern vbi_bool
_vbi_event_handlers_add		(_vbi_event_handlers *	es,
				 unsigned int		event_mask,
				 vbi_event_cb *		callback,
				 void *			user_data)
{
	_vbi_event_handler *eh, **ehp;
	unsigned int union_mask;
	vbi_bool found;

	assert (NULL != es);

	ehp = &es->first;

	union_mask = 0;

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

		union_mask |= eh->event_mask;
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

		union_mask |= event_mask;

		*ehp = eh;
	}

	es->event_mask = union_mask;

	return TRUE;
}

void
_vbi_event_handlers_destroy	(_vbi_event_handlers *	es)
{
	assert (NULL != es);

	_vbi_event_handlers_remove_by_event (es, (unsigned int) -1);

	CLEAR (*es);
}

vbi_bool
_vbi_event_handlers_init	(_vbi_event_handlers *	es)
{
	assert (NULL != es);

	CLEAR (*es);

	return TRUE;
}

#endif
