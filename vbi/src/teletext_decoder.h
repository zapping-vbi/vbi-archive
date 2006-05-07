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

/* $Id: teletext_decoder.h,v 1.1.2.6 2006-05-07 06:04:59 mschimek Exp $ */

#ifndef __ZVBI3_TELETEXT_DECODER_H__
#define __ZVBI3_TELETEXT_DECODER_H__

#include <inttypes.h>		/* uint8_t */

#include "macros.h"
#include "pdc.h"		/* vbi3_program_id, vbi3_pid_channel */
#include "network.h"		/* vbi3_network */
#include "cache.h"		/* vbi3_cache */
#include "event.h"		/* vbi3_event_cb */
#include "sampling_par.h"	/* vbi3_videostd_set */
#include "search.h"		/* vbi3_search */

VBI3_BEGIN_DECLS

/**
 * @addtogroup Teletext
 * @{
 */

/**
 * @brief Teletext decoder.
 *
 * The contents of this structure are private.
 * Call vbi3_teletext_decoder_new() to allocate a Teletext decoder.
 */
typedef struct _vbi3_teletext_decoder vbi3_teletext_decoder;

extern vbi3_search *
vbi3_teletext_decoder_search_utf8_new
				(vbi3_teletext_decoder *td,
				 const vbi3_network *	nk,
				 vbi3_pgno		pgno,
				 vbi3_subno		subno,
				 const char *		pattern,
				 vbi3_bool		casefold,
				 vbi3_bool		regexp,
				 vbi3_search_progress_cb *progress,
				 void *			user_data)
  __attribute__ ((malloc,
		  _vbi3_nonnull (1, 5)));
extern vbi3_bool
vbi3_teletext_decoder_get_top_title
				(vbi3_teletext_decoder *	td,
				 vbi3_top_title *	tt,
				 const vbi3_network *	nk,
				 vbi3_pgno		pgno,
				 vbi3_subno		subno)
  __attribute__ ((_vbi3_nonnull (1, 2)));
extern vbi3_top_title *
vbi3_teletext_decoder_get_top_titles
				(vbi3_teletext_decoder *	td,
				 const vbi3_network *	nk,
				 unsigned int *		n_elements)
  __attribute__ ((malloc,
		  _vbi3_nonnull (1, 3)));
extern vbi3_bool
vbi3_teletext_decoder_get_ttx_page_stat
				(vbi3_teletext_decoder *	td,
				 vbi3_ttx_page_stat *	ps,
				 const vbi3_network *	nk,
				 vbi3_pgno		pgno)
  __attribute__ ((_vbi3_nonnull (1, 2)));
extern void
vbi3_teletext_decoder_get_program_id
				(vbi3_teletext_decoder *	td,
				 vbi3_program_id *	pid,
				 vbi3_pid_channel	channel)
  __attribute__ ((_vbi3_nonnull (1, 2)));
extern vbi3_page *
vbi3_teletext_decoder_get_page_va_list
				(vbi3_teletext_decoder *	td,
				 const vbi3_network *	nk,
				 vbi3_pgno		pgno,
				 vbi3_subno		subno,
				 va_list		format_options)
  __attribute__ ((malloc,
		  _vbi3_nonnull (1)));
extern vbi3_page *
vbi3_teletext_decoder_get_page	(vbi3_teletext_decoder *	td,
				 const vbi3_network *	nk,
				 vbi3_pgno		pgno,
				 vbi3_subno		subno,
				 ...)
  __attribute__ ((malloc,
		  _vbi3_nonnull (1),
		  _vbi3_sentinel));
extern vbi3_bool
vbi3_teletext_decoder_get_network (vbi3_teletext_decoder *td,
				  vbi3_network *		nk)
  __attribute__ ((_vbi3_nonnull (1, 2)));
extern vbi3_cache *
vbi3_teletext_decoder_get_cache	(vbi3_teletext_decoder *	td)
  __attribute__ ((_vbi3_nonnull (1)));
extern void
vbi3_teletext_decoder_remove_event_handler
				(vbi3_teletext_decoder *	td,
				 vbi3_event_cb *		callback,
				 void *			user_data)
  __attribute__ ((_vbi3_nonnull (1)));
extern vbi3_bool
vbi3_teletext_decoder_add_event_handler
				(vbi3_teletext_decoder *	td,
				 vbi3_event_mask	event_mask,
				 vbi3_event_cb *		callback,
				 void *			user_data)
  __attribute__ ((_vbi3_nonnull (1)));
extern void
vbi3_teletext_decoder_reset	(vbi3_teletext_decoder *	td,
				 const vbi3_network *	nk,
				 vbi3_videostd_set	videostd_set)
  __attribute__ ((_vbi3_nonnull (1)));
extern vbi3_bool
vbi3_teletext_decoder_feed	(vbi3_teletext_decoder *	td,
				 const uint8_t		buffer[42],
				 double			timestamp)
  __attribute__ ((_vbi3_nonnull (1, 2)));
extern void
vbi3_teletext_decoder_delete	(vbi3_teletext_decoder *	td);

extern vbi3_teletext_decoder *
vbi3_teletext_decoder_new	(vbi3_cache *		ca,
				 const vbi3_network *	nk,
				 vbi3_videostd_set	videostd_set)
  __attribute__ ((malloc));

/** @} */

VBI3_END_DECLS

#endif /* __ZVBI3_TELETEXT_DECODER_H__ */
