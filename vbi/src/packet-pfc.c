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

/* $Id: packet-pfc.c,v 1.1.2.1 2004-02-25 17:35:29 mschimek Exp $ */

#include "../config.h"

#include <stdio.h>
#include <stdlib.h>

#include "hamm.h"
#include "packet-pfc.h"

#define BLOCK_SEPARATOR 0x0C
#define FILLER_BYTE 0x03

/**
 * @addtogroup PFCDemux Teletext Page Function Clear Demultiplexer
 * @ingroup LowDec
 * @brief Separating data transmitted in Page Function Clear
 *   Teletext packets (ETS 300 708 section 4).
 */

/**
 * @param pc PFC demultiplexer context allocated with vbi_pfc_demux_new().
 *
 * Resets the PFC demux context, useful for example after a channel
 * change.
 */
void
vbi_pfc_demux_reset		(vbi_pfc_demux *	pc)
{
	pc->ci			= 256;	/* normally 0 ... 15 */
	pc->packet		= 256;  /* normally 1 ... 25 */
	pc->n_packets		= 0;

	pc->bi			= 0;
	pc->left		= 0;

	pc->block.application_id = -1;	/* expect SH next */
}

/**
 * @param pc PFC demultiplexer context allocated with vbi_pfc_demux_new().
 * @param buffer Teletext packet (last 42 bytes, i. e. without clock
 *   run-in and framing code), as in struct vbi_sliced.
 *
 * Takes a complete stream of Teletext packets, filters out the page
 * and stream requested with vbi_pfc_demux_new() and assembles the
 * Page Function Clear data transmitted in this page in a buffer.
 * When a data block is complete, calls the output function given to
 * vbi_pfc_demux_new().
 *
 * @returns
 * FALSE if the packet contained incorrectable errors.
 */
vbi_bool
vbi_pfc_demux_demux		(vbi_pfc_demux *	pc,
				 const uint8_t		buffer[42])
{
	int pmag;
	vbi_pgno pgno;
	vbi_subno subno;
	unsigned int packet;
	unsigned int col;
	int bp;

	if ((pmag = vbi_iham16p (buffer)) < 0)
		goto desync;

	pgno = pmag & 7;
	if (0 == pgno)
		pgno = 0x800;
	else
		pgno <<= 8;

	packet = pmag >> 3;

	if (0 == packet) {
		unsigned int stream;
		unsigned int ci;

		pgno |= vbi_iham16p (buffer + 2);
		if (pgno < 0)
			goto desync;

		if (pgno != pc->block.pgno) {
			pc->n_packets = 0;
			return TRUE;
		}

		subno = vbi_iham16p (buffer + 4)
			+ vbi_iham16p (buffer + 6) * 256;
		if (subno < 0)
			goto desync;

		stream = (subno >> 8) & 15;
		if (stream != pc->block.stream) {
			pc->n_packets = 0;
			return TRUE;
		}

		ci = subno & 15;
		if (ci != pc->ci) {
			/* Page continuity lost, wait for new block. */
			vbi_pfc_demux_reset (pc);
		}

		pc->ci = (ci + 1) & 15; /* next ci expected */

		pc->packet = 1;
		pc->n_packets = ((subno >> 4) & 7) + ((subno >> 9) & 0x18);

		return TRUE;
	} else {
		/* In case 0 == C11 parallel page trasmission. */
		if ((pgno ^ pc->block.pgno) & 0xF00)
			return TRUE; /* not pc->block.pgno */
	}

	if (0 == pc->n_packets)
		return TRUE; /* not pc->block.pgno */

	if (packet > 25)
		return TRUE; /* filler packets, whatever */

	if (packet != pc->packet
	    || packet > pc->n_packets) {
		/* Packet continuity lost, wait for new
		   block and page header. */
		vbi_pfc_demux_reset (pc);
		return TRUE;
	}

	pc->packet = packet + 1; /* next packet expected */

	bp = vbi_iham8 (buffer[2]) * 3;
	if (bp < 0 || bp > 39) {
		/* Implausible pointer, possibly hamming error (-1). */
		goto desync;
	}

	col = 3;

	while (col < 42) {
		int bs;

		if (pc->left > 0) {
			unsigned int size;

			size = MIN (pc->left, 42 - col);

			memcpy (pc->block.block + pc->bi, buffer + col, size);

			pc->bi += size;
			pc->left -= size;

			if (pc->left > 0)
				return TRUE; /* packet done */

			col += size;

			if ((int) pc->block.application_id < 0) {
				int sh; /* structure header */

				sh = vbi_iham16p (pc->block.block)
					+ vbi_iham16p (pc->block.block + 2)
					* 256;

				if (sh < 0)
					goto desync;

				pc->block.application_id = sh & 0x1F;
				pc->block.block_size = sh >> 5;

				pc->bi = 0;
				pc->left = pc->block.block_size; 

				continue;
			} else {
				pc->callback (pc, pc->user_data, &pc->block);
			}
		}

		if (col <= 3) {
			if (bp >= 39) {
				/* No structure header, packet done. */
				return TRUE;
			}

			col = bp + 4; /* 2 pmag, 1 bp, 1 bs */
			bs = vbi_iham8 (buffer[col - 1]);
		} else {
			while (FILLER_BYTE == (bs = vbi_iham8 (buffer[col++]))) {
				if (col >= 42)
					return TRUE; /* packet done */
			}
		}

		if (BLOCK_SEPARATOR != bs) {
			goto desync;
		}

		/* First with application_id == -1 we read 4 bytes structure
		   header into block[], then with application_id >= 0
		   block_size data bytes. */

		pc->bi = 0;
		pc->left = 4;

		pc->block.application_id = -1;
	}

	return TRUE;

 desync:
	vbi_pfc_demux_reset (pc);

	return FALSE;
}

/**
 * @param pc PFC demultiplexer context allocated with
 *   vbi_pfc_demux_new(), can be @c NULL.
 *
 * Frees all resources associated with @a pc.
 */
void
vbi_pfc_demux_delete		(vbi_pfc_demux *	pc)
{
	if (NULL == pc)
		return;

	CLEAR (*pc);

	free (pc);		
}

/**
 * @param pgno Page to take PFC data from.
 * @param stream PFC stream to be demultiplexed.
 * @param cb Function to be called by vbi_pfc_demux_demux() when
 *   a new data block is available.
 * @param user_data User pointer passed through to @a cb function.
 *
 * Allocates a new Page Function Clear (ETS 300 708 section 4)
 * demultiplexer.
 *
 * @returns
 * Pointer to newly allocated PFC demux context which must be
 * freed with vbi_pfc_demux_delete() when done. @c NULL on failure
 * (out of memory).
 */
vbi_pfc_demux *
vbi_pfc_demux_new		(vbi_pgno		pgno,
				 unsigned int		stream,
				 vbi_pfc_demux_cb *	cb,
				 void *			user_data)
{
	vbi_pfc_demux *pc;

	assert (NULL != cb);

	if (!(pc = malloc (sizeof (*pc)))) {
		vbi_log_printf (__FUNCTION__, "Out of memory");
		return NULL;
	}

	vbi_pfc_demux_reset (pc);

	pc->callback		= cb;
	pc->user_data		= user_data;
	pc->block.pgno		= pgno;
	pc->block.stream	= stream;

	return pc;
}
