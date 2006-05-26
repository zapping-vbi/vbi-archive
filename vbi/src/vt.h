/*
 *  libzvbi - Teletext decoder
 *
 *  Copyright (C) 2000, 2001, 2003, 2004 Michael H. Schimek
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

/* $Id: vt.h,v 1.4.2.20 2006-05-26 00:43:06 mschimek Exp $ */

#ifndef VT_H
#define VT_H

#include "bcd.h"		/* vbi3_pgno, vbi3_subno */
#include "lang.h"		/* vbi3_ttx_charset_code */
#include "page.h"		/* vbi3_color */

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
 * which is defined as enum vbi3_page_type in vbi.h.
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

/** @internal */
typedef struct {
	page_function		function;

	/** NO_PAGE (pgno) when unused or broken. */
	vbi3_pgno		pgno;

	/**
	 * X/27/4 ... 5 format 1 (struct lop.link[]):
	 * Set of subpages required: 1 << (0 ... 15).
	 * Otherwise subpage number or VBI3_NO_SUBNO.
	 */
	vbi3_subno		subno;
} pagenum;

/** @internal */
#define NO_PAGE(pgno) (((pgno) & 0xFF) == 0xFF)

extern void
pagenum_dump			(const pagenum *	pn,
				 FILE *			fp);

/* Level one page enhancement */

/**
 * @internal
 * 12.3.1 Packet X/26 code triplet.
 * Broken triplets are set to -1, -1, -1.
 */
struct triplet {
	unsigned	address		: 8;
	unsigned	mode		: 8;
	unsigned	data		: 8;
}; /* __attribute__ ((packed)) */

/** @internal */
typedef struct triplet enhancement[16 * 13 + 1];

/* Level one page extension */

/**
 * @internal
 * 9.4.2.2 X/28/0, X/28/4 and
 * 10.6.4 MOT POP link fallback flags.
 */
typedef struct {
	vbi3_bool	black_bg_substitution;

	int		left_panel_columns;
	int		right_panel_columns;
} ext_fallback;

/** @internal */
#define VBI3_TRANSPARENT_BLACK 8

/**
 * @internal
 * 9.4.2 Packet X/28
 * 9.5 Packet M/29
 */
struct extension {
	/**
	 * We have data from packets X/28 (in lop) or M/29 (in magazine)
	 * with this set of designations. Magazine is always valid,
	 * LOP should fall back to magazine unless these bits are set:
	 *
	 * - 1 << 4 color_map[0 ... 15] is valid
	 * - 1 << 1 drcs_clut[] is valid
	 * - 1 << 0 or
	 *   1 << 4 everything else is valid.
	 */
	unsigned int		designations;

	/** Primary and secondary character set. */
	vbi3_ttx_charset_code	charset_code[2];

	/** Blah. */
	unsigned int		def_screen_color;
	unsigned int		def_row_color;

	/**
	 * Adding these values (0, 8, 16, 24) to character color
	 * 0 ... 7 gives index into color_map[] below.
	 */ 
	unsigned int		foreground_clut;
	unsigned int		background_clut;

	ext_fallback		fallback;

	/**
	 * - 2 dummy entries, 12x10x1 (G)DRCS use the color_map[]
	 *     like built-in chars.
	 * - 4 entries for 12x10x2 GDRCS pixel 0 ... 3 to color_map[].
	 * - 4 more for local DRCS
	 * - 16 entries for 12x10x4 and 6x5x4 GDRCS pixel 0 ... 15
	 *      to color_map[].
	 * - 16 more for local DRCS.
	 */
	vbi3_color		drcs_clut[2 + 2 * 4 + 2 * 16];

	/**
	 * CLUTs 0 ... 4 of 8 colors each. CLUT 2 & 3 are redefinable
	 * at Level 2.5, CLUT 0 to 3 at Level 3.5, except color_map[8]
	 * which is "transparent" color (VBI3_TRANSPARENT_BLACK).
	 * CLUT 4 contains libzvbi private colors which never change.
	 */
	vbi3_rgba		color_map[40];
};

extern void
extension_dump			(const struct extension *ext,
				 FILE *			fp);

