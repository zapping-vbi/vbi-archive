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

/* $Id: vbi_decoder.c,v 1.1.2.1 2004-05-12 01:40:44 mschimek Exp $ */

#include <assert.h>
#include <stdlib.h>		/* malloc() */
#include "vbi_decoder-priv.h"
#include "misc.h"
#include "vps.h"
#include "wss.h"

const vbi_program_id *
vbi_decoder_program_id		(vbi_decoder *		vbi,
				 vbi_pid_channel	channel)
{
	vt_network *vtn;

	assert (NULL != vbi);

	vtn = vbi->vt.network;

	assert ((unsigned int) channel < N_ELEMENTS (vtn->program_id));

	return &vtn->program_id[channel];
}

static void
network_event			(vbi_decoder *		vbi,
				 vbi_nuid		nuid)
{
	vbi_event e;
/*
	if (VBI_NUID_UNKNOWN != vbi->teletext.network->received_nuid) {
		reset (vbi, nuid, 0);
	}

	e.type = VBI_EVENT_NETWORK;
	e.nuid = vbi->teletext.network->client_nuid;
	e.timestamp = vbi->timestamp;
*/
	_vbi_event_handler_list_send (&vbi->handlers, &e);
}

static vbi_bool
decode_vps			(vbi_decoder *		vbi,
				 const uint8_t		buffer[13])
{
	if (vbi->handlers.event_mask & VBI_EVENT_PROG_ID) {
		vbi_program_id pid;
		vbi_program_id *p;
		vt_network *vtn;

		if (!vbi_decode_vps_pdc (&pid, buffer))
			return FALSE;

		vtn = vbi->vt.network;

//		if (pid.nuid != vtn->received_nuid)
//			network_event (vbi, pid.nuid);

		p = &vtn->program_id[VBI_PID_CHANNEL_VPS];

		if (p->pil != pid.pil
		    || p->pcs_audio != pid.pcs_audio
		    || p->pty != pid.pty) {
			vbi_event e;

			*p = pid;

			e.type = VBI_EVENT_PROG_ID;
//			e.nuid = vtn->client_nuid;
//			e.timestamp = vbi->timestamp;
			e.ev.prog_id = p;

			_vbi_event_handler_list_send (&vbi->handlers, &e);
		}
	} else {
		unsigned int cni;
		vbi_nuid nuid;
		vt_network *vtn;

		if (!vbi_decode_vps_cni (&cni, buffer))
			return FALSE;

		nuid = vbi_nuid_from_cni (VBI_CNI_TYPE_VPS, cni);

		vtn = vbi->vt.network;

//		if (nuid != vtn->received_nuid)
//			network_event (vbi, nuid);
	}

	return TRUE;
}

const vbi_aspect_ratio *
vbi_decoder_aspect_ratio	(vbi_decoder *		vbi)
{
	assert (NULL != vbi);

	return &vbi->aspect;
}

static void
aspect_event			(vbi_decoder *		vbi,
				 vbi_aspect_ratio *	r)
{
	vbi_event e;

	if (0 == memcmp (&vbi->aspect, r, sizeof (vbi->aspect)))
		return;

	vbi->aspect = *r;

	e.type = VBI_EVENT_PROG_ID;
//	e.nuid = vbi->teletext.network->client_nuid;
//	e.timestamp = vbi->timestamp;

	_vbi_event_handler_list_send (&vbi->handlers, &e);
}

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

		if (vbi->chswcd > 0 && --vbi->chswcd == 0) {
		  //			pthread_mutex_unlock(&vbi->chswcd_mutex);
			vbi_chsw_reset(vbi, 0);
		} else {
		  //			pthread_mutex_unlock(&vbi->chswcd_mutex);
		}
	}

	if (time > vbi->time)
		vbi->time = time;

	while (lines) {
		if (sliced->id & VBI_SLICED_TELETEXT_B_625) {
			r &= vbi_teletext_decoder_decode
				(&vbi->vt, sliced->data, time);
		} else if (sliced->id & (VBI_SLICED_CAPTION_525 |
					 VBI_SLICED_CAPTION_625)) {
			vbi_decode_caption(&vbi->cc, sliced->line, sliced->data);
		} else if (sliced->id & VBI_SLICED_VPS) {
			r &= decode_vps (vbi, sliced->data);
		} else if (sliced->id & VBI_SLICED_WSS_625) {
			vbi_aspect_ratio aspect;

			CLEAR (aspect); /* no random gaps in memcmp */

			if (vbi_decode_wss_625 (&aspect, sliced->data))
				aspect_event (vbi, &aspect);
			else
				r = FALSE;
		} else if (sliced->id & VBI_SLICED_WSS_CPR1204) {
			vbi_aspect_ratio aspect;

			CLEAR (aspect); /* no random gaps */

			if (vbi_decode_wss_cpr1204 (&aspect, sliced->data))
				aspect_event (vbi, &aspect);
			else
				r = FALSE;
		}

		sliced++;
		lines--;
	}

	if (vbi->handlers.event_mask & VBI_EVENT_TRIGGER)
		vbi_deferred_trigger(vbi);

	if (0 && (rand() % 511) == 0)
		vbi_eacem_trigger
		  (vbi, (unsigned char *) /* Latin-1 */
		   "<http://zapping.sourceforge.net>[n:Zapping][5450]");
}

vbi_page *
vbi_decoder_get_teletext_page_va_list
				(vbi_decoder *		vbi,
				 vbi_nuid		nuid,
				 vbi_pgno		pgno,
				 vbi_subno		subno,
				 va_list		format_options)
{
	return vbi_teletext_decoder_get_page_va_list
		(&vbi->vt, nuid, pgno, subno, format_options);
}

