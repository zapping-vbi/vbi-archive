/*
 *  libzvbi - VTX export function
 *
 *  Copyright (C) 2001 Michael H. Schimek
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

/* $Id: exp-vtx.c,v 1.4.2.6 2004-07-09 16:10:52 mschimek Exp $ */

/* VTX is the file format used by the VideoteXt application. It stores
   Teletext pages in raw level 1.0 format. Level 1.5 additional characters
   (e.g. accents), the FLOF and TOP navigation bars and the level 2.5
   chrome will be lost.
 
   Since restoring the raw page from a fmt_page is complicated we violate
   encapsulation by fetching a raw copy from the cache. :-( */

#include "../config.h"

#include <assert.h>
#include <inttypes.h>

#include "hamm.h"		/* vbi_rev8() */
#include "cache-priv.h"		/* cache_page */
#include "page-priv.h"		/* vbi_page_priv */
#include "intl-priv.h"
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

static vbi_bool
export				(vbi_export *		e,
				 const vbi_page *	pg)
{
	const vbi_page_priv *pgp;
	const cache_page *cp;
	struct header h;

	if (pg->pgno < 0x100 || pg->pgno > 0x8FF) {
		_vbi_export_error_printf
			(e, _("Can only export Teletext pages."));
		return FALSE;
	}

	pgp = CONST_PARENT (pg, vbi_page_priv, pg);

	if (pg->priv != pgp || !pgp->cp) {
		_vbi_export_error_printf (e, _("Page is not cached, sorry."));
		return FALSE;
	}

	cp = pgp->cp;

	if (cp->function != PAGE_FUNCTION_UNKNOWN
	    && cp->function != PAGE_FUNCTION_LOP) {
		_vbi_export_error_printf
			(e, _("Cannot export this page, not displayable."));
		goto error;
	}

	memcpy (h.signature, "VTXV4", 5);

	h.pagenum_l = cp->pgno & 0xFF;
	h.pagenum_h = (cp->pgno >> 8) & 15;

	h.hour = 0;
	h.minute = 0;

	h.charset = cp->national & 7;

	h.wst_flags = cp->flags & C4_ERASE_PAGE;
	h.wst_flags |= vbi_rev8 (cp->flags >> 12);
	h.vtx_flags = (0 << 7) | (0 << 6) | (0 << 5) | (0 << 4) | (0 << 3);
	/* notfound, pblf (?), hamming error, virtual, seven bits */

	if (fwrite (&h, sizeof (h), 1, e->fp) != 1)
		goto write_error;

	if (fwrite (cp->data.lop.raw, 40 * 24, 1, e->fp) != 1)
		goto write_error;

	return TRUE;

 write_error:
	_vbi_export_write_error (e);

 error:
	return FALSE;
}

static const vbi_export_info
export_info = {
	.keyword		= "vtx",
	.label			= N_("VTX"),
	.tooltip		= N_("Export this page as VTX file, the "
				     "format used by VideoteXt and vbidecode"),

	/* From VideoteXt examples/mime.types */
	.mime_type		= "application/videotext",
	.extension		= "vtx",
};

const _vbi_export_module
_vbi_export_module_vtx = {
	.export_info		= &export_info,

	/* no private data, no options */

	.export			= export
};
