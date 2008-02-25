/*
 *  libzvbi - DVB VBI multiplexer
 *
 *  Copyright (C) 2004, 2007 Michael H. Schimek
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

/* $Id: dvb_mux.h,v 1.2.2.4 2008-02-25 21:00:51 mschimek Exp $ */

#ifndef __ZVBI3_DVB_MUX_H__
#define __ZVBI3_DVB_MUX_H__

#include <inttypes.h>		/* uint8_t */
#include "macros.h"
#include "sliced.h"		/* vbi3_sliced, vbi3_service_set */
#include "sampling_par.h"	/* vbi3_videostd_set */

VBI3_BEGIN_DECLS

/* Public */

/**
 * @addtogroup DVBMux
 * @{
 */

extern vbi3_bool
vbi3_dvb_multiplex_sliced	(uint8_t **		packet,
				 unsigned int *		packet_left,
				 const vbi3_sliced **	sliced,
				 unsigned int *		sliced_left,
				 vbi3_service_set	service_mask,
				 unsigned int		data_identifier,
				 vbi3_bool		stuffing)
#ifndef DOXYGEN_SHOULD_SKIP_THIS
  _vbi3_nonnull ((1, 2, 3, 4))
#endif
  ;
extern vbi3_bool
vbi3_dvb_multiplex_raw		(uint8_t **		packet,
				 unsigned int *		packet_left,
				 const uint8_t **	raw,
				 unsigned int *		raw_left,
				 unsigned int		data_identifier,
				 vbi3_videostd_set	videostd_set,
				 unsigned int		line,
				 unsigned int		first_pixel_position,
				 unsigned int		n_pixels_total,
				 vbi3_bool		stuffing)
#ifndef DOXYGEN_SHOULD_SKIP_THIS
  _vbi3_nonnull ((1, 2, 3, 4))
#endif
  ;

/**
 * @brief DVB VBI multiplexer context.
 *
 * The contents of this structure are private.
 *
 * Call vbi3_dvb_pes_mux_new() or vbi3_dvb_ts_mux_new() to allocate
 * a DVB VBI multiplexer context.
 */
typedef struct _vbi3_dvb_mux vbi3_dvb_mux;

typedef vbi3_bool
vbi3_dvb_mux_cb			(vbi3_dvb_mux *		mx,
				 void *			user_data,
				 const uint8_t *	packet,
				 unsigned int		packet_size);

extern void
vbi3_dvb_mux_reset		(vbi3_dvb_mux *		mx)
  _vbi3_nonnull ((1));
extern vbi3_bool
vbi3_dvb_mux_cor		(vbi3_dvb_mux *		mx,
				 uint8_t **		buffer,
				 unsigned int *		buffer_left,
				 const vbi3_sliced **	sliced,
				 unsigned int *		sliced_lines,
				 vbi3_service_set	service_mask,
				 const uint8_t *	raw,
				 const vbi3_sampling_par *sampling_par,	 
				 int64_t		pts)
#ifndef DOXYGEN_SHOULD_SKIP_THIS
  _vbi3_nonnull ((1, 2, 3, 4, 5))
#endif
  ;
extern vbi3_bool
vbi3_dvb_mux_feed		(vbi3_dvb_mux *		mx,
				 const vbi3_sliced *	sliced,
				 unsigned int		sliced_lines,
				 vbi3_service_set	service_mask,
				 const uint8_t *	raw,
				 const vbi3_sampling_par *sampling_par,
				 int64_t		pts)
  _vbi3_nonnull ((1));
extern unsigned int
vbi3_dvb_mux_get_data_identifier (const vbi3_dvb_mux *	mx)
  _vbi3_nonnull ((1));
extern vbi3_bool
vbi3_dvb_mux_set_data_identifier (vbi3_dvb_mux *	mx,
				  unsigned int		data_identifier)
  _vbi3_nonnull ((1));
extern unsigned int
vbi3_dvb_mux_get_min_pes_packet_size
				(vbi3_dvb_mux *		mx)
  _vbi3_nonnull ((1));
extern unsigned int
vbi3_dvb_mux_get_max_pes_packet_size
				(vbi3_dvb_mux *		mx)
  _vbi3_nonnull ((1));
extern vbi3_bool
vbi3_dvb_mux_set_pes_packet_size (vbi3_dvb_mux *	mx,
				  unsigned int		min_size,
				  unsigned int		max_size)
  _vbi3_nonnull ((1));
extern void
vbi3_dvb_mux_delete		(vbi3_dvb_mux *		mx);
extern vbi3_dvb_mux *
vbi3_dvb_pes_mux_new		(vbi3_dvb_mux_cb *	callback,
				 void *			user_data)
  _vbi3_alloc;
extern vbi3_dvb_mux *
vbi3_dvb_ts_mux_new		(unsigned int		pid,
				 vbi3_dvb_mux_cb *	callback,
				 void *			user_data)
  _vbi3_alloc;

/** @} */

/* Private */

VBI3_END_DECLS

#endif /* __ZVBI3_DVB_MUX_H__ */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
