/*
 *  libzvbi - Network identification
 *
 *  Copyright (C) 2000-2004 Michael H. Schimek
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

/* $Id: network.c,v 1.1.2.8 2004-07-16 00:08:18 mschimek Exp $ */

#include "../config.h"

#include <stdlib.h>
#include <ctype.h>		/* isdigit() */
#include <assert.h>
#include "misc.h"		/* CLEAR() */
#include "bcd.h"		/* vbi_is_bcd(), vbi_bcd2dec() */
#include "conv.h"		/* _vbi_strdup_locale_utf8() */
#include "network.h"
#include "network-table.h"

/**
 * @param type CNI type.
 *
 * Returns the name of a CNI type, for example VBI_CNI_TYPE_PDC_A ->
 * "PDC_A". This is mainly intended for debugging.
 * 
 * @return
 * Static ASCII string, NULL if @a type is invalid.
 */
const char *
vbi_cni_type_name		(vbi_cni_type		type)
{
	switch (type) {

#undef CASE
#define CASE(type) case VBI_CNI_TYPE_##type : return #type ;

	CASE (NONE)
	CASE (VPS)
	CASE (8301)
	CASE (8302)
	CASE (PDC_A)
	CASE (PDC_B)

	}

	return NULL;
}

static unsigned int
cni_pdc_a_to_vps		(unsigned int		cni)
{
	unsigned int n;

	/* Relation guessed from values observed
	   in DE and AT. Is this defined anywhere? */

	switch (cni >> 12) {
	case 0x1A: /* Austria */
	case 0x1D: /* Germany */
		if (!vbi_is_bcd (cni & 0xFFF))
			return 0;

		n = vbi_bcd2dec (cni & 0xFFF);

		switch (n) {
		case 100 ... 163:
			cni = ((cni >> 4) & 0xF00) + n + 0xC0 - 100;
			break;

		case 200 ... 263: /* in DE */
			cni = ((cni >> 4) & 0xF00) + n + 0x80 - 200;
			break;

		default:
			return 0;
		}

		break;

	default:
		return 0;
	}

	return cni;
}

static unsigned int
cni_vps_to_pdc_a		(unsigned int		cni)
{
	switch (cni >> 8) {
	case 0xA: /* Austria */
	case 0xD: /* Germany */
		switch (cni & 0xFF) {
		case 0xC0 ... 0xFF:
			cni = ((cni << 4) & 0xF000) + 0x10000
				+ vbi_dec2bcd ((cni & 0xFF) - 0xC0 + 100);
			break;

		case 0x80 ... 0xBF:
			cni = ((cni << 4) & 0xF000) + 0x10000
				+ vbi_dec2bcd ((cni & 0xFF) - 0x80 + 200);
			break;

		default:
			return 0;
		}

		break;

	default:
		return 0;
	}

	return cni;
}

static const struct network *
cni_lookup			(vbi_cni_type		type,
				 unsigned int		cni,
				 const char *		caller)
{
	const struct network *p;
	const struct network *network_table_end =
		network_table + N_ELEMENTS (network_table);

	/* TODO binary search. */

	if (0 == cni)
		return NULL;

	switch (type) {
	case VBI_CNI_TYPE_8301:
		for (p = network_table; p < network_table_end; ++p)
			if (p->cni_8301 == cni) {
				return p;
			}
		break;

	case VBI_CNI_TYPE_8302:
		for (p = network_table; p < network_table_end; ++p)
			if (p->cni_8302 == cni) {
				return p;
			}

		cni &= 0x0FFF;

		/* fall through */

	case VBI_CNI_TYPE_VPS:
		for (p = network_table; p < network_table_end; ++p)
			if (p->cni_vps == cni) {
				return p;
			}
		break;

	case VBI_CNI_TYPE_PDC_A:
	{
		unsigned int cni_vps;

		cni_vps = cni_pdc_a_to_vps (cni);

		if (0 == cni_vps)
			return NULL;

		for (p = network_table; p < network_table_end; ++p)
			if (p->cni_vps == cni_vps) {
				return p;
			}

		break;
	}

	case VBI_CNI_TYPE_PDC_B:
		for (p = network_table; p < network_table_end; ++p)
			if (p->cni_pdc_b == cni) {
				return p;
			}

		/* try code | 0x0080 & 0x0FFF -> VPS ? */

		break;

	default:
		vbi_log_printf (VBI_DEBUG, caller,
				"Unknown CNI type %u\n", type);
		break;
	}

	return NULL;
}

