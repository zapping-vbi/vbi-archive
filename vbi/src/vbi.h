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

/* $Id: vbi.h,v 1.5.2.6 2004-02-18 07:53:55 mschimek Exp $ */

#ifndef VBI_H
#define VBI_H

#include <pthread.h>
#include <stdarg.h>

#include "vt.h"
#include "cc.h"
#include "sliced.h"
#include "event.h"
#include "cache.h"
#include "trigger.h"

struct event_handler {
	struct event_handler *	next;
	int			event_mask;
	vbi_event_handler	handler;
	void *			user_data;
};

typedef enum {
	/* uint8_t, 42, 0 */
	VBI_HOOK_TELETEXT_PACKET,
	/* uint8_t, 42, packet number */
	VBI_HOOK_TELETEXT_PACKET_8_30,
	/* uint8_t, variable, application id */
	VBI_HOOK_EACEM_TRIGGER,
	/* uint8_t (odd parity), 2, 0 */
	VBI_HOOK_CLOSED_CAPTION,
	/* uint8_t, 13, 0 */
	VBI_HOOK_VPS,
	VBI_HOOK_NUM
} vbi_decode_hook;

typedef void vbi_decode_hook_fn (vbi_decode_hook	hook,
				 void *			data,
				 unsigned int		size,
				 unsigned int		id);

struct decode_hook {
	struct decode_hook *	next;
	vbi_decode_hook_fn *	func;
};

/*
static __inline__ void
vbi_hook_call			(vbi_decoder *		vbi,
				 vbi_decode_hook	hook,
				 void *			data,
				 unsigned int		size,
				 unsigned int		id)
{
	struct decode_hook *h;

	for (h = vbi->decode_hooks[hook]; h; h = h->next)
		h->func (hook, data, size, id);
}
*/

struct page_clear {
	int			ci;		/* continuity index */
	int			packet;
	int			num_packets;
	int			bi;		/* block index */
	int			left;
	pfc_block		pfc;
};

struct vbi_decoder {
#if 0 /* obsolete */
	fifo			*source;
        pthread_t		mainloop_thread_id;
	int			quit;		/* XXX */
#endif
	double			time;

	pthread_mutex_t		chswcd_mutex;
        int                     chswcd;

	vbi_event		network;

	vbi_trigger *		triggers;

	pthread_mutex_t         prog_info_mutex;
	vbi_program_info        prog_info[2];
	int                     aspect_source;

	struct teletext		vt;
	struct caption		cc;

	cache *			cache;

	struct page_clear	epg_pc[2];

	/* preliminary */
	int			pageref;

	pthread_mutex_t		event_mutex;
	int			event_mask;
	struct event_handler *	handlers;
	struct event_handler *	next_handler;

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

	struct decode_hook *	decode_hooks[VBI_HOOK_NUM];
};

#ifndef VBI_DECODER
#define VBI_DECODER
/**
 * @ingroup Service
 * @brief Opaque VBI data service decoder object.
 *
 * Allocate with vbi_decoder_new().
 */
typedef struct vbi_decoder vbi_decoder;
#endif

/*
 *  vbi_page_type, the page identification codes,
 *  are derived from the MIP code scheme:
 *
 *  MIP 0x01 ... 0x51 -> 0x01 (subpages)
 *  MIP 0x70 ... 0x77 -> 0x70 (language)
 *  MIP 0x7B -> 0x7C (subpages)
 *  MIP 0x7E -> 0x7F (subpages)
 *  MIP 0x81 ... 0xD1 -> 0x81 (subpages)
 *  MIP reserved -> 0xFF (VBI_UNKNOWN_PAGE)
 *
 *  MIP 0x80 and 0xE0 ... 0xFE are not returned by
 *  vbi_classify_page().
 *
 *  TOP BTT mapping:
 *
 *  BTT 0 -> 0x00 (VBI_NOPAGE)
 *  BTT 1 -> 0x70 (VBI_SUBTITLE_PAGE)
 *  BTT 2 ... 3 -> 0x7F (VBI_PROGR_INDEX)
 *  BTT 4 ... 5 -> 0xFA (VBI_TOP_BLOCK -> VBI_NORMAL_PAGE) 
 *  BTT 6 ... 7 -> 0xFB (VBI_TOP_GROUP -> VBI_NORMAL_PAGE)
 *  BTT 8 ... 11 -> 0x01 (VBI_NORMAL_PAGE)
 *  BTT 12 ... 15 -> 0xFF (VBI_UNKNOWN_PAGE)
 *
 *  0xFA, 0xFB, 0xFF are reserved MIP codes used
 *  by libzvbi to identify TOP and unknown pages.
 */

