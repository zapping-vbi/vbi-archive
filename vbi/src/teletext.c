/*
 *  libzvbi - Teletext formatter
 *
 *  Copyright (C) 2000-2003 Michael H. Schimek
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
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

/* $Id: teletext.c,v 1.7.2.9 2004-03-31 00:41:34 mschimek Exp $ */

#include "../config.h"
#include "site_def.h"

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "intl-priv.h"
#include "bcd.h"
#include "vt.h"
#include "export.h"
#include "vbi.h"
#include "hamm.h"
#include "lang.h"
#include "pdc.h"

#define PGP_CHECK(ret_value)						\
do {									\
	assert (NULL != pg);						\
									\
	pgp = CONST_PARENT (pg, vbi_page_private, pg);			\
									\
	if (VBI_PAGE_PRIVATE_MAGIC != pgp->magic)			\
		return ret_value;					\
} while (0)

static void
character_set_designation	(const vbi_character_set *char_set[2],
				 vbi_character_set_code def_code,
				 const vt_extension *	ext,
				 const vt_page *	vtp);
static void
screen_color			(vbi_page *		pg,
				 unsigned int		flags,
				 unsigned int		color);
static vbi_bool
enhance				(vbi_page_private *		t,
				 object_type		type,
				 const vt_triplet *	p,
				 int			n_triplets,
				 int			inv_row,
				 int			inv_column);


/* Teletext page formatting ----------------------------------------------- */

#ifndef TELETEXT_FMT_LOG
#define TELETEXT_FMT_LOG 0
#endif

#define fmt_log(templ, args...)						\
do {									\
	if (TELETEXT_FMT_LOG)						\
		fprintf (stderr, templ , ##args);			\
} while (0)

static void
character_set_designation	(const vbi_character_set *char_set[2],
				 vbi_character_set_code default_code,
				 const vt_extension *	ext,
				 const vt_page *	vtp)
{
	unsigned int i;

	for (i = 0; i < 2; ++i) {
		const vbi_character_set *cs;
		vbi_character_set_code code;

		code = default_code;

		if (ext->charset_valid)
			code = ext->charset_code[i];

		cs = vbi_character_set_from_code ((code & ~7) + vtp->national);

		if (NULL == cs)
			cs = vbi_character_set_from_code (code);

		if (NULL == cs)
			cs = vbi_character_set_from_code (0);

		char_set[i] = cs;
	}
}

/**
 * Blurb.
 */
const vbi_character_set *
vbi_page_character_set		(const vbi_page *	pg,
				 unsigned int		level)
{
	const vbi_page_private *pgp;

	PGP_CHECK (NULL);

	return pgp->char_set[level & 1];
}

static void
screen_color			(vbi_page *		pg,
				 unsigned int		flags,
				 unsigned int		color)
{ 
	pg->screen_color = color;

	if (color == VBI_TRANSPARENT_BLACK
	    || (flags & (C5_NEWSFLASH | C6_SUBTITLE)))
		pg->screen_opacity = VBI_TRANSPARENT_SPACE;
	else
		pg->screen_opacity = VBI_OPAQUE;
}

static const vt_page *
resolve_obj_address		(vbi_page_private *	pgp,
				 const vt_triplet **	trip,
				 unsigned int *		remaining,
				 object_type		type,
				 vbi_pgno		pgno,
				 object_address		address,
				 page_function		function)
{
	const vt_page *vtp;
	vbi_subno subno;
	unsigned int packet;
	unsigned int pointer;
	unsigned int i;

	subno = address & 15;
	packet = ((address >> 7) & 3);
	i = ((address >> 5) & 3) * 3 + type;

	fmt_log ("obj invocation, source page %03x/%04x, "
		 "pointer packet %u triplet %u\n",
		 pgno, subno, packet + 1, i);

	vtp = vbi_cache_get (pgp->pg.vbi, NUID0, pgno, subno, 0x000F,
			     /* user access */ FALSE);

	if (!vtp) {
		fmt_log ("... page not cached\n");
		goto failure;
	}

	if (vtp->function == PAGE_FUNCTION_UNKNOWN) {
		if (!vbi_convert_cached_page (pgp->pg.vbi, &vtp, function)) {
			fmt_log ("... no g/pop page or hamming error\n");
			goto failure;
		}
	} else if (vtp->function == PAGE_FUNCTION_POP) {
		/* vtp->function = function; */ /* POP/GPOP */
	} else if (vtp->function != function) {
		fmt_log ("... source page wrong function %s, "
			 "expected %s\n",
			 page_function_name (vtp->function),
			 page_function_name (function));
		goto failure;
	}

	pointer = vtp->data.pop.pointer[packet * 24 + i * 2 
					+ ((address >> 4) & 1)];

	fmt_log ("... triplet pointer %u\n", pointer);

	if (pointer > 506) {
		fmt_log ("... triplet pointer %u > 506\n", pointer);
		goto failure;
	}

	if (TELETEXT_FMT_LOG) {
		packet = (pointer / 13) + 3;

		if (packet <= 25) {
			fmt_log ("... object start in packet %u, "
				 "triplet %u (pointer %u)\n",
				 packet, pointer % 13, pointer);
		} else {
			fmt_log ("... object start in packet 26/%u, "
				 "triplet %u (pointer %u)\n",
				 packet - 26, pointer % 13, pointer);	
		}
	}

	*trip = vtp->data.pop.triplet + pointer;
	*remaining = N_ELEMENTS (vtp->data.pop.triplet) - (pointer + 1);

	fmt_log ("... obj def: addr=0x%02x mode=0x%04x data=%u=0x%x\n",
		 (*trip)->address,
		 (*trip)->mode,
		 (*trip)->data, (*trip)->data);

	address ^= (*trip)->address << 7;
	address ^= (*trip)->data;

	if ((*trip)->mode != (type + 0x14)
	    || (address & 0x1FF)) {
		fmt_log ("... no object definition\n");
		goto failure;
	}

	*trip += 1;

	return vtp;

 failure:
	vbi_cache_unref (pgp->pg.vbi, vtp);

	return NULL;
}

#define enhance_flush(column) _enhance_flush (column, t, type,		\
 	&ac, &mac, acp, inv_row, inv_column, 				\
	&active_row, &active_column, row_color, invert)

/*
 *  Flushes accumulated attributes up to but excluding column,
 *  or -1 to end of row. This is a subfunction of enhance(), we use
 *  the macro above.
 *
 *  type	- current object type
 *  ac		- attributes
 *  mac		- attributes mask
 *  acp		- cursor position
 *  inv_        - position at object invocation
 *  active_     - current position (inv_ offset)
 */
static void
_enhance_flush			(int			column,
				 vbi_page_private *	t,
				 object_type		type,
				 vbi_char *		ac,
				 vbi_char *		mac,
				 vbi_char *		acp,
				 const int		inv_row,
				 const int		inv_column,
				 int *			active_row,
				 int *			active_column,
				 int			row_color,
				 int			invert)
{
	int row = inv_row + *active_row;
	int i;

	if (row >= 25)
		return;

	if (column == -1) {
		/* Flush entire row */

		switch (type) {
		case OBJECT_TYPE_PASSIVE:
		case OBJECT_TYPE_ADAPTIVE:
			column = *active_column + 1;
			break;

		default:
			column = 40;
			break;
		}
	}

	if (type == OBJECT_TYPE_PASSIVE && !mac->unicode) {
		*active_column = column;
		return;
	}

	fmt_log ("flush [%04x%c,F%d%c,B%d%c,S%d%c,O%d%c,H%d%c] %d ... %d\n",
		 ac->unicode, mac->unicode ? '*' : ' ',
		 ac->foreground, mac->foreground ? '*' : ' ',
		 ac->background, mac->background ? '*' : ' ',
		 ac->size, mac->size ? '*' : ' ',
		 ac->opacity, mac->opacity ? '*' : ' ',
		 !!(ac->attr & VBI_FLASH),
		 !!(mac->attr & VBI_FLASH) ? '*' : ' ',
		 *active_column, column - 1);

	i = inv_column + *active_column;

	while (i < inv_column + column && i < 40) {
		vbi_char c;
		int raw;

		c = acp[i];

		if (mac->attr & VBI_UNDERLINE) {
			unsigned int attr = ac->attr;

			if (!mac->unicode)
				ac->unicode = c.unicode;

			if (vbi_is_gfx (ac->unicode)) {
				if (attr & VBI_UNDERLINE)
					ac->unicode &= ~0x20; /* separated */
				else
					ac->unicode |= 0x20; /* contiguous */
				mac->unicode = ~0;
				attr &= ~VBI_UNDERLINE;
			}

			COPY_SET_MASK (c.attr, attr, VBI_UNDERLINE);
		}

		/* TODO: vbi_char is 64 bit, a masked move may work. */

		if (mac->foreground)
			c.foreground = (ac->foreground == VBI_TRANSPARENT_BLACK) ?
				row_color : ac->foreground;
		if (mac->background)
			c.background = (ac->background == VBI_TRANSPARENT_BLACK) ?
				row_color : ac->background;
		if (invert)
			SWAP (c.foreground, c.background);
		if (mac->opacity)
			c.opacity = ac->opacity;

		COPY_SET_MASK (c.attr, ac->attr,
			       mac->attr & (VBI_FLASH | VBI_CONCEAL));

		if (mac->unicode) {
			c.unicode = ac->unicode;
			mac->unicode = 0;

			if (mac->size)
				c.size = ac->size;
			else if (c.size > VBI_DOUBLE_SIZE)
				c.size = VBI_NORMAL_SIZE;
		}

		acp[i++] = c;

		if (type == OBJECT_TYPE_PASSIVE)
			break;

		if (type == OBJECT_TYPE_ADAPTIVE)
			continue;

		/* OBJECT_TYPE_ACTIVE */

		raw = (row == 0 && i < 9) ?
			0x20 : vbi_ipar8 (t->vtp->data.lop.raw[row][i - 1]);

		/* Set-after spacing attributes cancelling non-spacing */

		switch (raw) {
		case 0x00 ... 0x07:	/* alpha + foreground color */
		case 0x10 ... 0x17:	/* mosaic + foreground color */
			fmt_log ("... fg term %d %02x\n", i, raw);
			mac->foreground = 0;
			mac->attr &= ~VBI_CONCEAL;
			break;

		case 0x08:		/* flash */
			mac->attr &= ~VBI_FLASH;
			break;

		case 0x0A:		/* end box */
		case 0x0B:		/* start box */
			if (i < 40
			    && raw == vbi_ipar8 (t->vtp->data.lop.raw[row][i])) {
				fmt_log ("... boxed term %d %02x\n", i, raw);
				mac->opacity = 0;
			}
			
			break;
			
		case 0x0D:		/* double height */
		case 0x0E:		/* double width */
		case 0x0F:		/* double size */
			fmt_log ("... size term %d %02x\n", i, raw);
			mac->size = 0;
			break;
		}
		
		if (i > 39)
			break;
		
		raw = (row == 0 && i < 8) ?
			0x20 : vbi_ipar8 (t->vtp->data.lop.raw[row][i]);

		/* Set-at spacing attributes cancelling non-spacing */
		
		switch (raw) {
		case 0x09:		/* steady */
			mac->attr &= ~VBI_FLASH;
			break;
			
		case 0x0C:		/* normal size */
			fmt_log ("... size term %d %02x\n", i, raw);
			mac->size = 0;
			break;
			
		case 0x18:		/* conceal */
			mac->attr &= ~VBI_CONCEAL;
			break;
			
		/*
		 *  Non-spacing underlined/separated display attribute
		 *  cannot be cancelled by a subsequent spacing attribute.
		 */
			
		case 0x1C:		/* black background */
		case 0x1D:		/* new background */
			fmt_log ("... bg term %d %02x\n", i, raw);
			mac->background = 0;
			break;
		}
	}

	*active_column = column;
}