/**
 * @param to_type Type of CNI to convert to.
 * @param from_type Type of CNI to convert from.
 * @param cni CNI to convert.
 *
 * Converts a CNI from one type to another.
 *
 * @returns
 * Converted CNI or 0 if conversion is not possible.
 */
unsigned int
vbi_convert_cni			(vbi_cni_type		to_type,
				 vbi_cni_type		from_type,
				 unsigned int		cni)
{
	const struct network *p;

	if (!(p = cni_lookup (from_type, cni, __FUNCTION__)))
		return 0;

	switch (to_type) {
	case VBI_CNI_TYPE_VPS:
		return p->cni_vps;

	case VBI_CNI_TYPE_8301:
		return p->cni_8301;

	case VBI_CNI_TYPE_8302:
		return p->cni_8302;

	case VBI_CNI_TYPE_PDC_A:
		return cni_vps_to_pdc_a (p->cni_vps);

	case VBI_CNI_TYPE_PDC_B:
		return p->cni_pdc_b;

	default:
		vbi_log_printf (VBI_DEBUG, __FUNCTION__,
				"Unknown CNI type %u\n", to_type);
		break;
	}

	return 0;
}

/** @internal */
void
_vbi_network_dump		(const vbi_network *	nk,
				 FILE *			fp)
{
	assert (NULL != nk);
	assert (NULL != fp);

	printf ("'%s' call_sign=%s cni=%x/%x/%x/%x/%x country=%s",
		nk->name ? nk->name : "unknown",
		nk->call_sign[0] ? nk->call_sign : "unknown",
		nk->cni_vps, nk->cni_8301, nk->cni_8302,
		nk->cni_pdc_a, nk->cni_pdc_b,
		nk->country_code[0] ? nk->country_code : "unknown");
}

/**
 */
vbi_bool
vbi_network_set_name		(vbi_network *		nk,
				 const char *		name)
{
	char *name1;

	assert (NULL != nk);

	if (!(name1 = strdup (name)))
		return FALSE;

	free (nk->name);
	nk->name = name1;

	return TRUE;
}

/**
 */
vbi_bool
_vbi_network_set_name_from_ttx_header
				(vbi_network *		nk,
				 const uint8_t		buffer[40])
{
	unsigned int i;

	assert (NULL != nk);
	assert (NULL != buffer);

	for (i = 0; i < N_ELEMENTS (ttx_header_table); ++i) {
		const uint8_t *s1;
		const uint8_t *s2;
		uint8_t c1;
		uint8_t c2;
		char *name;

		s1 = ttx_header_table[i].header;
		s2 = buffer + 8;

		while (0 != (c1 = *s1) && s2 < &buffer[40]) {
			switch (c1) {
			case '?':
				break;

			case '#':
				if (!isdigit (*s2 & 0x7F))
					goto next;
				break;

			default:
				if (((c2 = *s2) & 0x7F) <= 0x20) {
					if (0x20 != c1)
						goto next;
					else
						break;
				} else if (0 != ((c1 ^ c2) & 0x7F)) {
					goto next;
				}
				break;
			}

			++s1;
			++s2;
		}

		name = _vbi_strdup_locale_utf8 (ttx_header_table[i].name);

		if (!name)
			return FALSE;

		free (nk->name);
		nk->name = name;

		return TRUE;

	next:
		;
	}

	return FALSE;
}

/**
 */
