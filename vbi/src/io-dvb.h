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

/* $Id: io-dvb.h,v 1.1.2.1 2004-04-05 04:42:27 mschimek Exp $ */

#ifndef __ZVBI_IO_DVB_H__
#define __ZVBI_IO_DVB_H__

#include <inttypes.h>		/* uintN_t */
#include "macros.h"
#include "sliced.h"		/* vbi_sliced, vbi_service_set */
#include "sampling.h"		/* vbi_videostd_set */

/* Public */

VBI_BEGIN_DECLS

/**
 * @addtogroup DVBMux
 * @{
 */

/**
 * @brief DVB VBI multiplexer context.
 *
 * The contents of this structure are private.
 *
 * Call vbi_dvb_mux_new() to allocate
 * a DVB VBI multiplexer context.
 */
typedef struct _vbi_dvb_mux vbi_dvb_mux;

typedef vbi_bool
vbi_dvb_mux_cb			(vbi_dvb_mux *		mx,
				 void *			user_data,
				 const uint8_t *	packet,
				 unsigned int		packet_size);

extern void
vbi_dvb_mux_reset		(vbi_dvb_mux *		mx);
extern vbi_bool
vbi_dvb_mux_sliced		(vbi_dvb_mux *		mx,
				 uint64_t		timestamp,
				 const vbi_sliced *	sliced,
				 unsigned int		sliced_size,
				 vbi_service_set	service_set);
vbi_bool
vbi_dvb_mux_samples		(vbi_dvb_mux *		mx,
				 uint64_t		timestamp,
				 const uint8_t *	samples,
				 unsigned int		samples_size,
				 unsigned int		line,
				 unsigned int		offset);
extern void
vbi_dvb_mux_delete		(vbi_dvb_mux *		mx);
extern vbi_dvb_mux *
vbi_dvb_mux_new			(unsigned int		data_identifier,
				 vbi_videostd_set	videostd_set,
				 vbi_dvb_mux_cb *	callback,
				 void *			user_data) vbi_alloc;

/* Private */

/** @internal */
struct _vbi_dvb_mux {
	unsigned int		data_identifier;
	vbi_videostd_set	videostd_set;

	vbi_dvb_mux_cb *	callback;
	void *			user_data;
};

/* Public */

extern vbi_bool
vbi_dvb_multiplex_sliced	(uint8_t **		packet,
				 unsigned int *		packet_left,
				 const vbi_sliced **	sliced,
				 unsigned int *		sliced_left,
				 unsigned int		data_identifier,
				 vbi_service_set	service_set);
extern vbi_bool
vbi_dvb_multiplex_samples	(uint8_t **		packet,
				 unsigned int *		packet_left,
				 const uint8_t **	samples,
				 unsigned int *		samples_left,
				 unsigned int		samples_size,
				 unsigned int		data_identifier,
				 vbi_videostd_set	videostd_set,
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
 * Call vbi_dvb_demux_new() to allocate
 * a DVB VBI multiplexer context.
 */
typedef struct _vbi_dvb_demux vbi_dvb_demux;

typedef vbi_bool
vbi_dvb_demux_cb		(vbi_dvb_demux *	dx,
				 void *			user_data,
				 const vbi_sliced *	sliced,
				 unsigned int		sliced_size);

extern void
vbi_dvb_demux_reset		(vbi_dvb_demux *	dx);
extern void
vbi_dvb_demux_delete		(vbi_dvb_demux *	dx);
extern vbi_dvb_demux *
vbi_dvb_demux_new		(vbi_dvb_demux_cb *	callback,
				 void *			user_data) vbi_alloc;

/* Private */

/** @internal */
struct _vbi_dvb_demux {
	vbi_dvb_demux_cb *	callback;
	void *			user_data;
};

/** @} */

/* Public */

VBI_END_DECLS

/* Private */

#endif /* __ZVBI_IO_DVB_H__ */