vbi_inline vbi_pgno
magazine_pop_link		(vbi_page_private *	pgp,
				 unsigned int		link)
{
	vbi_pgno pgno;

	if (pgp->max_level >= VBI_WST_LEVEL_3p5) {
		pgno = pgp->mag->pop_link[link + 8].pgno;

		if (!NO_PAGE (pgno))
			return pgno;
	}

	return pgp->mag->pop_link[link + 0].pgno;
}

/**
 * @internal
 * @param type Current object type
 * @param p Triplet requesting the object
 * @param row Current row
 * @param column Current column
 *
 * Recursive object invocation at row, column.
 *
 * @returns
 * FALSE on failure.
 */
static vbi_bool
object_invocation		(vbi_page_private *	pgp,
				 object_type		type,
				 const vt_triplet *	trip,
				 int			row,
				 int			column)
{
	object_type new_type;
	unsigned int source;
	const vt_page *trip_vtp;
	unsigned int n_triplets;
	vbi_bool success;

	new_type = trip->mode & 3;
	source = (trip->address >> 3) & 3;

	fmt_log ("enhancement obj invocation source %u type %s\n",
		 source, object_type_name (new_type));

	if (new_type <= type) {
		/* ETS 300 706, Section 13.2ff */
		fmt_log ("... type priority violation %s -> %s\n",
			 object_type_name (type),
			 object_type_name (new_type));
		return FALSE;
	}

	trip_vtp = NULL;
	n_triplets = 0;

	if (0 == source) {
		fmt_log ("... invalid source\n");
		return FALSE;
	} else if (1 == source) {
		unsigned int designation;
		unsigned int triplet;
		unsigned int offset;

		designation = (trip->data >> 4) + ((trip->address & 1) << 4);
		triplet = trip->data & 15;

		fmt_log ("... local obj %u/%u\n", designation, triplet);

		if (LOCAL_ENHANCEMENT_DATA != type
		    || triplet > 12) {
			fmt_log ("... invalid type %s or triplet\n",
				 object_type_name (type));
			return FALSE;
		}

		if (0 == (pgp->vtp->enh_lines & 1)) {
			fmt_log ("... have no packet %u\n", designation);
			return FALSE;
		}

		offset = designation * 13 + triplet;

		trip = pgp->vtp->data.enh_lop.enh + offset;
		n_triplets = N_ELEMENTS (pgp->vtp->data.enh_lop.enh) - offset;
	} else {
		page_function function;
		vbi_pgno pgno;
		unsigned int link;

		link = 0;

		if (3 == source) {
			fmt_log ("... global obj\n");

			function = PAGE_FUNCTION_GPOP;
			pgno = pgp->vtp->data.lop.link[24].pgno;

			if (NO_PAGE (pgno)) {
				pgno = magazine_pop_link (pgp, 0);
			} else {
				fmt_log ("... X/27/4 GPOP overrides MOT\n");
			}
		} else {
			fmt_log ("... public obj\n");

			function = PAGE_FUNCTION_POP;
			pgno = pgp->vtp->data.lop.link[25].pgno;

			if (NO_PAGE (pgno)) {
				link = pgp->mag->pop_lut
					[pgp->vtp->pgno & 0xFF];

				if (0 == link) {
					fmt_log ("... MOT pop_lut empty\n");
					return FALSE;
				}

				pgno = magazine_pop_link (pgp, link);
			} else {
				fmt_log ("... X/27/4 POP overrides MOT\n");
			}
		}

		if (NO_PAGE (pgno)) {
			fmt_log ("... dead MOT link %u\n", link);
			return FALSE;
		}

		trip_vtp = resolve_obj_address
			(pgp, &trip, &n_triplets,
			 new_type, pgno,
			 (trip->address << 7) + trip->data,
			 function);

		if (!trip_vtp)
			return FALSE;
	}

	success = enhance (pgp, new_type, trip, n_triplets, row, column);

	vbi_cache_unref (pgp->pg.vbi, trip_vtp);

	fmt_log ("... object done, %s\n",
		 success ? "success" : "failed");

	return success;
}

/**
 * @internal
 *
 * Like object_invocation(), but uses default object links if
 * available. Called when a page has no enhancement packets.
 */
vbi_inline vbi_bool
default_object_invocation	(vbi_page_private *	pgp)
{
	const vt_pop_link *pop;
	unsigned int link;
	unsigned int order;
	unsigned int i;

	fmt_log ("default obj invocation\n");

	link = pgp->mag->pop_lut[pgp->vtp->pgno & 0xFF];

	if (0 == link) {
		fmt_log ("...no pop link\n");
		return FALSE;
	}

	pop = pgp->mag->pop_link + link + 8;

	if (pgp->max_level < VBI_WST_LEVEL_3p5
	    || NO_PAGE (pop->pgno)) {
		pop = pgp->mag->pop_link + link;

		if (NO_PAGE (pop->pgno)) {
			fmt_log ("... dead MOT pop link %u\n", link);
			return FALSE;
		}
	}

	/* PASSIVE > ADAPTIVE > ACTIVE */
	order = (pop->default_obj[0].type > pop->default_obj[1].type);

	for (i = 0; i < 2; ++i) {
		object_type type;
		const vt_page *trip_vtp;
		const vt_triplet *trip;
		unsigned int n_triplets;
		vbi_bool success;

		type = pop->default_obj[i ^ order].type;

		if (OBJECT_TYPE_NONE == type)
			continue;

		fmt_log ("... invocation %u, type %s\n", i ^ order,
			 object_type_name (type));

		trip_vtp = resolve_obj_address
			(pgp, &trip, &n_triplets,
			 type, pop->pgno,
			 pop->default_obj[i ^ order].address,
			 PAGE_FUNCTION_POP);

		if (!trip_vtp)
			return FALSE;

		success = enhance (pgp, type, trip, n_triplets, 0, 0);

		vbi_cache_unref (pgp->pg.vbi, trip_vtp);

		if (!success)
			return FALSE;
	}

	fmt_log ("... default object done\n");

	return TRUE;
}



vbi_inline vbi_pgno
magazine_drcs_link		(vbi_page_private *	pgp,
				 unsigned int		link)
{
	vbi_pgno pgno;

	if (pgp->max_level >= VBI_WST_LEVEL_3p5) {
		pgno = pgp->mag->drcs_link[link + 8];

		if (!NO_PAGE (pgno))
			return pgno;
	}

	return pgp->mag->drcs_link[link + 0];
}

static const vt_page *
get_drcs_page			(vbi_page_private *	pgp,
				 vbi_nuid		nuid,
				 vbi_pgno		pgno,
				 vbi_subno		subno,
				 page_function		function)
{
	const vt_page *vtp;

	vtp = vbi_cache_get (pgp->pg.vbi, NUID0, pgno, subno, 0x000F,
			     /* user_access */ FALSE);

	if (!vtp) {
		fmt_log ("... page %llx/%03x/%04x not cached\n",
			 nuid, pgno, subno);
		goto failure;
	}

	if (vtp->function == PAGE_FUNCTION_UNKNOWN) {
		if (!vbi_convert_cached_page (pgp->pg.vbi, &vtp, function)) {
			fmt_log ("... no g/drcs page or hamming error\n");
			goto failure;
		}
	} else if (vtp->function == PAGE_FUNCTION_DRCS) {
		/* vtp->function = function; */ /* DRCS/GDRCS */
	} else if (vtp->function != function) {
		fmt_log ("... source page wrong function %s, "
			 "expected %s\n",
			 page_function_name (vtp->function),
			 page_function_name (function));
		goto failure;
	}

	return vtp;

 failure:
	vbi_cache_unref (pgp->pg.vbi, vtp);

	return NULL;
}

vbi_inline vbi_bool
reference_drcs_page		(vbi_page_private *	pgp,
				 unsigned int		normal,
				 unsigned int		plane,
				 unsigned int		offset,
				 vbi_subno		subno)
{
	const vt_page *drcs_vtp;
	page_function function;
	vbi_pgno pgno;
	unsigned int link;

	drcs_vtp = pgp->drcs_vtp[plane];

	if (NULL != drcs_vtp) {
		if (drcs_vtp->data.drcs.invalid & (1ULL << offset)) {
			fmt_log ("... invalid drcs, prob. tx error\n");
			return FALSE;
		}

		return TRUE;
	}

	link = 0;

	if (normal) {
		function = PAGE_FUNCTION_DRCS;
		pgno = pgp->vtp->data.lop.link[25].pgno;

		if (NO_PAGE (pgno)) {
			link = pgp->mag->drcs_lut[pgp->vtp->pgno & 0xFF];

			if (0 == link) {
				fmt_log ("... MOT drcs_lut empty\n");
				return FALSE;
			}

			pgno = magazine_drcs_link (pgp, link);
		} else {
			fmt_log ("... X/27/4 DRCS overrides MOT\n");
		}
	} else {
		function = PAGE_FUNCTION_GDRCS;
		pgno = pgp->vtp->data.lop.link[26].pgno;

		if (NO_PAGE (pgno)) {
			pgno = magazine_drcs_link (pgp, 0);
		} else {
			fmt_log ("... X/27/4 GDRCS overrides MOT\n");
		}
	}

	if (NO_PAGE (pgno)) {
		fmt_log ("... dead MOT link %d\n", link);
		return FALSE;
	}

	fmt_log ("... %s drcs from page %03x/%04x\n",
		 normal ? "normal" : "global", pgno, subno);

	drcs_vtp = get_drcs_page (pgp, pgp->pg.nuid, pgno, subno, function);

	if (NULL == drcs_vtp)
		return FALSE;

	if (drcs_vtp->data.drcs.invalid & (1ULL << offset)) {
		fmt_log ("... invalid drcs, prob. tx error\n");
		vbi_cache_unref (pgp->pg.vbi, drcs_vtp);
		return FALSE;
	}

	pgp->drcs_vtp[plane] = drcs_vtp;

	return TRUE;
}

/**
 * @param pg
 * @param unicode DRCS character code.
 *
 * When a vbi_char on Teletext vbi_page @a pg is a Dynamically
 * Redefinable Character, this function returns a pointer to the
 * character shape. The data is valid until the page is deleted
 * with vbi_page_delete().
 *
 * Characters are 12 pixels wide, 10 pixels high, and each pixel can
 * assume one of a set of 2, 4 or 16 colors. Pixels are stored left to
 * right and top to bottom. Every two pixels are stored in one byte,
 * first pixel in the last significant four bits, second pixel in the
 * most significant four bits.
 *
 * Pixels translate to color map indices through a color look-up table,
 * see the description of vbi_page for details.
 *
 * @returns
 * Pointer to character data, NULL if @a pg is invalid, @a unicode
 * is not a DRCS code, or no font data is available.
 */
const uint8_t *
vbi_page_drcs_data		(const vbi_page *	pg,
				 unsigned int		unicode)
{
	const vbi_page_private *pgp;
	const vt_page *drcs_vtp;
	unsigned int plane;
	unsigned int glyph;

	PGP_CHECK (NULL);

	if (!vbi_is_drcs (unicode))
		return NULL;

	plane = (unicode >> 6) & 0x1F;

	drcs_vtp = pgp->drcs_vtp[plane];

	if (NULL == drcs_vtp)
		return NULL;

	glyph = unicode & 0x3F;

	if (glyph >= 48)
		return NULL;

	return drcs_vtp->data.drcs.bits[glyph];
}

/*
 *  Enhances a level one page with data from page enhancement
 *  packets. May be called recursively. Returns success.
 *
 *  type	- current object type
 *  p		- vector of enhancement triplets
 *  n_triplets	- length of vector
 *  inv_	- cursor position at object invocation, cursor
 *		  is moved relative to this (default 0, 0)
 *
 *  XXX panels not implemented
 */