vbi_page *
vbi_decoder_get_teletext_page	(vbi_decoder *		vbi,
				 vbi_nuid		nuid,
				 vbi_pgno		pgno,
				 vbi_subno		subno,
				 ...)
{
	vbi_page *pg;
	va_list format_options;

	va_start (format_options, subno);

	pg = vbi_teletext_decoder_get_page_va_list
		(&vbi->vt, nuid, pgno, subno, format_options);

	va_end (format_options);

	return pg;
}

void
vbi_decoder_remove_event_handler(vbi_decoder *		vbi,
				 vbi_event_cb *		callback,
				 void *			user_data)
{
	assert (NULL != vbi);

	vbi_teletext_decoder_remove_event_handler
		(&vbi->vt, callback, user_data);

	vbi_caption_decoder_remove_event_handler
		(&vbi->cc, callback, user_data);

	_vbi_event_handler_list_remove (&vbi->handlers, callback, user_data);
}

vbi_bool
vbi_decoder_add_event_handler	(vbi_decoder *		vbi,
				 unsigned int		event_mask,
				 vbi_event_cb *		callback,
				 void *			user_data)
{
	assert (NULL != vbi);

	if (!vbi_teletext_decoder_add_event_handler
	    (&vbi->vt, event_mask & ~VBI_EVENT_CLOSE, callback, user_data))
		return FALSE;

	if (!vbi_caption_decoder_add_event_handler
	    (&vbi->cc, event_mask & ~VBI_EVENT_CLOSE, callback, user_data)) {
		vbi_teletext_decoder_remove_event_handler
			(&vbi->vt, callback, user_data);
		return FALSE;
	}

	event_mask &= VBI_EVENT_CLOSE;

	if (!_vbi_event_handler_list_add
	    (&vbi->handlers, event_mask, callback, user_data)) {
		vbi_teletext_decoder_remove_event_handler
			(&vbi->vt, callback, user_data);
		vbi_teletext_decoder_remove_event_handler
			(&vbi->cc, callback, user_data);
		return FALSE;
	}

	return TRUE;
}

static void
reset				(vbi_decoder *		vbi,
				 vbi_nuid		nuid,
				 double			time)
{
	vbi->reset_time = time;

	vbi->teletext_reset (&vbi->vt, nuid, time);
//	vbi->caption_reset (&vbi->cc, nuid, time);
}

void
vbi_decoder_reset		(vbi_decoder *		vbi,
				 vbi_nuid		nuid)
{
	reset (vbi, nuid, 0.0);
}

static void
teletext_reset_trampoline	(vbi_teletext_decoder *	td,
				 vbi_nuid		nuid,
				 double			time)
{
	reset (PARENT (td, vbi_decoder, vt), nuid, time);
}

/*
static void
caption_reset_trampoline	(vbi_caption_decoder *	cd,
				 vbi_nuid		nuid,
				 double			time)
{
	reset (PARENT (cd, vbi_decoder, cc), nuid, time);
}
*/

void
_vbi_decoder_destroy		(vbi_decoder *		vbi)
{
	vbi_event e;

	assert (NULL != vbi);

	e.type		= VBI_EVENT_CLOSE;
//	e.nuid		= td->network->client_nuid;
//	e.timestamp	= td->timestamp;

	_vbi_event_handler_list_send (&vbi->handlers, &e);

	_vbi_caption_decoder_destroy (&vbi->cc);
	_vbi_teletext_decoder_destroy (&vbi->vt);

	_vbi_event_handler_list_destroy (&vbi->handlers);

	CLEAR (*vbi);
}

vbi_bool
_vbi_decoder_init		(vbi_decoder *		vbi,
				 vbi_cache *		ca,
				 vbi_nuid		nuid)
{
	vbi_cache *cache;

	assert (NULL != vbi);

	CLEAR (*vbi);

	if (ca) {
		cache = ca;
	} else {
		if (!(cache = vbi_cache_new ()))
			return FALSE;
	}

	_vbi_event_handler_list_init (&vbi->handlers);

	_vbi_teletext_decoder_init (&vbi->vt, cache, nuid);
	_vbi_caption_decoder_init (&vbi->cc, cache, nuid);

	if (!ca) {
		/* Drop our reference. */
		vbi_cache_delete (cache);
	}

	// XXX init aspect


	vbi->reset_time = 0.0;

	/* Redirect reset requests to parent vbi_decoder. */

	vbi->teletext_reset = vbi->vt.virtual_reset;
	vbi->vt.virtual_reset = teletext_reset_trampoline;

//	vbi->caption_reset = vbi->cc.virtual_reset;
//	vbi->cc.virtual_reset = caption_reset_trampoline;

	return TRUE;
}

void
vbi_decoder_delete		(vbi_decoder *		vbi)
{
	if (NULL == vbi)
		return;

	_vbi_decoder_destroy (vbi);

	free (vbi);
}

vbi_decoder *
vbi_decoder_new			(vbi_cache *		ca)
{
	vbi_decoder *vbi;

	if (!(vbi = malloc (sizeof (*vbi)))) {
		vbi_log_printf (VBI_DEBUG, __FUNCTION__,
				"Out of memory (%u)", sizeof (*vbi));
		return NULL;
	}

        if (!_vbi_decoder_init (vbi, ca, VBI_NUID_NONE)) {
		free (vbi);
		vbi = NULL;
	}

	return vbi;
}
