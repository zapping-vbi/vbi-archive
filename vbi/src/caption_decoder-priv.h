/*
 *  libzvbi - Caption decoder
 *
 *  Copyright (C) 2000, 2001, 2002, 2005 Michael H. Schimek
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

/* $Id: caption_decoder-priv.h,v 1.1.2.3 2006-05-26 00:43:05 mschimek Exp $ */

#ifndef CAPTION_DECODER_PRIV_H
#define CAPTION_DECODER_PRIV_H

#include "cache-priv.h"
#include "event-priv.h"
#include "xds_demux.h"
#include "caption_decoder.h"

typedef enum {
	FIELD_1 = 0,
	FIELD_2,
	N_FIELDS
} field_num;

#define FIELD_UNKNOWN MAX_FIELDS

/* Closed Caption definitions. */

#define CHANNEL_UNKNOWN 0
#define MAX_CHANNELS 8

/* Sec. 15.119 (d) Screen format. */

#define FIRST_ROW		0
#define LAST_ROW		14
#define MAX_ROWS		(LAST_ROW + 1)

#define FIRST_COLUMN		0
#define LAST_COLUMN		31
#define MAX_COLUMNS		(LAST_COLUMN + 1)

/** This structure maintains Closed Caption channel state. */
typedef struct {
	/**
	 * Displayed and non-displayed buffer according to spec.
	 * Snapshot of displayed buffer at last row update.
	 */
	vbi3_char		buffer[3][MAX_ROWS][MAX_COLUMNS];

	/**
	 * For all buffers, if bit (1 << row) is set this row contains
	 * text, otherwise only TRANSPARENT_SPACEs. Intended to speed
	 * up copying and conversion of characters.
	 *
	 * When dirty[n] < 0 this buffer shall be erased to all
	 * TRANSPARENT_SPACEs before storing characters. Intended to
	 * avoid copying and erasing buffers which are never used.
	 */ 
	int			dirty[3];

	/** Index of displayed buffer, 0 or 1. */
	unsigned int		displayed_buffer;

	/** Cursor position. */
	unsigned int		curr_row;
	unsigned int		curr_column;

	/**
	 * Text window height in VBI3_CAPTION_MODE_ROLL_UP
	 * (ranges from curr_row - window_rows + 1 to curr_row inclusive).
	 *
	 * NOTE curr_row - window_rows + 1 can be < FIRST_ROW, this must
	 * be clipped before using window_rows:
	 * actual_rows = MIN (curr_row - FIRST_ROW + 1, window_rows);
	 *
	 * We won't do that at a Roll-Up Captions command because usually
	 * a Preamble Address Code follows which may change curr_row.
	 */
	unsigned int		window_rows;

	/**
	 * Current character attributes (foreground and background color,
	 * underlined, flash, italic).
	 */
	vbi3_char		curr_attr;

	/** Current caption mode or VBI3_CAPTION_MODE_UNKNOWN. */
	vbi3_caption_mode	mode;

	/** Last time when text was transmitted on this channel. */
	double			last_timestamp;
} caption_channel;

/* General stuff. */

typedef void
caption_reset_fn		(vbi3_caption_decoder *	cd,
				 cache_network *	cn,
				 double			time);
typedef void
caption_delete_fn		(vbi3_caption_decoder *	cd);

/** @internal */
struct _vbi3_caption_decoder {
	struct {
		/* Closed Caption decoder */

		/**
		 * Decoder state. We decode all channels in parallel, this
		 * way clients can switch between channels without data
		 * loss, or capture multiple channels with a single decoder
		 * instance.
		 */
		caption_channel		channel[MAX_CHANNELS];

		/**
		 * Current channel, switched by caption control codes. Can
		 * be @c CHANNEL_UNKNOWN (no channel number received yet).
		 */
		vbi3_pgno		curr_ch_num;

		/**
		 * To send a display update event (VBI3_EVENT_CC_PAGE) when the
		 * visible buffer of the current channel changed, but no more
		 * than once for each pair of Closed Caption bytes.
		 */
		caption_channel *	event_pending;
	}			cc;

	struct {
		/* ITV decoder */

		/** Incoming data. */
		uint8_t			buffer[256];

		/** Number of bytes in buffer. */
		unsigned int		size;
	}			itv;

	struct {
		/* XDS decoder */

		vbi3_xds_demux		demux;
	}			xds;

	/* Master demultiplexer */

	/** Receiving ITV data (channel VBI3_CHAPTION_T2). */
	vbi3_bool		in_itv[N_FIELDS];

	/** Receiving XDS data, as opposed to caption. */
	vbi3_bool		in_xds[N_FIELDS];

	/**
	 * Caption control codes (two bytes) may repeat once for
	 * error correction.
	 */
	int			expect_ctrl[N_FIELDS][2];

	/**
	 * Remember past parity errors: One bit for each call of
	 * vbi3_caption_decoder_feed(), most recent result in lsb. The idea
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

	/** Called by vbi3_caption_decoder_reset(). */
	caption_reset_fn *	virtual_reset;

	_vbi3_event_handler_list handlers;

	/** Called by vbi3_caption_decoder_delete(). */
	caption_delete_fn *	virtual_delete;
};

extern void
_vbi3_caption_decoder_resync	(vbi3_caption_decoder *	cd);
extern void
_vbi3_caption_decoder_destroy	(vbi3_caption_decoder *	cd);
extern vbi3_bool
_vbi3_caption_decoder_init	(vbi3_caption_decoder *	cd,
				 vbi3_cache *		ca,
				 const vbi3_network *	nk,
				 vbi3_videostd_set	videostd_set);

#endif /* CAPTION_DECODER_PRIV_H */
