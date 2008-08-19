/*
 *  libzvbi - Network identification
 *
 *  Copyright (C) 2000-2004 Michael H. Schimek
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

/* $Id: network.c,v 1.1.2.1 2008-08-19 10:56:06 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "misc.h"		/* CLEAR() */
#include "bcd.h"		/* vbi_is_bcd(), vbi_bcd2bin() */
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
 * Static ASCII string, NULL if @a type is unknown.
 */
const char *
_vbi_cni_type_name		(vbi_cni_type		type)
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

#undef CASE

static unsigned int
cni_pdc_a_to_8302		(unsigned int		in_cni)
{
	unsigned int out_cni;
	unsigned int n;

	/* Relation guessed from values observed
	   in DE (misc. networks), AT (ORF), IT (RAI 1).
	   Is this defined anywhere? */

	if (!vbi_is_bcd ((int) in_cni & 0xFFF))
		return 0;

	out_cni = (in_cni >> 4) & 0xFF00;

	n = vbi_bcd2bin ((int) in_cni & 0xFFF);

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
			out_cni |= vbi_bin2bcd ((int)(in_cni & 0xFF) + 100);
			break;

		default:
			return 0;
		}

	case 0x1A000: /* Austria */
	case 0x1D000: /* Germany */
		switch (in_cni & 0xFF) {
		case 0xC0 ... 0xFF:
			out_cni |= vbi_bin2bcd ((int)(in_cni & 0xFF)
						 - 0xC0 + 100);
			break;

		case 0x80 ... 0xBF:
			out_cni |= vbi_bin2bcd ((int)(in_cni & 0xFF)
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
	/* VPS transmits only the 4 lsb of the 8/30-2 country code. */

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
cni_lookup			(vbi_cni_type		type,
				 unsigned int		cni)
{
	const struct network *network_table_end;
	const struct network *p;
	unsigned int cni_vps;

	network_table_end = network_table + N_ELEMENTS (network_table);

	/* TODO: binary search. */

	if (0 == cni)
		goto unknown;

	switch (type) {
	case VBI_CNI_TYPE_UNKNOWN:
		break;

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
		cni_vps = cni_pdc_a_to_8302 (cni) & 0xFFF;

		if (0 == cni_vps)
			goto unknown;

		for (p = network_table; p < network_table_end; ++p)
			if (p->cni_vps == cni_vps) {
				return p;
			}

		break;

	case VBI_CNI_TYPE_PDC_B:
		for (p = network_table; p < network_table_end; ++p)
			if (p->cni_pdc_b == cni) {
				return p;
			}

		/* try code | 0x0080 & 0x0FFF -> VPS ? */

		break;
	}

 unknown:
	return NULL;
}

_vbi_inline vbi_bool
valid_cni_type			(vbi_cni_type		type)
{
	return ((unsigned int) type - 1
		< (unsigned int) VBI_CNI_TYPE_PDC_B);
}

/**
 * @param to_type Type of CNI to convert to.
 * @param from_type Type of CNI to convert from.
 * @param cni CNI to convert.
 *
 * Converts a CNI from one type to another. @a to_type and @a
 * from_type must not be @c VBI_CNI_TYPE_UNKNOWN.
 *
 * @returns
 * Converted CNI. If the conversion is not possible the function
 * returns 0 and sets the errno variable to indicate the error:
 * - @a c VBI_ERR_UNKNOWN_CNI - the @a cni is not in the CNI table and
 *   cannot be guessed.
 * - @a c VBI_ERR_INVALID_CNI_TYPE - @a to_type or @a from_type
 *   is invalid.
 */
unsigned int
vbi_convert_cni			(vbi_cni_type		to_type,
				 vbi_cni_type		from_type,
				 unsigned int		cni)
{
	const struct network *p;

	if (unlikely (!valid_cni_type (to_type)
		      || !valid_cni_type (from_type))) {
		errno = VBI_ERR_INVALID_CNI_TYPE;
		return 0;
	}

	if (to_type == from_type)
		return cni;

	switch (to_type) {
	case VBI_CNI_TYPE_UNKNOWN:
		break;

	case VBI_CNI_TYPE_VPS:
		p = cni_lookup (from_type, cni);
		if (NULL != p) {
			if (0 != p->cni_vps)
				return p->cni_vps;
		}

		if (VBI_CNI_TYPE_8302 == from_type)
			return cni_8302_to_vps (cni);

		if (VBI_CNI_TYPE_PDC_A == from_type)
			return cni_pdc_a_to_vps (cni);

		break;

	case VBI_CNI_TYPE_8301:
		p = cni_lookup (from_type, cni);
		if (NULL != p) {
			if (0 != p->cni_8301)
				return p->cni_8301;
		}

		/* XXX guess? */

		break;

	case VBI_CNI_TYPE_8302:
		p = cni_lookup (from_type, cni);
		if (NULL != p) {
			if (0 != p->cni_8302)
				return p->cni_8302;
		}

		if (VBI_CNI_TYPE_VPS == from_type)
			return cni_vps_to_8302 (cni);

		if (VBI_CNI_TYPE_PDC_A == from_type)
			return cni_pdc_a_to_8302 (cni);

		break;

	case VBI_CNI_TYPE_PDC_A:
		p = cni_lookup (from_type, cni);
		if (NULL != p) {
			/* FIXME if table has pdc_a, return that. */
			if (0 != p->cni_8302)
				return cni_8302_to_pdc_a (p->cni_8302);

			if (0 != p->cni_vps)
				return cni_vps_to_pdc_a (p->cni_vps);
		}

		if (VBI_CNI_TYPE_VPS == from_type)
			return cni_vps_to_pdc_a (cni);

		if (VBI_CNI_TYPE_8302 == from_type)
			return cni_8302_to_pdc_a (cni);

		break;

	case VBI_CNI_TYPE_PDC_B:
		p = cni_lookup (from_type, cni);
		if (NULL != p) {
			if (0 != p->cni_pdc_b)
				return p->cni_pdc_b;
		}

		break;
	}

	errno = VBI_ERR_UNKNOWN_CNI;

	return 0;
}

#if 0

/**
 * @internal
 */
vbi_bool
_vbi_network_dump		(const vbi_network *	nk,
				 FILE *			fp)
{
	_vbi_null_check (nk, FALSE);
	_vbi_null_check (fp, FALSE);

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

	return TRUE;
}

/**
 * Returns a NUL-terminated ASCII string similar to a GUID, containing
 * the call sign and all CNIs of the network. Non-ASCII characters are
 * encoded as a hex number %XX. You must free() this string when no
 * longer needed.
 */
char *
vbi_network_id_string		(const vbi_network *	nk)
{
	char buffer[sizeof (nk->call_sign) * 3 + 5 * 9 + 1];
	char *s;
	char *s_end;
	unsigned int i;

	s = buffer;
	s_end = buffer + sizeof (buffer);

	for (i = 0; i < sizeof (nk->call_sign); ++i) {
		if (0 == nk->call_sign[i]) {
			break;
		} else if (isalnum (nk->call_sign[i])) {
			*s++ = nk->call_sign[i];
		} else {
			s += snprintf (s, s_end - s, "%%%02x",
				       nk->call_sign[i]);
		}
	}

	s += snprintf (s, s_end - s, "-%8x", nk->cni_vps);
	s += snprintf (s, s_end - s, "-%8x", nk->cni_8301);
	s += snprintf (s, s_end - s, "-%8x", nk->cni_8302);
/* XXX we don't compare these: */
	s += snprintf (s, s_end - s, "-%8x", nk->cni_pdc_a);
	s += snprintf (s, s_end - s, "-%8x", nk->cni_pdc_b);

	/* DVB/ATSC PID? */

	return strdup (buffer);
}

#endif

/**
 */
vbi_bool
vbi_network_set_name		(vbi_network *		nk,
				 const char *		name)
{
	char *new_name;

	_vbi_null_check (nk, FALSE);

	if (NULL != name) {
		new_name = strdup (name);
		if (NULL == new_name) {
			errno = ENOMEM;
			return FALSE;
		}
	}

	vbi_free (nk->name);
	nk->name = new_name;

	return TRUE;
}

#if 0

/**
 * @internal
 */
const struct _vbi_network_pdc *
_vbi_network_get_pdc		(const vbi_network *	nk)
{
	const struct network *n;

	n = cni_lookup (VBI_CNI_TYPE_8301, nk->cni_8301);
	if (NULL != n)
		return n->pdc;

	n = cni_lookup (VBI_CNI_TYPE_8302, nk->cni_8302);
	if (NULL != n)
		return n->pdc;

	n = cni_lookup (VBI_CNI_TYPE_VPS, nk->cni_vps);
	if (NULL != n)
		return n->pdc;

	n = cni_lookup (VBI_CNI_TYPE_PDC_A, nk->cni_pdc_a);
	if (NULL != n)
		return n->pdc;

	n = cni_lookup (VBI_CNI_TYPE_PDC_B, nk->cni_pdc_b);
	if (NULL != n)
		return n->pdc;

	return NULL;
}

#endif

static char *
strdup_table_name		(const char *		name)
{
	return vbi_strndup_iconv (vbi_locale_codeset (), "UTF-8",
				  name, strlen (name) + 1, '_');
}

#if 0

/**
 * @internal
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

		vbi_free (nk->name);
		nk->name = name;

		return TRUE;

	next:
		;
	}

	return FALSE;
}

#endif

/**
 */
vbi_bool
vbi_network_set_cni		(vbi_network *		nk,
				 vbi_cni_type		type,
				 unsigned int		cni)
{
	const struct network *p;
	char *new_name;

	_vbi_null_check (nk, FALSE);

	if (!valid_cni_type (type)) {
		errno = VBI_ERR_INVALID_CNI_TYPE;
		return FALSE;
	}

	p = cni_lookup (type, cni);
	if (NULL == p) {
		errno = VBI_ERR_UNKNOWN_CNI;
		return FALSE;
	}

	/* Keep in mind our table may be wrong. */

	if (p->cni_vps && nk->cni_vps
	    && p->cni_vps != nk->cni_vps)
		goto mismatch;

	if (p->cni_8301 && nk->cni_8301
	    && p->cni_8301 != nk->cni_8301)
		goto mismatch;

	if (p->cni_8302 && nk->cni_8302
	    && p->cni_8302 != nk->cni_8302)
		goto mismatch;

	new_name = strdup_table_name (p->name);
	if (NULL == new_name) {
		errno = ENOMEM;
		return FALSE;
	}

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
		assert (0);
	}

	vbi_free (nk->name);
	nk->name = new_name;

	nk->cni_vps	= p->cni_vps;
	nk->cni_8301	= p->cni_8301;
	nk->cni_8302	= p->cni_8302;

	if (0 == nk->cni_pdc_a)
		nk->cni_pdc_a = cni_vps_to_pdc_a (p->cni_vps);

	if (0 == nk->cni_pdc_b)
		nk->cni_pdc_b = p->cni_pdc_b;

#if 0
	if (0 == nk->country_code[0]) {
		assert (p->country < N_ELEMENTS (country_table));

		strlcpy (nk->country_code,
			 country_table[p->country].country_code,
			 sizeof (nk->country_code));
	}
#endif

	return TRUE;

 mismatch:
	errno = VBI_ERR_CNI_MISMATCH;
	return FALSE;
}

/**
 */
vbi_bool
vbi_network_set_call_sign	(vbi_network *		nk,
				 const char *		call_sign)
{
	_vbi_null_check (nk, FALSE);
	_vbi_null_check (call_sign, FALSE);

	strlcpy (nk->call_sign, call_sign, sizeof (nk->call_sign));

#if 0
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

		strlcpy (nk->country_code,
			 country_code,
			 sizeof (nk->country_code));
	}
#endif

	return TRUE;
}

