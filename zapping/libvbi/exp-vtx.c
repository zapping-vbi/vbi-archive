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

/* $Id: exp-vtx.c,v 1.2 2001-03-09 17:39:01 mschimek Exp $ */

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

#include "vbi.h"
#include "hamm.h"	/* bit_reverse */
#include "export.h"

/* future */
#undef _
#define _(String) (String)

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

static int
vtx_output(struct export *e, char *name, struct fmt_page *pg)
{
	struct vt_page page, *vtp;
	struct header h;
	struct stat st;
	FILE *fp;

	if (pg->pgno < 0x100 || pg->pgno > 0x8FF) {
		export_error(e, _("can only export Teletext pages"));
		return 0;
	}

	/**/

	vtp = pg->vbi->cache->op->get(pg->vbi->cache, pg->pgno, pg->subno, 0xFFFF);

	if (!vtp) {
		export_error(e, _("page is not cached, sorry"));
		return 0;
	}

	page = *vtp;

	/**/

	if (page.function != PAGE_FUNCTION_UNKNOWN
	    && page.function != PAGE_FUNCTION_LOP) {
		export_error(e, _("cannot export this page"));
		return 0;
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

	if (!(fp = fopen(name, "w"))) {
		export_error(e, _("cannot create file '%s': %s"), name, strerror(errno));
		return -1;
	}

	if (fwrite(&h, sizeof(h), 1, fp) != 1)
		goto write_error;

	if (fwrite(page.data.lop.raw, 40 * 24, 1, fp) != 1)
		goto write_error;

	if (fclose(fp)) {
		fp = NULL;
		goto write_error;
	}

	return 0;

write_error:
	export_error(e, errno ?
		_("error while writing file '%s': %s") :
		_("error while writing file '%s'"), name, strerror(errno));

	if (fp)
		fclose(fp);

	if (!stat(name, &st) && S_ISREG(st.st_mode))
		remove(name);

	return -1;
}

struct export_module
export_vtx[1] =			// exported module definition
{
    {
	"vtx",			// id
	"vtx",			// extension
	0,			// options
	0,			// size
	0,			// open
	0,			// close
	0,			// option
	vtx_output		// output
    }
};
