/*
 *  libzvbi - Teletext decoder
 *
 *  Copyright (C) 2000, 2001 Michael H. Schimek
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

/* $Id: teletext_decoder-priv.h,v 1.1.2.1 2004-07-10 11:02:33 mschimek Exp $ */

#ifndef TELETEXT_DECODER_PRIV_H
#define TELETEXT_DECODER_PRIV_H

#include "cache-priv.h"		/* vbi_cache, vt.h */
#include "pfc_demux.h"		/* vbi_pfc_demux */
#include "trigger.h"		/* _vbi_trigger */
#include "teletext_decoder.h"

struct _vbi_teletext_decoder {
	/** The cache we use. */
	vbi_cache *		cache;

	/** Current network in the cache. */
	cache_network *		network;

	/**
	 * Pages in progress, per magazine in case of parallel transmission.
	 * When complete we copy the page into the cache.
	 */
	cache_page		buffer[8];
	cache_page *		current;

	/** Page Function Clear EPG Data. */
	vbi_pfc_demux		epg_stream[2];

	/** EACEM triggers. */
	_vbi_trigger *		triggers;

	/** Used for channel switch detection. */
	pagenum			header_page;
	uint8_t			header[40];

	double			time;
	double			reset_time;

	_vbi_event_handler_list handlers;

	void (* virtual_reset)	(vbi_teletext_decoder *	td,
				 cache_network *	cn,
				 double			time);

	/** Time to receive a packet 8/30 CNI or VPS CNI. */
	double			cni_830_timeout;
	double			cni_vps_timeout;

	vbi_videostd_set	videostd_set;
};

/* in packet.c */
extern cache_page *
_vbi_convert_cached_page	(cache_page *		cp,
				 page_function		new_function);
extern void
_vbi_teletext_decoder_resync	(vbi_teletext_decoder *	td);
extern void
_vbi_teletext_decoder_destroy	(vbi_teletext_decoder *	td);
extern vbi_bool
_vbi_teletext_decoder_init	(vbi_teletext_decoder *	td,
				 vbi_cache *		ca,
				 const vbi_network *	nk,
				 vbi_videostd_set	videostd_set);

#endif /* TELETEXT_DECODER_PRIV_H */
