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

/* $Id: vbi_decoder.c,v 1.1.2.7 2007-11-01 00:21:25 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "misc.h"
#ifndef ZAPPING8
#  include "vps.h"
#  include "wss.h"
#endif
#include "vbi_decoder-priv.h"

/**
 * @param vbi VBI decoder allocated with vbi3_decoder_new().
 * @param nk Identifies the network transmitting the page.
 * @param pgno Teletext page number or Closed Caption channel.
 * @param subno Teletext subpage number, can be @c VBI3_ANY_SUBNO
 *   (most recently received subpage, if any).
 * @param format_options Array of pairs of a vbi3_format_option and value,
 *   terminated by a @c 0 option. See vbi3_decoder_get_page().
 *
 * Allocates a new vbi3_page, formatted from a cached Teletext page.
 *
 * @returns
 * vbi3_page which must be freed with vbi3_page_delete() when done.
 * @c NULL on error.
 */
vbi3_page *
vbi3_decoder_get_page_va_list	(vbi3_decoder *		vbi,
				 const vbi3_network *	nk,
				 vbi3_pgno		pgno,
				 vbi3_subno		subno,
				 va_list		format_options)
{
	if (pgno < 0x100)
		return vbi3_caption_decoder_get_page_va_list
			(&vbi->cc, pgno, format_options);
	else
		return vbi3_teletext_decoder_get_page_va_list
			(&vbi->vt, nk, pgno, subno, format_options);
}

/**
 * @param vbi VBI decoder allocated with vbi3_decoder_new().
 * @param nk Identifies the network transmitting the page.
 * @param pgno Teletext page number or Closed Caption channel.
 * @param subno Teletext subpage number, can be @c VBI3_ANY_SUBNO
 *   (most recently received subpage, if any).
 * @param ... Array of pairs of a vbi3_format_option and value,
 *   terminated by a @c 0 option.
 *
 * Allocates a new vbi3_page, formatted from a cached Teletext
 * or Closed Caption page.
 *
 * Example:
 *
 * @code
 * vbi3_page *pg;
 *
 * pg = vbi3_decoder_get_page (vbi, NULL, 0x100, VBI3_ANY_SUBNO,
 *			      VBI3_NAVIGATION, TRUE,
 *			      VBI3_WST_LEVEL, VBI3_LEVEL_2p5,
 *			      0);
 * @endcode
 *
 * @returns
 * vbi3_page which must be freed with vbi3_page_delete() when done.
 * @c NULL on error.
 */
vbi3_page *
vbi3_decoder_get_page		(vbi3_decoder *		vbi,
				 const vbi3_network *	nk,
				 vbi3_pgno		pgno,
				 vbi3_subno		subno,
				 ...)
{
	vbi3_page *pg;
	va_list format_options;

	va_start (format_options, subno);

	if (pgno < 0x100)
		pg = vbi3_caption_decoder_get_page_va_list
			(&vbi->cc, pgno, format_options);
	else
		pg = vbi3_teletext_decoder_get_page_va_list
			(&vbi->vt, nk, pgno, subno, format_options);

	va_end (format_options);

	return pg;
}

#ifndef ZAPPING8

/**
 * @param vbi VBI decoder allocated with vbi3_decoder_new().
 * @param pid Program ID will be stored here.
 * @param channel Logical channel transmitting the ID.
 *
 * Returns the most recently on a logical channel received program ID.
 *
 * Sources can be VBI3_SLICED_TELETEXT_B and VBI3_SLICED_VPS (EN 300 231 PDC),
 * and VBI3_SLICED_CAPTION_525 (EIA 608 XDS) data.
 */
void
vbi3_decoder_get_program_id	(vbi3_decoder *		vbi,
				 vbi3_program_id *	pid,
				 vbi3_pid_channel	channel)
{
	cache_network *cn;

	assert (NULL != vbi);
	assert (NULL != pid);

	cn = vbi->vt.network;

	assert ((unsigned int) channel < N_ELEMENTS (cn->program_id));

	*pid = cn->program_id[channel];
}

