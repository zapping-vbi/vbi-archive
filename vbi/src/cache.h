/*
 *  libzvbi - Network cache
 *
 *  Copyright (C) 2001-2003 Michael H. Schimek
 *
 *  Based on code from AleVT 1.5.1
 *  Copyright (C) 1998, 1999 Edgar Toernig
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

/* $Id: cache.h,v 1.2.2.11 2004-07-09 16:10:52 mschimek Exp $ */

#ifndef __ZVBI_CACHE_H__
#define __ZVBI_CACHE_H__

#include <stdarg.h>
#include "network.h"
#include "format.h"

VBI_BEGIN_DECLS

/* in format.h */
/* typedef struct _vbi_cache vbi_cache; */

/**
 * @brief Teletext page type.
 *
 * Some networks provide information to classify Teletext pages,
 * this can be used for example to automatically find program schedule
 * and subtitle pages. See also x().
 */
typedef enum {
	VBI_NO_PAGE		= 0x00,
	VBI_NORMAL_PAGE		= 0x01,

	VBI_TOP_BLOCK		= 0x60,		/* libzvbi private */
	VBI_TOP_GROUP		= 0x61,		/* libzvbi private */
	VBI_NEWSFLASH_PAGE	= 0x62,		/* libzvbi private */

	VBI_SUBTITLE_PAGE	= 0x70,
	VBI_SUBTITLE_INDEX	= 0x78,
	VBI_NONSTD_SUBPAGES	= 0x79,
	VBI_PROGR_WARNING	= 0x7A,
	VBI_CURRENT_PROGR	= 0x7C,
	VBI_NOW_AND_NEXT	= 0x7D,
	VBI_PROGR_INDEX		= 0x7F,
	VBI_NOT_PUBLIC		= 0x80,
	VBI_PROGR_SCHEDULE	= 0x81,
	VBI_CA_DATA		= 0xE0,
	VBI_PFC_EPG_DATA	= 0xE3,
	VBI_PFC_DATA		= 0xE4,
	VBI_DRCS_PAGE		= 0xE5,
	VBI_POP_PAGE		= 0xE6,
	VBI_SYSTEM_PAGE		= 0xE7,
	VBI_KEYWORD_SEARCH_LIST = 0xF9,
	VBI_TRIGGER_DATA	= 0xFC,
	VBI_ACI_PAGE		= 0xFD,
	VBI_TOP_PAGE		= 0xFE,		/* MPT, AIT, MPT-EX */

	VBI_UNKNOWN_PAGE	= 0xFF,		/* libzvbi private */
} vbi_ttx_page_type;

/* in packet.c */
extern const char *
vbi_ttx_page_type_name		(vbi_ttx_page_type	type);

typedef struct {
	/** */
	vbi_ttx_page_type	page_type;
	/** */
	vbi_character_set_code	charset_code;
	/** Expected number of subpages: 0 or 2 ... 79. */
	unsigned int		subpages;
	/** Lowest subno received. */
	vbi_subno		subno_min;
	/** Highest subno received. */
	vbi_subno		subno_max;
} vbi_ttx_page_stat;

extern void
vbi_ttx_page_stat_destroy	(vbi_ttx_page_stat *	ps);
extern void
vbi_ttx_page_stat_init		(vbi_ttx_page_stat *	ps);
extern vbi_bool
vbi_cache_get_ttx_page_stat	(vbi_cache *		ca,
				 vbi_ttx_page_stat *	ps,
				 const vbi_network *	nk,
				 vbi_pgno		pgno);

/**
 */
typedef struct {
	/** */
	char *			title;
	/** */
	vbi_pgno		pgno;
	/** */
	vbi_subno		subno;
	/** */
	vbi_bool		group;
	int			reserved[2];
} vbi_top_title;

/* in top.c */
extern void
vbi_top_title_destroy		(vbi_top_title *	tt);
extern vbi_bool
vbi_top_title_copy		(vbi_top_title *	dst,
				 const vbi_top_title *	src);
extern void
vbi_top_title_init		(vbi_top_title *	tt);
extern void
vbi_top_title_array_delete	(vbi_top_title *	tt,
				 unsigned int		tt_size);
extern vbi_bool
vbi_cache_get_top_title		(vbi_cache *		ca,
				 vbi_top_title *	tt,
				 const vbi_network *	nk,
				 vbi_pgno		pgno,
				 vbi_subno		subno);
