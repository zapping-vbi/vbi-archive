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

/* $Id: teletext_decoder.h,v 1.1.2.5 2004-07-09 16:10:54 mschimek Exp $ */

#ifndef __ZVBI_TELETEXT_DECODER_H__
#define __ZVBI_TELETEXT_DECODER_H__

#include <inttypes.h>		/* uint8_t */
#include "macros.h"
#include "pdc.h"		/* vbi_program_id, vbi_pid_channel */
#include "network.h"		/* vbi_network */
#include "cache.h"		/* vbi_cache */
#include "event.h"		/* vbi_event_cb */

VBI_BEGIN_DECLS

/**
 * @addtogroup Teletext
 * @{
 */

/**
 * @brief Teletext decoder.
 *
 * The contents of this structure are private.
 * Call vbi_teletext_decoder_new() to allocate a Teletext decoder.
 */
typedef struct _vbi_teletext_decoder vbi_teletext_decoder;

extern vbi_bool
vbi_teletext_decoder_get_top_title
				(vbi_teletext_decoder *	td,
				 vbi_top_title *	tt,
				 const vbi_network *	nk,
				 vbi_pgno		pgno,
				 vbi_subno		subno);
extern vbi_top_title *
vbi_teletext_decoder_get_top_titles
				(vbi_teletext_decoder *	td,
				 const vbi_network *	nk,
				 unsigned int *		array_size);
extern vbi_bool
vbi_teletext_decoder_get_ttx_page_stat
				(vbi_teletext_decoder *	td,
				 vbi_ttx_page_stat *	ps,
				 const vbi_network *	nk,
				 vbi_pgno		pgno);
extern void
vbi_teletext_decoder_get_program_id
				(vbi_teletext_decoder *	td,
				 vbi_program_id *	pid,
				 vbi_pid_channel	channel);
extern vbi_page *
vbi_teletext_decoder_get_page_va_list
				(vbi_teletext_decoder *	td,
				 const vbi_network *	nk,
				 vbi_pgno		pgno,
				 vbi_subno		subno,
				 va_list		format_options);
extern vbi_page *
vbi_teletext_decoder_get_page	(vbi_teletext_decoder *	td,
				 const vbi_network *	nk,
				 vbi_pgno		pgno,
				 vbi_subno		subno,
				 ...);
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
				 const vbi_network *	nk,
				 vbi_videostd_set	videostd_set);
extern vbi_bool
vbi_teletext_decoder_decode	(vbi_teletext_decoder *	td,
				 const uint8_t		buffer[42],
				 double			timestamp);
extern void
vbi_teletext_decoder_delete	(vbi_teletext_decoder *	td);

extern vbi_teletext_decoder *
vbi_teletext_decoder_new	(vbi_cache *		ca,
				 const vbi_network *	nk,
				 vbi_videostd_set	videostd_set)
     vbi_alloc;

/** @} */

VBI_END_DECLS

#endif /* __ZVBI_TELETEXT_DECODER_H__ */