/**
 * @param vbi VBI decoder allocated with vbi3_decoder_new().
 * @param ar Aspect ratio information will be stored here.
 *
 * Returns the most recently received aspect ratio information.
 *
 * Sources can be VBI3_SLICED_WSS_625 (EN 300 294), VBI3_SLICED_WSS_CPR1204
 * (EIA-J CPR-1204) and VBI3_SLICED_CAPTION_525 (EIA 608 XDS) data.
 */
void
vbi3_decoder_get_aspect_ratio	(vbi3_decoder *		vbi,
				 vbi3_aspect_ratio *	ar)
{
	assert (NULL != vbi);
	assert (NULL != ar);

	*ar = vbi->vt.network->aspect_ratio;
}

#endif /* !ZAPPING8 */

vbi3_teletext_decoder *
vbi3_decoder_cast_to_teletext_decoder
				(vbi3_decoder *		vbi)
{
	assert (NULL != vbi);

	return &vbi->vt;
}

vbi3_caption_decoder *
vbi3_decoder_cast_to_caption_decoder
				(vbi3_decoder *		vbi)
{
	assert (NULL != vbi);

	return &vbi->cc;
}

/**
 * @internal
 * @param vbi VBI decoder allocated with vbi3_decoder_new().
 * @param cn New network, can be @c NULL if 0.0 != time.
 * @param time Deferred reset when time is greater than
 *   vbi3_decoder_feed() timestamp. Pass a negative number to
 *   cancel a deferred reset, 0.0 to reset immediately.
 *
 * Internal reset function.
 */
static void
internal_reset			(vbi3_decoder *		vbi,
				 cache_network *	cn,
				 double			time)
{
	vbi3_event e;

	if (time <= 0.0 /* reset now or cancel deferred reset */
	    || time > vbi->reset_time)
		vbi->reset_time = time;

	vbi->teletext_reset (&vbi->vt, cn, time);
	vbi->caption_reset (&vbi->cc, cn, time);

	if (0.0 == time /* reset now */) {
		e.type		= VBI3_EVENT_RESET;
		e.network	= &vbi->vt.network->network;
		e.timestamp	= vbi->timestamp;

		_vbi3_event_handler_list_send (&vbi->handlers, &e);
	}
}

void
vbi3_decoder_detect_channel_change
				(vbi3_decoder *		vbi,
				 vbi3_bool		enable)
{
	/* XXX different methods? */
	vbi->dcc = enable;
}

#ifndef ZAPPING8

static void
cni_change			(vbi3_decoder *		vbi,
				 vbi3_cni_type		type,
				 unsigned int		cni)
{
	cache_network *cn;
	double timeout;

	cn = vbi->vt.network;

	/* Timeout is variable because VPS and 8/30 repeat at
	   different rates. */
	timeout = 0.0;

	if (VBI3_CNI_TYPE_VPS != type
	    && 0 != cn->network.cni_vps
	    && (cn->network.cni_vps
		!= vbi3_convert_cni (VBI3_CNI_TYPE_VPS, type, cni))) {
		/* We cannot say with certainty if cni and cni_vps belong
		   to the same network. If 0 == vbi3_convert_cni() we cannot
		   convert, otherwise CNIs mismatch because the channel
		   changed, the network transmits wrong CNIs or the
		   conversion is incorrect.

		   After n seconds, if we cannot confirm the cni_vps or
		   receive a new one, we assume there was a channel change
		   and the new network does not transmit a cni_vps. */
		cn->confirm_cni_vps = cn->network.cni_vps;
		timeout = vbi->vt.cni_vps_timeout;
	}

	if (VBI3_CNI_TYPE_8301 != type
	    && 0 != cn->network.cni_8301
	    && (cn->network.cni_8301
		!= vbi3_convert_cni (VBI3_CNI_TYPE_8301, type, cni))) {
		cn->confirm_cni_8301 = cn->network.cni_8301;
		timeout = vbi->vt.cni_830_timeout;
	}

	if (VBI3_CNI_TYPE_8302 != type
	    && 0 != cn->network.cni_8302
	    && (cn->network.cni_8302
		!= vbi3_convert_cni (VBI3_CNI_TYPE_8302, type, cni))) {
		cn->confirm_cni_8302 = cn->network.cni_8302;
		timeout = vbi->vt.cni_830_timeout;
	}

	if (timeout > 0.0) {
		/* Arrange for a reset in timeout seconds
		   and discard all data received in the meantime. */
		internal_reset (vbi, cn, vbi->timestamp + timeout);
	}
}