vbi_bool
vbi_network_set_cni		(vbi_network *		nk,
				 vbi_cni_type		type,
				 unsigned int		cni)
{
	const struct network *p;
	char *name;

	assert (NULL != nk);

	switch (type) {
	case VBI_CNI_TYPE_VPS:
		nk->cni_vps = cni;
		break;

	case VBI_CNI_TYPE_8301:
		nk->cni_8301 = cni;
		break;

	case VBI_CNI_TYPE_8302:
		nk->cni_8302 = cni;
		break;

	case VBI_CNI_TYPE_PDC_A:
		nk->cni_pdc_a = cni;
		break;

	case VBI_CNI_TYPE_PDC_B:
		nk->cni_pdc_b = cni;
		break;

	default:
		vbi_log_printf (VBI_DEBUG, __FUNCTION__,
				"Unknown CNI type %u\n", type);
	}

	if (!(p = cni_lookup (type, cni, __FUNCTION__)))
		return FALSE;

	/* Keep in mind our table may be wrong. */

	if (p->cni_vps && nk->cni_vps
	    && p->cni_vps != nk->cni_vps)
		return FALSE;

	if (p->cni_8301 && nk->cni_8301
	    && p->cni_8301 != nk->cni_8301)
		return FALSE;

	if (p->cni_8302 && nk->cni_8302
	    && p->cni_8302 != nk->cni_8302)
		return FALSE;

	if (!(name = _vbi_strdup_locale_utf8 (p->name)))
		return FALSE;

	free (nk->name);
	nk->name = name;

	nk->cni_vps	= p->cni_vps;
	nk->cni_8301	= p->cni_8301;
	nk->cni_8302	= p->cni_8302;

	if (0 == nk->cni_pdc_a)
		nk->cni_pdc_a = cni_vps_to_pdc_a (p->cni_vps);

	if (0 == nk->cni_pdc_b)
		nk->cni_pdc_b = p->cni_pdc_b;

	if (0 == nk->country_code[0]) {
		assert (p->country < N_ELEMENTS (country_table));

		_vbi_strlcpy (nk->country_code,
			      country_table[p->country].country_code,
			      sizeof (nk->country_code));
	}

	return TRUE;
}

/**
 */
vbi_bool
vbi_network_set_call_sign	(vbi_network *		nk,
				 const char *		call_sign)
{
	assert (NULL != nk);
	assert (NULL != call_sign);

	_vbi_strlcpy (nk->call_sign, call_sign, sizeof (nk->call_sign));

	if (0 == nk->country_code[0]) {
		const char *country_code;

		country_code = "";

		/* See http://www.fcc.gov/cgb/statid.html */
		switch (call_sign[0]) {
		case 'A':
			switch (call_sign[1]) {
			case 'A' ... 'F':
				country_code = "US";
				break;
			}
			break;

		case 'K':
		case 'W':
		case 'N':
			country_code = "US";
			break;

		case 'C':
			switch (call_sign[1]) {
			case 'F' ... 'K':
			case 'Y' ... 'Z':
				country_code = "CA";
				break;
			}
			break;

		case 'V':
			switch (call_sign[1]) {
			case 'A' ... 'G':
			case 'O':
			case 'X' ... 'Y':
				country_code = "CA";
				break;
			}
			break;

		case 'X':
			switch (call_sign[1]) {
			case 'J' ... 'O':
				country_code = "CA";
				break;
			}
			break;
		}

		_vbi_strlcpy (nk->country_code,
			      country_code,
			      sizeof (nk->country_code));
	}

	return TRUE;
}

/**
 * @param nk vbi_network structure initialized with vbi_network_init()
 *   or vbi_network_copy().
 *
 * Frees all resources associated with @a nk, except the structure itself.
 */
void
vbi_network_destroy		(vbi_network *		nk)
{
	vbi_network_reset (nk);
}

/**
 * @param nk Initialized vbi_network structure.
 *
 * Resets all fields to zero.
 */
void
vbi_network_reset		(vbi_network *		nk)
{
	assert (NULL != nk);

	free (nk->name);

	CLEAR (*nk);
}

/**
 * @param dst vbi_network structure to initialize.
 * @param src Initialized vbi_network structure, can be @c NULL.
 *
 * Initializes all fields of @a dst by copying from @a src. Does
 * nothing if @a dst and @a src are the same.
 *
 * @returns
 * @c FALSE on failure (out of memory).
 */
vbi_bool
vbi_network_copy		(vbi_network *		dst,
				 const vbi_network *	src)
{
	assert (NULL != dst);

	if (!src) {
		CLEAR (*dst);
	} else if (dst != src) {
		char *name;

		name = NULL;

		if (src->name && !(name = strdup (src->name)))
			return FALSE;

		memcpy (dst, src, sizeof (*dst));

		dst->name = name;
	}

	return TRUE;
}

/**
 * @param nk vbi_network structure to initialize.
 *
 * Initializes all fields of @a nk to zero.
 *
 * @returns
 * @c TRUE.
 */
vbi_bool
vbi_network_init		(vbi_network *		nk)
{
	return vbi_network_copy (nk, NULL);
}

/**
 */
extern void
vbi_network_array_delete	(vbi_network *		nk,
				 unsigned int		nk_size)
{
	unsigned int i;

	if (NULL == nk || 0 == nk_size)
		return;

	for (i = 0; i < nk_size; ++i)
		vbi_network_destroy (nk + i);

	free (nk);
}
