#if 0

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

/* $Id: io-dvb.h,v 1.1.2.3 2006-05-07 06:04:58 mschimek Exp $ */

#ifndef __ZVBI3_IO_DVB_H__
#define __ZVBI3_IO_DVB_H__

#include <inttypes.h>		/* uintN_t */
#include "macros.h"
#include "sliced.h"		/* vbi3_sliced, vbi3_service_set */
#include "sampling.h"		/* vbi3_videostd_set */

VBI3_BEGIN_DECLS

/**
 * @addtogroup DVBMux
 * @{
 */

/**
 * @brief DVB VBI multiplexer context.
 *
 * The contents of this structure are private.
 *
 * Call vbi3_dvb_mux_new() to allocate
 * a DVB VBI multiplexer context.
 */
typedef struct _vbi3_dvb_mux vbi3_dvb_mux;

typedef vbi3_bool
vbi3_dvb_mux_cb			(vbi3_dvb_mux *		mx,
				 void *			user_data,
				 const uint8_t *	packet,
				 unsigned int		packet_size);

extern void
vbi3_dvb_mux_reset		(vbi3_dvb_mux *		mx);
extern vbi3_bool
vbi3_dvb_mux_sliced		(vbi3_dvb_mux *		mx,
				 uint64_t		timestamp,
				 const vbi3_sliced *	sliced,
				 unsigned int		sliced_size,
				 vbi3_service_set	service_set);
vbi3_bool
vbi3_dvb_mux_samples		(vbi3_dvb_mux *		mx,
				 uint64_t		timestamp,
				 const uint8_t *	samples,
				 unsigned int		samples_size,
				 unsigned int		line,
				 unsigned int		offset);
extern void
vbi3_dvb_mux_delete		(vbi3_dvb_mux *		mx);
extern vbi3_dvb_mux *
vbi3_dvb_mux_new			(unsigned int		data_identifier,
				 vbi3_videostd_set	videostd_set,
				 vbi3_dvb_mux_cb *	callback,
				 void *			user_data) vbi3_alloc;

/* Private */

/** @internal */
struct _vbi3_dvb_mux {
	unsigned int		data_identifier;
	vbi3_videostd_set	videostd_set;

	vbi3_dvb_mux_cb *	callback;
	void *			user_data;
};

/* Public */

extern vbi3_bool
vbi3_dvb_multiplex_sliced	(uint8_t **		packet,
				 unsigned int *		packet_left,
				 const vbi3_sliced **	sliced,
				 unsigned int *		sliced_left,
				 unsigned int		data_identifier,
				 vbi3_service_set	service_set);
extern vbi3_bool
vbi3_dvb_multiplex_samples	(uint8_t **		packet,
				 unsigned int *		packet_left,
				 const uint8_t **	samples,
				 unsigned int *		samples_left,
				 unsigned int		samples_size,
				 unsigned int		data_identifier,
				 vbi3_videostd_set	videostd_set,
				 unsigned int		line,
				 unsigned int		offset);

/** @} */

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
				 unsigned int		sliced_size);

extern void
vbi3_dvb_demux_reset		(vbi3_dvb_demux *	dx);
extern void
vbi3_dvb_demux_delete		(vbi3_dvb_demux *	dx);
extern vbi3_dvb_demux *
vbi3_dvb_demux_new		(vbi3_dvb_demux_cb *	callback,
				 void *			user_data) vbi3_alloc;

/* Private */

/** @internal */
struct _vbi3_dvb_demux {
	vbi3_dvb_demux_cb *	callback;
	void *			user_data;
};

/** @} */

VBI3_END_DECLS

#endif /* __ZVBI3_IO_DVB_H__ */

#endif /* 0 */
