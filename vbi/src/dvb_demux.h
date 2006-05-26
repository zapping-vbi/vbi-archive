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

/* $Id: dvb_demux.h,v 1.3.2.3 2006-05-26 00:43:05 mschimek Exp $ */

#ifndef __ZVBI3_DVB_DEMUX_H__
#define __ZVBI3_DVB_DEMUX_H__

#include <inttypes.h>		/* uintN_t */
#include "bcd.h"		/* vbi3_bool */
#include "sliced.h"		/* vbi3_sliced, vbi3_service_set */

VBI3_BEGIN_DECLS

/* Public */

/**
 * @addtogroup DVBDemux
 * @{
 */

/**
 * @brief DVB VBI demultiplexer.
 *
 * The contents of this structure are private.
 * Call vbi3_dvb_pes_demux_new() to allocate a DVB demultiplexer.
 */
typedef struct _vbi3_dvb_demux vbi3_dvb_demux;

/**
 * @param dx DVB demultiplexer context allocated with vbi3_dvb_pes_demux_new().
 * @param user_data User data pointer given to vbi3_dvb_pes_demux_new().
 * @param sliced Pointer to demultiplexed sliced data.
 * @param sliced_lines Number of lines in the @a sliced array.
 * @param pts Presentation Time Stamp associated with the first sliced
 *   line.
 *
 * The vbi3_dvb_demux_feed() function calls a function of this type when
 * a new frame of sliced data is complete.
 */
typedef vbi3_bool
vbi3_dvb_demux_cb		(vbi3_dvb_demux *	dx,
				 void *			user_data,
				 const vbi3_sliced *	sliced,
				 unsigned int		sliced_lines,
				 int64_t		pts);

extern void
vbi3_dvb_demux_reset		(vbi3_dvb_demux *	dx);
extern unsigned int
vbi3_dvb_demux_cor		(vbi3_dvb_demux *	dx,
				 vbi3_sliced *		sliced,
				 unsigned int 		sliced_lines,
				 int64_t *		pts,
				 const uint8_t **	buffer,
				 unsigned int *		buffer_left);
extern vbi3_bool
vbi3_dvb_demux_feed		(vbi3_dvb_demux *	dx,
				 const uint8_t *	buffer,
				 unsigned int		buffer_size);
extern void
vbi3_dvb_demux_set_log_fn	(vbi3_dvb_demux *	dx,
				 vbi3_log_mask		mask,
				 vbi3_log_fn *		log_fn,
				 void *			user_data);
extern void
vbi3_dvb_demux_delete		(vbi3_dvb_demux *	dx);
extern vbi3_dvb_demux *
vbi3_dvb_pes_demux_new		(vbi3_dvb_demux_cb *	callback,
				 void *			user_data);

/** @} */

/* Private */

VBI3_END_DECLS

#endif /* __ZVBI3_DVB_DEMUX_H__ */
