/*
 *  libzvbi - Events
 *
 *  Copyright (C) 2000, 2001, 2002 Michael H. Schimek
 *
 *  Based on code from AleVT 1.5.1
 *  Copyright (C) 1998,1999 Edgar Toernig (froese@gmx.de)
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

/* $Id: event.h,v 1.5.2.10 2004-07-09 16:10:52 mschimek Exp $ */

#ifndef EVENT_H
#define EVENT_H

#include <time.h>		/* time_t */
#include "bcd.h"		/* vbi_pgno, vbi_subno */
#include "network.h"		/* vbi_network */
#include "link.h"		/* vbi_link */
#include "aspect_ratio.h"	/* vbi_aspect_ratio */
#include "program_info.h"	/* vbi_program_info */
#include "pdc.h"		/* vbi_program_id */

VBI_BEGIN_DECLS

/**
 * @ingroup Event
 * @name Event types.
 * @{
 */

/**
 * @anchor VBI_EVENT_
 * No event.
 */
#define VBI_EVENT_NONE 0x0000
/**
 * The decoding context is about to be closed. delete() functions
 * send this event to clean up event handlers.
 */
#define	VBI_EVENT_CLOSE 0x0001
/**
 * The decoding context has been reset. reset() functions send
 * this event. Note a @c VBI_EVENT_NETWORK may follow.
 */
#define VBI_EVENT_RESET 0x0002
/**
 * The decoder received and cached another Teletext page
 * designated by ev.ttx_page.pgno and ev.ttx_page.subno.
 */
#define	VBI_EVENT_TTX_PAGE 0x0004
/**
 * A Closed Caption page has changed and needs visual update.
 * The "page", actually CC channel, is designated by ev.caption.pgno,
 * see vbi_pgno for details.
 */
#define VBI_EVENT_CAPTION 0x0008
/**
 */
#define	VBI_EVENT_NETWORK 0x0010
/**
 * @anchor VBI_EVENT_TRIGGER
 * 
 * Triggers are sent by broadcasters to start some action on the
 * user interface of modern TVs. Until libzvbi implements all ;-) of
 * WebTV and SuperTeletext the information available are program
 * related (or unrelated) URLs, short messages and Teletext
 * page links.
 * 
 * This event is sent when a trigger has fired, ev.trigger
 * points to a vbi_link structure describing the link in detail.
 * The structure must be read only.
 */
#define	VBI_EVENT_TRIGGER 0x0020
/**
 * @anchor VBI_EVENT_ASPECT
 *
 * The vbi decoder received new information (potentially from
 * PAL WSS, NTSC XDS or EIA-J CPR-1204) about the program
 * aspect ratio. ev.ratio is a pointer to a vbi_ratio structure.
 * The structure must be read only.
 */
#define	VBI_EVENT_ASPECT 0x0040
/**
 * We have new information about the current or next program.
 * ev.prog_info is a vbi_program_info pointer (due to size), read only.
 *
 * Preliminary.
 *
 * XXX Info from Teletext not implemented yet.
 * XXX Change to get_prog_info. network ditto?
 */
#define	VBI_EVENT_PROG_INFO 0x0080

/**
 * New information about Teletext pages is available, for example
 * a subtitle page has been enabled.
 */
#define VBI_EVENT_PAGE_TYPE 0x0100
/**
 * New information for TOP navigation is available, i.e. the
 * table of page titles changed. Typically clients receive this
 * event once or maybe a few times after a channel change, when
 * the decoder first receives uncached TOP data.
 */
#define VBI_EVENT_TOP_CHANGE 0x0200
/**
 * A new network local time (Teletext packet 8/30 format 1)
 * has been transmitted. ev.local_time.time contains UTC,
 * ev.local_time.gmtoff the local time offset in seconds east
 * of UTC. To get the local time of the network broadcasting
 * this add gmtoff to time.
 */
#define VBI_EVENT_LOCAL_TIME 0x0400
/**
 * A new program ID (VPS, PDC or XDS impulse capture ID) has
 * been transmitted. ev.prog_id points to a vbi_program_id
 * structure with details.
 */
