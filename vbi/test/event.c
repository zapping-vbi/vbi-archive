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

/* $Id: event.c,v 1.1.2.1 2004-07-09 16:10:55 mschimek Exp $ */

#undef NDEBUG

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#ifdef HAVE_GETOPT_LONG
#include <getopt.h>
#endif

#include "src/zvbi.h"

vbi_decoder *		vbi;
vbi_teletext_decoder *	td;
vbi_caption_decoder *	cd;
unsigned int		decoder;
vbi_bool		quit;
vbi_bool		option_header;

static void
dump_stats			(void)
{
	vbi_pgno pgno;
	vbi_ttx_page_stat ps;

	for (pgno = 0x100; pgno <= 0x8FF; pgno += 0x001) {
		vbi_bool r;

		r = vbi_decoder_get_ttx_page_stat (vbi, &ps, NULL, pgno);

		if (!r)
			continue;

		printf ("%03x: %02x=%s %02x subp=%d (%02x-%02x)\n",
			pgno,
			ps.page_type,
			vbi_ttx_page_type_name (ps.page_type),
			ps.charset_code,
			ps.subpages,
			ps.subno_min,
			ps.subno_max);
	} 
}

static void
dump_top			(void)
{
	vbi_top_title *tt;
	unsigned int size;
	unsigned int i;

	tt = vbi_decoder_get_top_titles (vbi, NULL, &size);

	if (!tt) {
		printf ("top NULL\n");
		return;
	}

	for (i = 0; i < size; ++i) {
		printf ("%2u %3x.%04x %u '%s'\n",
			i, tt[i].pgno,
			tt[i].subno,
			tt[i].group,
			tt[i].title);
	}

	vbi_top_title_array_delete (tt, size);
}

static vbi_bool
handler				(const vbi_event *	ev,
				 void *			unused)
{
	static vbi_bool closed = FALSE;

	assert (!closed);

	if (option_header) {
		printf ("Event %f %p ", ev->timestamp, ev->network);
		_vbi_network_dump (ev->network, stdout);
		putchar ('\n');
	}

	switch (ev->type) {
	case VBI_EVENT_NONE:
		printf ("NONE\n");
		assert (!"reached");

	case VBI_EVENT_CLOSE:
		printf ("CLOSE\n");
		closed = TRUE;
		break;

	case VBI_EVENT_RESET:
		printf ("RESET\n");
		break;

	case VBI_EVENT_TTX_PAGE:
		printf ("TTX_PAGE %03x.%02x "
			"roll=%u news=%u subt=%u upd=%u serial=%u\n",
			ev->ev.ttx_page.pgno,
			ev->ev.ttx_page.subno,
			!!(ev->ev.ttx_page.flags & VBI_ROLL_HEADER),
			!!(ev->ev.ttx_page.flags & VBI_NEWSFLASH),
			!!(ev->ev.ttx_page.flags & VBI_SUBTITLE),
			!!(ev->ev.ttx_page.flags & VBI_UPDATE),
			!!(ev->ev.ttx_page.flags & VBI_SERIAL));
		break;

	case VBI_EVENT_CAPTION:
		printf ("CAPTION %u\n",
			ev->ev.caption.channel);
		break;

	case VBI_EVENT_NETWORK:
		printf ("NETWORK ");
		if (!option_header)
			_vbi_network_dump (ev->network, stdout);
		putchar ('\n');
		break;

	case VBI_EVENT_TRIGGER:
		printf ("TRIGGER ");
		_vbi_link_dump (ev->ev.trigger, stdout);
		break;

	case VBI_EVENT_ASPECT:
		printf ("ASPECT ");
		_vbi_aspect_ratio_dump (ev->ev.aspect, stdout);
		putchar ('\n');
		break;

	case VBI_EVENT_PROG_INFO:
		printf ("PROG_INFO\n");
		_vbi_program_info_dump (ev->ev.prog_info, stdout);
		break;

	case VBI_EVENT_PAGE_TYPE:
		printf ("PAGE_TYPE\n");
		dump_stats ();
		break;

	case VBI_EVENT_TOP_CHANGE:
		printf ("TOP_CHANGE\n");
		dump_top ();
		break;

	case VBI_EVENT_LOCAL_TIME:
	{
		time_t time;
		struct tm tm;

		printf ("LOCAL_TIME %f %d ",
			(double) ev->ev.local_time.time,
			ev->ev.local_time.gmtoff);

		gmtime_r (&ev->ev.local_time.time, &tm);

		printf ("(%4u-%02u-%02u %02u:%02u:%02u UTC)\n",
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec);

		break;
	}

	case VBI_EVENT_PROG_ID:
		printf ("PROG_ID\n");
		_vbi_program_id_dump (ev->ev.prog_id, stdout);
		putchar ('\n');
		break;

	default:
		printf ("%u\n", ev->type);
		assert (!"reached");
	}

	return TRUE; /* pass on */
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

		vbi_decoder_decode (vbi, sliced, items, time);

		time += dt;
	}

	return;

abort:
	fprintf (stderr, "End of stream\n");
}

