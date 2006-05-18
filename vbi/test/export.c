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

/* $Id: export.c,v 1.5.2.14 2006-05-18 16:49:21 mschimek Exp $ */

#undef NDEBUG

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <locale.h>
#include <unistd.h>
#ifdef HAVE_GETOPT_LONG
#  include <getopt.h>
#endif
#include <ctype.h>
#include <errno.h>

#include "src/misc.h"
#include "src/zvbi.h"

typedef struct {
	uint16_t		pgno;
	uint16_t		subno;
} pgnum;

static pgnum *		pgnos;
static unsigned int	n_pgnos;
static unsigned int	max_pgnos;
static unsigned int	left_pgnos;

vbi3_dvb_demux *	dx;
vbi3_decoder *		vbi;
vbi3_export *		e;
unsigned int		delay;
char *			extens;
char			cr;
vbi3_bool		quit;

char *			filename_prefix;
char *			filename_suffix;
vbi3_bool		source_pes;
vbi3_charset_code	option_default_cs;
vbi3_rgba		option_default_fg = (vbi3_rgba) 0xFFFFFF;
vbi3_rgba		option_default_bg = (vbi3_rgba) 0x000000;
vbi3_bool		option_dump_pg;
vbi3_bool		option_enum;
vbi3_bool		option_header_only;
vbi3_bool		option_hyperlinks;
vbi3_bool		option_navigation;
vbi3_charset_code	option_override_cs;
vbi3_bool		option_row_update;
vbi3_bool		option_padding;
vbi3_bool		option_panels;
vbi3_bool		option_pdc_links;
vbi3_bool		option_stream;
vbi3_bool		option_dcc = TRUE;
vbi3_bool               option_fast;

unsigned int		verbose;

char *			my_name;

static void
pgnos_add			(vbi3_pgno		pgno,
				 vbi3_subno		subno)
{
	if (n_pgnos >= max_pgnos) {
		unsigned int new_max;
		pgnum *p;

		new_max = MAX (1U, max_pgnos * 2);

		p = realloc (pgnos, sizeof (*p) * new_max);
		if (NULL == p) {
			fprintf (stderr, "Out of memory.\n");
			exit (EXIT_FAILURE);
		}

		pgnos = p;
		max_pgnos = new_max;
	}

	pgnos[n_pgnos].pgno = pgno;
	pgnos[n_pgnos].subno = subno;

	++n_pgnos;
}

extern void
vbi3_preselection_dump		(const vbi3_preselection *pl,
				 FILE *			fp);
static void
pdc_dump			(vbi3_page *		pg)
{
	const vbi3_preselection *pl;
	unsigned int size;
	unsigned int i;

	pl = vbi3_page_get_preselections (pg, &size);
	assert (NULL != pl);

	for (i = 0; i < size; ++i) {
		fprintf (stderr, "%02u: ", i);
		_vbi3_preselection_dump (pl + i, stderr);
		fputc ('\n', stderr);
	}

	if (0 == i)
		fputs ("No PDC data\n", stderr);
}

static vbi3_bool
export_link			(vbi3_export *		export,
				 void *			user_data,
				 FILE *			fp,
				 const vbi3_link *	link)
{
	export = export;
	user_data = user_data;

	if (0)
		fprintf (stderr, "link text: \"%s\"\n", link->name);

	switch (link->type) {
	case VBI3_LINK_HTTP:
	case VBI3_LINK_FTP:
	case VBI3_LINK_EMAIL:
		fprintf (fp, "<a href=\"%s\">%s</a>",
			 link->url, link->name);
		break;

	case VBI3_LINK_PAGE:
	case VBI3_LINK_SUBPAGE:
		fprintf (fp, "<a href=\"ttx-%3x-%02x.html\">%s</a>",
			 link->pgno,
			 (VBI3_ANY_SUBNO == link->subno) ? 0 : link->subno,
			 link->name);
		break;

	default:
		fputs (link->name, fp);
		break;
	}

	return 0 == ferror (fp);
}

