/*
 *  libzvbi - Teletext decoder
 *
 *  Copyright (C) 2000-2004 Michael H. Schimek
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

/* $Id: teletext_decoder.h,v 1.1.2.3 2004-05-01 13:51:35 mschimek Exp $ */

#ifndef __ZVBI_TELETEXT_DECODER_H__
#define __ZVBI_TELETEXT_DECODER_H__

#include <inttypes.h>		/* uint8_t */
#include "macros.h"
#include "pdc.h"		/* vbi_program_id, vbi_pid_channel */
#include "network.h"		/* vbi_nuid */
#include "cache.h"		/* vbi_cache */
#include "event.h"		/* vbi_event_cb */

VBI_BEGIN_DECLS

/**
 * @ingroup Service
 * @brief Page classification.
 *
 * See vbi_classify_page().
 */
typedef enum {
	VBI_NO_PAGE		= 0x00,
	VBI_NORMAL_PAGE		= 0x01,

	VBI_TOP_BLOCK		= 0x60,		/* libzvbi private */
	VBI_TOP_GROUP		= 0x61,		/* libzvbi private */

	VBI_SUBTITLE_PAGE	= 0x70,
	VBI_SUBTITLE_INDEX	= 0x78,
	VBI_NONSTD_SUBPAGES	= 0x79,
	VBI_PROGR_WARNING	= 0x7A,
	VBI_CURRENT_PROGR	= 0x7C,
	VBI_NOW_AND_NEXT	= 0x7D,
	VBI_PROGR_INDEX		= 0x7F,
	VBI_NOT_PUBLIC		= 0x80,
	VBI_PROGR_SCHEDULE	= 0x81,
	VBI_CA_DATA		= 0xE0,
	VBI_PFC_EPG_DATA	= 0xE3,
	VBI_PFC_DATA		= 0xE4,
	VBI_DRCS_PAGE		= 0xE5,
	VBI_POP_PAGE		= 0xE6,
	VBI_SYSTEM_PAGE		= 0xE7,
	VBI_KEYWORD_SEARCH_LIST = 0xF9,
	VBI_TRIGGER_DATA	= 0xFC,
	VBI_ACI_PAGE		= 0xFD,
	VBI_TOP_PAGE		= 0xFE,		/* MPT, AIT, MPT-EX */

	VBI_UNKNOWN_PAGE	= 0xFF,		/* libzvbi private */
} vbi_page_type;

typedef struct _vbi_top_title vbi_top_title;

struct _vbi_top_title {
	vbi_pgno		pgno;
	vbi_subno		subno;
	char *			title;
	vbi_top_title *		submenu;
};

void
vbi_top_title_delete_vec	(vbi_top_title *	tt);

typedef struct _vbi_teletext_decoder vbi_teletext_decoder;

extern const vbi_program_id *
vbi_teletext_decoder_program_id	(vbi_teletext_decoder *	td,
				 vbi_pid_channel	channel);
extern void
vbi_teletext_decoder_remove_event_handler
				(vbi_teletext_decoder *	td,
				 vbi_event_cb *		callback,
				 void *			user_data);
extern vbi_bool
vbi_teletext_decoder_add_event_handler
				(vbi_teletext_decoder *	td,
				 unsigned int		event_mask,
				 vbi_event_cb *		callback,
				 void *			user_data);
extern void
vbi_teletext_decoder_reset	(vbi_teletext_decoder *	td,
				 vbi_nuid		nuid);
extern vbi_bool
vbi_teletext_decoder_decode	(vbi_teletext_decoder *	td,
				 const uint8_t		buffer[42],
				 double			timestamp);
extern void
vbi_teletext_decoder_delete	(vbi_teletext_decoder *	td);

extern vbi_teletext_decoder *
vbi_teletext_decoder_new	(vbi_cache *		ca) vbi_alloc;

VBI_END_DECLS

#endif /* __ZVBI_TELETEXT_DECODER_H__ */
