/*
 *  libzvbi
 *
 *  Copyright (C) 2004 Michael H. Schimek
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
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

/* $Id: dvb_demux.h,v 1.3.2.2 2006-05-07 06:04:58 mschimek Exp $ */

#ifndef __ZVBI3_DVB_DEMUX_H__
#define __ZVBI3_DVB_DEMUX_H__

#include <inttypes.h>		/* uintN_t */
#include "macros.h"
#include "sliced.h"		/* vbi3_sliced, vbi3_service_set */
#include "sampling_par.h"	/* vbi3_videostd_set */

VBI3_BEGIN_DECLS

/**
 * @addtogroup DVBDemux
 * @{
 */

/**
 * @brief DVB VBI demultiplexer context.
 *
 * The contents of this structure are private.
 *
 * Call vbi3_dvb_demux_new() to allocate
 * a DVB VBI multiplexer context.
 */
typedef struct _vbi3_dvb_demux vbi3_dvb_demux;

typedef vbi3_bool
vbi3_dvb_demux_cb		(vbi3_dvb_demux *	dx,
				 void *			user_data,
				 const vbi3_sliced *	sliced,
				 unsigned int		sliced_lines,
				 int64_t		pts);

extern void
_vbi3_dvb_demux_reset		(vbi3_dvb_demux *	dx);
extern unsigned int
_vbi3_dvb_demux_cor		(vbi3_dvb_demux *	dx,
				 vbi3_sliced *		sliced,
				 unsigned int 		sliced_lines,
				 int64_t *		pts,
				 const uint8_t **	buffer,
				 unsigned int *		buffer_left);
extern vbi3_bool
_vbi3_dvb_demux_feed		(vbi3_dvb_demux *	dx,
				 const uint8_t *	buffer,
				 unsigned int		buffer_size);
extern void
_vbi3_dvb_demux_delete		(vbi3_dvb_demux *	dx);
extern vbi3_dvb_demux *
_vbi3_dvb_pes_demux_new		(vbi3_dvb_demux_cb *	callback,
				 void *			user_data)
  __attribute__ ((_vbi3_alloc));

/** @} */

VBI3_END_DECLS

#endif /* __ZVBI3_DVB_DEMUX_H__ */
