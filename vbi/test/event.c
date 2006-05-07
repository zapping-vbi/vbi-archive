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

/* $Id: event.c,v 1.1.2.2 2006-05-07 06:05:00 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#ifdef HAVE_GETOPT_LONG
#  include <getopt.h>
#endif

#include "src/zvbi.h"

#define _(x) x /* TODO */

#define PROGRAM_NAME "zvbi-event"

static vbi3_dvb_demux *		dx;
static vbi3_decoder *		dec;
static vbi3_teletext_decoder *	td;
static vbi3_caption_decoder *	cd;

static vbi3_bool		source_is_pes;

static vbi3_bool		option_header;

#ifndef HAVE_PROGRAM_INVOCATION_NAME
static char *			program_invocation_name;
static char *			program_invocation_short_name;
#endif

static void
error_exit			(const char *		template,
				 ...)
{
	va_list ap;

	fprintf (stderr, "%s: ", program_invocation_short_name);

	va_start (ap, template);
	vfprintf (stderr, template, ap);
	va_end (ap);         

	fputc ('\n', stderr);

	exit (EXIT_FAILURE);
}

static void
no_mem_exit			(void)
{
	error_exit (_("Out of memory."));
}

static void
dump_stats			(void)
{
	vbi3_pgno pgno;
	vbi3_ttx_page_stat ps;

	for (pgno = 0x100; pgno <= 0x8FF; pgno += 0x001) {
		vbi3_bool success;

		success = vbi3_teletext_decoder_get_ttx_page_stat
			(td, &ps, /* network: current */ NULL, pgno);
		if (!success)
			continue;

		printf ("  Page %03x: %02x=%s %02x subp=%d (%02x-%02x)\n",
			pgno,
			ps.page_type,
			vbi3_page_type_name (ps.page_type),
			ps.character_set ? ps.character_set->code : 0,
			ps.subpages,
			ps.subno_min,
			ps.subno_max);
	}
}

static void
dump_top			(void)
{
	vbi3_top_title *tt;
	unsigned int n_titles;
	unsigned int i;

	tt = vbi3_teletext_decoder_get_top_titles
		(td, /* network: current */ NULL, &n_titles);
	if (NULL == tt) {
		printf ("  No TOP titles.\n");
		return;
	}

	for (i = 0; i < n_titles; ++i) {
		printf ("  TOP title %u: %3x.%04x group=%u '",
			i, tt[i].pgno,
			tt[i].subno,
			tt[i].group);
		vbi3_fputs_locale_utf8 (stdout, tt[i].title_,
					VBI3_NUL_TERMINATED);
		puts ("'");
	}

	vbi3_top_title_array_delete (tt, n_titles);
}

