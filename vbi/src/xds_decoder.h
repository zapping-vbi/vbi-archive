/*
 *  libzvbi - Extended Data Service decoder
 *
 *  Copyright (C) 2000-2004 Michael H. Schimek
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

/* $Id: xds_decoder.h,v 1.1.2.1 2004-04-05 04:42:27 mschimek Exp $ */

#ifndef __ZVBI_XDS_DECODER_H__
#define __ZVBI_XDS_DECODER_H__

#include <time.h>
#include <inttypes.h>
#include "macros.h"
#include "xds_demux.h"
#include "pdc.h"
#include "aspect_ratio.h"

/* Public */

VBI_BEGIN_DECLS

typedef unsigned int vbi_xds_date_flags; /* todo */

/**
 * @addtogroup XDSDecoder
 * @{
 */
extern vbi_bool
vbi_decode_xds_aspect_ratio	(vbi_aspect_ratio *	ar,
				 const vbi_xds_packet *	xp);
extern vbi_bool
vbi_decode_xds_tape_delay	(unsigned int *		tape_delay,
				 const vbi_xds_packet *	xp);
extern vbi_bool
vbi_decode_xds_time_of_day	(struct tm *		tm,
				 vbi_xds_date_flags *	date_flags,
				 const vbi_xds_packet *	xp);
extern vbi_bool
vbi_decode_xds_time_zone	(unsigned int *		hours_west,
				 vbi_bool *		dso,
				 const vbi_xds_packet *	xp);
extern vbi_bool
vbi_decode_xds_impulse_capture_id
				(vbi_program_id *	pid,
				 const vbi_xds_packet *	xp);
/** @} */

void
_vbi_decode_xds (vbi_xds_demux *xd, void *user_data, const vbi_xds_packet *xp);

VBI_END_DECLS

/* Private */

#endif /* __ZVBI_XDS_DECODER_H__ */
