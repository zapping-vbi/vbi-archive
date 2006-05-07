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

/* $Id: caption_decoder.h,v 1.1.2.3 2006-05-07 06:04:58 mschimek Exp $ */

#ifndef __ZVBI3_CAPTION_DECODER_H__
#define __ZVBI3_CAPTION_DECODER_H__

#include <inttypes.h>		/* uint8_t */
#include <stdarg.h>		/* va_list */

#include "macros.h"
#include "bcd.h"		/* vbi3_pgno */
#include "page.h"		/* vbi3_page */
#include "cache.h"		/* vbi3_cache */
#include "network.h"		/* vbi3_network */
#include "event.h"		/* vbi3_event_cb */
#include "sampling_par.h"	/* vbi3_videostd_set */

VBI3_BEGIN_DECLS

/**
 * @addtogroup Caption
 * @{
 */

typedef enum {
	VBI3_CAPTION_MODE_UNKNOWN,
	VBI3_CAPTION_MODE_POP_ON,
	VBI3_CAPTION_MODE_PAINT_ON,
	VBI3_CAPTION_MODE_ROLL_UP,
	VBI3_CAPTION_MODE_TEXT
} vbi3_caption_mode;

/**
 * @brief Meta data and statistical info about a Closed Caption channels.
 */
typedef struct {
	vbi3_pgno		channel;

	/** VBI3_SUBTITLE_PAGE or VBI3_NORMAL_PAGE. */
	vbi3_page_type		page_type;

	vbi3_caption_mode	caption_mode;

	/**
	 * Language, if a subtitle page. This is a ISO 639 two-character 
	 * language code string, for example "fr" for French. Can be
	 * NULL if unknown.
	 */
	const char *		language_code;

	double			last_received;

	void *			reserved1[2];
	unsigned int		reserved2[2];
} vbi3_cc_channel_stat;

extern void
vbi3_cc_channel_stat_destroy	(vbi3_cc_channel_stat *	cs)
  __attribute__ ((_vbi3_nonnull (1)));
extern void
vbi3_cc_channel_stat_init	(vbi3_cc_channel_stat *	cs)
  __attribute__ ((_vbi3_nonnull (1)));

/**
 * @brief Caption decoder.
 *
 * The contents of this structure are private.
 * Call vbi3_caption_decoder_new() to allocate a Caption decoder.
 */
typedef struct _vbi3_caption_decoder vbi3_caption_decoder;

extern vbi3_bool
vbi3_caption_decoder_get_cc_channel_stat
				(vbi3_caption_decoder *	cd,
				 vbi3_cc_channel_stat *	cs,
				 vbi3_pgno		channel)
  __attribute__ ((_vbi3_nonnull (1, 2)));
extern vbi3_page *
vbi3_caption_decoder_get_page_va_list
				(vbi3_caption_decoder *	cd,
				 vbi3_pgno		channel,
				 va_list		format_options)
  __attribute__ ((malloc,
		  _vbi3_nonnull (1)));
extern vbi3_page *
vbi3_caption_decoder_get_page	(vbi3_caption_decoder *	cd,
				 vbi3_pgno		channel,
				 ...)
  __attribute__ ((malloc,
		  _vbi3_nonnull (1),
		  _vbi3_sentinel));
extern vbi3_bool
vbi3_caption_decoder_feed	(vbi3_caption_decoder *	cd,
				 const uint8_t		buffer[2],
				 unsigned int		line,
				 double			timestamp)
  __attribute__ ((_vbi3_nonnull (1, 2)));
extern void
vbi3_caption_decoder_remove_event_handler
				(vbi3_caption_decoder *	cd,
				 vbi3_event_cb *	callback,
				 void *			user_data)
  __attribute__ ((_vbi3_nonnull (1)));
extern vbi3_bool
vbi3_caption_decoder_add_event_handler
				(vbi3_caption_decoder *	cd,
				 unsigned int		event_mask,
				 vbi3_event_cb *	callback,
				 void *			user_data)
  __attribute__ ((_vbi3_nonnull (1)));
extern vbi3_bool
vbi3_caption_decoder_get_network
				(vbi3_caption_decoder *	cd,
				 vbi3_network *		nk)
  __attribute__ ((_vbi3_nonnull (1, 2)));
extern vbi3_cache *
vbi3_caption_decoder_get_cache	(vbi3_caption_decoder *	cd)
  __attribute__ ((_vbi3_nonnull (1)));
extern void
vbi3_caption_decoder_reset	(vbi3_caption_decoder *	cd,
				 const vbi3_network *	nk,
				 vbi3_videostd_set	videostd_set);
extern void
vbi3_caption_decoder_delete	(vbi3_caption_decoder *	cd);
extern vbi3_caption_decoder *
vbi3_caption_decoder_new	(vbi3_cache *		ca,
				 const vbi3_network *	nk,
				 vbi3_videostd_set	videostd_set)
  __attribute__ ((malloc));

/** @} */

VBI3_END_DECLS

#endif /* __ZVBI3_CAPTION_DECODER_H__ */
