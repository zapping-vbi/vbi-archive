/*
 *  zvbi-ttxfilter -- Teletext filter
 *
 *  Copyright (C) 2005-2006 Michael H. Schimek
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

/* $Id: ttxfilter.c,v 1.2.2.3 2006-05-26 00:43:07 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <locale.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>
#ifdef HAVE_GETOPT_LONG
#  include <getopt.h>
#endif

#include "src/dvb_demux.h"
#include "src/packet_filter.h"

#include "sliced.h"

#define _(x) x /* TODO */

#define PROGRAM_NAME "zvbi-ttxfilter"

static vbi3_dvb_demux *		dx;
static vbi3_packet_filter *	pf;

static vbi3_bool		source_is_pes;

static double			start_time;
static double			end_time;

static vbi3_bool		option_keep_system_pages;

/* Data is all zero, hopefully ignored due to hamming and parity error. */
static vbi3_sliced		sliced_blank;

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
invalid_pgno_exit		(const char *		arg)
{
	error_exit (_("Invalid page number '%s'."), arg);
}

static void
no_mem_exit			(void)
{
	error_exit (_("Out of memory."));
}

static void
pes_frame			(const uint8_t *	bp,
				 unsigned int *		left,
				 vbi3_bool *		start)
{
	vbi3_sliced sliced_in[64];
	vbi3_sliced sliced_out[64];
	vbi3_sliced *sp;
	unsigned int n_lines_in;
	unsigned int n_lines_out;
	int64_t pts;
	vbi3_bool success;

	n_lines_in = vbi3_dvb_demux_cor (dx,
					  sliced_in,
					  /* max_lines */ 64,
					  &pts,
					  bp, left);
	if (0 == n_lines_in)
		return;

	if (*start) {
		start_time = start_time * 90000 + pts;
		end_time = end_time * 90000 + pts;
		*start = FALSE;
	}

	n_lines_out = 0;

	if (pts >= start_time
	    && pts < end_time) {
		success = vbi3_packet_filter_cor (pf,
						  sliced_out, &n_lines_out,
						  /* max_lines_out */ 64,
						  sliced_in, n_lines_in);
		if (success) {
			sp = sliced_out;
		} else {
			/* Don't spoil recordings. */
			fprintf (stderr, "%s: %s\n",
				 program_invocation_short_name,
				 vbi3_packet_filter_errstr (pf));
		
			sp = sliced_in;
			n_lines_out = n_lines_in;
		}
	}

	if (n_lines_out > 0) {
		fprintf (stderr, "OOPS!\n");
		/* Write sp, n_lines_out here. */
		exit (EXIT_FAILURE);
	}
}

static void
pes_mainloop			(void)
{
	uint8_t buffer[2048];
	vbi3_bool start;

	start = TRUE;

	while (1 == fread (buffer, sizeof (buffer), 1, stdin)) {
		const uint8_t *bp;
		unsigned int left;

		bp = buffer;
		left = sizeof (buffer);

		while (left > 0) {
			pes_frame (bp, &left, &start);
		}
	}

	fprintf (stderr, "\rEnd of stream\n");
}

static void
old_mainloop			(void)
{
	vbi3_bool start = TRUE;

	for (;;) {
		vbi3_sliced sliced_in[64];
		vbi3_sliced sliced_out[64];
		vbi3_sliced *sp;
		unsigned int n_lines_in;
		unsigned int n_lines_out;
		double timestamp;
		vbi3_bool success;

		n_lines_in = read_sliced (sliced_in,
					  &timestamp,
					  /* max_lines */ 64);
		if ((int) n_lines_in < 0)
			break; /* eof */

		if (start) {
			start_time += timestamp;
			end_time += timestamp;
			start = FALSE;
		}

		n_lines_out = 0;

		if (timestamp >= start_time
		    && timestamp < end_time) {
			success = vbi3_packet_filter_cor
				(pf,
				 sliced_out, &n_lines_out,
				 /* max_lines_out */ 64,
				 sliced_in, n_lines_in);

			if (success) {
				sp = sliced_out;
			} else {
				/* Don't spoil recordings. */
				fprintf (stderr, "%s: %s\n",
					 program_invocation_short_name,
					 vbi3_packet_filter_errstr (pf));

				sp = sliced_in;
				n_lines_out = n_lines_in;
			}
		}

		if (n_lines_out > 0) {
			success = write_sliced (sp, n_lines_out, timestamp);
			assert (success);

			fflush (stdout);
		} else if (0) {
			/* Decoder may assume data loss without
			   continuous timestamps. */
			success = write_sliced (&sliced_blank,
						/* n_lines */ 1,
						timestamp);
			assert (success);
		}
	}
}

static void
usage				(FILE *			fp)
{
	fprintf (fp, _("\
%s %s -- Teletext filter\n\n\
Copyright (C) 2005-2006 Michael H. Schimek\n\
This program is licensed under GPL 2. NO WARRANTIES.\n\n\
Usage: %s [options] [pages] < sliced vbi data > sliced vbi data\n\n\
-h | --help | --usage  Print this message and exit\n\
-t | --time from-to    Keep pages in this time interval (in seconds)\n\
-s | --system          Keep system pages (page inventories, DRCS etc)\n\
-P | --pes             Source is a DVB PES\n\
-V | --version         Print the program version and exit\n\n\
Valid page numbers are 100 to 899. You can also specify a range like\n\
150-299.\n\
"),
		 PROGRAM_NAME, VERSION, program_invocation_name);
}

