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

/* $Id: event.h,v 1.5.2.9 2004-05-12 01:40:43 mschimek Exp $ */

#ifndef EVENT_H
#define EVENT_H

#include <time.h>		/* time_t */
#include "bcd.h"
#include "network.h"
#include "link.h"		/* vbi_link */
#include "aspect_ratio.h"	/* vbi_aspect_ratio */
#include "pdc.h"		/* vbi_program_id */


#ifndef VBI_DECODER
#define VBI_DECODER
typedef struct vbi_decoder vbi_decoder;
#endif

/* Public */















/**
 * @addtogroup Event Events
 * @ingroup Service
 *
 * Typically the transmission of VBI data like a Teletext or Closed
 * Caption page spans several VBI lines or even video frames. So internally
 * the data service decoder maintains caches accumulating data. When a page
 * or other object is complete it calls the respective event handler to
 * notify the application.
 *
 * Clients can register any number of handlers needed, also different
 * handlers for the same event. They will be called in the order registered
 * from the vbi_decode() function. Since they block decoding, they should
 * return as soon as possible. The event structure and all data
 * pointed to from there must be read only. The data is only valid until
 * the handler returns.
 */

/*
 *  Program Info
 *
 *  ATTN this is new stuff and subject to change
 */

/**
 * @ingroup Event
 * @brief Program rating source.
 *
 * If program rating information is available (also known in the
 * U. S. as V-Chip data), this describes which rating scheme is
 * being used: U. S. film, U. S. TV, Canadian English or French TV. 
 * You can convert the rating code to a string with
 * vbi_rating_string().
 *
 * When the scheme is @c VBI_RATING_TV_US, additionally the
 * DLSV rating flags will be set.
 */
typedef enum {
	VBI_RATING_AUTH_NONE = 0,
	VBI_RATING_AUTH_MPAA,
	VBI_RATING_AUTH_TV_US,
	VBI_RATING_AUTH_TV_CA_EN,
	VBI_RATING_AUTH_TV_CA_FR
} vbi_rating_auth;

/**
 * @ingroup Event
 * @name US TV rating flags
 * @{
 */
#define VBI_RATING_D 0x08 /**< "sexually suggestive dialog" */
#define VBI_RATING_L 0x04 /**< "indecent language" */
#define VBI_RATING_S 0x02 /**< "sexual situations" */
#define VBI_RATING_V 0x01 /**< "violence" */
/** @} */

extern const char *	vbi_rating_string(vbi_rating_auth auth, int id);

/**
 * @ingroup Event
 * @brief Program classification schemes.
 *
 * libzvbi understands two different program classification schemes,
 * the EIA-608 based in the United States and the ETS 300 231 based
 * one in Europe. You can convert the program type code into a
 * string with vbi_prog_type_string().
 **/
typedef enum {
	VBI_PROG_CLASSF_NONE = 0,
	VBI_PROG_CLASSF_EIA_608,
	VBI_PROG_CLASSF_ETS_300231
} vbi_prog_classf;

/**
 * @addtogroup Event
 * @{
 */
extern const char *	vbi_prog_type_string(vbi_prog_classf classf, int id);
/** @} */

/**
 * @ingroup Event
 * @brief Type of audio transmitted on one (mono or stereo)
 * audio track.
 */
/* code depends on order, don't change */
typedef enum {
	VBI_AUDIO_MODE_NONE = 0,		/**< No sound. */
	VBI_AUDIO_MODE_MONO,			/**< Mono audio. */
	VBI_AUDIO_MODE_STEREO,			/**< Stereo audio. */
	VBI_AUDIO_MODE_STEREO_SURROUND,		/**< Surround. */
	VBI_AUDIO_MODE_SIMULATED_STEREO,	/**< ? */
	/**
	 * Spoken descriptions of the program for the blind, on a secondary audio track.
	 */
	VBI_AUDIO_MODE_VIDEO_DESCRIPTIONS,
	/**
	 * Unrelated to the current program.
	 */
	VBI_AUDIO_MODE_NON_PROGRAM_AUDIO,

	VBI_AUDIO_MODE_SPECIAL_EFFECTS,		/**< ? */
	VBI_AUDIO_MODE_DATA_SERVICE,		/**< ? */
	/**
	 * We have no information what is transmitted.
	 */
	VBI_AUDIO_MODE_UNKNOWN
} vbi_audio_mode;

/**
 * @ingroup Event
 *
 * Information about the current program, preliminary.
 */
