/*
 *  libzvbi - VBI decoding library
 *
 *  Copyright (C) 2000, 2001, 2002 Michael H. Schimek
 *  Copyright (C) 2000, 2001 Iñaki García Etxebarria
 *
 *  Based on AleVT 1.5.1
 *  Copyright (C) 1998, 1999 Edgar Toernig <froese@gmx.de>
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

/* $Id: vbi_decoder.c,v 1.1.2.2 2004-07-09 16:10:54 mschimek Exp $ */

#include <assert.h>
#include <stdlib.h>		/* malloc() */
#include "vbi_decoder-priv.h"
#include "misc.h"
#include "vps.h"
#include "wss.h"

/**
 * @param vbi VBI decoder allocated with vbi_decoder_new().
 * @param nk Identifies the network to query, can be @c NULL for the
 *   current network.
 * @param array_size Number of elements in the array will be stored here.
 *
 * Returns all Teletext TOP (Table Of Pages) titles received,
 * sorted by ascending page number.
 *
 * @returns
 * Array of vbi_top_title structures which must be freed with
 * vbi_top_title_array_delete() when done. @c NULL on error.
 */
vbi_top_title *
vbi_decoder_get_top_titles	(vbi_decoder *		vbi,
				 const vbi_network *	nk,
				 unsigned int *		array_size)
{
	return vbi_teletext_decoder_get_top_titles
		(&vbi->vt, nk, array_size);
}

/**
 * @param vbi VBI decoder allocated with vbi_decoder_new().
 * @param tt TOP title structure will be stored here.
 * @param nk Identifies the network to query, can be @c NULL for the
 *   current network.
 * @param pgno Teletext page number.
 * @param subno Teletext subpage number, can be @c VBI_ANY_SUBNO.
 *
 * Returns the Teletext TOP (Table Of Pages) title of a page.
 * 
 * @returns
 * vbi_top_title structure in @a *tt which must be destroyed with
 * vbi_top_title_destroy() when done. @c FALSE on error, in this
 * case @a *tt will be cleared so vbi_top_title_destroy() is safe.
 */
vbi_bool
vbi_decoder_get_top_title	(vbi_decoder *		vbi,
				 vbi_top_title *	tt,
				 const vbi_network *	nk,
				 vbi_pgno		pgno,
				 vbi_subno		subno)
{
	return vbi_teletext_decoder_get_top_title
		(&vbi->vt, tt, nk, pgno, subno);
}

/**
 * @param vbi VBI decoder allocated with vbi_decoder_new().
 * @param ps Page information will be stored here.
 * @param nk Identifies the network to query, can be @c NULL for the
 *   current network.
 * @param pgno Teletext page number.
 *
 * Returns information about the Teletext page.
 *
 * @returns
 * vbi_ttx_page_stat structure in @a *ps. @c FALSE on error.
 */
vbi_bool
vbi_decoder_get_ttx_page_stat	(vbi_decoder *		vbi,
				 vbi_ttx_page_stat *	ps,
				 const vbi_network *	nk,
				 vbi_pgno		pgno)
{
	return vbi_teletext_decoder_get_ttx_page_stat (&vbi->vt, ps, nk, pgno);
}

/**
 * @param vbi VBI decoder allocated with vbi_decoder_new().
 * @param nk Identifies the network transmitting the page.
 * @param pgno Teletext page number.
 * @param subno Teletext subpage number, can be @c VBI_ANY_SUBNO
 *   (most recently received subpage, if any).
 * @param format_options Array of pairs of a vbi_format_option and value,
 *   terminated by a @c 0 option. See vbi_decoder_get_teletext_page().
 *
 * Allocates a new vbi_page, formatted from a cached Teletext page.
 *
 * @returns
 * vbi_page which must be freed with vbi_page_delete() when done.
 * @c NULL on error.
 */
vbi_page *
vbi_decoder_get_teletext_page_va_list
				(vbi_decoder *		vbi,
				 const vbi_network *	nk,
				 vbi_pgno		pgno,
				 vbi_subno		subno,
				 va_list		format_options)
{
	return vbi_teletext_decoder_get_page_va_list
		(&vbi->vt, nk, pgno, subno, format_options);
}

