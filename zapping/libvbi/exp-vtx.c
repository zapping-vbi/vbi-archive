/*
 *  Zapzilla - VTX export function
 *
 *  Copyright (C) 2001 Michael H. Schimek
 *
 *  Based on code from AleVT 1.5.1
 *  Copyright (C) 1998,1999 Edgar Toernig (froese@gmx.de)
 *  Copyright 1999 by Paul Ortyl <ortylp@from.pl>
 *
 *  Based on code from VideoteXt 0.6
 *  Copyright (C) 1995-97 Martin Buck <martin-2.buck@student.uni-ulm.de>
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

/* $Id: exp-vtx.c,v 1.7 2001-09-02 03:25:58 mschimek Exp $ */

/*
 *  VTX is the file format used by VideoteXt. It stores Teletext pages in
 *  raw level 1.0 format. Level 1.5 additional characters (e.g. accents), the
 *  FLOF and TOP navigation bars and the level 2.5 chrome will be lost.
 *  (People interested in an XML based successor to VTX drop us a mail.)
 *
 *  Since restoring the raw page from a fmt_page is complicated we violate
 *  encapsulation by fetching a raw copy from the cache. :-(
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>

#include <sys/stat.h>
#include <unistd.h>

#include "vbi.h"	/* cache, vt.h */
#include "hamm.h"	/* bit_reverse */
#include "export.h"

struct header {
	char			signature[5];
	unsigned char		pagenum_l;
	unsigned char		pagenum_h;
	unsigned char		hour;
	unsigned char		minute;
	unsigned char		charset;
	unsigned char		wst_flags;
	unsigned char		vtx_flags;
};

/*
 *  VTX - VideoteXt File (VTXV4)
 */

static bool
vtx_output(vbi_export *e, FILE *fp, char *name, struct fmt_page *pg)
{
	struct vt_page page, *vtp;
	struct header h;
	struct stat st;

	if (pg->pgno < 0x100 || pg->pgno > 0x8FF) {
		set_errstr_printf(_("Can only export Teletext pages"));
		return FALSE;
	}

	/**/

	vtp = vbi_cache_get(pg->vbi, pg->pgno, pg->subno, -1);

	if (!vtp) {
		set_errstr_printf(_("Page is not cached, sorry"));
		return FALSE;
	}

	page = *vtp;

	/**/

	if (page.function != PAGE_FUNCTION_UNKNOWN
	    && page.function != PAGE_FUNCTION_LOP) {
		set_errstr_printf(_("Cannot export this page"));
		return FALSE;
	}

	memcpy(h.signature, "VTXV4", 5);

	h.pagenum_l = page.pgno & 0xFF;
	h.pagenum_h = (page.pgno >> 8) & 15;

	h.hour = 0;
	h.minute = 0;

	h.charset = page.national & 7;

	h.wst_flags = page.flags & C4_ERASE_PAGE;
	h.wst_flags |= bit_reverse[page.flags >> 12];
	h.vtx_flags = (0 << 7) | (0 << 6) | (0 << 5) | (0 << 4) | (0 << 3);
	/* notfound, pblf (?), hamming error, virtual, seven bits */

	if (name && !(fp = fopen(name, "w"))) {
		set_errstr_printf(_("Cannot create file '%s': %s"), name, strerror(errno));
		return FALSE;
	}

	if (fwrite(&h, sizeof(h), 1, fp) != 1)
		goto write_error;

	if (fwrite(page.data.lop.raw, 40 * 24, 1, fp) != 1)
		goto write_error;

	if (name && fclose(fp)) {
		fp = NULL;
		goto write_error;
	}

	return TRUE;

write_error:
	vbi_export_write_error(e, name);

	if (name) {
		if (fp)
			fclose(fp);

		if (!stat(name, &st) && S_ISREG(st.st_mode))
			remove(name);
	}

	return FALSE;
}

vbi_export_module_priv
export_vtx = {
	.pub = {
		.keyword	= "vtx",
		.label		= N_("VTX"),
		.tooltip	= N_("Export this page as VTX file, the format used by VideoteXt and vbidecode"),
	},
	.extension		= "vtx",
	.output			= vtx_output,
};