typedef struct vbi_program_info {
	/*
	 *  Refers to the current or next program.
	 *  (No [2] to allow clients filtering current data more easily.)
	 */
	unsigned int		future : 1;

	/* 01 Program Identification Number */

	/* If unknown all these fields are -1 */
	signed char		month;		/* 0 ... 11 */
	signed char		day;		/* 0 ... 30 */
	signed char		hour;		/* 0 ... 23 */
	signed char		min;		/* 0 ... 59 */

	/*
	 *  VD: "T indicates if a program is routinely tape delayed for the
	 *  Mountain and Pacific time zones."
	 */
	signed char		tape_delayed;

	/* 02 Program Length */

	/* If unknown all these fields are -1 */
	signed char		length_hour;	/* 0 ... 63 */
	signed char		length_min;	/* 0 ... 59 */

	signed char		elapsed_hour;	/* 0 ... 63 */
	signed char		elapsed_min;	/* 0 ... 59 */
	signed char		elapsed_sec;	/* 0 ... 59 */

	/* 03 Program name */

	/* If unknown title[0] == 0 */
	char			title[64];	/* ASCII + '\0' */

	/* 04 Program type */

	/*
	 *  If unknown type_classf == VBI_PROG_CLASSF_NONE.
	 *  VBI_PROG_CLASSF_EIA_608 can have up to 32 tags
	 *  identifying 96 keywords. Their numerical value
	 *  is given here instead of composing a string for
	 *  easier filtering. Use vbi_prog_type_str_by_id to
	 *  get the keywords. A zero marks the end.
	 */
	vbi_prog_classf		type_classf;
	int			type_id[33];

	/* 05 Program rating */

	/*
	 *  For details STFW for "v-chip"
	 *  If unknown rating_auth == VBI_RATING_NONE
	 */
	vbi_rating_auth		rating_auth;
	int			rating_id;

	/* Only valid when auth == VBI_RATING_TV_US */
	int			rating_dlsv;

	/* 06 Program Audio Services */

	/*
	 *  BTSC audio (two independent tracks) is flagged according to XDS,
	 *  Zweiton/NICAM/EIA-J audio is flagged mono/none, stereo/none or
	 *  mono/mono for bilingual transmissions.
	 */
	struct {
		/* If unknown mode == VBI_AUDIO_MODE_UNKNOWN */
		vbi_audio_mode		mode;
		/* If unknown lang_code == NULL */
		const char *		lang_code; /* ISO 639 */
	}			audio[2];	/* primary and secondary */

	/* 07 Program Caption Services */

	/*
	 *  Bits 0...7 corresponding to Caption page 1...8.
	 *  Note for the current program this information is also
	 *  available via vbi_classify_page().
	 *
	 *  If unknown caption_services == -1, _lang_code[] = NULL
	 */
	int			caption_services;
	const char *		caption_lang_code[8]; /* ISO 639 */

	/* 08 Copy Generation Management System */

	/* If unknown cgms_a == -1 */
	int			cgms_a; /* XXX */

	/* 09 Aspect Ratio */

	/*
	 *  Note for the current program this information is also
	 *  available via VBI_EVENT_ASPECT.
	 *
	 *  If unknown first_line == last_line == -1, ratio == 0.0
	 */
	vbi_aspect_ratio	aspect;

	/* 10 - 17 Program Description */

	/*
	 *  8 rows of 0...32 ASCII chars + '\0',
	 *  if unknown description[0...7][0] == 0
	 */
	char			description[8][33];
} vbi_program_info;

/**
 * @addtogroup Event
 * @{
 */
extern void		vbi_reset_prog_info(vbi_program_info *pi);
/** @} */


/**
 * @ingroup Event
 * @name Event types.
 * @{
 */

/**
 * @anchor VBI_EVENT_
 * No event.
 */
#define VBI_EVENT_NONE		0x0000
/**
 * The vbi decoding context is about to be closed. This event is
 * sent by vbi_decoder_delete() and can be used to clean up
 * event handlers.
 */
#define	VBI_EVENT_CLOSE		0x0001
/**
 * The vbi decoder received and cached another Teletext page
 * designated by ev.ttx_page.pgno and ev.ttx_page.subno.
 * 
 * ev.ttx_page.roll_header flags the page header as suitable for
 * rolling page numbers, e. g. excluding pages transmitted out
 * of order.
 * 
 * The ev.ttx_page.header_update flag is set when the header,
 * excluding the page number and real time clock, changed since the
 * last @c VBI_EVENT_TTX_PAGE. Note this may happen at midnight when the
 * date string changes. The ev.ttx_page.clock_update flag is set when
 * the real time clock changed since the last @c VBI_EVENT_TTX_PAGE (that is
 * at most once per second). They are both set at the first
 * @c VBI_EVENT_TTX_PAGE sent and unset while the received header
 * or clock field is corrupted.
 * 
 * If any of the roll_header, header_update or clock_update flags
 * are set ev.ttx_page.raw_header is a pointer to the raw header data
 * (40 bytes), which remains valid until the event handler returns.
 * ev.ttx_page.pn_offset will be the offset (0 ... 37) of the three
 * digit page number in the raw or formatted header. Allways call
 * vbi_fetch_vt_page() for proper translation of national characters
 * and character attributes, the raw header is only provided here
 * as a means to quickly detect changes.
 */
#define	VBI_EVENT_TTX_PAGE	0x0002
/**
 * A Closed Caption page has changed and needs visual update.
 * The page or "CC channel" is designated by ev.caption.pgno,
 * see vbi_pgno for details.
 * 
 * When the client is monitoring this page, the expected action is
 * to call vbi_fetch_cc_page(). To speed up rendering more detailed
 * update information is provided in vbi_page.dirty, see #vbi_page.
 * The vbi_page will be a snapshot of the status at fetch time
 * and not event time, vbi_page.dirty accumulates all changes since
 * the last fetch.
 */