/**
 * @param vbi VBI decoder allocated with vbi_decoder_new().
 * @param nk Identifies the network transmitting the page.
 * @param pgno Teletext page number.
 * @param subno Teletext subpage number, can be @c VBI_ANY_SUBNO
 *   (most recently received subpage, if any).
 * @param ... Array of pairs of a vbi_format_option and value,
 *   terminated by a @c 0 option.
 *
 * Allocates a new vbi_page, formatted from a cached Teletext page.
 *
 * Example:
 *
 * @code
 * vbi_page *pg;
 *
 * pg = vbi_decoder_get_teletext_page (vbi, NULL, 0x100, VBI_ANY_SUBNO,
 *				       VBI_NAVIGATION, TRUE,
 *				       VBI_WST_LEVEL, VBI_LEVEL_2p5,
 *				       0);
 * @endcode
 *
 * @returns
 * vbi_page which must be freed with vbi_page_delete() when done.
 * @c NULL on error.
 */
vbi_page *
vbi_decoder_get_teletext_page	(vbi_decoder *		vbi,
				 const vbi_network *	nk,
				 vbi_pgno		pgno,
				 vbi_subno		subno,
				 ...)
{
	vbi_page *pg;
	va_list format_options;

	va_start (format_options, subno);

	pg = vbi_teletext_decoder_get_page_va_list
		(&vbi->vt, nk, pgno, subno, format_options);

	va_end (format_options);

	return pg;
}

/**
 * @param vbi VBI decoder allocated with vbi_decoder_new().
 * @param pid Program ID will be stored here.
 * @param channel Logical channel transmitting the ID.
 *
 * Returns the most recently on a logical channel received program ID.
 *
 * Sources can be VBI_SLICED_TELETEXT_B and VBI_SLICED_VPS (EN 300 231 PDC),
 * and VBI_SLICED_CAPTION_525 (EIA 608 XDS) data.
 */
void
vbi_decoder_get_program_id	(vbi_decoder *		vbi,
				 vbi_program_id *	pid,
				 vbi_pid_channel	channel)
{
	cache_network *cn;

	assert (NULL != vbi);
	assert (NULL != pid);

	cn = vbi->vt.network;

	assert ((unsigned int) channel < N_ELEMENTS (cn->program_id));

	*pid = cn->program_id[channel];
}

/**
 * @param vbi VBI decoder allocated with vbi_decoder_new().
 * @param ar Aspect ratio information will be stored here.
 *
 * Returns the most recently received aspect ratio information.
 *
 * Sources can be VBI_SLICED_WSS_625 (EN 300 294), VBI_SLICED_WSS_CPR1204
 * (EIA-J CPR-1204) and VBI_SLICED_CAPTION_525 (EIA 608 XDS) data.
 */
void
vbi_decoder_get_aspect_ratio	(vbi_decoder *		vbi,
				 vbi_aspect_ratio *	ar)
{
	assert (NULL != vbi);
	assert (NULL != ar);

	*ar = vbi->vt.network->aspect_ratio;
}

/**
 * @internal
 * @param vbi VBI decoder allocated with vbi_decoder_new().
 * @param cn New network, can be @c NULL if 0.0 == time.
 * @param time Deferred reset when time is greater than
 *   vbi_decoder_decode() time. Pass a negative number to
 *   cancel a deferred reset, 0.0 to reset immediately.
 *
 * Internal reset function.
 */
static void
reset				(vbi_decoder *		vbi,
				 cache_network *	cn,
				 double			time)
{
	vbi_event e;

	if (time <= 0.0 || time > vbi->reset_time)
		vbi->reset_time = time;

	vbi->teletext_reset (&vbi->vt, cn, time);
	vbi->caption_reset (&vbi->cc, cn, time);

	if (0 == time /* reset now */) {
		e.type		= VBI_EVENT_RESET;
		e.network	= &vbi->vt.network->network;
		e.timestamp	= vbi->time;

		_vbi_event_handler_list_send (&vbi->handlers, &e);
	}
}