static vbi_bool
enhance				(vbi_page_private *		t,
				 object_type		type,
				 const vt_triplet *	p,
				 int			n_triplets,
				 int			inv_row,
				 int			inv_column)
{
	vbi_char ac, mac, *acp;
	int active_column, active_row;
	int offset_column, offset_row;
	int row_color, next_row_color;
	const vbi_character_set *cs;
	int invert;
	int drcs_s1[2];
	pdc_program *p1, pdc_tmp;
	int pdc_hour_mode;

	/* Current position rel. inv_row, inv_column) */

	active_column = 0;
	active_row = 0;

	acp = t->pg.text + inv_row * t->pg.columns;

	/* Origin modifier, reset after recurs. obj invocation */

	offset_column = 0;
	offset_row = 0;


	row_color = t->ext->def_row_color;
	next_row_color = row_color;

	drcs_s1[0] = 0; /* global */
	drcs_s1[1] = 0; /* normal */

	cs = t->char_set[0];

	/* Accumulated attributes and attr mask */

	CLEAR (ac);
	CLEAR (mac);

	invert = 0;

	if (type == OBJECT_TYPE_PASSIVE) {
		ac.foreground	= VBI_WHITE;
		ac.background	= VBI_BLACK;
		ac.opacity	= t->page_opacity[1];

		mac.foreground	= ~0;
		mac.background	= ~0;
		mac.opacity	= ~0;
		mac.size	= ~0;
		mac.attr	&= ~(VBI_UNDERLINE | VBI_CONCEAL | VBI_FLASH);
	}

	/*
	 *  PDC Preselection method "B"
	 *  ETS 300 231, Section 7.3.2.
	 */

	CLEAR (pdc_tmp);

	pdc_tmp.ad.year		= -1; /* not given */
	pdc_tmp.ad.month	= -1;
	pdc_tmp.at1.hour	= -1; /* not given */
	pdc_tmp.at2.hour	= -1;

	p1 = t->pdc_table;

	pdc_hour_mode = 0; /* mode of previous hour triplet */

	for (; n_triplets > 0; ++p, --n_triplets) {
		if (p->address >= 40) {
			/*
			 *  Row address triplets
			 */

			int s = p->data >> 5;
			int row = (p->address - 40) ? : (25 - 1);
			int column = 0;

			if (pdc_hour_mode) {
				fmt_log ("invalid pdc hours, no minutes follow\n");
				return FALSE;
			}

			switch (p->mode) {
			case 0x00:		/* full screen color */
				if (t->max_level >= VBI_WST_LEVEL_2p5
				    && s == 0 && type <= OBJECT_TYPE_ACTIVE)
					screen_color (&t->pg, t->vtp->flags,
						      p->data & 0x1F);
				break;

			case 0x07:		/* address display row 0 */
				if (p->address != 0x3F)
					break; /* reserved, no position */

				row = 0;

				/* fall through */

			case 0x01:		/* full row color */
				row_color = next_row_color;

				if (s == 0) {
					row_color = p->data & 0x1F;
					next_row_color = t->ext->def_row_color;
				} else if (s == 3) {
					row_color = p->data & 0x1F;
					next_row_color = row_color;
				}

				goto set_active;

			case 0x02:		/* reserved */
			case 0x03:		/* reserved */
				break;

			case 0x04:		/* set active position */
				if (t->max_level >= VBI_WST_LEVEL_2p5) {
					if (p->data >= 40)
						break; /* reserved */

					column = p->data;
				}

				if (row > active_row)
					row_color = next_row_color;

			set_active:
				if (row > 0 && 1 == t->pg.rows) {
					for (; n_triplets > 1; ++p, --n_triplets)
						if (p[1].address >= 40) {
							unsigned int mode;

							mode = p[1].mode;

							if (mode == 0x07)
								break;
							else if (mode >= 0x1F)
								goto terminate;
						}
					break;
				}

				fmt_log ("enh set_active row %d col %d\n",
					 row, column);

				if (row > active_row) {
					enhance_flush (-1); /* flush row */

					if (type != OBJECT_TYPE_PASSIVE)
						CLEAR (mac);
				}

				active_row = row;
				active_column = column;

				acp = &t->pg.text[(inv_row + active_row)
						  * t->pg.columns];
				break;

			case 0x05:		/* reserved */
			case 0x06:		/* reserved */
				break;

			case 0x08:		/* PDC data - Country of Origin
						   and Programme Source */
				pdc_tmp.cni = p->address * 256 + p->data;
				break;

			case 0x09:		/* PDC data - Month and Day */
				pdc_tmp.ad.month = (p->address & 15) - 1;
				pdc_tmp.ad.day = (p->data >> 4) * 10
					+ (p->data & 15) - 1;
				break;

			case 0x0A:		/* PDC data - Cursor Row and
						   Announced Starting Time Hours */
				if (!t->pdc_links)
					break;

				if (pdc_tmp.ad.month < 0
				    || pdc_tmp.cni == 0) {
					return FALSE;
				}

				pdc_tmp.at2.hour = ((p->data & 0x30) >> 4) * 10
					+ (p->data & 15);
				pdc_tmp.length = 0;
				pdc_tmp.at1_ptl_pos[1].row = row;
				pdc_tmp.caf = !!(p->data & 0x40);

				pdc_hour_mode = p->mode;
				break;

			case 0x0B:		/* PDC data - Cursor Row and
						   Announced Finishing Time Hours */
				pdc_tmp.at2.hour =
					((p->data & 0x70) >> 4) * 10
					+ (p->data & 15);

				pdc_hour_mode = p->mode;
				break;

			case 0x0C:		/* PDC data - Cursor Row and
						   Local Time Offset */
				pdc_tmp.lto = p->data;

				if (p->data & 0x40) /* 6 bit two's complement */
					pdc_tmp.lto |= ~0x7F;

				break;

			case 0x0D:		/* PDC data - Series Identifier
						   and Series Code */
				if (p->address == 0x30) {
					break;
				}

				pdc_tmp.pty = 0x80 + p->data;

				break;

			case 0x0E:		/* reserved */
			case 0x0F:		/* reserved */
				break;

			case 0x10:		/* origin modifier */
				if (t->max_level < VBI_WST_LEVEL_2p5)
					break;

				if (p->data >= 72)
					break; /* invalid */

				offset_column = p->data;
				offset_row = p->address - 40;

				fmt_log ("enh origin modifier col %+d row %+d\n",
					 offset_column, offset_row);

				break;

			case 0x11 ... 0x13:	/* object invocation */
				if (t->max_level < VBI_WST_LEVEL_2p5)
					break;

				row = inv_row + active_row;
				column = inv_column + active_column;

				if (!object_invocation (t, type, p,
							row + offset_row,
							column + offset_column))
					return FALSE;

				offset_row = 0;
				offset_column = 0;

				break;

			case 0x14:		/* reserved */
				break;

			case 0x15 ... 0x17:	/* object definition */
				fmt_log ("enh obj definition 0x%02x 0x%02x\n",
					 p->mode, p->data);
				goto terminate;

			case 0x18:		/* drcs mode */
				fmt_log ("enh DRCS mode 0x%02x\n", p->data);
				drcs_s1[p->data >> 6] = p->data & 15;
				break;

			case 0x19 ... 0x1E:	/* reserved */
				break;

			case 0x1F:		/* termination marker */
			default:
	                terminate:
				enhance_flush (-1); /* flush row */
				fmt_log ("enh terminated %02x\n", p->mode);
				goto finish;
			}
		} else {
			/*
			 *  Column address triplets
			 */

			int s = p->data >> 5;
			int column = p->address;
			int unicode;

			switch (p->mode) {
			case 0x00:		/* foreground color */
				if (t->max_level >= VBI_WST_LEVEL_2p5 && s == 0) {
					if (column > active_column)
						enhance_flush (column);

					ac.foreground = p->data & 0x1F;
					mac.foreground = ~0;

					fmt_log ("enh col %d foreground %d\n",
						 active_column, ac.foreground);
				}

				break;

			case 0x01:		/* G1 block mosaic character */
				if (t->max_level >= VBI_WST_LEVEL_2p5) {
					if (column > active_column)
						enhance_flush(column);

					if (p->data & 0x20) {
						/* G1 contiguous */
						unicode = 0xEE00 + p->data;

						goto store;
					} else if (p->data >= 0x40) {
						unicode = vbi_teletext_unicode
						       (cs->g0, VBI_SUBSET_NONE, p->data);
						goto store;
					}
				}

				break;

			case 0x0B:		/* G3 smooth mosaic or
						   line drawing character */
				if (t->max_level < VBI_WST_LEVEL_2p5)
					break;

				/* fall through */

			case 0x02:		/* G3 smooth mosaic or
						   line drawing character */
				if (p->data >= 0x20) {
					if (column > active_column)
						enhance_flush(column);

					unicode = 0xEF00 + p->data;
					goto store;
				}

				break;

			case 0x03:		/* background color */
				if (t->max_level >= VBI_WST_LEVEL_2p5 && s == 0) {
					if (column > active_column)
						enhance_flush(column);

					ac.background = p->data & 0x1F;
					mac.background = ~0;

					fmt_log ("enh col %d background %d\n",
						 active_column, ac.background);
				}

				break;

			case 0x04:		/* reserved */
			case 0x05:		/* reserved */
				break;

			case 0x06:		/* PDC data - Cursor Column and
						   Announced Starting and
						   Finishing Time Minutes */
				if (!t->pdc_links)
					break;

				pdc_tmp.at2.min = (p->data >> 4) * 10
					+ (p->data & 15);

				switch (pdc_hour_mode) {
				case 0x0A: /* Starting time */
					if (p1 > t->pdc_table && p1[-1].length == 0) {
						p1[-1].length = pdc_time_diff
							(&p1[-1].at2, &pdc_tmp.at2);

						if (p1[-1].length >= 12 * 60) {
							/* nonsense */
							--p1;
						}
					}

					pdc_tmp.at1_ptl_pos[1].column_begin = column;
					pdc_tmp.at1_ptl_pos[1].column_end = 40;

					if (p1 >= t->pdc_table
					    + N_ELEMENTS (t->pdc_table))
						return FALSE;

					*p1++ = pdc_tmp;

					pdc_tmp.pty = 0;

					break;

				case 0x0B: /* Finishing time */
					if (p1 == t->pdc_table)
						return FALSE;

					if (pdc_tmp.at2.hour >= 40) {
						/* Duration */
						p1[-1].length =
							(pdc_tmp.at2.hour - 40) * 60
							+ pdc_tmp.at2.min;
					} else {
						/* End time */
						p1[-1].length = pdc_time_diff
							(&p1[-1].at2, &pdc_tmp.at2);
					}

					break;

				default:
					fmt_log ("pdc hour triplet missing\n");
					return FALSE;
				}

				pdc_hour_mode = 0;

				break;

			case 0x07:		/* additional flash functions */
				if (t->max_level >= VBI_WST_LEVEL_2p5) {
					if (column > active_column)
						enhance_flush(column);

					/*
					 *  Only one flash function (if any)
					 *  implemented:
					 *  Mode 1 - Normal flash to background color
					 *  Rate 0 - Slow rate (1 Hz)
					 */
					COPY_SET_COND (ac.attr, VBI_FLASH,
						       p->data & 3);
					mac.attr |= VBI_FLASH;

					fmt_log ("enh col %d flash 0x%02x\n",
						 active_column, p->data);
				}

				break;

			case 0x08:		/* modified G0 and G2
						   character set designation */
				if (t->max_level >= VBI_WST_LEVEL_2p5) {
					const vbi_character_set *cs1;

					if (column > active_column)
						enhance_flush(column);

					if ((cs1 = vbi_character_set_from_code (p->data)))
						cs = cs1;

					fmt_log ("enh col %d modify character "
						 "set %d\n", active_column, p->data);
				}

				break;

			case 0x09:		/* G0 character */
				if (t->max_level >= VBI_WST_LEVEL_2p5
				    && p->data >= 0x20) {
					if (column > active_column)
						enhance_flush(column);

					unicode = vbi_teletext_unicode
						(cs->g0, VBI_SUBSET_NONE, p->data);
					goto store;
				}

				break;

			case 0x0A:		/* reserved */
				break;

			case 0x0C:		/* display attributes */
				if (t->max_level < VBI_WST_LEVEL_2p5)
					break;

				if (column > active_column)
					enhance_flush(column);

				ac.size = ((p->data & 0x40) ? VBI_DOUBLE_WIDTH : 0)
					+ ((p->data & 1) ? VBI_DOUBLE_HEIGHT : 0);
				mac.size = ~0;

				if (p->data & 2) {
					if (t->vtp->flags & (C5_NEWSFLASH | C6_SUBTITLE))
						ac.opacity = VBI_SEMI_TRANSPARENT;
					else
						ac.opacity = VBI_TRANSPARENT_SPACE;
				} else
					ac.opacity = t->page_opacity[1];
				mac.opacity = ~0;

				COPY_SET_COND (ac.attr, VBI_CONCEAL,
					       p->data & 4);
				mac.attr |= VBI_CONCEAL;

				/* (p->data & 8) reserved */

				invert = p->data & 0x10;

				COPY_SET_COND (ac.attr, VBI_UNDERLINE,
					       p->data & 0x20);
				mac.attr |= VBI_UNDERLINE;

				fmt_log ("enh col %d display attr 0x%02x\n",
					 active_column, p->data);

				break;

			case 0x0D:		/* drcs character invocation */
			{
				unsigned int normal;
				unsigned int offset;
				unsigned int page;

				if (t->max_level < VBI_WST_LEVEL_2p5)
					break;

				normal = p->data >> 6;
				offset = p->data & 0x3F;

				if (offset >= 48) {
					fmt_log ("invalid drcs offset %u\n", offset);
					break;
				}

				if (column > active_column)
					enhance_flush (column);

				page = normal * 16 + drcs_s1[normal];

				fmt_log ("enh col %d DRCS %d/0x%02x\n",
					 active_column, page, p->data);

				if (!reference_drcs_page (t, normal,
							  page, offset,
							  drcs_s1[normal]))
					return FALSE;

				unicode = 0xF000 + (page << 6) + offset;

				goto store;
			}

			case 0x0E:		/* font style */
			{
				int col, row, count;
				unsigned int attr;
				vbi_char *acp;

				if (t->max_level < VBI_WST_LEVEL_3p5)
					break;

				row = inv_row + active_row;
				count = (p->data >> 4) + 1;
				acp = &t->pg.text[row * t->pg.columns];

				attr = 0;
				if (p->data & 0x01)
					attr |= VBI_PROPORTIONAL;
				if (p->data & 0x02)
					attr |= VBI_BOLD;
				if (p->data & 0x04)
					attr |= VBI_ITALIC;

				while (row < 25 && count > 0) {
					for (col = inv_column + column;
					     col < 40; ++col) {
						acp[col].attr = (acp[col].attr & ~(VBI_PROPORTIONAL | VBI_BOLD | VBI_ITALIC)) | attr;
					}

					acp += t->pg.columns;
					row++;
					count--;
				}

				fmt_log ("enh col %d font style 0x%02x\n",
					 active_column, p->data);

				break;
			}

			case 0x0F:		/* G2 character */
				if (p->data >= 0x20) {
					if (column > active_column)
						enhance_flush(column);

					unicode = vbi_teletext_unicode
						(cs->g2, VBI_SUBSET_NONE, p->data);
					goto store;
				}

				break;

			case 0x10 ... 0x1F:	/* characters including
						   diacritical marks */
				if (p->data >= 0x20) {
					if (column > active_column)
						enhance_flush(column);

					unicode = vbi_teletext_composed_unicode
						(p->mode - 0x10, p->data);
			store:
					fmt_log ("enh row %d col %d print "
						 "0x%02x/0x%02x -> 0x%04x %c\n",
						 active_row, active_column,
						 p->mode, p->data,
						 unicode, unicode & 0xFF);

					ac.unicode = unicode;
					mac.unicode = ~0;
				}

				break;
			}
		}
	}

finish:
	if (p1 > t->pdc_table) {
		if (pdc_hour_mode || p1[-1].length == 0)
			p1--; /* incomplete start or end tag */

		if (0)
			pdc_program_array_dump (t->pdc_table,
						p1 - t->pdc_table,
						stderr);
	}

	if (0)
		vbi_page_private_dump (t, 2, stderr);

	return TRUE;
}

