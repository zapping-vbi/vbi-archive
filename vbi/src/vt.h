/*
 *  libzvbi - Teletext decoder
 *
 *  Copyright (C) 2000, 2001 Michael H. Schimek
 *
 *  Based on code from AleVT 1.5.1
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

/* $Id: vt.h,v 1.4.2.10 2004-04-03 00:07:55 mschimek Exp $ */

#ifndef VT_H
#define VT_H

#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "bcd.h"
#include "format.h"
#include "lang.h"

#ifndef VBI_DECODER
#define VBI_DECODER
typedef struct vbi_decoder vbi_decoder;
#endif

/**
 * @internal
 * EN 300 706, Section 9.4.2, Table 3 Page function.
 */
typedef enum {
	PAGE_FUNCTION_ACI		= -4,	/* libzvbi private */
	PAGE_FUNCTION_EPG		= -3,	/* libzvbi private */
	PAGE_FUNCTION_DISCARD		= -2,	/* libzvbi private */
	PAGE_FUNCTION_UNKNOWN		= -1,	/* libzvbi private */
	PAGE_FUNCTION_LOP		= 0,
	PAGE_FUNCTION_DATA,
	PAGE_FUNCTION_GPOP,
	PAGE_FUNCTION_POP,
	PAGE_FUNCTION_GDRCS,
	PAGE_FUNCTION_DRCS,
	PAGE_FUNCTION_MOT,
	PAGE_FUNCTION_MIP,
	PAGE_FUNCTION_BTT,
	PAGE_FUNCTION_AIT,
	PAGE_FUNCTION_MPT,
	PAGE_FUNCTION_MPT_EX,
	PAGE_FUNCTION_TRIGGER
} page_function;

extern const char *
page_function_name		(page_function		function);

/**
 * @internal
 * TOP BTT links to other TOP pages.
 * top_page_number() translates this to enum page_function.
 */
typedef enum {
	TOP_PAGE_FUNCTION_MPT = 1,
	TOP_PAGE_FUNCTION_AIT,
	TOP_PAGE_FUNCTION_MPT_EX,
} top_page_function;

/**
 * @internal
 * 9.4.2 Table 3 Page coding.
 */
typedef enum {
	PAGE_CODING_UNKNOWN = -1,	/**< libzvbi private */
	PAGE_CODING_ODD_PARITY,
	PAGE_CODING_UBYTES,
	PAGE_CODING_TRIPLETS,
	PAGE_CODING_HAMMING84,
	PAGE_CODING_AIT,
	/**
	 * First byte is a hamming 8/4 coded page_coding,
	 * describing remaining 39 bytes.
	 */
	PAGE_CODING_META84
} page_coding;

extern const char *
page_coding_name		(page_coding		coding);

/**
 * @internal
 * TOP BTT page type.
 * decode_btt_page() translates this to MIP page type
 * which is defined as enum vbi_page_type in vbi.h.
 */
typedef enum {
	BTT_NO_PAGE = 0,
	BTT_SUBTITLE,
	/** S = single page */
	BTT_PROGR_INDEX_S,
	/** M = multi-page (number of subpages in MPT or MPT-EX) */
	BTT_PROGR_INDEX_M,
	BTT_BLOCK_S,
	BTT_BLOCK_M,
	BTT_GROUP_S,
	BTT_GROUP_M,
	BTT_NORMAL_S,
	BTT_NORMAL_9,		/**< ? */
	BTT_NORMAL_M,
	BTT_NORMAL_11,		/**< ? */
	BTT_12,			/**< ? */
	BTT_13,			/**< ? */
	BTT_14,			/**< ? */
	BTT_15			/**< ? */
} btt_page_type;

/**
 * @internal
 * 12.3.1 Table 28 Enhancement object type.
 */