static const char
short_options [] = "acdfghilnoprsty";

#ifdef HAVE_GETOPT_LONG
static const struct option
long_options [] = {
	{ "teletext",	no_argument,		NULL,		'1' },
	{ "caption",	no_argument,		NULL,		'2' },
	{ "all",	no_argument,		NULL,		'a' },
	{ "caption",	no_argument,		NULL,		'c' },
	{ "header",	no_argument,		NULL,		'd' },
	{ "prog_info",	no_argument,		NULL,		'f' },
	{ "trigger",	no_argument,		NULL,		'g' },
	{ "help",	no_argument,		NULL,		'h' },
	{ "prog_id",	no_argument,		NULL,		'i' },
	{ "local_time",	no_argument,		NULL,		'l' },
	{ "network",	no_argument,		NULL,		'n' },
	{ "close",	no_argument,		NULL,		'o' },
	{ "top_change",	no_argument,		NULL,		'p' },
	{ "reset",	no_argument,		NULL,		'r' },
	{ "aspect",	no_argument,		NULL,		's' },
	{ "ttx_page",	no_argument,		NULL,		't' },
	{ "page_type",	no_argument,		NULL,		'y' },
	{ 0, 0, 0, 0 }
};
#else
#define getopt_long(ac, av, s, l, i) getopt(ac, av, s)
#endif

static void
usage				(FILE *			fp,
				 char **		argv)
{
	fprintf (fp,
		 "Usage: %s [options] < sliced vbi data\n\n"
		 "Decoding options:\n"
		 "-a		    All events\n"
		 "-c | --caption    VBI_EVENT_CAPTION\n"
		 "-f | --prog_info  VBI_EVENT_PROG_INFO\n"
		 "-g | --trigger    VBI_EVENT_TRIGGER\n"
		 "-i | --prog_id    VBI_EVENT_PROG_ID\n"
		 "-l | --local_time VBI_EVENT_LOCAL_TIME\n"
		 "-n | --network    VBI_EVENT_NETWORK\n"
		 "-o | --close      VBI_EVENT_CLOSE\n"
		 "-p | --top_change VBI_EVENT_TOP_CHANGE\n"
		 "-r | --reset      VBI_EVENT_RESET\n"
		 "-s | --aspect     VBI_EVENT_ASPECT\n"
		 "-t | --ttx_page   VBI_EVENT_TTX_PAGE\n"
		 "-y | --page_type  VBI_EVENT_PAGE_TYPE\n"
		 "\n"
		 "-1 | --teletext   Use teletext decoder\n"
		 "-2 | --caption    Use caption decoder\n"
		 "-d | --header     Print event header\n"
		 "", argv[0]);
}

int
main(int argc, char **argv)
{
	unsigned int events;
	int index;
	int c;

	events = 0;

	while (-1 != (c = getopt_long (argc, argv, short_options,
				       long_options, &index))) {
		switch (c) {
		case 0:
			break;

		case '1':
		case '2':
			decoder = c - '0';
			break;

		case 'a':
			events ^= -1;
			break;

		case 'c':
			events ^= VBI_EVENT_CAPTION;
			break;

		case 'd':
			option_header ^= TRUE;
			break;

		case 'f':
			events ^= VBI_EVENT_PROG_INFO;
			break;

		case 'g':
			events ^= VBI_EVENT_TRIGGER;
			break;

		case 'h':
			usage (stdout, argv);
			exit (EXIT_SUCCESS);
			break;

		case 'i':
			events ^= VBI_EVENT_PROG_ID;
			break;

		case 'l':
			events ^= VBI_EVENT_LOCAL_TIME;
			break;

		case 'n':
			events ^= VBI_EVENT_NETWORK;
			break;

		case 'o':
			events ^= VBI_EVENT_CLOSE;
			break;

		case 'p':
			events ^= VBI_EVENT_TOP_CHANGE;
			break;

		case 'r':
			events ^= VBI_EVENT_RESET;
			break;

		case 's':
			events ^= VBI_EVENT_ASPECT;
			break;

		case 't':
			events ^= VBI_EVENT_TTX_PAGE;
			break;

		case 'y':
			events ^= VBI_EVENT_PAGE_TYPE;
			break;

		default:
			usage (stderr, argv);
			exit (EXIT_FAILURE);
		}
	}

	if (0 == events) {
		usage (stderr, argv);
		exit (EXIT_FAILURE);
	}

	if (isatty (STDIN_FILENO)) {
		fprintf (stderr, "No VBI data on stdin\n");
		exit (EXIT_FAILURE);
	}

	switch (decoder) {
	case 0:
		assert ((vbi = vbi_decoder_new
			 (NULL, NULL, VBI_VIDEOSTD_SET_625_50)));
		assert (vbi_decoder_add_event_handler
			(vbi, events, handler, NULL));
		break;

	case 1:
	case 2:
		fprintf (stderr, "Not implemented yet\n");
		exit (EXIT_FAILURE);
	}

	stream ();

	exit (EXIT_SUCCESS);
}
