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

/* $Id: xds_decoder.h,v 1.1.2.4 2007-11-01 00:21:25 mschimek Exp $ */

#ifndef __ZVBI3_XDS_DECODER_H__
#define __ZVBI3_XDS_DECODER_H__

#include <time.h>
#include <inttypes.h>
#include "macros.h"
#include "xds_demux.h"
#include "pdc.h"
#include "aspect_ratio.h"

VBI3_BEGIN_DECLS

typedef unsigned int vbi3_xds_date_flags; /* todo */

/**
 * @addtogroup XDSDecoder
 * @{
 */
extern vbi3_bool
vbi3_decode_xds_aspect_ratio	(vbi3_aspect_ratio *	ar,
				 const vbi3_xds_packet *	xp);
extern vbi3_bool
vbi3_decode_xds_tape_delay	(unsigned int *		tape_delay,
				 const vbi3_xds_packet *	xp);
extern vbi3_bool
vbi3_decode_xds_time_of_day	(struct tm *		tm,
				 vbi3_xds_date_flags *	date_flags,
				 const vbi3_xds_packet *	xp);
extern vbi3_bool
vbi3_decode_xds_time_zone	(unsigned int *		hours_west,
				 vbi3_bool *		dso,
				 const vbi3_xds_packet *	xp);
extern vbi3_bool
vbi3_decode_xds_impulse_capture_id
				(vbi3_program_id *	pid,
				 const vbi3_xds_packet *	xp);
/** @} */

void
_vbi3_decode_xds (vbi3_xds_demux *xd, void *user_data, const vbi3_xds_packet *xp);

VBI3_END_DECLS

#endif /* __ZVBI3_XDS_DECODER_H__ */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