static vbi3_bool
decode_vps			(vbi3_decoder *		vbi,
				 const uint8_t		buffer[13])
{
	unsigned int cni;
	cache_network *cn;

	if (!vbi3_decode_vps_cni (&cni, buffer))
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
			internal_reset (vbi, cn, -1);
		}
	} else if (cni == cn->confirm_cni_vps) {
		vbi3_event e;

		/* The CNI is correct. */

		if (0 == cn->network.cni_vps) {
			/* First time CNI, assume no channel change. */

			vbi3_network_set_cni (&cn->network,
					     VBI3_CNI_TYPE_VPS, cni);

			cn->confirm_cni_vps = 0;

			if (0 == cn->confirm_cni_8301
			    && 0 == cn->confirm_cni_8302) {
				/* All CNIs valid, cancel reset requests. */
				internal_reset (vbi, cn, -1);
			}
		} else {
			vbi3_network nk;
			cache_network *cn;

			/* Different CNI, channel change detected. */

			vbi3_network_init (&nk);
			vbi3_network_set_cni (&nk, VBI3_CNI_TYPE_VPS, cni);

			cn = _vbi3_cache_add_network
				(vbi->vt.cache, &nk, vbi->vt.videostd_set);

			internal_reset (vbi, cn, 0.0 /* now */);

			cache_network_unref (cn);

			vbi3_network_destroy (&nk);
		}

		/* internal_reset() may have changed this. */
		cn = vbi->vt.network;

		e.type = VBI3_EVENT_NETWORK;
		e.network = &cn->network;
		e.timestamp = vbi->timestamp;

		_vbi3_event_handler_list_send (&vbi->handlers, &e);
	} else {
		/* VPS is poorly error protected but repeats once per frame.
		   We accept this CNI after receiving it twice in a row. */
		cn->confirm_cni_vps = cni;

		if (0 == cn->network.cni_vps) {
			/* First time CNI, channel change possible. */
			cni_change (vbi, VBI3_CNI_TYPE_VPS, cni);
		} else {
			/* Assume a channel change with unknown CNI if we
			   cannot confirm the new CNI or receive the old
			   CNI again within timeout seconds. */
			internal_reset (vbi, cn, vbi->timestamp
					+ vbi->vt.cni_vps_timeout);
		}

		/* Discard PDC data until we identified the network. */

		return TRUE;
	}

	if ((vbi->handlers.event_mask & VBI3_EVENT_PROG_ID)
	    && vbi->reset_time <= 0.0 /* not suspended */) {
		vbi3_program_id pid;
		vbi3_program_id *p;
		cache_network *cn;

		if (!vbi3_decode_vps_pdc (&pid, buffer))
			return FALSE;

		cn = vbi->vt.network;

		p = &cn->program_id[VBI3_PID_CHANNEL_VPS];

		if (p->cni != pid.cni
		    || p->pil != pid.pil
		    || p->pcs_audio != pid.pcs_audio
		    || p->pty != pid.pty) {
			vbi3_event e;

			/* Program ID changed. */

			*p = pid;

			e.type = VBI3_EVENT_PROG_ID;
			e.network = &cn->network;
			e.timestamp = vbi->timestamp;

			e.ev.prog_id = p;

			_vbi3_event_handler_list_send (&vbi->handlers, &e);
		}
	}

	return TRUE;
}

