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

/* $Id: pfc_demux.h,v 1.1.2.8 2008-02-25 20:59:39 mschimek Exp $ */

#ifndef __ZVBI3_PFC_DEMUX_H__
#define __ZVBI3_PFC_DEMUX_H__

#include <inttypes.h>		/* uint8_t */
#include <stdio.h>		/* FILE */
#include "bcd.h"		/* vbi3_pgno */
#include "sliced.h"

VBI3_BEGIN_DECLS

/* Public */

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
 * @param dx PFC demultiplexer context returned by
 *   vbi3_pfx_demux_new() and given to vbi3_pfc_demux_demux().
 * @param user_data User pointer given to vbi3_pfc_demux_new().
 * @param block Structure describing the received data block.
 * 
 * Function called by vbi3_pfc_demux_feed() when a
 * new data block is available.
 *
 * @returns
 * FALSE on error, will be returned by vbi3_pfc_demux_feed().
 *
 * @bugs
 * vbi_pfc_demux_feed() returns the @a user_data pointer as second
 * parameter the @a block pointer as third parameter, but prior to
 * version 0.2.26 this function incorrectly defined @a block as
 * second and @a user_data as third parameter.
 */
typedef vbi3_bool
vbi3_pfc_demux_cb		(vbi3_pfc_demux *	dx,
				 void *			user_data,
				 const vbi3_pfc_block *	block);

extern void
vbi3_pfc_demux_reset		(vbi3_pfc_demux *	dx)
  _vbi3_nonnull ((1));
extern vbi3_bool
vbi3_pfc_demux_feed		(vbi3_pfc_demux *	dx,
				 const uint8_t		buffer[42])
  _vbi3_nonnull ((1, 2));
extern vbi3_bool
vbi3_pfc_demux_feed_frame	(vbi3_pfc_demux *	dx,
				 const vbi3_sliced *	sliced,
				 unsigned int		n_lines)
  _vbi3_nonnull ((1, 2));
extern void
vbi3_pfc_demux_delete		(vbi3_pfc_demux *	dx);
extern vbi3_pfc_demux *
vbi3_pfc_demux_new		(vbi3_pgno		pgno,
				 unsigned int		stream,
				 vbi3_pfc_demux_cb *	callback,
				 void *			user_data)
  _vbi3_alloc _vbi3_nonnull ((3));

/* Private */

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
				 vbi3_bool		binary)
  _vbi3_nonnull ((1, 2));
extern vbi3_bool
_vbi3_pfc_demux_decode		(vbi3_pfc_demux *	dx,
				 const uint8_t		buffer[42])
  _vbi3_nonnull ((1, 2));
extern void
_vbi3_pfc_demux_destroy		(vbi3_pfc_demux *	dx)
  _vbi3_nonnull ((1));
extern vbi3_bool
_vbi3_pfc_demux_init		(vbi3_pfc_demux *	dx,
				 vbi3_pgno		pgno,
				 unsigned int		stream,
				 vbi3_pfc_demux_cb *	callback,
				 void *			user_data)
  _vbi3_nonnull ((1, 4));

/** @} */

VBI3_END_DECLS

#endif /* __ZVBI3_PFC_DEMUX_H__ */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
