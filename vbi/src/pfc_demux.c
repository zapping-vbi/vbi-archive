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

/* $Id: pfc_demux.c,v 1.1.2.1 2004-04-05 04:42:27 mschimek Exp $ */

#include "../config.h"

#include <assert.h>
#include <stdlib.h>		/* malloc() */
#include <string.h>		/* memcpy() */
#include "hamm.h"		/* vbi_iham8(), vbi_iham16p() */
#include "misc.h"		/* vbi_log_printf */
#include "pfc_demux.h"

#define BLOCK_SEPARATOR 0x0C
#define FILLER_BYTE 0x03

#define printable(c)							\
	((((c) & 0x7F) < 0x20 || ((c) & 0x7F) > 0x7E) ? '.' : ((c) & 0x7F))

/**
 * @addtogroup PFCDemux Teletext Page Function Clear Demultiplexer
 * @ingroup LowDec
 * @brief Separating data transmitted in Page Function Clear
 *   Teletext packets (ETS 300 708 section 4).
 */

/** @internal */
void
_vbi_pfc_block_dump		(const vbi_pfc_block *	pb,
				 FILE *			fp,
				 vbi_bool		binary)
{
	assert (NULL != pb);
	assert (NULL != fp);

	fprintf (fp, "PFC pgno=%x stream=%u id=%u size=%u\n",
		 pb->pgno, pb->stream,
		 pb->application_id,
		 pb->block_size);

	if (binary) {
		fwrite (pb->block, sizeof (pb->block[0]), pb->block_size, fp);
	} else {
		unsigned int i;

		for (i = 0; i < pb->block_size; ++i) {
			fputc (printable (pb->block[i]), fp);

			if ((i % 75) == 75)
				fputc ('\n', fp);
		}

		if ((i % 75) != 75)
			fputc ('\n', fp);
	}
}

/**
 * @param pc PFC demultiplexer context allocated with vbi_pfc_demux_new().
 *
 * Resets the PFC demux context, useful for example after a channel
 * change.
 */
void
vbi_pfc_demux_reset		(vbi_pfc_demux *	pc)
{
	assert (NULL != pc);

	pc->ci			= 256;	/* normally 0 ... 15 */
	pc->packet		= 256;  /* normally 1 ... 25 */
	pc->n_packets		= 0;	/* discard all */

	pc->bi			= 0;	/* empty buffer */
	pc->left		= 0;

	pc->block.application_id = (unsigned int) -1; /* expect SH next */
}

/** @internal */
vbi_bool
_vbi_pfc_demux_decode		(vbi_pfc_demux *	pc,
				 const uint8_t		buffer[42])
{
	unsigned int col;
	int bp;

	bp = vbi_iham8 (buffer[2]) * 3;
	if (bp < 0 || bp > 39) {
		/* Invalid pointer or hamming error (-1). */
		goto desynced;
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

			if (pc->left > 0) {
				/* Packet done, block unfinished. */
				return TRUE;
			}

			col += size;

			if ((int) pc->block.application_id < 0) {
				int sh; /* structure header */

				sh = vbi_iham16p (pc->block.block)
					+ vbi_iham16p (pc->block.block + 2)
					* 256;

				if (sh < 0) {
					/* Hamming error. */
					goto desynced;
				}

				pc->block.application_id = sh & 0x1F;
				pc->block.block_size = sh >> 5;

				pc->bi = 0;
				pc->left = pc->block.block_size; 

				continue;
			} else {
				if (!pc->callback (pc, pc->user_data,
						   &pc->block)) {
					goto desynced;
				}
			}
		}

		if (col <= 3) {
			if (bp >= 39) {
				/* No new block starts in this packet. */
				return TRUE;
			}

			col = bp + 4; /* 2 pmag, 1 bp, 1 bs */
			bs = vbi_iham8 (buffer[col - 1]);
		} else {
			while (FILLER_BYTE ==
			       (bs = vbi_iham8 (buffer[col++]))) {
				if (col >= 42) {
					/* No more data in this packet. */
					return TRUE;
				}
			}
		}

		if (BLOCK_SEPARATOR != bs) {
			/* BP must point to a block separator. */
			goto desynced;
		}

		/* First with application_id == -1 we read 4 bytes structure
		   header into block[], then with application_id >= 0
		   block_size data bytes. */

		pc->bi = 0;
		pc->left = 4;

		pc->block.application_id = (unsigned int) -1;
	}

	return TRUE;

 desynced:
	/* Incorrectable error, discard current block. */
	vbi_pfc_demux_reset (pc);

	return FALSE;
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

	assert (NULL != pc);
	assert (NULL != buffer);

	/* Packet filter. */

	if ((pmag = vbi_iham16p (buffer)) < 0)
		goto desynced;

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
			goto desynced;

		if (pgno != pc->block.pgno) {
			pc->n_packets = 0;
			return TRUE;
		}

		subno = vbi_iham16p (buffer + 4)
			+ vbi_iham16p (buffer + 6) * 256;
		if (subno < 0)
			goto desynced;

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
		/* In case 0 == C11 parallel page transmission. */
		if ((pgno ^ pc->block.pgno) & 0xF00) {
			/* Not pc->block.pgno. */
			return TRUE;
		}
	}

	if (0 == pc->n_packets) {
		/* Not pc->block.pgno. */
		return TRUE;
	}

	if (packet > 25) {
		/* Stuffing packets, whatever. */
		return TRUE;
	}

	if (packet != pc->packet
	    || packet > pc->n_packets) {
		/* Packet continuity lost, wait for new
		   block and page header. */
		vbi_pfc_demux_reset (pc);
		return TRUE;
	}

	pc->packet = packet + 1; /* next packet expected */

	/* Now the actual decoding. */	

	return _vbi_pfc_demux_decode (pc, buffer);

 desynced:
	/* Incorrectable error, discard current block. */
	vbi_pfc_demux_reset (pc);

	return FALSE;
}

/**
 * @internal
 */
void
_vbi_pfc_demux_destroy		(vbi_pfc_demux *	pc)
{
	assert (NULL != pc);

	CLEAR (*pc);
}

/**
 * @internal
 */
vbi_bool
_vbi_pfc_demux_init		(vbi_pfc_demux *	pc,
				 vbi_pgno		pgno,
				 unsigned int		stream,
				 vbi_pfc_demux_cb *	callback,
				 void *			user_data)
{
	assert (NULL != pc);
	assert (NULL != callback);

	vbi_pfc_demux_reset (pc);

	pc->callback		= callback;
	pc->user_data		= user_data;

	pc->block.pgno		= pgno;
	pc->block.stream	= stream;

	return TRUE;
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

	_vbi_pfc_demux_destroy (pc);

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
				 vbi_pfc_demux_cb *	callback,
				 void *			user_data)
{
	vbi_pfc_demux *pc;

	if (!(pc = malloc (sizeof (*pc)))) {
		vbi_log_printf (__FUNCTION__, "Out of memory");
		return NULL;
	}

	if (!_vbi_pfc_demux_init (pc, pgno, stream,
				  callback, user_data)) {
		free (pc);
		pc = NULL;
	}

	return pc;
}
