/*
 *  libzvbi
 *
 *  Copyright (C) 2001-2004 Michael H. Schimek
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

/* $Id: program_info.c,v 1.1.2.1 2004-07-09 16:10:53 mschimek Exp $ */

#include "../config.h"

#include <assert.h>
#include "misc.h"		/* CLEAR() */
#include "program_info.h"

/**
 */
const char *
vbi_xds_audio_name		(vbi_xds_audio		mode)
{
	switch (mode) {

#undef CASE
#define CASE(s) case VBI_XDS_AUDIO_##s : return #s ;

	CASE (NONE)
	CASE (MONO)
	CASE (STEREO)
	CASE (STEREO_SURROUND)
	CASE (SIMULATED_STEREO)
	CASE (VIDEO_DESCRIPTIONS)
	CASE (NON_PROGRAM_AUDIO)
	CASE (SPECIAL_EFFECTS)
	CASE (DATA_SERVICE)
	CASE (UNKNOWN)

	}

	return NULL;
}

/**
 */
void
_vbi_program_info_dump		(const vbi_program_info *pi,
				 FILE *			fp)
{
	assert (NULL != pi);
	assert (NULL != fp);

	fprintf (fp, "title='%s'\n",
		 pi->title ? pi->title : "none");

	fprintf (fp, "audio[0]=%s lang=%s\n",
		 vbi_xds_audio_name (pi->audio[0].mode),
		 pi->audio[0].lang_code ? pi->audio[0].lang_code : "unknown");

	fprintf (fp, "audio[1]=%s lang=%s\n",
		 vbi_xds_audio_name (pi->audio[1].mode),
		 pi->audio[1].lang_code ? pi->audio[1].lang_code : "unknown");

	fprintf (fp, "descr='%s'\n",
		 pi->description ? pi->description : "none");
}

/**
 */
void
vbi_program_info_destroy	(vbi_program_info *	pi)
{
	assert (NULL != pi);

	free (pi->title);
	free (pi->description);

	CLEAR (*pi);
}

/**
 */
void
vbi_program_info_init		(vbi_program_info *	pi)
{
	assert (NULL != pi);
	
	CLEAR (*pi);

	pi->audio[0].mode = VBI_XDS_AUDIO_UNKNOWN;
	pi->audio[1].mode = VBI_XDS_AUDIO_UNKNOWN;
}