static void
aspect_event			(vbi3_decoder *		vbi,
				 const vbi3_aspect_ratio *ar)
{
	cache_network *cn;
	vbi3_event e;

	cn = vbi->vt.network;

	if (0 == memcmp (&cn->aspect_ratio, ar, sizeof (cn->aspect_ratio))) {
		/* No change. */
		return;
	}

	/* WSS is poorly error protected but repeats once per frame.
	   We accept this aspect ratio after receiving it twice. */
	if (0 != memcmp (&vbi->confirm_aspect_ratio, ar,
			 sizeof (vbi->confirm_aspect_ratio))) {
		vbi->confirm_aspect_ratio = *ar;
		return;
	}

	cn->aspect_ratio = *ar;

	e.type = VBI3_EVENT_ASPECT;
	e.network = &cn->network;
	e.timestamp = vbi->timestamp;

	e.ev.aspect = &cn->aspect_ratio;

	_vbi3_event_handler_list_send (&vbi->handlers, &e);
}

#endif /* !ZAPPING8 */

/**
 */
void
vbi3_decoder_feed		(vbi3_decoder *		vbi,
				 const vbi3_sliced *	sliced,
				 unsigned int		n_lines,
				 double			timestamp)
{
	vbi3_bool all_success;

	if (vbi->dcc) {
		double dt;

		dt = timestamp - vbi->timestamp;

		if (vbi->timestamp > 0.0 && (dt < 0.025 || dt > 0.050)) {
			if (0)
				fprintf (stderr,
					 "vbi frame/s dropped at %f, dt=%f\n",
					 timestamp, dt);

			if (0 != vbi->vt.handlers.event_mask
			    || (VBI3_EVENT_NETWORK & vbi->handlers.event_mask)) {
				_vbi3_teletext_decoder_resync (&vbi->vt);
			}

			if (0 != vbi->cc.handlers.event_mask
			    || (VBI3_EVENT_NETWORK & vbi->handlers.event_mask)) {
				_vbi3_caption_decoder_resync (&vbi->cc);
			}

			/* Set to all failed. */
			vbi->error_history_vps = 0;
			vbi->error_history_wss_625 = 0;
			vbi->error_history_wss_cpr1204 = 0;

			vbi->timestamp = timestamp;

			/* Assuming a channel change, arrange for a reset in 1.5
			   seconds if we don't receive the old or a new network ID,
			   and discard all data received in the meantime. */
			internal_reset (vbi, /* cn */ NULL, vbi->timestamp + 1.5);
		} else {
// FIXME deferred reset here
		}
	}

	if (timestamp > vbi->timestamp) {
		vbi->timestamp = timestamp;

		if (vbi->handlers.event_mask & VBI3_EVENT_TIMER) {
			vbi3_event e;

			e.type = VBI3_EVENT_TIMER;
			e.network = &vbi->vt.network->network;
			e.timestamp = timestamp;

			_vbi3_event_handler_list_send (&vbi->handlers, &e);
		}
	}

	all_success = TRUE;

	while (n_lines > 0) {
		if (sliced->id & VBI3_SLICED_TELETEXT_B_625) {
			vbi->timestamp_teletext = vbi->timestamp;
			all_success &= vbi3_teletext_decoder_feed
				(&vbi->vt, sliced->data, vbi->timestamp);
		} else if (sliced->id & VBI3_SLICED_CAPTION_525) {
			vbi->timestamp_caption = vbi->timestamp;
			all_success &= vbi3_caption_decoder_feed
				(&vbi->cc, sliced->data,
				 sliced->line, vbi->timestamp);
#ifndef ZAPPING8
		} else if ((sliced->id & VBI3_SLICED_VPS)
			   && (0 == sliced->line || 16 == sliced->line)) {
			vbi->timestamp_vps = vbi->timestamp;
			all_success &= decode_vps (vbi, sliced->data);
		} else if ((sliced->id & VBI3_SLICED_WSS_625)
			   && (0 == sliced->line || 23 == sliced->line)) {
			vbi3_aspect_ratio aspect;
			vbi3_bool success;

			vbi->timestamp_wss_625 = vbi->timestamp;

			/* Make sure memcmp() won't see random bits in
			   gaps in the structure. */
			CLEAR (aspect);

			success = vbi3_decode_wss_625 (&aspect, sliced->data);
			vbi->error_history_wss_625 =
				vbi->error_history_wss_625 * 2 + success;
			all_success &= success;

			if (success) {
				aspect_event (vbi, &aspect);
			}
		} else if (sliced->id & VBI3_SLICED_WSS_CPR1204) {
			vbi3_aspect_ratio aspect;
			vbi3_bool success;

			vbi->timestamp_wss_cpr1204 = vbi->timestamp;

			CLEAR (aspect);

			success = vbi3_decode_wss_cpr1204 (&aspect,
							   sliced->data);
			vbi->error_history_wss_cpr1204 =
				vbi->error_history_wss_cpr1204 * 2 + success;
			all_success &= success;

			if (success) {
				aspect_event (vbi, &aspect);
			}
#endif /* !ZAPPING8 */
		}

		++sliced;
		--n_lines;
	}

#if 0 /* TODO */
	if (vbi->handlers.event_mask & VBI3_EVENT_TRIGGER)
		vbi3_deferred_trigger(vbi);

	if (0 && (rand() % 511) == 0)
		vbi3_eacem_trigger
		  (vbi, (unsigned char *) /* Latin-1 */
		   "<http://zapping.sourceforge.net>[n:Zapping][5450]");
#endif
}

