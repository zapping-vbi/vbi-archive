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

/* $Id: pfc_demux.h,v 1.1.2.1 2004-04-05 04:42:27 mschimek Exp $ */

#ifndef __ZVBI_PFC_DEMUX_H__
#define __ZVBI_PFC_DEMUX_H__

#include <inttypes.h>		/* uint8_t */
#include <stdio.h>		/* FILE */
#include "macros.h"
#include "bcd.h"		/* vbi_pgno */

/* Public */

VBI_BEGIN_DECLS

/**
 * @addtogroup PFCDemux
 * @{
 */

/**
 * @brief One block of data returned by vbi_pfc_demux_cb().
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
typedef struct _vbi_pfc_demux vbi_pfc_demux;

/**
 * @param pc PFC demultiplexer context returned by
 *   vbi_pfx_demux_new() and given to vbi_pfc_demux_demux().
 * @param user_data User pointer given to vbi_pfc_demux_new().
 * @param block Structure describing the received data block.
 * 
 * Function called by vbi_pfc_demux_demux() when a
 * new data block is available.
 */
typedef vbi_bool
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
				 vbi_pfc_demux_cb *	callback,
				 void *			user_data) vbi_alloc;

/** @internal */
struct _vbi_pfc_demux {
	unsigned int		ci;		/* continuity index */
	unsigned int		packet;		/* next packet expected */
	unsigned int		n_packets;	/* num packets expected */

	unsigned int		bi;		/* block index */
	unsigned int		left;		/* block bytes expected */

	vbi_pfc_demux_cb *	callback;
	void *			user_data;

	vbi_pfc_block		block;
};

extern void
_vbi_pfc_block_dump		(const vbi_pfc_block *	pb,
				 FILE *			fp,
				 vbi_bool		binary);
extern vbi_bool
_vbi_pfc_demux_decode		(vbi_pfc_demux *	pc,
				 const uint8_t		buffer[42]);
extern void
_vbi_pfc_demux_destroy		(vbi_pfc_demux *	pc);
extern vbi_bool
_vbi_pfc_demux_init		(vbi_pfc_demux *	pc,
				 vbi_pgno		pgno,
				 unsigned int		stream,
				 vbi_pfc_demux_cb *	callback,
				 void *			user_data);
/** @} */

VBI_END_DECLS

/* Private */

#endif /* __ZVBI_PFC_DEMUX_H__ */
