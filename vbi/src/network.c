/*
 *  libzvbi - Network identification
 *
 *  Copyright (C) 2000, 2001, 2002, 2003, 2004 Michael H. Schimek
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

/* $Id: network.c,v 1.1.2.13 2006-05-18 16:49:19 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <ctype.h>		/* isdigit() */

#include "misc.h"		/* CLEAR() */
#include "bcd.h"		/* vbi3_is_bcd(), vbi3_bcd2bin() */
#include "conv.h"		/* _vbi3_strdup_locale_utf8() */
#include "network.h"
#include "network-table.h"

/**
 * @param type CNI type.
 *
 * Returns the name of a CNI type, for example VBI3_CNI_TYPE_PDC_A ->
 * "PDC_A". This is mainly intended for debugging.
 * 
 * @return
 * Static ASCII string, NULL if @a type is invalid.
 */
const char *
vbi3_cni_type_name		(vbi3_cni_type		type)
{
	switch (type) {

#undef CASE
#define CASE(type) case VBI3_CNI_TYPE_##type : return #type ;

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
cni_pdc_a_to_8302		(unsigned int		in_cni)
{
	unsigned int out_cni;
	unsigned int n;

	/* Relation guessed from values observed
	   in DE (misc. networks), AT (ORF), IT (RAI 1).
	   Is this defined anywhere? */

	if (!vbi3_is_bcd ((int) in_cni & 0xFFF))
		return 0;

	out_cni = (in_cni >> 4) & 0xFF00;

	n = vbi3_bcd2bin ((int) in_cni & 0xFFF);

	switch (out_cni) {
	case 0x1500: /* Italy */
		/* There are some oddities: RAI 1 lists Rete 4 as 15105,
		   All Music as 15109, LA7 as 3B101 and MTV as 3B102.
		   TR 101 231 says 1505 is Canale Italia and 1509 is
		   Telenova. All Music is not in TR. 3B is the country
		   code of Monaco, which has no entries in TR, but LA7
		   and MTV Italia are listed in the Italian section,
		   with Italian 8/30-1 CNIs. These exceptions are noted
		   in networks.xml. */
		switch (n) {
		case 100 ... 163:
			out_cni |= n - 100;
			break;

		default:
			return 0;
		}

		break;

	case 0x1A00: /* Austria */
	case 0x1D00: /* Germany */
		switch (n) {
		case 100 ... 163:
			out_cni |= n + 0xC0 - 100;
			break;

		case 200 ... 263: /* only seen in DE */
			out_cni |= n + 0x80 - 200;
			break;

		default:
			return 0;
		}

		break;
	}

	return out_cni;
}

static unsigned int
cni_8302_to_pdc_a		(unsigned int		in_cni)
{
	unsigned int out_cni;

	out_cni = (in_cni & 0xFF00) << 4;

	switch (out_cni) {
	case 0x15000: /* Italy */
		switch (in_cni & 0xFF) {
		case 0x00 ... 0x3F:
			out_cni |= vbi3_bin2bcd ((int)(in_cni & 0xFF) + 100);
			break;

		default:
			return 0;
		}

	case 0x1A000: /* Austria */
	case 0x1D000: /* Germany */
		switch (in_cni & 0xFF) {
		case 0xC0 ... 0xFF:
			out_cni |= vbi3_bin2bcd ((int)(in_cni & 0xFF)
						 - 0xC0 + 100);
			break;

		case 0x80 ... 0xBF:
			out_cni |= vbi3_bin2bcd ((int)(in_cni & 0xFF)
						 - 0x80 + 200);
			break;

		default:
			return 0;
		}

		break;

	default:
		return 0;
	}

	return out_cni;
}

static unsigned int
cni_vps_to_8302			(unsigned int		cni)
{
	/* VPS has room for only the 4 lsb
	   of the 8/30-2 country code. */

	switch (cni & 0xF00) {
	case 0x400: /* Switzerland */
		return 0x2000 | cni;

	case 0x700: /* Ukraine */
		return 0x4000 | cni;

	case 0xA00: /* Austria */
	case 0xD00: /* Germany */
		return 0x1000 | cni;

	default:
		/* Other countries do not use VPS.
		   FIXME take this info from networks.xml. */
		break;
	}

	return 0;
}

static unsigned int
cni_8302_to_vps			(unsigned int		cni)
{
	return cni & 0xFFF;
}

static unsigned int
cni_pdc_a_to_vps		(unsigned int		cni)
{
	return cni_8302_to_vps (cni_pdc_a_to_8302 (cni));
}

static unsigned int
cni_vps_to_pdc_a		(unsigned int		cni)
{
	return cni_8302_to_pdc_a (cni_vps_to_8302 (cni));
}

static const struct network *
cni_lookup			(vbi3_cni_type		type,
				 unsigned int		cni)
{
	const struct network *p;
	const struct network *network_table_end =
		network_table + N_ELEMENTS (network_table);

	/* TODO binary search. */

	if (0 == cni)
		return NULL;

	switch (type) {
	case VBI3_CNI_TYPE_8301:
		for (p = network_table; p < network_table_end; ++p)
			if (p->cni_8301 == cni) {
				return p;
			}
		break;

	case VBI3_CNI_TYPE_8302:
		for (p = network_table; p < network_table_end; ++p)
			if (p->cni_8302 == cni) {
				return p;
			}

		cni &= 0x0FFF;

		/* fall through */

	case VBI3_CNI_TYPE_VPS:
		for (p = network_table; p < network_table_end; ++p)
			if (p->cni_vps == cni) {
				return p;
			}
		break;

	case VBI3_CNI_TYPE_PDC_A:
	{
		unsigned int cni_vps;

		cni_vps = cni_pdc_a_to_8302 (cni) & 0xFFF;

		if (0 == cni_vps)
			return NULL;

		for (p = network_table; p < network_table_end; ++p)
			if (p->cni_vps == cni_vps) {
				return p;
			}

		break;
	}

	case VBI3_CNI_TYPE_PDC_B:
		for (p = network_table; p < network_table_end; ++p)
			if (p->cni_pdc_b == cni) {
				return p;
			}

		/* try code | 0x0080 & 0x0FFF -> VPS ? */

		break;

	default:
		warning (NULL, "Unknown CNI type %u.", type);
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
 * Converted CNI or 0 if the conversion is not possible.
 */
unsigned int
vbi3_convert_cni			(vbi3_cni_type		to_type,
				 vbi3_cni_type		from_type,
				 unsigned int		cni)
{
	const struct network *p;

	if (to_type == from_type)
		return cni;

/* XXX should we really guess? */
	switch (to_type) {
	case VBI3_CNI_TYPE_VPS:
		p = cni_lookup (from_type, cni);
		if (NULL != p) {
			if (0 != p->cni_vps)
				return p->cni_vps;
		}

		if (VBI3_CNI_TYPE_8302 == from_type)
			return cni_8302_to_vps (cni);

		if (VBI3_CNI_TYPE_PDC_A == from_type)
			return cni_pdc_a_to_vps (cni);

		return 0;

	case VBI3_CNI_TYPE_8301:
		p = cni_lookup (from_type, cni);
		if (NULL == p) {
			return 0;
		}

		/* XXX guess? */
		return p->cni_8301;

	case VBI3_CNI_TYPE_8302:
		p = cni_lookup (from_type, cni);
		if (NULL != p) {
			if (0 != p->cni_8302)
				return p->cni_8302;
		}

		if (VBI3_CNI_TYPE_VPS == from_type)
			return cni_vps_to_8302 (cni);

		if (VBI3_CNI_TYPE_PDC_A == from_type)
			return cni_pdc_a_to_8302 (cni);

		return 0;

	case VBI3_CNI_TYPE_PDC_A:
		p = cni_lookup (from_type, cni);
		if (NULL != p) {
			/* FIXME if table has pdc_a, return that. */
			if (0 != p->cni_8302)
				return cni_8302_to_pdc_a (p->cni_8302);

			if (0 != p->cni_vps)
				return cni_vps_to_pdc_a (p->cni_vps);
		}

		if (VBI3_CNI_TYPE_VPS == from_type)
			return cni_vps_to_pdc_a (cni);

		if (VBI3_CNI_TYPE_8302 == from_type)
			return cni_8302_to_pdc_a (cni);

		return 0;

	case VBI3_CNI_TYPE_PDC_B:
		p = cni_lookup (from_type, cni);
		if (NULL == p) {
			return 0;
		}

		return p->cni_pdc_b;

	default:
		warning (NULL, "Unknown CNI type %u.", to_type);
		break;
	}

	return 0;
}

/**
 * @internal
 */
void
_vbi3_network_dump		(const vbi3_network *	nk,
				 FILE *			fp)
{
	assert (NULL != nk);
	assert (NULL != fp);

	fprintf (fp,
		 "'%s' call_sign=%s cni=%x/%x/%x/%x/%x country=%s",
		 nk->name ? nk->name : "unknown",
		 nk->call_sign[0] ? nk->call_sign : "unknown",
		 nk->cni_vps,
		 nk->cni_8301,
		 nk->cni_8302,
		 nk->cni_pdc_a,
		 nk->cni_pdc_b,
		 nk->country_code[0] ? nk->country_code : "unknown");
}

/**
 * Returns a NUL-terminated ASCII string similar to a GUID, containing
 * the call sign and all CNIs of the network. Non-ASCII characters are
 * encoded as a hex number %XX. You must free() this string when no
 * longer needed.
 */
char *
vbi3_network_id_string		(const vbi3_network *	nk)
{
	char buffer[sizeof (nk->call_sign) * 3 + 5 * 9 + 1];
	char *s;
	unsigned int i;

	s = buffer;

	for (i = 0; i < sizeof (nk->call_sign); ++i) {
		if (0 == nk->call_sign[i]) {
			break;
		} else if (isalnum (nk->call_sign[i])) {
			*s++ = nk->call_sign[i];
		} else {
			s += sprintf (s, "%%%02x", nk->call_sign[i]);
		}
	}

	s += sprintf (s, "-%8x", nk->cni_vps);
	s += sprintf (s, "-%8x", nk->cni_8301);
	s += sprintf (s, "-%8x", nk->cni_8302);
/* XXX we don't compare these: */
	s += sprintf (s, "-%8x", nk->cni_pdc_a);
	s += sprintf (s, "-%8x", nk->cni_pdc_b);

	/* DVB/ATSC PID? */

	return strdup (buffer);
}

/**
 */
vbi3_bool
vbi3_network_set_name		(vbi3_network *		nk,
				 const char *		name)
{
	char *name1;

	assert (NULL != nk);

	if (!(name1 = strdup (name)))
		return FALSE;

	vbi3_free (nk->name);
	nk->name = name1;

	return TRUE;
}

/**
 * @internal
 */
const struct _vbi3_network_pdc *
_vbi3_network_get_pdc		(const vbi3_network *	nk)
{
	const struct network *n;

	n = cni_lookup (VBI3_CNI_TYPE_8301, nk->cni_8301);
	if (NULL != n)
		return n->pdc;

	n = cni_lookup (VBI3_CNI_TYPE_8302, nk->cni_8302);
	if (NULL != n)
		return n->pdc;

	n = cni_lookup (VBI3_CNI_TYPE_VPS, nk->cni_vps);
	if (NULL != n)
		return n->pdc;

	n = cni_lookup (VBI3_CNI_TYPE_PDC_A, nk->cni_pdc_a);
	if (NULL != n)
		return n->pdc;

	n = cni_lookup (VBI3_CNI_TYPE_PDC_B, nk->cni_pdc_b);
	if (NULL != n)
		return n->pdc;

	return NULL;
}

static char *
strdup_table_name		(const char *		name)
{
	return vbi3_strndup_iconv (vbi3_locale_codeset (), "UTF-8",
				   name, strlen (name) + 1);
}

/**
 * @internal
 */
vbi3_bool
_vbi3_network_set_name_from_ttx_header
				(vbi3_network *		nk,
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

		s1 = (const uint8_t *) ttx_header_table[i].header;
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

		name = strdup_table_name (ttx_header_table[i].name);
		if (!name)
			return FALSE;

		vbi3_free (nk->name);
		nk->name = name;

		return TRUE;

	next:
		;
	}

	return FALSE;
}

/**
 */
vbi3_bool
vbi3_network_set_cni		(vbi3_network *		nk,
				 vbi3_cni_type		type,
				 unsigned int		cni)
{
	const struct network *p;
	char *name;

	assert (NULL != nk);

	switch (type) {
	case VBI3_CNI_TYPE_VPS:
		nk->cni_vps = cni;
		break;

	case VBI3_CNI_TYPE_8301:
		nk->cni_8301 = cni;
		break;

	case VBI3_CNI_TYPE_8302:
		nk->cni_8302 = cni;
		break;

	case VBI3_CNI_TYPE_PDC_A:
		nk->cni_pdc_a = cni;
		break;

	case VBI3_CNI_TYPE_PDC_B:
		nk->cni_pdc_b = cni;
		break;

	default:
		warning (NULL, "Unknown CNI type %u.", type);
		break;
	}

	if (!(p = cni_lookup (type, cni)))
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

	if (!(name = strdup_table_name (p->name)))
		return FALSE;

	vbi3_free (nk->name);
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

		_vbi3_strlcpy (nk->country_code,
			      country_table[p->country].country_code,
			      sizeof (nk->country_code));
	}

	return TRUE;
}

/**
 */
vbi3_bool
vbi3_network_set_call_sign	(vbi3_network *		nk,
				 const char *		call_sign)
{
	assert (NULL != nk);
	assert (NULL != call_sign);

	_vbi3_strlcpy (nk->call_sign, call_sign, sizeof (nk->call_sign));

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

		_vbi3_strlcpy (nk->country_code,
			      country_code,
			      sizeof (nk->country_code));
	}

	return TRUE;
}

/**
 * @param nk Initialized vbi3_network structure.
 *
 * @returns
 * @c TRUE if all fields user_data, cni_vps, cni_8301, cni_8302
 * and call_sign are unset (zero).
 */
vbi3_bool
vbi3_network_is_anonymous	(const vbi3_network *	nk)
{
	assert (NULL != nk);

	return (NULL == nk->user_data
		&& 0 == (nk->cni_vps |
			 nk->cni_8301 |
			 nk->cni_8302 |
			 nk->call_sign[0]));
}

/**
 * @param nk1 Initialized vbi3_network structure.
 * @param nk2 Initialized vbi3_network structure.
 *
 * Compares two networks for weak equality. Networks are considered
 * weakly equal if each pair of user_data, cni_vps, cni_8301, cni_8302
 * and call_sign is equal or one value is unset (zero).
 *
 * @returns
 * @c TRUE if the networks are weakly equal.
 */
vbi3_bool
vbi3_network_weak_equal		(const vbi3_network *	nk1,
				 const vbi3_network *	nk2)
{
	assert (NULL != nk1);
	assert (NULL != nk2);

	if (nk1->user_data && nk2->user_data)
		if (nk1->user_data != nk2->user_data)
			return FALSE;

	if (nk1->cni_vps && nk2->cni_vps)
		if (nk1->cni_vps != nk2->cni_vps)
			return FALSE;

	if (nk1->cni_8301 && nk2->cni_8301)
		if (nk1->cni_8301 != nk2->cni_8301)
			return FALSE;

	if (nk1->cni_8302 && nk2->cni_8302)
		if (nk1->cni_8302 != nk2->cni_8302)
			return FALSE;

	if (nk1->call_sign[0] && nk2->call_sign[0])
		if (0 != strcmp (nk1->call_sign, nk2->call_sign))
			return FALSE;

	return TRUE;
}

/**
 * @param nk1 Initialized vbi3_network structure.
 * @param nk2 Initialized vbi3_network structure.
 *
 * Compares two networks for equality. Networks are considered equal
 * if each pair of user_data, cni_vps, cni_8301, cni_8302 and
 * call_sign is equal or both values are unset (zero).
 *
 * @returns
 * @c TRUE if the networks are equal.
 */
vbi3_bool
vbi3_network_equal		(const vbi3_network *	nk1,
				 const vbi3_network *	nk2)
{
	assert (NULL != nk1);
	assert (NULL != nk2);

	if (nk1->user_data || nk2->user_data)
		if (nk1->user_data != nk2->user_data)
			return FALSE;

	if (0 != ((nk1->cni_vps ^ nk2->cni_vps) |
		  (nk1->cni_8301 ^ nk2->cni_8301) |
		  (nk1->cni_8302 ^ nk2->cni_8302)))
		return FALSE;

	if (nk1->call_sign[0] || nk2->call_sign[0])
		if (0 != strcmp (nk1->call_sign, nk2->call_sign))
			return FALSE;

	return TRUE;
}

/**
 * @param nk Initialized vbi3_network structure.
 *
 * Resets all fields to zero, creating an anonymous network.
 */
void
vbi3_network_reset		(vbi3_network *		nk)
{
	assert (NULL != nk);

	vbi3_free (nk->name);

	CLEAR (*nk);
}

/**
 * @param nk vbi3_network structure initialized with vbi3_network_init()
 *   or vbi3_network_copy().
 *
 * Frees all resources associated with @a nk, except the structure itself.
 */
void
vbi3_network_destroy		(vbi3_network *		nk)
{
	vbi3_network_reset (nk);
}

/**
 * @param dst Initialized vbi3_network structure.
 * @param src Initialized vbi3_network structure, can be @c NULL.
 *
 * Sets all fields of @a dst by copying from @a src. Does
 * nothing if @a dst and @a src are the same.
 *
 * @returns
 * @c FALSE on failure (out of memory), leaving @a dst unmodified.
 */
vbi3_bool
vbi3_network_set			(vbi3_network *		dst,
				 const vbi3_network *	src)
{
	assert (NULL != dst);

	if (dst == src)
		return TRUE;

	if (!src) {
		vbi3_network_reset (dst);
	} else {
		char *name;

		name = NULL;

		if (src->name && !(name = strdup (src->name)))
			return FALSE;

		vbi3_free (dst->name);

		memcpy (dst, src, sizeof (*dst));

		dst->name = name;
	}

	return TRUE;
}

/**
 * @param dst vbi3_network structure to initialize.
 * @param src Initialized vbi3_network structure, can be @c NULL.
 *
 * Initializes all fields of @a dst by copying from @a src.
 * Does nothing if @a dst and @a src are the same.
 *
 * @returns
 * @c FALSE on failure (out of memory), with @a dst unmodified.
 */
vbi3_bool
vbi3_network_copy		(vbi3_network *		dst,
				 const vbi3_network *	src)
{
	assert (NULL != dst);

	if (dst == src)
		return TRUE;

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
 * @param nk vbi3_network structure to initialize.
 *
 * Initializes all fields of @a nk to zero, creating an
 * anonymous network.
 *
 * @returns
 * @c TRUE.
 */
vbi3_bool
vbi3_network_init		(vbi3_network *		nk)
{
	return vbi3_network_copy (nk, NULL);
}

/**
 * @param nk malloc()ed array of initialized vbi3_network structures,
 *   can be NULL.
 * @param n_elements Number of elements in the array.
 * 
 * Frees an array of vbi3_network structures and all data associated
 * with them. Does nothing if @a nk or @a n_elements are zero.
 */
extern void
vbi3_network_array_delete	(vbi3_network *		nk,
				 unsigned int		n_elements)
{
	unsigned int i;

	if (NULL == nk || 0 == n_elements)
		return;

	for (i = 0; i < n_elements; ++i)
		vbi3_network_destroy (nk + i);

	vbi3_free (nk);
}
