/*
 *  libzvbi - Teletext decoder
 *
 *  Copyright (C) 2000, 2001, 2002, 2003, 2004 Michael H. Schimek
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

/* $Id: ttx_decoder.c,v 1.1.2.2 2008-08-22 07:58:43 mschimek Exp $ */

#include "../site_def.h"

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <assert.h>
#include "hamm.h"		/* vbi_unham functions */
//#include "packet-830.h"	/* vbi_decode_teletext_830x functions */
#include "conv.h"		/* _vbi_strdup_locale_teletext() */
#include "ttx_page_stat.h"
#include "ttx_page-priv.h"
#include "top_title-priv.h"
#include "ttx_decoder-priv.h"

/**
 * @addtogroup Teletext Teletext Decoder
 * @ingroup HiDec
 */

#ifndef TTX_DECODER_CHSW_TEST
#  define TTX_DECODER_CHSW_TEST 0
#endif

#ifndef TTX_DECODER_LOG
#  define TTX_DECODER_LOG 0
#endif

#define log(templ, args...)						\
do {									\
	if (TTX_DECODER_LOG)					\
		fprintf (stderr, templ , ##args);			\
} while (0)


#define MAG8(mag0) ((mag0) ? (mag0) : 8)

/* EN 300 706 Table 30 Color Map */

static const vbi_rgba
default_color_map [40] = {
	VBI_RGBA (0x00, 0x00, 0x00), VBI_RGBA (0xFF, 0x00, 0x00),
	VBI_RGBA (0x00, 0xFF, 0x00), VBI_RGBA (0xFF, 0xFF, 0x00),
	VBI_RGBA (0x00, 0x00, 0xFF), VBI_RGBA (0xFF, 0x00, 0xFF),
	VBI_RGBA (0x00, 0xFF, 0xFF), VBI_RGBA (0xFF, 0xFF, 0xFF),
	VBI_RGBA (0x00, 0x00, 0x00), VBI_RGBA (0x77, 0x00, 0x00),
	VBI_RGBA (0x00, 0x77, 0x00), VBI_RGBA (0x77, 0x77, 0x00),
	VBI_RGBA (0x00, 0x00, 0x77), VBI_RGBA (0x77, 0x00, 0x77),
	VBI_RGBA (0x00, 0x77, 0x77), VBI_RGBA (0x77, 0x77, 0x77),
	VBI_RGBA (0xFF, 0x00, 0x55), VBI_RGBA (0xFF, 0x77, 0x00),
	VBI_RGBA (0x00, 0xFF, 0x77), VBI_RGBA (0xFF, 0xFF, 0xBB),
	VBI_RGBA (0x00, 0xCC, 0xAA), VBI_RGBA (0x55, 0x00, 0x00),
	VBI_RGBA (0x66, 0x55, 0x22), VBI_RGBA (0xCC, 0x77, 0x77),
	VBI_RGBA (0x33, 0x33, 0x33), VBI_RGBA (0xFF, 0x77, 0x77),
	VBI_RGBA (0x77, 0xFF, 0x77), VBI_RGBA (0xFF, 0xFF, 0x77),
	VBI_RGBA (0x77, 0x77, 0xFF), VBI_RGBA (0xFF, 0x77, 0xFF),
	VBI_RGBA (0x77, 0xFF, 0xFF), VBI_RGBA (0xDD, 0xDD, 0xDD),

	/* Private, immutable colors for the navigation bar. */

	VBI_RGBA (0x00, 0x00, 0x00), VBI_RGBA (0xFF, 0x00, 0x00),
	VBI_RGBA (0x00, 0xFF, 0x00), VBI_RGBA (0xFF, 0xFF, 0x00),
	VBI_RGBA (0x00, 0x00, 0xFF), VBI_RGBA (0xFF, 0x00, 0xFF),
	VBI_RGBA (0x00, 0xFF, 0xFF), VBI_RGBA (0xFF, 0xFF, 0xFF),
};

/** @internal */
const char *
_vbi_ttx_page_function_name	(enum ttx_page_function	function)
{
#undef CASE
#define CASE(function) case PAGE_FUNCTION_##function : return #function ;
	switch (function) {
	CASE (ACI)
	CASE (EPG)
	CASE (DISCARD)
	CASE (UNKNOWN)
	CASE (LOP)
	CASE (DATA)
	CASE (GPOP)
	CASE (POP)
	CASE (GDRCS)
	CASE (DRCS)
	CASE (MOT)
	CASE (MIP)
	CASE (BTT)
	CASE (AIT)
	CASE (MPT)
	CASE (MPT_EX)
	CASE (TRIGGER)
	}

	return NULL;
}

/** @internal */
const char *
_vbi_ttx_page_coding_name	(enum ttx_page_coding	coding)
{
#undef CASE
#define CASE(coding) case PAGE_CODING_##coding : return #coding ;
	switch (coding) {
	CASE (UNKNOWN)
	CASE (ODD_PARITY)
	CASE (UBYTES)
	CASE (TRIPLETS)
	CASE (HAMMING84)
	CASE (AIT)
	CASE (META84)
	}

	return NULL;
}

/** @internal */
const char *
_vbi_ttx_object_type_name	(enum ttx_object_type	type)
{
	switch (type) {
	case OBJECT_TYPE_NONE:		return "NONE/LOCAL_ENH";
	case OBJECT_TYPE_ACTIVE:	return "ACTIVE";
	case OBJECT_TYPE_ADAPTIVE:	return "ADAPTIVE";
	case OBJECT_TYPE_PASSIVE:	return "PASSIVE";
	}

	return NULL;
}

/** @internal */
const char *
_vbi_ttx_drcs_mode_name		(enum ttx_drcs_mode	mode)
{
#undef CASE
#define CASE(mode) case DRCS_MODE_##mode : return #mode ;
	switch (mode) {
	CASE (12_10_1)
	CASE (12_10_2)
	CASE (12_10_4)
	CASE (6_5_4)
	CASE (SUBSEQUENT_PTU)
	CASE (NO_DATA)
	}

	return NULL;
}

/**
 * @param type Teletext page type.
 *
 * Returns the name of a Teletext page type, for example
 * VBI_SUBTITLE_PAGE -> "SUBTITLE_PAGE". This is mainly intended for
 * debugging.
 * 
 * @returns
 * Static ASCII string, NULL if @a type is invalid.
 */
const char *
_vbi_ttx_page_type_name		(vbi_ttx_page_type	type)
{
#undef CASE
#define CASE(type) case VBI_##type : return #type ;
	switch (type) {
	CASE (NO_PAGE)
	CASE (NORMAL_PAGE)
	CASE (SUBTITLE_PAGE)
	CASE (SUBTITLE_INDEX)
	CASE (CLOCK_PAGE)
	CASE (PROGR_WARNING)
	CASE (CURRENT_PROGR)
	CASE (NOW_AND_NEXT)
	CASE (PROGR_INDEX)
	CASE (PROGR_SCHEDULE)
	CASE (UNKNOWN_PAGE)
	CASE (NOT_PUBLIC)
	CASE (CA_DATA)
	CASE (PFC_EPG_DATA)
	CASE (PFC_DATA)
	CASE (DRCS_DATA)
	CASE (POP_DATA)
	CASE (SYSTEM_DATA)
	CASE (KEYWORD_SEARCH_DATA)
	CASE (TOP_BLOCK)
	CASE (TOP_GROUP)
	CASE (NEWSFLASH_PAGE)
	CASE (TRIGGER_DATA)
	CASE (ACI_DATA)
	CASE (TOP_DATA)
	}

	return NULL;
}

#undef CASE

/** @internal */
void
_vbi_ttx_page_link_dump		(const struct ttx_page_link *pl,
				 FILE *			fp)
{
	fprintf (fp, "%s %3x.%04x",
		 _vbi_ttx_page_function_name (pl->function),
		 pl->pgno, pl->subno);
}

static void
cache_page_raw_dump		(const cache_page *	cp,
				 FILE *			fp,
				 enum ttx_page_coding	coding)
{
	unsigned int i;
	unsigned int j;

	fprintf (fp, "Page %03x.%04x\n", cp->pgno, cp->subno);

	for (j = 0; j < 25; ++j) {
		switch (coding) {
		case PAGE_CODING_TRIPLETS:
			for (i = 0; i < 13; ++i) {
				const uint8_t *p;

				p = &cp->data.lop.raw[j][1 + i * 3];
				fprintf (fp, "%05x ", vbi_unham24p (p));
			}

			break;

		case PAGE_CODING_HAMMING84:
			for (i = 0; i < 40; ++i) {
				unsigned int c;

				c = cp->data.lop.raw[j][i];
				fprintf (fp, "%x", vbi_unham8 (c));
			}

			break;

		default:
			for (i = 0; i < 40; ++i) {
				unsigned int c;

				c = cp->data.lop.raw[j][i];
				fprintf (fp, "%02x ", c);
			}

			break;
		}

		for (i = 0; i < 40; ++i)
			fputc (_vbi_to_ascii (cp->data.lop.raw[j][i]), fp);

		fputc ('\n', fp);
	}
}

/** @internal */
void
_vbi_ttx_extension_dump		(const struct ttx_extension *ext,
				 FILE *			fp)
{
	unsigned int i;

	fprintf (fp, "Extension:\n"
		 "  designations %08x\n"
		 "  charset=%u,%u def_screen_color=%u row_color=%u\n"
		 "  bbg_subst=%u panel=%u,%u clut=%u,%u\n"
		 "  12x10x2 global dclut=",
		 ext->designations,
		 ext->charset_code[0],
		 ext->charset_code[1],
		 ext->def_border_color,
		 ext->def_row_color,
		 ext->fallback.black_bg_substitution,
		 ext->fallback.left_panel_columns,
		 ext->fallback.right_panel_columns,
		 ext->foreground_clut,
		 ext->background_clut);

	for (i = 0; i < 4; ++i)
		fprintf (fp, "%u, ", ext->drcs_clut[i + 2]);

	fputs ("\n  12x10x2 dclut=", fp);

	for (i = 0; i < 4; ++i)
		fprintf (fp, "%u, ", ext->drcs_clut[i + 6]);

	fputs ("\n  12x10x4 global dclut=", fp);

	for (i = 0; i < 16; ++i)
		fprintf (fp, "%u, ", ext->drcs_clut[i + 10]);

	fputs ("\n  12x10x4 dclut=", fp);

	for (i = 0; i < 16; ++i)
		fprintf (fp, "%u, ", ext->drcs_clut[i + 26]);

	fputs ("\n  color_map=\n", fp);

	for (i = 0; i < 40; ++i) {
		fprintf (fp, "%08x, ", ext->color_map[i]);

		if ((i % 8) == 7)
			fputc ('\n', fp);
	}

	fputc ('\n', fp);
}

static vbi_bool
unham_page_link			(struct ttx_page_link *	pl,
				 const uint8_t		buffer[6],
				 unsigned int		mag0)
{
	int b1, b2, b3;

	b1 = vbi_unham16p (buffer + 0);
	b2 = vbi_unham16p (buffer + 2);
	b3 = vbi_unham16p (buffer + 4);

	if ((b1 | b2 | b3) < 0)
		return FALSE;

	mag0 ^= ((b3 >> 5) & 6) + (b2 >> 7);

	pl->function	= PAGE_FUNCTION_UNKNOWN;
	pl->pgno	= MAG8 (mag0) * 256 + b1;
	pl->subno	= (b3 * 256 + b2) & 0x3F7F;

	/* subno flags? */

	return TRUE;
}

static void
clear_lop			(cache_page *		cp)
{
	memset (cp->data.lop.raw, 0x20, sizeof (cp->data.lop.raw));

	/* NO_PAGE (pgno): (pgno & 0xFF) == 0xFF. */
	memset (cp->data.lop.link, -1, sizeof (cp->data.lop.link));

	cp->data.lop.have_fastext = FALSE;
}

static void
clear_enhancement		(cache_page *		cp)
{
	/* Valid range of mode 0x00 ... 0x1F, broken -1. */
	memset (cp->data.enh_lop.enh, -1, sizeof (cp->data.enh_lop.enh));
}

/*
	10.5 (Global) Public Object Page
*/

static void
clear_pop_page			(cache_page *		cp)
{
	/* Valid range 0 ... 506 (39 packets * 13 triplets),
	   unused pointers 511 (10.5.1.2), broken -1. */
	memset (cp->data.pop.pointer, -1, sizeof (cp->data.pop.pointer));

	/* Valid range of mode 0x00 ... 0x1F, broken -1. */
	memset (cp->data.pop.triplet, -1, sizeof (cp->data.pop.triplet));
}

static vbi_bool
decode_pop_packet		(cache_page *		cp,
				 const uint8_t		buffer[40],
				 unsigned int		packet)
{
	int n18[13];
	int designation;
	int err;
	unsigned int i;

	designation = vbi_unham8 (buffer[0]);

	err = 0;

	for (i = 0; i < 13; ++i)
		err |= n18[i] = vbi_unham24p (buffer + 1 + i * 3);

	if (TTX_DECODER_LOG) {
		log ("POP page %x.%x flags %x packet %u designation %d\n",
		     cp->pgno, cp->subno, cp->flags, packet, designation);

		for (i = 1; i < 13; ++i)
			log ("... %u: %d %x %u %u\n",
			     i, n18[i], n18[i],
			     n18[i] & 0x1FF, (n18[i] >> 9) & 0x1FF);
	}

	if ((designation | err) < 0) {
		return FALSE;
	}

	if (26 == packet)
		packet += designation;

	switch (packet) {
	case 1 ... 2:
		/* 10.5.1.2: Must be pointer table. */
		if (0 && !(designation & 1)) {
			return FALSE;
		}

		/* fall through */

	case 3 ... 4:
		if (designation & 1) {
			unsigned int index;

			/* Pointer table. */

			index = (packet - 1) * 12 * 2;

			/* 10.5.1.2: triplet[0] is reserved. */
			for (i = 1; i < 13; ++i) {
				cp->data.pop.pointer[index + 0] =
					n18[i] & 0x1FF;
				cp->data.pop.pointer[index + 1] =
					n18[i] >> 9;

				log ("PT %2u %2u (%2u): %3u %3u\n",
				     packet, i, index,
				     n18[i] & 0x1FF,
				     n18[i] >> 9);
				
				index += 2;
			}

			return TRUE;
		}

		/* fall through */

	case 5 ... 42:
	{
		struct ttx_triplet *trip;

		trip = &cp->data.pop.triplet[(packet - 3) * 13];

		for (i = 0; i < 13; ++i) {
			trip->address	= (n18[i] >> 0) & 0x3F;
			trip->mode	= (n18[i] >> 6) & 0x1F;
			trip->data	= (n18[i] >> 11);
			
			++trip;
		}

		return TRUE;
	}

	default:
		assert (0);
	}

	return FALSE;
}

static vbi_bool
convert_pop_page		(cache_page *		dst,
				 const cache_page *	src,
				 enum ttx_page_function	function)
{
	unsigned int packet;
	const uint8_t *raw;

	assert (dst != src);

	clear_pop_page (dst);

	dst->function = function;

	raw = src->data.unknown.raw[1];

	for (packet = 1; packet <= 25; ++packet) {
		if (src->lop_packets & (1 << packet))
			if (!decode_pop_packet (dst, raw, packet))
				return FALSE;

		raw += 40;
	}

	assert ((39 * 13 + 1) == N_ELEMENTS (dst->data.pop.triplet));
	assert ((16 * 13 + 1) == N_ELEMENTS (src->data.enh_lop.enh));

	if (src->x26_designations)
		memcpy (dst->data.pop.triplet + 23 * 13,
			src->data.enh_lop.enh,
			sizeof (src->data.enh_lop.enh));

	return TRUE;
}

/*
	10.6 Magazine Organization Table
 */

static void
decode_mot_page_lut		(struct ttx_magazine *	mag,
				 const uint8_t		buffer[2],
				 vbi_pgno		pgno)
{
	int n0, n1;

	n0 = vbi_unham8 (buffer[0]);
	n1 = vbi_unham8 (buffer[1]);

	if ((n0 | n1) < 0)
		return;

	/* n0 & 8 "global objects required" ignored. */
	/* n1 & 8 "global drcs required" ignored. */

	mag->pop_lut[pgno] = n0 & 7;
	mag->drcs_lut[pgno] = n1 & 7;
}

static void
decode_mot_page_pop		(vbi_ttx_decoder *	td,
				 struct ttx_pop_link	pop[4],
				 const uint8_t		buffer[40])
{
	unsigned int i;

	for (i = 0; i < 4; ++i) {
		int n4[10];
		int err;
		unsigned int j;
		struct ttx_page_stat *ps;

		err = 0;

		for (j = 0; j < 10; ++j)
			err |= n4[j] = vbi_unham8 (buffer[j]);

		if (err < 0)
			continue;

		buffer += 10;

		/* n4[0] & 8 "link page number may be superseded
		   by X/27/4 or X/27/5 data" ignored. */

		pop->pgno = MAG8 (n4[0] & 7) * 256 + n4[1] * 16 + n4[2];

		ps = cache_network_page_stat (td->network, pop->pgno);
		ps->page_type = VBI_SYSTEM_DATA;
		ps->subcode = n4[3]; /* highest S1 transmitted */

		if (n4[4] & 1) {
			/* No side-panels, no black background
			   colour substitution. */
			CLEAR (pop->fallback);
		} else {
			unsigned int s = (n4[4] >> 1) & 3;

			pop->fallback.black_bg_substitution = n4[4] >> 3;

			/* s: 0+0, 16+0, 0+16, 8+8 */
			pop->fallback.left_panel_columns = "\00\20\00\10"[s];
			pop->fallback.right_panel_columns = "\00\00\20\10"[s];
		}

		pop->default_obj[0].type = (enum ttx_object_type)(n4[5] & 3);
		pop->default_obj[0].address = (n4[7] << 4) + n4[6];

		pop->default_obj[1].type = (enum ttx_object_type)(n4[5] >> 2);
		pop->default_obj[1].address = (n4[9] << 4) + n4[8];

		++pop;
	}
}

static void
decode_mot_page_drcs		(vbi_ttx_decoder *	td,
				 vbi_pgno		pgno[8],
				 const uint8_t		buffer[40])
{
	unsigned int i;

	for (i = 0; i < 8; ++i) {
		int n4[4];
		int err;
		unsigned int j;
		struct ttx_page_stat *ps;

		err = 0;

		for (j = 0; j < 4; ++j)
			err |= n4[j] = vbi_unham8 (buffer[j]);

		if (err < 0)
			continue;

		buffer += 4;

		/* n4[0] & 8 "link page number may be superseded
		   by X/27/4 or X/27/5 data" ignored. */

		pgno[i] = MAG8 (n4[0] & 7) * 256 + n4[1] * 16 + n4[2];

		ps = cache_network_page_stat (td->network, pgno[i]);

		ps->page_type = VBI_SYSTEM_DATA;
		ps->subcode = n4[3]; /* highest S1 transmitted */
	}
}

static void
decode_mot_page			(vbi_ttx_decoder *	td,
				 const cache_page *	cp)
{
	struct ttx_magazine *mag;
	const uint8_t *raw;
	unsigned int packet;
	vbi_pgno pgno;

	mag = cache_network_magazine (td->network, cp->pgno);

	raw = cp->data.unknown.raw[1];

	pgno = 0;

	for (packet = 1; packet <= 8; ++packet) {
		if (cp->lop_packets & (1 << packet)) {
			vbi_pgno i;

			for (i = 0x00; i <= 0x09; raw += 2, ++i)
				decode_mot_page_lut (mag, raw, pgno + i);

			for (i = 0x10; i <= 0x19; raw += 2, ++i)
				decode_mot_page_lut (mag, raw, pgno + i);
		} else {
			raw += 40;
		}

		pgno += 0x20;
	}

	pgno = 0;

	for (packet = 9; packet <= 14; ++packet) {
		if (cp->lop_packets & (1 << packet)) {
			vbi_pgno i;

			for (i = 0x0A; i <= 0x0F; raw += 2, ++i)
				decode_mot_page_lut (mag, raw, pgno + i);

			if (14 == packet) /* 0xFA ... 0xFF, rest unused */
				break;

			for (i = 0x1A; i <= 0x1F; raw += 2, ++i)
				decode_mot_page_lut (mag, raw, pgno + i);

			for (i = 0x2A; i <= 0x2F; raw += 2, ++i)
				decode_mot_page_lut (mag, raw, pgno + i);

			raw += 40 - 3 * 6 * 2;
		} else {
			raw += 40;
		}

		pgno += 0x30;
	}

	/* Level 2.5 POP links. */

	for (packet = 19; packet <= 20; ++packet)
		if (cp->lop_packets & (1 << packet))
			decode_mot_page_pop (td, mag->pop_link[0]
					     + (packet - 19) * 4,
					     cp->data.unknown.raw[packet]);

	/* Level 2.5 DRCS links. */

	if (cp->lop_packets & (1 << 21))
		decode_mot_page_drcs (td, mag->drcs_link[0],
				      cp->data.unknown.raw[21]);

	/* Level 3.5 POP links. */

	for (packet = 22; packet <= 23; ++packet)
		if (cp->lop_packets & (1 << packet))
			decode_mot_page_pop (td, mag->pop_link[1]
					     + (packet - 22) * 4,
					     cp->data.unknown.raw[packet]);

	/* Level 3.5 DRCS links. */

	if (cp->lop_packets & (1 << 24))
		decode_mot_page_drcs (td, mag->drcs_link[1],
				      cp->data.unknown.raw[24]);
}

/*
	11.2 Table Of Pages navigation
*/

static vbi_bool
unham_top_page_number			(struct ttx_page_link *	pl,
				 const uint8_t		buffer[8])
{
	int n4[8];
	int err;
	unsigned int i;
	vbi_pgno pgno;
	vbi_subno subno;

	err = 0;

	for (i = 0; i < 8; ++i)
		err |= n4[i] = vbi_unham8 (buffer[i]);

	pgno = n4[0] * 256 + n4[1] * 16 + n4[2];

	if (err < 0
	    || pgno < 0x100
	    || pgno > 0x8FF)
		return FALSE;

	subno = (n4[3] << 12) | (n4[4] << 8) | (n4[5] << 4) | n4[6];

	switch ((enum ttx_top_page_function) n4[7]) {
	case TOP_PAGE_FUNCTION_AIT:
		pl->function = PAGE_FUNCTION_AIT;
		break;

	case TOP_PAGE_FUNCTION_MPT:
		pl->function = PAGE_FUNCTION_MPT;
		break;

	case TOP_PAGE_FUNCTION_MPT_EX:
		pl->function = PAGE_FUNCTION_MPT_EX;
		break;

	default:
		pl->function = PAGE_FUNCTION_UNKNOWN;
		break;
	}

	pl->pgno = pgno;
	pl->subno = subno & 0x3F7F; /* flags? */

	return TRUE;
}

/* 11.2 Basic TOP Table */

static vbi_bool
top_page_stat			(cache_network *	cn,
				 vbi_pgno		pgno,
				 enum ttx_btt_page_type	btt_type)
{
	struct ttx_page_stat *ps;
	vbi_ttx_page_type page_type;
	unsigned int subcode;
	vbi_bool changed;

	subcode = SUBCODE_SINGLE_PAGE;

	switch (btt_type) {
	case BTT_NO_PAGE:
		page_type = VBI_NO_PAGE;
		subcode = SUBCODE_UNKNOWN;
		break;

		/* Observation: BTT_SUBTITLE only when the page is
		   transmitted, otherwise BTT_NO_PAGE. */
	case BTT_SUBTITLE:
		page_type = VBI_SUBTITLE_PAGE;
		break;

	case BTT_PROGR_INDEX_M:
		subcode = SUBCODE_MULTI_PAGE;
		/* fall through */

	case BTT_PROGR_INDEX_S:
		/* Usually _SCHEDULE, not _INDEX. */
		page_type = VBI_PROGR_SCHEDULE;
		break;

	case BTT_BLOCK_M:
		subcode = SUBCODE_MULTI_PAGE;
		/* fall through */

	case BTT_BLOCK_S:
		page_type = VBI_TOP_BLOCK;
		break;

	case BTT_GROUP_M:
		subcode = SUBCODE_MULTI_PAGE;
		/* fall through */

	case BTT_GROUP_S:
		page_type = VBI_TOP_GROUP;
		break;

	case BTT_NORMAL_M:
	case BTT_NORMAL_11:	/* ? */
		subcode = SUBCODE_MULTI_PAGE;
		/* fall through */

	case BTT_NORMAL_S:
	case BTT_NORMAL_9:	/* ? */
		page_type = VBI_NORMAL_PAGE;
		break;

	default:
		page_type = VBI_UNKNOWN_PAGE;
		subcode = SUBCODE_UNKNOWN;
		break;
	}

	log ("BTT %04x: %2u %04x %s\n",
	     pgno, btt_type, subcode,
	     _vbi_ttx_page_type_name (page_type));

	ps = cache_network_page_stat (cn, pgno);

	changed = FALSE;

	if (ps->page_type != page_type
	    && (VBI_UNKNOWN_PAGE == ps->page_type
		|| VBI_SUBTITLE_PAGE == ps->page_type
		|| VBI_SUBTITLE_PAGE == page_type)) {
		ps->page_type = page_type;
		changed = TRUE;
	}

	/* We only ever increase the subcode, such that the table
	   is consistent when BTT, MIP and the number of received
	   subpages disagree. SUBCODE_MULTI_PAGE means the page
	   has subpages, how many is not known (yet).

	   Interestingly there doesn't seem to be a btt_type for
	   clock pages, I saw only BTT_NORMAL_S. */
	if (SUBCODE_UNKNOWN == ps->subcode
	    || (SUBCODE_SINGLE_PAGE == ps->subcode
		&& SUBCODE_MULTI_PAGE == subcode)) {
		ps->subcode = subcode;
	}

	return changed;
}

static vbi_bool
top_link_stat			(cache_network *	cn,
				 unsigned int		link)
{
	const struct ttx_page_link *pl;
	struct ttx_page_stat *ps;

	cn->have_top = TRUE;

	pl = &cn->btt_link[link];

	switch (pl->function) {
	case PAGE_FUNCTION_MPT:
	case PAGE_FUNCTION_AIT:
	case PAGE_FUNCTION_MPT_EX:
		if (TTX_DECODER_LOG) {
			log ("BTT %2u: ", link);
			_vbi_ttx_page_link_dump (pl, stderr);
			fputc ('\n', stderr);
		}

		ps = cache_network_page_stat (cn, pl->pgno);
		ps->page_type = VBI_TOP_DATA;

		return TRUE;

	default:
		return FALSE;
	}
}

static void
decode_btt_page			(vbi_ttx_decoder *	td,
				 const cache_page *	cp)
{
	unsigned int packet;
	const uint8_t *raw;
	cache_network *cn;
	vbi_bool changed;
	vbi_pgno pgno;

	raw = cp->data.unknown.raw[1];

	changed = FALSE;

	pgno = 0x100;

	for (packet = 1; packet <= 20; ++packet) {
		unsigned int i;

		if (cp->lop_packets & (1 << packet)) {
			for (i = 0; i < 40; ++i) {
				enum ttx_btt_page_type btt_type;

				btt_type = vbi_unham8 (raw[i]);

				if ((int) btt_type < 0) {
					pgno = vbi_add_bcd (pgno, 1);
					continue;
				}

				changed |= top_page_stat (td->network,
							  pgno, btt_type);

				pgno = vbi_add_bcd (pgno, 1);
			}
		} else {
			pgno = vbi_add_bcd (pgno, 0x40);
		}

		raw += 40;
	}

	/* Links to other TOP pages. */

	cn = td->network;

	for (packet = 21; packet <= 22; ++packet) {
		if (cp->lop_packets & (1 << packet)) {
			unsigned int l;
			unsigned int i;

			l = (packet - 21) * 5;

			for (i = 0; i < 5; ++i) {
				if (unham_top_page_number (&cn->btt_link[l], raw))
					top_link_stat (cn, l);

				raw += 8;
				++l;
			}
		} else {
			raw += 40;
		}
	}

	/* What's in packet 23? */

#if 0
	if (changed && (td->handlers.event_mask & VBI_EVENT_PAGE_TYPE)) {
		vbi_event e;

		CLEAR (e);

		e.type		= VBI_EVENT_PAGE_TYPE;
		e.network	= &td->network->network;
		e.timestamp	= td->timestamp;

		_vbi_event_handler_list_send (&td->handlers, &e);
	}
#endif
}

/* 11.2 Additional Information Table */

static vbi_bool
decode_ait_packet		(cache_page *		cp,
				 const uint8_t		buffer[40],
				 unsigned int		packet)
{
	unsigned int i;

	assert (46 * sizeof (struct ttx_ait_title)
		== sizeof (cp->data.ait.title));

	if (packet < 1 || packet > 23)
		return FALSE;

	for (i = 0; i < 2; ++i) {
		struct ttx_ait_title temp;
		unsigned int j;
		int c;

		if (!unham_top_page_number (&temp.link, buffer))
			continue;

		buffer += 8;

		for (j = 0; j < 12; ++j) {
			/* Usually filled up with spaces, but zeroes also
			   possible. */
			c = vbi_unpar8 (buffer[j]);
			if (c && c < 0x20)
				break;

			temp.text[j] = c;
		}

		if (j < 12)
			continue;

		buffer += 12;

		if (TTX_DECODER_LOG) {
			log ("AIT %2u: ", packet * 2 - 2 + i);
			_vbi_ttx_page_link_dump (&temp.link, stderr);
			fputc (' ', stderr);
			for (j = 0; j < 12; ++j)
				log ("%02x ", temp.text[j] & 0xFF);
			fputc ('>', stderr);
			for (j = 0; j < 12; ++j)
				fputc (_vbi_to_ascii (temp.text[j]), stderr);
			fputs ("<\n", stderr);
		}

		cp->data.ait.title[packet * 2 - 2 + i] = temp;
	}

	return TRUE;
}

static vbi_bool
convert_ait_page		(cache_page *		dst,
				 const cache_page *	src)
{
	unsigned int packet;
	const uint8_t *raw;

	/* NO_PAGE (pgno): (pgno & 0xFF) == 0xFF. */
	memset (&dst->data.ait, -1, sizeof (dst->data.ait));

	dst->function = PAGE_FUNCTION_AIT;

	raw = src->data.unknown.raw[1];

	for (packet = 1; packet <= 23; ++packet) {
		if (src->lop_packets & (1 << packet))
			if (!decode_ait_packet (dst, raw, packet))
				return FALSE;

		raw += 40;
	}

	dst->data.ait.checksum = 0; /* changed */

	return TRUE;
}

/* 11.2 Multi-Page Table */

static void
decode_mpt_page			(vbi_ttx_decoder *	td,
				 const cache_page *	cp)
{
	unsigned int packet;
	const uint8_t *raw;
	vbi_pgno pgno;

	raw = cp->data.unknown.raw[1];

	pgno = 0x100;

	for (packet = 1; packet <= 20; ++packet) {
		if (cp->lop_packets & (1 << packet)) {
			unsigned int i;

			for (i = 0; i < 40; ++i) {
				struct ttx_page_stat *ps;
				vbi_ttx_page_type page_type;
				unsigned int subcode;
				int n;

				if ((n = vbi_unham8 (raw[i])) < 0) {
					pgno = vbi_add_bcd (pgno, 1);
					continue;
				}

				ps = cache_network_page_stat
					(td->network, pgno);

				page_type = ps->page_type;
				subcode = ps->subcode;

				log ("MPT %04x: %04x %x %s\n",
				     pgno, subcode, n,
				     _vbi_ttx_page_type_name (page_type));

				if (n > 9) {
					/* Has more than 9 subpages, details
					   may follow in MPT-EX. */
					n = 0x10;
				}

				if (SUBCODE_UNKNOWN == subcode)
					subcode = 0x0000;
				else if (SUBCODE_MULTI_PAGE == subcode)
					subcode = 0x0002; /* at least */

				if (VBI_NO_PAGE != page_type
				    && VBI_UNKNOWN_PAGE != page_type
				    && (unsigned int) n > subcode) {
					ps->subcode = n;
				}

				pgno = vbi_add_bcd (pgno, 1);
			}
		} else {
			pgno = vbi_add_bcd (pgno, 0x40);
		}

		raw += 40;
	}
}

/* 11.2 Multi-Page Extension Table */

static void
decode_mpt_ex_page		(vbi_ttx_decoder *	td,
				 const cache_page *	cp)
{
	unsigned int packet;
	const uint8_t *raw;

	raw = cp->data.unknown.raw[1];

	for (packet = 1; packet <= 23; ++packet) {
		if (cp->lop_packets & (1 << packet)) {
			unsigned int i;

			for (i = 0; i < 5; ++i) {
				struct ttx_page_link pl;
				struct ttx_page_stat *ps;
				vbi_ttx_page_type page_type;
				unsigned int subcode;

				if (!unham_top_page_number (&pl, raw)) {
					raw += 8;
					continue;
				}

				pl.subno = vbi_bin2bcd (pl.subno & 0x7F);

				if (TTX_DECODER_LOG) {
					log ("MPT-EX %3u: ",
					     (packet - 1) * 5 + i);
					_vbi_ttx_page_link_dump (&pl, stderr);
					fputc ('\n', stderr);
				}

				if (pl.pgno < 0x100)
					break;
				else if (pl.pgno > 0x8FF
					 || pl.subno < 0x02
					 || pl.subno > 0x99)
					continue;

				ps = cache_network_page_stat
					(td->network, pl.pgno);

				page_type = ps->page_type;
				subcode = ps->subcode;

				if (SUBCODE_UNKNOWN == subcode)
					subcode = 0x0000;
				else if (SUBCODE_MULTI_PAGE == subcode)
					subcode = 0x0002; /* at least */

				if (VBI_NO_PAGE != page_type
				    && VBI_UNKNOWN_PAGE != page_type
				    && (unsigned int) pl.subno > subcode) {
					ps->subcode = pl.subno;
				}

				raw += 8;
			}
		} else {
			raw += 40;
		}
	}
}

/*
	11.3 Magazine Inventory Page
*/

static vbi_bool
mip_page_stat			(cache_network *	cn,
				 const cache_page *	cp,
				 const uint8_t **	raw,
				 unsigned int *		sub_index,
				 vbi_pgno		pgno)
{
	struct ttx_page_stat *ps;
	vbi_ttx_page_type page_type;
	vbi_ttx_page_type old_type;
	int code;
	unsigned int subcode;
	unsigned int old_subcode;
	vbi_bool changed;

	code = vbi_unham16p (*raw);
	*raw += 2;

	if (code < 0)
		return FALSE;

	changed = FALSE;

	ps = cache_network_page_stat (cn, pgno);

	page_type = VBI_UNKNOWN_PAGE;
	subcode = SUBCODE_UNKNOWN;

	old_type = ps->page_type;
	old_subcode = ps->subcode;

	switch (code) {
	case 0x00: 	    /* VBI_NO_PAGE */
	case 0x01:	    /* VBI_NORMAL_PAGE, single page */
	case 0x78:	    /* VBI_SUBTITLE_INDEX */
	case 0x7A:	    /* VBI_PROGR_WARNING (adult content etc) */
	case 0x7C:	    /* VBI_CURRENT_PROGR, single page */
	case 0x7D:	    /* VBI_NOW_AND_NEXT */
	case 0x7F:	    /* VBI_PROGR_INDEX, single page */
	case 0x80:	    /* VBI_NOT_PUBLIC */
	case 0x81:	    /* VBI_PROGR_SCHEDULE, single page */
	case 0xE3:	    /* VBI_PFC_EPG_DATA */
	case 0xE4:	    /* VBI_PFC_DATA */
	case 0xE7:	    /* VBI_SYSTEM_PAGE, e.g. MOT and MIP */
	case 0xF9:	    /* VBI_KEYWORD_SEARCH_LIST, single page */
	case 0xFC:	    /* VBI_TRIGGER_DATA */
	case 0xFD:	    /* VBI_ACI_PAGE (automatic channel installation) */
	case 0xFE:	    /* VBI_TOP_PAGE (AIT, MPT, MPT-EX) */
		page_type = code;
		subcode = 0;
		break;

	case 0x02 ... 0x4F: /* VBI_NORMAL_PAGE with 2 ... 79 subpages */
		page_type = VBI_NORMAL_PAGE;
		subcode = vbi_bin2bcd (code);
		break;

	case 0x50 ... 0x51: /* normal page */
	case 0xD0 ... 0xD1: /* program */
	case 0xE0 ... 0xE1: /* page format CA */
	case 0x7B:	    /* current program, multi-page */
	case 0x7E:	    /* program index, multi-page */
	case 0xF8:	    /* keyword search list, multi-page */
	{
		unsigned int packet = *sub_index / 13;
		const uint8_t *raw;
		int n;

		if (packet > 24 || 0 == (cp->lop_packets & (1 << packet)))
			return FALSE;

		raw = &cp->data.unknown.raw[packet][*sub_index % 13];

		n = vbi_unham16p (raw) | (vbi_unham8 (raw[2]) << 8);

		if (n < 0) {
			return FALSE;
		} else if (0x1 == (code & 0xF)) {
			/* 4096+ subpages? Strange but true. */
			subcode = n + (1 << 12);
		} else if (n < 2) {
			return FALSE;
		} else {
			subcode = n;
		}

		page_type = (code == 0xF8) ? VBI_KEYWORD_SEARCH_DATA :
			    (code == 0x7E) ? VBI_PROGR_INDEX :
			    (code == 0x7B) ? VBI_CURRENT_PROGR :
			    (code >= 0xE0) ? VBI_CA_DATA :
			    (code >= 0xD0) ? VBI_PROGR_SCHEDULE :
					     VBI_NORMAL_PAGE;
		break;
	}

	case 0x52 ... 0x5F: /* reserved */
	case 0x60 ... 0x61: /* reserved, we use them as
			       VBI_TOP_BLOCK (= BTT_BLOCK_S/M)
			       VBI_TOP_GROUP (= BTT_GROUP_S/M) */
	case 0x62 ... 0x6F: /* reserved */
	case 0xD2 ... 0xDF: /* reserved */
	case 0xFF: 	    /* reserved, we use it as VBI_UNKNOWN_PAGE */
		return FALSE;

	case 0x70 ... 0x77:
		page_type = VBI_SUBTITLE_PAGE;
		subcode = 0;

		if (0xFF == ps->charset_code) {
			const struct ttx_magazine *mag;
			vbi_ttx_charset_code charset_code;

			mag = cache_network_magazine (cn, cp->pgno);

			charset_code = (mag->extension.charset_code[0]
					& (unsigned int) ~7)
				| (code & 7);

			if (vbi_ttx_charset_from_code (charset_code)) {
				ps->charset_code = charset_code;
			} else {
				ps->charset_code = charset_code & 7;
			}

			changed = TRUE;
		}

		break;

	case 0x79:
		page_type = VBI_CLOCK_PAGE;
		subcode = 0x3F7F;
		break;

	case 0x82 ... 0xCF: /* VBI_PROGR_SCHEDULE with 2 ... 79 subpages */
		page_type = VBI_PROGR_SCHEDULE;
		subcode = vbi_bin2bcd (code & 0x7F);
		break;

	case 0xE2:	    /* Page format CA, undefined number of subpages. */
	case 0xE5:	    /* VBI_DRCS_PAGE */
	case 0xE6:	    /* VBI_POP_PAGE */
	case 0xF0 ... 0xF3: /* broadcaster system page */
	case 0xF4 ... 0xF6: /* test page */
	case 0xF7:	    /* displayable system page */
		page_type = code;
		subcode = old_subcode;
		break;

	case 0xE8 ... 0xEB:
		page_type = VBI_DRCS_DATA;
		subcode = old_subcode;
		break;

	case 0xEC ... 0xEF:
		page_type = VBI_POP_DATA;
		subcode = old_subcode;
		break;
	}

	if (old_type != page_type
	    && (VBI_UNKNOWN_PAGE == old_type
		|| VBI_SUBTITLE_PAGE == old_type
		|| VBI_SUBTITLE_PAGE == page_type)) {
		ps->page_type = page_type;
		changed = TRUE;
	}

	if (SUBCODE_UNKNOWN == old_subcode)
		old_subcode = 0x0000;
	else if (SUBCODE_MULTI_PAGE == old_subcode)
		old_subcode = 0x0002; /* at least */

	if (subcode > old_subcode) {
		ps->subcode = subcode;
	}

	log ("MIP %04x: %02x:%02x:%04x %s\n",
	     pgno, page_type, ps->charset_code, subcode,
	     _vbi_ttx_page_type_name (page_type));

	return changed;
}

static void
decode_mip_page			(vbi_ttx_decoder *	td,
				 const cache_page *	cp)
{
	unsigned int packet;
	unsigned int sub_index;
	const uint8_t *raw;
	vbi_bool changed;
	vbi_pgno pgno;

	sub_index = 15 * 13;

	raw = cp->data.unknown.raw[1];

	changed = FALSE;

	pgno = cp->pgno & 0xF00;

	for (packet = 1; packet <= 8; ++packet) {
		/* 11.3.2: Packets which contain only 0x00 VBI_NO_PAGE
		   are optional. */
		if (cp->lop_packets & (1 << packet)) {
			vbi_pgno i;

			for (i = 0x00; i <= 0x09; ++i)
				changed |= mip_page_stat (td->network, cp,
							  &raw, &sub_index,
							  pgno + i);

			for (i = 0x10; i <= 0x19; ++i)
				changed |= mip_page_stat (td->network, cp,
							  &raw, &sub_index,
							  pgno + i);
		} else {
			raw += 40;
		}

		pgno += 0x20;
	}

	pgno = cp->pgno & 0xF00;

	for (packet = 9; packet <= 14; ++packet) {
		/* 11.3.2: Packets which contain only 0x00 VBI_NO_PAGE
		   are optional. */
		if (cp->lop_packets & (1 << packet)) {
			vbi_pgno i;

			for (i = 0x0A; i <= 0x0F; ++i)
				changed |= mip_page_stat (td->network, cp,
							  &raw, &sub_index,
							  pgno + i);

			if (14 == packet) /* 0xFA ... 0xFF, rest unused */
				break;

			for (i = 0x1A; i <= 0x1F; ++i)
				changed |= mip_page_stat (td->network, cp,
							  &raw, &sub_index,
							  pgno + i);

			for (i = 0x2A; i <= 0x2F; ++i)
				changed |= mip_page_stat (td->network, cp,
							  &raw, &sub_index,
							  pgno + i);
		} else {
			raw += 40;
		}

		pgno += 0x30;
	}

#if 0
	if (changed && (td->handlers.event_mask & VBI_EVENT_PAGE_TYPE)) {
		vbi_event e;

		CLEAR (e);

		e.type		= VBI_EVENT_PAGE_TYPE;
		e.network	= &td->network->network;
		e.timestamp	= td->timestamp;

		_vbi_event_handler_list_send (&td->handlers, &e);
	}
#endif
}

/*
	14. Dynamically Re-definable Characters download page
*/

static void
cache_page_drcs_dump		(const cache_page *	cp,
				 FILE *			fp)
{
	unsigned int i;
	const uint8_t *p;

	fprintf (fp, "DRCS page %03x.%04x\n", cp->pgno, cp->subno);

	p = cp->data.drcs.chars[0];

	for (i = 0; i < DRCS_PTUS_PER_PAGE; ++i) {
		unsigned int j;

		fprintf (fp, "#%2u mode %02x\n ", i, cp->data.drcs.mode[i]);

		for (j = 0; j < 10; ++j) {
			unsigned int k;

			for (k = 0; k < 6; ++k)
				fprintf (fp, "%x%x", p[k] & 15, p[k] >> 4);

			p += 6;
			fputs ("\n ", fp);
		}
	}
}

static const unsigned int
expand1 [64] = {
	0x000000, 0x100000, 0x010000, 0x110000,
	0x001000, 0x101000, 0x011000, 0x111000,
	0x000100, 0x100100, 0x010100, 0x110100,
	0x001100, 0x101100, 0x011100, 0x111100,
	0x000010, 0x100010, 0x010010, 0x110010,
	0x001010, 0x101010, 0x011010, 0x111010,
	0x000110, 0x100110, 0x010110, 0x110110,
	0x001110, 0x101110, 0x011110, 0x111110,
	0x000001, 0x100001, 0x010001, 0x110001,
	0x001001, 0x101001, 0x011001, 0x111001,
	0x000101, 0x100101, 0x010101, 0x110101,
	0x001101, 0x101101, 0x011101, 0x111101,
	0x000011, 0x100011, 0x010011, 0x110011,
	0x001011, 0x101011, 0x011011, 0x111011,
	0x000111, 0x100111, 0x010111, 0x110111,
	0x001111, 0x101111, 0x011111, 0x111111
};

static const unsigned int
expand2 [8] = {
	0x000000, 0x110000, 0x001100, 0x111100,
	0x000011, 0x110011, 0x001111, 0x111111,
};

static void
decode_drcs_page		(cache_page *		cp)
{
	uint8_t *s;
	uint8_t *d;
	uint64_t invalid;
	unsigned int i;

	invalid = 0;

	s = cp->data.drcs.lop.raw[1];

	for (i = 0; i < DRCS_PTUS_PER_PAGE / 2; ++i) {
		if (cp->lop_packets & (2 << i)) {
			unsigned int j;

			for (j = 0; j < 40; ++j)
				if (vbi_unpar8 (s[j]) < 0x40) {
					invalid |= ((uint64_t) 3) << (i * 2);
					break;
				}
		} else {
			invalid |= ((uint64_t) 3) << (i * 2);
		}

		s += 40;
	}

	d = cp->data.drcs.chars[0];
	s = cp->data.drcs.lop.raw[1];

	for (i = 0; i < DRCS_PTUS_PER_PAGE; ++i) {
		unsigned int j;
		unsigned int q;

		switch ((enum ttx_drcs_mode) cp->data.drcs.mode[i]) {
		case DRCS_MODE_12_10_1:
			if (invalid & (((uint64_t) 1) << i)) {
				s += 20;
				d += 60;
			} else {
				for (j = 0; j < 20; ++j) {
					q = expand1[*s++ & 0x3F];
					d[0] = q;
					d[1] = q >> 8;
					d[2] = q >> 16;
					d += 3;
				}
			}

			break;

		case DRCS_MODE_12_10_2:
			if (invalid & (((uint64_t) 3) << i)) {
				invalid |= ((uint64_t) 3) << i;
				s += 40;
				d += 60;
			} else {
				for (j = 0; j < 20; ++j) {
					q = expand1[s[ 0] & 0x3F]
					  + expand1[s[20] & 0x3F] * 2;
					++s;
					d[0] = q;
					d[1] = q >> 8;
					d[2] = q >> 16;
					d += 3;
				}
			}

			i += 1;

			break;

		case DRCS_MODE_12_10_4:
			if (invalid & (((uint64_t) 15) << i)) {
				invalid |= ((uint64_t) 15) << i;
				s += 80;
				d += 60;
			} else {
				for (j = 0; j < 20; ++j) {
					q = expand1[s[ 0] & 0x3F]
					  + expand1[s[20] & 0x3F] * 2
					  + expand1[s[40] & 0x3F] * 4
					  + expand1[s[60] & 0x3F] * 8;
					++s;
					d[0] = q;
					d[1] = q >> 8;
					d[2] = q >> 16;
					d += 3;
				}
			}

			i += 3;

			break;

		case DRCS_MODE_6_5_4:
			if (invalid & (((uint64_t) 1) << i)) {
				s += 20;
				d += 60;
			} else {
				for (j = 0; j < 5; ++j) {
					q = expand2[s[0] & 7]
					  + expand2[s[1] & 7] * 2
					  + expand2[s[2] & 7] * 4
					  + expand2[s[3] & 7] * 8;
					d[0] = q;
					d[1] = q >> 8;
					d[2] = q >> 16;
					q = expand2[(s[0] >> 3) & 7]
					  + expand2[(s[1] >> 3) & 7] * 2
					  + expand2[(s[2] >> 3) & 7] * 4
					  + expand2[(s[3] >> 3) & 7] * 8;
					s += 4;
					d[3] = q;
					d[4] = q >> 8;
					d[5] = q >> 16;
					memcpy (d + 6, d, 6);
					d += 12;
				}
			}

			break;

		default:
			s += 20;
			d += 60;
			break;
		}
	}

	cp->data.drcs.invalid &= invalid;

	if (0)
		cache_page_drcs_dump (cp, stderr);
}

/**
 * @internal
 *
 * Since MOT, MIP and X/28 are optional, the function of a system page
 * may not be clear until we format a LOP and find a link.
 */
cache_page *
_vbi_convert_cached_page	(cache_page *		cp,
				 enum ttx_page_function	new_function)
{
	cache_page temp;
	cache_page *cp1;

	if (PAGE_FUNCTION_UNKNOWN != cp->function) {
		return NULL;
	}

	log ("Convert page %s -> %s\n",
	     _vbi_ttx_page_function_name (cp->function),
	     _vbi_ttx_page_function_name (new_function));

	cache_page_copy (&temp, cp);

	switch (new_function) {
	case PAGE_FUNCTION_LOP:
		temp.function = new_function;
		break;

	case PAGE_FUNCTION_GPOP:
	case PAGE_FUNCTION_POP:
		if (!convert_pop_page (&temp, cp, new_function))
			return NULL;
		break;

	case PAGE_FUNCTION_GDRCS:
	case PAGE_FUNCTION_DRCS:
	{
		unsigned int i;

		/* 14.1: Packet X/28/3 is optional when PTUs use only
		   mode 12x10x1. B.2: X/28 must be transmitted before
		   packets 1 ... 24.

		   Since we didn't readily recognize the page as DRCS,
		   X/28/3 has been omitted, so 12x10x1 must be assumed.
		   When we missed X/28/3 due to a transmission error
		   we will correct the page when retransmitted. */

		for (i = 0; i < DRCS_PTUS_PER_PAGE; ++i)
			temp.data.drcs.mode[i] = DRCS_MODE_12_10_1;

		temp.function = new_function;
		temp.data.drcs.invalid = (uint64_t) -1; /* all */

		decode_drcs_page (&temp);

		break;
	}

	case PAGE_FUNCTION_AIT:
		if (!convert_ait_page (&temp, cp))
			return NULL;
		break;

	default:
		/* Needs no conversion. */
		assert (0);
	}

	if (!(cp1 = _vbi_cache_put_page (cp->network->cache,
					 cp->network,
					 &temp)))
		return NULL;

	cache_page_unref (cp);

	return cp1;
}


static vbi_ttx_charset_code
page_charset_code		(vbi_ttx_decoder *	td,
				 const cache_page *	cp)
{
	vbi_ttx_charset_code charset_code;
	const struct ttx_magazine *mag;

	if (cp->x28_designations
	    & cp->data.ext_lop.ext.designations & 0x11) {
		charset_code = (cp->data.ext_lop.ext.charset_code[0]
				& (unsigned int) ~7) + cp->national;

		if (vbi_ttx_charset_from_code (charset_code))
			return charset_code;

		charset_code = cp->data.ext_lop.ext.charset_code[0];

		if (vbi_ttx_charset_from_code (charset_code))
			return charset_code;
	}

	mag = cache_network_magazine (td->network, cp->pgno);

	charset_code = (mag->extension.charset_code[0] &
			(unsigned int) ~7) + cp->national;

	if (vbi_ttx_charset_from_code (charset_code))
		return charset_code;

	charset_code = mag->extension.charset_code[0];

	if (vbi_ttx_charset_from_code (charset_code))
		return charset_code;

	return 0xFF; /* unknown */
}

/* 9.3.1 Teletext packet 0, Page Header. */

#if 0
static int
same_header			(vbi_pgno		cur_pgno,
				 const uint8_t *	cur,
				 vbi_pgno		ref_pgno,
				 const uint8_t *	ref)
{
	uint8_t buf[3];
	unsigned int i;
	unsigned int j;
	int neq;
	int err;

	/* NB this assumes vbi_is_bcd (cur_pgno),
	   which follows from roll_header. */

	buf[2] =  (cur_pgno       & 15) + '0';
	buf[1] = ((cur_pgno >> 4) & 15) + '0';
	buf[0] =  (cur_pgno >> 8)       + '0';

	vbi_par (buf, 3);

	j = 32;

	neq = 0;
	err = 0;

	/* Header: 8 byte page address & control, 24 text,
	   	   8 byte hh:mm:ss (usually). */

	for (i = 8; i < 29; ++i) {
		if (cur[0] == buf[0]
		    && cur[1] == buf[1]
		    && cur[2] == buf[2])
			break;
		err |= vbi_unpar8 (*cur);
		err |= vbi_unpar8 (*ref);
		neq |= *cur++ ^ *ref++;
	}

	/* Skip page number. */

	j = i;
	i += 3;
	cur += 3;
	ref += 3;

	for (; i < 32; ++i) {
		err |= vbi_unpar8 (*cur);
		err |= vbi_unpar8 (*ref);
		neq |= *cur++ ^ *ref++;
	}

	/* Parity error or no page number. */
	if (err < 0 || j >= 32)
		return -3; /* inconclusive */

	if (!neq) {
		/* I have yet to see equal headers from different
		   networks, so this is probably correct. */
		return TRUE;
	}

	/* Comparing headers from different magazines
	   can result in false negatives. */
	if ((cur_pgno ^ ref_pgno) & 0xF00)
		return -2; /* inconclusive */

	/* Often headers show the current date. Check for false
	   negative due to date transition. 23xmmxss -> 00xmmxss. */
	if (((ref[32] * 256 + ref[33]) & 0x7F7F) == 0x3233
	    && ((cur[32] * 256 + cur[33]) & 0x7F7F) == 0x3030) {
		return -1; /* inconclusive */
	}

	/* Pages with headers from the same network can still differ,
	   but what more can we do. */

	return FALSE;
}
#endif

#if 0

/* Note "clock" may not be a clock. */
static vbi_bool
same_clock			(const uint8_t *	cur,
				 const uint8_t *	ref)
{
	unsigned int i;

	for (i = 32; i < 40; ++i) {
	       	if (*cur != *ref
		    && (vbi_ipar8 (*cur) | vbi_ipar8 (*ref)) >= 0)
			return FALSE;
		++cur;
		++ref;
	}

	return TRUE;
}

static void
network_event			(vbi_ttx_decoder *	td)
{
	td = td;

	vbi_event e;

	CLEAR (e);

	e.type = VBI_EVENT_NETWORK;
	e.network = &td->network->network;
	e.timestamp = td->timestamp;

	_vbi_event_handler_list_send (&td->handlers, &e);

	e.type = VBI_EVENT_TOP_CHANGE;

	_vbi_event_handler_list_send (&td->handlers, &e);

/* XXX?
	e.type = VBI_EVENT_PAGE_TYPE;

	_vbi_event_handler_list_send (&td->handlers, &e);

	e.type = VBI_EVENT_PROG_INFO;
	e.ev.prog_info = &cn->program_info;

	_vbi_event_handler_list_send (&td->handlers, &e);
*/
	/* XXX ASPECT? */
	/* XXX PROG_ID? */
}

static void
detect_channel_change		(vbi_ttx_decoder *	td,
				 const cache_page *	cp)
{
	vbi_network *nk;
	int r;

	if (0 == td->header_page.pgno) {
		/* First page after channel change, test may
		   return inconclusive due to hamming error. */
		r = same_header (cp->pgno, cp->data.lop.raw[0] + 8,
				 cp->pgno, cp->data.lop.raw[0] + 8);
	} else {
		r = same_header (cp->pgno, cp->data.lop.raw[0] + 8,
				 td->header_page.pgno, td->header + 8);
	}

	switch (r) {
	case TRUE:
		if (TTX_DECODER_CHSW_TEST) {
			fprintf (stderr, "+");
			fflush (stderr);
		}

		td->header_page.pgno = cp->pgno;
		COPY (td->header, cp->data.lop.raw[0]);

		/* Cancel reset. */
		td->virtual_reset (td, NULL, -1.0);

		if (0) { /* XXX improve */

		nk = &td->network->network;

#if 0
		if (!nk->name
		    && 0 == nk->call_sign[0]
		    && 0 == (nk->cni_vps | nk->cni_8301 | nk->cni_8302)) {
			if (_vbi_network_set_name_from_ttx_header
			    (nk, cp->data.lop.raw[0]))
				network_event (td);
		}
#endif
		}

		break;

	case FALSE:
		if (TTX_DECODER_CHSW_TEST) {
			fprintf (stderr, "-");
			fflush (stderr);
		}

		td->header_page.pgno = cp->pgno;
		COPY (td->header, cp->data.lop.raw[0]);

		/* Reset at next vbi_ttx_decoder_feed() call. */
		td->virtual_reset (td, NULL, td->timestamp + 0.0);

		break;

	default:
		if (TTX_DECODER_CHSW_TEST) {
			fprintf (stderr, "%d", -r);
			fflush (stderr);
		}

		if (-1 == r) {
			/* Date transition. */
			td->header_page.pgno = cp->pgno;
			COPY (td->header, cp->data.lop.raw[0]);
		} else if (td->reset_time <= 0) {
			/* Suspect a channel change. */
			td->virtual_reset (td, NULL, td->timestamp
					   + td->cni_830_timeout);
		}

		break;
	}
}
#endif

static void
store_page_and_send_event	(vbi_ttx_decoder *	td,
				 cache_page *		cp,
				 vbi_bool		roll_header)
{
	cache_page *cp1;
	vbi_event e;

	log ("... store %03x.%04x, "
	     "packets=%x x26=%x x27=%x x28=%x\n",
	     cp->pgno, cp->subno,
	     cp->lop_packets,
	     cp->x26_designations,
	     cp->x27_designations,
	     cp->x28_designations);

	cp1 = _vbi_cache_put_page (td->cache, td->network, cp);
	if (NULL == cp1)
		return;

	CLEAR (e);

	e.type = VBI_EVENT_TTX_PAGE;
	e.network = &td->network->network;
	e.capture_time = td->timestamp.sys;
	e.pts = td->timestamp.pts;

	e.ev.ttx_page.pgno = cp->pgno;
	e.ev.ttx_page.subno = cp->subno;
 
	e.ev.ttx_page.flags = roll_header ? VBI_ROLL_HEADER : 0;
	e.ev.ttx_page.flags |= cp->flags & (C5_NEWSFLASH |
					    C6_SUBTITLE |
					    C8_UPDATE |
					    C11_MAGAZINE_SERIAL);

	_vbi_event_handler_list_send (&td->handlers, &e);

	cache_page_unref (cp1);
}

static void
store_page			(vbi_ttx_decoder *	td,
				 cache_page *		cp)
{
	cache_page *cp1;

#if 0
	if (td->reset_time > 0) {
		/* Suspended because we suspect a channel change. */
		if (PAGE_FUNCTION_LOP != cp->function)
			return;
	}
#endif

	if (0 && !vbi_is_bcd (cp->pgno) && 0xFF != (0xFF & cp->pgno)) {
		fprintf (stderr, "store_page:\n");
		cache_page_raw_dump (cp, stderr, PAGE_CODING_ODD_PARITY);
	}

	switch (cp->function) {
	case PAGE_FUNCTION_EPG:
	case PAGE_FUNCTION_ACI:
	case PAGE_FUNCTION_DISCARD:
	case PAGE_FUNCTION_DATA:
		/* Discard. */
		break;

	case PAGE_FUNCTION_LOP:
	{
		struct ttx_page_stat *ps;
		vbi_bool roll_header;

		if (0)
			cache_page_raw_dump (cp, stderr,
					     PAGE_CODING_ODD_PARITY);

		roll_header = (vbi_is_bcd (cp->pgno)
			       && 0 == (cp->flags &
					(C5_NEWSFLASH |
					 C6_SUBTITLE |
					 C7_SUPPRESS_HEADER |
					 C9_INTERRUPTED |
					 C10_INHIBIT_DISPLAY)));
#if 0
		if (roll_header)
			detect_channel_change (td, cp);
#endif
#if 0
		/* We know there was a channel change, or the check was
		   inconclusive and we suspect there was a change, or
		   a change was already suspected but the header is unsuitable
		   to make a decision now. Discard received data. */
		if (td->reset_time > 0)
			return;
#endif
		/* Collect information about pages
		   not listed in MIP and BTT. */

		ps = cache_network_page_stat (td->network, cp->pgno);

		ps->flags = cp->flags;

		switch ((vbi_ttx_page_type) ps->page_type) {
		case VBI_NO_PAGE:
		case VBI_UNKNOWN_PAGE:
			if (cp->flags & C6_SUBTITLE)
				ps->page_type = VBI_SUBTITLE_PAGE;
			else
				ps->page_type = VBI_NORMAL_PAGE;

			if (0xFF == ps->charset_code) {
				ps->charset_code =
					page_charset_code (td, cp);
			}

			break;

		case VBI_NORMAL_PAGE:
			if (cp->flags & C6_SUBTITLE)
				ps->page_type = VBI_SUBTITLE_PAGE;

			if (0xFF == ps->charset_code) {
				ps->charset_code =
					page_charset_code (td, cp);
			}

			break;

		case VBI_TOP_BLOCK:
		case VBI_TOP_GROUP:
		case VBI_SUBTITLE_INDEX:
		case VBI_CLOCK_PAGE:
		case VBI_PROGR_WARNING:
		case VBI_CURRENT_PROGR:
		case VBI_NOW_AND_NEXT:
		case VBI_PROGR_INDEX:
		case VBI_PROGR_SCHEDULE:
			if (0xFF == ps->charset_code) {
				ps->charset_code =
					page_charset_code (td, cp);
			}

			break;

		case VBI_SUBTITLE_PAGE:
			if (cp->flags & C6_SUBTITLE) {
				/* Keep up-to-date, just in case. */
				ps->charset_code =
					page_charset_code (td, cp);
			} else {
				/* Fixes bug in ORF 1 BTT. */
				ps->page_type = VBI_NORMAL_PAGE;
			}

			break;

		default:
			break;
		}

		if (td->handlers.event_mask & VBI_EVENT_TTX_PAGE) {
			store_page_and_send_event (td, cp, roll_header);
		}

		break;
	}

	case PAGE_FUNCTION_GPOP:
	case PAGE_FUNCTION_POP:
		/* Page was decoded on the fly. */

		if (td->handlers.event_mask & VBI_EVENT_TTX_PAGE) {
			cp1 = _vbi_cache_put_page
				(td->cache, td->network, cp);

			cache_page_unref (cp1);
		}

		break;

	case PAGE_FUNCTION_GDRCS:
	case PAGE_FUNCTION_DRCS:
		/* (Global) Dynamically Redefinable Characters download page -
		   Identify valid PTUs, convert PTU bit planes to character
		   pixmaps and store in cp->data.drcs.chars[]. Note
		   characters can span multiple packets. */
		if (td->handlers.event_mask & VBI_EVENT_TTX_PAGE) {
			decode_drcs_page (cp);

			cp1 = _vbi_cache_put_page
				(td->cache, td->network, cp);

			cache_page_unref (cp1);
		}

		break;

	case PAGE_FUNCTION_MOT:
		/* Magazine Organization Table -
		   Store valid (G)POP and (G)DRCS links in
		   td->network->magazine[n], update td->network->pages[]. */
		decode_mot_page (td, cp);
		break;

	case PAGE_FUNCTION_MIP:
		/* Magazine Inventory Page -
		   Store valid information in td->network->pages[]. */
		decode_mip_page (td, cp);
		break;

	case PAGE_FUNCTION_BTT:
		/* Basic TOP Table -
		   Store valid information in td->network->pages[], valid
		   links to other BTT pages in td->network->btt_link[]. */
		decode_btt_page (td, cp);
		break;

	case PAGE_FUNCTION_AIT:
		/* Page was decoded on the fly. */
		cp1 = _vbi_cache_put_page (td->cache, td->network, cp);

		if (cp1) {
			unsigned int i;
			unsigned int sum;

			sum = 0;

			for (i = 0; i < sizeof (cp->data.ait.title); ++i)
				sum += ((uint8_t *) &cp->data.ait.title)[i];

#if 0
			if (cp->data.ait.checksum != sum
			    && (td->handlers.event_mask
				& VBI_EVENT_TOP_CHANGE)) {
				vbi_event e;

				CLEAR (e);

				e.type		= VBI_EVENT_TOP_CHANGE;
				e.network	= &td->network->network;
				e.timestamp	= td->timestamp;

				_vbi_event_handler_list_send
					(&td->handlers, &e);
			}
#endif
			cp1->data.ait.checksum = sum;

			cache_page_unref (cp1);
		}

		break;

	case PAGE_FUNCTION_MPT:
		/* Multi-Page Table -
		   Store valid information about single-digit subpages in
		   td->network->pages[]. */
		decode_mpt_page (td, cp);
		break;

	case PAGE_FUNCTION_MPT_EX:
		/* Multi-Page Extension Table -
		   Store valid information about multi-digit subpages in
		   td->network->pages[]. */
		decode_mpt_ex_page (td, cp);
		break;

	case PAGE_FUNCTION_TRIGGER:
#if 0
		if (td->handlers.event_mask & VBI_EVENT_TRIGGER)
			eacem_trigger (td, cp);
#endif
		break;

	case PAGE_FUNCTION_UNKNOWN:
		/* The page is not listed in MIP, MOT or BTT, or
		   we have no MIP, MOT or BTT (yet), and
		   the page has no X/28/0 or /4, and
		   the page has a non-bcd page number, and
		   was not referenced from another page (yet). */
		cp1 = _vbi_cache_put_page (td->cache, td->network, cp);
		cache_page_unref (cp1);
		break;
	}
}

#if 0 /* TODO */

static vbi_bool
epg_callback			(vbi_pfc_demux *	pc,
				 void *			user_data,
				 const vbi_pfc_block *	block)
{
	vbi_ttx_decoder *td = user_data;

	return TRUE;
}

#endif

static vbi_bool
decode_packet_0			(vbi_ttx_decoder *	td,
				 cache_page *		cp,
				 const uint8_t		buffer[42],
				 unsigned int		mag0)
{
	int page;
	vbi_pgno pgno;
	vbi_subno subno;
	unsigned int flags;
	cache_page *cached_cp;

	if ((page = vbi_unham16p (buffer + 2)) < 0) {
		_vbi_ttx_decoder_resync (td);

		log ("Hamming error in packet 0 page number\n");

		return FALSE;
	}

	pgno = ((0 == mag0) ? 0x800 : mag0 << 8) + page;

	if (0 && !vbi_is_bcd (page) && 0xFF != page) {
		fprintf (stderr, "System page %x\n", pgno);
	} else {
		log ("Packet 0 page %x\n", pgno);
	}

	if (td->current) {
		if (td->current->flags & C11_MAGAZINE_SERIAL) {
			/* New pgno terminates the most recently
			   received page. */

			if (td->current->pgno != pgno) {
				store_page (td, td->current);
				cp->function = PAGE_FUNCTION_DISCARD;
			}
		} else {
			/* A new pgno terminates the most recently
			   received page of the same magazine. */
			if (0 != ((cp->pgno ^ pgno) & 0xFF)) {
				store_page (td, cp);
				cp->function = PAGE_FUNCTION_DISCARD;
			}
		}
	}

	/* A.1: mFF time filling / terminator. */
	if (0xFF == page) {
		td->current = NULL;
		cp->function = PAGE_FUNCTION_DISCARD;
		return TRUE;
	}

	subno = (vbi_unham16p (buffer + 4)
		 + vbi_unham16p (buffer + 6) * 256);

	/* C7 ... C14 */
	flags = vbi_unham16p (buffer + 8);

	if ((int)(subno | flags) < 0) {
		td->current = NULL;
		cp->function = PAGE_FUNCTION_DISCARD;
		return FALSE;
	}

	td->current = cp;

	if (PAGE_FUNCTION_DISCARD != cp->function
	    && cp->pgno == pgno
	    && cp->flags == (flags << 16) + subno) {
		/* Repeated header for time filling. */
		return TRUE;
	}

	cp->pgno	= pgno;
	cp->national	= vbi_rev8 (flags) & 7;
	cp->flags	= (flags << 16) + subno;

	if (vbi_is_bcd (page)) {
		subno &= 0x3F7F;

		log ("Normal page %03x.%04x flags %06x\n",
		     cp->pgno, subno, cp->flags);

		if (!vbi_is_bcd (subno)) {
			td->current = NULL;
			cp->function = PAGE_FUNCTION_DISCARD;
			return FALSE;
		}

		cp->subno = subno;
	} else {
		cp->subno = subno & 0x000F;

		log ("System page %03x.%04x flags %06x\n",
		     cp->pgno, cp->subno, cp->flags);
	}

	if (cp->flags & C4_ERASE_PAGE)
		goto erase;

	cached_cp = _vbi_cache_get_page (td->cache, td->network,
					 cp->pgno, cp->subno,
					 /* subno mask */ -1);

	if (cached_cp) {
		log ("... %03x.%04x is cached, "
		     "packets=%x x26=%x x27=%x x28=%x\n",
		     cached_cp->pgno, cached_cp->subno,
		     cached_cp->lop_packets,
		     cached_cp->x26_designations,
		     cached_cp->x27_designations,
		     cached_cp->x28_designations);

		/* Page is already cached. We load it into our page buffer,
		   add received data and store the page back when complete.
		   Note packet decoders write only correct data, this way
		   we can fix broken pages in two or more passes. */

		memcpy (&cp->data, &cached_cp->data,
			cache_page_size (cached_cp)
			- (sizeof (*cp) - sizeof (cp->data)));

		cp->function = cached_cp->function;

		switch (cp->function) {
		case PAGE_FUNCTION_UNKNOWN:
		case PAGE_FUNCTION_LOP:
			/* Store header packet. */
			memcpy (cp->data.unknown.raw[0], buffer + 2, 40);
			break;

		default:
			break;
		}

		cp->lop_packets = cached_cp->lop_packets;
		cp->x26_designations = cached_cp->x26_designations;
		cp->x27_designations = cached_cp->x27_designations;
		cp->x28_designations = cached_cp->x28_designations;

		cache_page_unref (cached_cp);

		cached_cp = NULL;
	} else {
		struct ttx_page_stat *ps;
		
		/* Page is not cached, we rebuild from scratch.
		   EPG, DATA and TRIGGER pages are never cached, they must
		   be handled by the appropriate demultiplexer.
		   Data from MOT, MIP, BTT, MPT and MPT_EX pages is cached
		   in the cache_network struct, not as page data. */
	erase:
		ps = cache_network_page_stat (td->network, cp->pgno);

		log ("... rebuild %03x.%04x from scratch\n",
		     cp->pgno, cp->subno);

		/* Determine the page function from A.10 reserved page numbers.
		   A.1: MIP and MOT always use subno 0.
		   XXX should erase MIP/MOT/TOP tables on C4. */

		if (0x1B0 == cp->pgno) {
			cp->function = PAGE_FUNCTION_ACI;
			ps->page_type = VBI_ACI_DATA;
		} else if (0x1F0 == cp->pgno) {
			cp->function = PAGE_FUNCTION_BTT;
			ps->page_type = VBI_TOP_DATA;
			CLEAR (cp->data.ext_lop);
		} else if (0xFD == page && 0 == cp->subno) {
			cp->function = PAGE_FUNCTION_MIP;
			ps->page_type = VBI_SYSTEM_DATA;
			CLEAR (cp->data.ext_lop);
		} else if (0xFE == page && 0 == cp->subno) {
			cp->function = PAGE_FUNCTION_MOT;
			ps->page_type = VBI_SYSTEM_DATA;
			CLEAR (cp->data.ext_lop);
		} else {
			cp->function = PAGE_FUNCTION_UNKNOWN;

			clear_lop (cp);
			clear_enhancement (cp);

			/* No X/28 packets received. */
			cp->data.ext_lop.ext.designations = 0;

			/* Store header packet. */
			memcpy (cp->data.unknown.raw[0] + 0, buffer + 2, 40);
		}

		/* Packet 0 received. */
		cp->lop_packets = 1;
		cp->x26_designations = 0;
		cp->x27_designations = 0;
		cp->x28_designations = 0;
	}

	/* Determine the page function from MOT, MIP or BTT data. Another
	   opportunity arises when we receive packet X/28, see there. */

	if (PAGE_FUNCTION_UNKNOWN == cp->function) {
		struct ttx_page_stat *ps;

		ps = cache_network_page_stat (td->network, cp->pgno);

		switch ((vbi_ttx_page_type) ps->page_type) {
		case VBI_NO_PAGE:
			/* Who's wrong here? */
			ps->page_type = VBI_UNKNOWN_PAGE;
			cp->function = PAGE_FUNCTION_DISCARD;
			break;

		case VBI_NORMAL_PAGE:
		case VBI_TOP_BLOCK:
		case VBI_TOP_GROUP:
		case VBI_SUBTITLE_PAGE:
		case VBI_SUBTITLE_INDEX:
		case VBI_CLOCK_PAGE:
		case VBI_CURRENT_PROGR:
		case VBI_NOW_AND_NEXT:
		case VBI_PROGR_WARNING:
		case VBI_PROGR_INDEX:
		case VBI_PROGR_SCHEDULE:
			cp->function = PAGE_FUNCTION_LOP;
			break;

		case VBI_SYSTEM_DATA:
			/* Not ACI, BTT, MOT or MIP (reserved page number),
			   not MPT, AIT or MPT-EX (VBI_TOP_PAGE),
			   so it remains unknown. */
			break;

		case VBI_TOP_DATA:
		{
			cache_network *cn;
			unsigned int i;

			cn = td->network;

			for (i = 0; i < N_ELEMENTS (cn->btt_link); ++i)
				if (cn->btt_link[i].pgno == cp->pgno)
					break;

			if (i < N_ELEMENTS (cn->btt_link)) {
				switch (cn->btt_link[i].function) {
				case PAGE_FUNCTION_MPT:
				case PAGE_FUNCTION_MPT_EX:
					cp->function =
						cn->btt_link[i].function;
					break;

				case PAGE_FUNCTION_AIT:
				{
					cache_page temp;

					cache_page_copy (&temp, cp);
					convert_ait_page (cp, &temp);

					cp->function = PAGE_FUNCTION_AIT;

					break;
				}

				default:
					assert (0);
					break;
				}
			} else {
				log ("%03x.%04x claims to be "
				     "TOP page, but no link found\n",
				     cp->pgno, cp->subno);
			}

			break;
		}

		case VBI_DRCS_DATA:
		{
			unsigned int i;

			/* 14.1: Packet X/28/3 is optional when PTUs use only
			   mode 12x10x1. B.2: X/28 must be transmitted before
			   packets 1 ... 24.

			   We have only the header yet, so let's default.
			   Conversion of packets 1 ... 24 we may have cached
			   before takes place when the page is complete. */
			for (i = 0; i < DRCS_PTUS_PER_PAGE; ++i)
				cp->data.drcs.mode[i] = DRCS_MODE_12_10_1;

			cp->function = PAGE_FUNCTION_DRCS;

			break;
		}

		case VBI_POP_DATA:
		{
			cache_page temp;

			cache_page_copy (&temp, cp);
			convert_pop_page (cp, &temp, PAGE_FUNCTION_POP);

			break;
		}

		case VBI_TRIGGER_DATA:
			cp->function = PAGE_FUNCTION_TRIGGER;
			break;

		case VBI_PFC_EPG_DATA:
		{
			cp->function = PAGE_FUNCTION_EPG;

#if 0 /* TODO */
			if (0 == td->epg_stream[0].block.pgno) {
				_vbi_pfc_demux_init (&td->epg_stream[0],
						     cp->pgno, 1,
						     epg_callback, td);

				_vbi_pfc_demux_init (&td->epg_stream[1],
						     cp->pgno, 2,
						     epg_callback, td);
			}
#endif
			break;
		}

		case VBI_NOT_PUBLIC:
		case VBI_CA_DATA:
		case VBI_PFC_DATA:
		case VBI_KEYWORD_SEARCH_DATA:
			cp->function = PAGE_FUNCTION_DISCARD;
			break;

		default:
			if (vbi_is_bcd (page))
				cp->function = PAGE_FUNCTION_LOP;
			/* else remains unknown. */
			break;
		}

		log ("... identified as %s\n",
		     _vbi_ttx_page_function_name (cp->function));
	}

	switch (cp->function) {
	case PAGE_FUNCTION_ACI:
	case PAGE_FUNCTION_TRIGGER:
		/* ? */
		break;

	case PAGE_FUNCTION_BTT:
	case PAGE_FUNCTION_AIT:
	case PAGE_FUNCTION_MPT:
	case PAGE_FUNCTION_MPT_EX:
		/* ? */
		/* Observation: BTT with 3Fx0, others with 0000. */
		break;

	case PAGE_FUNCTION_LOP:
		/* 0000 single page, 0001 ... 0079 multi page
		   2359 clock page / subpages not to be cached. */
		break;

	case PAGE_FUNCTION_EPG:
	case PAGE_FUNCTION_DATA:
		/* Uses different scheme, see EN 300 708 4.3.1 */
		break;

	case PAGE_FUNCTION_GPOP:
	case PAGE_FUNCTION_POP:
	case PAGE_FUNCTION_GDRCS:
	case PAGE_FUNCTION_DRCS:
	case PAGE_FUNCTION_MOT:
	case PAGE_FUNCTION_MIP:
	{
		unsigned int last_packet;
		unsigned int continuity;

		/* A.1, B.6 Page numbering. */
		/* C12, C13 not observed in the field, last_packet
		   is unreliable, so this information is pretty useless. */

		last_packet = subno >> 8;
		continuity = (subno >> 4) & 15;

		if (last_packet >= 26)
			log ("... last packet X/26/%u ci %u ",
			     last_packet - 26, continuity);
		else
			log ("... last packet X/%u ci %u ",
			     last_packet, continuity);

		if (cp->flags & C13_PARTIAL_PAGE)
			log ("partial page");
		else
			log ("full page");

		if (cp->flags & C12_FRAGMENT)
			log (" fragment\n");
		else
			log (", complete or first fragment\n");

		break;
	}

	case PAGE_FUNCTION_DISCARD:
	case PAGE_FUNCTION_UNKNOWN:
		break;
	}

	return TRUE;
}

/* 9.4.1 Teletext packet 26, enhancement data. */

static vbi_bool
decode_packet_26		(vbi_ttx_decoder *	td,
				 cache_page *		cp,
				 const uint8_t		buffer[42])
{
	int n18[13];
	int designation;
	int err;
	struct ttx_triplet *trip;
	unsigned int i;

	switch (cp->function) {
	case PAGE_FUNCTION_DISCARD:
		return TRUE;

	case PAGE_FUNCTION_GPOP:
	case PAGE_FUNCTION_POP:
		return decode_pop_packet (cp, buffer + 2, 26);

	case PAGE_FUNCTION_ACI:
	case PAGE_FUNCTION_EPG:
	case PAGE_FUNCTION_DATA:
	case PAGE_FUNCTION_GDRCS:
	case PAGE_FUNCTION_DRCS:
	case PAGE_FUNCTION_MOT:
	case PAGE_FUNCTION_MIP:
	case PAGE_FUNCTION_BTT:
	case PAGE_FUNCTION_AIT:
	case PAGE_FUNCTION_MPT:
	case PAGE_FUNCTION_MPT_EX:
		/* Not supposed to have X/26, tx error? */
		_vbi_ttx_decoder_resync (td);
		return FALSE;

	case PAGE_FUNCTION_UNKNOWN:
	case PAGE_FUNCTION_LOP:
	case PAGE_FUNCTION_TRIGGER:
		break;
	}

	designation = vbi_unham8 (buffer[2]);

	err = 0;

	for (i = 0; i < 13; ++i)
		err |= n18[i] = vbi_unham24p (buffer + 3 + i * 3);

	if (TTX_DECODER_LOG) {
		log ("Packet X/26/%u page %x.%x flags %x\n",
		     designation, cp->pgno, cp->subno, cp->flags);

		for (i = 1; i < 13; ++i)
			log ("... %u: %d %x\n",
			     i, n18[i], n18[i]);
	}

	if ((designation | err) < 0) {
		return FALSE;
	}

#if 0
	/* B.2, D.1.3: Packets X/26 must arrive in designation order
	   0 ... 15. No check here if the sequence is correct
	   and complete, that happens in page formatting. */
	if (cp->x26_designations != (0x0FFFFU >> (16 - designation))) {
		cp->x26_designations = 0;
		return FALSE;
	}
#endif

	cp->x26_designations |= 1 << designation;

	trip = &cp->data.enh_lop.enh[designation * 13];

	for (i = 0; i < 13; ++i) {
		trip->address	= (n18[i] >> 0) & 0x3F;
		trip->mode	= (n18[i] >> 6) & 0x1F;
		trip->data	= (n18[i] >> 11);

		++trip;
	}

	return TRUE;
}

/* 9.6 Teletext packet 27, page linking. */

static vbi_bool
decode_packet_27		(vbi_ttx_decoder *	td,
				 cache_page *		cp,
				 const uint8_t		buffer[42])
{
	const uint8_t *p;
	int designation;

	switch (cp->function) {
	case PAGE_FUNCTION_DISCARD:
		return TRUE;

	case PAGE_FUNCTION_ACI:
	case PAGE_FUNCTION_EPG:
	case PAGE_FUNCTION_DATA:
	case PAGE_FUNCTION_GPOP:
	case PAGE_FUNCTION_POP:
	case PAGE_FUNCTION_GDRCS:
	case PAGE_FUNCTION_DRCS:
	case PAGE_FUNCTION_MOT:
	case PAGE_FUNCTION_MIP:
	case PAGE_FUNCTION_BTT:
	case PAGE_FUNCTION_AIT:
	case PAGE_FUNCTION_MPT:
	case PAGE_FUNCTION_MPT_EX:
	case PAGE_FUNCTION_TRIGGER:
		/* Not supposed to have X/27, tx error? */
		_vbi_ttx_decoder_resync (td);
		return FALSE;

	case PAGE_FUNCTION_UNKNOWN:
	case PAGE_FUNCTION_LOP:
		break;
	}

	if ((designation = vbi_unham8 (buffer[2])) < 0)
		return FALSE;

	log ("Packet X/27/%u page %03x.%x\n",
	     designation, cp->pgno, cp->subno);

	p = buffer + 3;

	switch (designation) {
	case 0:
	{
		int control;
		unsigned int crc;

		if ((control = vbi_unham8 (buffer[39])) < 0)
			return FALSE;

		log ("... control %02x\n", control);

		/* CRC cannot be trusted, some stations transmit
		   incorrect data. Link Control Byte bits 1 ... 3
		   (disable green, yellow, blue key) cannot be
		   trusted, EN 300 706 is ambiguous and not all
		   stations follow the suggestions of ETR 287. */

		crc = buffer[40] + buffer[41] * 256;

		log ("... crc %04x\n", crc);

		/* ETR 287 section 10.4: Have FLOF, display row 24. */
		cp->data.lop.have_fastext = control >> 3;

		/* fall through */
	}

	case 1 ... 3:
	{
		unsigned int i;

		/* X/27/0 ... 4 */

		for (i = 0; i < 6; ++i) {
			struct ttx_page_link *pl;

			pl = &cp->data.lop.link[designation * 6 + i];

			/* Hamming error ignored. We just don't store
			   broken page numbers. */
			unham_page_link (pl, p,
					 (unsigned int)(cp->pgno >> 8) & 7);
			p += 6;

			log ("... link[%u] = %03x.%04x\n",
			     i, pl->pgno, pl->subno);
		}

		cp->x27_designations |= 1 << designation;

		break;
	}

	case 4 ... 7:
	{
		unsigned int i;

		/* X/27/4 ... 7 */

		/* (Never observed this in the field;
		    let's hope the code is correct.) */

		for (i = 0; i < 6; ++i) {
			struct ttx_page_link *pl;
			int t1, t2;

			t1 = vbi_unham24p (p + 0);
			t2 = vbi_unham24p (p + 3);

			if ((t1 | t2) < 0)
				break;

			p += 6;

			if (designation <= 5 && (t1 & (1 << 10))) {
				unsigned int mag0;
				unsigned int validity;
				
				/* 9.6.2 Packets X/27/4 and X/27/5 format 1. */

				/* t1: tt ttmmm1uu uuxxvvff */
				/* t2: ss ssssssss ssssssx0 */

				pl = &cp->data.lop.link[designation * 6 + i];

				/* GPOP, POP, GDRCS, DRCS */
				pl->function = PAGE_FUNCTION_GPOP + (t1 & 3);

				validity = (t1 >> 2) & 3;

				mag0 = (cp->pgno ^ (t1 >> 3)) & 0x700;

				pl->pgno = ((0 == mag0) ? 0x800 : mag0)
					+ (t1 >> 10)
					+ ((t1 >> 6) & 0x00F);

				/* NB this is "set of required subpages",
				   not a real subno. */
				pl->subno = t2 >> 2;

				log ("... link[%u] = %s %x/%x\n",
				     i,
				     _vbi_ttx_page_function_name
				     (pl->function),
				     pl->pgno, pl->subno);

				/* XXX

				   "The function of first four links in a
				   packet X/27/4 with the link coding of
				   table 15 is fixed for Level 2.5, as shown
				   in table 16. The Page Validity bits may
				   also indicate their use at Level 3.5. The
				   function of the remaining two links in a
				   packet X/27/4 and the first two links in
				   a packet X/27/5 is defined by the Link
				   Function and Page Validity bits. These
				   links do not contain information
				   relevant to a Level 2.5 decoder." */
			} else {
				/* 9.6.3 Packets X/27/4 to X/27/7 format 2 -
				   compositional linking in data broadcasting
				   applications. */

				/* Ignored. */
			}
		}

		cp->x27_designations |= 1 << designation;

		break;
	}

	case 8 ... 15:
		/* Undefined, ignored. */
		break;
	}

	return TRUE;
}

/*
	9.4.2, 9.4.3, 9.5 Teletext packets 28 and 29,
	Level 2.5/3.5 enhancement.
*/

struct bit_stream {
        int *			triplet;
	unsigned int		buffer;
	unsigned int		left;
};

static unsigned int
get_bits			(struct	bit_stream *	bs,
				 unsigned int		count)
{
	unsigned int r;
	int n;

	r = bs->buffer;
	n = count - bs->left;

	if (n > 0) {
		bs->buffer = *(bs->triplet)++;
		r |= bs->buffer << bs->left;
		bs->left = 18 - n;
	} else {
		n = count;
		bs->left -= count;
	}

	bs->buffer >>= n;

	return r & ((1UL << count) - 1);
}

static vbi_bool
decode_packet_28_29		(vbi_ttx_decoder *	td,
				 cache_page *		cp,
				 const uint8_t		buffer[42],
				 unsigned int		packet)
{
	const uint8_t *p;
	int designation;
	int n18[13];
	struct bit_stream bs;
	struct ttx_extension *ext;
	unsigned int i;
	unsigned int j;
	int err;

	switch (cp->function) {
	case PAGE_FUNCTION_DISCARD:
		return TRUE;

	default:
		break;
	}

	p = buffer + 2;

	if ((designation = vbi_unham8 (*p++)) < 0)
		return FALSE;

	log ("Packet %u/%u/%u page %03x.%x\n",
	     (cp->pgno >> 8) & 7, packet, designation,
	     cp->pgno, cp->subno);

	err = 0;

	for (i = 0; i < 13; ++i) {
		err |= n18[i] = vbi_unham24p (p);
		p += 3;
	}

	bs.triplet = n18;
	bs.buffer = 0;
	bs.left = 0;

	switch (designation) {
	case 0:
	{
		/* X/28/0, M/29/0 Level 2.5 */

		if (n18[0] < 0)
			return FALSE;

		if (28 == packet) {
			/*
			   triplet[13 ... 0], bit 18 ... 1:
			        ... ssss pppppp p 000 0000 a)
                           reserved 0011 111111 1 001 0001 b)
			   reserved 0011 111111 1 ccc ffff c)
                           reserved cccc 000000 0 xx0 0000 d)
                           reserved cccc 000000 1 000 0100 e)
                           reserved cccc 000000 1 000 0101 f)
			             ... 000000 0 ccc ffff g)
                                     ... 000000 1 111 0011 h)
                                     ... 000010 0 111 0011 h)

			   a) X/28/0 format 1 - function = LOP,
			      coding = ODD_PARITY, primary and secondary
			      character set code (0 ... 87) (EN 300 706,
			      9.4.2.2)
			   b) X/28/0 format 1 - function = DATA (PFC),
			      coding = UBYTES (EN 300 708, 4.3.3)
			   c) X/28/0 format 1 for other types of pages
			      (EN 300 706, 9.4.2.4)
			   d) X/28/0 format 2 (CA) - coding 0 ... 3.
			      (EN 300 706, 9.4.3 and EN 300 708, 5.4.1)
			   e), f) same as d)
			   g) observed on SWR with cccffff != 0, in violation
			      of c).
			   h) observed on ARTE, transmitted with various
			      level one pages. Apparently not format 1 or 2.

			   How am I supposed to distinguish a) with p=0 (en)
			   or p=1 (de) from d)? Not to mention g) and h).
			*/

			if (0 == (n18[0] & 0x3F00)
			    && 0 != (n18[0] & 0x7F)) {
				unsigned int i;

				log ("... format 2 ignored.\n");

				if (0) {
					for (i = 0; i < 40; ++i)
						fprintf (stderr, "%02x ",
							 buffer[2 + i]);
					fputc ('\n', stderr);

					for (i = 0; i < 13; ++i)
						fprintf (stderr, "%05x ",
							 n18[i]);
					fputc ('\n', stderr);
				}

				break;
			}
		}

		/* fall through */
	}

	case 4:
	{
		enum ttx_page_function function;
		enum ttx_page_coding coding;

		/* X/28/0, M/29/0 Level 2.5 */
		/* X/28/4, M/29/4 Level 3.5 */

		if (n18[0] < 0)
			return FALSE;

		function = get_bits (&bs, 4);
		coding = get_bits (&bs, 3);

		log ("... %s, %s\n",
		     _vbi_ttx_page_function_name (function),
		     _vbi_ttx_page_coding_name (coding));

		if (!_vbi_ttx_page_function_valid (function)
		    || !_vbi_ttx_page_coding_valid (coding))
			break; /* undefined */

		if (28 == packet) {
			if (PAGE_FUNCTION_UNKNOWN == cp->function) {
				switch (function) {
				case PAGE_FUNCTION_ACI:
				case PAGE_FUNCTION_EPG:
				case PAGE_FUNCTION_DISCARD:
				case PAGE_FUNCTION_UNKNOWN:
					/* libzvbi private */
					assert (0);

				case PAGE_FUNCTION_LOP:
					/* ZDF and BR3 transmit GPOP 1EE/..
					   with 1/28/0 function 0 =
					   PAGE_FUNCTION_LOP, should be
					   PAGE_FUNCTION_GPOP. Cannot
					   trust function. */
					break;

				case PAGE_FUNCTION_DATA:
				case PAGE_FUNCTION_MOT:
				case PAGE_FUNCTION_MIP:
				case PAGE_FUNCTION_BTT:
				case PAGE_FUNCTION_MPT:
				case PAGE_FUNCTION_MPT_EX:
				case PAGE_FUNCTION_TRIGGER:
					cp->function = function;
					break;

				case PAGE_FUNCTION_GPOP:
				case PAGE_FUNCTION_POP:
				{
					cache_page temp;

					cache_page_copy (&temp, cp);
					convert_pop_page (cp, &temp, function);

					break;
				}

				case PAGE_FUNCTION_GDRCS:
				case PAGE_FUNCTION_DRCS:
				{
					unsigned int i;

					/* See comments in decode_packet_0. */
					for (i= 0; i < DRCS_PTUS_PER_PAGE; ++i)
						cp->data.drcs.mode[i] =
							DRCS_MODE_12_10_1;

					cp->function = function;

					break;
				}

				case PAGE_FUNCTION_AIT:
				{
					cache_page temp;

					cache_page_copy (&temp, cp);
					convert_ait_page (cp, &temp);

					break;
				}
				}
			} else if (function != cp->function) {
				/* Who's wrong here? */
				cp->function = PAGE_FUNCTION_DISCARD;
				return FALSE;
			}

			/* 9.4.2.2: Rest of packet applies to LOP only. */
			if (PAGE_FUNCTION_LOP != function)
				break;

			/* All remaining triplets must be correct. */
			if (err < 0)
				return FALSE;

			ext = &cp->data.ext_lop.ext;

			cp->x28_designations |= 1 << designation;
		} else {
			/* 9.5.1: Rest of packet applies if zero. */
			if (function | coding)
				break;

			if (err < 0)
				return FALSE;

			ext = &cache_network_magazine (td->network, cp->pgno)
				->extension;
		}

		if (4 == designation
		    && (ext->designations & (1 << 0))) {
			/* 9.4.2.2: X/28/0 takes precedence over X/28/4,
			   9.5.1: M/29/0 takes precendence over M/29/4,
			   for all but the colour map entry coding. */
			get_bits (&bs, 7 + 7 + 1 + 1 + 1 + 4);
		} else {
			unsigned int left_panel;
			unsigned int right_panel;
			unsigned int left_columns;

			ext->charset_code[0] = get_bits (&bs, 7);
			ext->charset_code[1] = get_bits (&bs, 7);

			left_panel = get_bits (&bs, 1);
			right_panel = get_bits (&bs, 1);

			/* 0 - panels required at Level 3.5 only,
			   1 - at 2.5 and 3.5
			   ignored. */
			get_bits (&bs, 1);

			left_columns = get_bits (&bs, 4);

			if (left_panel && 0 == left_columns)
				left_columns = 16;

			ext->fallback.left_panel_columns =
				left_columns & -left_panel;
			ext->fallback.right_panel_columns =
				(16 - left_columns) & -right_panel;
		}

		/* Color map for CLUTs 0 & 1 (desig. 4) or 2 & 3. */

		j = (4 == designation) ? 16 : 32;

		for (i = j - 16; i < j; ++i) {
			vbi_rgba col = get_bits (&bs, 12);

			if (8 == i) /* transparent */
				continue;

			col = VBI_RGBA ((col     ) & 15,
					(col >> 4) & 15,
					(col >> 8));

			ext->color_map[i] = col | (col << 4);
		}

		/* libzvbi private, invariable. */
		memcpy (ext->color_map + 32, default_color_map + 32,
			8 * sizeof (*ext->color_map));

		if (4 == designation
		    && (ext->designations & (1 << 0))) {
			/* 9.4.2.2: X/28/0 takes precedence over X/28/4,
			   9.5.1: M/29/0 takes precendence over M/29/4,
			   for all but the colour map entry coding. */
			get_bits (&bs, 5 + 5 + 1 + 3);
		} else {
			ext->def_border_color = get_bits (&bs, 5);
			ext->def_row_color = get_bits (&bs, 5);

			ext->fallback.black_bg_substitution =
				get_bits (&bs, 1);

			i = get_bits (&bs, 3); /* color table remapping */

			ext->foreground_clut = "\00\00\00\10\10\20\20\20"[i];
			ext->background_clut = "\00\10\20\10\20\10\20\30"[i];
		}

		ext->designations |= 1 << designation;

		if (0) {
			_vbi_ttx_extension_dump (ext, stderr);
		}

		break;
	}

	case 1: /* X/28/1, M/29/1 Level 3.5 DRCS CLUT */
	{
		if (err < 0)
			return FALSE;

		if (28 == packet) {
			ext = &cp->data.ext_lop.ext;

			cp->x28_designations |= 1 << 1;
		} else {
			ext = &cache_network_magazine (td->network, cp->pgno)
				->extension;
		}

		/* 9.4.4: "Compatibility, not for Level 2.5/3.5 decoders."
		   No more details, so we ignore this triplet. */
		++bs.triplet;

		for (i = 0; i < 8; ++i)
			ext->drcs_clut[i + 2] =
				vbi_rev8 (get_bits (&bs, 5)) >> 3;

		for (i = 0; i < 32; ++i)
			ext->drcs_clut[i + 10] =
				vbi_rev8 (get_bits (&bs, 5)) >> 3;

		ext->designations |= 1 << 1;

		break;
	}

	case 2:
	{
		/* CA key packet ignored. */

		break;
	}

	case 3:
	{
		enum ttx_page_function function;
		enum ttx_page_coding coding;

		/* X/28/3 Level 3.5 DRCS download page modes */

		if (29 == packet)
			break; /* M/29/3 undefined */

		if (n18[0] < 0)
			return FALSE;

		cp->x28_designations |= 1 << 3;

		function = get_bits (&bs, 4);
		coding = get_bits (&bs, 3);

		log ("... %s, %s\n",
		     _vbi_ttx_page_function_name (function),
		     _vbi_ttx_page_coding_name (coding));

		if (PAGE_FUNCTION_GDRCS != function
		    && PAGE_FUNCTION_DRCS != function)
			break; /* undefined */

		if (err < 0)
			return FALSE;

		if (PAGE_FUNCTION_UNKNOWN == cp->function) {
			cp->function = function;
		} else if (function != cp->function) {
			/* Who's wrong here? */
			cp->function = PAGE_FUNCTION_DISCARD;
			return FALSE;
		}

		get_bits (&bs, 11); /* reserved */

		for (i = 0; i < 48; ++i)
			cp->data.drcs.mode[i] = get_bits (&bs, 4);

		/* Rest reserved. */

		break;
	}

	case 5 ... 15:
		/* Undefined, ignored. */

		break;
	}

	return TRUE;
}

/* 9.8 Teletext packet 8/30, broadcast service data. */

#if 0
static void
cni_change			(vbi_ttx_decoder *	td,
				 vbi_cni_type		type,
				 unsigned int		cni,
				 double			timeout_min)
{
	cache_network *cn;
	double timeout;

	cn = td->network;
	timeout = 0.0;

	if (VBI_CNI_TYPE_VPS != type
	    && 0 != cn->network.cni_vps
	    && (cn->network.cni_vps
		!= vbi_convert_cni (VBI_CNI_TYPE_VPS, type, cni))) {
		/* We cannot say with certainty if cni and cni_vps belong
		   to the same network. If 0 == vbi_convert_cni() we cannot
		   convert, otherwise CNIs mismatch because the channel
		   changed, the network transmits wrong CNIs or the
		   conversion is incorrect.

		   After n seconds, if we cannot confirm the cni_vps or
		   receive a new one, we assume there was a channel change
		   and the new network does not transmit a cni_vps. */
		cn->confirm_cni_vps = cn->network.cni_vps;
		timeout = td->cni_vps_timeout;
	}

	if (VBI_CNI_TYPE_8301 != type
	    && 0 != cn->network.cni_8301
	    && (cn->network.cni_8301
		!= vbi_convert_cni (VBI_CNI_TYPE_8301, type, cni))) {
		cn->confirm_cni_8301 = cn->network.cni_8301;
		timeout = td->cni_830_timeout;
	}

	if (VBI_CNI_TYPE_8302 != type
	    && 0 != cn->network.cni_8302
	    && (cn->network.cni_8302
		!= vbi_convert_cni (VBI_CNI_TYPE_8302, type, cni))) {
		cn->confirm_cni_8302 = cn->network.cni_8302;
		timeout = td->cni_830_timeout;
	}

	if (timeout > 0.0) {
		td->virtual_reset (td, NULL,
				   td->timestamp + MAX (timeout, timeout_min));
	}
}

static vbi_bool
status_change			(vbi_ttx_decoder *	td,
				 const uint8_t		buffer[42])
{
	const vbi_ttx_charset *cs;
	char *title;

	cs = vbi_ttx_charset_from_code (0); /* XXX ok? */
	title = vbi_strndup_iconv_teletext (vbi_locale_codeset (), cs,
					     buffer + 22, 20,
					     /* repl_char */ '?');

	if (!title)
		return FALSE;

#if 1
	td = td;
#else
	{
		cache_network *cn;
		vbi_event e;

		cn = td->network;

		vbi_free (cn->program_info.title);
		cn->program_info.title = title;

		CLEAR (e);

		e.type = VBI_EVENT_PROG_INFO;
		e.network = &cn->network;
		e.timestamp = td->timestamp;
		e.ev.prog_info = &cn->program_info;

		_vbi_event_handler_list_send (&td->handlers, &e);
	}
#endif

	return TRUE;
}
#endif

static vbi_bool
decode_packet_8_30		(vbi_ttx_decoder *	td,
				 const uint8_t		buffer[42])
{
	int designation;

	td = td; /* unused */

	if ((designation = vbi_unham8 (buffer[2])) < 0)
		return FALSE;

	log ("Packet 8/30/%d\n", designation);

	if (designation > 4)
		return TRUE; /* undefined, ignored */

#if 0
	if (td->reset_time <= 0.0) {
		if (td->handlers.event_mask & VBI_EVENT_TTX_PAGE) {
			struct ttx_page_link pl;

			if (!unham_page_link (&pl, buffer + 3, 0))
				return FALSE;

			/* 9.8.1 Table 18 Note 2 */
			if (!NO_PAGE (pl.pgno)) {
				pl.function = PAGE_FUNCTION_LOP;
				td->network->initial_page = pl;
			}
		}

		if (td->handlers.event_mask & VBI_EVENT_PROG_INFO) {
			uint8_t *s;
			unsigned int i;
			int err;

			err = 0;

			for (i = 0; i < 20; ++i) {
				err |= vbi_unpar8 (buffer[i + 22]);

				if (0) {
					int c = buffer[i + 22];

					fprintf (stderr, "%2u %02x %02x %c\n",
						 i,
						 td->network->status[i],
						 c & 0x7F,
						 _vbi_to_ascii (c));
				}
			}

			if (err < 0)
				return FALSE;

			assert (20 == sizeof (td->network->status));

			s = td->network->status;

			if (0 != memcmp (s, buffer + 22, 20)) {
				if (status_change (td, buffer))
					memcpy (s, buffer + 22, 20);
			}
		}
	}
#endif

	if (designation < 2) {
#if 0
		unsigned int cni;
		cache_network *cn;

		/* 8/30 format 1 */

		if (!vbi_decode_teletext_8301_cni (&cni, buffer))
			return FALSE;

		cn = td->network;

		if (0 == cni) {
			/* Should probably ignore this. */
		} else if (cni == cn->network.cni_8301) {
			/* No CNI change, no channel change. */

			cn->confirm_cni_8301 = 0;

			if (0 == cn->confirm_cni_vps
			    && 0 == cn->confirm_cni_8302) {
				/* All CNIs valid, cancel reset requests. */
				td->virtual_reset (td, NULL, -1.0);
			}
		} else if (cni == cn->confirm_cni_8301) {
			/* The CNI is correct. */

			if (0 == cn->network.cni_8301) {
				/* First CNI, assume no channel change. */

				vbi_network_set_cni (&cn->network,
						      VBI_CNI_TYPE_8301, cni);

				cn->confirm_cni_8301 = 0;

				if (0 == cn->confirm_cni_vps
				    && 0 == cn->confirm_cni_8302) {
					/* All CNIs valid, cancel reset
					   requests. */
					td->virtual_reset (td, NULL, -1.0);
				}
			} else {
				vbi_network nk;
				cache_network *cn;

				/* Different CNI, channel change detected. */

				vbi_network_init (&nk);
				vbi_network_set_cni (&nk, VBI_CNI_TYPE_8301,
						     cni);

				cn = _vbi_cache_add_network
					(td->cache, &nk, td->videostd_set);

				td->virtual_reset (td, cn, 0.0 /* now */);

				cache_network_unref (cn);

				vbi_network_destroy (&nk);
			}

			network_event (td);
		} else {
			/* The packet 8/30 format 1 CNI is poorly error
			   protected. We accept this CNI after receiving it
			   twice in a row. */
			cn->confirm_cni_8301 = cni;

			if (0 == cn->network.cni_8301) {
				/* First CNI, channel change possible. */
				cni_change (td, VBI_CNI_TYPE_8301, cni,
					    td->cni_830_timeout);
			} else {
				/* Assume a channel change with unknown CNI
				   if we cannot confirm the new CNI or receive
				   the old CNI again within n seconds. */
				td->virtual_reset (td, NULL, td->timestamp
						   + td->cni_830_timeout);
			}
		}

		if (td->handlers.event_mask & VBI_EVENT_LOCAL_TIME) {
			vbi_event e;

			CLEAR (e);

			if (!vbi_decode_teletext_8301_local_time
			    (&e.ev.local_time.time,
			     &e.ev.local_time.gmtoff,
			     buffer))
				return FALSE;

			e.type		= VBI_EVENT_LOCAL_TIME;
			e.network	= &td->network->network;
			e.timestamp	= td->timestamp;

			_vbi_event_handler_list_send (&td->handlers, &e);
		}
#endif
	} else {
#if 0
		unsigned int cni;
		cache_network *cn;

		/* 8/30 format 2 */

		if (!vbi_decode_teletext_8302_cni (&cni, buffer))
			return FALSE;

		cn = td->network;

		if (0 == cni) {
			/* Should probably ignore this. */
		} else if (cni == cn->network.cni_8302) {
			/* No CNI change, no channel change. */

			cn->confirm_cni_8302 = 0; /* confirmed */

			if (0 == cn->confirm_cni_vps
			    && 0 == cn->confirm_cni_8301) {
				/* All CNIs valid, cancel reset requests. */
				td->virtual_reset (td, NULL, -1.0);
			}
		} else {
			if (0 == cn->network.cni_8302) {
				/* First CNI, channel change possible. */

				vbi_network_set_cni
					(&cn->network, VBI_CNI_TYPE_8302, cni);

				cn->confirm_cni_8302 = 0;

				cni_change (td, VBI_CNI_TYPE_8302, cni, 0.0);

				if (0 == cn->confirm_cni_vps
				    && 0 == cn->confirm_cni_8301) {
					/* All CNIs valid, cancel reset
					   requests. */
					td->virtual_reset (td, NULL, -1.0);
				}
			} else {
				vbi_network nk;
				cache_network *cn;

				/* Different CNI, channel change detected. */

				vbi_network_init (&nk);
				vbi_network_set_cni (&nk,
						     VBI_CNI_TYPE_8302, cni);

				cn = _vbi_cache_add_network
					(td->cache, &nk, td->videostd_set);

				td->virtual_reset (td, cn, 0.0 /* now */);

				cache_network_unref (cn);

				vbi_network_destroy (&nk);
			}

			network_event (td);
		}

		if (td->handlers.event_mask & VBI_EVENT_PROG_ID) {
			vbi_program_id pid;
			vbi_program_id *p;
			
			if (!vbi_decode_teletext_8302_pdc (&pid, buffer))
				return FALSE;

			p = &td->network->program_id[pid.channel];

			if (p->cni != pid.cni
			    || p->pil != pid.pil
			    || ((p->luf ^ pid.luf) |
				(p->mi ^ pid.mi) |
				(p->prf ^ pid.prf))
			    || p->pcs_audio != pid.pcs_audio
			    || p->pty != pid.pty) {
				vbi_event e;

				*p = pid;

				CLEAR (e);

				e.type		= VBI_EVENT_PROG_ID;
				e.network	= &td->network->network;
				e.timestamp	= td->timestamp;
				e.ev.prog_id	= p;

				_vbi_event_handler_list_send
					(&td->handlers, &e);
			}
		}
#endif
	}

	return TRUE;
}

/**
 * @param td Teletext decoder allocated with vbi_ttx_decoder_new().
 * @param buffer Teletext packet as defined for @c VBI_SLICED_TELETEXT_B,
 *   i.e. 42 bytes without clock run-in and framing code.
 * @param timestamp System time in seconds when the Teletext packet
 *   was captured.
 * 
 * Decodes a teletext packet and update the decoder state.
 * 
 * Return value:
 * @c FALSE if the packet contained incorrectable errors. 
 */
vbi_bool
vbi_ttx_decoder_feed		(vbi_ttx_decoder *	td,
				 const uint8_t		buffer[42],
				 const struct timeval *	capture_time,
				 int64_t		pts)
{
	vbi_bool success;
	cache_page *cp;
	int pmag;
	int mag0;
	int packet;

	success = FALSE;

	td->timestamp.sys = *capture_time;
	td->timestamp.pts = pts;

#if 0
	if (td->reset_time > 0
	    && timestamp >= td->reset_time) {
		cache_network *cn;

		/* Deferred reset. */

		cn = _vbi_cache_add_network
			(td->cache, NULL, td->videostd_set);

		td->virtual_reset (td, cn, 0.0 /* now */);
		cache_network_unref (cn);

		network_event (td);
	}
#endif

	if ((pmag = vbi_unham16p (buffer)) < 0)
		goto finish;

	mag0 = pmag & 7;
	packet = pmag >> 3;

	cp = &td->buffer[mag0];

	if (0) {
		unsigned int i;

		fprintf (stderr, "packet %xxx %d >",
			 (0 == mag0) ? 8 : mag0, packet);

		for (i = 0; i < 40; i++)
			fputc (_vbi_to_ascii (buffer[2 + i]), stderr);

		fprintf (stderr, "<\n");
	}

	if (packet < 30
	    && 0 == (td->handlers.event_mask
		     & (VBI_EVENT_TTX_PAGE /* |
			VBI_EVENT_TRIGGER |
			VBI_EVENT_PAGE_TYPE |
			VBI_EVENT_TOP_CHANGE */ ))) {
		success = TRUE;
		goto finish;
	}

	switch (packet) {
	case 0:
		/* Page header. */
		success = decode_packet_0 (td, cp, buffer,
					   (unsigned int) mag0);
		break;

	case 1 ... 25:
		/* Page body. */
		switch (cp->function) {
		case PAGE_FUNCTION_DISCARD:
			success = TRUE;
			break;

		case PAGE_FUNCTION_GPOP:
		case PAGE_FUNCTION_POP:
			success = decode_pop_packet (cp, buffer + 2,
						     (unsigned int) packet);
			break;

		case PAGE_FUNCTION_GDRCS:
		case PAGE_FUNCTION_DRCS:
			memcpy (cp->data.drcs.lop.raw[packet],
				buffer + 2, 40);
			success = TRUE;
			break;

		case PAGE_FUNCTION_AIT:
			success = decode_ait_packet (cp, buffer + 2,
						     (unsigned int) packet);
			break;

		case PAGE_FUNCTION_EPG:
			/* TODO */
			success = TRUE;
			break;

		case PAGE_FUNCTION_LOP:
		case PAGE_FUNCTION_TRIGGER:
		{
			unsigned int i;
			int err;

			err = 0;

			for (i = 0; i < 40; ++i)
				err |= vbi_unpar8 (buffer[2 + i]);

			if (err < 0)
				break;

			memcpy (cp->data.unknown.raw[packet], buffer + 2, 40);

			success = TRUE;

			break;
		}

		case PAGE_FUNCTION_MOT:
		case PAGE_FUNCTION_MIP:
		case PAGE_FUNCTION_BTT:
		case PAGE_FUNCTION_MPT:
		case PAGE_FUNCTION_MPT_EX:
		default:
			memcpy (cp->data.unknown.raw[packet], buffer + 2, 40);
			success = TRUE;
			break;
		}

		cp->lop_packets |= 1 << packet;

		break;

	case 26:
		/* Page enhancement packet. */
		success = decode_packet_26 (td, cp, buffer);
		break;

	case 27:
		/* Page linking. */
		success = decode_packet_27 (td, cp, buffer);
		break;

	case 28:
	case 29:
		/* Level 2.5/3.5 enhancement. */
		success = decode_packet_28_29 (td, cp, buffer,
					       (unsigned int) packet);
		break;

	case 30:
	case 31:
	{
		unsigned int channel;

		/* IDL packet (EN 300 708). */

		channel = pmag & 15;

		switch (channel) {
		case 0:
			success = decode_packet_8_30 (td, buffer);
			break;

		/* 1 ... 3	= Packet 1/30 ... 3/30 */
		/* 4		= Low bit rate audio */
		/* 5 ... 6	= Datavideo */
		/* 7		= Packet 7/30 */
		/* 8 ... 11	= IDL Format A (Packet 8/31, 1/31 ... 3/31) */
		/* 12		= Low bit rate audio */
		/* 13 ... 14	= Datavideo */
		/* 15		= Packet 7/31 */

		default:
			log ("IDL %u\n", channel);
			success = TRUE;
			break;
		}

		break;
	}

	default:
		assert (0);
	}

 finish:
	td->error_history = td->error_history * 2 + success;
	return success;
}

/**
 * @param td Teletext decoder allocated with vbi_ttx_decoder_new().
 * @param sliced Sliced VBI data.
 * @param n_lines Number of lines in the @a sliced array.
 * @param timestamp System time in seconds when the sliced data was
 *   captured.
 *
 * This function works like vbi_ttx_decoder_feed() but operates
 * on sliced VBI data and filters out @c VBI_SLICED_TELETEXT_B_625.
 */
vbi_bool
vbi_ttx_decoder_feed_frame
				(vbi_ttx_decoder *	td,
				 const vbi_sliced *	sliced,
				 unsigned int		n_lines,
				 const struct timeval *	capture_time,
				 int64_t		pts)
{
	const vbi_sliced *end;

	assert (NULL != td);
	assert (NULL != sliced);

	for (end = sliced + n_lines; sliced < end; ++sliced) {
		if (sliced->id & VBI_SLICED_TELETEXT_B_625) {
			if (!vbi_ttx_decoder_feed (td,
						   sliced->data,
						   capture_time,
						   pts))
				return FALSE;
		}
	}

	return TRUE;
}

/**
 */
vbi_bool
vbi_ttx_decoder_get_top_titles	(const vbi_ttx_decoder *td,
				 vbi_top_title **	tt_array,
				 unsigned int *		n_elements)
{
	_vbi_null_check (td, FALSE);
	_vbi_null_check (tt_array, FALSE);
	_vbi_null_check (n_elements, FALSE);

	return _vbi_cn_get_top_titles (td->network,
				       tt_array,
				       n_elements);
}

/**
 */
vbi_page *
vbi_ttx_decoder_get_page_va	(vbi_ttx_decoder *	td,
				 const vbi_network *	nk,
				 vbi_pgno		pgno,
				 vbi_subno		subno,
				 va_list		options)
{
	cache_network *cn;
	cache_page *cp;
	vbi_subno subno_mask;
	struct ttx_page *tp;

	cp = NULL;
	tp = NULL;

	if (NULL != nk) {
		cn = _vbi_cache_get_network (td->cache, nk);
		if (NULL == cn)
			goto failed;
	} else {
		cn = td->network;
	}

	subno_mask = -1;

	if (VBI_ANY_SUBNO == subno) {
		subno = 0;
		subno_mask = 0;
	}

	cp = _vbi_cache_get_page (td->cache, cn, pgno, subno, subno_mask);
	if (NULL == cp)
		goto failed;

	tp = _vbi_ttx_page_new ();
	if (NULL == tp)
		goto failed;

	if (!_vbi_ttx_page_from_cache_page_va (tp, cp, options))
		goto failed;

	cache_page_unref (cp);

	if (NULL != nk)
		cache_network_unref (cn);

	return &tp->pg;

 failed:
	if (NULL != tp)
		vbi_page_delete (&tp->pg);

	cache_page_unref (cp);

	if (NULL != nk)
		cache_network_unref (cn);

	return NULL;
}

/**
 */
vbi_page *
vbi_ttx_decoder_get_page	(vbi_ttx_decoder *	td,
				 const vbi_network *	nk,
				 vbi_pgno		pgno,
				 vbi_subno		subno,
				 ...)
{
	vbi_page *pg;
	va_list options;

	va_start (options, subno);
	pg = vbi_ttx_decoder_get_page_va (td, nk, pgno, subno, options);
	va_end (options);

	return pg;
}

/**
 * @internal
 * @param td Teletext decoder allocated with vbi_ttx_decoder_new().
 *
 * Call this when Teletext packets might have been lost. After a
 * channel change call vbi_ttx_decoder_reset() instead.
 */
vbi_bool
_vbi_ttx_decoder_resync	(vbi_ttx_decoder *	td)
{
	unsigned int i;

	/* Discard all in progress pages. */

	for (i = 0; i < N_ELEMENTS (td->buffer); ++i)
		td->buffer[i].function = PAGE_FUNCTION_DISCARD;

	td->current = NULL;

#if 0 /* TODO */
	vbi_pfc_demux_reset (&td->epg_stream[0]);
	vbi_pfc_demux_reset (&td->epg_stream[1]);
#endif

	return TRUE;
}

/** @internal */
void
cache_network_dump_teletext	(const cache_network *	cn,
				 FILE *			fp)
{
	unsigned int i;
	vbi_pgno pgno;

	_vbi_ttx_page_link_dump (&cn->initial_page, fp);

	for (i = 0; i < N_ELEMENTS (cn->btt_link); ++i) {
		fprintf (fp, "\nbtt_link[%u]=", i);
		_vbi_ttx_page_link_dump (cn->btt_link + i, fp);
	}

	fputs ("\nstatus=\"", fp);

	for (i = 0; i < N_ELEMENTS (cn->status); ++i)
		fputc (_vbi_to_ascii (cn->status[i]), fp);

	fputs ("\"\npage_stat=\n", fp);

	for (pgno = 0x100; pgno < 0x8FF; pgno += 8) {
		vbi_pgno unit;

		for (unit = 0; unit < 8; ++unit) {
			const struct ttx_page_stat *ps;

			ps = cache_network_const_page_stat (cn, pgno + unit);

			fprintf (fp, "%02x:%02x:%04x:%2u/%2u:%02x-%02x ",
				 ps->page_type,
				 ps->charset_code,
				 ps->subcode,
				 ps->n_subpages,
				 ps->max_subpages,
				 ps->subno_min,
				 ps->subno_max);
		}

		fputc ('\n', fp);
	}

	fputc ('\n', fp);
}

static void
ttx_extension_init		(struct ttx_extension *	ext)
{
	unsigned int i;

	CLEAR (*ext);

	/* EN 300 706 Annex A.5. */
	ext->def_border_color	= VBI_BLACK;
	ext->def_row_color	= VBI_BLACK;

	for (i = 0; i < 8; ++i)
		ext->drcs_clut[2 + i] = i & 3;

	for (i = 0; i < 32; ++i)
		ext->drcs_clut[2 + 8 + i] = i & 15;

	memcpy (ext->color_map, default_color_map,
		sizeof (ext->color_map));
}

static void
ttx_magazine_init		(struct ttx_magazine *	mag)
{
	ttx_extension_init (&mag->extension);

	/* Valid range 0 ... 7. */
	memset (mag->pop_lut, -1, sizeof (mag->pop_lut));
	memset (mag->drcs_lut, -1, sizeof (mag->pop_lut));

	/* NO_PAGE (pgno): (pgno & 0xFF) == 0xFF. */
	memset (mag->pop_link, -1, sizeof (mag->pop_link));
	memset (mag->drcs_link, -1, sizeof (mag->drcs_link));
}

/**
 * @internal
 *
 * Provides default magazine parameters to be used at Level 1 and 1.5.
 * Remember editors can redefine most of the magazine parameters at
 * Level 2.5 and 3.5.
 */
const struct ttx_magazine *
_vbi_ttx_default_magazine	(void)
{
	static struct ttx_magazine default_magazine;

	if (!NO_PAGE (default_magazine.pop_link[0][0].pgno))
		ttx_magazine_init (&default_magazine);

	return &default_magazine;
}

static void
ttx_page_stat_init		(struct ttx_page_stat *	ps)
{
	CLEAR (*ps);

	ps->page_type		= VBI_UNKNOWN_PAGE;
	ps->charset_code	= 0xFF;
	ps->subcode		= SUBCODE_UNKNOWN;
}

/** @internal */
void
cache_network_destroy_teletext	(cache_network *	cn)
{
	/* Nothing to do. */

	cn = cn;
}

/**
 * @internal
 * @param cn cache_network structure to be initialized.
 *
 * Initializes the Teletext fields of a cache_network structure.
 */
void
cache_network_init_teletext	(cache_network *	cn)
{
	unsigned int i;

	/* D.3: In absence of packet 8/30/0 ... 4 assume page 100. */
	cn->initial_page.function	= PAGE_FUNCTION_LOP;
	cn->initial_page.pgno		= 0x100;
	cn->initial_page.subno		= VBI_ANY_SUBNO;

	for (i = 0; i < N_ELEMENTS (cn->_magazines); ++i)
		ttx_magazine_init (&cn->_magazines[i]);

	for (i = 0; i < N_ELEMENTS (cn->_pages); ++i)
		ttx_page_stat_init (&cn->_pages[i]);

	/* NO_PAGE (pgno): (pgno & 0xFF) == 0xFF. */
	memset (cn->btt_link, -1, sizeof (cn->btt_link));

	CLEAR (cn->status);

	cn->have_top = FALSE;
}

/**
 * @internal
 * @param td Teletext decoder allocated with vbi_ttx_decoder_new().
 * @param cn New network, can be @c NULL if 0.0 != time.
 * @param time Deferred reset when time is greater than
 *   vbi_ttx_decoder_feed() timestamp. Pass a negative number to
 *   cancel a deferred reset, 0.0 to reset immediately.
 *
 * Internal reset function, called via td->virtual_reset().
 */
static void
internal_reset			(vbi_ttx_decoder *	td,
				 cache_network *	cn,
				 double			time)
{
	assert (NULL != td);

#if 0
	if (0)
		fprintf (stderr, "teletext reset %f: %f -> %f\n",
			 td->timestamp, td->reset_time, time);

	if (time <= 0.0 /* reset now or cancel deferred reset */
	    || time > td->reset_time)
		td->reset_time = time;
#endif

	if (0.0 != time) {
		/* Don't reset now. */
		return;
	}

	assert (NULL != cn);

	cache_network_unref (td->network);
	td->network = cache_network_ref (cn);

#if 0 /* TODO */
	td->epg_stream[0].block.pgno = 0;
	td->epg_stream[1].block.pgno = 0;
#endif

	td->header_page.pgno = 0;
	CLEAR (td->header);

	_vbi_ttx_decoder_resync (td);

	if (internal_reset == td->virtual_reset) {
		vbi_event e;

		CLEAR (e);

		e.type		= VBI_EVENT_RESET;
		e.network	= &td->network->network;

		_vbi_event_handler_list_send (&td->handlers, &e);
	}
}

/**
 * @param td Teletext decoder allocated with vbi_ttx_decoder_new().
 * @param callback Function to be called on events.
 * @param user_data User pointer passed through to the @a callback function.
 * 
 * Removes an event handler from the Teletext decoder, if a handler with
 * this @a callback and @a user_data has been registered. You can
 * safely call this function from a handler removing itself or another
 * handler.
 */
vbi_bool
vbi_ttx_decoder_remove_event_handler
				(vbi_ttx_decoder *	td,
				 vbi_event_cb *		callback,
				 void *			user_data)
{
#if 0
	vbi_cache_remove_event_handler	(td->cache,
					 callback,
					 user_data);
#endif
	_vbi_event_handler_list_remove_by_callback (&td->handlers,
						    callback,
						    user_data);

	return TRUE;
}

/**
 * @param td Teletext decoder allocated with vbi_ttx_decoder_new().
 * @param event_mask Set of events (@c VBI_EVENT_) the handler is
 *   waiting for, can be -1 for all and 0 for none.
 * @param callback Function to be called on events by
 *   vbi_ttx_decoder_feed() and other vbi_teletext functions as noted.
 * @param user_data User pointer passed through to the @a callback
 *   function.
 * 
 * Adds a new event handler to the Teletext decoder. When the @a callback
 * with @a user_data is already registered the function merely changes the
 * set of events it will receive in the future. When the @a event_mask is
 * empty the function does nothing or removes an already registered event
 * handler. You can safely call this function from an event handler.
 *
 * Any number of handlers can be added, also different handlers for the
 * same event which will be called in registration order.
 *
 * @returns
 * @c FALSE on failure (out of memory).
 */
vbi_bool
vbi_ttx_decoder_add_event_handler
				(vbi_ttx_decoder *	td,
				 vbi_event_mask	event_mask,
				 vbi_event_cb *		callback,
				 void *			user_data)
{
	vbi_event_mask ttx_mask;
	vbi_event_mask add_mask;
	vbi_event_mask rem_mask;

#if 0
	if (!vbi_cache_add_event_handler (td->cache,
					  event_mask,
					  callback,
					  user_data))
		return FALSE;
#endif
	ttx_mask = event_mask & (VBI_EVENT_CLOSE |
				 VBI_EVENT_RESET |
				 VBI_EVENT_TTX_PAGE);

	add_mask = ttx_mask & ~td->handlers.event_mask;
	rem_mask = td->handlers.event_mask & ~event_mask;

#if 0
	if (rem_mask & VBI_EVENT_TRIGGER) {
		/* Don't fire old triggers. */
		_vbi_trigger_list_delete (&td->triggers);
	}
#endif

	if (0 == ttx_mask) {
		return TRUE;
	}

	if (NULL != _vbi_event_handler_list_add (&td->handlers,
						  ttx_mask,
						  callback,
						  user_data)) {
		if (add_mask & (VBI_EVENT_TTX_PAGE /* |
				VBI_EVENT_TRIGGER */)) {
			/* XXX is this really necessary? */
			_vbi_ttx_decoder_resync (td);
		}

		return TRUE;
	} else {
#if 0
		vbi_cache_remove_event_handler	(td->cache,
						 callback,
						 user_data);
#endif
	}

	return FALSE;
}

/**
 * @param td Teletext decoder allocated with vbi_ttx_decoder_new().
 * @param nk Identifies the new network, can be @c NULL.
 *
 * Resets the Teletext decoder, useful for example after a channel
 * change. This function sends a @c VBI_EVENT_RESET.
 *
 * You can pass a vbi_network structure to identify the new network in
 * advance, before the decoder receives a CNI (if any is transmitted).
 */
vbi_bool
vbi_ttx_decoder_reset		(vbi_ttx_decoder *	td,
				 const vbi_network *	nk)
{
	cache_network *cn;

	_vbi_null_check (td, FALSE);

	cn = _vbi_cache_add_network (td->cache, nk,
				     VBI_VIDEOSTD_SET_625_50);

	td->virtual_reset (td, cn, 0.0 /* now */);

	cache_network_unref (cn);

	return TRUE;
}

/**
 * @internal
 * @param td Initialized Teletext decoder.
 *
 * Frees all resources associated with @a td, except the structure
 * itself. This function sends a @c VBI_EVENT_CLOSE.
 */
vbi_bool
_vbi_ttx_decoder_destroy	(vbi_ttx_decoder *	td)
{
	vbi_event e;

	_vbi_null_check (td, FALSE);

	CLEAR (e);
	e.type = VBI_EVENT_CLOSE;

	_vbi_event_handler_list_send (&td->handlers, &e);

	_vbi_event_handler_list_destroy (&td->handlers);

#if 0
	_vbi_trigger_list_delete (&td->triggers);
#endif

	cache_network_unref (td->network);

	/* Delete if we hold the last reference. */
	vbi_cache_unref (td->cache);

	CLEAR (*td);

	return TRUE;
}

/**
 * @internal
 * @param td Teletext decoder to be initialized.
 * @param ca Cache to be used by this Teletext decoder, can be @c NULL.
 *   To allocate a cache call vbi_cache_new(). Caches have a reference
 *   counter, you can vbi_cache_unref() after calling this function.
 * @param nk Initial network (see vbi_ttx_decoder_reset()),
 *   can be @c NULL.
 *
 * Initializes a Teletext decoder structure.
 *
 * @returns
 * @c FALSE on failure (out of memory).
 */
vbi_bool
_vbi_ttx_decoder_init		(vbi_ttx_decoder *	td,
				 vbi_cache *		ca,
				 const vbi_network *	nk)
{
	cache_network *cn;

	_vbi_null_check (td, FALSE);

	CLEAR (*td);

	if (NULL != ca)
		td->cache = vbi_cache_ref (ca);
	else
		td->cache = vbi_cache_new ();

	if (NULL == td->cache)
		return FALSE;

	td->virtual_reset = internal_reset;

#if 0
	td->cni_830_timeout = 2.5; /* sec */
	td->cni_vps_timeout = 5 / 25.0; /* sec */
#endif

	_vbi_event_handler_list_init (&td->handlers);

	cn = _vbi_cache_add_network (td->cache, nk,
				     VBI_VIDEOSTD_SET_625_50);
	internal_reset (td, cn, 0.0 /* now */);
	cache_network_unref (cn);

	_vbi_timestamp_reset (&td->timestamp);

	return TRUE;
}

static void
internal_delete			(vbi_ttx_decoder *	td)
{
	assert (NULL != td);

	_vbi_ttx_decoder_destroy (td);

	vbi_free (td);
}

/**
 * @param td Teletext decoder allocated with vbi_ttx_decoder_new(),
 *   can be @c NULL.
 *
 * Frees all resources associated with @a td. This function sends a
 * @c VBI_EVENT_CLOSE.
 */
void
vbi_ttx_decoder_delete		(vbi_ttx_decoder *	td)
{
	if (NULL == td)
		return;

	if (NULL != td->virtual_delete)
		td->virtual_delete (td);
}

/**
 * @param ca Cache to be used by this Teletext decoder. If @a ca is @c
 *   NULL the function allocates a new cache. To allocate a cache
 *   yourself call vbi_cache_new(). Caches have a reference counter,
 *   you can vbi_cache_unref() after calling this function.
 *
 * Allocates a new Teletext (EN 300 706) decoder. Decoded data is
 * available through the following functions:
 * - vbi_ttx_decoder_get_page()
 *
 * To be notified when new data is available call
 * vbi_ttx_decoder_add_event_handler().
 *
 * @returns
 * Pointer to newly allocated Teletext decoder which must be
 * freed with vbi_ttx_decoder_delete() when done.
 * @c NULL on failure (out of memory).
 */
vbi_ttx_decoder *
vbi_ttx_decoder_new		(vbi_cache *		ca,
				 const vbi_network *	nk)
{
	vbi_ttx_decoder *td;

	td = vbi_malloc (sizeof (*td));
	if (unlikely (NULL == td)) {
		return NULL;
	}

	if (likely (_vbi_ttx_decoder_init (td, ca, nk))) {
		td->virtual_delete = internal_delete;
	} else {
		int saved_errno = errno;

		vbi_free (td);
		td = NULL;

		errno = saved_errno;
	}

	return td;
}

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