static vbi3_bool
export_pdc			(vbi3_export *		export,
				 void *			user_data,
				 FILE *			fp,
				 const vbi3_preselection *pl,
				 const char *		text)
{
	unsigned int end;

	export = export;
	user_data = user_data;

	end = pl->at1_hour * 60 + pl->at1_minute + pl->length;

	/* XXX pl->title uses locale encoding but the html page may not
	   (export charset parameter). */
	fprintf (fp, "<acronym title=\"%04u-%02u-%02u "
		 "%02u:%02u-%02u:%02u "
		 "VPS/PDC: %02u%02u TTX: %x Title: %s"
		 "\">%s</acronym>",
		 pl->year, pl->month, pl->day,
		 pl->at1_hour, pl->at1_minute,
		 (end / 60 % 24), end % 60,
		 pl->at2_hour, pl->at2_minute,
		 pl->_pgno, pl->title, text);

	return 0 == ferror (fp);
}

static void
page_dump			(vbi3_page *		pg)
{
	unsigned int row;

	for (row = 0; row < pg->rows; ++row) {
		const vbi3_char *cp;
		unsigned int column;

		fprintf (stderr, "%2d: >", row);

		cp = pg->text + row * pg->columns;

		for (column = 0; column < pg->columns; ++column) {
			int c;

			c = cp[column].unicode;
			if (c < 0x20 || c > 0x7E)
				c = '.';

			fputc (c, stderr);
		}

		fputs ("<\n", stderr);
	}
}

static void
do_export			(vbi3_pgno		pgno,
				 vbi3_subno		subno,
				 double			timestamp)
{
	FILE *fp;
	vbi3_page *pg;

	if (delay > 0) {
		--delay;
		return;
	}

	if (verbose && !option_stream) {
		fprintf (stderr, "\nSaving... ");

		if (option_dump_pg || isatty (STDERR_FILENO)) {
			fputc ('\n', stderr);
		}

		fflush (stderr);
	}

	if (pgno >= 0x100) {
		if (0 != option_override_cs) {
			pg = vbi3_decoder_get_page
				(vbi, NULL /* current network */,
				 pgno, subno,
				 VBI3_HEADER_ONLY, option_header_only, 
				 VBI3_PADDING, option_padding,
				 VBI3_PANELS, option_panels,
				 VBI3_NAVIGATION, option_navigation,
				 VBI3_HYPERLINKS, option_hyperlinks,
				 VBI3_PDC_LINKS, option_pdc_links,
				 VBI3_WST_LEVEL, VBI3_WST_LEVEL_3p5,
				 VBI3_OVERRIDE_CHARSET_0, option_override_cs,
				 VBI3_END);
		} else {
			pg = vbi3_decoder_get_page
				(vbi, NULL /* current network */,
				 pgno, subno,
				 VBI3_HEADER_ONLY, option_header_only, 
				 VBI3_PADDING, option_padding,
				 VBI3_PANELS, option_panels,
				 VBI3_NAVIGATION, option_navigation,
				 VBI3_HYPERLINKS, option_hyperlinks,
				 VBI3_PDC_LINKS, option_pdc_links,
				 VBI3_WST_LEVEL, VBI3_WST_LEVEL_3p5,
				 VBI3_DEFAULT_CHARSET_0, option_default_cs,
				 VBI3_END);
		}
	} else {
		pg = vbi3_decoder_get_page
			(vbi, NULL /* current network */,
			 pgno, 0,
			 VBI3_PADDING, option_padding,
			 VBI3_DEFAULT_FOREGROUND, option_default_fg,
			 VBI3_DEFAULT_BACKGROUND, option_default_bg,
			 VBI3_ROW_CHANGE, option_row_update,
			 VBI3_END);
	}

	assert (NULL != pg);

	if (option_dump_pg) {
		page_dump (pg);
	}

	if (1 == n_pgnos || NULL == filename_prefix) {
		fp = stdout;
	} else {
		char name[256];

		if (NULL == filename_suffix) {
			snprintf (name, sizeof (name) - 1,
				  "%s-%03x-%02x.%s",
				  filename_prefix, pgno, subno, extens);
		} else {
			snprintf (name, sizeof (name) - 1,
				  "%s-%03x-%02x%s",
				  filename_prefix, pgno, subno,
				  filename_suffix);
		}

		fp = fopen (name, "w");
		if (NULL == fp) {
			fprintf (stderr,
				 "%s: Could not open '%s' for output: %s\n",
				 my_name, optarg, strerror (errno));
			exit (EXIT_FAILURE);
		}
	}

	vbi3_export_set_timestamp (e, timestamp);

	if (!vbi3_export_stdio (e, fp, pg)) {
		fprintf (stderr, "Export failed: %s\n",
			 vbi3_export_errstr (e));
		exit (EXIT_FAILURE);
	}

	if (verbose && !option_stream) {
		fprintf (stderr, "done\n");
	}

	if (option_enum) {
		pdc_dump (pg);
	}

	fflush (fp);

	if (fp != stdout) {
		int r;

		r = fclose (fp);
		assert (0 == r);
	}

	vbi3_page_delete (pg);

	if (option_stream) {
		/* More... */
	} else if (1 == n_pgnos) {
		quit = TRUE;
	}
}

