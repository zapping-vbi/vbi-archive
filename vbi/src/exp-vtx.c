/*
 *  libzvbi - VTX export function
 *
 *  Copyright (C) 2001, 2002, 2003, 2004, 2007 Michael H. Schimek
 *
 *  Based on code from AleVT 1.5.1
 *  Copyright (C) 1998, 1999 Edgar Toernig <froese@gmx.de>
 *  Copyright (C) 1999 Paul Ortyl <ortylp@from.pl>
 *
 *  Based on code from VideoteXt 0.6
 *  Copyright (C) 1995, 1996, 1997 Martin Buck
 *    <martin-2.buck@student.uni-ulm.de>
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

/* $Id: exp-vtx.c,v 1.4.2.10 2007-11-11 03:06:12 mschimek Exp $ */

/* VTX is the file format used by the VideoteXt application. It stores
   Teletext pages in raw level 1.0 format. Level 1.5 additional characters
   (e.g. accents), the FLOF and TOP navigation bars and the level 2.5
   chrome will be lost.
 
   Since restoring the raw page from a fmt_page is complicated if not
   impossible, we violate encapsulation by fetching a raw copy from
   the cache. :-( */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <assert.h>
#include <inttypes.h>

#include "hamm.h"		/* vbi3_rev8() */
#include "cache-priv.h"		/* cache_page */
#include "page-priv.h"		/* vbi3_page_priv */
#ifdef ZAPPING8
#  include "common/intl-priv.h"
#else
#  include "version.h"
#  include "intl-priv.h"
#endif
#include "export-priv.h"

struct header {
	uint8_t			signature[5];
	uint8_t			pagenum_l;
	uint8_t			pagenum_h;
	uint8_t			hour;
	uint8_t			minute;
	uint8_t			charset;
	uint8_t			wst_flags;
	uint8_t			vtx_flags;
};

/*
 *  VTX - VideoteXt File (VTXV4)
 */

static vbi3_bool
export				(vbi3_export *		e,
				 const vbi3_page *	pg)
{
	const vbi3_page_priv *pgp;
	const cache_page *cp;
	struct header *h;
	size_t needed;

	if (pg->pgno < 0x100 || pg->pgno > 0x8FF) {
		_vbi3_export_error_printf
			(e, _("Can only export Teletext pages."));
		return FALSE;
	}

	pgp = CONST_PARENT (pg, vbi3_page_priv, pg);

	if (pg->priv != pgp || !pgp->cp) {
		_vbi3_export_error_printf (e, _("Page is not cached."));
		return FALSE;
	}

	cp = pgp->cp;

	if (cp->function != PAGE_FUNCTION_UNKNOWN
	    && cp->function != PAGE_FUNCTION_LOP) {
		_vbi3_export_error_printf
			(e, _("Cannot export this page, is not displayable."));
		goto error;
	}

	needed = sizeof (*h);
	if (VBI3_EXPORT_TARGET_ALLOC == e->target)
		needed += 40 * 24;

	if (!_vbi3_export_grow_buffer_space (e, needed))
		return FALSE;

	h = (struct header *)(e->buffer.data + e->buffer.offset);

	memcpy (h->signature, "VTXV4", 5);

	h->pagenum_l = pg->pgno & 0xFF;
	h->pagenum_h = (pg->pgno >> 8) & 15;

	h->hour = 0;
	h->minute = 0;

	h->charset = cp->national & 7;

	h->wst_flags = cp->flags & C4_ERASE_PAGE;
	h->wst_flags |= vbi3_rev8 (cp->flags >> 12);
	h->vtx_flags = (0 << 7) | (0 << 6) | (0 << 5) | (0 << 4) | (0 << 3);
	/* notfound, pblf (?), hamming error, virtual, seven bits */

	e->buffer.offset += sizeof (*h);

	if (!vbi3_export_write (e, cp->data.lop.raw, 40 * 24))
		goto error;

	return TRUE;

 error:
	return FALSE;
}

static const vbi3_export_info
export_info = {
	.keyword		= "vtx",
	.label			= N_("VTX"),
	.tooltip		= N_("Export the page as VTX file, the "
				     "format used by VideoteXt and vbidecode"),

	/* From VideoteXt examples/mime.types */
	.mime_type		= "application/videotext",
	.extension		= "vtx",
};

const _vbi3_export_module
_vbi3_export_module_vtx = {
	.export_info		= &export_info,

	/* no private data, no options */

	.export			= export
};

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
