/*
 *  libzvbi -- Teletext decoder internals
 *
 *  Copyright (C) 2000-2008 Michael H. Schimek
 *
 *  Originally based on AleVT 1.5.1 by Edgar Toernig
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the 
 *  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, 
 *  Boston, MA  02110-1301  USA.
 *  libzvbi - Teletext decoder
 */

/* $Id: ttx_decoder-priv.h,v 1.1.2.1 2008-08-19 10:56:06 mschimek Exp $ */

#ifndef TTX_DECODER_PRIV_H
#define TTX_DECODER_PRIV_H

#include "event-priv.h"		/* _vbi_event_handler_list */
#include "cache-priv.h"		/* vbi_cache, vt.h */
#include "ttx_decoder.h"

typedef void
ttx_decoder_reset_fn		(vbi_ttx_decoder *	td,
				 cache_network *	cn,
				 double			time);
typedef void
ttx_decoder_delete_fn		(vbi_ttx_decoder *	td);

struct _vbi_ttx_decoder {
	/**
	 * Pages in progress, per magazine in case of parallel
	 * transmission.  When complete we copy the page into the
	 * cache.
	 */
	cache_page		buffer[8];

	/**
	 * Current page, points into buffer[], switched by a page header
	 * with different magazine number. Can be @c NULL (no header
	 * received yet).
	 */
	cache_page *		current;

#if 0
	/** Page Function Clear EPG Data. */
	vbi_pfc_demux		epg_stream[2];

	/** EACEM triggers. */
	_vbi_trigger *		triggers;
#endif

	/** Used for channel switch detection, see there. */
	struct ttx_page_link	header_page;
	uint8_t			header[40];

#if 0
	/** Time to receive a packet 8/30 CNI or VPS CNI. */
	double			cni_830_timeout;
	double			cni_vps_timeout;
#endif

	/**
	 * Remember past uncorrectable errors: One bit for each call of
	 * vbi_teletext_decoder_feed(), most recent result in lsb. The idea
	 * is to disable the decoder if we get too many errors.
	 */
	unsigned int		error_history;

	/* Interface */

	/** The cache we use. */
	vbi_cache *		cache;

	/** Current network in the cache. */
	cache_network *		network;

	/** The time when we last received data. */
	struct timestamp	timestamp;

//	double			reset_time;

	/** Called by vbi_ttx_decoder_reset(). */
	ttx_decoder_reset_fn *	virtual_reset;

	_vbi_event_handler_list handlers;

	/** Called by vbi_ttx_decoder_delete(). */
	ttx_decoder_delete_fn *	virtual_delete;
};

extern cache_page *
_vbi_convert_cached_page	(cache_page *		cp,
				 enum ttx_page_function	new_function);
extern vbi_bool
_vbi_ttx_decoder_resync		(vbi_ttx_decoder *	td);
extern vbi_bool
_vbi_ttx_decoder_destroy	(vbi_ttx_decoder *	td);
extern vbi_bool
_vbi_ttx_decoder_init		(vbi_ttx_decoder *	td,
				 vbi_cache *		ca,
				 const vbi_network *	nk);

#endif /* TTX_DECODER_PRIV_H */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