static void
cni_change			(vbi_decoder *		vbi,
				 vbi_cni_type		type,
				 unsigned int		cni)
{
	cache_network *cn;
	double timeout;

	cn = vbi->vt.network;
	timeout = 0.0;

	if (VBI_CNI_TYPE_VPS != type
	    && 0 != cn->network.cni_vps
	    && (cn->network.cni_vps
		!= vbi_convert_cni (VBI_CNI_TYPE_VPS, type, cni))) {
		/* We cannot say with certainty if cni and cni_vps belong
		   to the same network. If 0 == vbi_convert_cni() we cannot
		   convert, otherwise CNIs mismatch because the channel
		   changed, the network transmits wrong CNIs or the
		   conversion is incorrect.

		   After n seconds, if we cannot confirm the cni_vps or
		   receive a new one, we assume there was a channel change
		   and the new network does not transmit a cni_vps. */
		cn->confirm_cni_vps = cn->network.cni_vps;
		timeout = vbi->vt.cni_vps_timeout;
	}

	if (VBI_CNI_TYPE_8301 != type
	    && 0 != cn->network.cni_8301
	    && (cn->network.cni_8301
		!= vbi_convert_cni (VBI_CNI_TYPE_8301, type, cni))) {
		cn->confirm_cni_8301 = cn->network.cni_8301;
		timeout = vbi->vt.cni_830_timeout;
	}

	if (VBI_CNI_TYPE_8302 != type
	    && 0 != cn->network.cni_8302
	    && (cn->network.cni_8302
		!= vbi_convert_cni (VBI_CNI_TYPE_8302, type, cni))) {
		cn->confirm_cni_8302 = cn->network.cni_8302;
		timeout = vbi->vt.cni_830_timeout;
	}

	if (timeout > 0.0)
		reset (vbi, NULL, vbi->time + timeout);
}

static vbi_bool
decode_vps			(vbi_decoder *		vbi,
				 const uint8_t		buffer[13])
{
	unsigned int cni;
	cache_network *cn;

	if (!vbi_decode_vps_cni (&cni, buffer))
		return FALSE;

	cn = vbi->vt.network;

	if (0 == cni) {
		/* Should probably ignore this. */
	} else if (cni == cn->network.cni_vps) {
		/* No CNI change, no channel change. */

		cn->confirm_cni_vps = 0;

		if (0 == cn->confirm_cni_8301
		    && 0 == cn->confirm_cni_8302) {
			/* All CNIs valid, cancel reset requests. */
			reset (vbi, NULL, -1);
		}
	} else if (cni == cn->confirm_cni_vps) {
		vbi_event e;

		/* The CNI is correct. */

		if (0 == cn->network.cni_vps) {
			/* First CNI, assume no channel change. */

			vbi_network_set_cni (&cn->network,
					     VBI_CNI_TYPE_VPS, cni);

			cn->confirm_cni_vps = 0;

			if (0 == cn->confirm_cni_8301
			    && 0 == cn->confirm_cni_8302) {
				/* All CNIs valid, cancel reset requests. */
				reset (vbi, NULL, -1);
			}
		} else {
			vbi_network nk;
			cache_network *cn;

			/* Different CNI, channel change detected. */

			vbi_network_init (&nk);
			vbi_network_set_cni (&nk, VBI_CNI_TYPE_VPS, cni);

			cn = _vbi_cache_add_network
				(vbi->vt.cache, &nk, vbi->vt.videostd_set);

			reset (vbi, cn, 0 /* now */);

			cache_network_release (cn);

			vbi_network_destroy (&nk);
		}

		/* NB reset() invalidates cn. */

		e.type = VBI_EVENT_NETWORK;
		e.network = &vbi->vt.network->network;
		e.timestamp = vbi->time;

		_vbi_event_handler_list_send (&vbi->handlers, &e);
	} else {
		/* VPS is poorly error protected. We accept this CNI
		   after receiving it twice in a row. */
		cn->confirm_cni_vps = cni;

		if (0 == cn->network.cni_vps) {
			/* First CNI, channel change possible. */
			cni_change (vbi, VBI_CNI_TYPE_VPS, cni);
		} else {
			/* Assume a channel change with unknown CNI if we
			   cannot confirm the new CNI or receive the old
			   CNI again within n seconds. */
			reset (vbi, NULL, vbi->time
			       + vbi->vt.cni_vps_timeout);
		}

		return TRUE;
	}

	if ((vbi->handlers.event_mask & VBI_EVENT_PROG_ID)
	    && vbi->reset_time <= 0.0 /* not suspended */) {
		vbi_program_id pid;
		vbi_program_id *p;
		cache_network *cn;

		if (!vbi_decode_vps_pdc (&pid, buffer))
			return FALSE;

		cn = vbi->vt.network;

		p = &cn->program_id[VBI_PID_CHANNEL_VPS];

		if (p->cni != pid.cni
		    || p->pil != pid.pil
		    || p->pcs_audio != pid.pcs_audio
		    || p->pty != pid.pty) {
			vbi_event e;

			*p = pid;

			e.type = VBI_EVENT_PROG_ID;
			e.network = &cn->network;
			e.timestamp = vbi->time;

			e.ev.prog_id = p;

			_vbi_event_handler_list_send (&vbi->handlers, &e);
		}
	}

	return TRUE;
}

