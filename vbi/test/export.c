/*
 *  libzvbi test
 *
 *  Copyright (C) 2000, 2001 Michael H. Schimek
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

/* $Id: export.c,v 1.5.2.2 2004-02-13 02:10:15 mschimek Exp $ */

#undef NDEBUG

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include <libzvbi.h>

vbi_decoder *		vbi;
vbi_bool		quit = FALSE;
vbi_pgno		pgno;
vbi_export *		ex;
char *			extension;
vbi_bool		option_columns_41;
vbi_bool		option_navigation;
vbi_bool		option_hyperlinks;
vbi_bool		option_pdc_links;
vbi_bool		option_enum;
unsigned int		delay;

extern void
vbi_preselection_dump		(const vbi_preselection *pl,
				 FILE *			fp);
static void
pdc_dump (vbi_page *pg)
{
	vbi_preselection pl;
	unsigned int i;

	for (i = 0; vbi_page_pdc_enum (pg, &pl, i); ++i) {
		fprintf (stderr, "%02u: ", i);
		vbi_preselection_dump (&pl, stderr);
	}

	if (0 == i)
		fputs ("No PDC data\n", stderr);
}

static void
handler(vbi_event *ev, void *unused)
{
	FILE *fp;
	vbi_page *pg;

	fprintf(stderr, "\rPage %03x.%02x ",
		ev->ev.ttx_page.pgno,
		ev->ev.ttx_page.subno & 0xFF);

	if (pgno != -1 && ev->ev.ttx_page.pgno != pgno)
		return;

	if (delay > 0) {
		--delay;
		return;
	}

	fprintf(stderr, "\nSaving... ");
	if (isatty(STDERR_FILENO))
		fputc('\n', stderr);
	fflush(stderr);

	pg = vbi_fetch_vt_page(vbi,
			       ev->ev.ttx_page.pgno,
			       ev->ev.ttx_page.subno,
			       VBI_WST_LEVEL, VBI_WST_LEVEL_3p5,
			       VBI_41_COLUMNS, option_columns_41,
			       VBI_NAVIGATION, option_navigation,
			       VBI_HYPERLINKS, option_hyperlinks,
			       VBI_PDC_LINKS, option_pdc_links,
			       VBI_END);

	/* Just for fun */
	if (pgno == -1) {
		char name[256];
		
		snprintf(name, sizeof(name) - 1, "ttx-%03x-%02x.%s",
			 ev->ev.ttx_page.pgno,
			 ev->ev.ttx_page.subno,
			 extension);

		assert((fp = fopen(name, "w")));
	} else
		fp = stdout;

	if (!vbi_export_stdio(ex, fp, pg)) {
		fprintf(stderr, "failed: %s\n", vbi_export_errstr(ex));
		exit(EXIT_FAILURE);
	} else {
		fprintf(stderr, "done\n");
	}

	if (option_enum)
		pdc_dump (pg);

	vbi_page_delete (pg);

	if (pgno == -1)
		assert(fclose(fp) == 0);
	else
		quit = TRUE;
}

static void
stream(void)
{
	char buf[256];
	double time = 0.0, dt;
	int index, items, i;
	vbi_sliced *s, sliced[40];

	while (!quit) {
		if (ferror(stdin) || !fgets(buf, 255, stdin))
			goto abort;

		dt = strtod(buf, NULL);
		items = fgetc(stdin);

		assert(items < 40);

		for (s = sliced, i = 0; i < items; s++, i++) {
			index = fgetc(stdin);
			s->line = (fgetc(stdin) + 256 * fgetc(stdin)) & 0xFFF;

			if (index < 0)
				goto abort;

			switch (index) {
			case 0:
				s->id = VBI_SLICED_TELETEXT_B;
				fread(s->data, 1, 42, stdin);
				break;
			case 1:
				s->id = VBI_SLICED_CAPTION_625; 
				fread(s->data, 1, 2, stdin);
				break; 
			case 2:
				s->id = VBI_SLICED_VPS; 
				fread(s->data, 1, 13, stdin);
				break;
			case 3:
				s->id = VBI_SLICED_WSS_625; 
				fread(s->data, 1, 2, stdin);
				break;
			case 4:
				s->id = VBI_SLICED_WSS_CPR1204; 
				fread(s->data, 1, 3, stdin);
				break;
			case 7:
				s->id = VBI_SLICED_CAPTION_525; 
				fread(s->data, 1, 2, stdin);
				break;
			default:
				fprintf(stderr, "\nOops! Unknown data %d "
					"in sample file\n", index);
				exit(EXIT_FAILURE);
			}
		}

		if (feof(stdin) || ferror(stdin))
			goto abort;

		vbi_decode(vbi, sliced, items, time);

		time += dt;
	}

	return;

abort:
	fprintf(stderr, "\rEnd of stream, page %03x not found\n", pgno);
}

int
main(int argc, char **argv)
{
	char *module, *t;
	vbi_export_info *xi;
	unsigned int i;

	module= NULL;
	pgno = 0;
	option_columns_41 = FALSE;
	option_navigation = FALSE;
	option_hyperlinks = FALSE;
	option_pdc_links = FALSE;
	delay = 0;

	for (i = 1; i < argc; ++i) {
		if (0 == strcmp ("-d", argv[i])) {
			delay = 3;
		} else if (0 == strcmp ("-4", argv[i])) {
			option_columns_41 = TRUE;
		} else if (0 == strcmp ("-n", argv[i])) {
			option_navigation = TRUE;
		} else if (0 == strcmp ("-h", argv[i])) {
			option_hyperlinks = TRUE;
		} else if (0 == strcmp ("-p", argv[i])) {
			option_pdc_links = TRUE;
		} else if (0 == strcmp ("-e", argv[i])) {
			option_enum = TRUE;
		} else if (module) {
			if (0 != pgno)
				goto bad_args;

			pgno = strtol (argv[i], NULL, 16);
		} else {
			module = argv[i];
		}
	}

	if (!module || 0 == pgno) {
	bad_args:
		fprintf (stderr, "Usage: %s [format options] module[;export "
			 "options] pgno <vbi data >file\n"
			 "module e.g. \"ppm\", pgno e.g. 100 (hex)\n"
			 "format options:\n"
			 "-4 add 41st column\n"
			 "-n add navigation bar\n"
			 "-h add hyperlinks\n"
			 "-p add PDC links\n",
			 argv[0]);
		exit (EXIT_FAILURE);
	}

	if (isatty (STDIN_FILENO)) {
		fprintf (stderr, "No vbi data on stdin\n");
		exit (EXIT_FAILURE);
	}

	if (!(ex = vbi_export_new(module, &t))) {
		fprintf(stderr, "Failed to open export module '%s': %s\n",
			module, t);
		exit(EXIT_FAILURE);
	}


	assert((xi = vbi_export_info_export(ex)));
	assert((extension = strdup(xi->extension)));
	extension = strtok_r(extension, ",", &t);

	assert((vbi = vbi_decoder_new()));

	assert(vbi_event_handler_add(vbi, VBI_EVENT_TTX_PAGE, handler, NULL)); 

	stream();

	exit(EXIT_SUCCESS);
}