static void
post_enhance			(vbi_page_private *		t)
{
	vbi_char ac, *acp;
	unsigned int row;
	unsigned int column;
	unsigned int last_row;
	unsigned int last_column;

	acp = t->pg.text;

	last_row = (1 == t->pg.rows) ? 0 : 25 - 2;
	last_column = t->pg.columns - 1;

	for (row = 0; row <= last_row; ++row) {
		for (column = 0; column < t->pg.columns; ++acp, ++column) {
			if (acp->opacity == VBI_TRANSPARENT_SPACE
			    || (acp->foreground == VBI_TRANSPARENT_BLACK
				&& acp->background == VBI_TRANSPARENT_BLACK)) {
				acp->opacity = VBI_TRANSPARENT_SPACE;
				acp->unicode = 0x0020;
			} else if (acp->background == VBI_TRANSPARENT_BLACK) {
				acp->opacity = VBI_SEMI_TRANSPARENT;
			} else {
				/* transparent foreground not implemented */
			}

			switch (acp->size) {
			case VBI_NORMAL_SIZE:
				if (row < last_row
				    && (acp[t->pg.columns].size == VBI_DOUBLE_HEIGHT2
					|| acp[t->pg.columns].size == VBI_DOUBLE_SIZE2)) {
					acp[t->pg.columns].unicode = 0x0020;
					acp[t->pg.columns].size = VBI_NORMAL_SIZE;
				}

				if (column < 39
				    && (acp[1].size == VBI_OVER_TOP
					|| acp[1].size == VBI_OVER_BOTTOM)) {
					acp[1].unicode = 0x0020;
					acp[1].size = VBI_NORMAL_SIZE;
				}

				break;

			case VBI_DOUBLE_HEIGHT:
				if (row < last_row) {
					ac = acp[0];
					ac.size = VBI_DOUBLE_HEIGHT2;
					acp[t->pg.columns] = ac;
				}

				break;

			case VBI_DOUBLE_SIZE:
				if (row < last_row) {
					ac = acp[0];
					ac.size = VBI_DOUBLE_SIZE2;
					acp[t->pg.columns] = ac;
					ac.size = VBI_OVER_BOTTOM;
					acp[t->pg.columns + 1] = ac;
				}

				/* fall through */

			case VBI_DOUBLE_WIDTH:
				if (column < 39) {
					ac = acp[0];
					ac.size = VBI_OVER_TOP;
					acp[1] = ac;
				}

				break;

			default:
				break;
			}
		}

		fmt_log ("\n");
	}
}

/* Artificial 41st column. Often column 0 of a LOP contains only set-after
   attributes and thus all black spaces, unlike column 39. To balance the
   view we add a black column 40. If OTOH column 0 has been modified using
   enhancement we extend column 39. */
static void
column_41			(vbi_page_private *		t)
{
	vbi_char *acp;
	unsigned int row;
	vbi_bool black0;
	vbi_bool cont39;

	acp = t->pg.text;

	/* Header. */

	acp[40] = acp[39];
	acp[40].unicode = 0x0020;

	if (1 == t->pg.rows)
		return;

	acp += 41;

	black0 = TRUE;
	cont39 = TRUE;

	for (row = 1; row <= 24; ++row) {
		if (0x0020 != acp[0].unicode
		    || VBI_BLACK != acp[0].background) {
			black0 = FALSE;
		}

		if (vbi_is_gfx (acp[39].unicode)) {
			if (acp[38].unicode != acp[39].unicode
			    || acp[38].foreground != acp[39].foreground
			    || acp[38].background != acp[39].background) {
				cont39 = FALSE;
			}
		}

		acp += 41;
	}

	acp = t->pg.text + 41;

	if (!black0 && cont39) {
		for (row = 1; row <= 24; ++row) {
			acp[40] = acp[39];

			if (!vbi_is_gfx (acp[39].unicode))
				acp[40].unicode = 0x0020;

			acp += 41;
		}
	} else {
		vbi_char ac;

		CLEAR (ac);

		ac.unicode	= 0x0020;
		ac.foreground	= t->ext->foreground_clut + VBI_WHITE;
		ac.background	= t->ext->background_clut + VBI_BLACK;
		ac.opacity	= t->page_opacity[1];

		for (row = 1; row <= 24; ++row) {
			acp[40] = ac;
			acp += 41;
		}
	}

	/* Navigation bar. */

	acp[40] = acp[39];
	acp[40].unicode = 0x0020;
}

/**
 * @internal
 *
 * Formats a level one page. Source is pgp->vtp.
 */
static void
level_one_page			(vbi_page_private *	pgp)
{
	static const unsigned int mosaic_separate = 0xEE00 - 0x20;
	static const unsigned int mosaic_contiguous = 0xEE20 - 0x20;
	char buf[16];
	unsigned int column;
	unsigned int row;
	unsigned int i;

	/* Current page number in header */

	sprintf (buf, "\2%03x.%02x\7",
		 pgp->pg.pgno & 0xFFF,
		 pgp->pg.subno & 0xFF);

//	pgp->pg.double_height_lower = 0;

	i = 0; /* t->vtp->data.lop.raw[] */

	for (row = 0; row < pgp->pg.rows; ++row) {
		const vbi_character_set *cs;
		unsigned int mosaic_plane;
		unsigned int held_mosaic_unicode;
		vbi_bool hold;
		vbi_bool mosaic;
		vbi_bool double_height;
		vbi_bool wide_char;
		vbi_char ac, *acp;

		acp = pgp->pg.text + row * pgp->pg.columns;

		/* G1 block mosaic, blank, contiguous */
		held_mosaic_unicode = mosaic_contiguous + 0x20;

		CLEAR (ac);

		ac.unicode	= 0x0020;
		ac.foreground	= pgp->ext->foreground_clut + VBI_WHITE;
		ac.background	= pgp->ext->background_clut + VBI_BLACK;
		mosaic_plane	= mosaic_contiguous;
		ac.opacity	= pgp->page_opacity[row > 0];
		cs		= pgp->char_set[0];
		hold		= FALSE;
		mosaic		= FALSE;

		double_height	= FALSE;
		wide_char	= FALSE;

		for (column = 0; column < 40; ++column) {
			int raw;

			if (row == 0 && column < 8) {
				raw = buf[column];
				++i;
			} else {
				raw = vbi_ipar8
					(pgp->vtp->data.lop.raw[0][i++]);

				if (raw < 0) /* parity error */
					raw = 0x20;
			}

			/* Set-at spacing attributes */

			switch (raw) {
			case 0x09:		/* steady */
				ac.attr &= ~VBI_FLASH;
				break;

			case 0x0C:		/* normal size */
				ac.size = VBI_NORMAL_SIZE;
				break;

			case 0x18:		/* conceal */
				ac.attr |= VBI_CONCEAL;
				break;

			case 0x19:		/* contiguous mosaics */
				mosaic_plane = mosaic_contiguous;
				break;

			case 0x1A:		/* separated mosaics */
				mosaic_plane = mosaic_separate;
				break;

			case 0x1C:		/* black background */
				ac.background = pgp->ext->background_clut
					+ VBI_BLACK;
				break;

			case 0x1D:		/* new background */
				ac.background = pgp->ext->background_clut
					+ (ac.foreground & 7);
				break;

			case 0x1E:		/* hold mosaic */
				hold = TRUE;
				break;

			default:
				break;
			}

			if (raw <= 0x1F) {
				ac.unicode = (hold & mosaic) ?
					held_mosaic_unicode : 0x0020;
			} else {
				if (mosaic && (raw & 0x20)) {
					held_mosaic_unicode =
						mosaic_plane + raw;
					ac.unicode = held_mosaic_unicode;
				} else {
					ac.unicode = vbi_teletext_unicode
						(cs->g0, cs->subset, raw);
				}
			}

			if (!wide_char) {
				acp[column] = ac;

				wide_char = /* !! */
					(ac.size & VBI_DOUBLE_WIDTH);

				if (wide_char && column < 39) {
					acp[column + 1] = ac;
					acp[column + 1].size = VBI_OVER_TOP;
				}
			}

			/* Set-after spacing attributes */

			switch (raw) {
			case 0x00 ... 0x07:	/* alpha + foreground color */
				ac.foreground = pgp->ext->foreground_clut
					+ (raw & 7);
				ac.attr &= ~VBI_CONCEAL;
				mosaic = FALSE;
				break;

			case 0x08:		/* flash */
				ac.attr |= VBI_FLASH;
				break;

			case 0x0A:		/* end box */
				if (column >= 39)
					break;
				if (0x0A == vbi_ipar8
				    (pgp->vtp->data.lop.raw[0][i]))
					ac.opacity =
						pgp->page_opacity[row > 0];
				break;

			case 0x0B:		/* start box */
				if (column >= 39)
					break;
				if (0x0B == vbi_ipar8
				    (pgp->vtp->data.lop.raw[0][i]))
					ac.opacity =
						pgp->boxed_opacity[row > 0];
				break;

			case 0x0D:		/* double height */
				if (row == 0 || row >= 23)
					break;
				ac.size = VBI_DOUBLE_HEIGHT;
				double_height = TRUE;
				break;

			case 0x0E:		/* double width */
				fmt_log ("spacing col %d row %d double width\n",
					column, row);
				if (column >= 39)
					break;
				ac.size = VBI_DOUBLE_WIDTH;
				break;

			case 0x0F:		/* double size */
				fmt_log ("spacing col %d row %d double size\n",
					column, row);
				if (column >= 39 || row == 0 || row >= 23)
					break;
				ac.size = VBI_DOUBLE_SIZE;
				double_height = TRUE;
				break;

			case 0x10 ... 0x17:	/* mosaic + foreground color */
				ac.foreground = pgp->ext->foreground_clut
					+ (raw & 7);
				ac.attr &= ~VBI_CONCEAL;
				mosaic = TRUE;
				break;

			case 0x1B:		/* ESC */
				cs = pgp->char_set[cs == pgp->char_set[0]];
				break;

			case 0x1F:		/* release mosaic */
				hold = FALSE;
				break;
			}
		}

		if (double_height) {
			for (column = 0; column < pgp->pg.columns; ++column) {
				ac = acp[column];

				switch (ac.size) {
				case VBI_DOUBLE_HEIGHT:
					ac.size = VBI_DOUBLE_HEIGHT2;
					acp[pgp->pg.columns + column] = ac;
					break;
		
				case VBI_DOUBLE_SIZE:
					ac.size = VBI_DOUBLE_SIZE2;
					acp[pgp->pg.columns + column] = ac;
					++column;
					ac.size = VBI_OVER_BOTTOM;
					acp[pgp->pg.columns + column] = ac;
					break;

				default: /* NORMAL, DOUBLE_WIDTH, OVER_TOP */
					ac.size = VBI_NORMAL_SIZE;
					ac.unicode = 0x0020;
					acp[pgp->pg.columns + column] = ac;
					break;
				}
			}

			i += 40;
			++row;

//			pgp->pg.double_height_lower |= 1 << row;
		}
	}

	if (0) {
		if (row < 25) {
			vbi_char ac;

			CLEAR (ac);

			ac.foreground	= pgp->ext->foreground_clut + VBI_WHITE;
			ac.background	= pgp->ext->background_clut + VBI_BLACK;
			ac.opacity	= pgp->page_opacity[1];
			ac.unicode	= 0x0020;

			for (i = row * pgp->pg.columns;
			     i < pgp->pg.rows * pgp->pg.columns; ++i)
				pgp->pg.text[i] = ac;
		}
	}

	if (0)
		vbi_page_private_dump (pgp, 0, stderr);
}

