/*
 *  Zapzilla/libvbi
 *
 *  Copyright (C) 2001 Michael H. Schimek
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

/* $Id: vbi.h,v 1.39 2001-12-17 05:28:11 mschimek Exp $ */

#ifndef VBI_H
#define VBI_H

/* Private */

#include <pthread.h>

#include "../common/types.h"
#include "libvbi.h"

#include "vt.h"
#include "cc.h"
#include "cache.h"

#include "../common/fifo.h"
#include "decoder.h"

typedef struct _vbi_trigger vbi_trigger;

struct event_handler {
	struct event_handler *	next;
	int			event_mask;
	void 			(* handler)(vbi_event *ev, void *user_data);
	void *			user_data;
};

struct vbi
{
	fifo			*source;
	double			time;

        pthread_t		mainloop_thread_id;
	int			quit;		/* XXX */

	pthread_mutex_t		chswcd_mutex;
        int                     chswcd;

	vbi_event		network;

	vbi_trigger *		triggers;

	pthread_mutex_t		prog_info_mutex;
	vbi_program_info	prog_info[2];
	int                     aspect_source;

	int			brightness;
	int			contrast;

	struct teletext		vt;
	struct caption		cc;

	struct cache		cache;

	pthread_mutex_t		event_mutex;
	int			event_mask;
	struct event_handler *	handlers;
	struct event_handler *	next_handler;

	unsigned char		wss_last[2];
	int			wss_rep_ct;
	double			wss_time;

	/* Property of the vbi_push_video caller */

	enum tveng_frame_pixformat
				video_fmt;
	int			video_width; 
	double			video_time;
	vbi_bit_slicer_fn *	wss_slicer_fn;
	struct vbi_bit_slicer	wss_slicer;
	producer		wss_producer;
};

static inline void
vbi_send_event(struct vbi *vbi, vbi_event *ev)
{
	struct event_handler *eh;

	pthread_mutex_lock(&vbi->event_mutex);

	for (eh = vbi->handlers; eh; eh = vbi->next_handler) {
		vbi->next_handler = eh->next;

		if (eh->event_mask & ev->type)
			eh->handler(ev, eh->user_data);
	}

	pthread_mutex_unlock(&vbi->event_mutex);
}

extern void		vbi_transp_colourmap(struct vbi *vbi, attr_rgba *d, attr_rgba *s, int entries);
extern void             vbi_chsw_reset(struct vbi *vbi, nuid nuid);

#endif /* VBI_H */
