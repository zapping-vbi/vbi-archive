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

/* $Id: vt.h,v 1.4.2.6 2004-02-13 02:15:27 mschimek Exp $ */

#ifndef VT_H
#define VT_H

#include <string.h>
#include <inttypes.h>

#include "bcd.h"
#include "format.h"

#ifndef VBI_DECODER
#define VBI_DECODER
typedef struct vbi_decoder vbi_decoder;
#endif

/**
 * @internal
 *
 * Page function code according to ETS 300 706, Section 9.4.2,
 * Table 3: Page function and page coding bits
 * (packets X/28/0 Format 1, X/28/3 and X/28/4).
 */
typedef enum {
	PAGE_FUNCTION_EPG = -4,		/* libzvbi private */
	PAGE_FUNCTION_TRIGGER = -3,	/* libzvbi private */
	PAGE_FUNCTION_DISCARD = -2,	/* libzvbi private */
	PAGE_FUNCTION_UNKNOWN = -1,	/* libzvbi private */
	PAGE_FUNCTION_LOP,
	PAGE_FUNCTION_DATA_BROADCAST,
	PAGE_FUNCTION_GPOP,
	PAGE_FUNCTION_POP,
	PAGE_FUNCTION_GDRCS,
	PAGE_FUNCTION_DRCS,
	PAGE_FUNCTION_MOT,
	PAGE_FUNCTION_MIP,
	PAGE_FUNCTION_BTT,
	PAGE_FUNCTION_AIT,
	PAGE_FUNCTION_MPT,
	PAGE_FUNCTION_MPT_EX
} page_function;

extern const char *
page_function_name		(page_function		function);

/**
 * @internal
 *
 * Page coding code according to ETS 300 706, Section 9.4.2,
 * Table 3: Page function and page coding bits
 * (packets X/28/0 Format 1, X/28/3 and X/28/4).
 */
typedef enum {
	PAGE_CODING_UNKNOWN = -1,	/* libzvbi private */
	PAGE_CODING_PARITY,
	PAGE_CODING_BYTES,
	PAGE_CODING_TRIPLETS,
	PAGE_CODING_HAMMING84,
	PAGE_CODING_AIT,
	PAGE_CODING_META84
} page_coding;

/**
 * @internal
 *
 * DRCS character coding according to ETS 300 706, Section 14.2,
 * Table 31: DRCS modes; Section 9.4.6, Table 9: Coding of Packet
 * X/28/3 for DRCS Downloading Pages.
 */
typedef enum {
	DRCS_MODE_12_10_1,
	DRCS_MODE_12_10_2,
	DRCS_MODE_12_10_4,
	DRCS_MODE_6_5_4,
	DRCS_MODE_SUBSEQUENT_PTU = 14,
	DRCS_MODE_NO_DATA
} drcs_mode;

/* Level one page extension */

typedef struct _ext_fallback ext_fallback;

struct _ext_fallback {
	int		black_bg_substitution;
	int		left_side_panel;
	int		right_side_panel;
	int		left_panel_columns;
};

#define VBI_TRANSPARENT_BLACK 8

typedef struct _vt_extension vt_extension;

struct _vt_extension {
	unsigned int	designations;

	int		char_set[2];		/* primary, secondary */

	int		def_screen_color;
	int		def_row_color;

	int		foreground_clut;	/* 0, 8, 16, 24 */
	int		background_clut;

	ext_fallback	fallback;

	/** f/b, dclut4, dclut16; see also vbi_page. */
	vbi_color	drcs_clut[2 * 1 + 2 * 4 + 2 * 16];

	vbi_rgba	color_map[40];
};

typedef struct _vt_triplet vt_triplet;

/**
 * @internal
 *
 * Packet X/26 code triplet according to ETS 300 706,
 * Section 12.3.1.
 */
struct _vt_triplet {
	unsigned	address		: 8;
	unsigned	mode		: 8;
	unsigned	data		: 8;
} __attribute__ ((packed));

typedef struct _vt_pagenum vt_pagenum;

struct _vt_pagenum {
	unsigned int		type; // XXX enum
	vbi_pgno		pgno;
	vbi_subno		subno;
};

typedef struct _ait_entry ait_entry;

struct _ait_entry {
	vt_pagenum		page;
	uint8_t			text[12];
};

