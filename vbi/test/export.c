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

/* $Id: export.c,v 1.5.2.8 2004-05-01 13:51:35 mschimek Exp $ */

#undef NDEBUG

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include "src/zvbi.h"

vbi_decoder *		vbi;
vbi_bool		quit = FALSE;
vbi_pgno		pgno;
vbi_export *		e;
char *			extens;
vbi_bool		option_columns_41;
vbi_bool		option_navigation;
vbi_bool		option_hyperlinks;
vbi_bool		option_pdc_links;
vbi_bool		option_enum;
vbi_bool		option_stream;
vbi_bool		option_quiet;
unsigned int		delay;
char			cr;

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
		_vbi_preselection_dump (&pl, stderr);
		fputc ('\n', stderr);
	}

	if (0 == i)
		fputs ("No PDC data\n", stderr);
}

static vbi_bool
export_link			(vbi_export *		export,
				 void *			user_data,
				 FILE *			fp,
				 const vbi_link *	link,
				 const char *		text)
{
	if (0)
		fprintf (stderr, "link text: \"%s\"\n", text);

	switch (link->type) {
	case VBI_LINK_HTTP:
	case VBI_LINK_FTP:
	case VBI_LINK_EMAIL:
		fprintf (fp, "<a href=\"%s\">%s</a>",
			 link->url, text);
		break;

	case VBI_LINK_PAGE:
	case VBI_LINK_SUBPAGE:
		fprintf (fp, "<a href=\"ttx-%3x-%02x.html\">%s</a>",
			 link->pgno,
			 (VBI_ANY_SUBNO == link->subno) ?
			 0 : link->subno,
			 text);
		break;

	default:
		fputs (text, fp);
		break;
	}

	return 0 == ferror (fp);
}

static vbi_bool
export_pdc			(vbi_export *		export,
				 void *			user_data,
				 FILE *			fp,
				 const vbi_preselection *pl,
				 const char *		text)
{
	unsigned int end;

	end = pl->at1_hour * 60 + pl->at1_minute + pl->length;

	fprintf (fp, "<acronym title=\"%04u-%02u-%02u "
		 "%02u:%02u-%02u:%02u "
		 "VPS/PDC: %02u%02u\">%s</acronym>",
		 pl->year, pl->month + 1, pl->day + 1,
		 pl->at1_hour, pl->at1_minute,
		 (end / 60 % 24), end % 60,
		 pl->at2_hour, pl->at2_minute,
		 text);

	return 0 == ferror (fp);
}

static void
handler(vbi_event *ev, void *unused)
{
	static double timestamp = 0.0;
	FILE *fp;
	vbi_page *pg;

	if (!option_quiet)
		fprintf(stderr, "Page %03x.%02x %c",
			ev->ev.ttx_page.pgno,
			ev->ev.ttx_page.subno & 0xFF,
			cr);

	if (pgno >= 0 && ev->ev.ttx_page.pgno != pgno)
		return;

	if (delay > 0) {
		--delay;
		return;
	}

	if (!option_quiet) {
		fprintf(stderr, "\nSaving... ");
		if (isatty(STDERR_FILENO))
			fputc('\n', stderr);
		fflush(stderr);
	}

	pg = vbi_fetch_vt_page(vbi,
			       ev->ev.ttx_page.pgno,
			       ev->ev.ttx_page.subno,
			       VBI_WST_LEVEL, VBI_WST_LEVEL_3p5,
			       VBI_41_COLUMNS, option_columns_41,
			       VBI_NAVIGATION, option_navigation,
			       VBI_HYPERLINKS, option_hyperlinks,
			       VBI_PDC_LINKS, option_pdc_links,
			       0);

	if (pgno == -1) {
		char name[256];
		
		snprintf(name, sizeof(name) - 1, "ttx-%03x-%02x.%s",
			 ev->ev.ttx_page.pgno,
			 ev->ev.ttx_page.subno,
			 extens);

		assert((fp = fopen(name, "w")));
	} else {
		fp = stdout;
	}

	vbi_export_set_timestamp (e, timestamp);
	timestamp += 1.0;

	if (!vbi_export_stdio (e, fp, pg)) {
		fprintf (stderr, "failed: %s\n", vbi_export_errstr (e));
		exit (EXIT_FAILURE);
	} else {
		if (!option_quiet)
			fprintf (stderr, "done\n");
	}

	if (option_enum)
		pdc_dump (pg);

	vbi_page_delete (pg);

	if (option_stream) {
	} else if (pgno < 0) {
		if (pgno == -1)
			assert(fclose(fp) == 0);
	} else {
		quit = TRUE;
	}
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
	if (option_stream) {
		/* Finalize. */
		vbi_export_stdio (e, stdout, NULL);
	}

	fprintf(stderr, "\rEnd of stream, page %03x not found\n", pgno);
}

int
main(int argc, char **argv)
{
	char *module, *t;
	const vbi_export_info *xi;
	unsigned int i;

	cr = isatty (STDERR_FILENO) ? '\r' : '\n';

	module= NULL;
	pgno = 0;
	option_columns_41 = FALSE;
	option_navigation = FALSE;
	option_hyperlinks = FALSE;
	option_pdc_links = FALSE;
	option_enum = FALSE;
	delay = 0;

	for (i = 1; i < argc; ++i) {
		if (0 == strcmp ("-d", argv[i])) {
			delay += 1;
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
		} else if (0 == strcmp ("-s", argv[i])) {
			option_stream = TRUE;
		} else if (0 == strcmp ("-q", argv[i])) {
			option_quiet = TRUE;
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

	if (!(e = vbi_export_new (module, &t))) {
		fprintf (stderr, "Failed to open export module '%s': %s\n",
			 module, t);
		exit (EXIT_FAILURE);
	}

	if (0 == strncmp (module, "html", 4)) {
		if (option_hyperlinks)
			vbi_export_set_link_cb (e, export_link, NULL);

		if (option_pdc_links)
			vbi_export_set_pdc_cb (e, export_pdc, NULL);
	}

	assert ((xi = vbi_export_info_from_export (e)));
	assert ((extens = strdup (xi->extension)));
	extens = strtok_r (extens, ",", &t);

	assert ((vbi = vbi_decoder_new ()));

	assert (vbi_event_handler_register
		(vbi, VBI_EVENT_TTX_PAGE, handler, NULL)); 

	stream ();

	exit (EXIT_SUCCESS);
}
