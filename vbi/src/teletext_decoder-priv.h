/*
 *  libzvbi - Teletext decoder
 *
 *  Copyright (C) 2000, 2001, 2002, 2003, 2004 Michael H. Schimek
 *
 *  Based on code from AleVT 1.5.1
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

/* $Id: teletext_decoder-priv.h,v 1.1.2.3 2007-11-01 00:21:25 mschimek Exp $ */

#ifndef TELETEXT_DECODER_PRIV_H
#define TELETEXT_DECODER_PRIV_H

#include "event-priv.h"		/* _vbi3_event_handler_list */
#include "cache-priv.h"		/* vbi3_cache, vt.h */
#ifndef ZAPPING8
#  include "pfc_demux.h"	/* vbi3_pfc_demux */
#  include "trigger.h"		/* _vbi3_trigger */
#endif
#include "teletext_decoder.h"

typedef void
teletext_reset_fn		(vbi3_teletext_decoder *td,
				 cache_network *	cn,
				 double			time);
typedef void
teletext_delete_fn		(vbi3_teletext_decoder *td);

struct _vbi3_teletext_decoder {
	/**
	 * Pages in progress, per magazine in case of parallel transmission.
	 * When complete we copy the page into the cache.
	 */
	cache_page		buffer[8];

	/**
	 * Current page, points into buffer[], switched by a page header
	 * with different magazine number. Can be @c NULL (no header
	 * received yet).
	 */
	cache_page *		current;

#ifndef ZAPPING8
	/** Page Function Clear EPG Data. */
	vbi3_pfc_demux		epg_stream[2];

	/** EACEM triggers. */
	_vbi3_trigger *		triggers;
#endif

	/** Used for channel switch detection, see there. */
	pagenum			header_page;
	uint8_t			header[40];

	/** Time to receive a packet 8/30 CNI or VPS CNI. */
	double			cni_830_timeout;
	double			cni_vps_timeout;

	/**
	 * Remember past uncorrectable errors: One bit for each call of
	 * vbi3_teletext_decoder_feed(), most recent result in lsb. The idea
	 * is to disable the decoder if we get too many errors.
	 */
	unsigned int		error_history;

	/* Interface */

	/** The cache we use. */
	vbi3_cache *		cache;

	/** Current network in the cache. */
	cache_network *		network;

	double			timestamp;
	double			reset_time;

	vbi3_videostd_set	videostd_set;

	/** Called by vbi3_teletext_decoder_reset(). */
	teletext_reset_fn *	virtual_reset;

	_vbi3_event_handler_list handlers;

	/** Called by vbi3_teletext_decoder_delete(). */
	teletext_delete_fn *	virtual_delete;
};

/* in packet.c */
extern cache_page *
_vbi3_convert_cached_page	(cache_page *		cp,
				 page_function		new_function);
extern void
_vbi3_teletext_decoder_resync	(vbi3_teletext_decoder *	td);
extern void
_vbi3_teletext_decoder_destroy	(vbi3_teletext_decoder *	td);
extern vbi3_bool
_vbi3_teletext_decoder_init	(vbi3_teletext_decoder *	td,
				 vbi3_cache *		ca,
				 const vbi3_network *	nk,
				 vbi3_videostd_set	videostd_set);

#endif /* TELETEXT_DECODER_PRIV_H */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