typedef vt_triplet enhancement[16 * 13 + 1];

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
	vt_pagenum		link[6 * 6];	/* X/27/0-5 links */
	vbi_bool		flof;
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
	 * Page flags C4_ERASE_PAGE ... C11_MAGAZIN_SERIAL.
	 */
	int			flags;

	/**
	 * One bit for each LOP and enhancement packet
	 */
	int			lop_lines;
	int			enh_lines;

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
			vt_extension	ext;
		}		ext_lop;
		struct {
			uint16_t	pointer[96];
			vt_triplet	triplet[39 * 13 + 1];
/* XXX preset [+1] mode (not 0xFF) or catch */
		}		gpop, pop;
		struct {
			uint8_t			raw[26][40];
			uint8_t			bits[48][12 * 10 / 2];
			uint8_t			mode[48];
			uint64_t		invalid;
		}		gdrcs, drcs;

		ait_entry	ait[46];

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
static __inline__ unsigned int
vtp_size			(const vt_page *	vtp)
{
	const unsigned int header_size = sizeof(*vtp) - sizeof(vtp->data);

	switch (vtp->function) {
	case PAGE_FUNCTION_UNKNOWN:
	case PAGE_FUNCTION_LOP:
		if (vtp->data.lop.ext)
			return header_size + sizeof(vtp->data.ext_lop);
		else if (vtp->enh_lines)
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

static __inline__ vt_page *
copy_vt_page			(vt_page *		tvtp,
				 const vt_page *	svtp)
{
	memcpy (tvtp, svtp, vtp_size (svtp));
	return tvtp;
}

/**
 * @internal
 *
 * TOP BTT page class.
 */
typedef enum {
	BTT_NO_PAGE = 0,
	BTT_SUBTITLE,
	BTT_PROGR_INDEX_S,
	BTT_PROGR_INDEX_M,
	BTT_BLOCK_S,
	BTT_BLOCK_M,
	BTT_GROUP_S,
	BTT_GROUP_M,
	BTT_NORMAL_S,
	BTT_NORMAL_9, /* ? */
	BTT_NORMAL_M,
	BTT_NORMAL_11 /* ? */
	/* 12 ... 15 ? */
} btt_page_class;

/**
 * @internal
 *
 * Enhancement object type according to ETS 300 706, Section 12.3.1,
 * Table 28: Function of Row Address triplets.
 */
typedef enum {
	LOCAL_ENHANCEMENT_DATA = 0,
	OBJECT_TYPE_NONE = 0,
	OBJECT_TYPE_ACTIVE,
	OBJECT_TYPE_ADAPTIVE,
	OBJECT_TYPE_PASSIVE
} object_type;

extern const char *
object_type_name		(object_type		type);

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

typedef struct _vt_pop_link vt_pop_link;

struct _vt_pop_link {
	vbi_pgno		pgno;
	ext_fallback		fallback;
	struct {
		object_type		type;
		object_address		address;
	}			default_obj[2];
};

typedef struct _vt_magazine vt_magazine;

struct _vt_magazine {
	vt_extension		extension;

	uint8_t			pop_lut[256];
	uint8_t			drcs_lut[256];

    	vt_pop_link		pop_link[16];
	vbi_pgno		drcs_link[16];
};

struct raw_page {
	vt_page			page[1];
        uint8_t		        drcs_mode[48];
	int			num_triplets;
	int			___ait_page; // XXX unused??
};

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

struct page_info {
	unsigned 		code		: 8;
	unsigned		language	: 8;
	unsigned 		subcode		: 16;
};

struct teletext {
	vbi_wst_level		max_level;

	vt_pagenum		header_page;
	uint8_t			header[40];

        vt_pagenum		initial_page;
	vt_magazine		magazine[9];	/* 1 ... 8; #0 unmodified level 1.5 default */

	int			region;

	struct page_info	page_info[0x800];

	vt_pagenum		btt_link[15];
	vbi_bool		top;		/* use top navigation, flof overrides */

	struct raw_page		raw_page[8];
	struct raw_page		*current;
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
	VBI_END = 0,
	/**
	 * Format only the first row.
	 */
	VBI_HEADER_ONLY,
	/**
	 * Add an artificial 41st column. Often column 0 of a page
	 * contains all black spaces, unlike column 39. This will
	 * result in a more balanced view.
	 */
	VBI_41_COLUMNS,
	/**
	 * Enable TOP or FLOF navigation in row 25.
	 */
	VBI_NAVIGATION,
	/**
	 * Scan the page for page numbers, URLs, e-mail addresses
	 * etc. and create hyperlinks.
	 */
	VBI_HYPERLINKS,
	/**
	 * Scan the page for PDC Method A/B preselection data
	 * and create a PDC table and links.
	 */
	VBI_PDC_LINKS,
	/**
	 * Format the page at the given Teletext implementation level.
	 * Parameter: vbi_wst_level.
	 */
	VBI_WST_LEVEL,
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

extern vbi_page *	vbi_fetch_vt_page(vbi_decoder *vbi,
		  		vbi_pgno		pgno,
				vbi_subno		subno,
					  ...);
extern vbi_page *
vbi_fetch_vt_page_va (vbi_decoder *vbi,
		  		vbi_pgno		pgno,
				vbi_subno		subno,
		      va_list ap);

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
static_inline void
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
extern vbi_bool		vbi_decode_teletext(vbi_decoder *vbi, uint8_t *p);
extern void		vbi_teletext_desync(vbi_decoder *vbi);
extern void             vbi_teletext_channel_switched(vbi_decoder *vbi);
extern vbi_bool		vbi_convert_cached_page	(vbi_decoder *		vbi,
						 const vt_page **	vtpp,
						 page_function		new_function);


#endif