#define VBI_EVENT_CAPTION	0x0004
/**
 * Some station/network identifier has been received or is no longer
 * transmitted (vbi_network all zero, eg. after a channel switch).
 * ev.network is a vbi_network object, read only. The event will not
 * repeat*) unless a different identifier has been received and confirmed.
 *
 * Minimum time to identify network, when data service is transmitted:
 * <table>
 * <tr><td>VPS (DE/AT/CH only):</td><td>0.08 s</td></tr>
 * <tr><td>Teletext PDC, 8/30:</td><td>2 s</td></tr>
 * <tr><td>Teletext X/26:</td><td>unknown</td></tr>
 * <tr><td>XDS (US only):</td><td>unknown, between 0.1x to 10x seconds</td></tr>
 * </table>
 *
 * *) VPS/TTX and XDS will not combine in real life, feeding the decoder
 *    with artificial data can confuse the logic.
 */
#define	VBI_EVENT_NETWORK	0x0008
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
#define	VBI_EVENT_TRIGGER	0x0010
/**
 * @anchor VBI_EVENT_ASPECT
 *
 * The vbi decoder received new information (potentially from
 * PAL WSS, NTSC XDS or EIA-J CPR-1204) about the program
 * aspect ratio. ev.ratio is a pointer to a vbi_ratio structure.
 * The structure must be read only.
 */
#define	VBI_EVENT_ASPECT	0x0040
/**
 * We have new information about the current or next program.
 * ev.prog_info is a vbi_program_info pointer (due to size), read only.
 *
 * Preliminary.
 *
 * XXX Info from Teletext not implemented yet.
 * XXX Change to get_prog_info. network ditto?
 */
#define	VBI_EVENT_PROG_INFO	0x0080
/** @} */

#include <inttypes.h>

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
/* XXX network, aspect, prog_info: should only notify about
 * changes and provide functions to query current value.
 */
typedef struct vbi_event {
	int			type;
	union {
		struct {
			vbi_pgno		pgno;
			vbi_subno		subno;
			vbi_ttx_page_flags	flags;
	        }			ttx_page;
		struct {
			int			pgno;
		}			caption;
		vbi_network		network;
                vbi_link *		trigger;
                vbi_aspect_ratio	aspect;
		vbi_program_info *	prog_info;
#define VBI_EVENT_PAGE_TYPE 0x0800
		struct { }		page_type;
#define VBI_EVENT_TOP_CHANGE 0x4000
		struct { }		top_change;
#define VBI_EVENT_LOCAL_TIME 0x1000
		struct {
			time_t			time;
			int			gmtoff;
		}			local_time;
#define VBI_EVENT_PROG_ID 0x2000
		vbi_program_id *	prog_id;
	}			ev;
} vbi_event;

#if 0 // obsolete
typedef void (* vbi_event_handler)(vbi_event *event, void *user_data);

extern vbi_bool		vbi_event_handler_add(vbi_decoder *vbi, int event_mask,
					      vbi_event_handler handler,
					      void *user_data);
extern void		vbi_event_handler_remove(vbi_decoder *vbi,
						 vbi_event_handler handler);
extern vbi_bool		vbi_event_handler_register(vbi_decoder *vbi, int event_mask,
						   vbi_event_handler handler,
						   void *user_data);
extern void		vbi_event_handler_unregister(vbi_decoder *vbi,
						     vbi_event_handler handler,
						     void *user_data);
extern void		vbi_send_event(vbi_decoder *vbi, vbi_event *ev);
#endif











typedef vbi_bool
vbi_event_cb			(const vbi_event *	event,
				 void *			user_data);

/* Private */

typedef struct _vbi_event_handler _vbi_event_handler;

struct _vbi_event_handler {
	_vbi_event_handler *	next;
	vbi_event_cb *		callback;
	void *			user_data;
	unsigned int		event_mask;
	unsigned int		blocked;
};

typedef struct {
	_vbi_event_handler *	first;
	_vbi_event_handler *	current;
	unsigned int		event_mask;
} _vbi_event_handler_list;

extern void
_vbi_event_handler_list_send	(_vbi_event_handler_list *es,
				 const vbi_event *	ev);
extern void
_vbi_event_handler_list_remove_by_event
			    	(_vbi_event_handler_list *es,
				 unsigned int		event_mask);
extern void
_vbi_event_handler_list_remove	(_vbi_event_handler_list *es,
				 vbi_event_cb *		callback,
				 void *			user_data);
extern vbi_bool
_vbi_event_handler_list_add	(_vbi_event_handler_list *es,
				 unsigned int		event_mask,
				 vbi_event_cb *		callback,
				 void *			user_data);
extern void
_vbi_event_handler_list_destroy	(_vbi_event_handler_list *es);
extern vbi_bool
_vbi_event_handler_list_init	(_vbi_event_handler_list *es);








#endif /* EVENT_H */
