/*
 *  Real Time Encoder
 *
 *  Copyright (C) 2000-2001 Iñaki García Etxebarria
 *  Modified 2001 Michael H. Schimek
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
/*
  Shows info about available backends, codecs, options, etc ...
*/
#include "../src/rte.h"
#include <stdio.h>
#include <assert.h>

/* FIXME: Needed for satifying rte dependencies, get rid of it */
int verbose;

static void
show_codec_info (rte_codec_info *ci)
{
	char *types[] = {"Video", "Audio", "Sliced VBI"};

	printf("\tKeyword:\t%s\n", ci->keyword);
	printf("\tStream type:\t%s\n", types[ci->stream_type]);
	printf("\tLabel:\t\t%s\n", ci->label);
	printf("\tTooltip:\t%s\n\n", ci->tooltip);
}

static void
show_context_info (rte_context_info *ci)
{
	rte_context *context;
	rte_codec_info *di;
	int i;

	printf("++ %s [keyword: %s]\n", ci->label, ci->keyword);
	printf("Backend:\t%s\n", ci->backend);
	printf("Tooltip:\t%s\n", ci->tooltip);
	printf("Mime types:\t%s\n", ci->mime_type);
	printf("Extension:\t%s\n\n", ci->extension);
	printf("Audio elementary:\t%d\n", ci->elementary[RTE_STREAM_AUDIO]);
	printf("Video elementary:\t%d\n", ci->elementary[RTE_STREAM_VIDEO]);
	printf("VBI elementary:\t\t%d\n",
	       ci->elementary[RTE_STREAM_SLICED_VBI]);

	context = rte_context_new(ci->keyword);
	assert(context != NULL);

	for (i=0; (di = rte_codec_info_enum(context, i)); i++)
		show_codec_info(di);

	rte_context_delete(context);
}

int main(int argc, char *argv[])
{
	rte_context_info *ci;
	int i;

	verbose = 0;

	for (i=0; (ci = rte_context_info_enum(i)); i++)
		show_context_info(ci);
	
	return 0;
}