extern vbi_top_title *
vbi_cache_get_top_titles	(vbi_cache *		ca,
				 const vbi_network *	nk,
				 unsigned int *		array_size);

/**
 * Values for the vbi_format_option @c VBI_WST_LEVEL.
 */
typedef enum {
	/**
	 * Level 1 - Basic Teletext pages. All pages can be formatted
	 * like this since networks transmit Level 1 data as fallback
	 * for older Teletext decoders.
	 */
	VBI_WST_LEVEL_1,
	/**
	 * Level 1.5 - Additional national and graphics characters.
	 */
	VBI_WST_LEVEL_1p5,
	/**
	 * Level 2.5 - Additional text styles, more colors, DRCS, side
	 * panels. You should enable Level 2.5 only if you can render
	 * and/or export such pages.
	 */
	VBI_WST_LEVEL_2p5,
	/**
	 * Level 3.5 - Multicolor DRCS, proportional script.
	 */
	VBI_WST_LEVEL_3p5
} vbi_wst_level;

typedef enum {
	/**
	 * Format only the first row.
	 * Parameter: vbi_bool, default FALSE.
	 */
	/* We use random numbering for safety because these
	   values are used in variable function arguments. */
	VBI_HEADER_ONLY = 0x37138F00,
	/**
	 * Often column 0 of a page contains all black spaces,
	 * unlike column 39. Enabling this option will result in
	 * a more balanced view.
	 * Parameter: vbi_bool, default FALSE.
	 */
	VBI_41_COLUMNS,
	/**
	 * Not implemented yet.
	 * Format the page with side panels if it has any. This
	 * option takes precedence over VBI_41_COLUMNS if side panels
	 * are present.
	 * Parameter: vbi_bool, default FALSE.
	 */
	VBI_PANELS,
	/**
	 * Enable TOP or FLOF navigation in row 25.
	 * - 0 disable
	 * - 1 FLOF or TOP style 1
	 * - 2 FLOF or TOP style 2
	 * Parameter: int, default 0.
	 */
	VBI_NAVIGATION,
	/**
	 * Scan the page for page numbers, URLs, e-mail addresses
	 * etc. and create hyperlinks.
	 * Parameter: vbi_bool, default FALSE.
	 */
	VBI_HYPERLINKS,
	/**
	 * Scan the page for PDC Method A/B preselection data
	 * and create a PDC table and links.
	 * Parameter: vbi_bool, default FALSE.
	 */
	VBI_PDC_LINKS,
	/**
	 * Format the page at the given Teletext implementation level.
	 * Parameter: vbi_wst_level, default VBI_WST_LEVEL_1.
	 */
	VBI_WST_LEVEL,
	/**
	 * The default character set code. Codes transmitted by the
	 * network take precedence. When the network transmits only the
	 * three last significant bits, this value provides the higher
	 * bits, or if this yields no valid code all bits.
	 * Parameter: vbi_character_set_code, default 0 (English).
	 */
	VBI_CHAR_SET_DEFAULT,
	/**
	 * Overrides the primary and secondary character set code of a
	 * page. This takes precedence over VBI_CHAR_SET_DEFAULT and any
	 * code transmitted by the network.
	 * Parameter: vbi_character_set_code, default is transmitted value.
	 */
	VBI_CHAR_SET_OVERRIDE,
} vbi_format_option;

/* in teletext.c */
extern vbi_page *
vbi_cache_get_teletext_page_va_list
				(vbi_cache *		ca,
				 const vbi_network *	nk,
				 vbi_pgno		pgno,
				 vbi_subno		subno,
				 va_list		format_options);
extern vbi_page *
vbi_cache_get_teletext_page	(vbi_cache *		ca,
				 const vbi_network *	nk,
				 vbi_pgno		pgno,
				 vbi_subno		subno,
				 ...);
/* in cache.c */
extern vbi_network *
vbi_cache_get_networks		(vbi_cache *		ca,
				 unsigned int *		array_size);
extern void
vbi_cache_release		(vbi_cache *		ca);
extern vbi_cache *
vbi_cache_new_ref		(vbi_cache *		ca);
extern void
vbi_cache_delete		(vbi_cache *		ca);
extern vbi_cache *
vbi_cache_new			(void) vbi_alloc;

VBI_END_DECLS

#endif /* __ZVBI_CACHE_H__ */
