/*
 *  libzvbi
 *
 *  Copyright (C) 2006 Michael H. Schimek
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

/* $Id: packet_filter.c,v 1.1.2.1 2006-05-07 06:04:58 mschimek Exp $ */

#include <stdlib.h>
#include <errno.h>
#include <assert.h>

#ifdef ZAPPING8
#  include "common/intl-priv.h"
#else
#  include "intl-priv.h"
#endif
#include "hamm.h"		/* vbi3_unham16p() */
#include "event.h"		/* VBI3_SERIAL */
#include "misc.h"		/* SWAP() */
#include "packet_filter.h"

struct page_range {
	vbi3_pgno		first;
	vbi3_pgno		last;
};

struct subpage_range {
	vbi3_pgno		pgno;
	vbi3_subno		first;
	vbi3_subno		last;
};

struct _vbi3_packet_filter {
	vbi3_sliced *		output_buffer;
	unsigned int		output_max_lines;

	unsigned int		keep_mag_set_next;

	struct page_range *	keep_pages;
	unsigned int		keep_pages_size;
	unsigned int		keep_pages_capacity;

	struct subpage_range *	keep_subpages;
	unsigned int		keep_subpages_size;
	unsigned int		keep_subpages_capacity;

	vbi3_bool		keep_system_pages;

	vbi3_service_set	keep_services;

	char *			errstr;

	vbi3_packet_filter_cb *	callback;
	void *			user_data;
};

static void
no_mem_error			(vbi3_packet_filter *	pf)
{
	free (pf->errstr);

	/* Error ignored. */
	pf->errstr = strdup (_("Out of memory."));

	errno = ENOMEM;
}

void
vbi3_packet_filter_keep_system_pages
				(vbi3_packet_filter *	pf,
				 vbi3_bool		keep)
{
	assert (NULL != pf);

	pf->keep_system_pages = !!keep;
}

static vbi3_bool
extend_vector			(vbi3_packet_filter *	pf,
				 void **		vector,
				 unsigned int *		capacity,
				 unsigned int		element_size)
{
	void *new_vec;
	unsigned int new_capacity;

	new_capacity = 8;

	if (*capacity > 0)
		new_capacity = *capacity * 2;

	new_vec = realloc (*vector, new_capacity * element_size);
	if (NULL == new_vec) {
		no_mem_error (pf);
		return FALSE;
	}

	*vector = new_vec;
	*capacity = new_capacity;

	return TRUE;
}

vbi3_bool
vbi3_packet_filter_keep_pages	(vbi3_packet_filter *	pf,
				 vbi3_pgno		first_pgno,
				 vbi3_pgno		last_pgno)
{
	unsigned int i;

	assert (NULL != pf);

	if (first_pgno > last_pgno)
		SWAP (first_pgno, last_pgno);

	for (i = 0; i < pf->keep_pages_size; ++i) {
		if (last_pgno >= pf->keep_pages[i].first
		    && first_pgno <= pf->keep_pages[i].last) {
			if (first_pgno < pf->keep_pages[i].first)
				pf->keep_pages[i].first = first_pgno;
			if (last_pgno > pf->keep_pages[i].last)
				pf->keep_pages[i].last = last_pgno;

			return TRUE;
		}
	}

	if (i >= pf->keep_pages_capacity) {
		if (!extend_vector (pf, (void **) &pf->keep_pages,
				    &pf->keep_pages_capacity,
				    sizeof (*pf->keep_pages)))
			return FALSE;
	}

	pf->keep_pages[i].first = first_pgno;
	pf->keep_pages[i].last = last_pgno;

	pf->keep_pages_size = i + 1;

	return TRUE;
}

vbi3_bool
vbi3_packet_filter_keep_subpages
				(vbi3_packet_filter *	pf,
				 vbi3_pgno		pgno,
				 vbi3_subno		first_subno,
				 vbi3_subno		last_subno)
{
	unsigned int i;

	assert (NULL != pf);

	if (first_subno > last_subno)
		SWAP (first_subno, last_subno);

	for (i = 0; i < pf->keep_pages_size; ++i) {
		if (pgno == pf->keep_subpages[i].pgno
		    && last_subno >= pf->keep_subpages[i].first
		    && first_subno <= pf->keep_subpages[i].last) {
			if (first_subno < pf->keep_subpages[i].first)
				pf->keep_subpages[i].first = first_subno;
			if (last_subno > pf->keep_subpages[i].last)
				pf->keep_subpages[i].last = last_subno;

			return TRUE;
		}
	}

	if (i >= pf->keep_subpages_capacity) {
		if (!extend_vector (pf, (void **) &pf->keep_subpages,
				    &pf->keep_subpages_capacity,
				    sizeof (*pf->keep_subpages)))
			return FALSE;
	}

	pf->keep_subpages[i].pgno = pgno;
	pf->keep_subpages[i].first = first_subno;
	pf->keep_subpages[i].last = last_subno;

	pf->keep_subpages_size = i + 1;

	return TRUE;
}

