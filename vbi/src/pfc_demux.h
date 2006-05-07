/*
 *  libzvbi - Teletext packet page format clear demultiplexer
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

/* $Id: pfc_demux.h,v 1.1.2.3 2006-05-07 06:04:58 mschimek Exp $ */

#ifndef __ZVBI3_PFC_DEMUX_H__
#define __ZVBI3_PFC_DEMUX_H__

#include <inttypes.h>		/* uint8_t */
#include <stdio.h>		/* FILE */
#include "macros.h"
#include "bcd.h"		/* vbi3_pgno */

VBI3_BEGIN_DECLS

/**
 * @addtogroup PFCDemux
 * @{
 */

/**
 * @brief One block of data returned by vbi3_pfc_demux_cb().
 */
typedef struct {
	/** Source page as requested with vbi3_pfc_demux_new(). */
	vbi3_pgno		pgno;

	/** Source stream as requested with vbi3_pfc_demux_new(). */
	unsigned int		stream;

	/** Application ID transmitted with this data block. */
	unsigned int		application_id;

	/** Size of the data block in bytes, 1 ... 2048. */
	unsigned int		block_size;

	/** Data block. */
	uint8_t			block[2048];
} vbi3_pfc_block;

/**
 * @brief PFC demultiplexer context.
 *
 * The contents of this structure are private.
 *
 * Call vbi3_pfc_demux_new() to allocate a PFC
 * demultiplexer context.
 */
typedef struct _vbi3_pfc_demux vbi3_pfc_demux;

/**
 * @param pc PFC demultiplexer context returned by
 *   vbi3_pfx_demux_new() and given to vbi3_pfc_demux_demux().
 * @param user_data User pointer given to vbi3_pfc_demux_new().
 * @param block Structure describing the received data block.
 * 
 * Function called by vbi3_pfc_demux_demux() when a
 * new data block is available.
 */
typedef vbi3_bool
vbi3_pfc_demux_cb		(vbi3_pfc_demux *	pc,
				 void *			user_data,
				 const vbi3_pfc_block *	block);

extern void
vbi3_pfc_demux_reset		(vbi3_pfc_demux *	pc);
extern vbi3_bool
vbi3_pfc_demux_demux		(vbi3_pfc_demux *	pc,
				 const uint8_t		buffer[42]);
extern void
vbi3_pfc_demux_delete		(vbi3_pfc_demux *	pc);
extern vbi3_pfc_demux *
vbi3_pfc_demux_new		(vbi3_pgno		pgno,
				 unsigned int		stream,
				 vbi3_pfc_demux_cb *	callback,
				 void *			user_data)
  __attribute__ ((_vbi3_alloc));

/** @internal */
struct _vbi3_pfc_demux {
	/** Expected next continuity index. */
	unsigned int		ci;

	/** Expected next packet. */
	unsigned int		packet;

	/** Expected number of packets. */
	unsigned int		n_packets;

	/** Block index. */
	unsigned int		bi;

	/** Expected number of block bytes. */
	unsigned int		left;

	vbi3_pfc_demux_cb *	callback;
	void *			user_data;

	vbi3_pfc_block		block;
};

extern void
_vbi3_pfc_block_dump		(const vbi3_pfc_block *	pb,
				 FILE *			fp,
				 vbi3_bool		binary);
extern vbi3_bool
_vbi3_pfc_demux_decode		(vbi3_pfc_demux *	pc,
				 const uint8_t		buffer[42]);
extern void
_vbi3_pfc_demux_destroy		(vbi3_pfc_demux *	pc);
extern vbi3_bool
_vbi3_pfc_demux_init		(vbi3_pfc_demux *	pc,
				 vbi3_pgno		pgno,
				 unsigned int		stream,
				 vbi3_pfc_demux_cb *	callback,
				 void *			user_data);
/** @} */

VBI3_END_DECLS

#endif /* __ZVBI3_PFC_DEMUX_H__ */