/**
 * @param vbi VBI decoder allocated with vbi3_decoder_new().
 * @param callback Function to be called on events.
 * @param user_data User pointer passed through to the @a callback function.
 * 
 * Removes all event handlers from the list with this @a callback and
 * @a user_data. You can safely call this function from a handler removing
 * itself or another handler.
 */
void
vbi3_decoder_remove_event_handler
				(vbi3_decoder *		vbi,
				 vbi3_event_cb *		callback,
				 void *			user_data)
{
	assert (NULL != vbi);

	vbi3_teletext_decoder_remove_event_handler
		(&vbi->vt, callback, user_data);

	vbi3_caption_decoder_remove_event_handler
		(&vbi->cc, callback, user_data);

	_vbi3_event_handler_list_remove_by_callback
		(&vbi->handlers, callback, user_data);
}

/**
 * @param vbi VBI decoder allocated with vbi3_decoder_new().
 * @param event_mask Set of events (@c VBI3_EVENT_) the handler is waiting
 *   for, can be -1 for all and 0 for none.
 * @param callback Function to be called on events by
 *   vbi3_decoder_feed() and other vbi3_decoder methods as noted.
 * @param user_data User pointer passed through to the @a callback function.
 * 
 * Adds a new event handler to the VBI decoder. When the @a callback
 * with @a user_data is already registered the function merely changes the
 * set of events it will receive in the future. When the @a event_mask is
 * empty the function does nothing or removes a registered event
 * handler. You can safely call this function from an event handler.
 *
 * Any number of handlers can be added, also different handlers for the
 * same event which will be called in registration order.
 *
 * @returns
 * @c FALSE on failure (out of memory), removing the handler.
 */