static void
aspect_event			(vbi_decoder *		vbi,
				 const vbi_aspect_ratio *ar)
{
	cache_network *cn;
	vbi_event e;

	cn = vbi->vt.network;

	if (0 == memcmp (&cn->aspect_ratio, ar, sizeof (cn->aspect_ratio)))
		return;

	cn->aspect_ratio = *ar;

	e.type = VBI_EVENT_ASPECT;
	e.network = &vbi->vt.network->network;
	e.timestamp = vbi->time;

	e.ev.aspect = &cn->aspect_ratio;

	_vbi_event_handler_list_send (&vbi->handlers, &e);
}

/**
 */
void
vbi_decoder_decode		(vbi_decoder *		vbi,
				 vbi_sliced *		sliced,
				 unsigned int		lines,
				 double			time)
{
	vbi_bool r;
	double d;

	r = TRUE;

	d = time - vbi->time;

	if (vbi->time > 0 && (d < 0.025 || d > 0.050)) {
	  /*
	   *  Since (dropped >= channel switch) we give
	   *  ~1.5 s, then assume a switch.
	   */
	  //obsolete	  pthread_mutex_lock(&vbi->chswcd_mutex);

#warning later
	  //	  if (vbi->chswcd == 0)
	  //		  vbi->chswcd = 40;

	  //	  pthread_mutex_unlock(&vbi->chswcd_mutex);

	  if (0)
		  fprintf(stderr, "vbi frame/s dropped at %f, D=%f\n",
			  time, time - vbi->time);

	  if (vbi->handlers.event_mask &
	      (VBI_EVENT_TTX_PAGE | VBI_EVENT_NETWORK))
		  _vbi_teletext_decoder_resync (&vbi->vt);
	  if (vbi->handlers.event_mask &
	      (VBI_EVENT_CAPTION | VBI_EVENT_NETWORK))
		  vbi_caption_decoder_resync (&vbi->cc);
	} else {
	  //		pthread_mutex_lock(&vbi->chswcd_mutex);

		if (0) { //vbi->chswcd > 0 && --vbi->chswcd == 0) {
		  //			pthread_mutex_unlock(&vbi->chswcd_mutex);
//			vbi_chsw_reset(vbi, 0);
		} else {
		  //			pthread_mutex_unlock(&vbi->chswcd_mutex);
		}
	}

	if (time > vbi->time)
		vbi->time = time;

	while (lines) {
		if (sliced->id & VBI_SLICED_TELETEXT_B_625) {
			vbi->time_teletext = vbi->time;

			r &= vbi_teletext_decoder_decode
				(&vbi->vt, sliced->data, time);
		} else if ((sliced->id & VBI_SLICED_CAPTION_525)
			   && (0 == sliced->line || 21 == sliced->line)) {
			vbi->time_caption = vbi->time;

			vbi_decode_caption (&vbi->cc, sliced->line,
					    sliced->data);
		} else if ((sliced->id & VBI_SLICED_CAPTION_625)
			   && (0 == sliced->line || 22 == sliced->line)) {
			vbi->time_caption = vbi->time;

			vbi_decode_caption (&vbi->cc, sliced->line,
					    sliced->data);
		} else if ((sliced->id & VBI_SLICED_VPS)
			   && (0 == sliced->line || 16 == sliced->line)) {
			vbi->time_vps = vbi->time;

			r &= decode_vps (vbi, sliced->data);
		} else if ((sliced->id & VBI_SLICED_WSS_625)
			   && (0 == sliced->line || 23 == sliced->line)) {
			vbi_aspect_ratio aspect;

			vbi->time_wss_625 = vbi->time;

			CLEAR (aspect); /* no random gaps in memcmp */

			if (vbi_decode_wss_625 (&aspect, sliced->data))
				aspect_event (vbi, &aspect);
			else
				r = FALSE;
		} else if (sliced->id & VBI_SLICED_WSS_CPR1204) {
			vbi_aspect_ratio aspect;

			vbi->time_wss_cpr1204 = vbi->time;

			CLEAR (aspect); /* no random gaps */

			if (vbi_decode_wss_cpr1204 (&aspect, sliced->data))
				aspect_event (vbi, &aspect);
			else
				r = FALSE;
		}

		sliced++;
		lines--;
	}

//	if (vbi->handlers.event_mask & VBI_EVENT_TRIGGER)
//		vbi_deferred_trigger(vbi);

//	if (0 && (rand() % 511) == 0)
//		vbi_eacem_trigger
//		  (vbi, (unsigned char *) /* Latin-1 */
//		   "<http://zapping.sourceforge.net>[n:Zapping][5450]");
}