/* Hyperlinks ------------------------------------------------------------- */

static const uint8_t SECTION_SIGN = 0xA7;

static unsigned int
keycmp				(const char *		s1,
				 const char *		key)
{
	const char *s;
	char c;

	for (s = s1; (c = *key); ++s, ++key) {
		if (*s == c)
			continue;

		if (c >= 'a' && c <= 'z')
			if ((*s ^ c) == 0x20)
				continue;

		return 0;
	}

	return s - s1;
}

static vbi_bool
keyword				(vbi_link *		ld,
				 const char *		buf,
				 vbi_pgno		pgno,
				 vbi_subno		subno,
				 unsigned int *		start,
				 unsigned int *		end)
{
	const char *s;
	unsigned int proto;
	unsigned int address;

	ld->type	= VBI_LINK_NONE;
	ld->name[0]	= 0;
	ld->url[0]	= 0;
	ld->pgno	= 0;
	ld->subno	= VBI_ANY_SUBNO;

	s = buf + *start;
	*end = *start + 1; /* unknown character */

	if (isdigit (*s)) {
		const char *s1;
		unsigned int n1, n2;
		unsigned int digits;

		s1 = s;
		n1 = 0;

		do n1 = n1 * 16 + (*s & 15);
		while (isdigit (*++s));

		digits = s - s1;
		*end += digits - 1;

		if (digits > 3 || isdigit (s1[-1]))
			return FALSE;

		if (digits == 3) {
			if (pgno == (vbi_pgno) n1)
				return FALSE;

			if (n1 < 0x100 || n1 > 0x899)
				return FALSE;

			ld->type = VBI_LINK_PAGE;
			ld->pgno = n1;

			return TRUE;
		}

		if (*s != '/' && *s != ':')
			return FALSE;

		s1 = ++s;
		n2 = 0;

		while (isdigit (*s))
			n2 = n2 * 16 + (*s++ & 15);

		digits = s - s1; 
		*end += digits + 1;

		if (digits == 0 || digits > 2
		    || subno != (vbi_subno) n1)
			return FALSE;

		ld->type = VBI_LINK_SUBPAGE;
		ld->pgno = pgno;

		if (n1 == n2)
			ld->subno = 0x01; /* subpage "n1/n2" -> "1/n2" */
		else
			ld->subno = vbi_add_bcd (n1, 0x01);

		return TRUE;
	} else if (*s == '>' && s[1] == '>' && s[-1] != '>') {
		for (s += 2; *s == 0x20; ++s)
			;

		*end = s - buf;

		if (*s)
			return FALSE;

		if (subno == 0 || subno == VBI_ANY_SUBNO) {
			if (pgno == 0x899)
				return FALSE;

			ld->type = VBI_LINK_PAGE;
			ld->pgno = vbi_add_bcd (pgno, 0x001);
			return TRUE;
		} else if (subno < 0x99) {
			/* XXX wrap? */

			ld->type = VBI_LINK_SUBPAGE;
			ld->pgno = pgno;
			ld->subno = vbi_add_bcd (subno, 0x01);
			return TRUE;
		}

		return FALSE;
	} else if (*s == 'h') {
		if (!(proto = keycmp (s, "https://"))
		    && !(proto = keycmp (s, "http://")))
			return FALSE;
		ld->type = VBI_LINK_HTTP;
	} else if (*s == '(') {
		if (!(proto = keycmp (s, "(at)"))
		    && !(proto = keycmp (s, "(a)")))
			return FALSE;
		ld->type = VBI_LINK_EMAIL;
		strcpy (ld->url, "mailto:");
	} else if ((proto = keycmp (s, "www."))) {
		ld->type = VBI_LINK_HTTP;
		strcpy (ld->url, "http://");
	} else if ((proto = keycmp (s, "ftp://"))) {
		ld->type = VBI_LINK_FTP;
	} else if (*s == '@' || *s == SECTION_SIGN) {
		ld->type = VBI_LINK_EMAIL;
		strcpy (ld->url, "mailto:");
		proto = 1;
	} else {
		return FALSE;
	}

	*end = *start + proto;

	{
		const char *s1;
		unsigned int domains;

		s += proto;
		s1 = s;
		domains = 0;

		for (;;) {
			static const char *valid = "%&/=?+-~:;@_"; /* RFC 1738 */
			const char *s1;
			
			s1 = s;

			while (isalnum (*s) || strchr (valid, *s))
				++s;

			if (s == s1)
				return FALSE;

			if (*s != '.')
				break;

			++s;
			++domains;
		}

		if (domains == 0) {
			ld->type = VBI_LINK_NONE;
			return FALSE;
		}

		address = s - s1;
	}

	*end += address;

	if (ld->type == VBI_LINK_EMAIL) {
		static const char *valid = "-~._";
		const char *s1;
		unsigned int recipient;

		s = buf + *start;
		s1 = s;

		while (isalnum (s[-1]) || strchr (valid, s[-1]))
			--s;

		recipient = s1 - s;

		if (recipient == 0) {
			ld->type = VBI_LINK_NONE;
			return FALSE;
		}

		*start -= recipient;

		assert (sizeof (ld->url) > 43);

		strncat (ld->url, s1 - recipient, recipient);
		strcat  (ld->url, "@");
		strncat (ld->url, s1 + proto, address);
	} else {
		assert (sizeof (ld->url) > 43);

		strncat (ld->url, buf + *start, proto + address);
	}

	return TRUE;
}

/*
 *  Analyses page contents and adds link flags when keywords appear:
 *  http and ftp urls, e-mail addresses, 3-digit page
 *  numbers, subpage numbers "n/m", next page ">>".
 */
vbi_inline void
hyperlinks			(vbi_page_private *		t,
				 unsigned int		row)
{
	unsigned char buffer[43]; /* one row, two spaces on the sides and NUL */
	vbi_bool link[43];
	vbi_link ld;
	vbi_char *acp;
	unsigned int i;
	unsigned int j;

	acp = t->pg.text + row * t->pg.columns;

	for (i = j = 0; i < 40; ++i) {
		if (acp[i].size == VBI_OVER_TOP
		    || acp[i].size == VBI_OVER_BOTTOM)
			continue;
		++j;

		if (acp[i].unicode >= 0x20 && acp[i].unicode <= 0xFF)
			buffer[j] = acp[i].unicode;
		else
			buffer[j] = 0x20;
	}

	buffer[0] = ' '; 
	buffer[j + 1] = ' ';
	buffer[j + 2] = 0;

	CLEAR (link);

	i = 0;

	while (i < 40) {
		unsigned int end;

		if (keyword (&ld, buffer, t->pg.pgno, t->pg.subno, &i, &end)) {
			for (j = i; j < end; ++j)
				link[j] = TRUE;
		}

		i = end;
	}

	for (i = 0, j = 1; i < 40; ++i) {
		if (acp[i].size == VBI_OVER_TOP
		    || acp[i].size == VBI_OVER_BOTTOM)
			continue;

		COPY_SET_COND (acp[i].attr, VBI_LINK, link[j++]);
	}
}

/**
 * @param pg With vbi_fetch_vt_page() obtained vbi_page.
 * @param ld Place to store information about the link.
 * @param column Column 0 ... pg->columns - 1 of the character in question.
 * @param row Row 0 ... pg->rows - 1 of the character in question.
 * 
 * A vbi_page (in practice only Teletext pages) may contain hyperlinks
 * such as HTTP URLs, e-mail addresses or links to other pages. Characters
 * being part of a hyperlink have a set vbi_char->link flag, this function
 * returns a more verbose description of the link.
 *
 * @returns
 * TRUE if the link is valid.
 */
vbi_bool
vbi_page_hyperlink		(const vbi_page *	pg,
				 vbi_link *		ld,
				 unsigned int		column,
				 unsigned int		row)
{
	const vbi_page_private *pgp;
	char buffer[43];
	const vbi_char *acp;
	unsigned int i;
	unsigned int j;
	unsigned int start;
	unsigned int end;

	PGP_CHECK (FALSE);

	assert (ld != NULL);
	assert (column < pg->columns);
	assert (row < pg->rows);

	ld->nuid = pg->nuid;
	ld->type = VBI_LINK_NONE;

	acp = pg->text + row * pg->columns;

	if (row == 0 || pg->pgno < 0x100
	    || !(acp[column].attr & VBI_LINK))
		return FALSE;

	if (row == 25) {
		unsigned int i;

		i = pgp->nav_index[column];

		ld->type	= VBI_LINK_PAGE;
		ld->pgno	= pgp->nav_link[i].pgno;
		ld->subno	= pgp->nav_link[i].subno;

		return TRUE;
	}

	start = 0;

	for (i = j = 0; i < 40; ++i) {
		if (acp[i].size == VBI_OVER_TOP
		    || acp[i].size == VBI_OVER_BOTTOM)
			continue;
		++j;

		if (i < column && !(acp[i].attr & VBI_LINK))
			start = j + 1;

		if (acp[i].unicode >= 0x20 && acp[i].unicode <= 0xFF)
			buffer[j] = acp[i].unicode;
		else
			buffer[j] = 0x20;
	}

	buffer[0] = ' '; 
	buffer[j + 1] = ' ';
	buffer[j + 2] = 0;

	return keyword (ld, buffer, pg->pgno, pg->subno, &start, &end);
}