static void
filter_pgnos                    (void)
{
	unsigned int i;

	if (0 == n_pgnos) {
		return;
	}

	for (i = 0; i < n_pgnos; ++i) {
		vbi3_ttx_page_stat ps;

		if (!vbi3_teletext_decoder_get_ttx_page_stat
		    (vbi3_decoder_cast_to_teletext_decoder (vbi),
		     &ps, /* nk: current */ NULL, pgnos[i].pgno)) {
			continue;
		}

#warning XXX what are the defaults in ps until we receive the page inventory?
		if (VBI3_NO_PAGE == ps.page_type) {
			goto remove;
		}

		if (VBI3_ANY_SUBNO == pgnos[i].subno) {
			continue;
		}

		if (ps.subpages > 0
		    && vbi3_bcd2bin (pgnos[i].subno) > ps.subpages) {
			goto remove;
		}

		continue;

	remove:
		pgnos[i].pgno = 0;
		pgnos[i].subno = 0;

		quit = (0 == --left_pgnos);
	}
}

static vbi3_bool
handler				(const vbi3_event *	ev,
				 void *			user_data)
{
	vbi3_pgno pgno;
	vbi3_subno subno;

	user_data = user_data; /* unused */

	switch (ev->type) {
	case VBI3_EVENT_TTX_PAGE:
		pgno = ev->ev.ttx_page.pgno;
		subno = ev->ev.ttx_page.subno;

		if (verbose) {
			fprintf (stderr, "Teletext page %03x.%02x %c",
				 pgno, subno & 0xFF, cr);
		}

		break;

	case VBI3_EVENT_CC_PAGE:
		if (option_row_update
		    && !(ev->ev.caption.flags & VBI3_ROW_UPDATE))
			return TRUE;

		pgno = ev->ev.caption.channel;
		subno = VBI3_ANY_SUBNO;

		if (verbose) {
			fprintf (stderr, "Caption channel %u %c",
				 pgno, cr);
		}

		break;

	case VBI3_EVENT_PAGE_TYPE:
		filter_pgnos ();
		break;

	default:
		assert (0);
	}

	if (n_pgnos > 0) {
		unsigned int i;

		for (i = 0; i < n_pgnos; ++i) {
			if (pgno == pgnos[i].pgno
			    && (VBI3_ANY_SUBNO == pgnos[i].subno
				|| subno == pgnos[i].subno)) {
				do_export (pgno, subno, ev->timestamp);

				if (!option_stream) {
					pgnos[i].pgno = 0;
					pgnos[i].subno = 0;

					quit = (0 == --left_pgnos);
				}
			}
		}
	} else {
		do_export (pgno, subno, ev->timestamp);
	}

	return TRUE; /* handled */
}

static void
pes_mainloop			(void)
{
	uint8_t buffer[2048];

	while (1 == fread (buffer, sizeof (buffer), 1, stdin)) {
		const uint8_t *bp;
		unsigned int left;

		bp = buffer;
		left = sizeof (buffer);

		while (left > 0) {
			vbi3_sliced sliced[64];
			unsigned int lines;
			int64_t pts;

			lines = _vbi3_dvb_demux_cor (dx,
						    sliced, 64,
						    &pts,
						    &bp, &left);
#warning no lines (pts ok, no frame dropping) or error?
			if (lines > 0)
				vbi3_decoder_feed (vbi, sliced, lines,
						    pts / 90000.0);

			if (quit)
				return;
		}
	}

	if (!feof (stdin)) {
		perror ("Read error");
		exit (EXIT_FAILURE);
	}
}

