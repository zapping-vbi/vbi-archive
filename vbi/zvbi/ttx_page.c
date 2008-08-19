/*
 *  libzvbi -- Teletext page formatting
 *
 *  Copyright (C) 2000-2008 Michael H. Schimek
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the 
 *  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, 
 *  Boston, MA  02110-1301  USA.
 */

/* $Id: ttx_page.c,v 1.1.2.2 2008-08-19 16:36:29 mschimek Exp $ */

#include "../site_def.h"

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "hamm.h"
#include "ttx.h"
#include "ttx_charset.h"
#include "ttx_page-priv.h"
#include "ttx_decoder-priv.h"

#ifndef TELETEXT_FMT_LOG
#  define TELETEXT_FMT_LOG 0
#endif

#define fmt_log(templ, args...)						\
do {									\
	if (TELETEXT_FMT_LOG)						\
		fprintf (stderr, templ , ##args);			\
} while (0)

#define PGP_CHECK(ret_value)						\
do {									\
	assert (NULL != pg);						\
									\
	pgp = CONST_PARENT (pg, vbi_page_priv, pg);			\
									\
	if (pg->priv != pgp)						\
		return ret_value;					\
} while (0)

static vbi_bool
enhance				(struct ttx_page *	pgp,
				 enum ttx_object_type	type,
				 const struct ttx_triplet *trip,
				 unsigned int		n_triplets,
				 unsigned int		inv_row,
				 unsigned int		inv_column);

/**
 */
vbi_bool
_vbi_ttx_page_dump		(const struct ttx_page *tp,
				 FILE *			fp,
				 unsigned int		mode)
{
	unsigned int row;
	unsigned int column;
	const vbi_char *acp;

	_vbi_null_check (tp, FALSE);
	_vbi_null_check (fp, FALSE);

	acp = tp->pg.text;

	for (row = 0; row < tp->pg.rows; ++row) {
		fprintf (fp, "%2u: ", row);

		for (column = 0; column < tp->pg.columns; ++column) {
			int c;

			switch (mode) {
			case 0:
				c = acp->unicode;
				if (c < 0x20 || c >= 0x7F)
					c = '.';
				fputc (c, fp);
				break;

			case 1:
				fprintf (fp, "%04x ", acp->unicode);
				break;

			case 2:
				fprintf (fp, "%04xF%uB%uS%uO%uL%u%u ",
					 acp->unicode,
					 acp->foreground,
					 acp->background,
					 acp->size,
					 acp->opacity,
					 !!(acp->attr & VBI_LINK),
					 !!(acp->attr & VBI_PDC));
				break;
			}

			++acp;
		}

		fputc ('\n', fp);
	}

	return TRUE;
}

/**
 * @internal
 */
void
_vbi_ttx_charset_init		(const vbi_ttx_charset *charset[2],
				 vbi_ttx_charset_code	default_code_0,
				 vbi_ttx_charset_code	default_code_1,
				 const struct ttx_extension *ext,
				 const cache_page *	cp)
{
	unsigned int i;

	/* Primary and secondary. */
	for (i = 0; i < 2; ++i) {
		const vbi_ttx_charset *cs;
		vbi_ttx_charset_code code;

		code = i ? default_code_1 : default_code_0;

		if (ext && (ext->designations & 0x11)) {
			/* Have X/28/0 or M/29/0 or /4. */
			code = ext->charset_code[i];
		}

		cs = vbi_ttx_charset_from_code
			((code & (unsigned int) ~7) + cp->national);

		if (!cs)
			cs = vbi_ttx_charset_from_code (code);

		if (!cs)
			cs = vbi_ttx_charset_from_code (0);

		charset[i] = cs;
	}
}


static void
init_border_color		(vbi_page *		pg,
				 unsigned int		flags,
				 unsigned int		color)
{
	pg->border_color = color;

	if (color == VBI_TRANSPARENT_BLACK
	    || (flags & (C5_NEWSFLASH | C6_SUBTITLE)))
		pg->border_opacity = VBI_TRANSPARENT_SPACE;
	else
		pg->border_opacity = VBI_OPAQUE;
}

/* Level One Page ---------------------------------------------------------- */

static vbi_bool
level_one_row			(struct ttx_page *	tp,
				 unsigned int		row)
{
	static const unsigned int mosaic_separate = 0xEE00 - 0x20;
	static const unsigned int mosaic_contiguous = 0xEE20 - 0x20;
	const vbi_ttx_charset *cs;
	unsigned int mosaic_plane;
	unsigned int held_mosaic_unicode;
	vbi_bool hold;
	vbi_bool mosaic;
	vbi_bool double_height_row;
	vbi_bool wide_char;
	const uint8_t *rawp;
	int raw;
	vbi_char *acp;
	vbi_char ac;
	unsigned int column;

	rawp = tp->cp->data.lop.raw[row];
	acp = tp->pg.text + row * tp->pg.columns;

	/* G1 block mosaic, blank, contiguous. */
	held_mosaic_unicode = mosaic_contiguous + 0x20;

	CLEAR (ac);

	ac.unicode		= 0x0020;
	ac.foreground		= tp->ext->foreground_clut + VBI_WHITE;
	ac.background		= tp->ext->background_clut + VBI_BLACK;
	mosaic_plane		= mosaic_contiguous;
	ac.opacity		= tp->page_opacity[row > 0];
	cs			= tp->char_set[0];
	hold			= FALSE;
	mosaic			= FALSE;
	double_height_row	= FALSE;
	wide_char		= FALSE;

	for (column = 0; column < 40; ++column) {
		raw = vbi_unpar8 (*rawp++);

		if ((0 == row && column < 8) || raw < 0)
			raw = 0x20;

		/* Set-at spacing attributes. */

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
			ac.background = tp->ext->background_clut + VBI_BLACK;
			break;

		case 0x1D:		/* new background */
			ac.background = tp->ext->background_clut
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
				held_mosaic_unicode = mosaic_plane + raw;
				ac.unicode = held_mosaic_unicode;
			} else {
				ac.unicode = vbi_ttx_unicode
					(cs->g0, cs->subset,
					 (unsigned int) raw);
			}
		}

		if (wide_char) {
			wide_char = FALSE;
		} else {
			acp[column] = ac;

			wide_char = !!(ac.size & VBI_DOUBLE_WIDTH);
			if (wide_char) {
				if (column < 39) {
					acp[column + 1] = ac;
					acp[column + 1].size = VBI_OVER_TOP;
				} else {
					acp[column].size = VBI_NORMAL_SIZE;
					wide_char = FALSE;
				}
			}
		}

		/* Set-after spacing attributes. */

		switch (raw) {
		case 0x00 ... 0x07:	/* alpha + foreground color */
			ac.foreground = tp->ext->foreground_clut + (raw & 7);
			ac.attr &= ~VBI_CONCEAL;
			mosaic = FALSE;
			break;

		case 0x08:		/* flash */
			ac.attr |= VBI_FLASH;
			break;

		case 0x0A:		/* end box */
			/* 12.2 Table 26: Double transmission
			   as additional error protection. */
			if (column >= 39)
				break;
			if (0x0A == vbi_unpar8 (*rawp))
				ac.opacity = tp->page_opacity[row > 0];
			break;

		case 0x0B:		/* start box */
			/* 12.2 Table 26: Double transmission
			   as additional error protection. */
			if (column >= 39)
				break;
			if (0x0B == vbi_unpar8 (*rawp))
				ac.opacity = tp->boxed_opacity[row > 0];
			break;

		case 0x0D:		/* double height */
			if (row == 0 || row >= 23)
				break;
			ac.size = VBI_DOUBLE_HEIGHT;
			double_height_row = TRUE;
			break;

		case 0x0E:		/* double width */
			if (column >= 39)
				break;
			ac.size = VBI_DOUBLE_WIDTH;
			break;

		case 0x0F:		/* double size */
			if (column >= 39 || row == 0 || row >= 23)
				break;
			ac.size = VBI_DOUBLE_SIZE;
			double_height_row = TRUE;
			break;

		case 0x10 ... 0x17:	/* mosaic + foreground color */
			ac.foreground = tp->ext->foreground_clut + (raw & 7);
			ac.attr &= ~VBI_CONCEAL;
			mosaic = TRUE;
			break;

		case 0x1B:		/* ESC */
			cs = tp->char_set[cs == tp->char_set[0]];
			break;

		case 0x1F:		/* release mosaic */
			hold = FALSE;
			break;
		}
	}

	return double_height_row;
}

static void
level_one_extend_row		(struct ttx_page *	tp,
				 unsigned int		row)
{
	vbi_char *acp;
	unsigned int column;

	/* 12.2 Table 26: When double height (or double size) characters are
	   used on a given row, the row below normal height characters on that
	   row is displayed with the same local background colour and no
	   foreground data. */

	acp = tp->pg.text + row * tp->pg.columns;

	for (column = 0; column < tp->pg.columns; ++column) {
		vbi_char ac = acp[column];

		switch (ac.size) {
		case VBI_DOUBLE_HEIGHT:
			ac.size = VBI_DOUBLE_HEIGHT2;
			acp[tp->pg.columns + column] = ac;
			break;
		
		case VBI_DOUBLE_SIZE:
			ac.size = VBI_DOUBLE_SIZE2;
			acp[tp->pg.columns + column] = ac;
			++column;
			ac.size = VBI_OVER_BOTTOM;
			acp[tp->pg.columns + column] = ac;
			break;

		case VBI_NORMAL_SIZE:
		case VBI_DOUBLE_WIDTH:
		case VBI_OVER_TOP:
			ac.size = VBI_NORMAL_SIZE;
			ac.unicode = 0x0020;
			acp[tp->pg.columns + column] = ac;
			break;

		default:
			assert (0);
		}
	}
}

/**
 * @internal
 * @param pgp Current vbi_page.
 *
 * Formats a level one page.
 */
static void
level_one_page			(struct ttx_page *	tp)
{
	unsigned int row;

	for (row = 0; row < tp->pg.rows; ++row) {
		vbi_bool double_height_row;

		double_height_row = level_one_row (tp, row);
		if (double_height_row) {
			level_one_extend_row (tp, row);
			++row;
		}
	}

	if (0)
		_vbi_ttx_page_dump (tp, stderr, 0);
}

/* Level One Page Enhancement ---------------------------------------------- */

static cache_page *
get_system_page			(const struct ttx_page *tp,
				 vbi_pgno		pgno,
				 vbi_subno		subno,
				 enum ttx_page_function	function)
{
	cache_page *cp;
	cache_page *cp1;

	cp = _vbi_cache_get_page (tp->pg.cache,
				  /* const cast */
				  (cache_network *) tp->cn,
				  pgno, subno, 0x000F);

	if (!cp) {
		fmt_log ("... page %03x.%04x not cached\n",
			 pgno, subno);
		goto failure;
	}

	switch (cp->function) {
	case PAGE_FUNCTION_UNKNOWN:
		if ((cp1 = _vbi_convert_cached_page (cp, function)))
			return cp1;

		fmt_log ("... not %s or hamming error\n",
			 _vbi_ttx_page_function_name (function));

		goto failure;

	case PAGE_FUNCTION_POP:
	case PAGE_FUNCTION_GPOP:
		if (PAGE_FUNCTION_POP == function
		    || PAGE_FUNCTION_GPOP == function)
			return cp;
		break;

	case PAGE_FUNCTION_DRCS:
	case PAGE_FUNCTION_GDRCS:
		if (PAGE_FUNCTION_DRCS == function
		    || PAGE_FUNCTION_GDRCS == function)
			return cp;
		break;

	default:
		break;
	}

	fmt_log ("... source page wrong function %s, expected %s\n",
		 _vbi_ttx_page_function_name (cp->function),
		 _vbi_ttx_page_function_name (function));

 failure:
	cache_page_unref (cp);

	return NULL;
}

/*
	Objects
*/

static const struct ttx_pop_link *
magazine_pop_link		(const struct ttx_page *tp,
				 unsigned int		link)
{
	const struct ttx_pop_link *pop;

	if (tp->max_level >= VBI_TTX_LEVEL_3p5) {
		pop = &tp->mag->pop_link[1][link];

		if (!NO_PAGE (pop->pgno))
			return pop;
	}

	return &tp->mag->pop_link[0][link];
}