static vbi3_bool
handler				(const vbi3_event *	ev,
				 void *			user_data)
{
	static vbi3_bool closed = FALSE;

	user_data = user_data; /* unused */

	/* Must not send anything after close event. */
	assert (!closed);

	if (option_header) {
		printf ("Event time=%f network=%p ",
			ev->timestamp, ev->network);

		_vbi3_network_dump (ev->network, stdout);

		putchar ('\n');
	}

	switch (ev->type) {
	case VBI3_EVENT_NONE:
		printf ("NONE\n");
		assert (0);

	case VBI3_EVENT_CLOSE:
		printf ("CLOSE\n");
		closed = TRUE;
		break;

	case VBI3_EVENT_RESET:
		printf ("RESET\n");
		break;

	case VBI3_EVENT_TTX_PAGE:
		printf ("TTX_PAGE %03x.%02x "
			"roll=%u news=%u subt=%u upd=%u serial=%u\n",
			ev->ev.ttx_page.pgno,
			ev->ev.ttx_page.subno,
			!!(ev->ev.ttx_page.flags & VBI3_ROLL_HEADER),
			!!(ev->ev.ttx_page.flags & VBI3_NEWSFLASH),
			!!(ev->ev.ttx_page.flags & VBI3_SUBTITLE),
			!!(ev->ev.ttx_page.flags & VBI3_UPDATE),
			!!(ev->ev.ttx_page.flags & VBI3_SERIAL));

		assert (NULL != td);

		break;

	case VBI3_EVENT_CC_RAW:
	{
		unsigned int i;

		printf ("CC_RAW ch=%u row=%u text='",
			ev->ev.cc_raw.channel,
			ev->ev.cc_raw.row);

		for (i = 0; i < ev->ev.cc_raw.length; ++i) {
			int c;

			c = vbi3_printable (ev->ev.cc_raw.text[i].unicode);
			putchar (c);
		}

		puts ("'");

		assert (NULL != cd);

		break;
	}

	case VBI3_EVENT_CC_PAGE:
		printf ("CC_PAGE ch=%u char_update=%u word=%u "
			"row=%u page_update=%u start_rolling=%u\n",
			ev->ev.caption.channel,
			!!(ev->ev.caption.flags & VBI3_CHAR_UPDATE),
			!!(ev->ev.caption.flags & VBI3_WORD_UPDATE),
			!!(ev->ev.caption.flags & VBI3_ROW_UPDATE),
			!!(ev->ev.caption.flags & VBI3_PAGE_UPDATE),
			!!(ev->ev.caption.flags & VBI3_START_ROLLING));

		assert (NULL != cd);

		break;

	case VBI3_EVENT_NETWORK:
		printf ("NETWORK ");

		if (option_header) {
			/* Already printed. */
		} else {
			_vbi3_network_dump (ev->network, stdout);
		}

		putchar ('\n');

		break;

	case VBI3_EVENT_TRIGGER:
		printf ("TRIGGER ");
		_vbi3_link_dump (ev->ev.trigger, stdout);
		assert (NULL != dec);
		break;

	case VBI3_EVENT_ASPECT:
		printf ("ASPECT ");
		_vbi3_aspect_ratio_dump (ev->ev.aspect, stdout);
		putchar ('\n');
		break;

	case VBI3_EVENT_PROG_INFO:
		printf ("PROG_INFO\n");
		_vbi3_program_info_dump (ev->ev.prog_info, stdout);
		break;

	case VBI3_EVENT_PAGE_TYPE:
		printf ("PAGE_TYPE\n");
		dump_stats ();
		break;

	case VBI3_EVENT_TOP_CHANGE:
		printf ("TOP_CHANGE\n");
		dump_top ();
		assert (NULL != td);
		break;

	case VBI3_EVENT_LOCAL_TIME:
	{
		struct tm tm;

		printf ("LOCAL_TIME time=%f gmtoff=%d ",
			(double) ev->ev.local_time.time,
			ev->ev.local_time.gmtoff);

		gmtime_r (&ev->ev.local_time.time, &tm);

		printf ("(%4u-%02u-%02u %02u:%02u:%02u UTC)\n",
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec);

		break;
	}

	case VBI3_EVENT_PROG_ID:
		printf ("PROG_ID\n");
		_vbi3_program_id_dump (ev->ev.prog_id, stdout);
		putchar ('\n');
		break;

	case VBI3_EVENT_TIMER:
		printf ("TIMER\n");
		break;

	default:
		printf ("Unknown event 0x%x\n", ev->type);
		assert (0);
	}

	return FALSE; /* not handled, pass on */
}

static void
decode				(const vbi3_sliced *	sliced,
				 unsigned int		n_lines,
				 double			sample_time,
				 int64_t		stream_time)
{
	stream_time = stream_time; /* unused */

	if (dec) {
		vbi3_decoder_feed (dec, sliced, n_lines, sample_time);
	} else if (td) {
		unsigned int i;

		for (i = 0; i < n_lines; ++i) {
			if (sliced[i].id & VBI3_SLICED_TELETEXT_B)
				vbi3_teletext_decoder_feed
					(td, sliced[i].data, sample_time);
		}
	} else if (cd) {
		unsigned int i;

		for (i = 0; i < n_lines; ++i) {
			if (sliced[i].id & VBI3_SLICED_CAPTION_525)
				vbi3_caption_decoder_feed
					(cd, sliced[i].data,
					 sliced[i].line, sample_time);
		}
	} else {
		assert (0);
	}
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
			unsigned int n_lines;
			int64_t pts;

			n_lines = _vbi3_dvb_demux_cor (dx,
						    sliced, 64,
						    &pts,
						    &bp, &left);
			if (n_lines > 0)
				decode (sliced, n_lines, 0, pts);
		}
	}

	fprintf (stderr, "\rEnd of stream\n");
}