/**
 * @param vbi VBI decoder allocated with vbi_decoder_new().
 * @param callback Function to be called on events.
 * @param user_data User pointer passed through to the @a callback function.
 * 
 * Removes all event handlers from the list with this @a callback and
 * @a user_data. You can safely call this function from a handler removing
 * itself or another handler.
 */
void
vbi_decoder_remove_event_handler
				(vbi_decoder *		vbi,
				 vbi_event_cb *		callback,
				 void *			user_data)
{
	assert (NULL != vbi);

	vbi_teletext_decoder_remove_event_handler
		(&vbi->vt, callback, user_data);

	vbi_caption_decoder_remove_event_handler
		(&vbi->cc, callback, user_data);

	_vbi_event_handler_list_remove_by_callback
		(&vbi->handlers, callback, user_data);
}

/**
 * @param vbi VBI decoder allocated with vbi_decoder_new().
 * @param event_mask Set of events (@c VBI_EVENT_) the handler is waiting
 *   for, can be -1 for all and 0 for none.
 * @param callback Function to be called on events by
 *   vbi_decoder_decode() and other vbi_decoder functions as noted.
 * @param user_data User pointer passed through to the @a callback function.
 * 
 * Adds a new event handler to the VBI decoder. When the @a callback
 * with @a user_data is already registered the function merely changes the
 * set of events it will receive in the future. When the @a event_mask is
 * empty the function does nothing or removes an already registered event
 * handler. You can safely call this function from an event handler.
 *
 * Any number of handlers can be added, also different handlers for the
 * same event which will be called in registration order.
 *
 * @returns
 * @c FALSE on failure.
 */
vbi_bool
vbi_decoder_add_event_handler	(vbi_decoder *		vbi,
				 unsigned int		event_mask,
				 vbi_event_cb *		callback,
				 void *			user_data)
{
	static const unsigned int virtual_events =
		VBI_EVENT_CLOSE |
		VBI_EVENT_RESET;

	assert (NULL != vbi);

	if (!vbi_teletext_decoder_add_event_handler
	    (&vbi->vt, event_mask & ~virtual_events, callback, user_data))
		return FALSE;

	if (!vbi_caption_decoder_add_event_handler
	    (&vbi->cc, event_mask & ~virtual_events, callback, user_data)) {
		vbi_teletext_decoder_remove_event_handler
			(&vbi->vt, callback, user_data);
		return FALSE;
	}

	event_mask &=
		virtual_events |
		VBI_EVENT_NETWORK |
		VBI_EVENT_PROG_ID |
		VBI_EVENT_ASPECT;

	if (event_mask && !_vbi_event_handler_list_add
	    (&vbi->handlers, event_mask, callback, user_data)) {
		vbi_teletext_decoder_remove_event_handler
			(&vbi->vt, callback, user_data);
		vbi_caption_decoder_remove_event_handler
			(&vbi->cc, callback, user_data);
		return FALSE;
	}

	return TRUE;
}

/**
 * @param vbi VBI decoder allocated with vbi_decoder_new().
 * @param nk Identifies the new network, can be @c NULL.
 *
 * Resets the VBI decoder, useful for example after a channel change.
 *
 * This function sends a @c VBI_EVENT_RESET.
 */
void
vbi_decoder_reset		(vbi_decoder *		vbi,
				 const vbi_network *	nk,
				 vbi_videostd_set	videostd_set)
{
	cache_network *cn;

	cn = _vbi_cache_add_network (vbi->vt.cache, nk, videostd_set);
	reset (vbi, cn, 0.0);
	cache_network_release (cn);
}

static void
teletext_reset_trampoline	(vbi_teletext_decoder *	td,
				 cache_network *	cn,
				 double			time)
{
	reset (PARENT (td, vbi_decoder, vt), cn, time);
}