vbi3_bool
vbi3_decoder_add_event_handler	(vbi3_decoder *		vbi,
				 vbi3_event_mask	event_mask,
				 vbi3_event_cb *	callback,
				 void *			user_data)
{
	vbi3_event_mask child_events;

	assert (NULL != vbi);

	child_events = event_mask & ~(VBI3_EVENT_CLOSE |
				      VBI3_EVENT_RESET |
				      VBI3_EVENT_TIMER);

	if (vbi3_teletext_decoder_add_event_handler
	    (&vbi->vt, child_events, callback, user_data)) {

		if (vbi3_caption_decoder_add_event_handler
		    (&vbi->cc, child_events, callback, user_data)) {

			event_mask &= (VBI3_EVENT_CLOSE |
				       VBI3_EVENT_RESET |
				       VBI3_EVENT_NETWORK |
				       VBI3_EVENT_PROG_ID |
				       VBI3_EVENT_ASPECT |
				       VBI3_EVENT_TIMER);

			if (0 == event_mask) {
				return TRUE;
			}

			if (_vbi3_event_handler_list_add
			    (&vbi->handlers, event_mask,
			     callback, user_data)) {
				return TRUE;
			}

			vbi3_caption_decoder_remove_event_handler
				(&vbi->cc, callback, user_data);
		}

		vbi3_teletext_decoder_remove_event_handler
			(&vbi->vt, callback, user_data);
	}

	return FALSE;
}

/**
 * @param vbi VBI decoder allocated with vbi3_decoder_new().
 * @param nk Identifies the new network, can be @c NULL.
 * @param videostd_set The new video standard.
 *
 * Resets the VBI decoder, useful for example after a channel change.
 *
 * This function sends a @c VBI3_EVENT_RESET.
 */
void
vbi3_decoder_reset		(vbi3_decoder *		vbi,
				 const vbi3_network *	nk,
				 vbi3_videostd_set	videostd_set)
{
	cache_network *cn;

	cn = _vbi3_cache_add_network (vbi->vt.cache, nk, videostd_set);

	vbi->vt.videostd_set = videostd_set;
	vbi->cc.videostd_set = videostd_set;

	internal_reset (vbi, cn, /* time: now */ 0.0);

	cache_network_unref (cn);
}

static void
teletext_reset_trampoline	(vbi3_teletext_decoder *	td,
				 cache_network *	cn,
				 double			time)
{
	internal_reset (PARENT (td, vbi3_decoder, vt), cn, time);
}

static void
caption_reset_trampoline	(vbi3_caption_decoder *	cd,
				 cache_network *	cn,
				 double			time)
{
	internal_reset (PARENT (cd, vbi3_decoder, cc), cn, time);
}

/**
 * @param vbi VBI decoder structure to be destroyed.
 *
 * Frees all resources associated with @a vbi, except the structure itself.
 * This function sends a @c VBI3_EVENT_CLOSE.
 */
void
_vbi3_decoder_destroy		(vbi3_decoder *		vbi)
{
	vbi3_event e;

	assert (NULL != vbi);

	e.type		= VBI3_EVENT_CLOSE;
	e.network	= &vbi->vt.network->network;
	e.timestamp	= vbi->timestamp;

	_vbi3_event_handler_list_send (&vbi->handlers, &e);

	_vbi3_caption_decoder_destroy (&vbi->cc);
	_vbi3_teletext_decoder_destroy (&vbi->vt);

	_vbi3_event_handler_list_destroy (&vbi->handlers);

	/* Make it unusable. */

	CLEAR (*vbi);
}

/**
 * @internal
 * @param vbi VBI decoder structure to be initialized.
 * @param ca Cache to be used by this decoder, can be @c NULL.
 *   To allocate a cache call vbi3_cache_new(). Caches have a reference
 *   counter, you can vbi3_cache_unref() after calling this function.
 * @param nk Current network, can be @c NULL.
 * @param videostd_set The current video standard.
 *
 * Initialize a VBI decoder structure.
 *
 * @returns
 * @c FALSE on failure (out of memory).
 */