static void
old_mainloop			(void)
{
	char buf[256];
	double time = 0.0, dt;
	int index, items, i;
	vbi3_sliced *s, sliced[40];

	while (!quit) {
		if (ferror (stdin) || !fgets (buf, 255, stdin))
			goto abort;

		dt = strtod (buf, NULL);
		if (dt < 0.0) {
			dt = -dt;
		}

		items = fgetc (stdin);

		assert (items < 40);

		for (s = sliced, i = 0; i < items; s++, i++) {
			index = fgetc (stdin);
			s->line = (fgetc (stdin)
				   + 256 * fgetc (stdin)) & 0xFFF;

			if (index < 0)
				goto abort;

			switch (index) {
			case 0:
				s->id = VBI3_SLICED_TELETEXT_B;
				fread (s->data, 1, 42, stdin);
				break;
			case 1:
				s->id = VBI3_SLICED_CAPTION_625; 
				fread (s->data, 1, 2, stdin);
				break; 
			case 2:
				s->id = VBI3_SLICED_VPS; 
				fread (s->data, 1, 13, stdin);
				break;
			case 3:
				s->id = VBI3_SLICED_WSS_625; 
				fread (s->data, 1, 2, stdin);
				break;
			case 4:
				s->id = VBI3_SLICED_WSS_CPR1204; 
				fread (s->data, 1, 3, stdin);
				break;
			case 7:
				s->id = VBI3_SLICED_CAPTION_525; 
				fread (s->data, 1, 2, stdin);
				break;
			default:
				fprintf (stderr, "\nOops! Unknown data %d "
					 "in sample file\n", index);
				exit(EXIT_FAILURE);
			}
		}

		if (feof (stdin) || ferror (stdin))
			goto abort;

		vbi3_decoder_feed (vbi, sliced, items, time);

		time += dt;
	}

	return;

abort:
	if (ferror (stdin)) {
		perror ("Read error");
		exit (EXIT_FAILURE);
	}
}

static const char
short_options [] = "cdefhlno:prsvwPTV";

#ifdef HAVE_GETOPT_LONG
static const struct option
long_options [] = {
	{ "dcc",	no_argument,		NULL,		'c' },
	{ "pad",	no_argument,		NULL,		'd' },
	{ "pdc-enum",	no_argument,		NULL,		'e' },
	{ "fast",	no_argument,		NULL,		'f' },
	{ "help",	no_argument,		NULL,		'h' },
	{ "links",	no_argument,		NULL,		'l' },
	{ "nav",	no_argument,		NULL,		'n' },
	{ "output",	required_argument,	NULL,		'o' },
	{ "pdc",	no_argument,		NULL,		'p' },
	{ "row-update", no_argument,		NULL,		'r' },
	{ "stream",	no_argument,		NULL,		's' },
	{ "verbose",	no_argument,		NULL,		'v' },
	{ "wait",	no_argument,		NULL,		'w' },
	{ "pes",	no_argument,		NULL,		'P' },
	{ "dump-pg",	no_argument,		NULL,		'T' },
	{ "version",	no_argument,		NULL,		'V' },
	{ NULL, 0, 0, 0 }
};
#else
#  define getopt_long(ac, av, s, l, i) getopt(ac, av, s)
#endif

static void
usage				(FILE *			fp)
{
	fprintf (fp, "\
Libzvbi test/export version " VERSION "\n\
Copyright (C) 2004-2005 Michael H. Schimek\n\
This program is licensed under GPL 2. NO WARRANTIES.\n\n\
Exports a Teletext or Closed Caption page in various formats.\n\n\
Usage: %s [options] format page < sliced vbi data > file\n\n\
Currently supported formats are:\n\
html           HTML page\n\
mpsub          MPSub (MPlayer) subtitle file\n\
png            PNG image\n\
ppm            PPM (raw RGB) image\n\
qttext         QuickTime Text subtitle/caption file\n\
realtext       RealText subtitle/caption file\n\
sami           SAMI 1.0 subtitle/caption file\n\
subrip         SubRip subtitle file\n\
subviewer      SubViewer 2.x subtitle file\n\
text           Plain text\n\
vtx            VTX file (VideoteXt application)\n\
               Most formats have options you can set by appending them as\n\
	       a comma separated list, e.g. text,charset=UTF-8,... Try\n\
               test/explist for an overview.\n\
Valid page numbers are:\n\
1 ... 8        Closed Caption channel 1 ... 4, text channel 1 ... 4.\n\
100 ... 899    Teletext page 100 ... 899.\n\
0              All Teletext pages. In this case files are stored in the\n\
               current directory rather than sent to stdout. Not useful\n\
               with caption/subtitle formats.\n\
Input options:\n\
-P | --pes     Source is a DVB PES (autodetected when it starts with a PES\n\
               packet header), otherwise test/capture --sliced output.\n\
-f | --fast    Don't wait for pages which are currently not in\n\
               transmission.\n\
-w | --wait n  Start at the second (third, fourth, ...) transmission of\n\
               this page.\n\
Formatting options:\n\
-n | --nav     Add TOP or FLOF navigation elements to Teletext pages.\n\
-p | --pad     Add an extra column to Teletext pages for a more balanced\n\
               view, spaces to Caption pages for readability.\n\
Export options:\n\
-e | --pdc-enum Print additional Teletext PDC information.\n\
-o | --output name  Write the page to this file. The page number and\n\
                    a suitable .ext will be appended if necessary\n\
-l | --links   Turn http, smtp, etc links on a Teletext page into\n\
               hyperlinks in HTML output.\n\
-p | --pdc     Turn PDC markup on a Teletext page into hyperlinks.\n\
-r | --row-update Export a page only when a row is complete, not on every\n\
               new character. Has only an effect on roll-up and paint-on\n\
               style caption.\n\
-s | --stream  Export all (rather than one) transmissions of this page\n\
               to a single file. This is the default for caption/subtitle\n\
               formats, the option toggles. Not really useful with other\n\
               formats.\n\
-T | --dump-pg Dump the vbi_page being exported, for debugging.\n\
Miscellaneous options:\n\
-h | --help    Print this message.\n\
-v | --verbose Increase verbosity (progress messages).\n\
-V | --version Print the version number of this program.\n\
",
		 my_name);
}