static ttx_object_address
triplet_object_address		(const struct ttx_triplet *trip)
{
	/* 
	   .mode	 .address	     .data
	   1 0 x m1  m0	 1 s1  s0 x n8	n7   n6	 n5   n4  n3 n2 n1 n0
		 -type-	   source   packet   triplet  lsb ----s1-----
				    -------------address-------------
	*/

	return ((trip->address & 3) << 7) | trip->data;
}

/**
 * @internal
 * @param tp Current vbi_page.
 *
 * @param trip_cp Returns reference to page containing object triplets,
 *   must cache_page_unref() when done.
 * @param trip Returns pointer to object triplets.
 * @param trip_size Returns number of object triplets.
 *
 * @param type Type of the object (ACTIVE, ADAPTIVE, PASSIVE).
 * @param pgno Page where the object is stored.
 * @param address Address of the object within the page.
 *
 * @param function Presumed function of the page (POP, GPOP).
 *
 * Resolves a POP or GPOP object address (12.3.1 Table 28 Mode 10001).
 *
 * @returns
 * FALSE on failure.
 */
static vbi_bool
resolve_obj_address		(struct ttx_page *	tp,
				 cache_page **		trip_cp,
				 const struct ttx_triplet **trip,
				 unsigned int *		trip_size,
				 enum ttx_object_type	type,
				 vbi_pgno		pgno,
				 ttx_object_address	address,
				 enum ttx_page_function	function)
{
	cache_page *cp;
	vbi_subno subno;
	unsigned int lsb;
	unsigned int tr;
	unsigned int packet;
	unsigned int pointer;
	const struct ttx_triplet *t;

	cp = NULL;

	subno	= address & 15;
	lsb	= ((address >> 4) & 1);
	tr	= ((address >> 5) & 3) * 3 + type - 1;
	packet	= ((address >> 7) & 3);

	fmt_log ("obj invocation, source page %03x.%04x, "
		 "pointer packet=%u triplet=%u lsb=%u\n",
		 pgno, subno, packet + 1, tr + 1, lsb);

	if (!(cp = get_system_page (tp, pgno, subno, function)))
		goto failure;

	pointer = cp->data.pop.pointer[packet * (12 * 2) + tr * 2 + lsb];