static void
old_mainloop			(void)
{
	double time;

	time = 0.0;

	for (;;) {
		char buf[256];
		double dt;
		unsigned int n_items;
		vbi3_sliced sliced[40];
		vbi3_sliced *s;

		if (ferror (stdin) || !fgets (buf, 255, stdin))
			goto abort;

		dt = strtod (buf, NULL);
		n_items = fgetc (stdin);

		assert (n_items < 40);

		s = sliced;

		while (n_items-- > 0) {
			int index;

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
				fread(s->data, 1, 2, stdin);
				break;

			default:
				fprintf (stderr,
					 "\nOops! Unknown data type %d "
					 "in sample file\n", index);
				exit (EXIT_FAILURE);
			}

			++s;
		}

		decode (sliced, s - sliced, time, 0);

		if (feof (stdin) || ferror (stdin))
			goto abort;

		time += dt;
	}

	return;

abort:
	fprintf (stderr, "\rEnd of stream\n");
}

static void
usage				(FILE *			fp)
{
	fprintf (fp, _("\
%s %s -- VBI decoder event test\n\n\
Copyright (C) 2004, 2006 Michael H. Schimek\n\
This program is licensed under GPL 2 or later. NO WARRANTIES.\n\n\
Usage: %s [options] < sliced VBI data\n\n\
-P | --pes        Source is a DVB PES\n\
\n\
-1 | --teletext   Use only teletext decoder\n\
-2 | --caption    Use only caption decoder\n\
\n\
-a                Print all events\n\
-c | --caption    VBI3_EVENT_CAPTION\n\
-f | --prog_info  VBI3_EVENT_PROG_INFO\n\
-g | --trigger    VBI3_EVENT_TRIGGER\n\
-i | --prog_id    VBI3_EVENT_PROG_ID\n\
-l | --local_time VBI3_EVENT_LOCAL_TIME\n\
-m | --timer      VBI3_EVENT_TIMER\n\
-n | --network    VBI3_EVENT_NETWORK\n\
-o | --close      VBI3_EVENT_CLOSE\n\
-p | --top_change VBI3_EVENT_TOP_CHANGE\n\
-r | --reset      VBI3_EVENT_RESET\n\
-s | --aspect     VBI3_EVENT_ASPECT\n\
-t | --ttx_page   VBI3_EVENT_TTX_PAGE\n\
-y | --page_type  VBI3_EVENT_PAGE_TYPE\n\
\n\
-d | --header     Print event header\n\
"),
		 PROGRAM_NAME, VERSION, program_invocation_name);
}

static const char
short_options [] = "12acdfghilmnoprstyP";

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
	{ "timer",	no_argument,		NULL,		'm' },
	{ "network",	no_argument,		NULL,		'n' },
	{ "close",	no_argument,		NULL,		'o' },
	{ "top_change",	no_argument,		NULL,		'p' },
	{ "reset",	no_argument,		NULL,		'r' },
	{ "aspect",	no_argument,		NULL,		's' },
	{ "ttx_page",	no_argument,		NULL,		't' },
	{ "page_type",	no_argument,		NULL,		'y' },
	{ "pes",	no_argument,		NULL,		'P' },
	{ NULL, 0, 0, 0 }
};
#else
#define getopt_long(ac, av, s, l, i) getopt(ac, av, s)
#endif

static int			option_index;

