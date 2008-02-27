/*
 *  libzvbi -- DVB VBI demultiplexer
 *
 *  Copyright (C) 2004, 2007 Michael H. Schimek
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the 
 *  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, 
 *  Boston, MA  02110-1301  USA.
 */

/* $Id: dvb_demux.h,v 1.3.2.6 2008-02-27 07:57:47 mschimek Exp $ */

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

/* Experimental. */
vbi3_bool
_vbi3_dvb_skip_data_unit		(const uint8_t **	buffer,
				 unsigned int *		buffer_left)
  _vbi3_nonnull ((1, 2));
/* Experimental. */
vbi3_bool
_vbi3_dvb_demultiplex_sliced	(vbi3_sliced *		sliced,
				 unsigned int * 	n_lines,
				 unsigned int		max_lines,
				 const uint8_t **	buffer,
				 unsigned int *		buffer_left)
  _vbi3_nonnull ((1, 2, 4, 5));
/* Experimental. */
extern vbi3_dvb_demux *
_vbi3_dvb_ts_demux_new		(vbi3_dvb_demux_cb *	callback,
				 void *			user_data,
				 unsigned int		pid);

VBI3_END_DECLS

#endif /* __ZVBI3_DVB_DEMUX_H__ */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