static const char
short_options [] = "hst:PV";

#ifdef HAVE_GETOPT_LONG
static const struct option
long_options [] = {
	{ "help",	no_argument,		NULL,		'h' },
	{ "usage",	no_argument,		NULL,		'h' },
	{ "time",	no_argument,		NULL,		't' },
	{ "system",	no_argument,		NULL,		's' },
	{ "pes",	no_argument,		NULL,		'P' },
	{ "version",	no_argument,		NULL,		'V' },
	{ NULL, 0, 0, 0 }
};
#else
#  define getopt_long(ac, av, s, l, i) getopt(ac, av, s)
#endif

static int			option_index;

static vbi3_bool
is_valid_pgno			(vbi3_pgno		pgno)
{
	if (!vbi3_is_bcd (pgno))
		return FALSE;

	if (pgno >= 0x100 && pgno <= 0x899)
		return TRUE;

	return FALSE;
}

static void
option_time			(void)
{
	char *s = optarg;

	start_time = strtod (s, &s);

	while (isspace (*s))
		++s;

	if ('-' != *s++)
		goto invalid;

	end_time = strtod (s, &s);

	if (start_time < 0
	    || end_time < 0
	    || end_time <= start_time)
		goto invalid;

	return;

 invalid:
	fprintf (stderr, _("Invalid time range '%s'.\n"), optarg);
	usage (stderr);
	exit (EXIT_FAILURE);
}

int
main				(int			argc,
				 char **		argv)
{
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

	start_time = 0.0;
	end_time = 1e40;

	for (;;) {
		int c;

		c = getopt_long (argc, argv, short_options,
				 long_options, &option_index);
		if (-1 == c)
			break;

		switch (c) {
		case 0: /* getopt_long() flag */
			break;

		case 'h':
			usage (stdout);
			exit (EXIT_SUCCESS);

		case 's':
			option_keep_system_pages ^= TRUE;
			break;

		case 't':
			assert (NULL != optarg);
			option_time ();
			break;

		case 'P':
			source_is_pes ^= TRUE;
			break;

		case 'V':
			printf (PROGRAM_NAME " " VERSION "\n");
			exit (EXIT_SUCCESS);

		default:
			usage (stderr);
			exit (EXIT_FAILURE);
		}
	}

	pf = vbi3_packet_filter_new (/* callback */ NULL,
				     /* user_data */ NULL);
	if (NULL == pf)
		no_mem_exit ();

	vbi3_packet_filter_keep_system_pages (pf, option_keep_system_pages);

	if (argc > optind) {
		unsigned int n_pages;
		int i;

		n_pages = 0;

		for (i = optind; i < argc; ++i) {
			const char *s;
			char *end;
			vbi3_pgno first_pgno;
			vbi3_pgno last_pgno;
			vbi3_bool success;

			s = argv[i];

			first_pgno = strtoul (s, &end, 16);
			s = end;

			if (!is_valid_pgno (first_pgno))
				invalid_pgno_exit (argv[i]);

			last_pgno = first_pgno;

			while (*s && isspace (*s))
				++s;

			if ('-' == *s) {
				++s;

				while (*s && isspace (*s))
					++s;

				last_pgno = strtoul (s, &end, 16);
				s = end;

				if (!is_valid_pgno (last_pgno))
					invalid_pgno_exit (argv[i]);
			} else if (0 != *s) {
				invalid_pgno_exit (argv[i]);
			}

			success = vbi3_packet_filter_keep_pages
				(pf, first_pgno, last_pgno);
			if (!success)
				no_mem_exit ();
		}
	}

	sliced_blank.id = VBI3_SLICED_TELETEXT_B_L10_625;
	sliced_blank.line = 7;

	if (isatty (STDIN_FILENO))
		error_exit (_("No VBI data on standard input."));

	c = getchar ();
	ungetc (c, stdin);

	if (0 == c || source_is_pes) {
		dx = vbi3_dvb_pes_demux_new (/* callback */ NULL,
					      /* user_data */ NULL);
		if (NULL == dx)
			no_mem_exit ();

		pes_mainloop ();
	} else {
		struct timeval tv;
		double timestamp;
		vbi3_bool success;
		int r;

		r = gettimeofday (&tv, NULL);
		if (-1 == r)
			error_exit (_("Cannot determine system time. %s."),
				    strerror (errno));

		timestamp = tv.tv_sec + tv.tv_usec * (1 / 1e6);

		success = open_sliced_read (stdin);
		if (!success)
			error_exit (_("Cannot open input stream. %s."),
				    strerror (errno));

		success = open_sliced_write (stdout, timestamp);
		if (!success)
			error_exit (_("Cannot open output stream. %s."),
				    strerror (errno));

		old_mainloop ();
	}

	exit (EXIT_SUCCESS);
}
