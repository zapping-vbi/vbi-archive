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

/* $Id: vbi.h,v 1.5.2.10 2004-04-08 23:36:26 mschimek Exp $ */

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
#include "lang.h"

struct event_handler {
	struct event_handler *	next;
	int			event_mask;
	vbi_event_handler	handler;
	void *			user_data;
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

	vbi_teletext_decoder	vt;
	struct caption		cc;

	cache *			cache;

#if 0 // TODO
	struct page_clear	epg_pc[2];
#endif
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


extern const char *
_vbi_page_type_name		(vbi_page_type		type);

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

/**
 * @ingroup Service
 * @brief Teletext implementation level.
 */
typedef enum {
	VBI_WST_LEVEL_1,   /**< 1 - Basic Teletext pages */
	VBI_WST_LEVEL_1p5, /**< 1.5 - Additional national and graphics characters */
	/**
	 * 2.5 - Additional text styles, more colors and DRCS. You should
	 * enable Level 2.5 only if you can render and/or export such pages.
	 */
	VBI_WST_LEVEL_2p5,
	VBI_WST_LEVEL_3p5  /**< 3.5 - Multicolor DRCS, proportional script */
} vbi_wst_level;

typedef struct vbi_page_private {
	vbi_page		pg;

	unsigned int		magic;

	const magazine *	mag;
	const extension *	ext;

	const vt_page *		vtp;

	vbi_wst_level		max_level;
	vbi_bool		pdc_links;

	vbi_preselection *	pdc_table;
	unsigned int		pdc_table_size;

	const vt_page *		drcs_vtp[32];

	const vbi_character_set *char_set[2];

//	uint32_t		double_height_lower;	/* legacy */

	/* 0 header, 1 other rows. */
	vbi_opacity		page_opacity[2];
	vbi_opacity		boxed_opacity[2];

	/* Navigation related, see vbi_page_nav_link(). For
	   simplicity nav_index[] points from each character
	   in the TOP/FLOF row 25 (max 64 columns) to the
	   corresponding nav_link element. */
	pagenum		nav_link[6];
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
extern const vbi_character_set *
vbi_page_character_set		(const vbi_page *	pg,
				 unsigned int		level);

//extern pthread_once_t	vbi_init_once;
extern void
vbi_init			(void)
     __attribute__ ((constructor));

extern void		vbi_transp_colormap	(vbi_decoder *		vbi,
						 vbi_rgba *		d,
						 const vbi_rgba *	s,
						 int			entries);
extern void             vbi_chsw_reset(vbi_decoder *vbi, vbi_nuid nuid);

extern void		vbi_asprintf(char **errstr, char *templ, ...);


#include <stdarg.h>



/**
 * @addtogroup Cache
 * @{
 */

typedef enum {
	/**
	 * Format only the first row.
	 * Parameter: vbi_bool.
	 */
	VBI_HEADER_ONLY = 0x37138F00,
	/**
	 * Often column 0 of a page contains all black spaces,
	 * unlike column 39. Enabling this option will result in
	 * a more balanced view.
	 * Parameter: vbi_bool.
	 */
	VBI_41_COLUMNS,
	/**
	 * Enable TOP or FLOF navigation in row 25.
	 * Parameter: vbi_bool.
	 */
	VBI_NAVIGATION,
	/**
	 * Scan the page for page numbers, URLs, e-mail addresses
	 * etc. and create hyperlinks.
	 * Parameter: vbi_bool.
	 */
	VBI_HYPERLINKS,
	/**
	 * Scan the page for PDC Method A/B preselection data
	 * and create a PDC table and links.
	 * Parameter: vbi_bool.
	 */
	VBI_PDC_LINKS,
	/**
	 * Format the page at the given Teletext implementation level.
	 * Parameter: vbi_wst_level.
	 */
	VBI_WST_LEVEL,
	/**
	 * Parameter: vbi_character_set_code.
	 */
	VBI_CHAR_SET_DEFAULT,
	/**
	 * Parameter: vbi_character_set_code.
	 */
	VBI_CHAR_SET_OVERRIDE,
} vbi_format_option;

extern vbi_page *
vbi_page_new			(void);
extern void
vbi_page_delete			(vbi_page *		pg);
extern vbi_page *
vbi_page_copy			(const vbi_page *	pg);
extern const uint8_t *
vbi_page_drcs_data		(const vbi_page *	pg,
				 unsigned int		unicode);

extern vbi_page *
vbi_fetch_vt_page		(vbi_decoder *		vbi,
				 vbi_pgno		pgno,
				 vbi_subno		subno,
				 ...);
extern vbi_page *
vbi_fetch_vt_page_va_list	(vbi_decoder *		vbi,
				 vbi_pgno		pgno,
				 vbi_subno		subno,
				 va_list		format_options);

extern int		vbi_page_title(vbi_decoder *vbi, int pgno, int subno, char *buf);
/** @} */
/**
 * @addtogroup Event
 * @{
 */
extern vbi_bool
vbi_page_hyperlink		(const vbi_page *	pg,
				 vbi_link *		ld,
				 unsigned int		column,
				 unsigned int		row);
extern vbi_bool
vbi_page_nav_enum		(const vbi_page *	pg,
				 vbi_link *		ld,
				 unsigned int		index);
vbi_inline void
vbi_page_home_link		(const vbi_page *	pg,
				 vbi_link *		ld)
{
	vbi_page_nav_enum (pg, ld, 5);
}

/** @} */

#endif /* VBI_H */
