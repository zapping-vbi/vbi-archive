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

/* $Id: vbi_decoder-priv.h,v 1.1.2.3 2006-05-07 06:04:59 mschimek Exp $ */

#ifndef VBI3_DECODER_PRIV_H
#define VBI3_DECODER_PRIV_H

#include "caption_decoder-priv.h"
#include "teletext_decoder-priv.h"	/* vbi3_teletext_decoder */
#include "event.h"
#ifndef ZAPPING8
#  include "trigger.h"
#endif
#include "vbi_decoder.h"

struct vbi3_decoder {
	/** Activity monitoring. */
	double			timestamp_teletext;
	double			timestamp_caption;
	double			timestamp_vps;
	double			timestamp_wss_625;
	double			timestamp_wss_cpr1204;

#ifndef ZAPPING8
	vbi3_aspect_ratio	confirm_aspect_ratio;
#endif
#if 0 /* TODO */
	vbi3_trigger *		triggers;
#endif

	vbi3_bool		dcc;

	/**
	 * Remember past transmission errors: One bit for each call of
	 * the decoder, most recent result in lsb.
	 */
	unsigned int		error_history_vps;
	unsigned int		error_history_wss_625;
	unsigned int		error_history_wss_cpr1204;

	vbi3_teletext_decoder	vt;
	vbi3_caption_decoder	cc;

	double			timestamp;
	double 			reset_time;

	teletext_reset_fn *	teletext_reset;
	caption_reset_fn *	caption_reset;

	_vbi3_event_handler_list handlers;
};

extern void
_vbi3_decoder_destroy		(vbi3_decoder *		vbi);
extern vbi3_bool
_vbi3_decoder_init		(vbi3_decoder *		vbi,
				 vbi3_cache *		ca,
				 const vbi3_network *	nk,
				 vbi3_videostd_set	videostd_set);

#endif /* VBI3_DECODER_PRIV_H */