static void
caption_reset_trampoline	(vbi_caption_decoder *	cd,
				 cache_network *	cn,
				 double			time)
{
	reset (PARENT (cd, vbi_decoder, cc), cn, time);
}

/**
 * @param vbi VBI decoder structure to be destroyed.
 *
 * Frees all resources associated with @a vbi, except the structure itself.
 * This function sends a @c VBI_EVENT_CLOSE.
 */
void
_vbi_decoder_destroy		(vbi_decoder *		vbi)
{
	vbi_event e;

	assert (NULL != vbi);

	e.type		= VBI_EVENT_CLOSE;
	e.network	= &vbi->vt.network->network;
	e.timestamp	= vbi->time;

	_vbi_event_handler_list_send (&vbi->handlers, &e);

	_vbi_caption_decoder_destroy (&vbi->cc);
	_vbi_teletext_decoder_destroy (&vbi->vt);

	_vbi_event_handler_list_destroy (&vbi->handlers);

	CLEAR (*vbi);
}

/**
 * @internal
 * @param vbi VBI decoder structure to be initialized.
 * @param ca Cache to be used by this decoder, can be @c NULL.
 *   To allocate a cache call vbi_cache_new(). Caches have a reference
 *   counter, you can vbi_cache_release() after calling this function.
 * @param nk Initial network (see vbi_decoder_reset()),
 *   can be @c NULL.
 *
 * Initialize a VBI decoder structure.
 *
 * @returns
 * @c FALSE on failure (out of memory).
 */
vbi_bool
_vbi_decoder_init		(vbi_decoder *		vbi,
				 vbi_cache *		ca,
				 const vbi_network *	nk,
				 vbi_videostd_set	videostd_set)
{
	vbi_cache *cache;

	assert (NULL != vbi);

	CLEAR (*vbi);

	vbi->time		= 0.0;

	vbi->time_teletext	= -1000000.0;
	vbi->time_caption	= -1000000.0;
	vbi->time_vps		= -1000000.0;
	vbi->time_wss_625	= -1000000.0;
	vbi->time_wss_cpr1204	= -1000000.0;

	if (ca) {
		cache = ca;
	} else {
		if (!(cache = vbi_cache_new ()))
			return FALSE;
	}

	_vbi_event_handler_list_init (&vbi->handlers);

	_vbi_teletext_decoder_init (&vbi->vt, cache, nk, videostd_set);
	_vbi_caption_decoder_init (&vbi->cc, cache, nk, videostd_set);

	if (!ca) {
		/* Drop our reference. */
		vbi_cache_release (cache);
	}

	vbi->reset_time = 0.0;

	/* Redirect reset requests to parent vbi_decoder. */

	vbi->teletext_reset = vbi->vt.virtual_reset;
	vbi->vt.virtual_reset = teletext_reset_trampoline;

	vbi->caption_reset = vbi->cc.virtual_reset;
	vbi->cc.virtual_reset = caption_reset_trampoline;

	return TRUE;
}

/**
 * @param vbi VBI decoder allocated with vbi_decoder_new(), can be @c NULL.
 *
 * Frees all resources associated with @a vbi. This function sends a
 * @c VBI_EVENT_CLOSE.
 */
void
vbi_decoder_delete		(vbi_decoder *		vbi)
{
	if (NULL == vbi)
		return;

	_vbi_decoder_destroy (vbi);

	free (vbi);
}

/**
 * @param ca Cache to be used by this decoder, can be @c NULL.
 *   To allocate a cache call vbi_cache_new(). Caches have a reference
 *   counter, you can vbi_cache_release() after calling this function.
 *
 * Allocates a new VBI (Teletext, Closed Caption, VPS, WSS) decoder.
 *
 * @returns
 * Pointer to newly allocated VBI decoder which must be freed with
 * vbi_decoder_delete() when done. @c NULL on failure (out of memory).
 */
vbi_decoder *
vbi_decoder_new			(vbi_cache *		ca,
				 const vbi_network *	nk,
				 vbi_videostd_set	videostd_set)
{
	vbi_decoder *vbi;

	if (!(vbi = malloc (sizeof (*vbi)))) {
		vbi_log_printf (VBI_DEBUG, __FUNCTION__,
				"Out of memory (%u)", sizeof (*vbi));
		return NULL;
	}

        if (!_vbi_decoder_init (vbi, ca, nk, videostd_set)) {
		free (vbi);
		vbi = NULL;
	}

	return vbi;
}
