/*
 *  libzvbi - Closed Caption decoder
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

/* $Id: caption_decoder.h,v 1.1.2.1 2004-05-12 01:40:43 mschimek Exp $ */

#ifndef __ZVBI_CAPTION_DECODER_H__
#define __ZVBI_CAPTION_DECODER_H__

#include <inttypes.h>
#include "macros.h"
#include "network.h"
#include "event.h"

VBI_BEGIN_DECLS

typedef struct _vbi_caption_decoder vbi_caption_decoder;

extern const vbi_program_id *
vbi_caption_decoder_program_id	(vbi_caption_decoder *	cd);
extern void
vbi_caption_decoder_remove_event_handler
				(vbi_caption_decoder *	cd,
				 vbi_event_cb *		callback,
				 void *			user_data);
extern vbi_bool
vbi_caption_decoder_add_event_handler
				(vbi_caption_decoder *	cd,
				 unsigned int		event_mask,
				 vbi_event_cb *		callback,
				 void *			user_data);
extern void
vbi_caption_decoder_reset	(vbi_caption_decoder *	cd,
				 vbi_nuid		nuid);
extern vbi_bool
vbi_caption_decoder_decode	(vbi_caption_decoder *	cd,
				 const uint8_t		buffer[2],
				 double			timestamp);
extern void
vbi_caption_decoder_delete	(vbi_caption_decoder *	cd);

extern vbi_caption_decoder *
vbi_caption_decoder_new		(vbi_cache *		ca) vbi_alloc;

VBI_END_DECLS

#endif /* __ZVBI_CAPTION_DECODER_H__ */
