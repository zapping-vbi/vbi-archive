/*
 *  libzvbi - Teletext packet decoder, page format clear
 *
 *  Copyright (C) 2003-2004 Michael H. Schimek
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

/* $Id: packet-pfc.h,v 1.1.2.1 2004-02-25 17:35:29 mschimek Exp $ */

#ifndef PACKET_PFC_H
#define PACKET_PFC_H

#include <inttypes.h>

#include "bcd.h"

/* Public */

/**
 * @addtogroup PFCDemux
 * @{
 */

/**
 * One block of data returned by vbi_pfc_demux_cb().
 */
typedef struct {
	/** Source page as requested with vbi_pfc_demux_new(). */
	vbi_pgno		pgno;

	/** Source stream as requested with vbi_pfc_demux_new(). */
	unsigned int		stream;

	/** Application ID transmitted with this data block. */
	unsigned int		application_id;

	/** Size of the data block in bytes, 1 ... 2048. */
	unsigned int		block_size;

	/** Data block. */
	uint8_t			block[2048];
} vbi_pfc_block;

/**
 * @brief PFC demultiplexer context.
 *
 * The contents of this structure are private.
 *
 * Call vbi_pfc_demux_new() to allocate a PFC
 * demultiplexer context.
 */
typedef struct vbi_pfc_demux vbi_pfc_demux;

/**
 * @param pc PFC demultiplexer context returned by
 *   vbi_pfx_demux_new() and given to vbi_pfc_demux_demux().
 * @param user_data User pointer given to vbi_pfc_demux_new().
 * @param block Structure describing the received data block.
 * 
 * Function called by vbi_pfc_demux_demux() when a
 * new data block is available.
 */
typedef void
vbi_pfc_demux_cb		(vbi_pfc_demux *	pc,
				 void *			user_data,
				 const vbi_pfc_block *	block);

extern void
vbi_pfc_demux_reset		(vbi_pfc_demux *	pc);
extern vbi_bool
vbi_pfc_demux_demux		(vbi_pfc_demux *	pc,
				 const uint8_t		buffer[42]);
extern void
vbi_pfc_demux_delete		(vbi_pfc_demux *	pc);
extern vbi_pfc_demux *
vbi_pfc_demux_new		(vbi_pgno		pgno,
				 unsigned int		stream,
				 vbi_pfc_demux_cb *	cb,
				 void *			user_data);
/** @} */

/* Private */

struct vbi_pfc_demux {
	vbi_pfc_demux *		next;

	unsigned int		ci;		/* continuity index */
	unsigned int		packet;
	unsigned int		n_packets;

	unsigned int		bi;		/* block index */
	unsigned int		left;

	vbi_pfc_demux_cb *	callback;
	void *			user_data;

	vbi_pfc_block		block;
};

#endif /* PACKET_PFC_H */
