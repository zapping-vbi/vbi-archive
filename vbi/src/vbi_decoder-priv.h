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

/* $Id: vbi_decoder-priv.h,v 1.1.2.2 2004-07-09 16:10:54 mschimek Exp $ */

#ifndef VBI_DECODER_PRIV_H
#define VBI_DECODER_PRIV_H

#include "cc.h"
#include "event.h"
#include "trigger.h"
#include "teletext_decoder-priv.h"	/* vbi_teletext_decoder */
#include "vbi_decoder.h"

struct vbi_decoder {

	double			time;

	/** Activity monitoring. */
	double			time_teletext;
	double			time_caption;
	double			time_vps;
	double			time_wss_625;
	double			time_wss_cpr1204;



  //	pthread_mutex_t		chswcd_mutex;
  //      int                     chswcd;

  //	vbi_event		network;

  // TODO	vbi_trigger *		triggers;


	vbi_teletext_decoder	vt;
	vbi_caption_decoder	cc;



	double 			reset_time;

	void (* teletext_reset)	(vbi_teletext_decoder *	td,
				 cache_network *	cn,
				 double			time);

	void (* caption_reset)	(vbi_caption_decoder *	cd,
				 cache_network *	cn,
				 double			time);


  //	cache *		cache;

	/* preliminary */
	int			pageref;

	_vbi_event_handler_list handlers;



};

extern void
_vbi_decoder_destroy		(vbi_decoder *		vbi);
extern vbi_bool
_vbi_decoder_init		(vbi_decoder *		vbi,
				 vbi_cache *		ca,
				 const vbi_network *	nk,
				 vbi_videostd_set	videostd_set);

#endif /* VBI_DECODER_PRIV_H */