typedef enum {
	LOCAL_ENHANCEMENT_DATA = 0,	/**< depends on context */
	OBJECT_TYPE_NONE = 0,
	OBJECT_TYPE_ACTIVE,
	OBJECT_TYPE_ADAPTIVE,
	OBJECT_TYPE_PASSIVE
} object_type;

extern const char *
object_type_name		(object_type		type);

/**
 * @internal
 * 14.2 Table 31, 9.4.6 Table 9 DRCS character coding.
 */
typedef enum {
	DRCS_MODE_12_10_1,
	DRCS_MODE_12_10_2,
	DRCS_MODE_12_10_4,
	DRCS_MODE_6_5_4,
	DRCS_MODE_SUBSEQUENT_PTU = 14,
	DRCS_MODE_NO_DATA
} drcs_mode;

extern const char *
drcs_mode_name			(drcs_mode		mode);

/** @internal */
#define DRCS_PTUS_PER_PAGE 48

/**
 * @internal
 */
typedef struct {
	page_function		function;

	/** NO_PAGE (pgno) when unused or broken. */
	vbi_pgno		pgno;

	/**
	 * X/27/4 ... 5 format 1 (struct lop.link[]):
	 * Set of subpages required: 1 << (0 ... 15).
	 * Otherwise subpage number or VBI_NO_SUBNO.
	 */
	vbi_subno		subno;
} pagenum;

/** @internal */
#define NO_PAGE(pgno) (((pgno) & 0xFF) == 0xFF)

extern void
pagenum_dump			(const pagenum *	pn,
				 FILE *			fp);

/**
 * @internal
 * 12.3.1 Packet X/26 code triplet.
 * Broken triplets are set to -1, -1, -1.
 */
struct _triplet {
	unsigned	address		: 8;
	unsigned	mode		: 8;
	unsigned	data		: 8;
} __attribute__ ((packed));

/** @internal */
typedef struct _triplet triplet;

/* Level one page extension */

typedef struct {
	int		black_bg_substitution;
	int		left_panel_columns;
	int		right_panel_columns;
} ext_fallback;

#define VBI_TRANSPARENT_BLACK 8

typedef struct {
	unsigned int		designations;

	vbi_character_set_code	charset_code[2]; /* primary, secondary */
	vbi_bool		charset_valid;

	int			def_screen_color;
	int			def_row_color;

	int			foreground_clut; /* 0, 8, 16, 24 */
	int			background_clut;

	ext_fallback		fallback;

	/** f/b, dclut4, dclut16; see also vbi_page. */
	vbi_color		drcs_clut[2 * 1 + 2 * 4 + 2 * 16];

	vbi_rgba		color_map[40];
} extension;

extern void
extension_dump			(const extension *	ext,
				 FILE *			fp);

typedef struct {
	pagenum			page;
	uint8_t			text[12];
} ait_title;

typedef triplet enhancement[16 * 13 + 1];

#define NO_PAGE(pgno) (((pgno) & 0xFF) == 0xFF)

/*                              0xE03F7F    nat. char. subset and sub-page */
#define C4_ERASE_PAGE		0x000080 /* erase previously stored packets */
#define C5_NEWSFLASH		0x004000 /* box and overlay */
#define C6_SUBTITLE		0x008000 /* box and overlay */
#define C7_SUPPRESS_HEADER	0x010000 /* row 0 not to be displayed */
#define C8_UPDATE		0x020000
#define C9_INTERRUPTED		0x040000
#define C10_INHIBIT_DISPLAY	0x080000 /* rows 1-24 not to be displayed */
#define C11_MAGAZINE_SERIAL	0x100000

/* Level one page */

struct lop {
	uint8_t			raw[26][40];
	pagenum		link[6 * 6];	/* X/27/0-5 links */
	vbi_bool		have_flof;
	vbi_bool		ext;
};

typedef struct _vt_page vt_page;

