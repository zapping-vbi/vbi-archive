/*
 *  libzvbi - Closed Caption decoder
 *
 *  Copyright (C) 2000, 2001, 2002 Michael H. Schimek
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

/* $Id: cc.h,v 1.2.2.6 2004-07-09 16:10:52 mschimek Exp $ */

#ifndef CC_H
#define CC_H

#include <pthread.h>

#include "bcd.h"
#include "format.h"
#include "xds_decoder.h"
#include "caption_decoder.h"
#include "cache-priv.h"

typedef enum {
	MODE_NONE,
	MODE_POP_ON,
	MODE_PAINT_ON,
	MODE_ROLL_UP,
	MODE_TEXT
} cc_mode;

typedef struct {
	cc_mode			mode;

	int			col, col1;
	int			row, row1;
	int			roll;

        int			nul_ct;	/* XXX should be 'silence count' */
	double			time;

	/*
	 *  ISO 639 language code (e.g. "fr"), and a NULL array terminator.
	 *  Used by vbi_cache_page_language().
	 */
	const char *		lang_code[2];

	vbi_char		attr;
	vbi_char *		line;

	int			hidden;
	vbi_page		pg[2];
} cc_channel;

struct _vbi_caption_decoder {
  // obsolete
  //	pthread_mutex_t		mutex;

	unsigned char		last[2];		/* field 1, cc command repetition */

	int			curr_chan;
	vbi_char		transp_space[2];	/* caption, text mode */

	/*
	 *  The CC "page cache". Indices refer to caption channel
	 *  1-4, text channel 1-4. channel[8] stores the garbage
	 *  when we don't know the channel number.
	 */
	cc_channel		channel[9];

	vbi_xds_demux		xds_demux;
	int			xds;

	unsigned char		itv_buf[256];
	int			itv_count;

	int			info_cycle[2];

	void (* virtual_reset)	(vbi_caption_decoder *	cd,
				 cache_network *	cn,
				 double			time);
};

/* Public */

/**
 * @addtogroup Cache
 * @{
 */
extern vbi_bool		vbi_fetch_cc_page(vbi_caption_decoder *cd,
					  vbi_page *pg,
					  vbi_pgno pgno, vbi_bool reset);
/** @} */

/* Private */

extern void
_vbi_caption_decoder_init	(vbi_caption_decoder *	cd,
				 vbi_cache *		ca,
				 const vbi_network *	nk,
				 vbi_videostd_set	videostd_set);
extern void
_vbi_caption_decoder_destroy	(vbi_caption_decoder *	cd);
extern void
vbi_decode_caption		(vbi_caption_decoder *	cd,
				 int line, uint8_t *buf);
extern void
vbi_caption_decoder_resync	(vbi_caption_decoder *	cd);
extern void
vbi_caption_channel_switched	(vbi_caption_decoder *	cd);
extern void
vbi_caption_color_level		(vbi_caption_decoder *	cd);

#endif /* CC_H */
