/*
 *  libzvbi - Links
 *
 *  Copyright (C) 2001, 2002, 2003, 2004 Michael H. Schimek
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

/* $Id: link.c,v 1.1.2.6 2006-05-18 16:49:19 mschimek Exp $ */

#include "../site_def.h"

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "misc.h"
#include "link.h"

/** */
const char *
vbi3_link_type_name		(vbi3_link_type		type)
{
	switch (type) {

#undef CASE
#define CASE(type) case VBI3_LINK_##type : return #type ;

	CASE (NONE)
	CASE (MESSAGE)
	CASE (PAGE)
	CASE (SUBPAGE)
	CASE (HTTP)
	CASE (FTP)
	CASE (EMAIL)
	CASE (LID)
	CASE (TELEWEB)

	}

	return NULL;
}

/** @internal */
void
_vbi3_link_dump			(const vbi3_link *	ld,
				 FILE *			fp)
{
	assert (NULL != ld);
	assert (NULL != fp);

	fprintf (fp, "%s eacem=%u name='%s' url='%s' script='%s' "
		 "pgno=%x subno=%x expires=%f itv=",
		 vbi3_link_type_name (ld->type),
		 ld->eacem,
		 ld->name ? ld->name : "none",
		 ld->url ? ld->url : "none",
		 ld->script ? ld->script : "none",
		 ld->pgno, ld->subno, ld->expires);

	switch (ld->itv_type) {
	case VBI3_WEBLINK_UNKNOWN:	  fputs ("UNKNOWN", fp); break;
	case VBI3_WEBLINK_PROGRAM_RELATED: fputs ("PROGRAM", fp); break;
	case VBI3_WEBLINK_NETWORK_RELATED: fputs ("NETWORK", fp); break;
	case VBI3_WEBLINK_STATION_RELATED: fputs ("STATION", fp); break;
	case VBI3_WEBLINK_SPONSOR_MESSAGE: fputs ("SPONSOR", fp); break;
	case VBI3_WEBLINK_OPERATOR:	  fputs ("OPERATOR", fp); break;
	default:
		fprintf (fp, "%u??", ld->itv_type);
		break;
	}

	fputc ('\n', fp);

	if (ld->network) {
		_vbi3_network_dump (ld->network, fp);
		fputc ('\n', fp);
	}
}

/**
 * @param lk vbi3_link structure initialized with vbi3_link_init()
 *   or vbi3_link_copy().
 *
 * Frees all resources associated with @a lk, except the structure itself.
 */
void
vbi3_link_destroy		(vbi3_link *		ld)
{
	assert (NULL != ld);

	vbi3_free (ld->name);
	vbi3_free (ld->url);
	vbi3_free (ld->script);

	if (ld->nk_alloc) {
		vbi3_network_destroy (ld->network);
		vbi3_free (ld->network);
	}

	CLEAR (*ld);
}

/**
 * @param dst vbi3_link structure to initialize.
 * @param src Initialized vbi3_link structure, can be @c NULL.
 *
 * Initializes all fields of @a dst by copying from @a src. Does
 * nothing if @a dst and @a src are the same.
 *
 * @returns
 * @c FALSE on failure (out of memory).
 */
vbi3_bool
vbi3_link_copy			(vbi3_link *		dst,
				 const vbi3_link *	src)
{
	assert (NULL != dst);

	if (dst == src)
		return TRUE;

	if (!src) {
		vbi3_link_init (dst);
	} else if (dst != src) {
		char *name;
		char *url;
		char *script;
		vbi3_network *nk;

		name	= NULL;
		url	= NULL;
		script	= NULL;
		nk	= NULL;

		if (src->name && !(name = strdup (src->name)))
			return FALSE;

		if (src->url && !(url = strdup (src->url))) {
			vbi3_free (name);
			return FALSE;
		}

		if (src->script && !(script = strdup (src->script))) {
			vbi3_free (url);
			vbi3_free (name);
			return FALSE;
		}

		if (src->network) {
			if (!(nk = vbi3_malloc (sizeof (*nk)))) {
				vbi3_free (script);
				vbi3_free (url);
				vbi3_free (name);
				return FALSE;
			}

			vbi3_network_copy (nk, src->network);
		}

		dst->type	= src->type;
		dst->eacem	= src->eacem;

		dst->name	= name;
		dst->url	= url;
		dst->script	= script;

		dst->network	= nk;
		dst->nk_alloc	= (NULL != nk);

		dst->pgno	= src->pgno;
		dst->subno	= src->subno;

		dst->expires	= src->expires;
		dst->itv_type	= src->itv_type;
		dst->priority	= src->priority;
		dst->autoload	= src->autoload;
	}

	return TRUE;
}

/**
 * @param ld vbi3_link structure to initialize.
 *
 * Initializes all fields of @a ld to zero.
 *
 * @returns
 * @c TRUE.
 */
void
vbi3_link_init		(vbi3_link *		ld)
{
	assert (NULL != ld);

	ld->type	= VBI3_LINK_NONE;

	ld->eacem	= FALSE;

	ld->name	= NULL;
	ld->url	= NULL;
	ld->script	= NULL;

	ld->network	= NULL;
	ld->nk_alloc	= FALSE;

	ld->pgno	= 0;
	ld->subno	= VBI3_ANY_SUBNO;

	ld->expires	= 0.0;
	ld->itv_type	= VBI3_WEBLINK_UNKNOWN;
	ld->priority	= 9;
	ld->autoload	= FALSE;
}