/**
 * @internal
 *
 * This structure holds a raw Teletext page as decoded by
 * vbi_teletext_packet(), stored in the Teletext page cache, and
 * formatted by vbi_format_vt_page() creating a vbi_page. It is
 * thus not directly accessible by the client. Note the size
 * (of the union) will vary in order to save cache memory.
 **/
struct _vt_page {
	/**
	 * Defines the page function and which member of the
	 * union applies.
	 */ 
	page_function		function;

	/**
	 * Page and subpage number.
	 */
	vbi_pgno		pgno;
	vbi_subno		subno;

	/**
	 * National character set designator 0 ... 7.
	 */
	int			national;

	/**
	 * Page flags C4 ... C14. Other bits will be set, just ignore them.
	 */
	unsigned int		flags;

	/**
	 * Sets of packets we received. This may include packets
	 * with hamming errors.
	 *
	 * lop_packets:	1 << packet 0 ... 25
	 * x26_designations: 1 << X/26 designation 0 ... 15
	 */
	unsigned int		lop_packets;
	unsigned int		x26_designations;
	unsigned int		x27_designations;
	unsigned int		x28_designations;

	union {
		struct lop	unknown;
		struct lop	lop;
		struct {
			struct lop	lop;
			enhancement	enh;
		}		enh_lop;
		struct {
			struct lop	lop;
			enhancement	enh;
			extension	ext;
		}		ext_lop;
		struct {
			/**
			 * 12 * 2 triplet pointers from packet 1 ... 4.
			 * Valid range 0 ... 506 (39 packets * 13 triplets),
			 * unused pointers 511 (10.5.1.2), broken -1.
			 */
			uint16_t	pointer[4 * 12 * 2];

			/**
			 * 13 triplets from each of packet 3 ... 25 and
			 * 26/0 ... 26/15.
			 *
			 * Valid range of mode 0x00 ... 0x1F, broken -1.
			 */
		  	triplet			triplet[39 * 13 + 1];
		}		gpop, pop;
		struct {
			struct lop		lop;
			uint8_t			chars[48][12 * 10 / 2];
			uint8_t			mode[48];
			uint64_t		invalid;
		}		gdrcs, drcs;
		struct {
			ait_title	title[46];

			/** Used to detect changes. */
			unsigned int	checksum;
		}		ait;

	}		data;

	/* 
	 *  Dynamic size, add no fields below unless
	 *  vt_page is statically allocated.
	 */
};

/**
 * @internal
 * @param vtp Teletext page in question.
 * 
 * @return Storage size required for the raw Teletext page,
 * depending on its function and the data union member used.
 **/
vbi_inline unsigned int
vt_page_size			(const vt_page *	vtp)
{
	const unsigned int header_size = sizeof(*vtp) - sizeof(vtp->data);

	switch (vtp->function) {
	case PAGE_FUNCTION_UNKNOWN:
	case PAGE_FUNCTION_LOP:
		if (vtp->data.lop.ext)
			return header_size + sizeof(vtp->data.ext_lop);
		else if (vtp->x26_designations)
			return header_size + sizeof(vtp->data.enh_lop);
		else
			return header_size + sizeof(vtp->data.lop);

	case PAGE_FUNCTION_GPOP:
	case PAGE_FUNCTION_POP:
		return header_size + sizeof(vtp->data.pop);

	case PAGE_FUNCTION_GDRCS:
	case PAGE_FUNCTION_DRCS:
		return header_size + sizeof(vtp->data.drcs);

	case PAGE_FUNCTION_AIT:
		return header_size + sizeof(vtp->data.ait);

	default:
		return sizeof(*vtp);
	}
}

vbi_inline vt_page *
vt_page_copy			(vt_page *		tvtp,
				 const vt_page *	svtp)
{
	memcpy (tvtp, svtp, vt_page_size (svtp));
	return tvtp;
}