static void
output_option			(void)
{
	assert (NULL != optarg);

	free (filename_prefix);
	filename_prefix = NULL;

	free (filename_suffix);
	filename_suffix = NULL;

	if (0 == strcmp (optarg, "-")) {
	} else {
		char *s;

		s = strchr (optarg, '.');
		if (NULL == s) {
			filename_prefix = strdup (optarg);
			assert (NULL != filename_prefix);
		} else {
			filename_prefix = strndup (optarg, s - optarg);
			assert (NULL != filename_prefix);

			filename_suffix = strdup (s);
			assert (NULL != filename_suffix);
		}
	}
}

static vbi3_bool
strtopgnum			(vbi3_pgno *		pgno,
				 vbi3_subno *		subno,
				 const char **		s1)
{
	const char *s;
	char *end;

	s = *s1;

	while (*s && isspace (*s))
		++s;

	*pgno = strtoul (s, &end, 16);
	*subno = VBI3_ANY_SUBNO;
	s = end;

	if (0 == *pgno)
		return TRUE;

	if (!vbi3_is_bcd (*pgno))
		return FALSE;

	if (*pgno >= 1 && *pgno <= 8) {
	} else if (*pgno >= 0x100 && *pgno <= 0x899) {
		if ('.' == *s) {
			++s;

			*subno = strtoul (s, &end, 16);
   			s = end;

			if (!vbi3_is_bcd (*subno))
				return FALSE;

			if (*subno < 0x00 || *subno > 0x79)
				return FALSE;
		}
	} else {
		return FALSE;
	}

	*s1 = s;

	return TRUE;
}

static unsigned int
pgnos_from_argv			(char **		argv,
				 int			argc)
{
	while (argc-- > 0) {
		const char *s;
		vbi3_pgno first_pgno;
		vbi3_subno first_subno;
		vbi3_pgno last_pgno;
		vbi3_subno last_subno;

		s = *argv++;

		if (!strtopgnum (&first_pgno, &first_subno, &s)) {
			usage (stderr);
			exit (EXIT_FAILURE);
		}

		if (0 == first_pgno) {
			return 0;
		}

		last_pgno = first_pgno;
		last_subno = first_subno;

		while (*s && isspace (*s)) {
			++s;
		}

		if ('-' == *s) {
			++s;

			if (!strtopgnum (&last_pgno, &last_subno, &s)) {
				usage (stderr);
				exit (EXIT_FAILURE);
			}

			if (0 == last_pgno) {
				usage (stderr);
				exit (EXIT_FAILURE);
			}

			if (last_pgno < first_pgno
			    || last_subno < first_subno) {
				SWAP (first_pgno, last_pgno);
				SWAP (first_subno, last_subno);
			}
		}

		if (VBI3_ANY_SUBNO == first_subno) {
			if (VBI3_ANY_SUBNO != last_subno) {
				usage (stderr);
				exit (EXIT_FAILURE);
			}

			do {
				pgnos_add (first_pgno, first_subno);
				first_pgno = vbi3_add_bcd (first_pgno, 0x001);
			} while (first_pgno <= last_pgno);
		} else {
			if (first_pgno != last_pgno
			    || VBI3_ANY_SUBNO == last_subno) {
				usage (stderr);
				exit (EXIT_FAILURE);
			}

			do {
				pgnos_add (first_pgno, first_subno);
				first_subno = vbi3_add_bcd (first_subno, 0x01);
			} while (first_subno <= last_subno);
		}
	}

	return n_pgnos;
}