vbi3_bool
_vbi3_decoder_init		(vbi3_decoder *		vbi,
				 vbi3_cache *		ca,
				 const vbi3_network *	nk,
				 vbi3_videostd_set	videostd_set)
{
	vbi3_cache *cache;

	assert (NULL != vbi);

	CLEAR (*vbi);

	vbi->dcc = TRUE;

	/* Most recent VBI data timestamp. */
	vbi->timestamp			= -1000000.0;

	/* Ditto for each service, to detect activity. */
	vbi->timestamp_teletext		= -1000000.0;
	vbi->timestamp_caption		= -1000000.0;
	vbi->timestamp_vps		= -1000000.0;
	vbi->timestamp_wss_625		= -1000000.0;
	vbi->timestamp_wss_cpr1204	= -1000000.0;

	if (ca) {
		cache = ca;
	} else {
		cache = vbi3_cache_new ();
		if (NULL == cache) {
			return FALSE;
		}
	}

	_vbi3_event_handler_list_init (&vbi->handlers);

	_vbi3_teletext_decoder_init (&vbi->vt, cache, nk, videostd_set);
	_vbi3_caption_decoder_init (&vbi->cc, cache, nk, videostd_set);

	if (NULL == ca) {
		/* Drop our reference. */
		vbi3_cache_unref (cache);
	}

	vbi->reset_time = 0.0; /* no deferred reset */

	/* Redirect reset requests to parent vbi3_decoder. */

	vbi->teletext_reset = vbi->vt.virtual_reset;
	vbi->vt.virtual_reset = teletext_reset_trampoline;

	vbi->caption_reset = vbi->cc.virtual_reset;
	vbi->cc.virtual_reset = caption_reset_trampoline;

	return TRUE;
}

/**
 * @param vbi VBI decoder allocated with vbi3_decoder_new(), can be @c NULL.
 *
 * Frees all resources associated with @a vbi. This function sends a
 * @c VBI3_EVENT_CLOSE.
 */
void
vbi3_decoder_delete		(vbi3_decoder *		vbi)
{
	if (NULL == vbi)
		return;

	_vbi3_decoder_destroy (vbi);

	vbi3_free (vbi);
}

static void
teletext_delete_trampoline	(vbi3_teletext_decoder *td)
{
	vbi3_decoder_delete (PARENT (td, vbi3_decoder, vt));
}

static void
caption_delete_trampoline	(vbi3_caption_decoder *	cd)
{
	vbi3_decoder_delete (PARENT (cd, vbi3_decoder, cc));
}

/**
 * @param ca Cache to be used by this decoder, can be @c NULL.
 *   To allocate a cache call vbi3_cache_new(). Caches have a reference
 *   counter, you can vbi3_cache_unref() after calling this function.
 * @param nk Current network, can be @c NULL.
 * @param videostd_set The current video standard.
 *
 * Allocates a new VBI (Teletext, Closed Caption, VPS, WSS, etc) decoder.
 *
 * @returns
 * Pointer to newly allocated VBI decoder which must be freed with
 * vbi3_decoder_delete() when done. @c NULL on failure (out of memory).
 */
vbi3_decoder *
vbi3_decoder_new		(vbi3_cache *		ca,
				 const vbi3_network *	nk,
				 vbi3_videostd_set	videostd_set)
{
	vbi3_decoder *vbi;

	vbi = vbi3_malloc (sizeof (*vbi));

	if (NULL != vbi) {
		if (_vbi3_decoder_init (vbi, ca, nk, videostd_set)) {
			/* Make it safe to delete a vbi3_teletext_decoder
			   or vbi3_caption_decoder cast from a vbi3_decoder. */
			vbi->vt.virtual_delete = teletext_delete_trampoline;
			vbi->cc.virtual_delete = caption_delete_trampoline;
		} else {
			vbi3_free (vbi);
			vbi = NULL;
		}
	}

	return vbi;
}

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