#define VBI_EVENT_PROG_ID 0x0800

/** @} */

/**
 * @brief Teletext page flags.
 */
typedef enum {
	/**
	 * The page header is suitable for a rolling display
	 * and clock updates.
	 */
	VBI_ROLL_HEADER		= 0x000001,
	/**
	 * Newsflash page.
	 */
	VBI_NEWSFLASH		= 0x004000,
	/**
	 * Subtitle page.
	 */
	VBI_SUBTITLE		= 0x008000,
	/**
	 * The page has changed since the last transmission. This
	 * flag is under editorial control, not set by the
	 * vbi_teletext_decoder. See EN 300 706, Section A.2 for details.
	 */
	VBI_UPDATE		= 0x020000,
	/**
	 * When this flag is set, pages are in serial transmission. When
	 * cleared, pages are in series only within their magazine (pgno &
	 * 0x0FF). In the latter case a rolling header should display only
	 * pages from the same magazine.
	 */
	VBI_SERIAL		= 0x100000,
} vbi_ttx_page_flags; 

/**
 * @ingroup Event
 * @brief Event union.
 */
typedef struct vbi_event {
	/** Event type, one of the VBI_EVENT_ symbols. */
	unsigned int		type;

	/** The network this event refers to. */
	const vbi_network *	network;

	/** Capture time passed to the decoder when the event occured. */
	double			timestamp;

	union {
		struct {
			vbi_pgno		pgno;
			vbi_subno		subno;
			vbi_ttx_page_flags	flags;
	        }			ttx_page;
		struct {
			vbi_pgno		channel;
		}			caption;
		const vbi_link *	trigger;
		const vbi_aspect_ratio *aspect;
		const vbi_program_info *prog_info;
		struct { }		page_type;
		struct { }		top_change;
		struct {
			time_t			time;
			int			gmtoff;
		}			local_time;
		const vbi_program_id *	prog_id;
	}			ev;
} vbi_event;

typedef vbi_bool
vbi_event_cb			(const vbi_event *	event,
				 void *			user_data);

/* Private */

/** @internal */
typedef struct _vbi_event_handler vbi_event_handler;

/** @internal */
struct _vbi_event_handler {
	vbi_event_handler *	next;
	vbi_event_cb *		callback;
	void *			user_data;
	unsigned int		event_mask;
	unsigned int		blocked;
};

/** @internal */
typedef struct {
	vbi_event_handler *	first;
	vbi_event_handler *	current;
	unsigned int		event_mask;
} _vbi_event_handler_list;

extern const char *
_vbi_event_name			(unsigned int		event);

#if 0
#define _vbi_event_handler_list_send(es, ev)				\
do {									\
	fprintf (stderr, "%s:%u event %s\n",				\
		 __FILE__, __LINE__, _vbi_event_name ((ev)->type));	\
        __vbi_event_handler_list_send (es, ev);				\
} while (0)
#else
#define _vbi_event_handler_list_send(es, ev)				\
	__vbi_event_handler_list_send (es, ev)
#endif

extern void
__vbi_event_handler_list_send	(_vbi_event_handler_list *es,
				 const vbi_event *	ev);
extern void
_vbi_event_handler_list_remove_by_event
			    	(_vbi_event_handler_list *es,
				 unsigned int		event_mask);
extern void
_vbi_event_handler_list_remove_by_callback
				(_vbi_event_handler_list *es,
				 vbi_event_cb *		callback,
				 void *			user_data);
extern void
_vbi_event_handler_list_remove	(_vbi_event_handler_list *es,
				 vbi_event_handler *	eh);
extern vbi_event_handler *
_vbi_event_handler_list_add	(_vbi_event_handler_list *es,
				 unsigned int		event_mask,
				 vbi_event_cb *		callback,
				 void *			user_data);
extern void
_vbi_event_handler_list_destroy	(_vbi_event_handler_list *es);
extern vbi_bool
_vbi_event_handler_list_init	(_vbi_event_handler_list *es);

VBI_END_DECLS

#endif /* EVENT_H */