vbi3_service_set
vbi3_packet_filter_keep_services
				(vbi3_packet_filter *	pf,
				 vbi3_service_set	services)
{
	assert (NULL != pf);

	return pf->keep_services |= services;
}

void
vbi3_packet_filter_drop_pages	(vbi3_packet_filter *	pf,
				 vbi3_pgno		first_pgno,
				 vbi3_pgno		last_pgno)
{
	unsigned int i;

	assert (NULL != pf);

	if (first_pgno > last_pgno)
		SWAP (first_pgno, last_pgno);

	for (i = 0; i < pf->keep_pages_size; ++i) {
		if (last_pgno >= pf->keep_pages[i].first
		    && first_pgno <= pf->keep_pages[i].last) {
			if (first_pgno > pf->keep_pages[i].first)
				pf->keep_pages[i].first = first_pgno;
			if (last_pgno < pf->keep_pages[i].last)
				pf->keep_pages[i].last = last_pgno;
		}
	}
}

void
vbi3_packet_filter_drop_subpages
				(vbi3_packet_filter *	pf,
				 vbi3_pgno		pgno,
				 vbi3_subno		first_subno,
				 vbi3_subno		last_subno)
{
	unsigned int i;

	assert (NULL != pf);

	if (first_subno > last_subno)
		SWAP (first_subno, last_subno);

	for (i = 0; i < pf->keep_pages_size; ++i) {
		if (pgno == pf->keep_subpages[i].pgno
		    && last_subno >= pf->keep_subpages[i].first
		    && first_subno <= pf->keep_subpages[i].last) {
			if (first_subno > pf->keep_subpages[i].first)
				pf->keep_subpages[i].first = first_subno;
			if (last_subno < pf->keep_subpages[i].last)
				pf->keep_subpages[i].last = last_subno;
		}
	}
}

vbi3_service_set
vbi3_packet_filter_drop_services
				(vbi3_packet_filter *	pf,
				 vbi3_service_set	services)
{
	assert (NULL != pf);

	return pf->keep_services &= ~services;
}

void
vbi3_packet_filter_reset	(vbi3_packet_filter *	pf)
{
	assert (NULL != pf);

	pf->keep_mag_set_next = 0;
}

static vbi3_bool
decode_packet_0			(vbi3_packet_filter *	pf,
				 unsigned int *		keep_mag_set,
				 const uint8_t		buffer[42],
				 unsigned int		magazine)
{
	int page;
	int flags;
	vbi3_pgno pgno;
	vbi3_subno subno;
	unsigned int mag_set;

	page = vbi3_unham16p (buffer + 2);
	if (page < 0) {
		free (pf->errstr);
		/* Error ignored. */
		pf->errstr = strdup (_("Hamming error in Teletext "
				       "packet 0 page number."));
		errno = EINVAL;
		return FALSE;
	}

	if (0xFF == page) {
		/* Filler, discard. */
		*keep_mag_set = 0;
		return TRUE;
	}

	pgno = magazine * 0x100 + page;

	flags = vbi3_unham16p (buffer + 4)
		| (vbi3_unham16p (buffer + 6) << 8)
		| (vbi3_unham16p (buffer + 8) << 16);
	if (flags < 0) {
		free (pf->errstr);
		/* Error ignored. */
		pf->errstr = strdup (_("Hamming error in Teletext "
				       "packet 0 flags."));
		errno = EINVAL;
		return FALSE;
	}

	if (flags & VBI3_SERIAL) {
		mag_set = -1;
	} else {
		mag_set = 1 << magazine;
	}

	subno = 0; /* TODO */

	if (!vbi3_is_bcd (pgno)) {
		/* Page inventories and TOP pages (e.g. to
		   find subtitles), DRCS and object pages, etc. */
		if (pf->keep_system_pages)
			goto match;
	} else {
		unsigned int i;

		for (i = 0; i < pf->keep_pages_size; ++i) {
			if (pgno >= pf->keep_pages[i].first
			    && pgno <= pf->keep_pages[i].last)
				goto match;
		}

		for (i = 0; i < pf->keep_subpages_size; ++i) {
			if (pgno == pf->keep_subpages[i].pgno
			    && subno >= pf->keep_subpages[i].first
			    && subno <= pf->keep_subpages[i].last)
				goto match;
		}
	}

	if (*keep_mag_set & mag_set) {
		/* Terminate page. */
		pf->keep_mag_set_next = *keep_mag_set & ~mag_set;
	} else {
		*keep_mag_set &= ~mag_set;
		pf->keep_mag_set_next = *keep_mag_set;
	}

	return TRUE;

 match:
	*keep_mag_set |= mag_set;
	pf->keep_mag_set_next = *keep_mag_set;

	return TRUE;
}

