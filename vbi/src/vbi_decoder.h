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

/* $Id: vbi_decoder.h,v 1.1.2.4 2004-07-09 16:10:54 mschimek Exp $ */

#ifndef __ZVBI_VBI_DECODER_H__
#define __ZVBI_VBI_DECODER_H__

#include "macros.h"
#include "format.h"		/* vbi_page */
#include "caption_decoder.h"
#include "teletext_decoder.h"

VBI_BEGIN_DECLS

/**
 * @ingroup Service
 * @brief VBI decoder.
 *
 * The contents of this structure are private.
 * Call vbi_decoder_new() to allocate a VBI decoder.
 */
typedef struct vbi_decoder vbi_decoder;

extern vbi_top_title *
vbi_decoder_get_top_titles	(vbi_decoder *		vbi,
				 const vbi_network *	nk,
				 unsigned int *		array_size);
extern vbi_bool
vbi_decoder_get_top_title	(vbi_decoder *		vbi,
				 vbi_top_title *	tt,
				 const vbi_network *	nk,
				 vbi_pgno		pgno,
				 vbi_subno		subno);
extern vbi_bool
vbi_decoder_get_ttx_page_stat	(vbi_decoder *		vbi,
				 vbi_ttx_page_stat *	ps,
				 const vbi_network *	nk,
				 vbi_pgno		pgno);
extern vbi_page *
vbi_decoder_get_teletext_page_va_list
				(vbi_decoder *		vbi,
				 const vbi_network *	nk,
				 vbi_pgno		pgno,
				 vbi_subno		subno,
				 va_list		format_options);
extern vbi_page *
vbi_decoder_get_teletext_page	(vbi_decoder *		vbi,
				 const vbi_network *	nk,
				 vbi_pgno		pgno,
				 vbi_subno		subno,
				 ...);
extern void
vbi_decoder_get_program_id	(vbi_decoder *		vbi,
				 vbi_program_id *	pid,
				 vbi_pid_channel	channel);
extern void
vbi_decoder_get_aspect_ratio	(vbi_decoder *		vbi,
				 vbi_aspect_ratio *	ar);
extern void
vbi_decoder_decode		(vbi_decoder *		vbi,
				 vbi_sliced *		sliced,
				 unsigned int		sliced_size,
				 double			time);
extern void
vbi_decoder_reset		(vbi_decoder *		vbi,
				 const vbi_network *	nk,
				 vbi_videostd_set	videostd_set);
extern void
vbi_decoder_remove_event_handler
				(vbi_decoder *		vbi,
				 vbi_event_cb *		callback,
				 void *			user_data);
extern vbi_bool
vbi_decoder_add_event_handler	(vbi_decoder *		vbi,
				 unsigned int		event_mask,
				 vbi_event_cb *		callback,
				 void *			user_data);
extern void
vbi_decoder_delete		(vbi_decoder *		vbi);
extern vbi_decoder *
vbi_decoder_new			(vbi_cache *		ca,
				 const vbi_network *	nk,
				 vbi_videostd_set	videostd_set)
     vbi_alloc;

VBI_END_DECLS

#endif /* __ZVBI_VBI_DECODER_H__ */
