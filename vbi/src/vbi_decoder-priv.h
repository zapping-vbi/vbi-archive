/*
 *  libzvbi - VBI decoding library
 *
 *  Copyright (C) 2000, 2001, 2002 Michael H. Schimek
 *  Copyright (C) 2000, 2001 Iñaki García Etxebarria
 *
 *  Based on AleVT 1.5.1
 *  Copyright (C) 1998, 1999 Edgar Toernig <froese@gmx.de>
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

/* $Id: vbi_decoder-priv.h,v 1.1.2.1 2004-05-12 01:40:44 mschimek Exp $ */

#ifndef VBI_DECODER_PRIV_H
#define VBI_DECODER_PRIV_H

#include "vt.h"
#include "cc.h"
#include "event.h"
#include "trigger.h"
#include "vbi_decoder.h"

struct vbi_decoder {
#if 0 /* obsolete */
	fifo			*source;
        pthread_t		mainloop_thread_id;
	int			quit;		/* XXX */
#endif
	double			time;

  //	pthread_mutex_t		chswcd_mutex;
        int                     chswcd;

	vbi_event		network;

	vbi_trigger *		triggers;

  //	pthread_mutex_t         prog_info_mutex;
	vbi_program_info        prog_info[2];
  //	int                     aspect_source;

	vbi_teletext_decoder	vt;
	vbi_caption_decoder	cc;

	vbi_aspect_ratio	aspect;

	double 			reset_time;

	void (* teletext_reset)	(vbi_teletext_decoder *	td,
				 vbi_nuid		nuid,
				 double			time);

	void (* caption_reset)	(vbi_caption_decoder *	cd,
				 vbi_nuid		nuid,
				 double			time);


  //	cache *		cache;

#if 0 // TODO
	struct page_clear	epg_pc[2];
#endif
	/* preliminary */
	int			pageref;

	_vbi_event_handler_list handlers;

	unsigned char		wss_last[2];
	int			wss_rep_ct;
	double			wss_time;

	/* Property of the vbi_push_video caller */
#if 0 /* obsolete */
	enum tveng_frame_pixformat
				video_fmt;
	int			video_width; 
	double			video_time;
	vbi_bit_slicer_fn *	wss_slicer_fn;
	vbi_bit_slicer		wss_slicer;
	producer		wss_producer;
#endif

};


#endif /* VBI_DECODER_PRIV_H */