static vbi3_bool
teletext			(vbi3_packet_filter *	pf,
				 vbi3_bool *		keep,
				 const uint8_t		buffer[42],
				 unsigned int		line)
{
	int pmag;
	unsigned int magazine;
	unsigned int packet;
	unsigned int keep_mag_set;

	line = line;

	keep_mag_set = pf->keep_mag_set_next;

	pmag = vbi3_unham16p (buffer);
	if (pmag < 0) {
		free (pf->errstr);
		/* Error ignored. */
		pf->errstr = strdup (_("Hamming error in Teletext "
				       "packet number.\n"));
		errno = EINVAL;
		return FALSE;
	}

	magazine = pmag & 7;
	if (0 == magazine)
		magazine = 8;

	packet = pmag >> 3;

	switch (packet) {
	case 0: /* Page header. */
		if (!decode_packet_0 (pf, &keep_mag_set, buffer, magazine))
			return FALSE;
		break;

	case 1 ... 25: /* Page body. */
	case 26: /* Page enhancement packet. */
	case 27: /* Page linking. */
	case 28:
	case 29: /* Level 2.5/3.5 enhancement. */
		break;

	case 30:
	case 31: /* IDL packet (ETS 300 708). */
		*keep = FALSE;
		return TRUE;

	default:
		assert (0);
	}

	*keep = !!(keep_mag_set & (1 << magazine));

	return TRUE;
}

/* NB sliced_out == sliced_in permitted. */
vbi3_bool
vbi3_packet_filter_cor		(vbi3_packet_filter *	pf,
				 vbi3_sliced *		sliced_out,
				 unsigned int *		n_lines_out,
				 unsigned int		max_lines_out,
				 const vbi3_sliced *	sliced_in,
				 unsigned int 		n_lines_in)
{
	unsigned int max_lines_out1;

	assert (NULL != pf);
	assert (NULL != sliced_out);
	assert (NULL != n_lines_out);
	assert (NULL != sliced_in);

	max_lines_out1 = max_lines_out;

	while (n_lines_in > 0) {
		vbi3_bool pass_through;

		if (0 == max_lines_out)
			break;

		if (sliced_in->id & pf->keep_services) {
			pass_through = TRUE;
		} else {
			switch (sliced_in->id) {
			case VBI3_SLICED_TELETEXT_B_L10_625:
			case VBI3_SLICED_TELETEXT_B_L25_625:
			case VBI3_SLICED_TELETEXT_B_625:
				if (!teletext (pf,
					       &pass_through,
					       sliced_in->data,
					       sliced_in->line))
					return FALSE;
				break;

			default:
				pass_through = FALSE;
				break;
			}
		}

		if (pass_through) {
			memcpy (sliced_out,
				sliced_in,
				sizeof (*sliced_out));

			++sliced_out;
			--max_lines_out;
		}

		++sliced_in;
		--n_lines_in;
	}

	*n_lines_out = max_lines_out1 - max_lines_out;

	return TRUE;
}

vbi3_bool
vbi3_packet_filter_feed		(vbi3_packet_filter *	pf,
				 const vbi3_sliced *	sliced,
				 unsigned int		n_lines)
{
	assert (NULL != pf);
	assert (NULL != sliced);

	if (pf->output_max_lines < n_lines) {
		vbi3_sliced *s;
		unsigned int n;

		n = MIN (n_lines, 50U);
		s = realloc (pf->output_buffer,
			     n * sizeof (*pf->output_buffer));
		if (NULL == s) {
			no_mem_error (pf);
			return FALSE;
		}

		pf->output_buffer = s;
		pf->output_max_lines = n;
	}

	if (!vbi3_packet_filter_cor (pf,
				     pf->output_buffer,
				     &n_lines,
				     pf->output_max_lines,
				     sliced,
				     n_lines))
		return FALSE;

	if (NULL != pf->callback)
		return pf->callback (pf,
				     pf->output_buffer,
				     n_lines,
				     pf->user_data);

	return TRUE;
}

const char *
vbi3_packet_filter_errstr	(vbi3_packet_filter *	pf)
{
	assert (NULL != pf);

	return pf->errstr;
}

void
vbi3_packet_filter_delete	(vbi3_packet_filter *	pf)
{
	if (NULL == pf)
		return;

	free (pf->keep_subpages);
	free (pf->keep_pages);

	free (pf->output_buffer);

	free (pf->errstr);

	CLEAR (*pf);

	vbi3_free (pf);		
}

vbi3_packet_filter *
vbi3_packet_filter_new		(vbi3_packet_filter_cb *callback,
				 void *			user_data)
{
	vbi3_packet_filter *pf;

	pf = vbi3_malloc (sizeof (*pf));
	if (NULL == pf) {
		return NULL;
	}

	CLEAR (*pf);

	pf->callback = callback;
	pf->user_data = user_data;

	return pf;
}
