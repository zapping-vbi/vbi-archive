#ifndef VT_H
#define VT_H

#include "misc.h"

#define W		40
#define H		25
#define BAD_CHAR	0xb8	// substitute for chars with bad parity

extern int debug;

struct vt_event
{
    int type;
    void *resource;	/* struct xio_win *, struct vbi *, ... */
    int i1, i2, i3, i4;
    void *p1;
};

#define EV_CLOSE	1
#define EV_KEY		2	// i1:KEY_xxx  i2:shift-flag
#define EV_MOUSE	3	// i1:button  i2:shift-flag i3:x  i4:y
#define EV_SELECTION	4	// i1:len  p1:data
#define EV_PAGE		5	// p1:vt_page	i1:query-flag
#define EV_HEADER	6	// i1:pgno  i2:subno  i3:flags  p1:data
#define EV_XPACKET	7	// i1:mag  i2:pkt  i3:errors  p1:data
#define EV_RESET	8	// ./.
#define EV_TIMER	9	// ./.

#define KEY_F(i)	(1000+i)
#define KEY_LEFT	2001
#define KEY_RIGHT	2002
#define KEY_UP		2003
#define KEY_DOWN	2004
#define KEY_PUP		2005
#define KEY_PDOWN	2006
#define KEY_DEL		2007
#define KEY_INS		2008

/*
 *
 */

typedef enum {
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

typedef unsigned int	rgba;			/* 0xAABBGGRR */

#define TRANSPARENT_BLACK 8

typedef struct {
	unsigned int	designations;

	char		char_set[2];		/* primary, secondary */

	char		def_screen_colour;
	char		def_row_colour;

	char		foreground_clut;	/* 0, 8, 16, 24 */
	char		background_clut;

	ext_fallback	fallback;

	u8		drcs_clut[2 + 2 * 4 + 2 * 16];
						/* f/b, dclut4, dclut16 */
	rgba		colour_map[36];
} vt_extension;

typedef struct vt_triplet {
	unsigned	address : 8;
	unsigned	mode : 8;
	unsigned	data : 8;
} __attribute__ ((packed)) vt_triplet;

typedef struct vt_pagenum {
	unsigned	type : 4;
	unsigned	pgno : 12;
	unsigned	subno : 16;
} vt_pagenum;

typedef struct {
	vt_pagenum	page;
	unsigned char	text[12];
} ait_entry;

typedef vt_triplet vt_enhancement[16 * 13 + 1];

#define NO_PAGE(pgno) (((pgno) & 0xFF) == 0xFF)

struct vt_page
{
	page_function		function;
	int			pgno, subno;
	int			national;
	int			flags;
	u32			lop_lines, enh_lines;		/* set of received lines */

	/* added temporarily: */
	struct vbi *	vbi;

	union {
		struct lop {
			u8		raw[26][40];
			vt_pagenum	link[6 * 6];		/* X/27/0-5 links */
			char		flof, ext;
		}		unknown, lop;
		struct {
			struct lop	lop;
			vt_enhancement	enh;
		}		enh_lop;
		struct {
			struct lop	lop;
			vt_enhancement	enh;
			vt_extension	ext;
		}		ext_lop;
		struct {
			u16		pointer[96];
			vt_triplet	triplet[39 * 13 + 1];
// XXX preset [+1] mode (not 0xFF) or catch
		}		gpop, pop;
		struct {
			u8		raw[26][40];
			u8		bits[48][12 * 10 / 2];	/* XXX too large for a union? */
			u8		mode[48];
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

/*                              0xE03F7F 	national character subset and sub-page */
#define C4_ERASE_PAGE		0x000080	/* erase previously stored packets */
#define C5_NEWSFLASH		0x004000	/* box and overlay */
#define C6_SUBTITLE		0x008000	/* box and overlay */
#define C7_SUPPRESS_HEADER	0x010000	/* row 0 not to be displayed */
#define C8_UPDATE		0x020000
#define C9_INTERRUPTED		0x040000
#define C10_INHIBIT_DISPLAY	0x080000	/* rows 1-24 not to be displayed */
#define C11_MAGAZINE_SERIAL	0x100000




#define MIP_NO_PAGE		0x00
#define MIP_NORMAL_PAGE		0x01
#define MIP_SUBTITLE		0x70
#define MIP_SUBTITLE_INDEX	0x78
#define MIP_CLOCK		0x79	/* sort of */
#define MIP_WARNING		0x7A
#define MIP_INFORMATION 	0x7B
#define MIP_NOW_AND_NEXT	0x7D
#define MIP_TV_INDEX		0x7F
#define MIP_TV_SCHEDULE		0x81
#define MIP_SYSTEM_PAGE		0xE7
#define MIP_TOP_PAGE		0xFE
#define MIP_UNKNOWN		0xFF	/* Zapzilla internal code */

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
	vt_extension	extension;

	unsigned char	pop_lut[256];
	unsigned char	drcs_lut[256];

    	pop_link	pop_link[16];
	int		drcs_link[16];	/* pgno */
} magazine;




#define ANY_SUB		0x3f7f	// universal subpage number


#endif