/* Navigation enhancements ------------------------------------------------ */

#ifndef TELETEXT_NAV_LOG
#define TELETEXT_NAV_LOG 0
#endif

#define nav_log(templ, args...)						\
do { /* Check syntax in any case */					\
	if (TELETEXT_NAV_LOG)						\
		fprintf (stderr, templ , ##args);			\
} while (0)

/*
 *  FLOF navigation
 */

static const vbi_color
flof_link_col [4] = {
	VBI_RED,
	VBI_GREEN,
	VBI_YELLOW,
	VBI_CYAN
};

vbi_inline vbi_char *
navigation_row			(vbi_page_private *		t)
{
	return t->pg.text + 25 * t->pg.columns;
}

static vbi_char *
clear_navigation_bar		(vbi_page_private *		t)
{
	vbi_char ac, *acp;
	unsigned int i;

	acp = navigation_row (t);

	CLEAR (ac);

	ac.foreground	= 32 + VBI_WHITE; /* 32: immutable color */
	ac.background	= 32 + VBI_BLACK;
	ac.opacity	= t->page_opacity[1];
	ac.unicode	= 0x0020;

	for (i = 0; i < t->pg.columns; ++i) {
		acp[i] = ac;
	}

	return acp;
}

/*
 *  We have FLOF links but no labels in row 25. This function replaces
 *  row 25 using the FLOF page numbers as labels.
 */
vbi_inline void
flof_navigation_bar		(vbi_page_private *		t)
{
	vbi_char ac, *acp;
	unsigned int i;

	acp = clear_navigation_bar (t);

	ac = *acp;
	ac.attr |= VBI_LINK;

	for (i = 0; i < 4; ++i) {
		unsigned int pos;
		unsigned int k;

		pos = i * 10 + 3;

		ac.foreground = flof_link_col[i];

		for (k = 0; k < 3; ++k) {
			unsigned int digit;

			digit = t->vtp->data.lop.link[i].pgno;
			digit = (digit >> ((2 - k) * 4)) & 15;

			if (digit > 9)
				ac.unicode = digit + 'A' - 9;
			else
				ac.unicode = digit + '0';

			acp[pos + k] = ac;

			t->nav_index[pos + k] = i;
		}

		t->nav_link[i] = t->vtp->data.lop.link[i];
	}
}

/*
 *  Adds link flags to a page navigation bar (row 25) from FLOF data.
 */
vbi_inline void
flof_links		(vbi_page_private *		t)
{
	vbi_char *acp;
	unsigned int start;
	unsigned int end;
	unsigned int i;
	int color;

	acp = navigation_row (t);

	start = 0;
	color = -1;

	for (i = 0; i <= 40; ++i) {
		if (i == 40 || (acp[i].foreground & 7) != color) {
			unsigned int k;

			for (k = 0; k < 4; ++k)
				if (flof_link_col[k] == (unsigned int) color)
					break;

			if (k < 4 && !NO_PAGE (t->vtp->data.lop.link[k].pgno)) {
				/* Leading and trailing spaces not sensitive */

				for (end = i - 1; end >= start
					     && acp[end].unicode == 0x0020; --end)
					;

				for (; end >= start; --end) {
					acp[end].attr |= VBI_LINK;
					t->nav_index[end] = k;
				}

		    		t->nav_link[k] = t->vtp->data.lop.link[k];
			}

			if (i >= 40)
				break;

			color = acp[i].foreground & 7;
			start = i;
		}

		if (start == i && acp[i].unicode == 0x0020)
			++start;
	}
}

/*
 *  TOP navigation
 */

/*
 *  Finds the TOP page label for pgno and stores in row 25 at
 *  index 0-2 using font and foreground color. ff adds 0-2 '>'
 *  chars. This is a subfunction of top_navigation_bar().
 */
static vbi_bool
top_label			(vbi_page_private *		t,
				 const vbi_character_set *cs,
				 unsigned int		index,
				 vbi_pgno		pgno,
				 vbi_color		foreground,
				 unsigned int		ff)
{
	vbi_char *acp;
	unsigned int column;
	unsigned int i;

	acp = navigation_row (t);

	column = index * 13 + 1;

	for (i = 0; i < 8; ++i) {
		const vt_page *vtp;
		const ait_entry *ait;
		unsigned int j;

		vtp = NULL;

		if (t->pg.vbi->vt.btt_link[i].type != 2)
			goto cont;

		vtp = vbi_cache_get (t->pg.vbi, NUID0,
				     t->pg.vbi->vt.btt_link[i].pgno,
				     t->pg.vbi->vt.btt_link[i].subno, 0x3f7f,
				     /* user access */ FALSE);
		if (!vtp) {
			nav_log ("top ait page %x not cached\n",
				 t->pg.vbi->vt.btt_link[i].pgno);
			goto cont;
		} else if (vtp->function != PAGE_FUNCTION_AIT) {
			nav_log ("no ait page %x\n", vtp->pgno);
			goto cont;
		}

		for (ait = vtp->data.ait, j = 0; j < 46; ++ait, ++j) {
			unsigned int sh;
			int i;

			if (ait->page.pgno != pgno)
				continue;

			t->nav_link[index].pgno = pgno;
			t->nav_link[index].subno = VBI_ANY_SUBNO;

			for (i = 11; i >= 0; --i)
				if (ait->text[i] > 0x20)
					break;

			if (ff && (i <= (int)(11 - ff))) {
				sh = (11 - ff - i) >> 1;

				acp += sh;
				column += sh;

				acp[i + 1].attr |= VBI_LINK;
				t->nav_index[column + i + 1] = index;

				acp[i + 2].unicode = 0x003E;
				acp[i + 2].foreground = foreground;
				acp[i + 2].attr |= VBI_LINK;
				t->nav_index[column + i + 2] = index;

				if (ff > 1) {
					acp[i + 3].unicode = 0x003E;
					acp[i + 3].foreground = foreground;
					acp[i + 3].attr |= VBI_LINK;
					t->nav_index[column + i + 3] = index;
				}
			} else {
				sh = (11 - i) >> 1;

				acp += sh;
				column += sh;
			}

			for (; i >= 0; --i) {
				acp[i].unicode = vbi_teletext_unicode
					(cs->g0, cs->subset,
					 MAX (ait->text[i], (uint8_t) 0x20));

				acp[i].foreground = foreground;
				acp[i].attr |= VBI_LINK;
				
				t->nav_index[column + i] = index;
			}

			vbi_cache_unref (t->pg.vbi, vtp);

			return TRUE;
		}
		
	cont:
		vbi_cache_unref (t->pg.vbi, vtp);
	}

	return FALSE;
}

vbi_inline vbi_pgno
add_modulo			(vbi_pgno		pgno,
				 int			incr)
{
	return ((pgno - 0x100 + incr) & 0x7FF) + 0x100;
}

/*
 *  Replaces row 25 by labels and links from TOP data.
 */
vbi_inline void
top_navigation_bar		(vbi_page_private *		t)
{
	vbi_pgno pgno;
	vbi_bool have_group;

	nav_log ("page mip/btt: %d\n",
		 t->pg.vbi->vt.page_info[t->vtp->pgno - 0x100].code);

	clear_navigation_bar (t);

	if (VBI_OPAQUE != t->page_opacity[1])
		return;

	/* Supposedly TOP bar should be formatted like
	   (prev page) (next group) (next block) (next page)
	   but Zapping already had page buttons in the GUI.
	   Will consider a switch later. */

	/* Item 1, current block/chapter */

	pgno = t->pg.pgno;

	do {
		vbi_page_type type;

		type = t->pg.vbi->vt.page_info[pgno - 0x100].code;

		if (type == VBI_TOP_BLOCK || type == VBI_TOP_GROUP) {
			// XXX font?
			top_label (t, t->char_set[0], 0, pgno, 32 + VBI_WHITE, 0);
			break;
		}

		pgno = add_modulo (pgno, -1);
	} while (pgno != t->pg.pgno);

	/* Item 2 & 3, next group and block */

	pgno = t->pg.pgno;
	have_group = FALSE;

	for (;;) {
		pgno = add_modulo (pgno, +1);

		if (pgno == t->vtp->pgno)
			break;

		// XXX font?

		switch (t->pg.vbi->vt.page_info[pgno - 0x100].code) {
		case VBI_TOP_BLOCK:
			top_label (t, t->char_set[0], 2,
				   pgno, 32 + VBI_YELLOW, 2);
			return;

		case VBI_TOP_GROUP:
			if (!have_group) {
				top_label (t, t->char_set[0], 1,
					   pgno, 32 + VBI_GREEN, 1);
				have_group = TRUE;
			}
			break;
		}
	}
}

/*
 *  These functions build an index page (pgno 0x9nn) from TOP
 *  data. Actually we should make the TOP data itself available
 *  and let the client build a menu.
 *
 *  XXX
 */

static const ait_entry *
next_ait(vbi_decoder *vbi, int pgno, int subno, const vt_page **mvtp)
{
	const ait_entry *ait;
	const ait_entry *mait = NULL;
	int mpgno = 0xFFF, msubno = 0xFFFF;
	int i, j;

	*mvtp = NULL;

	for (i = 0; i < 8; i++) {
		const vt_page *vtp, *xvtp = NULL;

		if (vbi->vt.btt_link[i].type != 2)
			continue;

		vtp = vbi_cache_get (vbi, NUID0,
				     vbi->vt.btt_link[i].pgno, 
				     vbi->vt.btt_link[i].subno, 0x3f7f,
				     /* user access */ FALSE);
		if (!vtp) {
			nav_log ("top ait page %x not cached\n",
			       vbi->vt.btt_link[i].pgno);
			continue;
		} else if (vtp->function != PAGE_FUNCTION_AIT) {
			nav_log ("no ait page %x\n", vtp->pgno);
			goto end;
		}

		for (ait = vtp->data.ait, j = 0; j < 46; ait++, j++) {
			if (!ait->page.pgno)
				break;

			if (ait->page.pgno < pgno
			    || (ait->page.pgno == pgno && ait->page.subno <= subno))
				continue;

			if (ait->page.pgno > mpgno
			    || (ait->page.pgno == mpgno && ait->page.subno > msubno))
				continue;

			mait = ait;
			mpgno = ait->page.pgno;
			msubno = ait->page.subno;
			xvtp = vtp;
		}

		if (xvtp) {
			vbi_cache_unref (vbi, *mvtp);
			*mvtp = xvtp;
		} else {
		end:
			vbi_cache_unref (vbi, vtp);
		}
	}

	return mait;
}

static int
top_index(vbi_decoder *vbi, vbi_page *pg, int subno, int columns)
{
	const vt_page *vtp;
	vbi_char ac, *acp;
	const ait_entry *ait;
	int i, j, k, n, lines;
	int xpgno, xsubno;
	const vt_extension *ext;
	char *index_str;
	unsigned int ii;

	pg->vbi = vbi;

	subno = vbi_bcd2dec(subno);

	pg->rows = 25;
	pg->columns = columns;

	pg->dirty.y0 = 0;
	pg->dirty.y1 = 25 - 1;
	pg->dirty.roll = 0;

	ext = &vbi->vt.magazine[0].extension;

	screen_color(pg, 0, 32 + VBI_BLUE);

	assert (sizeof (pg->color_map) == sizeof (ext->color_map));
	memcpy (pg->color_map, ext->color_map, sizeof (pg->color_map));

	assert (sizeof (pg->drcs_clut) == sizeof (ext->drcs_clut));
	memcpy (pg->drcs_clut, ext->drcs_clut, sizeof (pg->drcs_clut));

/*
	pg->page_opacity[0] = VBI_OPAQUE;
	pg->page_opacity[1] = VBI_OPAQUE;
	pg->boxed_opacity[0] = VBI_OPAQUE;
	pg->boxed_opacity[1] = VBI_OPAQUE;

	CLEAR (pg->drcs);
*/
	CLEAR (ac);

	ac.foreground	= VBI_BLACK; // 32 + VBI_BLACK;
	ac.background	= 32 + VBI_BLUE;
	ac.opacity	= VBI_OPAQUE;
	ac.unicode	= 0x0020;
	ac.size		= VBI_NORMAL_SIZE;

	for (ii = 0; ii < pg->rows * pg->columns; ii++)
		pg->text[ii] = ac;

	ac.size = VBI_DOUBLE_SIZE;

	/* FIXME */
	/* TRANSLATORS: Title of TOP Index page,
	   for now please Latin-1 or ASCII only */
	index_str = _("TOP Index");

	for (i = 0; index_str[i]; i++) {
		ac.unicode = index_str[i];
		pg->text[1 * pg->columns + 2 + i * 2] = ac;
	}

	ac.size = VBI_NORMAL_SIZE;

	acp = &pg->text[4 * pg->columns];
	lines = 17;
	xpgno = 0;
	xsubno = 0;

	vtp = NULL;

	while ((ait = next_ait(vbi, xpgno, xsubno, &vtp))) {
		xpgno = ait->page.pgno;
		xsubno = ait->page.subno;

		/* No docs, correct? */
#warning todo
//		character_set_designation(pg->font, ext, vtp);

		if (subno > 0) {
			if (lines-- == 0) {
				subno--;
				lines = 17;
			}

			continue;
		} else if (lines-- <= 0)
			continue;

		for (i = 11; i >= 0; i--)
			if (ait->text[i] > 0x20)
				break;

		switch (vbi->vt.page_info[ait->page.pgno - 0x100].code) {
		case VBI_TOP_GROUP:
			k = 3;
			break;

		default:
    			k = 1;
		}

#warning todo
#if 0
		for (j = 0; j <= i; j++) {
			acp[k + j].unicode = vbi_teletext_unicode(pg->font[0]->G0,
				pg->font[0]->subset, (ait->text[j] < 0x20) ? 0x20 : ait->text[j]);
		}
#endif
		for (k += i + 2; k <= 33; k++)
			acp[k].unicode = '.';

		for (j = 0; j < 3; j++) {
			n = ((ait->page.pgno >> ((2 - j) * 4)) & 15) + '0';

			if (n > '9')
				n += 'A' - '9';

			acp[j + 35].unicode = n;
 		}

		acp += pg->columns;
	}

	if (vtp)
		vbi_cache_unref (vbi, vtp);

	return 1;
}

vbi_inline void
ait_title			(vbi_decoder *		vbi,
				 const vt_page *	vtp,
	  const ait_entry *ait, char *buf)
{
	const vbi_character_set *char_set[2];
	int i;

	character_set_designation (char_set, 0, &vbi->vt.magazine[0].extension, vtp);

	for (i = 11; i >= 0; i--)
		if (ait->text[i] > 0x20)
			break;
	buf[i + 1] = 0;

	for (; i >= 0; i--) {
		unsigned int unicode = vbi_teletext_unicode
			(char_set[0]->g0, char_set[0]->subset,
			 (ait->text[i] < 0x20) ? 0x20 : ait->text[i]);

		buf[i] = (unicode >= 0x20 && unicode <= 0xFF) ? unicode : 0x20;
	}
}

/**
 * @param vbi Initialized vbi decoding context.
 * @param pgno Page number, see vbi_pgno.
 * @param subno Subpage number.
 * @param buf Place to store the title, Latin-1 format, at least
 *   41 characters including the terminating zero.
 * 
 * Given a Teletext page number this function tries to deduce a
 * page title for bookmarks or other purposes, mainly from navigation
 * data. (XXX TODO: FLOF)
 * 
 * @return
 * @c TRUE if a title has been found.
 */
vbi_bool
vbi_page_title(vbi_decoder *vbi, int pgno, int subno, char *buf)
{
	const ait_entry *ait;
	int i, j;

	subno = subno;

	if (vbi->vt.top) {
		for (i = 0; i < 8; i++) {
			const vt_page *vtp;

			if (vbi->vt.btt_link[i].type != 2)
				continue;

			vtp = vbi_cache_get (vbi, NUID0,
					     vbi->vt.btt_link[i].pgno, 
					     vbi->vt.btt_link[i].subno, 0x3f7f,
					     /* user access */ FALSE);
			if (!vtp) {
				nav_log ("p/t top ait page %x not cached\n",
				       vbi->vt.btt_link[i].pgno);
				continue;
			}

			if (vtp->function != PAGE_FUNCTION_AIT) {
				nav_log ("p/t no ait page %x\n", vtp->pgno);
				goto end;
			}

			for (ait = vtp->data.ait, j = 0; j < 46; ait++, j++)
				if (ait->page.pgno == pgno) {

					ait_title(vbi, vtp, ait, buf);
					vbi_cache_unref (vbi, vtp);
					return TRUE;
				}
		end:
			vbi_cache_unref (vbi, vtp);
		}
	} else {
		/* find a FLOF link and the corresponding label */
	}

	return FALSE;
}

/*
 *  Navigation (TOP and FLOF) enhancements.
 */
static void
navigation			(vbi_page_private *		t)
{
	if (t->vtp->data.lop.flof) {
		vbi_pgno home_pgno = t->vtp->data.lop.link[5].pgno;

		if (home_pgno >= 0x100 && home_pgno <= 0x899
		    && (home_pgno & 0xFF) != 0xFF) {
			t->nav_link[5].pgno = home_pgno;
			t->nav_link[5].subno = t->vtp->data.lop.link[5].subno;
		}

		if (t->vtp->lop_lines & (1 << 24)) {
			flof_links (t);
		} else {
			flof_navigation_bar (t);
		}
	} else if (t->pg.vbi->vt.top) {
		top_navigation_bar (t);
	}
}

/**
 * @param pg With vbi_fetch_vt_page() obtained vbi_page.
 * @param ld Place to store information about the link.
 * @param index Number 0 ... 5 of the link.
 * 
 * When a vbi_page has been formatted with TOP or FLOF
 * navigation enabled the last row may contain four links
 * to other pages. Apart of being hyperlinks (see
 * vbi_page_hyperlink()) you can also query the links by
 * ordinal number, 0 refering to the leftmost and 3 to
 * the rightmost link displayed.
 *
 * Further all Teletext pages have a built-in home link,
 * by default page 100, but also the magazine intro page
 * or another page selected by the editor. This link has
 * number 5.
 *
 * Other numbers are currently unused.
 *
 * @returns
 * TRUE if the link is valid.
 */
vbi_bool
vbi_page_nav_enum		(const vbi_page *	pg,
				 vbi_link *		ld,
				 unsigned int		index)
{
	const vbi_page_private *pgp;

	PGP_CHECK (FALSE);

	assert (NULL != ld);

	if (pg->pgno < 0x100 || index > 5
	    || pgp->nav_link[index].pgno < 0x100) {
		ld->type = VBI_LINK_NONE;
		return FALSE;
	}

	ld->type = VBI_LINK_PAGE;
	ld->pgno = pgp->nav_link[index].pgno;
	ld->subno = pgp->nav_link[index].subno;

	return TRUE;
}

/* ------------------------------------------------------------------------ */

/**
 * @internal
 * @param Initialized vbi_decoder context.
 * @param pgp Place to store the formatted page.
 * @param vtp Raw Teletext page.
 * @param format_options Format option list.
 * 
 * See vbi_format_vt_page().
 *
 * @return
 * @c TRUE if the page could be formatted.
 */
vbi_bool
vbi_format_vt_page_va_list	(vbi_decoder *		vbi,
				 vbi_page_private *	pgp,
				 const vt_page *	vtp,
				 va_list		format_options)
{
	vbi_bool option_navigation;
	vbi_bool option_hyperlinks;
	vbi_character_set_code option_cs_default;
	const vbi_character_set *option_cs_override;

	if (vtp->function != PAGE_FUNCTION_LOP &&
	    vtp->function != PAGE_FUNCTION_TRIGGER)
		return FALSE;

	fmt_log ("\nFormatting page %03x/%04x pgp=%p\n",
		 vtp->pgno, vtp->subno, pgp);

	pgp->pg.vbi		= vbi;

	/* Source page */

	pgp->pg.nuid		= vbi->network.ev.network.nuid;
	pgp->pg.pgno		= vtp->pgno;
	pgp->pg.subno		= vtp->subno;

	vbi_cache_ref (pgp->pg.vbi, vtp);

	pgp->vtp		= vtp;

	pgp->pg.dirty.y0	= 0;
	pgp->pg.dirty.y1	= pgp->pg.rows - 1;
	pgp->pg.dirty.roll	= 0;

	/* Magazine and extension defaults */

	pgp->mag = (pgp->max_level <= VBI_WST_LEVEL_1p5) ?
		vbi->vt.magazine /* default magazine */
		: vbi->vt.magazine + (vtp->pgno >> 8);

	if (vtp->data.lop.ext)
		pgp->ext = &vtp->data.ext_lop.ext;
	else
		pgp->ext = &pgp->mag->extension;

	/* Colors */

	screen_color (&pgp->pg, vtp->flags, pgp->ext->def_screen_color);

	assert (sizeof (pgp->pg.color_map) == sizeof (pgp->ext->color_map));
	memcpy (pgp->pg.color_map, pgp->ext->color_map,
		sizeof (pgp->pg.color_map));

	assert (sizeof (pgp->pg.drcs_clut) == sizeof (pgp->ext->drcs_clut));
	memcpy (pgp->pg.drcs_clut, pgp->ext->drcs_clut,
		sizeof (pgp->pg.drcs_clut));

	/* Opacity */

	pgp->page_opacity[1] = VBI_OPAQUE;
	pgp->boxed_opacity[1] = VBI_SEMI_TRANSPARENT;

	if (vtp->flags & (C5_NEWSFLASH | C6_SUBTITLE | C10_INHIBIT_DISPLAY))
		pgp->page_opacity[1] = VBI_TRANSPARENT_SPACE;

	if (vtp->flags & C10_INHIBIT_DISPLAY)
		pgp->boxed_opacity[1] =	VBI_TRANSPARENT_SPACE;

	if (pgp->vtp->flags & C7_SUPPRESS_HEADER) {
		pgp->page_opacity[0] = VBI_TRANSPARENT_SPACE;
		pgp->boxed_opacity[0] = VBI_TRANSPARENT_SPACE;
	} else {
		pgp->page_opacity[0] = pgp->page_opacity[1];
		pgp->boxed_opacity[0] = pgp->boxed_opacity[1];
	}

	/* Options */

	pgp->max_level		= VBI_WST_LEVEL_1;
	pgp->pdc_links		= FALSE;

	pgp->pg.rows		= 25;
	pgp->pg.columns		= 40;

	option_navigation = FALSE;
	option_hyperlinks = FALSE;
	option_cs_default = 0;
	option_cs_override = NULL;

	for (;;) {
		vbi_format_option option;

		option = va_arg (format_options, vbi_format_option);

		switch (option) {
		case VBI_HEADER_ONLY:
			pgp->pg.rows =
				va_arg (format_options, vbi_bool) ? 1 : 25;
			break;

		case VBI_41_COLUMNS:
			pgp->pg.columns =
				va_arg (format_options, vbi_bool) ? 41 : 40;
			break;

		case VBI_NAVIGATION:
			option_navigation = va_arg (format_options, vbi_bool);
			break;

		case VBI_HYPERLINKS:
			option_hyperlinks = va_arg (format_options, vbi_bool);
			break;

		case VBI_PDC_LINKS:
			pgp->pdc_links = va_arg (format_options, vbi_bool);
			break;

		case VBI_WST_LEVEL:
			pgp->max_level =
				va_arg (format_options, vbi_wst_level);
			break;

		case VBI_CHAR_SET_DEFAULT:
			option_cs_default = va_arg (format_options,
						    vbi_character_set_code);
			break;

		case VBI_CHAR_SET_OVERRIDE:
			option_cs_override = vbi_character_set_from_code
				(va_arg (format_options,
					 vbi_character_set_code));
			break;

		default:
			option = 0;
			break;
		}

		if (0 == option)
			break;
	}

	/* Character set designation */

	if (option_cs_override) {
		pgp->char_set[0] = option_cs_override;
		pgp->char_set[1] = option_cs_override;
	} else {
		character_set_designation (pgp->char_set,
					   option_cs_default,
					   pgp->ext, vtp);
	}

	/* Format level one page */

	level_one_page (pgp);

	/* PDC Preselection method "A" links */

	if (pgp->pg.rows > 1 && pgp->pdc_links) {
		pgp->pdc_table_size =
			pdc_method_a (pgp->pdc_table,
				      N_ELEMENTS (pgp->pdc_table),
				      vtp->data.lop.raw);
	}

	/* Local enhancement data and objects */

	if (pgp->max_level >= VBI_WST_LEVEL_1p5) {
		vbi_page page;
		vbi_bool success;

		page = pgp->pg;

		CLEAR (pgp->drcs_vtp);

		if (!(vtp->flags & (C5_NEWSFLASH | C6_SUBTITLE))) {
			pgp->boxed_opacity[0] = VBI_TRANSPARENT_SPACE;
			pgp->boxed_opacity[1] = VBI_TRANSPARENT_SPACE;
		}

		if (vtp->enh_lines & 1) {
			fmt_log ("enhancement packets %08x\n",
				 vtp->enh_lines);

			success = enhance
				(pgp, LOCAL_ENHANCEMENT_DATA,
				 vtp->data.enh_lop.enh,
				 N_ELEMENTS (vtp->data.enh_lop.enh),
				 0, 0);
		} else {
			success = default_object_invocation (pgp);
		}

		if (success) {
			if (pgp->max_level >= VBI_WST_LEVEL_2p5)
				post_enhance (pgp);
		} else {
			unsigned int i;

			/* Roll back incomplete enhancements. */

			for (i = 0; i < N_ELEMENTS (pgp->drcs_vtp); ++i) {
				if (pgp->drcs_vtp[i])
					vbi_cache_unref (vbi,
							 pgp->drcs_vtp[i]);
				pgp->drcs_vtp[i] = NULL;
			}

			pgp->pg = page;
		}
	}

	pgp->nav_link[5] = vbi->vt.initial_page;

	if (pgp->pg.rows > 1) {
		unsigned int row;

		if (option_hyperlinks)
			for (row = 1; row < 25; ++row)
				hyperlinks (pgp, row);

		if (option_navigation)
			navigation (pgp);

		if (pgp->pdc_links)
			vbi_page_mark_pdc (&pgp->pg, pgp->pdc_table,
					   pgp->pdc_table
					   + pgp->pdc_table_size);
	}

	if (41 == pgp->pg.columns)
		column_41 (pgp);

	if (0)
		vbi_page_private_dump (pgp, 2, stderr);

	return TRUE;
}

/**
 * @internal
 * @param Initialized vbi_decoder context.
 * @param pgp Place to store the formatted page.
 * @param vtp Raw Teletext page.
 * @param ... Format option list, see vbi_fetch_vt_page() for details.
 * 
 * Format a page @a pg from a raw Teletext page @a vtp.
 * 
 * @return
 * @c TRUE if the page could be formatted.
 */
vbi_bool
vbi_format_vt_page		(vbi_decoder *		vbi,
				 vbi_page_private *	pgp,
				 const vt_page *	vtp,
				 ...)
{
	vbi_bool r;
	va_list format_options;

	va_start (format_options, vtp);
	r = vbi_format_vt_page_va_list (vbi, pgp, vtp, format_options);
	va_end (format_options);

	return r;
}

/**
 * @param vbi Initialized vbi_decoder context.
 * @param pg Place to store the formatted page.
 * @param pgno Page number of the page to fetch, see vbi_pgno.
 * @param subno Subpage number to fetch (optional @c VBI_ANY_SUBNO).
 * @param format_options Format option list, see vbi_fetch_vt_page() for details.
 * 
 * Fetches a Teletext page designated by @a pgno and @a subno from the
 * cache, formats and stores it in @a pg. Formatting is limited to row
 * 0 ... @a display_rows - 1 inclusive. The really useful values
 * are 1 (format header only) or 25 (everything). Likewise
 * @a navigation can be used to save unnecessary formatting time.
 * 
 * @return
 * @c FALSE if the page is not cached or could not be formatted
 * for other reasons, for instance is a data page not intended for
 * display. Level 2.5/3.5 pages which could not be formatted e. g.
 * due to referencing data pages not in cache are formatted at a
 * lower level.
 */
vbi_page *
vbi_fetch_vt_page_va_list	(vbi_decoder *		vbi,
				 vbi_pgno		pgno,
				 vbi_subno		subno,
				 va_list		format_options)
{
	vbi_page *pg;
	vbi_page_private *pgp;

	if (!(pg = vbi_page_new ()))
		return NULL;

	pgp = PARENT (pg, vbi_page_private, pg);

	if (0x900 == pgno) {
		unsigned int row;

		if (subno == VBI_ANY_SUBNO)
			subno = 0;

		CLEAR (*pgp);

		pgp->pg.vbi		= vbi;

		pgp->pg.rows		= 25;
		pgp->pg.columns		= 40;

		for (;;) {
			vbi_format_option option;

			option = va_arg (format_options, vbi_format_option);

			switch (option) {
			case VBI_HEADER_ONLY:
				pgp->pg.rows =
					va_arg (format_options, vbi_bool) ?
					1 : 25;
				break;

			case VBI_41_COLUMNS:
				pgp->pg.columns =
					va_arg (format_options, vbi_bool) ?
					41 : 40;
				break;

			case VBI_NAVIGATION:
			case VBI_HYPERLINKS:
			case VBI_PDC_LINKS:
				va_arg (format_options, vbi_bool);
				break;

			case VBI_WST_LEVEL:
				va_arg (format_options, vbi_wst_level);
				break;

			default:
				option = 0;
				break;
			}

			if (0 == option)
				break;
		}

		if (!vbi->vt.top
		    || !top_index (vbi, &pgp->pg, subno, pgp->pg.columns)) {
			vbi_page_delete (&pgp->pg);
			return NULL;
		}

		pgp->pg.nuid = vbi->network.ev.network.nuid;
		pgp->pg.pgno = 0x900;
		pgp->pg.subno = subno;

		post_enhance (pgp);

		for (row = 1; row < pgp->pg.rows; ++row)
			hyperlinks (pgp, row);
	} else {
		const vt_page *vtp;

		vtp = vbi_cache_get (vbi, NUID0,
				     pgno, subno, ~0,
				     /* user access */ FALSE);

		if (NULL == vtp) {
			vbi_page_delete (&pgp->pg);
			return NULL;
		}

		if (!vbi_format_vt_page_va_list (vbi, pgp, vtp,
						 format_options)) {
			vbi_page_delete (&pgp->pg);
			return NULL;
		}

		vbi_cache_unref (vbi, vtp);
	}

	return &pgp->pg;
}

/**
 * @param vbi Initialized vbi_decoder context.
 * @param pg Place to store the formatted page.
 * @param pgno Page number of the page to fetch, see vbi_pgno.
 * @param subno Subpage number to fetch (optional @c VBI_ANY_SUBNO).
 * @param ... Format option list, see vbi_fetch_vt_page() for details.
 * 
 * Fetches a Teletext page designated by @a pgno and @a subno from the
 * cache, formats and stores it in @a pg. Formatting is limited to row
 * 0 ... @a display_rows - 1 inclusive. The really useful values
 * are 1 (format header only) or 25 (everything). Likewise
 * @a navigation can be used to save unnecessary formatting time.
 * 
 * @return
 * @c FALSE if the page is not cached or could not be formatted
 * for other reasons, for instance is a data page not intended for
 * display. Level 2.5/3.5 pages which could not be formatted e. g.
 * due to referencing data pages not in cache are formatted at a
 * lower level.
 */
vbi_page *
vbi_fetch_vt_page		(vbi_decoder *		vbi,
				 vbi_pgno		pgno,
				 vbi_subno		subno,
				 ...)
{
	vbi_page *pg;
	va_list format_options;

	va_start (format_options, subno);
	pg = vbi_fetch_vt_page_va_list (vbi, pgno, subno, format_options);
	va_end (format_options);

	return pg;
}

/**
 * @param pg A vbi_page allocated with vbi_page_new(),
 *   vbi_fetch_vt_page() or vbi_fetch_vt_page_va_list().
 *
 * Creates a copy of the vbi_page.
 *
 * @returns
 * Pointer to newly allocated vbi_page, which must be deleted with
 * vbi_page_delete() when done, NULL if out of memory.
 */
vbi_page *
vbi_page_copy			(const vbi_page *	pg)
{
	const vbi_page_private *old_pgp;
	vbi_page_private *new_pgp;

	assert (NULL != pg);

	old_pgp = CONST_PARENT (pg, vbi_page_private, pg);

	if (!(new_pgp = malloc (sizeof (*new_pgp)))) {
		vbi_log_printf (__FUNCTION__, "Out of memory");
		return NULL;
	}

	if (VBI_PAGE_PRIVATE_MAGIC != old_pgp->magic) {
		CLEAR (*new_pgp);

		new_pgp->pg = *pg;
	} else {
		*new_pgp = *old_pgp;

		if (new_pgp->pg.vbi) {
			unsigned int i;

			if (new_pgp->vtp)
				vbi_cache_ref (new_pgp->pg.vbi, new_pgp->vtp);

			for (i = 0; i < N_ELEMENTS (new_pgp->drcs_vtp); ++i)
				if (new_pgp->drcs_vtp[i])
					vbi_cache_ref (new_pgp->pg.vbi,
						       new_pgp->drcs_vtp[i]);
		}
	}

	return &new_pgp->pg;
}

/**
 * @param pg A vbi_page allocated with vbi_page_new(),
 *   vbi_fetch_vt_page() or vbi_fetch_vt_page_va_list().
 *
 * Deletes a vbi_page.
 */
void
vbi_page_delete			(vbi_page *		pg)
{
	vbi_page_private *pgp;

	if (NULL == pg)
		return;

	pgp = PARENT (pg, vbi_page_private, pg);

	if (VBI_PAGE_PRIVATE_MAGIC != pgp->magic) {
		vbi_log_printf (__FUNCTION__,
				"vbi_page %p not allocated by libzvbi", pg);
		return;
	}

	if (pgp->pg.vbi) {
		unsigned int i;

		if (pgp->vtp)
			vbi_cache_unref (pgp->pg.vbi, pgp->vtp);

		for (i = 0; i < N_ELEMENTS (pgp->drcs_vtp); ++i)
			if (pgp->drcs_vtp[i])
				vbi_cache_unref (pgp->pg.vbi,
						 pgp->drcs_vtp[i]);
	}

	CLEAR (*pgp);

	free (pgp);
}

/**
 * Allocates a new, empty vbi_page.
 *
 * @returns
 * NULL when out of memory. The vbi_page must be deleted
 * with vbi_page_delete() when done.
 */
vbi_page *
vbi_page_new			(void)
{
	vbi_page_private *pgp;

	if (!(pgp = calloc (1, sizeof (*pgp)))) {
		vbi_log_printf (__FUNCTION__, "Out of memory");
		return NULL;
	}

	pgp->magic = VBI_PAGE_PRIVATE_MAGIC;

	return &pgp->pg;
}
