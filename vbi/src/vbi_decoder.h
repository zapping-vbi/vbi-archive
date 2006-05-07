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

/* $Id: vbi_decoder.h,v 1.1.2.5 2006-05-07 06:04:59 mschimek Exp $ */

#ifndef __ZVBI3_VBI3_DECODER_H__
#define __ZVBI3_VBI3_DECODER_H__

#include "macros.h"
#include "page.h"		/* vbi3_page */
#include "sliced.h"
#include "caption_decoder.h"
#include "teletext_decoder.h"

VBI3_BEGIN_DECLS

/**
 * @ingroup Service
 * @brief VBI decoder.
 *
 * The contents of this structure are private.
 * Call vbi3_decoder_new() to allocate a VBI decoder.
 */
typedef struct vbi3_decoder vbi3_decoder;

extern vbi3_page *
vbi3_decoder_get_page_va_list	(vbi3_decoder *		vbi,
				 const vbi3_network *	nk,
				 vbi3_pgno		pgno,
				 vbi3_subno		subno,
				 va_list		format_options)
  __attribute__ ((malloc,
		  _vbi3_nonnull (1)));
extern vbi3_page *
vbi3_decoder_get_page		(vbi3_decoder *		vbi,
				 const vbi3_network *	nk,
				 vbi3_pgno		pgno,
				 vbi3_subno		subno,
				 ...)
  __attribute__ ((malloc,
		  _vbi3_nonnull (1),
		  _vbi3_sentinel));
extern void
vbi3_decoder_get_program_id	(vbi3_decoder *		vbi,
				 vbi3_program_id *	pid,
				 vbi3_pid_channel	channel)
  __attribute__ ((_vbi3_nonnull (1, 2)));
#ifndef ZAPPING8
extern void
vbi3_decoder_get_aspect_ratio	(vbi3_decoder *		vbi,
				 vbi3_aspect_ratio *	ar)
  __attribute__ ((_vbi3_nonnull (1, 2)));
#endif
extern vbi3_teletext_decoder *
vbi3_decoder_cast_to_teletext_decoder
				(vbi3_decoder *		vbi)
  __attribute__ ((_vbi3_nonnull (1),
		  _vbi3_pure));
extern vbi3_caption_decoder *
vbi3_decoder_cast_to_caption_decoder
				(vbi3_decoder *		vbi)
  __attribute__ ((_vbi3_nonnull (1),
		  _vbi3_pure));
extern void
vbi3_decoder_detect_channel_change
				(vbi3_decoder *		vbi,
				 vbi3_bool		enable)
  __attribute__ ((_vbi3_nonnull (1)));
extern void
vbi3_decoder_feed		(vbi3_decoder *		vbi,
				 const vbi3_sliced *	sliced,
				 unsigned int		n_lines,
				 double			time)
  __attribute__ ((_vbi3_nonnull (1, 2)));
extern void
vbi3_decoder_reset		(vbi3_decoder *		vbi,
				 const vbi3_network *	nk,
				 vbi3_videostd_set	videostd_set)
  __attribute__ ((_vbi3_nonnull (1)));
extern void
vbi3_decoder_remove_event_handler
				(vbi3_decoder *		vbi,
				 vbi3_event_cb *	callback,
				 void *			user_data)
  __attribute__ ((_vbi3_nonnull (1)));
extern vbi3_bool
vbi3_decoder_add_event_handler	(vbi3_decoder *		vbi,
				 vbi3_event_mask	event_mask,
				 vbi3_event_cb *	callback,
				 void *			user_data)
  __attribute__ ((_vbi3_nonnull (1)));
extern void
vbi3_decoder_delete		(vbi3_decoder *		vbi);
extern vbi3_decoder *
vbi3_decoder_new		(vbi3_cache *		ca,
				 const vbi3_network *	nk,
				 vbi3_videostd_set	videostd_set)
  __attribute__ ((malloc));

VBI3_END_DECLS

#endif /* __ZVBI3_VBI3_DECODER_H__ */
