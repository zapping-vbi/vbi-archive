/*
 *  Teletext decoder
 *
 *  Copyright (C) 2000-2001 Michael H. Schimek
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

#ifndef VT_H
#define VT_H

#include "format.h"
#include "../common/types.h"

typedef enum {
	PAGE_FUNCTION_TRIGGER = -3,	/* private */
	PAGE_FUNCTION_DISCARD = -2,	/* private */
	PAGE_FUNCTION_UNKNOWN = -1,	/* private */
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

typedef enum {
	PAGE_CODING_UNKNOWN = -1,
	PAGE_CODING_PARITY,
	PAGE_CODING_BYTES,
	PAGE_CODING_TRIPLETS,
	PAGE_CODING_HAMMING84,
	PAGE_CODING_AIT,
	PAGE_CODING_META84
} page_coding;

typedef enum {
	DRCS_MODE_12_10_1,
	DRCS_MODE_12_10_2,
	DRCS_MODE_12_10_4,
	DRCS_MODE_6_5_4,
	DRCS_MODE_SUBSEQUENT_PTU = 14,
	DRCS_MODE_NO_DATA
} drcs_mode;

/*
    Only a minority of pages need this
 */

typedef struct {
	char		black_bg_substitution;
	char		left_side_panel;
	char		right_side_panel;
	char		left_panel_columns;
} ext_fallback;

#define TRANSPARENT_BLACK 8

typedef struct {
	unsigned int	designations;

	char		char_set[2];		/* primary, secondary */

	char		def_screen_colour;
	char		def_row_colour;

	char		foreground_clut;	/* 0, 8, 16, 24 */
	char		background_clut;

	ext_fallback	fallback;

	unsigned char   drcs_clut[2 + 2 * 4 + 2 * 16];
						/* f/b, dclut4, dclut16 */
	attr_rgba	colour_map[40];
} extension;

typedef struct vt_triplet {
	unsigned	address : 8;
	unsigned	mode : 8;
	unsigned	data : 8;
} __attribute__ ((packed)) vt_triplet;

typedef struct vt_pagenum {
	unsigned	type : 8;
	unsigned	pgno : 16;
	unsigned	subno : 16;
} pagenum;

typedef struct {
	pagenum	        page;
	unsigned char	text[12];
} ait_entry;

typedef vt_triplet enhancement[16 * 13 + 1];

#define NO_PAGE(pgno) (((pgno) & 0xFF) == 0xFF)

#ifndef ANY_SUB
#define ANY_SUB		0x3F7F
#endif

/*                              0xE03F7F 	national character subset and sub-page */
#define C4_ERASE_PAGE		0x000080	/* erase previously stored packets */
#define C5_NEWSFLASH		0x004000	/* box and overlay */
#define C6_SUBTITLE		0x008000	/* box and overlay */
#define C7_SUPPRESS_HEADER	0x010000	/* row 0 not to be displayed */
#define C8_UPDATE		0x020000
#define C9_INTERRUPTED		0x040000
#define C10_INHIBIT_DISPLAY	0x080000	/* rows 1-24 not to be displayed */
#define C11_MAGAZINE_SERIAL	0x100000

struct vt_page
{
	page_function		function;

	int			pgno, subno;

	int			national;
	int			flags;

	unsigned int		lop_lines;		/* set of received lines */
        unsigned int            enh_lines;

	union {
		struct lop {
			unsigned char	raw[26][40];
		        pagenum	        link[6 * 6];		/* X/27/0-5 links */
			char		flof, ext;
		}		unknown, lop;
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
			unsigned short	pointer[96];
			vt_triplet	triplet[39 * 13 + 1];
// XXX preset [+1] mode (not 0xFF) or catch
		}		gpop, pop;
		struct {
			unsigned char		raw[26][40];
			unsigned char		bits[48][12 * 10 / 2];
			unsigned char		mode[48];
			unsigned long long	invalid;
		}		gdrcs, drcs;

		ait_entry	ait[46];

	}		data;

	/* 
	 *  Dynamic size, no fields below unless
	 *  vt_page is statically allocated.
	 */
};