/**
 * @internal
 *
 * MOT default, POP and GPOP object address.
 *
 * n8  n7  n6  n5  n4  n3  n2  n1  n0
 * packet  triplet lsb ----- s1 -----
 *
 * According to ETS 300 706, Section 12.3.1, Table 28
 * (under Mode 10001 - Object Invocation ff.)
 */
typedef int object_address;

typedef struct {
	vbi_pgno		pgno;
	ext_fallback		fallback;
	struct {
		object_type		type;
		object_address		address;
	}			default_obj[2];
} pop_link;

typedef struct {
	extension		extension;

	uint8_t			pop_lut[256];
	uint8_t			drcs_lut[256];

    	pop_link		pop_link[2][8];
	vbi_pgno		drcs_link[2][8];
} magazine;

/* Public */

#include <stdarg.h>

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

/* Private */

#define SUBCODE_SINGLE_PAGE 0x0000
#define SUBCODE_MULTI_PAGE 0xFFFE
#define SUBCODE_UNKNOWN 0xFFFF

typedef struct {
	/* Information gathered from MOT, MIP, BTT, G/POP pages. */

	/** vbi_page_type. */
	uint8_t			page_type;

	/** vbi_character_set_code, 0xFF unknown. */
  	uint8_t			charset_code;

	/**
	 * Highest subpage number transmitted.
	 * - 0x0000		single page
	 * - 0x0002 - 0x0099	multi-page
	 * - 0x0100 - 0x3F7F	clock page?
	 * - 0xFFFE		has 2+ subpages (libzvbi)
	 * - 0xFFFF		unknown (libzvbi)
	 */
	uint16_t 		subcode;

	/* Cache statistics. */

	/** Subpages cached now and ever. */
	uint8_t			n_subpages;
	uint8_t			max_subpages;

	/** Subpage numbers encountered (0x00 ... 0x99). */
	uint8_t			subno_min;
	uint8_t			subno_max;
} page_stat;

typedef struct {
        pagenum			initial_page;
	magazine		magazines[9];	/* 1 ... 8; #0 unmodified level 1.5 default */
	page_stat		pages[0x800];

	pagenum			btt_link[2 * 5];
	vbi_bool		have_top;		/* use top navigation, flof overrides */
} vt_network;

/** @internal */
vbi_inline magazine *
vt_network_magazine		(vt_network *		vtn,
				 vbi_pgno		pgno)
{
	assert (pgno >= 0x100 && pgno <= 0x8FF);
	return vtn->magazines + (pgno >> 8);
}

/** @internal */
vbi_inline page_stat *
vt_network_page_stat		(vt_network *		vtn,
				 vbi_pgno		pgno)
{
	assert (pgno >= 0x100 && pgno <= 0x8FF);
	return vtn->pages - 0x100 + pgno;
}

typedef struct _vbi_teletext_decoder vbi_teletext_decoder;

struct _vbi_teletext_decoder {
//	vbi_wst_level		max_level;

	vt_network		network[1];

	pagenum			header_page;
	uint8_t			header[40];

//	int			region;

	vt_page			buffer[8];
	vt_page	*		current;
};

/* Public */

/**
 * @addtogroup Service
 * @{
 */
extern void		vbi_teletext_set_default_region(vbi_decoder *vbi, int default_region);
extern void		vbi_teletext_set_level(vbi_decoder *vbi, int level);
/** @} */


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

/* Private */

/* packet.c */

extern void		vbi_teletext_init(vbi_decoder *vbi);
extern void		vbi_teletext_destroy(vbi_decoder *vbi);
extern vbi_bool		vbi_decode_teletext(vbi_decoder *vbi, 		    const uint8_t buffer[42]);
extern void		vbi_teletext_desync(vbi_decoder *vbi);
extern void             vbi_teletext_channel_switched(vbi_decoder *vbi);
extern vbi_bool		_vbi_convert_cached_page	(vbi_decoder *		vbi,
						 const vt_page **	vtpp,
						 page_function		new_function);


#endif
