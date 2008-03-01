/*
 *  libzvbi -- VBI decoder event test
 *
 *  Copyright (C) 2006, 2007 Michael H. Schimek
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301, USA.
 */

/* $Id: event.c,v 1.1.2.7 2008-03-01 15:51:16 mschimek Exp $ */

/* For libzvbi version 0.3.x. */

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

#include "src/misc.h"
#include "src/zvbi.h"

#include "sliced.h"

#undef _
#define _(x) x /* TODO */

#define PROGRAM_NAME "zvbi-event"

static const char *		option_in_file_name;
static enum file_format		option_in_file_format;
static unsigned int		option_in_ts_pid;
static vbi3_bool		option_print_header;

static struct stream *		rst;
static vbi3_decoder *		dec;
static vbi3_teletext_decoder *	td;
static vbi3_caption_decoder *	cd;

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
			ps.ttx_charset ? ps.ttx_charset->code : 0,
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
		printf ("  TOP title %u: %3x.%04x group=%u \"%s\"\n",
			i, tt[i].pgno, tt[i].subno,
			tt[i].group, tt[i].xtitle);
	}

	vbi3_top_title_array_delete (tt, n_titles);
}

static vbi3_bool
event_handler			(const vbi3_event *	ev,
				 void *			user_data)
{
	static vbi3_bool closed = FALSE;

	user_data = user_data; /* unused */

	/* We should not receive anything after a close event. */
	assert (!closed);

	if (option_print_header) {
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

			c = _vbi3_to_ascii (ev->ev.cc_raw.text[i].unicode);
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

		if (option_print_header) {
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

static vbi3_bool
decode_function			(const vbi3_sliced *	sliced,
				 unsigned int		n_lines,
				 const uint8_t *	raw,
				 const vbi3_sampling_par *sp,
				 double			sample_time,
				 int64_t		stream_time)
{
	raw = raw; /* unused */
	sp = sp;
	stream_time = stream_time;

	if (NULL != dec) {
		vbi3_decoder_feed (dec, sliced, n_lines, sample_time);
	} else if (NULL != td) {
		unsigned int i;

		for (i = 0; i < n_lines; ++i) {
			if (sliced[i].id & VBI3_SLICED_TELETEXT_B)
				vbi3_teletext_decoder_feed
					(td, sliced[i].data, sample_time);
		}
	} else if (NULL != cd) {
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

	return TRUE;
}

static void
usage				(FILE *			fp)
{
	fprintf (fp, _("\
%s %s -- VBI decoder event test\n\n\
Copyright (C) 2006, 2007 Michael H. Schimek\n\
This program is licensed under GPLv2 or later. NO WARRANTIES.\n\n\
Usage: %s [options] < sliced VBI data\n\
-h | --help | --usage  Print this message and exit\n\
-q | --quiet           Suppress progress and error messages\n\
-v | --verbose         Increase verbosity\n\
-V | --version         Print the program version and exit\n\
Input options:\n\
-i | --input name      Read the VBI data from this file instead of\n\
                       standard input\n\
-P | --pes             Source is a DVB PES stream\n\
-T | --ts pid          Source is a DVB TS stream\n\
Decoder options:\n\
-1 | --teletext        Use only Teletext decoder\n\
-2 | --caption         Use only Closed Caption decoder\n\
-d | --header          Print event header\n\
Which events to report:\n\
-a                     All events\n\
-c | --caption         VBI3_EVENT_CAPTION\n\
-d | --prog_id         VBI3_EVENT_PROG_ID\n\
-f | --prog_info       VBI3_EVENT_PROG_INFO\n\
-g | --trigger         VBI3_EVENT_TRIGGER\n\
-l | --local_time      VBI3_EVENT_LOCAL_TIME\n\
-m | --timer           VBI3_EVENT_TIMER\n\
-n | --network         VBI3_EVENT_NETWORK\n\
-o | --close           VBI3_EVENT_CLOSE\n\
-p | --top_change      VBI3_EVENT_TOP_CHANGE\n\
-r | --reset           VBI3_EVENT_RESET\n\
-s | --aspect          VBI3_EVENT_ASPECT\n\
-t | --ttx_page        VBI3_EVENT_TTX_PAGE\n\
-y | --page_type       VBI3_EVENT_PAGE_TYPE\n\
"),
		 PROGRAM_NAME, VERSION, program_invocation_name);
}

static const char
short_options [] = "12acdefghi:lmnopqrstvyPT:V";

#ifdef HAVE_GETOPT_LONG
static const struct option
long_options [] = {
	{ "teletext",	no_argument,		NULL,		'1' },
	{ "caption",	no_argument,		NULL,		'2' },
	{ "all",	no_argument,		NULL,		'a' },
	{ "caption",	no_argument,		NULL,		'c' },
	{ "prog_id",	no_argument,		NULL,		'd' },
	{ "header",	no_argument,		NULL,		'e' },
	{ "prog_info",	no_argument,		NULL,		'f' },
	{ "trigger",	no_argument,		NULL,		'g' },
	{ "help",	no_argument,		NULL,		'h' },
	{ "usage",	no_argument,		NULL,		'h' },
	{ "input",	required_argument,	NULL,		'i' },
	{ "local_time",	no_argument,		NULL,		'l' },
	{ "timer",	no_argument,		NULL,		'm' },
	{ "network",	no_argument,		NULL,		'n' },
	{ "close",	no_argument,		NULL,		'o' },
	{ "top_change",	no_argument,		NULL,		'p' },
	{ "quiet",	no_argument,		NULL,		'q' },
	{ "reset",	no_argument,		NULL,		'r' },
	{ "aspect",	no_argument,		NULL,		's' },
	{ "ttx_page",	no_argument,		NULL,		't' },
	{ "verbose",	no_argument,		NULL,		'v' },
	{ "page_type",	no_argument,		NULL,		'y' },
	{ "pes",	no_argument,		NULL,		'P' },
	{ "ts",		required_argument,	NULL,		'T' },
	{ "version",	no_argument,		NULL,		'V' },
	{ NULL, 0, 0, 0 }
};
#else
#  define getopt_long(ac, av, s, l, i) getopt(ac, av, s)
#endif

static int			option_index;

int
main				(int			argc,
				 char **		argv)
{
	unsigned int event_mask;
	unsigned int decoder;

	init_helpers (argc, argv);

	option_in_file_format = FILE_FORMAT_SLICED;

	event_mask = 0;
	decoder = 0;

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
			event_mask = -1;
			break;

		case 'c':
			event_mask ^= VBI3_EVENT_CC_PAGE;
			break;

		case 'd':
			event_mask ^= VBI3_EVENT_PROG_ID;
			break;

		case 'e':
			option_print_header = TRUE;
			break;

		case 'f':
			event_mask ^= VBI3_EVENT_PROG_INFO;
			break;

		case 'g':
			event_mask ^= VBI3_EVENT_TRIGGER;
			break;

		case 'h':
			usage (stdout);
			exit (EXIT_SUCCESS);

		case 'i':
			assert (NULL != optarg);
			option_in_file_name = optarg;
			break;

		case 'l':
			event_mask ^= VBI3_EVENT_LOCAL_TIME;
			break;

		case 'm':
			event_mask ^= VBI3_EVENT_TIMER;
			break;

		case 'n':
			event_mask ^= VBI3_EVENT_NETWORK;
			break;

		case 'o':
			event_mask ^= VBI3_EVENT_CLOSE;
			break;

		case 'p':
			event_mask ^= VBI3_EVENT_TOP_CHANGE;
			break;

		case 'q':
			parse_option_quiet ();
			break;

		case 'r':
			event_mask ^= VBI3_EVENT_RESET;
			break;

		case 's':
			event_mask ^= VBI3_EVENT_ASPECT;
			break;

		case 't':
			event_mask ^= VBI3_EVENT_TTX_PAGE;
			break;

		case 'v':
			parse_option_verbose ();
			break;

		case 'y':
			event_mask ^= VBI3_EVENT_PAGE_TYPE;
			break;

		case 'P':
			option_in_file_format = FILE_FORMAT_DVB_PES;
			break;

		case 'T':
			option_in_ts_pid = parse_option_ts ();
			option_in_file_format = FILE_FORMAT_DVB_TS;
			break;

		case 'V':
			printf (PROGRAM_NAME " " VERSION "\n");
			exit (EXIT_SUCCESS);

		default:
			usage (stderr);
			exit (EXIT_FAILURE);
		}
	}

	if (0 == event_mask) {
		event_mask = -1;
	}

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
			(dec, event_mask,
			 event_handler, /* user_data */ NULL);
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
			(td, event_mask,
			 event_handler, /* user_data */ NULL);
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
			(cd, event_mask,
			 event_handler, /* user_data */ NULL);
		if (!success)
			no_mem_exit ();

		break;

	default:
		assert (0);
	}

	rst = read_stream_new (option_in_file_name,
			       option_in_file_format,
			       option_in_ts_pid,
			       decode_function);

	stream_loop (rst);

	stream_delete (rst);
	rst = NULL;

	error_msg (_("End of stream."));

	vbi3_caption_decoder_delete (cd);
	cd = NULL;

	vbi3_teletext_decoder_delete (td);
	td = NULL;

	vbi3_decoder_delete (dec);
	dec = NULL;

	exit (EXIT_SUCCESS);
}