static inline int
vtp_size(struct vt_page *vtp)
{
	switch (vtp->function) {
	case PAGE_FUNCTION_UNKNOWN:
	case PAGE_FUNCTION_LOP:
		if (vtp->data.lop.ext)
			return sizeof(*vtp) - sizeof(vtp->data)	+ sizeof(vtp->data.ext_lop);
		else if (vtp->enh_lines)
			return sizeof(*vtp) - sizeof(vtp->data)	+ sizeof(vtp->data.enh_lop);
		else
			return sizeof(*vtp) - sizeof(vtp->data)	+ sizeof(vtp->data.lop);

	case PAGE_FUNCTION_GPOP:
	case PAGE_FUNCTION_POP:
		return sizeof(*vtp) - sizeof(vtp->data)	+ sizeof(vtp->data.pop);

	case PAGE_FUNCTION_GDRCS:
	case PAGE_FUNCTION_DRCS:
		return sizeof(*vtp) - sizeof(vtp->data)	+ sizeof(vtp->data.drcs);

	case PAGE_FUNCTION_AIT:
		return sizeof(*vtp) - sizeof(vtp->data)	+ sizeof(vtp->data.ait);

	default:
	}

	return sizeof(*vtp);
}

/*
 *  MIP 0x01 ... 0x51 -> 0x01 (subpages)
 *  MIP 0x70 ... 0x77 -> 0x70 (language)
 *  MIP 0x7B -> 0x7C (subpages)
 *  MIP 0x7E -> 0x7F (subpages)
 *  MIP 0x81 ... 0xD1 -> 0x81 (subpages)
 *  MIP reserved -> 0xFF (VBI_UNKNOWN_PAGE)
 *
 *  MIP 0x80 and 0xE0 ... 0xFE are for decoder internal
 *  use only and have no corresponding VBI_ classification.
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
 *  internally by libvbi.
 */
#define VBI_NOT_PUBLIC		0x80
#define VBI_CA_DATA_BROADCAST	0xE0
#define VBI_EPG_DATA		0xE3
#define VBI_SYSTEM_PAGE		0xE7
#define VBI_DISP_SYSTEM_PAGE	0xF7
#define VBI_KEYWORD_SEARCH_LIST 0xF9
#define VBI_TOP_BLOCK		0xFA
#define VBI_TOP_GROUP		0xFB
#define VBI_ACI			0xFD
#define VBI_TOP_PAGE		0xFE

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
	BTT_NORMAL_11, /* ? */
	/* 12 ... 15 ? */
} btt_page_class;

typedef enum {
	LOCAL_ENHANCEMENT_DATA = 0,
	OBJ_TYPE_NONE = 0,
	OBJ_TYPE_ACTIVE,
	OBJ_TYPE_ADAPTIVE,
	OBJ_TYPE_PASSIVE
} object_type;

/*
 *  MOT default, POP and GPOP
 *
 *  n8  n7  n6  n5  n4  n3  n2  n1  n0
 *  packet  triplet lsb ----- s1 -----
 */
typedef int object_address;

typedef struct {
	int		pgno;
	ext_fallback	fallback;
	struct {
		object_type	type;
		object_address	address;
	}		default_obj[2];
} pop_link;

typedef struct {
	extension	extension;

	unsigned char	pop_lut[256];
	unsigned char	drcs_lut[256];

    	pop_link	pop_link[16];
	int		drcs_link[16];	/* pgno */
} magazine;

struct raw_page
{
	struct vt_page		page[1];
        unsigned char	        drcs_mode[48];
	int			num_triplets;
	int			ait_page;
};

typedef enum {
	VBI_LEVEL_1,
	VBI_LEVEL_1p5,
	VBI_LEVEL_2p5,
	VBI_LEVEL_3p5
} vbi_wst_level;

struct teletext {
	vbi_wst_level		max_level;

	unsigned char		header[32];

        pagenum		        initial_page;
	magazine		magazine[9];		/* 1 ... 8; #0 unmodified level 1.5 default */

	struct page_info {
		unsigned 		code : 8;
		unsigned		language : 8;
		unsigned 		subcode : 16;
	}			page_info[0x800];

	pagenum		        btt_link[15];
	bool			top;			/* use top navigation, flof overrides */

	struct raw_page		raw_page[8];
	struct raw_page		*current;
};

struct vbi; /* parent of struct teletext */

/* packet.c */

extern void		vbi_init_teletext(struct teletext *vt);
extern bool		vbi_teletext_packet(struct vbi *vbi, unsigned char *p);
extern void		vbi_teletext_desync(struct vbi *vbi);
extern struct vt_page *	vbi_convert_page(struct vbi *vbi, struct vt_page *vtp, bool cached, page_function new_function);

extern void		vbi_vps(struct vbi *vbi, unsigned char *p);

/* teletext.c */

extern int		vbi_format_page(struct vbi *vbi, struct fmt_page *pg, struct vt_page *vtp, vbi_wst_level max_level, int display_rows, int navigation);

#endif
