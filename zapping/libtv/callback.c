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

/* $Id: callback.c,v 1.5 2007-08-30 14:14:01 mschimek Exp $ */

#include <assert.h>
#include <stdlib.h>
#include "callback.h"

struct _tv_callback {
	tv_callback *		next;
	tv_callback **		prev_next;
	tv_callback_fn *	notify;
	tv_callback_fn *	destroy;
	void *			user_data;
	unsigned int		blocked;
};

void
tv_nullify_pointer		(void *			object,
				 void *			user_data)
{
	assert (NULL != user_data);

	object = object;
	* ((void **) user_data)	= NULL;
}

/**
 * Remove a callback function from its list. Argument is the result
 * of tv_callback_add().
 *
 * Intended for tveng clients. Note the destroy handler is not called.
 */
void
tv_callback_remove		(tv_callback *		cb)
{
	tv_callback *next;

	if (!cb)
		return;

	if ((next = cb->next))
		next->prev_next = cb->prev_next;

	if (cb->prev_next)
		*cb->prev_next = next;

	free (cb);
}

void
tv_callback_delete		(tv_callback *		cb,
				 void *			object)
{
	assert (object != NULL);

	if (!cb)
		return;

	if (cb->destroy)
		cb->destroy (object, cb->user_data);

	tv_callback_remove (cb);
}

/**
 * Removes all callbacks from the list with the given notify and destroy
 * functions and user_data.
 *
 * Intended for tveng clients. Note the destroy handler is not called.
 */
void
tv_callback_remove_all		(tv_callback *		list,
				 tv_callback_fn *	notify,
				 tv_callback_fn *	destroy,
				 void *			user_data)
{
	while (list) {
		if ((NULL == user_data || list->user_data == user_data)
		    && (NULL == notify || list->notify == notify)
		    && (NULL == destroy || list->destroy == destroy)) {
			tv_callback *next;

			if ((next = list->next))
				next->prev_next = list->prev_next;

			if (list->prev_next)
				*list->prev_next = next;

			free (list);

			list = next;
		} else {
			list = list->next;
		}
	}
}

void
tv_callback_delete_all		(tv_callback *		list,
				 tv_callback_fn *	notify,
				 tv_callback_fn *	destroy,
				 void *			user_data,
				 void *			object)
{
	assert (object != NULL);

	while (list) {
		if ((NULL == user_data || list->user_data == user_data)
		    && (NULL == notify || list->notify == notify)
		    && (NULL == destroy || list->destroy == destroy)) {
			tv_callback *next;

			/* XXX needs ref counter? */
			if (list->destroy)
				list->destroy (object, list->user_data);

			if ((next = list->next))
				next->prev_next = list->prev_next;

			if (list->prev_next)
				*list->prev_next = next;

			free (list);

			list = next;
		} else {
			list = list->next;
		}
	}
}

/**
 * Add a function to a callback list. The notify function is called
 * on some event, the destroy function (if not NULL) before the list
 * is deleted.
 *
 * Not intended to be called directly by tveng clients.
 */
tv_callback *
tv_callback_add			(tv_callback **		list,
				 tv_callback_fn *	notify,
				 tv_callback_fn *	destroy,
				 void *			user_data)
{
	tv_callback *cb;

	assert (NULL != list);
	assert (NULL != notify || NULL != destroy);

	for (; (cb = *list); list = &cb->next)
		;

	if (!(cb = calloc (1, sizeof (*cb))))
		return NULL;

	*list = cb;

	cb->prev_next = list;

	cb->notify = notify;
	cb->destroy = destroy;
	cb->user_data = user_data;

	return cb;
}

/**
 * Temporarily block calls to the notify handler.
 */
void
tv_callback_block		(tv_callback *		cb)
{
	if (cb)
		++cb->blocked;
}

void
tv_callback_block_all		(tv_callback *		list,
				 tv_callback_fn *	notify,
				 tv_callback_fn *	destroy,
				 void *			user_data)
{
	for (; list; list = list->next) {
		if ((NULL == user_data || list->user_data == user_data)
		    && (NULL == notify || list->notify == notify)
		    && (NULL == destroy || list->destroy == destroy)) {
			++list->blocked;
		}
	}
}

/**
 * Counterpart of tv_callback_block().
 */
void
tv_callback_unblock		(tv_callback *		cb)
{
	if (cb && cb->blocked > 0)
		--cb->blocked;
}

void
tv_callback_unblock_all		(tv_callback *		list,
				 tv_callback_fn *	notify,
				 tv_callback_fn *	destroy,
				 void *			user_data)
{
	for (; list; list = list->next) {
		if ((NULL == user_data || list->user_data == user_data)
		    && (NULL == notify || list->notify == notify)
		    && (NULL == destroy || list->destroy == destroy)) {
			if (list->blocked > 0)
				--list->blocked;
		}
	}
}

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