/**
 * @internal
 *
 * 12.3.1 Table 28 Mode 10001, 10101 - Object invocation,
 * object definition. See also triplet_object_address().
 *
 * MOT default, POP and GPOP object address.
 *
 * n8  n7  n6  n5  n4  n3  n2  n1  n0
 * packet  triplet lsb ----- s1 -----
 */
typedef int object_address;

/** @internal */
struct ait_title {
	pagenum			page;
	uint8_t			text[12];
};

/* Level one page */

/**
 * @internal
 * 9.3.1.3 Control bits (0xE03F7F),
 * 15.2 National subset C12-C14,
 * B.6 Transmission rules for enhancement data.
 */
#define C4_ERASE_PAGE		0x000080
#define C5_NEWSFLASH		0x004000
#define C6_SUBTITLE		0x008000
#define C7_SUPPRESS_HEADER	0x010000
#define C8_UPDATE		0x020000
#define C9_INTERRUPTED		0x040000
#define C10_INHIBIT_DISPLAY	0x080000
#define C11_MAGAZINE_SERIAL	0x100000
#define C12_FRAGMENT		0x200000
#define C13_PARTIAL_PAGE	0x400000
#define C14_RESERVED		0x800000

/**
 * @internal
 * @brief Level one page.
 */
struct lop {
	/** Raw text as received. */
	uint8_t			raw[26][40];

	/** Packet X/27/0-5 links. */
	pagenum			link[6 * 6];

	/**
	 * Packet X/27 flag (ETR 287 section 10.4):
	 * Have FLOF navigation, display row 24.
	 */
	vbi3_bool		have_flof;
};

/* Magazine */

/**
 * @internal
 * 10.6.4 MOT object links
 */
struct pop_link {
	vbi3_pgno		pgno;
	ext_fallback		fallback;
	struct {
		object_type		type;
		object_address		address;
	}			default_obj[2];
};

/**
 * @internal
 * @brief Magazine defaults.
 */
struct magazine {
	/** Default extension. */
	struct extension	extension;

	/**
	 * Page number to pop_link[] and drcs_link[] for default
	 * object invocation. Valid range is 0 ... 7, broken -1.
	 */
	uint8_t			pop_lut[0x100];
	uint8_t			drcs_lut[0x100];

	/**
	 * Level 2.5 (0) or 3.5 (1), 1 global and 7 local links to
	 * POP/DRCS page. Unused or broken: NO_PAGE (pgno).
	 */
    	struct pop_link		pop_link[2][8];
	vbi3_pgno		drcs_link[2][8];
};

/* Network */

/** @inline */
#define SUBCODE_SINGLE_PAGE	0x0000
/** @inline */
#define SUBCODE_MULTI_PAGE	0xFFFE
/** @inline */
#define SUBCODE_UNKNOWN		0xFFFF

/** @inline */
struct page_stat {
	/* Information gathered from MOT, MIP, BTT, G/POP pages. */

	/** Actually a vbi3_page_type. */
	uint8_t			page_type;

	/** Actually a vbi3_ttx_charset_code, 0xFF unknown. */
  	uint8_t			ttx_charset_code;

	/**
	 * Highest subpage number transmitted according to MOT, MIP, BTT.
	 * - 0x0000		single page (SUBCODE_SINGLE_PAGE)
	 * - 0x0002 - 0x0079	multi-page
	 * - 0x0080 - 0x3F7F	clock page, other pages with non-standard
	 *			subpages not to be cached
	 * - 0xFFFE		has 2+ subpages (libzvbi) (SUBCODE_MULTI_PAGE)
	 * - 0xFFFF		unknown (libzvbi) (SUBCODE_UNKNOWN)
	 */
	uint16_t 		subcode;

	/** Last received page flags (cache_page.flags) */
	uint32_t	       flags;

	/* Cache statistics. */

	/** Subpages cached now and ever. */
	uint8_t			n_subpages;
	uint8_t			max_subpages;

	/** Subpage numbers actually encountered (0x00 ... 0x79). */
	uint8_t			subno_min;
	uint8_t			subno_max;
};

extern const struct magazine *
_vbi3_teletext_decoder_default_magazine (void);

#endif /* VT_H */