	if (pointer > 506) {
		fmt_log ("... triplet pointer %u > 506\n", pointer);
		goto failure;
	} else {
		fmt_log ("... triplet pointer %u\n", pointer);
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

	t = cp->data.pop.triplet + pointer;

	fmt_log ("... obj def: addr=0x%02x mode=0x%04x data=%u=0x%x\n",
		 t->address, t->mode, t->data, t->data);

	/* Object definition points to itself. */

	if (t->mode != (type + 0x14)
	    || triplet_object_address (t) != address) {
		fmt_log ("... no object definition\n");
		goto failure;
	}

	*trip_cp	= cp;
	*trip		= t + 1;
	*trip_size	= N_ELEMENTS (cp->data.pop.triplet) - (pointer + 1);

	return TRUE;

 failure:
	cache_page_unref (cp);

	return FALSE;
}

/**
 * @internal
 * @param tp Current vbi_page.
 * @param type Current object type.
 * @param trip Triplet requesting the object.
 * @param row Current row.
 * @param column Current column.
 *
 * Recursive object invocation at row, column.
 *
 * @returns
 * FALSE on failure.
 */
static vbi_bool
object_invocation		(struct ttx_page *	tp,
				 enum ttx_object_type	type,
				 const struct ttx_triplet *trip,
				 unsigned int		row,
				 unsigned int		column)
{
	enum ttx_object_type new_type;
	unsigned int source;
	cache_page *trip_cp;
	unsigned int n_triplets;
	vbi_bool success;

	new_type = trip->mode & 3;
	source = (trip->address >> 3) & 3;

	fmt_log ("enhancement obj invocation source %u type %s\n",
		 source, _vbi_ttx_object_type_name (new_type));

	if (new_type <= type) {
		/* EN 300 706 13.2ff. */
		fmt_log ("... type priority violation %s -> %s\n",
			 _vbi_ttx_object_type_name (type),
			 _vbi_ttx_object_type_name (new_type));
		return FALSE;
	}

	trip_cp = NULL;
	n_triplets = 0;

	switch (source) {
	case 0:
		fmt_log ("... invalid source\n");
		return FALSE;

	case 1:
	{
		unsigned int designation;
		unsigned int triplet;
		unsigned int offset;

		/*
		  .mode		.address	   .data
		  1 0 0 m1  m0	1 s1  s0 x n8 n7   n6 n5 n4 n3 n2 n1 n0	  
			-type-	  source      -designation- --triplet--
		*/

		designation = (trip->data >> 4) + ((trip->address & 1) << 3);
		triplet = trip->data & 15;

		fmt_log ("... local obj %u/%u\n", designation, triplet);

		if (LOCAL_ENHANCEMENT_DATA != type
		    || triplet > 12) {
			fmt_log ("... invalid type %s or triplet %u\n",
				 _vbi_ttx_object_type_name (type),
				 triplet);
			return FALSE;
		}

		/* Shortcut. May find more missing packets in enhance(). */
		if (0 == (tp->cp->x26_designations & (1 << designation))) {
			fmt_log ("... have no packet X/26/%u\n",
				 designation);
			return FALSE;
		}

		offset = designation * 13 + triplet;

		trip = tp->cp->data.enh_lop.enh + offset;
		n_triplets = N_ELEMENTS (tp->cp->data.enh_lop.enh) - offset;

		break;
	}

	case 2:
	{
		vbi_pgno pgno;

		fmt_log ("... public obj\n");

		pgno = tp->cp->data.lop.link[25].pgno;

		if (NO_PAGE (pgno)) {
			unsigned int link;

			link = tp->mag->pop_lut[tp->cp->pgno & 0xFF];

			if (link > 7) {
				fmt_log ("... MOT pop_lut empty\n");
				return FALSE;
			}

			pgno = magazine_pop_link (tp, link)->pgno;

			if (NO_PAGE (pgno)) {
				fmt_log ("... dead MOT link %u\n", link);
				return FALSE;
			}
		} else {
			fmt_log ("... X/27/4 POP overrides MOT\n");
		}

		if (!resolve_obj_address (tp, &trip_cp, &trip, &n_triplets,
					  new_type, pgno,
					  triplet_object_address (trip),
					  PAGE_FUNCTION_POP))
			return FALSE;

		break;
	}

	case 3:
	{
		vbi_pgno pgno;

		fmt_log ("... global obj\n");

		pgno = tp->cp->data.lop.link[24].pgno;

		if (NO_PAGE (pgno)) {
			unsigned int link = 0;

			pgno = magazine_pop_link (tp, link)->pgno;

			if (NO_PAGE (pgno)) {
				fmt_log ("... dead MOT link %u\n", link);
				return FALSE;
			}
		} else {
			fmt_log ("... X/27/4 GPOP overrides MOT\n");
		}

		if (!resolve_obj_address (tp, &trip_cp, &trip, &n_triplets,
					  new_type, pgno,
					  triplet_object_address (trip),
					  PAGE_FUNCTION_GPOP))
			return FALSE;

		break;
	}

	default:
		assert (0);
	}

	success = enhance (tp, new_type, trip, n_triplets, row, column);

	cache_page_unref (trip_cp);

	fmt_log ("... object done, %s\n",
		 success ? "success" : "failed");

	return success;
}

/**
 * @internal
 * @param tp Current vbi_page.
 *
 * Like object_invocation(), but uses default object links if
 * available. Called when a page has no enhancement packets.
 */
static vbi_bool
default_object_invocation	(struct ttx_page *	tp)
{
	const struct ttx_pop_link *pop;
	unsigned int link;
	unsigned int order;
	unsigned int i;

	fmt_log ("default obj invocation\n");

	link = tp->mag->pop_lut[tp->cp->pgno & 0xFF];

	if (link > 7) {
		fmt_log ("...no pop link\n");
		return FALSE;
	}

	pop = magazine_pop_link (tp, link);

	if (NO_PAGE (pop->pgno)) {
		fmt_log ("... dead MOT pop link %u\n", link);
		return FALSE;
	}

	/* PASSIVE > ADAPTIVE > ACTIVE */
	order = (pop->default_obj[0].type > pop->default_obj[1].type);

	for (i = 0; i < 2; ++i) {
		enum ttx_object_type type;
		cache_page *trip_cp;
		const struct ttx_triplet *trip;
		unsigned int n_triplets;
		vbi_bool success;

		type = pop->default_obj[i ^ order].type;

		if (OBJECT_TYPE_NONE == type)
			continue;

		fmt_log ("... invocation %u, type %s\n",
			 i ^ order, _vbi_ttx_object_type_name (type));

		if (!resolve_obj_address (tp, &trip_cp, &trip, &n_triplets,
					  type, pop->pgno,
					  pop->default_obj[i ^ order].address,
					  PAGE_FUNCTION_POP))
			return FALSE;

		success = enhance (tp, type, trip, n_triplets, 0, 0);

		cache_page_unref (trip_cp);

		if (!success)
			return FALSE;
	}

	fmt_log ("... default object done\n");

	return TRUE;
}

/*
	DRCS
 */

static vbi_pgno
magazine_drcs_link		(const struct ttx_page *	tp,
				 unsigned int		link)
{
	vbi_pgno pgno;

	if (tp->max_level >= VBI_TTX_LEVEL_3p5) {
		pgno = tp->mag->drcs_link[1][link];

		if (!NO_PAGE (pgno))
			return pgno;
	}

	return tp->mag->drcs_link[0][link];
}

static const uint8_t *
get_drcs_data			(const cache_page *	drcs_cp,
				 unsigned int		glyph)
{
	uint64_t ptu_mask;

	if (!drcs_cp)
		return NULL;

	if (glyph >= 48)
		return NULL;

	switch (drcs_cp->data.drcs.mode[glyph]) {
	case DRCS_MODE_12_10_1:
		ptu_mask = ((uint64_t) 1) << glyph;
		break;

	case DRCS_MODE_12_10_2:
		/* Uses two PTUs. */
		ptu_mask = ((uint64_t) 3) << glyph;
		break;

	case DRCS_MODE_12_10_4:
	case DRCS_MODE_6_5_4:
		/* Uses four PTUs. */
		ptu_mask = ((uint64_t) 15) << glyph;
		break;

	default:
		/* No character data or glyph points into subsequent PTU. */
		return NULL;
	}

	if (drcs_cp->data.drcs.invalid & ptu_mask) {
		/* Data is incomplete. */
		return NULL;
	}

	return drcs_cp->data.drcs.chars[glyph];
}

/**
 * @internal
 * @param tp Current vbi_page.
 * @param normal Normal or global DRCS.
 * @param glyph 0 ... 47.
 * @param subno Subpage to take DRCS from
 *   (pgno from page links or magazine defaults).
 */
static vbi_bool
reference_drcs_page		(struct ttx_page *	tp,
				 unsigned int		normal,
				 unsigned int		glyph,
				 vbi_subno		subno)
{
	cache_page *drcs_cp;
	enum ttx_page_function function;
	vbi_pgno pgno;
	unsigned int plane;
	unsigned int link;

	plane = normal * 16 + subno;

	{
		const cache_page *drcs_cp;

		if ((drcs_cp = tp->drcs_cp[plane]))
			return !!get_drcs_data (drcs_cp, glyph);
	}

	link = 0;

	if (normal) {
		function = PAGE_FUNCTION_DRCS;
		pgno = tp->cp->data.lop.link[25].pgno;

		if (NO_PAGE (pgno)) {
			link = tp->mag->drcs_lut[tp->cp->pgno & 0xFF];

			if (link > 7) {
				fmt_log ("... MOT drcs_lut empty\n");
				return FALSE;
			}

			pgno = magazine_drcs_link (tp, link);

			if (NO_PAGE (pgno)) {
				fmt_log ("... dead MOT link %u\n", link);
				return FALSE;
			}
		} else {
			fmt_log ("... X/27/4 DRCS overrides MOT\n");
		}
	} else /* global */ {
		function = PAGE_FUNCTION_GDRCS;
		pgno = tp->cp->data.lop.link[26].pgno;

		if (NO_PAGE (pgno)) {
			pgno = magazine_drcs_link (tp, link);

			if (NO_PAGE (pgno)) {
				fmt_log ("... dead MOT link %u\n", link);
				return FALSE;
			}
		} else {
			fmt_log ("... X/27/4 GDRCS overrides MOT\n");
		}
	}

	fmt_log ("... %s drcs from page %03x.%04x\n",
		 normal ? "normal" : "global", pgno, subno);

	if (!(drcs_cp = get_system_page (tp, pgno, subno, function)))
		return FALSE;

	if (!get_drcs_data (drcs_cp, glyph)) {
		fmt_log ("... invalid drcs, prob. tx error\n");
		cache_page_unref (drcs_cp);
		return FALSE;
	}

	tp->drcs_cp[plane] = drcs_cp;

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
 * Pointer to character data, NULL if @a pg is invalid, not a Teletext
 * page, @a unicode is not a DRCS code, or no DRCS data is available.
 */
const uint8_t *
vbi_ttx_page_get_drcs_data	(const vbi_page *	pg,
				 unsigned int		unicode)
{
	const struct ttx_page *tp;
	const cache_page *drcs_cp;
	unsigned int plane;
	unsigned int glyph;

	_vbi_null_check (pg, NULL);

	if (!vbi_is_drcs (unicode))
		return NULL;

	if ((void *) TTX_PAGE_MAGIC != pg->_private)
		return FALSE;

	tp = CONST_PARENT (pg, struct ttx_page, pg);

	plane = (unicode >> 6) & 0x1F;

	if (!(drcs_cp = tp->drcs_cp[plane])) {
		/* No character of this plane in tp->text[]. */
		return NULL;
	}

	glyph = unicode & 0x3F;

	return get_drcs_data (drcs_cp, glyph);
}

/*
	Enhancement
 */

/** @internal */
typedef struct {
	/** Current page. */
	struct ttx_page *		tp;

	/** Referencing object type. */
	enum ttx_object_type		type;

	/** Triplet vector (triplets left). */
	const struct ttx_triplet *	trip;
	unsigned int			n_triplets;

	/** Current text row. */
	vbi_char *			curr_line;

	/** Cursor position at object invocation. */
	unsigned int			inv_row;
	unsigned int			inv_column;

	/** Current cursor position, relative to inv_row, inv_column. */
	unsigned int			active_row;
	unsigned int			active_column;

	/** Origin modifier. */
	unsigned int			offset_row;
	unsigned int			offset_column;

	vbi_color			row_color;
	vbi_color			next_row_color;

	/** Global (0) and normal (1) DRCS subno. */
	vbi_subno			drcs_s1[2];

	/** Current character set. */
	const vbi_ttx_charset *	cs;

	/** Accumulated attributes and attribute mask. */
	vbi_char			ac;
	vbi_char			mac;

	/** Foreground/background invert attribute. */
	vbi_bool			invert;

	/** PDC Preselection method "B". ETS 300 231, Section 7.3.2. */

#if 0
	/** Current entry. */
	vbi_preselection *		p1;
	/** Accumulator. */
	vbi_preselection		pdc_tmp;
	/** Previous hour triplet. */
	struct ttx_triplet		pdc_hour;
#endif
} enhance_state;

/**
 * @internal
 *
 * Flushes accumulated attributes up to but excluding column,
 * or -1 to end of row.
 */
static void
enhance_flush			(enhance_state *	st,
				 unsigned int		column_end)
{
	unsigned int row;
	unsigned int column;

	row = st->inv_row + st->active_row;

	if (row >= 25)
		return;

	if (column_end >= 40) {
		/* Flush entire row. */

		switch (st->type) {
		case OBJECT_TYPE_PASSIVE:
		case OBJECT_TYPE_ADAPTIVE:
			column_end = st->active_column + 1;
			break;

		default:
			column_end = 40;
			break;
		}
	}

	if (OBJECT_TYPE_PASSIVE == st->type
	    && !st->mac.unicode) {
		st->active_column = column_end;
		return;
	}

	fmt_log ("... flush [%04x%c,F%d%c,B%d%c,S%d%c,O%d%c,H%d%c] %d - %d\n",
		 st->ac.unicode, st->mac.unicode ? '*' : ' ',
		 st->ac.foreground, st->mac.foreground ? '*' : ' ',
		 st->ac.background, st->mac.background ? '*' : ' ',
		 st->ac.size, st->mac.size ? '*' : ' ',
		 st->ac.opacity, st->mac.opacity ? '*' : ' ',
		 !!(st->ac.attr & VBI_FLASH),
		 !!(st->mac.attr & VBI_FLASH) ? '*' : ' ',
		 st->active_column, column_end - 1);

	column = st->inv_column + st->active_column;

	while (column < (st->inv_column + column_end) && column < 40) {
		vbi_char c;
		int raw;

		c = st->curr_line[column];

		if (st->mac.attr & VBI_UNDERLINE) {
			unsigned int attr = st->ac.attr;

			if (!st->mac.unicode)
				st->ac.unicode = c.unicode;

			if (vbi_is_gfx (st->ac.unicode)) {
				if (attr & VBI_UNDERLINE) {
					/* Separated. */
					st->ac.unicode &= ~0x20;
				} else {
					/* Contiguous. */
					st->ac.unicode |= 0x20;
				}

				st->mac.unicode = 0xFFFF;
				attr &= ~VBI_UNDERLINE;
			}

			COPY_SET_MASK (c.attr, attr, VBI_UNDERLINE);
		}

		if (st->mac.foreground)
			c.foreground = (st->ac.foreground
					== VBI_TRANSPARENT_BLACK) ?
				st->row_color : st->ac.foreground;

		if (st->mac.background)
			c.background = (st->ac.background
					== VBI_TRANSPARENT_BLACK) ?
				st->row_color : st->ac.background;

		if (st->invert)
			SWAP (c.foreground, c.background);

		if (st->mac.opacity)
			c.opacity = st->ac.opacity;

		COPY_SET_MASK (c.attr, st->ac.attr,
			       st->mac.attr & (VBI_FLASH | VBI_CONCEAL));

		if (st->mac.unicode) {
			c.unicode = st->ac.unicode;
			st->mac.unicode = 0;

			if (st->mac.size)
				c.size = st->ac.size;
			else if (c.size > VBI_DOUBLE_SIZE)
				c.size = VBI_NORMAL_SIZE;
		}

		st->curr_line[column++] = c;

		if (OBJECT_TYPE_PASSIVE == st->type)
			break;

		if (OBJECT_TYPE_ADAPTIVE == st->type)
			continue;

		/* OBJECT_TYPE_ACTIVE */

		raw = (0 == row && column < 9) ?
			0x20 : vbi_unpar8 (st->tp->cp->data.lop.raw
					    [row][column - 1]);

		/* Set-after spacing attributes cancelling non-spacing. */

		switch (raw) {
		case 0x00 ... 0x07:	/* alpha + foreground color */
		case 0x10 ... 0x17:	/* mosaic + foreground color */
			fmt_log ("... fg term %d %02x\n", column, raw);
			st->mac.foreground = 0;
			st->mac.attr &= ~VBI_CONCEAL;
			break;

		case 0x08:		/* flash */
			st->mac.attr &= ~VBI_FLASH;
			break;

		case 0x0A:		/* end box */
		case 0x0B:		/* start box */
			if (column < 40
			    && raw == vbi_unpar8
			    (st->tp->cp->data.lop.raw[row][column])) {
				fmt_log ("... boxed term %d %02x\n",
					 column, raw);
				st->mac.opacity = 0;
			}
			
			break;
			
		case 0x0D:		/* double height */
		case 0x0E:		/* double width */
		case 0x0F:		/* double size */
			fmt_log ("... size term %d %02x\n", column, raw);
			st->mac.size = 0;
			break;
		}
		
		if (column > 39)
			break;

		raw = (0 == row && column < 8) ?
			0x20 : vbi_unpar8 (st->tp->cp->data.lop.raw
					    [row][column]);

		/* Set-at spacing attributes cancelling non-spacing. */

		switch (raw) {
		case 0x09:		/* steady */
			st->mac.attr &= ~VBI_FLASH;
			break;
			
		case 0x0C:		/* normal size */
			fmt_log ("... size term %d %02x\n", column, raw);
			st->mac.size = 0;
			break;
			
		case 0x18:		/* conceal */
			st->mac.attr &= ~VBI_CONCEAL;
			break;
			
		/* Non-spacing underlined/separated display attribute
		   cannot be cancelled by a subsequent spacing attribute. */

		case 0x1C:		/* black background */
		case 0x1D:		/* new background */
			fmt_log ("... bg term %d %02x\n", column, raw);
			st->mac.background = 0;
			break;
		}
	}

	st->active_column = column_end;
}

static vbi_bool
enhance_row_triplet		(enhance_state *	st)
{
	unsigned int row;
	unsigned int column;
	unsigned int s;
	unsigned int pos;

#if 0
	if (st->pdc_hour.mode > 0) {
		fmt_log ("... enh invalid pdc hours, no minutes follow\n");
		return FALSE;
	}
#endif

	row = st->trip->address - 40;
	row = (0 == row) ? 24 : row;

	column = 0;

	s = st->trip->data >> 5;

	switch (st->trip->mode) {
	case 0x00:		/* full screen color */
		if (st->tp->max_level < VBI_TTX_LEVEL_2p5)
			break;

		if (0 == s && st->type <= OBJECT_TYPE_ACTIVE) {
			unsigned int flags;
			unsigned int color;

			flags = st->tp->cp->flags;
			color = st->trip->data & 0x1F;

			init_border_color (&st->tp->pg, flags, color);
		}

		break;

	case 0x07:		/* address display row 0 */
		if (st->tp->max_level < VBI_TTX_LEVEL_1p5)
			break;
		
		if (st->trip->address != 0x3F)
			break; /* reserved, no position */

		row = 0;

		/* fall through */

	case 0x01:		/* full row color */
		if (st->tp->max_level < VBI_TTX_LEVEL_1p5)
			break;

		st->row_color = st->next_row_color;

		switch (s) {
		case 0:
			st->row_color = st->trip->data & 0x1F;
			st->next_row_color = st->tp->ext->def_row_color;
			break;

		case 3:
			st->row_color = st->trip->data & 0x1F;
			st->next_row_color = st->row_color;
			break;
		}

		goto set_active;

	case 0x02:		/* reserved */
	case 0x03:		/* reserved */
		break;

	case 0x04:		/* set active position */
		if (st->tp->max_level >= VBI_TTX_LEVEL_2p5) {
			if (st->trip->data >= 40)
				break; /* reserved */

			column = st->trip->data;
		} else if (st->tp->max_level < VBI_TTX_LEVEL_1p5) {
			break;
		}

		if (row > st->active_row)
			st->row_color = st->next_row_color;

	set_active:
		if (row > 0 && 1 == st->tp->pg.rows) {
			/* Shortcut: we need only row 0 triplets,
			   skip other. */
			while (st->n_triplets > 1) {
				if (st->trip[1].address >= 40) {
					unsigned int mode;

					mode = st->trip[1].mode;

					if (mode >= 0x20)
						return FALSE;
					if (mode == 0x1F)
						goto terminate;
					if (mode == 0x07)
						break;
				}

				++st->trip;
				--st->n_triplets;
			}

			break;
		}

		fmt_log ("... enh set_active row %d col %d\n", row, column);

		if (row > st->active_row) {
			enhance_flush (st, (unsigned int) -1);

			if (st->type != OBJECT_TYPE_PASSIVE)
				CLEAR (st->mac);
		}

		st->active_row = row;
		st->active_column = column;

		pos = (st->inv_row + st->active_row) * st->tp->pg.columns;
		st->curr_line = &st->tp->pg.text[pos];

		break;

	case 0x05:		/* reserved */
	case 0x06:		/* reserved */
		break;

	case 0x08:		/* PDC data - Country of Origin
				   and Programme Source */
	{
		/*
		   .address	    .data
		   1 1 c3 c2 c1 c0  n6	n5 n4 n3 n2 n1 n0
		       --country--  msb ------source-----

		   -> 0011 cccc 0mss ssss  as in TR 101 231.
		*/
#if 0
		st->pdc_tmp.cni = st->trip->address * 256 + st->trip->data;
		st->pdc_tmp.cni_type = VBI_CNI_TYPE_PDC_B;
#endif
		break;
	}

	case 0x09:		/* PDC data - Month and Day */
		/*
		   .address	    .data
		   1 1 m3 m2 m1 m0  0 t1 t0 u3 u2 u1 u0
		       ---month---    ------day--------

		   We do a plausibility check when we find start hour
		   too.
		*/
#if 0
		st->pdc_tmp.month = (st->trip->address & 15);
		st->pdc_tmp.day = (st->trip->data >> 4) * 10
				   + (st->trip->data & 15);
#endif
		break;

	case 0x0A:		/* PDC data - Cursor Row and
				   Announced Starting Time Hours */
		/*
		   .address	     .data
		   1 r4 r3 r2 r1 r0  caf t1 t0 u3 u2 u1 u0
		     ------row-----	 -------hour------
		*/
#if 0
		if (!st->p1) {
			break;
		}

		if (   ((unsigned int) st->pdc_tmp.month - 1) > 11
		    || ((unsigned int) st->pdc_tmp.day - 1) > 30
		    || 0 == st->pdc_tmp.cni) {
			/* PDC data is broken, but no need to throw
			   away all the enhancement. */
			st->p1 = NULL;
			break;
		}

		st->pdc_tmp.at2_hour = (((st->trip->data & 0x30) >> 4) * 10
					+ (st->trip->data & 15));
		st->pdc_tmp.length = 0;

		st->pdc_tmp._at1_ptl[1].row = row;

		st->pdc_tmp.flags = 0;
		if (st->trip->data & 0x40)
			st->pdc_tmp.flags = VBI_PDC_ENCRYPTED;

		st->pdc_hour = *st->trip;
#endif
		break;

	case 0x0B:		/* PDC data - Cursor Row and
				   Announced Finishing Time Hours */
		/*
		   .address	     .data
		   1 r4 r3 r2 r1 r0  msb t1 t0 u3 u2 u1 u0
		     -----row------	 -------hour------
		*/
#if 0
		/* msb: Duration or end time, see column triplet 0x06. */
		st->pdc_tmp.at2_hour = (((st->trip->data & 0x70) >> 4) * 10
					+ (st->trip->data & 15));
		st->pdc_hour = *st->trip;
#endif
		break;

	case 0x0C:		/* PDC data - Cursor Row and
				   Local Time Offset */
	{
		unsigned int lto;

		/*
		   .address	     .data
		   1 r4 r3 r2 r1 r0  t6 t5 t4 t3 t2 t1 t0
		     ------row-----  -------offset-------
		*/

		lto = st->trip->data;

		if (lto & 0x40) /* 7 bit two's complement */
			lto |= ~0x7F;
#if 0
		st->pdc_tmp.seconds_east = lto * 15 * 60;
		st->pdc_tmp.seconds_east_valid = TRUE;
#endif
		break;
	}

	case 0x0D:		/* PDC data - Series Identifier
				   and Series Code */
		/*
		   .address	  .data
		   1 1 0 0 0 0 0  p6 p5 p4 p3 p2 p1 p0
				  ---------pty--------
		*/

		/* 0x30 == is series code. */
		if (0x30 != st->trip->address)
			break;
#if 0		
		st->pdc_tmp.pty = 0x80 + st->trip->data;
#endif
		break;

	case 0x0E:		/* reserved */
	case 0x0F:		/* reserved */
		break;

	case 0x10:		/* origin modifier */
		if (st->tp->max_level < VBI_TTX_LEVEL_2p5)
			break;

		if (st->trip->data >= 72)
			return FALSE; /* invalid */

		/* Note side panels are not implemented yet.  Characters
		   for column >= 40 which should go into left or right
		   panel are discarded. */

		st->offset_row = st->trip->address - 40;
		st->offset_column = st->trip->data;

		fmt_log ("... enh origin modifier col %+d row %+d\n",
			 st->offset_column, st->offset_row);

		break;

	case 0x11 ... 0x13:	/* object invocation */
	{
#if 0
		vbi_preselection *table;
#endif
		vbi_bool success;

		if (st->tp->max_level < VBI_TTX_LEVEL_2p5)
			break;
		
		row = st->inv_row + st->active_row;
		column = st->inv_column + st->active_column;

#if 0
		table = st->tp->pdc_table;
		st->tp->pdc_table = NULL;
#endif
		success = object_invocation (st->tp, st->type, st->trip,
					     row + st->offset_row,
					     column + st->offset_column);
#if 0
		st->tp->pdc_table = table;
#endif
		if (!success)
			return FALSE;

		st->offset_row = 0;
		st->offset_column = 0;
		
		break;
	}

	case 0x14:		/* reserved */
		break;

	case 0x15 ... 0x17:	/* object definition */
		fmt_log ("... enh obj definition 0x%02x 0x%02x\n",
			 st->trip->mode, st->trip->data);
		goto terminate;

	case 0x18:		/* drcs mode */
		fmt_log ("... enh DRCS mode 0x%02x\n", st->trip->data);

		st->drcs_s1[st->trip->data >> 6] = st->trip->data & 15;

		break;

	case 0x19 ... 0x1E:	/* reserved */
		break;

	case 0x1F:		/* termination marker */
	terminate:
		if (st->tp->max_level >= VBI_TTX_LEVEL_1p5)
			enhance_flush (st, (unsigned int) -1);

		fmt_log ("... enh terminated %02x\n", st->trip->mode);

		st->n_triplets = 0;

		break;

	default:
		return FALSE;
	}

	return TRUE;
}

#if 0
static void
pdc_duration			(enhance_state *	st)
{
	int length;

	length = (st->pdc_tmp.at2_hour - st->p1[-1].at2_hour) * 60
		+ (st->pdc_tmp.at2_minute - st->p1[-1].at2_minute);

	if (length < 0 || length >= 12 * 60) {
		/* Junk. */
		--st->p1;
	} else {
		st->p1[-1].length = length;
	}
}
#endif

static vbi_bool
enhance_column_triplet		(enhance_state *	st)
{
	unsigned int unicode;
	unsigned int column;
	unsigned int s;

	column = st->trip->address;
	s = st->trip->data >> 5;

	switch (st->trip->mode) {
	case 0x00:		/* foreground color */
		if (st->tp->max_level < VBI_TTX_LEVEL_2p5)
			break;

		if (0 != s)
			break;

		if (column > st->active_column)
			enhance_flush (st, column);

		st->ac.foreground = st->trip->data & 0x1F;
		st->mac.foreground = 0xFF;

		fmt_log ("... enh col %d foreground %d\n",
			 st->active_column, st->ac.foreground);

		break;

	case 0x01:		/* G1 block mosaic character */
		if (st->tp->max_level < VBI_TTX_LEVEL_2p5)
			break;

		if (column > st->active_column)
			enhance_flush (st, column);

		if (st->trip->data & 0x20) {
			/* G1 contiguous */
			unicode = 0xEE00 + st->trip->data;
			goto store;
		} else if (st->trip->data >= 0x40) {
			unsigned int code;

			code = st->trip->data;
			unicode = vbi_ttx_unicode
				(st->cs->g0, VBI_SUBSET_NONE, code);
			goto store;
		}

		break;

	case 0x0B:		/* G3 smooth mosaic or
				   line drawing character */
		if (st->tp->max_level < VBI_TTX_LEVEL_2p5)
			break;

		/* fall through */

	case 0x02:		/* G3 smooth mosaic or
				   line drawing character */
		if (st->tp->max_level < VBI_TTX_LEVEL_1p5)
			break;

		if (st->trip->data < 0x20)
			break;

		if (column > st->active_column)
			enhance_flush (st, column);

		unicode = 0xEF00 + st->trip->data;

		goto store;

	case 0x03:		/* background color */
		if (st->tp->max_level < VBI_TTX_LEVEL_2p5)
			break;

		if (0 != s)
			break;

		if (column > st->active_column)
			enhance_flush (st, column);

		st->ac.background = st->trip->data & 0x1F;
		st->mac.background = 0xFF;

		fmt_log ("... enh col %d background %d\n",
			 st->active_column, st->ac.background);

		break;

	case 0x04:		/* reserved */
	case 0x05:		/* reserved */
		break;

	case 0x06:		/* PDC data - Cursor Column and
				   Announced Starting and
				   Finishing Time Minutes */
		/*
		   .address	      .data
		   c5 c4 c3 c2 c1 c0  t2 t1 t0 u3 u2 u1 u0
		   ------column-----  -------minute-------
		*/
#if 0
		if (!st->p1)
			break;

		st->pdc_tmp.at2_minute = ((st->trip->data >> 4) * 10
					  + (st->trip->data & 15));

		switch (st->pdc_hour.mode) {
		case 0x0A: /* Starting time */
			if (st->p1 > st->tp->pdc_table
			    && 0 == st->p1[-1].length) {
				pdc_duration (st);
			}

			st->pdc_tmp._at1_ptl[1].column_begin = column;
			st->pdc_tmp._at1_ptl[1].column_end = 40;

			if (st->p1 >= st->tp->pdc_table + 24)
				return FALSE;

			*st->p1++ = st->pdc_tmp;

			st->pdc_tmp.pty = 0;

			break;

		case 0x0B: /* Finishing time */
			if (st->p1 == st->tp->pdc_table) {
				/* Finish what? PDC is broken, but no need
				   to throw away all the enhancement. */
				st->p1 = NULL;
				break;
			}

			/* See row triplet 0x0B. */
			if (st->pdc_tmp.at2_hour >= 40) {
				/* Duration. */

				st->p1[-1].length =
					(st->pdc_tmp.at2_hour - 40) * 60
					+ st->pdc_tmp.at2_minute;
			} else {
				/* End time. */

				pdc_duration (st);
			}

			break;

		default:
			fmt_log ("... pdc hour triplet missing\n");

			/* PDC is broken. */
			st->p1 = NULL;
			break;
		}

		st->pdc_hour.mode = 0;
#endif
		break;

	case 0x07:		/* additional flash functions */
		if (st->tp->max_level < VBI_TTX_LEVEL_2p5)
			break;

		if (column > st->active_column)
			enhance_flush (st, column);

		/* XXX Only one flash function implemented:
		   Mode 1 - Normal flash to background color
		   Rate 0 - Slow rate (1 Hz) */
		COPY_SET_COND (st->ac.attr, VBI_FLASH, st->trip->data & 3);
		st->mac.attr |= VBI_FLASH;

		fmt_log ("... enh col %d flash 0x%02x\n",
			 st->active_column, st->trip->data);

		break;

	case 0x08:		/* modified G0 and G2
				   character set designation */
	{
		const vbi_ttx_charset *cs;

		if (st->tp->max_level < VBI_TTX_LEVEL_2p5)
			break;

		if (column > st->active_column)
			enhance_flush (st, column);

		cs = vbi_ttx_charset_from_code
			((unsigned int) st->trip->data);

		if (cs) {
			st->cs = cs;

			fmt_log ("... enh col %d modify character set %d\n",
				 st->active_column, st->trip->data);
		}

		break;
	}

	case 0x09:		/* G0 character */
		if (st->tp->max_level < VBI_TTX_LEVEL_2p5)
			break;

		if (st->trip->data < 0x20)
			break;

		if (column > st->active_column)
			enhance_flush (st, column);

		unicode = vbi_ttx_unicode (st->cs->g0, VBI_SUBSET_NONE,
					   (unsigned int) st->trip->data);
		goto store;

	case 0x0A:		/* reserved */
		break;

	case 0x0C:		/* display attributes */
		if (st->tp->max_level < VBI_TTX_LEVEL_2p5)
			break;

		if (column > st->active_column)
			enhance_flush (st, column);

		st->ac.size = ((st->trip->data & 0x40) ? VBI_DOUBLE_WIDTH : 0)
			+ ((st->trip->data & 1) ? VBI_DOUBLE_HEIGHT : 0);
		st->mac.size = 0xFF;

		if (st->trip->data & 2) {
			if (st->tp->cp->flags & (C5_NEWSFLASH | C6_SUBTITLE))
				st->ac.opacity = VBI_TRANSLUCENT;
			else
				st->ac.opacity = VBI_TRANSPARENT_SPACE;
		} else {
			st->ac.opacity = st->tp->page_opacity[1];
		}

		st->mac.opacity = 0xFF;

		COPY_SET_COND (st->ac.attr, VBI_CONCEAL,
			       st->trip->data & 4);
		st->mac.attr |= VBI_CONCEAL;

		/* (st->trip->data & 8) reserved */

		st->invert = !!(st->trip->data & 0x10);

		COPY_SET_COND (st->ac.attr, VBI_UNDERLINE,
			       st->trip->data & 0x20);
		st->mac.attr |= VBI_UNDERLINE;

		fmt_log ("... enh col %d display attr 0x%02x\n",
			 st->active_column, st->trip->data);

		break;

	case 0x0D:		/* drcs character invocation */
	{
		unsigned int normal;
		unsigned int glyph;
		unsigned int plane;

		if (st->tp->max_level < VBI_TTX_LEVEL_2p5)
			break;

		glyph = st->trip->data & 0x3F;
		normal = st->trip->data >> 6;

		if (glyph >= 48) {
			fmt_log ("invalid drcs offset %u\n", glyph);
			break;
		}

		if (column > st->active_column)
			enhance_flush (st, column);

		fmt_log ("... enh col %d DRCS %u/%u\n",
			 st->active_column, normal, glyph);

		if (!reference_drcs_page (st->tp, normal, glyph,
					  st->drcs_s1[normal]))
			return FALSE;

		plane = 16 * normal + st->drcs_s1[normal];
		unicode = 0xF000 + (plane << 6) + glyph;

		goto store;
	}

	case 0x0E:		/* font style */
	{
		vbi_char *acp;
		unsigned int n_rows;
		unsigned int row;
		unsigned int attr;

		if (st->tp->max_level < VBI_TTX_LEVEL_3p5)
			break;

		attr = 0;

		if (st->trip->data & 0x01)
			attr |= VBI_PROPORTIONAL;
		if (st->trip->data & 0x02)
			attr |= VBI_BOLD;
		if (st->trip->data & 0x04)
			attr |= VBI_ITALIC;

		n_rows = (st->trip->data >> 4) + 1;

		fmt_log ("... enh %u ... %u, %u ... %u font style 0x%02x\n",
			 st->inv_column + column, 39,
			 row, row + n_rows - 1, attr);

		row = st->inv_row + st->active_row;
		acp = &st->tp->pg.text[row * st->tp->pg.columns];

		while (row < 25 && n_rows > 0) {
			unsigned int col;

			for (col = st->inv_column + column; col < 40; ++col) {
				COPY_SET_MASK (acp[col].attr, attr,
					       (VBI_PROPORTIONAL |
						VBI_BOLD |
						VBI_ITALIC));
			}
			
			acp += st->tp->pg.columns;
			++row;
			--n_rows;
		}
		
		break;
	}

	case 0x0F:		/* G2 character */
		if (st->tp->max_level < VBI_TTX_LEVEL_1p5)
			break;

		if (st->trip->data < 0x20)
			break;

		if (column > st->active_column)
			enhance_flush (st, column);

		unicode = vbi_ttx_unicode (st->cs->g2, VBI_SUBSET_NONE,
					   (unsigned int) st->trip->data);
		goto store;

	case 0x10 ... 0x1F:	/* characters including
				   diacritical marks */
		if (st->tp->max_level < VBI_TTX_LEVEL_1p5)
			break;

		if (st->trip->data < 0x20)
			break;

		if (column > st->active_column)
			enhance_flush (st, column);

		unicode = vbi_ttx_composed_unicode
			((unsigned int)(st->trip->mode - 0x10),
			 (unsigned int) st->trip->data);

	store:
		fmt_log ("... enh row %d col %d print "
			 "0x%02x/0x%02x -> 0x%04x %c\n",
			 st->active_row, st->active_column,
			 st->trip->mode, st->trip->data,
			 unicode, unicode & 0xFF);

		st->ac.unicode = unicode;
		st->mac.unicode = 0xFFFF;

		break;

	default:
		return FALSE;
	}

	return TRUE;
}

/**
 * @internal
 * @param tp Current vbi_page.
 * @param type Current object type.
 * @param trip Triplet vector.
 * @param n_triplets Triplet vector size.
 * @param inv_row Cursor position at object invocation.
 * @param inv_column Cursor position at object invocation.
 *
 * Enhances a level one page with data from page enhancement
 * packets.
 *
 * @bugs
 * XXX panels not implemented.
 *
 * @returns
 * FALSE on failure.
 */
static vbi_bool
enhance				(struct ttx_page *	tp,
				 enum ttx_object_type	type,
				 const struct ttx_triplet *trip,
				 unsigned int		n_triplets,
				 unsigned int		inv_row,
				 unsigned int		inv_column)
{
	enhance_state st;

	st.tp			= tp;

	st.type			= type;

	st.trip			= trip;
	st.n_triplets		= n_triplets;

	st.inv_row		= inv_row;
	st.inv_column		= inv_column;

	st.active_row		= 0;
	st.active_column	= 0;

	st.curr_line		= tp->pg.text + inv_row * tp->pg.columns;

	st.offset_row		= 0;
	st.offset_column	= 0;

	st.row_color		= tp->ext->def_row_color;
	st.next_row_color	= tp->ext->def_row_color;

	st.drcs_s1[0]		= 0; /* global */
	st.drcs_s1[1]		= 0; /* normal */

	st.cs			= tp->char_set[0];

	CLEAR (st.ac);
	CLEAR (st.mac);

	st.invert		= FALSE;

	if (OBJECT_TYPE_PASSIVE == type) {
		st.ac.foreground	= VBI_WHITE;
		st.ac.background	= VBI_BLACK;
		st.ac.opacity		= tp->page_opacity[1];

		st.mac.foreground	= 0xFF;
		st.mac.background	= 0xFF;
		st.mac.opacity		= 0xFF;
		st.mac.size		= 0xFF;
		st.mac.attr		= (VBI_UNDERLINE |
					   VBI_CONCEAL |
					   VBI_FLASH);
	}

	/* PDC Preselection method "B"
	   ETS 300 231, Section 7.3.2. */
#if 0
	st.p1 = tp->pdc_table;

	CLEAR (st.pdc_tmp);

	st.pdc_tmp.month = 0;
	st.pdc_hour.mode = 0;
#endif

	/* Main loop. */

	while (st.n_triplets > 0) {
		if (st.trip->address >= 40) {
			if (!enhance_row_triplet (&st))
				return FALSE;

			if (0 == st.n_triplets)
				break;
		} else {
			if (!enhance_column_triplet (&st))
				return FALSE;
		}

		++st.trip;
		--st.n_triplets;
	}

#if 0
	if (st.p1 > tp->pdc_table) {
		vbi_preselection *p;
		time_t now;
		struct tm t;

		time (&now);

		if ((time_t) -1 == now) {
			tp->pdc_table_size = 0;
			goto finish;
		}

		localtime_r (&now, &t);

		if (st.pdc_hour.mode > 0
		    || 0 == st.p1[-1].length)
			--st.p1; /* incomplete start or end tag */

		for (p = tp->pdc_table; p < st.p1; ++p) {
			int d;

			/* XXX correct?? */
			d = t.tm_mon - p->month;
			if (d > 0 && d <= 6)
				p->year = t.tm_year + 1901;
			else
				p->year = t.tm_year + 1900;

			p->at1_hour = p->at2_hour;
			p->at1_minute = p->at2_minute;
		}

		tp->pdc_table_size = st.p1 - tp->pdc_table;

		if (0)
			_vbi_preselection_array_dump (tp->pdc_table,
						      tp->pdc_table_size,
						      stderr);
	}
#endif

 finish:
	if (0)
		_vbi_ttx_page_dump (tp, stderr, 2);

	return TRUE;
}

static void
post_enhance			(struct ttx_page *	tp)
{
	vbi_char ac, *acp;
	unsigned int row;
	unsigned int column;
	unsigned int last_row;
	unsigned int last_column;

	acp = tp->pg.text;

	last_row = (1 == tp->pg.rows) ? 0 : 25 - 2;
	last_column = tp->pg.columns - 1;

	for (row = 0; row <= last_row; ++row) {
		for (column = 0; column < tp->pg.columns; ++acp, ++column) {
			if (acp->opacity == VBI_TRANSPARENT_SPACE
			    || (acp->foreground == VBI_TRANSPARENT_BLACK
				&& acp->background == VBI_TRANSPARENT_BLACK)) {
				acp->opacity = VBI_TRANSPARENT_SPACE;
				acp->unicode = 0x0020;
			} else if (acp->background == VBI_TRANSPARENT_BLACK) {
				acp->opacity = VBI_TRANSLUCENT;
			} else {
				/* transparent foreground not implemented */
			}

			switch (acp->size) {
			case VBI_NORMAL_SIZE:
				if (row < last_row
				    && (acp[tp->pg.columns].size
					== VBI_DOUBLE_HEIGHT2
					|| acp[tp->pg.columns].size
					== VBI_DOUBLE_SIZE2)) {
					acp[tp->pg.columns].unicode = 0x0020;
					acp[tp->pg.columns].size =
						VBI_NORMAL_SIZE;
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
					acp[tp->pg.columns] = ac;
				}

				break;

			case VBI_DOUBLE_SIZE:
				if (row < last_row) {
					ac = acp[0];
					ac.size = VBI_DOUBLE_SIZE2;
					acp[tp->pg.columns] = ac;
					ac.size = VBI_OVER_BOTTOM;
					acp[tp->pg.columns + 1] = ac;
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

/* PDC Preselection -------------------------------------------------------- */

#if 0

/**
 * @internal
 * @param pg Formatted page.
 * @param pbegin Take AT-1 and PTL positions from this table.
 * @param pend End of the table.
 *
 * Adds PDC flags to the text and extracts program titles.
 */
static void
pdc_post_proc			(vbi_page *		pg,
				 vbi_preselection *	pbegin,
				 vbi_preselection *	pend)
{
	vbi_preselection *p;

	for (p = pbegin; p < pend; ++p) {
		unsigned int i;

		for (i = 0; i < N_ELEMENTS (p->_at1_ptl); ++i) {
			unsigned int row;

			row = p->_at1_ptl[i].row;

			if (row > 0) {
				vbi_char *acp;
				unsigned int j;

				acp = pg->text + pg->columns * row;

				/* We could mark exactly the AT-1 and PTL
				   characters, but from a usability
				   standpoint it's better to make the
				   sensitive area larger. */
				for (j = 0; j < pg->columns; ++j) {
					acp[j].attr |= VBI_PDC;

					switch (acp[j].size) {
					case VBI_DOUBLE_HEIGHT:
					case VBI_DOUBLE_SIZE:
					case VBI_OVER_TOP:
						acp[pg->columns].attr |=
							VBI_PDC;
						break;

					default:
						break;
					}
				}
			}
		}

#ifndef ZAPPING8
		_vbi_pdc_title_post_proc (pg, p);
#endif
	}
}

/**
 * @param pg
 * @param column Column 0 ... pg->columns - 1 of the character in question.
 * @param row Row 0 ... pg->rows - 1 of the character in question.
 * 
 * Describe me.
 *
 * @returns
 * Describe me.
 */
const vbi_preselection *
vbi_ttx_page_get_pdc_link	(const vbi_page *	pg,
				 unsigned int		column,
				 unsigned int		row)
{
	const struct ttx_page *tp;
	const vbi_preselection *p;
	const vbi_preselection *end;
	const vbi_preselection *match;

	TP_CHECK (FALSE);

	if (0 == row
	    || row >= tp->pg.rows
	    || column >= tp->pg.columns)
		return FALSE;

	end = tp->pdc_table + tp->pdc_table_size;
	match = NULL;

	for (p = tp->pdc_table; p < end; ++p) {
		unsigned int i;

		for (i = 0; i < N_ELEMENTS (p->_at1_ptl); ++i) {
			if (row != p->_at1_ptl[i].row)
				continue;

			if (!match)
				match = p;

			if (column >= p->_at1_ptl[i].column_begin
			    && column < p->_at1_ptl[i].column_end)
				goto finish;
		}
	}

	if (!match)
		return NULL;

	p = match;

 finish:
	return p;
}

/**
 * @param pg
 * @param n_elements
 * 
 * Describe me.
 *
 * @returns
 * Describe me.
 */
const vbi_preselection *
vbi_ttx_page_get_preselections	(const vbi_page *	pg,
				 unsigned int *		n_elements)
{
	const struct ttx_page *tp;

	TP_CHECK (FALSE);

	assert (NULL != n_elements);

	*n_elements = tp->pdc_table_size;

	return tp->pdc_table;
}

#endif

/* Column 41 --------------------------------------------------------------- */

/**
 * @internal
 * @param tp Current vbi_page.
 *
 * Artificial 41st column. Often column 0 of a LOP contains only set-after
 * attributes and thus all black spaces, unlike column 39. To balance the
 * view we add a black column 40. If OTOH column 0 has been modified using
 * enhancement we extend column 39.
 */
static void
column_41			(struct ttx_page *	tp)
{
	vbi_char *acp;
	unsigned int row;
	vbi_bool black0;
	vbi_bool cont39;

	acp = tp->pg.text;

	/* Header. */

	acp[40] = acp[39];
	acp[40].unicode = 0x0020;

	if (1 == tp->pg.rows)
		return;

	/* Body. */

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

	acp = tp->pg.text + 41;

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
		ac.foreground	= tp->ext->foreground_clut + VBI_WHITE;
		ac.background	= tp->ext->background_clut + VBI_BLACK;
		ac.opacity	= tp->page_opacity[1];

		for (row = 1; row <= 24; ++row) {
			acp[40] = ac;
			acp += 41;
		}
	}

	/* Navigation bar. */

	acp[40] = acp[39];
	acp[40].unicode = 0x0020;
}

/* Hyperlinks -------------------------------------------------------------- */

#if 0

#define SECTION_SIGN 0xA7

/* Simple case insensitive strcmp. */
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
			if (0x20 == (*s ^ c))
				continue;

		return 0;
	}

	return s - s1;
}

static vbi_bool
keyword				(vbi_link *		ld,
				 vbi_network *		nk,
				 const char *		buf,
				 vbi_pgno		pgno,
				 vbi_subno		subno,
				 unsigned int *		start,
				 unsigned int *		end)
{
	const char *s;
	unsigned int len;
	unsigned int address;
	vbi_link_type type;
	const char *proto;

	s = buf + *start;
	*end = *start + 1; /* unknown character */

	proto = "";

	if (isdigit (*s)) {
		const char *s1;
		unsigned int num1;
		unsigned int num2;

		/* Page number "###". */

		s1 = s;
		num1 = 0;

		do num1 = num1 * 16 + (*s & 15);
		while (isdigit (*++s));

		len = s - s1;
		*end += len - 1;

		if (len > 3 || isdigit (s1[-1]))
			return FALSE;

		if (3 == len) {
			if (pgno == (vbi_pgno) num1)
				return FALSE;

			if (num1 < 0x100 || num1 > 0x899)
				return FALSE;

			if (ld) {
				vbi_link_init (ld);

				ld->type = VBI_LINK_PAGE;
				ld->network = nk;
				ld->pgno = num1;
			}

			return TRUE;
		}

		/* Subpage number "##/##". */

		if (*s != '/' && *s != ':')
			return FALSE;

		s1 = ++s;
		num2 = 0;

		while (isdigit (*s))
			num2 = num2 * 16 + (*s++ & 15);

		len = s - s1; 
		*end += len + 1;

		if (0 == len || len > 2
		    || subno != (vbi_subno) num1)
			return FALSE;

		if (ld) {
			vbi_link_init (ld);

			ld->type = VBI_LINK_SUBPAGE;
			ld->network = nk;
			ld->pgno = pgno;

			if (num1 == num2)
				ld->subno = 0x01; /* wrap */
			else
				ld->subno = vbi_add_bcd ((int) num1, 0x01);
		}

		return TRUE;
	} else if (*s == '>'
		   && s[1] == '>'
		   && s[-1] != '>') {
		for (s += 2; 0x20 == *s; ++s)
			;

		*end = s - buf;

		if (*s)
			return FALSE;

		if (0 == subno
		    || VBI_ANY_SUBNO == subno) {
			if (0x899 == pgno)
				return FALSE;

			if (ld) {
				vbi_link_init (ld);

				ld->type = VBI_LINK_PAGE;
				ld->network = nk;
				ld->pgno = vbi_add_bcd (pgno, 0x001);
			}

			return TRUE;
		} else if (subno < 0x99) {
			/* XXX wrap? */

			if (ld) {
				vbi_link_init (ld);

				ld->type = VBI_LINK_SUBPAGE;
				ld->network = nk;
				ld->pgno = pgno;
				ld->subno = vbi_add_bcd (subno, 0x01);
			}

			return TRUE;
		}

		return FALSE;
	} else if (*s == 'h') {
		if (0 == (len = keycmp (s, "https://"))
		    && 0 == (len = keycmp (s, "http://")))
			return FALSE;
		type = VBI_LINK_HTTP;
	} else if (*s == '(') {
		if (0 == (len = keycmp (s, "(at)"))
		    && 0 == (len = keycmp (s, "(a)")))
			return FALSE;
		type = VBI_LINK_EMAIL;
	} else if ((len = keycmp (s, "www.")) > 0) {
		type = VBI_LINK_HTTP;
		proto = "http://";
	} else if ((len = keycmp (s, "ftp://")) > 0) {
		type = VBI_LINK_FTP;
	} else if (*s == '@' || *s == SECTION_SIGN) {
		type = VBI_LINK_EMAIL;
		len = 1;
	} else {
		return FALSE;
	}

	*end = *start + len;

	{
		const char *s1;
		unsigned int domains;

		s += len;
		s1 = s;
		domains = 0;

		for (;;) {
			/* RFC 1738 */
			static const char *valid = "%&/=?+-~:;@_";
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

		if (0 == domains)
			return FALSE;

		address = s - s1;
	}

	*end += address;

	if (VBI_LINK_EMAIL == type) {
		static const char *valid = "-~._";
		const char *s1;
		unsigned int recipient;

		s = buf + *start;
		s1 = s;

		while (isalnum (s[-1]) || strchr (valid, s[-1]))
			--s;

		recipient = s1 - s;

		if (0 == recipient)
			return FALSE;

		*start -= recipient;

		if (ld) {
			char *url;

			if (!(url = vbi_malloc (recipient + address + 9)))
				return FALSE;

			strcpy (url, "mailto:");
			strlcpy (url + 7, s1 - recipient, recipient);
			url[recipient + 7] = '@';
			strlcpy (url + recipient + 7, s1 + len, address);

			vbi_link_init (ld);

			ld->type = type;
			ld->url = url;
		}
	} else {
		if (ld) {
			char *url;
			unsigned int plen;

			plen = strlen (proto);
			len += address;

			if (!(url = vbi_malloc (plen + len + 1)))
				return FALSE;

			strcpy (url, proto);
			strlcpy (url + plen, buf + *start, len);

			vbi_link_init (ld);

			ld->type = type;
			ld->url = url;
		}
	}

	return TRUE;
}

/**
 * @internal
 * @param tp Current vbi_page.
 * @param row Row to scan.
 *
 * Analyses page contents and adds link flags when keywords appear:
 * http and ftp urls, e-mail addresses, 3-digit page
 * numbers, subpage numbers "n/m", next page ">>".
 */
static void
hyperlinks			(struct ttx_page *	tp,
				 unsigned int		row)
{
	/* One row, two spaces on the sides and NUL. */
	char buffer[43];
	vbi_bool link[43];
	vbi_char *acp;
	unsigned int i;
	unsigned int j;

	acp = tp->pg.text + row * tp->pg.columns;

	j = 0;

	for (i = 0; i < 40; ++i) {
		if (VBI_OVER_TOP == acp[i].size
		    || VBI_OVER_BOTTOM == acp[i].size)
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

		if (keyword (NULL, NULL, buffer,
			     tp->pg.pgno, tp->pg.subno, &i, &end)) {
			for (j = i; j < end; ++j)
				link[j] = TRUE;
		}

		i = end;
	}

	j = 1;

	for (i = 0; i < 40; ++i) {
		if (VBI_OVER_TOP == acp[i].size
		    || VBI_OVER_BOTTOM == acp[i].size) {
			if (i > 0)
				COPY_SET_MASK (acp[i].attr,
					       acp[i - 1].attr, VBI_LINK);
			continue;
		}

		COPY_SET_COND (acp[i].attr, VBI_LINK, link[j++]);
	}
}

/**
 * @param pg With vbi_fetch_vt_page() obtained vbi_page.
 * @param ld Place to store information about the link.
 *   NOTE vbi_link_destroy().
 * @param column Column 0 ... pg->columns - 1 of the character in question.
 * @param row Row 0 ... pg->rows - 1 of the character in question.
 * 
 * A vbi_page (in practice only Teletext pages) may contain hyperlinks
 * such as HTTP URLs, e-mail addresses or links to other pages. Characters
 * being part of a hyperlink have a set vbi_char->link flag, this function
 * returns a more verbose description of the link.
 *
 * @returns
 * TRUE if the link is valid.    NOTE vbi_link_destroy().
 */
vbi_bool
vbi_ttx_page_get_hyperlink	(const vbi_page *	pg,
				 vbi_link *		ld,
				 unsigned int		column,
				 unsigned int		row)
{
	const struct ttx_page *tp;
	char buffer[43];
	const vbi_char *acp;
	unsigned int i;
	unsigned int j;
	unsigned int start;
	unsigned int end;

	TP_CHECK (FALSE);

	assert (NULL != ld);

	if (pg->pgno < 0x100
	    || 0 == row
	    || row >= pg->rows
	    || column >= pg->columns)
		return FALSE;

	acp = pg->text + row * pg->columns;

	if (!(acp[column].attr & VBI_LINK))
		return FALSE;

	if (row == 25) {
		int i;

		i = tp->link_ref[column];

		if (i < 0)
			return FALSE;

		vbi_link_init (ld);

		ld->type	= VBI_LINK_PAGE;
		ld->network	= &tp->cn->network;
		ld->pgno	= tp->link[i].pgno;
		ld->subno	= tp->link[i].subno;

		return TRUE;
	}

	start = 0;
	j = 0;

	for (i = 0; i < 40; ++i) {
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

	return keyword (ld, &tp->cn->network, buffer,
			pg->pgno, pg->subno, &start, &end);
}

#endif

/* Navigation enhancements ------------------------------------------------- */

#if 0

#ifndef TELETEXT_NAV_LOG
#define TELETEXT_NAV_LOG 0
#endif

#define nav_log(templ, args...)						\
do {									\
	if (TELETEXT_NAV_LOG)						\
		fprintf (stderr, templ , ##args);			\
} while (0)

/*
	FLOF navigation
*/

static const vbi_color
flof_link_col [4] = {
	VBI_RED,
	VBI_GREEN,
	VBI_YELLOW,
	VBI_CYAN
};

static vbi_char *
navigation_row			(struct ttx_page *	tp)
{
	return tp->pg.text + 25 * tp->pg.columns;
}

static vbi_char *
clear_navigation_bar		(struct ttx_page *	tp)
{
	vbi_char ac;
	vbi_char *acp;
	unsigned int columns;
	unsigned int i;

	acp = navigation_row (tp);

	CLEAR (ac);

	ac.foreground	= 32 + VBI_WHITE; /* 32: immutable color */
	ac.background	= 32 + VBI_BLACK;
	ac.opacity	= tp->page_opacity[1];
	ac.unicode	= 0x0020;

	columns = tp->pg.columns;

	for (i = 0; i < columns; ++i) {
		acp[i] = ac;
	}

	return acp;
}

/* We have FLOF links but no labels in row 25. This function replaces
   row 25 using the FLOF page numbers as labels. */
static void
flof_navigation_bar		(struct ttx_page *	tp)
{
	vbi_char ac;
	vbi_char *acp;
	unsigned int i;

	acp = clear_navigation_bar (tp);

	ac = *acp;
	ac.attr |= VBI_LINK;

	for (i = 0; i < 4; ++i) {
		unsigned int pos;
		unsigned int k;

		pos = i * 10 + 3;

		ac.foreground = flof_link_col[i];

		for (k = 0; k < 3; ++k) {
			unsigned int digit;

			digit = tp->cp->data.lop.link[i].pgno;
			digit = (digit >> ((2 - k) * 4)) & 15;

			if (digit > 9)
				ac.unicode = digit + 'A' - 9;
			else
				ac.unicode = digit + '0';

			acp[pos + k] = ac;

			tp->link_ref[pos + k] = i;
		}

		tp->link[i].pgno = tp->cp->data.lop.link[i].pgno;
		tp->link[i].subno = tp->cp->data.lop.link[i].subno;
	}
}

/* Adds link flags to a page navigation bar (row 25) from FLOF data. */
static void
flof_links			(struct ttx_page *	tp)
{
	vbi_char *acp;
	unsigned int start;
	unsigned int end;
	unsigned int i;
	int color;

	acp = navigation_row (tp);

	start = 0;
	color = -1;

	for (i = 0; i <= 40; ++i) {
		if (i == 40 || (acp[i].foreground & 7) != color) {
			unsigned int k;
			struct ttx_page_link *pn;

			for (k = 0; k < 4; ++k)
				if (flof_link_col[k] == (unsigned int) color)
					break;

			pn = &tp->cp->data.lop.link[k];

			if (k < 4 && !NO_PAGE (pn->pgno)) {
				/* Leading and trailing spaces not sensitive */

				for (end = i - 1; end >= start
				     && acp[end].unicode == 0x0020; --end)
					;

				for (; end >= start; --end) {
					acp[end].attr |= VBI_LINK;
					tp->link_ref[end] = k;
				}

				tp->link[k].pgno = pn->pgno;
				tp->link[k].subno = pn->subno;
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
	TOP navigation
*/

static void
write_link			(struct ttx_page *	tp,
				 vbi_char *		acp,
				 const char *		s,
				 unsigned int		n,
				 unsigned int		indx,
				 unsigned int		column,
				 vbi_color		foreground)
{
	while (n-- > 0) {
		acp[column].unicode = *s++;
		acp[column].foreground = foreground;
		acp[column].attr |= VBI_LINK;

		tp->link_ref[column] = indx;

		++column;
	}
}

/**
 * @internal
 * @param indx Create tp->nav_link 0 ... 3.
 * @param column Store text in a 12 character wide slot starting at
 *   this column (0 ... 40 - 12).
 * @param pgno Store name of this page.
 * @param foreground Store text using this color.
 * @param ff add 0 ... 2 '>' characters (if space is available).
 *
 * Creates a TOP label for pgno in row 25.
 */
static vbi_bool
top_label			(struct ttx_page *	tp,
				 const vbi_ttx_charset *cs,
				 unsigned int		indx,
				 unsigned int		column,
				 vbi_pgno		pgno,
				 vbi_color		foreground,
				 unsigned int		ff)
{
	const struct ttx_ait_title *ait;
	cache_page *ait_cp;
	vbi_char *acp;
	unsigned int sh;
	int i;

	if (!(ait = cache_network_get_ait_title
	      (tp->cn, &ait_cp, pgno, VBI_ANY_SUBNO)))
		return FALSE;

	acp = navigation_row (tp);

	tp->link[indx].pgno = pgno;
	tp->link[indx].subno = VBI_ANY_SUBNO;

	for (i = 11; i >= 0; --i)
		if (ait->text[i] > 0x20)
			break;

	if (ff > 0 && (i <= (int)(11 - ff))) {
		sh = (11 - ff - i) >> 1;

		acp[sh + i + 1].attr |= VBI_LINK;
		tp->link_ref[column + sh + i + 1] = indx;

		write_link (tp, acp, ">>", ff,
			    indx, column + i + sh + 1,
			    foreground);
	} else {
		sh = (11 - i) >> 1; /* center */
	}

	acp += sh;
	column += sh;

	while (i >= 0) {
		uint8_t c;

		c = MAX (ait->text[i], (uint8_t) 0x20);
		acp[i].unicode = vbi_teletext_unicode (cs->g0, cs->subset, c);
		acp[i].foreground = foreground;
		acp[i].attr |= VBI_LINK;

		tp->link_ref[column + i] = indx;

		--i;
	}

	cache_page_unref (ait_cp);

	return TRUE;
}

/**
 * @internal
 *
 * Replaces row 25 by labels and links from TOP data.
 * Style: Prev-page Next-chapter Next-block Next-page
 */
static void
top_navigation_bar_style_1	(struct ttx_page *	tp)
{
	vbi_pgno pgno;
	vbi_bool have_group;
	const struct ttx_page_stat *ps;
	vbi_char *acp;

	if (TELETEXT_NAV_LOG) {
		ps = cache_network_const_page_stat (tp->cn, tp->cp->pgno);
		nav_log ("page mip/btt: %d\n", ps->page_type);
	}

	clear_navigation_bar (tp);

	if (VBI_OPAQUE != tp->page_opacity[1])
		return;

	pgno = tp->pg.pgno;

	acp = navigation_row (tp);

	tp->link[0].pgno = vbi_add_bcd (pgno, -1);
	tp->link[0].subno = VBI_ANY_SUBNO;

	write_link (tp, acp, "(<<)", 4, 0, 1, 32 + VBI_RED);

	tp->link[3].pgno = vbi_add_bcd (pgno, +1);
	tp->link[3].subno = VBI_ANY_SUBNO;

	write_link (tp, acp, "(>>)", 4, 3, 40 - 5, 32 + VBI_CYAN);

	/* Item 2 & 3, next group and block */

	pgno = tp->pg.pgno;
	have_group = FALSE;

	for (;;) {
		pgno = ((pgno - 0xFF) & 0x7FF) + 0x100;

		if (pgno == tp->cp->pgno)
			break;

		ps = cache_network_const_page_stat (tp->cn, pgno);

		switch (ps->page_type) {
		case VBI_TOP_BLOCK:
			/* XXX should we use char_set of the AIT page? */
			top_label (tp, tp->char_set[0],
				   1, 0 * 14 + 7,
				   pgno, 32 + VBI_GREEN, 0);
			return;

		case VBI_TOP_GROUP:
			if (!have_group) {
				/* XXX as above */
				top_label (tp, tp->char_set[0],
					   2, 1 * 14 + 7,
					   pgno, 32 + VBI_BLUE, 0);
				have_group = TRUE;
			}

			break;
		}
	}
}

/**
 * @internal
 *
 * Replaces row 25 by labels and links from TOP data.
 * Style: Current-block-or-chapter  Next-chapter >  Next-block >>
 */
static void
top_navigation_bar_style_2	(struct ttx_page *	tp)
{
	vbi_pgno pgno;
	vbi_bool have_group;
	const struct ttx_page_stat *ps;

	if (TELETEXT_NAV_LOG) {
		ps = cache_network_const_page_stat (tp->cn, tp->cp->pgno);
		nav_log ("page mip/btt: %d\n", ps->page_type);
	}

	clear_navigation_bar (tp);

	if (VBI_OPAQUE != tp->page_opacity[1])
		return;

	/* Item 1, current block/chapter */

	pgno = tp->pg.pgno;

	do {
		vbi_page_type type;

		ps = cache_network_const_page_stat (tp->cn, pgno);
		type = ps->page_type;

		if (VBI_TOP_BLOCK == type
		    || VBI_TOP_GROUP == type) {
			/* XXX should we use char_set of the AIT page? */
			top_label (tp, tp->char_set[0],
				   0, 0 * 13 + 1,
				   pgno, 32 + VBI_WHITE, 0);
			break;
		}

		pgno = ((pgno - 0x101) & 0x7FF) + 0x100;
	} while (pgno != tp->pg.pgno);

	/* Item 2 & 3, next group and block */

	pgno = tp->pg.pgno;
	have_group = FALSE;

	for (;;) {
		pgno = ((pgno - 0xFF) & 0x7FF) + 0x100;

		if (pgno == tp->cp->pgno)
			break;

		ps = cache_network_const_page_stat (tp->cn, pgno);

		switch (ps->page_type) {
		case VBI_TOP_BLOCK:
			/* XXX should we use char_set of the AIT page? */
			top_label (tp, tp->char_set[0],
				   2, 2 * 13 + 1,
				   pgno, 32 + VBI_YELLOW, 2);
			return;

		case VBI_TOP_GROUP:
			if (!have_group) {
				/* XXX as above */
				top_label (tp, tp->char_set[0],
					   1, 1 * 13 + 1,
					   pgno, 32 + VBI_GREEN, 1);
				have_group = TRUE;
			}

			break;
		}
	}
}

/* Navigation (TOP and FLOF) enhancements. */
static void
navigation			(struct ttx_page *	tp,
				 int			style)
{
	cache_page *cp;

	cp = tp->cp;

	if (cp->data.lop.have_flof) {
		vbi_pgno home_pgno = cp->data.lop.link[5].pgno;

		if (home_pgno >= 0x100 && home_pgno <= 0x899
		    && !NO_PAGE (home_pgno)) {
			tp->link[5].pgno = home_pgno;
			tp->link[5].subno = cp->data.lop.link[5].subno;
		}

		if (cp->lop_packets & (1 << 24)) {
			flof_links (tp);
		} else {
			flof_navigation_bar (tp);
		}
	} else if (tp->cn->have_top) {
		if (2 == style)
			top_navigation_bar_style_2 (tp);
		else
			top_navigation_bar_style_1 (tp);
	}
}

/**
 * @param pg With vbi_fetch_vt_page() obtained vbi_page.
 * @param ld Place to store information about the link.
 * DONT vbi_link_destroy().
 * @param indx Number 0 ... 5 of the link.
 * 
 * When a Teletext vbi_page has been formatted with TOP or FLOF
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
 * FIXME
 * @returns
 * TRUE if the link is valid. DONT vbi_link_destroy().
 */
const vbi_link *
vbi_ttx_page_get_link		(const vbi_page *	pg,
				 unsigned int		indx)
{
	const struct ttx_page *tp;

	TP_CHECK (NULL);

	if (pg->pgno < 0x100
	    || indx >= N_ELEMENTS (tp->link)
	    || tp->link[indx].pgno < 0x100) {
		return NULL;
	}

	return &tp->link[indx];
}

const vbi_link *
vbi_ttx_page_get_home_link	(const vbi_page *	pg)
{
	return vbi_page_get_teletext_link (pg, 5);
}

#endif

/* ------------------------------------------------------------------------ */

/**
 * @internal
 * @param tp Initialized struct ttx_page, results will be stored here.
 * @param cp Source page.
 * @param format_options Array of pairs of a vbi_format_option and value,
 *   terminated by a @c 0 option. See vbi_cache_get_teletext_page().
 *
 * Formats a cached Teletext page and stores results in @a tp.
 *
 * @returns
 * @c FALSE on error.
 */
vbi_bool
_vbi_ttx_page_from_cache_page_va
				(struct ttx_page *	tp,
				 cache_page *		cp,
				 va_list		options)
{
	vbi_bool option_hyperlinks;
	vbi_bool option_pdc_links;
	int option_navigation_style;
	vbi_ttx_charset_code option_cs_default[2];
	const vbi_ttx_charset *option_cs_override[2];
	const struct ttx_extension *ext;
	cache_network *cn;
	unsigned int i;

	assert (NULL != tp);
	assert (NULL != cp);

	if (cp->function != PAGE_FUNCTION_LOP &&
	    cp->function != PAGE_FUNCTION_TRIGGER)
		return FALSE;

	fmt_log ("\nFormatting page %03x/%04x tp=%p\n",
		 cp->pgno, cp->subno, tp);

	/* Source page */

	cn = cp->network;

	assert (NULL != cn);
	assert (NULL != cn->cache);

	tp->cn			= cache_network_ref (cn);
	tp->cp			= cache_page_ref (cp);

	tp->pg.cache		= cn->cache;
	tp->pg.network		= &cn->network;
	tp->pg.pgno		= cp->pgno;
	tp->pg.subno		= cp->subno;

	/* Options */

	tp->max_level		= VBI_TTX_LEVEL_1;

	tp->pg.rows		= 25;
	tp->pg.columns		= 40;
#if 0
	tp->pdc_table		= NULL;
	tp->pdc_table_size	= 0;
#endif
	option_navigation_style	= 0;
	option_hyperlinks	= FALSE;
	option_pdc_links	= FALSE;
	option_cs_default[0]	= 0;
	option_cs_default[1]	= 0;
	option_cs_override[0]	= NULL;
	option_cs_override[1]	= NULL;

	for (;;) {
		vbi_ttx_format_option option;

		option = va_arg (options, vbi_ttx_format_option);

		switch (option) {
		case VBI_HEADER_ONLY:
			tp->pg.rows =
				va_arg (options, vbi_bool) ? 1 : 25;
			break;

		case VBI_COLUMN_41:
			tp->pg.columns =
				va_arg (options, vbi_bool) ? 41 : 40;
			break;

		case VBI_PANELS:
			/* TODO */
			(void) va_arg (options, vbi_bool);
			break;

		case VBI_NAVIGATION:
			option_navigation_style =
				va_arg (options, int);
			break;

		case VBI_HYPERLINKS:
			option_hyperlinks = va_arg (options, vbi_bool);
			break;

		case VBI_PDC_LINKS:
			option_pdc_links = va_arg (options, vbi_bool);
			break;

		case VBI_TTX_LEVEL:
			tp->max_level =
				va_arg (options, vbi_ttx_level);
			break;

		case VBI_DEFAULT_CHARSET_0:
			option_cs_default[0] = va_arg (options,
						       vbi_ttx_charset_code);
			break;

		case VBI_DEFAULT_CHARSET_1:
			option_cs_default[1] = va_arg (options,
						       vbi_ttx_charset_code);
			break;

		case VBI_OVERRIDE_CHARSET_0:
			option_cs_override[0] = vbi_ttx_charset_from_code
				(va_arg (options, vbi_ttx_charset_code));
			break;

		case VBI_OVERRIDE_CHARSET_1:
			option_cs_override[1] = vbi_ttx_charset_from_code
				(va_arg (options, vbi_ttx_charset_code));
			break;

		default:
			option = 0;
			break;
		}

		if (0 == option)
			break;
	}

	/* Magazine and extension defaults */

	if (tp->max_level <= VBI_TTX_LEVEL_1p5)
		tp->mag = _vbi_ttx_default_magazine ();
	else
		tp->mag = cache_network_const_magazine (cn, cp->pgno);

	if (cp->x28_designations & 0x11)
		tp->ext = &cp->data.ext_lop.ext;
	else
		tp->ext = &tp->mag->extension;

	/* Colors */

	init_border_color (&tp->pg, cp->flags, tp->ext->def_border_color);

	ext = &tp->mag->extension;

	COPY (tp->pg.color_map, ext->color_map);
	COPY (tp->pg.drcs_clut, ext->drcs_clut);

	if (tp->max_level >= VBI_TTX_LEVEL_2p5
	    && (cp->x28_designations & 0x13)) {
		ext = &cp->data.ext_lop.ext;

		if (ext->designations & (1 << 4)) /* X/28/4 -> CLUT 0 & 1 */
			memcpy (tp->pg.color_map + 0, ext->color_map + 0,
				16 * sizeof (*tp->pg.color_map));

		if (ext->designations & (1 << 1)) /* X/28/1 -> drcs_clut */
			COPY (tp->pg.drcs_clut, ext->drcs_clut);

		if (ext->designations & (1 << 0)) /* X/28/0 -> CLUT 2 & 3 */
			memcpy (tp->pg.color_map + 16, ext->color_map + 16,
				16 * sizeof (*tp->pg.color_map));
	}

	/* Opacity */

	tp->page_opacity[1] = VBI_OPAQUE;
	tp->boxed_opacity[1] = VBI_TRANSLUCENT;

	if (cp->flags & (C5_NEWSFLASH | C6_SUBTITLE | C10_INHIBIT_DISPLAY))
		tp->page_opacity[1] = VBI_TRANSPARENT_SPACE;

	if (cp->flags & C10_INHIBIT_DISPLAY)
		tp->boxed_opacity[1] =	VBI_TRANSPARENT_SPACE;

	if (cp->flags & C7_SUPPRESS_HEADER) {
		tp->page_opacity[0] = VBI_TRANSPARENT_SPACE;
		tp->boxed_opacity[0] = VBI_TRANSPARENT_SPACE;
	} else {
		tp->page_opacity[0] = tp->page_opacity[1];
		tp->boxed_opacity[0] = tp->boxed_opacity[1];
	}

	/* Character set designation */

	_vbi_ttx_charset_init (tp->char_set,
				 option_cs_default[0],
				 option_cs_default[1],
				 tp->ext, cp);

	if (option_cs_override[0])
		tp->char_set[0] = option_cs_override[0];
	if (option_cs_override[1])
		tp->char_set[1] = option_cs_override[1];

	/* Format level one page */

	level_one_page (tp);

	/* PDC Preselection method "A" links */

#if 0
	if (tp->pg.rows > 1 && option_pdc_links) {
		tp->pdc_table = _vbi_preselection_array_new (25);

		if (tp->pdc_table) {
			tp->pdc_table_size =
				_vbi_pdc_method_a (tp->pdc_table, 25,
						    tp->pg.network,
						    tp->pg.pgno,
						    cp->data.lop.raw);
		}
	}
#endif

	/* Local enhancement data and objects */

	if (tp->max_level >= VBI_TTX_LEVEL_1p5) {
		vbi_page page;
		vbi_bool success;

		page = tp->pg;

		CLEAR (tp->drcs_cp);

		if (!(cp->flags & (C5_NEWSFLASH | C6_SUBTITLE))) {
			tp->boxed_opacity[0] = VBI_TRANSPARENT_SPACE;
			tp->boxed_opacity[1] = VBI_TRANSPARENT_SPACE;
		}

		fmt_log ("enhancement packets %08x\n",
			 cp->x26_designations);

		if (cp->x26_designations & 1) {
			success = enhance
				(tp, LOCAL_ENHANCEMENT_DATA,
				 cp->data.enh_lop.enh,
				 N_ELEMENTS (cp->data.enh_lop.enh),
				 0, 0);
		} else {
			success = default_object_invocation (tp);
		}

		if (success) {
			if (tp->max_level >= VBI_TTX_LEVEL_2p5)
				post_enhance (tp);
		} else {
			unsigned int i;

			/* Roll back incomplete enhancements. */

			for (i = 0; i < N_ELEMENTS (tp->drcs_cp); ++i) {
				cache_page_unref (tp->drcs_cp[i]);
				tp->drcs_cp[i] = NULL;
			}

			tp->pg = page;
		}
#if 0
	} else if (option_pdc_links
		   && tp->pdc_table
		   && 0 == tp->pdc_table_size) {
		/* Just PDC preselection from packets X/26. */

		if (cp->x26_designations & 1) {
			enhance	(tp, LOCAL_ENHANCEMENT_DATA,
				 cp->data.enh_lop.enh,
				 N_ELEMENTS (cp->data.enh_lop.enh),
				 0, 0);
		}
#endif
	}

#if 0
	for (i = 0; i < N_ELEMENTS (tp->link); ++i) {
		vbi_link *ld;

		ld = &tp->link[i];

		ld->type	= VBI_LINK_PAGE;
		ld->name	= NULL;
		ld->pgno	= 0;

		/* No allocated copy for efficency. */
		ld->network	= &tp->cn->network;
	}

	tp->link[5].pgno = cn->initial_page.pgno;
	tp->link[5].subno = cn->initial_page.subno;

	/* -1 no link. */
	memset (tp->link_ref, -1, sizeof (tp->link_ref));

	if (tp->pg.rows > 1) {
		unsigned int row;

		if (option_hyperlinks)
			for (row = 1; row < 25; ++row)
				hyperlinks (tp, row);

		if (option_navigation_style > 0)
			navigation (tp, option_navigation_style);

		if (tp->pdc_table_size > 0)
			pdc_post_proc (&tp->pg,
				       tp->pdc_table,
				       tp->pdc_table + tp->pdc_table_size);
	}
#endif

	if (41 == tp->pg.columns)
		column_41 (tp);

	if (0)
		_vbi_ttx_page_dump (tp, stderr, 2);

	return TRUE;
}

/* ------------------------------------------------------------------------ */

/**
 */
static struct ttx_page *
ttx_page_dup			(const struct ttx_page *tp)
{
	struct ttx_page *new_tp;
	unsigned int i;

	new_tp = vbi_malloc (sizeof (*new_tp));
	if (NULL == new_tp) {
		return NULL;
	}

	*new_tp = *tp;

	new_tp->pg.ref_count = 1;

#if 0
	if (NULL != new_tp->pdc_table) {
		new_tp->pdc_table =
			_vbi_preselection_array_dup (new_tp->pdc_table,
						     new_tp->pdc_table_size);
		if (NULL == new_tp->pdc_table) {
			vbi_free (new_tp);
			errno = ENOMEM;
			return NULL;
		}
	}
#endif

	if (NULL != new_tp->pg.cache) {
		if (NULL != new_tp->cn)
			cache_network_ref (new_tp->cn);

		if (NULL != new_tp->cp)
			cache_page_ref (new_tp->cp);

		for (i = 0; i < N_ELEMENTS (new_tp->drcs_cp); ++i) {
			if (NULL != new_tp->drcs_cp[i])
				cache_page_ref (new_tp->drcs_cp[i]);
		}
	}

	/* Should vbi_link_copy() tp->link[i] here, but we use no
	   allocated fields except network, which for efficiency just
	   points at our ref counted tp->cn. */

	return new_tp;
}

/**
 */
vbi_page *
vbi_page_dup			(const vbi_page *	pg)
{
	vbi_page *new_pg;

	_vbi_null_check (pg, NULL);

	if ((void *) TTX_PAGE_MAGIC == pg->_private) {
		const struct ttx_page *tp;
		struct ttx_page *new_tp;

		tp = CONST_PARENT (pg, struct ttx_page, pg);

		new_tp = ttx_page_dup (tp);
		if (NULL == new_tp) {
			return NULL;
		}

		new_pg = &new_tp->pg;
	} else {
		new_pg = malloc (sizeof (*new_pg));
		if (NULL == new_pg) {
			return NULL;
		}

		*new_pg = *pg;
	}

	return new_pg;
}

/**
 * @internal
 * @param tp Initialized struct ttx_page.
 *
 * Frees all resources associated with @a tp, except the structure
 * itself. Ignores tp->pg.ref_count.
 */
void
_vbi_ttx_page_destroy		(struct ttx_page *	tp)
{
	unsigned int i;

	assert (NULL != tp);

	if (NULL != tp->pg.cache) {
		for (i = 0; i < N_ELEMENTS (tp->drcs_cp); ++i) {
			cache_page_unref (tp->drcs_cp[i]);
		}

		cache_page_unref (tp->cp);
		cache_network_unref (tp->cn);
	}

#if 0
	_vbi_preselection_array_delete (tp->pdc_table,
					tp->pdc_table_size);
#endif

	CLEAR (*tp);
}

/**
 */
void
_vbi_ttx_page_init		(struct ttx_page *	tp)
{
	assert (NULL != tp);

	CLEAR (*tp);

	tp->pg.ref_count = 1;
	tp->pg._private = (void *) TTX_PAGE_MAGIC;
}

/**
 * Frees all resources associated with @a pg, regardless of
 * any remaining references to it.
 */
void
vbi_page_delete			(vbi_page *		pg)
{
	if (NULL == pg)
		return;

	if ((void *) TTX_PAGE_MAGIC == pg->_private) {
		struct ttx_page *tp;

		tp = PARENT (pg, struct ttx_page, pg);
		_vbi_ttx_page_destroy (tp);
	}

	vbi_free (pg);
}

/**
 * Releases a vbi_page reference. When this is the last reference
 * the function calls vbi_page_delete().
 */
void
vbi_page_unref			(vbi_page *		pg)
{
	if (NULL == pg)
		return;

	if (pg->ref_count > 1) {
		--pg->ref_count;
		return;
	}

	vbi_page_delete (pg);
}

/**
 * Creates a new reference to the page.
 *
 * @returns
 * @a pg. You must call vbi_page_unref() when the reference is
 * no longer needed.
 */
vbi_page *
vbi_page_ref			(vbi_page *		pg)
{
	_vbi_null_check (pg, NULL);

	++pg->ref_count;

	return pg;
}

/**
 * @internal
 * Allocates a new, empty ttx_page.
 *
 * @returns
 * @c NULL when out of memory. The ttx_page must be freed with
 * vbi_page_unref() or vbi_page_delete() when done.
 */
struct ttx_page *
_vbi_ttx_page_new		(void)
{
	struct ttx_page *tp;

	tp = vbi_malloc (sizeof (*tp));
	if (NULL == tp) {
		return NULL;
	}

	_vbi_ttx_page_init (tp);

	return tp;
}

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