int
main				(int			argc,
				 char **		argv)
{
	char *module, *t;
	const vbi3_export_info *xi;
	vbi3_bool success;
	vbi3_event_mask events;
	int index;
	int c;

	setlocale (LC_ALL, "");

	my_name = argv[0];

	while (-1 != (c = getopt_long (argc, argv, short_options,
				       long_options, &index))) {
		switch (c) {
		case 0:
			break;

		case 'c':
			option_dcc ^= TRUE;
			break;

		case 'd':
			option_padding ^= TRUE;
			break;

		case 'e':
			option_enum ^= TRUE;
			break;

		case 'f':
			option_fast ^= TRUE;
			break;

		case 'h':
			usage (stdout);
			exit (EXIT_SUCCESS);

		case 'l':
			option_hyperlinks ^= TRUE;
			break;

		case 'n':
			option_navigation ^= TRUE;
			break;

		case 'o':
			output_option ();
			break;

		case 'p':
			option_pdc_links ^= TRUE;
			break;

		case 'r':
			option_row_update ^= TRUE;
			break;

		case 's':
			option_stream ^= TRUE;
			break;

		case 'v':
			++verbose;
			break;

		case 'w':
			delay += 1;
			break;

		case 'P':
			source_pes ^= TRUE;
			break;

		case 'T':
			option_dump_pg ^= TRUE;
			break;

		case 'V':
			printf (VERSION "\n");
			exit (EXIT_SUCCESS);

		default:
			usage (stderr);
			exit (EXIT_FAILURE);
		}
	}

	if (argc - optind < 2) {
		usage (stderr);
		exit (EXIT_FAILURE);
	}

	option_pdc_links |= option_enum;

	module = argv[optind++];
	e = vbi3_export_new (module, &t);
	if (NULL == e) {
		fprintf (stderr, "Failed to open export module '%s': %s\n",
			 module, t);
		exit (EXIT_FAILURE);
	}

	pgnos_from_argv (&argv[optind], argc - optind);
	left_pgnos = n_pgnos;

	if (isatty (STDIN_FILENO)) {
		fprintf (stderr, "No vbi data on stdin\n");
		exit (EXIT_FAILURE);
	}

	cr = isatty (STDERR_FILENO) ? '\r' : '\n';

	if (0 == strncmp (module, "html", 4)) {
		if (option_hyperlinks)
			vbi3_export_set_link_cb (e, export_link, NULL);

		if (option_pdc_links)
			vbi3_export_set_pdc_cb (e, export_pdc, NULL);
	}

	xi = vbi3_export_info_from_export (e);
	assert (NULL != xi);

	extens = strdup (xi->extension);
	assert (NULL != extens);

	extens = strtok_r (extens, ",", &t);

	if (xi->open_format)
		option_stream ^= TRUE;

	vbi = vbi3_decoder_new (NULL, NULL, VBI3_VIDEOSTD_SET_625_50);
	assert (NULL != vbi);

	vbi3_decoder_detect_channel_change (vbi, option_dcc);

	events = (VBI3_EVENT_TTX_PAGE |
		  VBI3_EVENT_CC_PAGE);

	if (option_fast) {
		events |= VBI3_EVENT_PAGE_TYPE;
	}

	success = vbi3_decoder_add_event_handler (vbi, events, handler,
						  /* user_data */ NULL);
	assert (success);

	c = getchar ();
	ungetc (c, stdin);

	if (0 == c || source_pes) {
		dx = _vbi3_dvb_pes_demux_new (/* callback */ NULL, NULL);
		assert (NULL != dx);

		pes_mainloop ();
	} else {
		old_mainloop ();
	}

	if (xi->open_format) {
		/* Finalize. */
		/* XXX error check */
		vbi3_export_stdio (e, stdout, NULL);
	}

	if (0 == n_pgnos) {
		quit = TRUE; /* end of file ok */
	}

	if (!quit && verbose) {
		fprintf (stderr, "\nEnd of stream, "
			 "requested pages not found\n");
	}

	exit (EXIT_SUCCESS);
}