int
main				(int			argc,
				 char **		argv)
{
	unsigned int events;
	int decoder;
	int c;

#ifndef HAVE_PROGRAM_INVOCATION_NAME
	{
		unsigned int i;

		for (i = strlen (argv[0]); i > 0; --i) {
			if ('/' == argv[0][i - 1])
				break;
		}

		program_invocation_short_name = &argv[0][i];
	}
#endif

	setlocale (LC_ALL, "");

	events = 0;

	for (;;) {
		int c;

		c = getopt_long (argc, argv, short_options,
				 long_options, &option_index);
		if (-1 == c)
			break;

		switch (c) {
		case 0: /* getopt_long() flag */
			break;

		case '1':
		case '2':
			decoder = c - '0';
			break;

		case 'a':
			events ^= -1;
			break;

		case 'c':
			events ^= VBI3_EVENT_CC_PAGE;
			break;

		case 'd':
			option_header ^= TRUE;
			break;

		case 'f':
			events ^= VBI3_EVENT_PROG_INFO;
			break;

		case 'g':
			events ^= VBI3_EVENT_TRIGGER;
			break;

		case 'h':
			usage (stdout);
			exit (EXIT_SUCCESS);

		case 'i':
			events ^= VBI3_EVENT_PROG_ID;
			break;

		case 'l':
			events ^= VBI3_EVENT_LOCAL_TIME;
			break;

		case 'm':
			events ^= VBI3_EVENT_TIMER;
			break;

		case 'n':
			events ^= VBI3_EVENT_NETWORK;
			break;

		case 'o':
			events ^= VBI3_EVENT_CLOSE;
			break;

		case 'p':
			events ^= VBI3_EVENT_TOP_CHANGE;
			break;

		case 'r':
			events ^= VBI3_EVENT_RESET;
			break;

		case 's':
			events ^= VBI3_EVENT_ASPECT;
			break;

		case 't':
			events ^= VBI3_EVENT_TTX_PAGE;
			break;

		case 'y':
			events ^= VBI3_EVENT_PAGE_TYPE;
			break;

		case 'P':
			source_is_pes ^= TRUE;
			break;

		default:
			usage (stderr);
			exit (EXIT_FAILURE);
		}
	}

	if (0 == events) {
		usage (stderr);
		exit (EXIT_FAILURE);
	}

	if (isatty (STDIN_FILENO))
		error_exit (_("No VBI data on standard input."));

	switch (decoder) {
		vbi3_bool success;

	case 0:
		dec = vbi3_decoder_new (/* cache */ NULL,
					/* network */ NULL,
					VBI3_VIDEOSTD_SET_625_50);
		if (NULL == dec)
			no_mem_exit ();

		td = vbi3_decoder_cast_to_teletext_decoder (dec);
		cd = vbi3_decoder_cast_to_caption_decoder (dec);

		success = vbi3_decoder_add_event_handler
			(dec, events, handler, /* user_data */ NULL);
		if (!success)
			no_mem_exit ();

		break;

	case 1:
		td = vbi3_teletext_decoder_new (/* cache */ NULL,
						/* network */ NULL,
						VBI3_VIDEOSTD_SET_625_50);
		if (NULL == td)
			no_mem_exit ();

		success = vbi3_teletext_decoder_add_event_handler
			(td, events, handler, /* user_data */ NULL);
		if (!success)
			no_mem_exit ();

		break;

	case 2:
		cd = vbi3_caption_decoder_new (/* cache */ NULL,
					       /* network */ NULL,
					       VBI3_VIDEOSTD_SET_625_50);
		if (NULL == cd)
			no_mem_exit ();

		success = vbi3_caption_decoder_add_event_handler
			(cd, events, handler, /* user_data */ NULL);
		if (!success)
			no_mem_exit ();

		break;

	default:
		assert (0);
	}

	c = getchar ();
	ungetc (c, stdin);

	if (0 == c || source_is_pes) {
		dx = _vbi3_dvb_pes_demux_new (/* callback */ NULL,
					      /* used_data */ NULL);
		if (NULL == dx)
			no_mem_exit ();

		pes_mainloop ();
	} else {
		old_mainloop ();
	}

	exit (EXIT_SUCCESS);

	return 0;
}