/**
 * @param nk Initialized vbi_network structure.
 *
 * @returns
 * @c TRUE if the fields user_data, cni_vps, cni_8301, cni_8302 and
 * call_sign are all unset (zero), @c FALSE if not or if @a nk is
 * a @c NULL pointer.
 *
 * @since 0.3.1
 */
vbi_bool
vbi_network_anonymous		(const vbi_network *	nk)
{
	if (unlikely (NULL == nk))
		return FALSE;

	return (NULL == nk->user_data
		&& 0 == (nk->cni_vps |
			 nk->cni_8301 |
			 nk->cni_8302 |
			 nk->call_sign[0]));
}

/**
 * @param nk1 Initialized vbi_network structure.
 * @param nk2 Initialized vbi_network structure.
 *
 * Compares two networks for weak equality. Networks are considered
 * weakly equal if each pair of user_data, cni_vps, cni_8301, cni_8302
 * and call_sign is equal or one value is unset (zero).
 *
 * @returns
 * @c TRUE if the networks are equal, @c FALSE if they are not equal
 * or if @a nk1 or @a nk2 is @a NULL.
 *
 * @since 0.3.1
 */
vbi_bool
vbi_network_weakly_equal	(const vbi_network *	nk1,
				 const vbi_network *	nk2)
{
	if (unlikely (NULL == nk1 || NULL == nk2))
		return FALSE;

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
 * @param nk1 Initialized vbi_network structure.
 * @param nk2 Initialized vbi_network structure.
 *
 * Compares two networks for equality. Networks are considered equal
 * if each pair of user_data, cni_vps, cni_8301, cni_8302 and
 * call_sign is equal or both values are unset (zero).
 *
 * @returns
 * @c TRUE if the networks are equal, @c FALSE if they are not equal
 * or if @a nk1 or @a nk2 is @a NULL.
 *
 * @since 0.3.1
 */
vbi_bool
vbi_network_equal		(const vbi_network *	nk1,
				 const vbi_network *	nk2)
{
	if (unlikely (NULL == nk1 || NULL == nk2))
		return FALSE;

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
 * @param nk Initialized vbi_network structure.
 *
 * Resets all fields to zero, creating an anonymous network.
 *
 * @returns
 * @c FALSE and sets errno to @c VBI_ERR_NULL_ARG if @a nk is
 * a @c NULL pointer.
 *
 * @since 0.3.1
 */
vbi_bool
vbi_network_reset		(vbi_network *		nk)
{
	_vbi_null_check (nk, FALSE);

	vbi_free (nk->name);

	CLEAR (*nk);

	return TRUE;
}

/**
 * @param nk vbi_network structure initialized with vbi_network_init()
 *   or vbi_network_copy().
 *
 * Frees all resources associated with @a nk, except for the structure
 * itself.
 *
 * @returns
 * @c FALSE and sets errno to @c VBI_ERR_NULL_ARG if @a nk is
 * a @c NULL pointer.
 *
 * @since 0.3.1
 */
vbi_bool
vbi_network_destroy		(vbi_network *		nk)
{
	return vbi_network_reset (nk);
}

/**
 * @param dst Initialized vbi_network structure.
 * @param src Initialized vbi_network structure, can be @c NULL.
 *
 * Sets all fields of @a dst by copying from @a src. If @a src is @c
 * NULL the function works like vbi_network_reset(). It does nothing
 * if @a dst == @a src.
 *
 * @returns
 * On failure the @a dst remains unmodified, the function returns
 * @c FALSE and sets the errno variable to indicate the error:
 * - @c ENOMEM - insufficient memory.
 * - @c VBI_ERR_NULL_ARG - @a dst is a @c NULL pointer.
 *
 * @since 0.3.1
 */
vbi_bool
vbi_network_set			(vbi_network *		dst,
				 const vbi_network *	src)
{
	char *new_name;

	if (NULL == src)
		return vbi_network_reset (dst);

	_vbi_null_check (dst, FALSE);

	if (dst == src)
		return TRUE;

	new_name = NULL;

	if (NULL != src->name) {
		new_name = strdup (src->name);
		if (NULL == new_name) {
			errno = ENOMEM;
			return FALSE;
		}
	}

	vbi_free (dst->name);

	memcpy (dst, src, sizeof (*dst));

	dst->name = new_name;

	return TRUE;
}

/**
 * @param dst vbi_network structure to initialize.
 * @param src Initialized vbi_network structure, can be @c NULL.
 *
 * Initializes all fields of @a dst by copying the fields of @a src.
 * If @a src is @c NULL the function works like vbi_network_init().
 *
 * @returns
 * On failure the @a dst remains unmodified, the function returns
 * @c FALSE and sets the errno variable to indicate the error:
 * - @c ENOMEM - insufficient memory.
 * - @c VBI_ERR_NULL_ARG - @a dst is a @c NULL pointer.
 *
 * @since 0.3.1
 */
vbi_bool
vbi_network_copy		(vbi_network *		dst,
				 const vbi_network *	src)
{
	_vbi_null_check (dst, FALSE);

	if (dst == src)
		return TRUE;

	if (NULL == src) {
		CLEAR (*dst);
	} else if (dst != src) {
		char *new_name = NULL;

		if (NULL != src->name) {
			new_name = strdup (src->name);
			if (NULL == new_name) {
				errno = ENOMEM;
				return FALSE;
			}
		}

		memcpy (dst, src, sizeof (*dst));

		dst->name = new_name;
	}

	return TRUE;
}

/**
 * @param nk vbi_network structure to initialize.
 *
 * Initializes all fields of @a nk to zero, creating an
 * anonymous network.
 *
 * @returns
 * @c FALSE if @a nk is @a NULL.
 *
 * @since 0.3.1
 */
vbi_bool
vbi_network_init		(vbi_network *		nk)
{
	return vbi_network_copy (nk, NULL);
}

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
