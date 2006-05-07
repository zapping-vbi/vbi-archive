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

/* $Id: dvb_mux.h,v 1.2.2.2 2006-05-07 06:04:58 mschimek Exp $ */

#ifndef __ZVBI3_DVB_MUX_H__
#define __ZVBI3_DVB_MUX_H__

#include <inttypes.h>		/* uint8_t */
#include "macros.h"
#include "sliced.h"		/* vbi3_sliced, vbi3_service_set */
#include "sampling_par.h"	/* vbi3_videostd_set */

VBI3_BEGIN_DECLS

/**
 * @addtogroup DVBMux
 * @{
 */

extern void
_vbi3_dvb_multiplex_sliced	(uint8_t **		packet,
				 unsigned int *		packet_left,
				 const vbi3_sliced **	sliced,
				 unsigned int *		sliced_left,
				 unsigned int		data_identifier,
				 vbi3_service_set	service_set);
extern void
_vbi3_dvb_multiplex_samples	(uint8_t **		packet,
				 unsigned int *		packet_left,
				 const uint8_t **	samples,
				 unsigned int *		samples_left,
				 unsigned int		samples_size,
				 unsigned int		data_identifier,
				 vbi3_videostd_set	videostd_set,
				 unsigned int		line,
				 unsigned int		offset);

/**
 * @brief DVB VBI multiplexer context.
 *
 * The contents of this structure are private.
 *
 * Call vbi3_dvb_mux_pes_new() or vbi3_dvb_mux_ts_new() to allocate
 * a DVB VBI multiplexer context.
 */
typedef struct _vbi3_dvb_mux vbi3_dvb_mux;

typedef vbi3_bool
vbi3_dvb_mux_cb			(vbi3_dvb_mux *		mx,
				 void *			user_data,
				 const uint8_t *	packet,
				 unsigned int		packet_size);

extern void
_vbi3_dvb_mux_reset		(vbi3_dvb_mux *		mx);
extern vbi3_bool
_vbi3_dvb_mux_mux		(vbi3_dvb_mux *		mx,
				 const vbi3_sliced *	sliced,
				 unsigned int		sliced_size,
				 vbi3_service_set	service_set,
				 int64_t		pts);
extern void
_vbi3_dvb_mux_delete		(vbi3_dvb_mux *		mx);
extern vbi3_dvb_mux *
_vbi3_dvb_mux_pes_new		(unsigned int		data_identifier,
				 unsigned int		packet_size,
				 vbi3_videostd_set	videostd_set,
				 vbi3_dvb_mux_cb *	callback,
				 void *			user_data)
  __attribute ((_vbi3_alloc));
extern vbi3_dvb_mux *
_vbi3_dvb_mux_ts_new		(unsigned int		pid,
				 unsigned int		data_identifier,
				 unsigned int		packet_size,
				 vbi3_videostd_set	videostd_set,
				 vbi3_dvb_mux_cb *	callback,
				 void *			user_data)
  __attribute__ ((_vbi3_alloc));

/** @} */

VBI3_END_DECLS

#endif /* __ZVBI3_DVB_MUX_H__ */