/* Public */

/**
 * @ingroup Service
 * @brief Page classification.
 *
 * See vbi_classify_page().
 */
typedef enum {
	VBI_NO_PAGE = 0x00,
	VBI_NORMAL_PAGE = 0x01,
	VBI_SUBTITLE_PAGE = 0x70,
	VBI_SUBTITLE_INDEX = 0x78,
	VBI_NONSTD_SUBPAGES = 0x79,
	VBI_PROGR_WARNING = 0x7A,
	VBI_CURRENT_PROGR = 0x7C,
	VBI_NOW_AND_NEXT = 0x7D,
	VBI_PROGR_INDEX = 0x7F,
	VBI_PROGR_SCHEDULE = 0x81,
	VBI_UNKNOWN_PAGE = 0xFF,
/* Private */
#ifndef DOXYGEN_SHOULD_SKIP_THIS
	VBI_NOT_PUBLIC = 0x80,
	VBI_CA_DATA_BROADCAST =	0xE0,
	VBI_EPG_DATA = 0xE3,
	VBI_SYSTEM_PAGE = 0xE7,
	VBI_DISP_SYSTEM_PAGE = 0xF7,
	VBI_KEYWORD_SEARCH_LIST = 0xF9,
	VBI_TOP_BLOCK = 0xFA,
	VBI_TOP_GROUP = 0xFB,
	VBI_TRIGGER_DATA = 0xFC,
	VBI_ACI = 0xFD,
	VBI_TOP_PAGE = 0xFE
#endif
/* Public */
} vbi_page_type;

/**
 * @addtogroup Service
 * @{
 */
extern vbi_decoder *	vbi_decoder_new(void);
extern void		vbi_decoder_delete(vbi_decoder *vbi);
extern void		vbi_decode(vbi_decoder *vbi, vbi_sliced *sliced,
				   int lines, double timestamp);
extern void             vbi_channel_switched(vbi_decoder *vbi, vbi_nuid nuid);
extern const char **
vbi_cache_page_language		(vbi_decoder *		vbi,
				 vbi_pgno		pgno);
extern vbi_page_type	vbi_classify_page(vbi_decoder *vbi, vbi_pgno pgno,
					  vbi_subno *subno);
extern void		vbi_version(unsigned int *major, unsigned int *minor, unsigned int *micro);

typedef void vbi_log_fn		(const char *		function,
				 const char *		message,
				 void *			user_data);
extern void
vbi_set_log_fn			(vbi_log_fn *		function,
				 void *			user_data);
/** @} */

/* Private */

#define VBI_PAGE_PRIVATE_MAGIC 0x7540c4f2

typedef struct vbi_page_private {
	vbi_page		pg;

	unsigned int		magic;

	const vt_magazine *	mag;
	const vt_extension *	ext;

	const vt_page *		vtp;

	vbi_wst_level		max_level;
	vbi_bool		pdc_links;

	pdc_program		pdc_table [25];
	unsigned int		pdc_table_size;

	const vt_page *		drcs_vtp[32];

	struct vbi_font_descr *	font[2];

//	uint32_t		double_height_lower;	/* legacy */

	/* 0 header, 1 other rows. */
	vbi_opacity		page_opacity[2];
	vbi_opacity		boxed_opacity[2];

	/* Navigation related, see vbi_page_nav_link(). For
	   simplicity nav_index[] points from each character
	   in the TOP/FLOF row 25 (max 64 columns) to the
	   corresponding nav_link element. */
	vt_pagenum		nav_link[6];
	uint8_t			nav_index[64];
} vbi_page_private;

extern void
vbi_page_private_dump		(const vbi_page_private *pgp,
				 unsigned int		mode,
				 FILE *			fp);

/* teletext.c */
extern vbi_bool
vbi_format_vt_page_va_list	(vbi_decoder *		vbi,
				 vbi_page_private *	pgp,
				 const vt_page *	vtp,
				 va_list		format_options);
extern vbi_bool
vbi_format_vt_page		(vbi_decoder *		vbi,
				 vbi_page_private *	pgp,
				 const vt_page *	vtp,
				 ...);
extern unsigned int
vbi_page_character_set		(const vbi_page *	pg,
				 unsigned int		level);

extern pthread_once_t	vbi_init_once;
extern void		vbi_init(void);

extern void		vbi_transp_colormap	(vbi_decoder *		vbi,
						 vbi_rgba *		d,
						 const vbi_rgba *	s,
						 int			entries);
extern void             vbi_chsw_reset(vbi_decoder *vbi, vbi_nuid nuid);

extern void		vbi_asprintf(char **errstr, char *templ, ...);

#endif /* VBI_H */
