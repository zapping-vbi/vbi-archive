/*
 *  libzvbi test
 *
 *  Copyright (C) 2004 Michael H. Schimek
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

/* $Id: sliced2pes.c,v 1.3.2.2 2006-05-07 06:05:00 mschimek Exp $ */

#undef NDEBUG

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <assert.h>
#include <unistd.h>
#include <time.h>
#ifdef HAVE_GETOPT_LONG
#include <getopt.h>
#endif

#include "src/zvbi.h"

static vbi3_dvb_mux *		mx;

static vbi3_bool
binary_ts_pes			(vbi3_dvb_mux *		mx,
				 void *			user_data,
				 const uint8_t *	packet,
				 unsigned int		packet_size)
{
	fwrite (packet, 1, packet_size, stdout);
	return TRUE;
}

static void
mainloop			(void)
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

		/* XXX shouldn't use system time. */
		_vbi3_dvb_mux_mux (mx, sliced, s - sliced, -1, time * 90000);

		if (feof (stdin) || ferror (stdin))
			goto abort;

		time += dt;
	}

	return;

abort:
	fflush (stdout);

	fprintf (stderr, "\rEnd of stream\n");
}

static const char
short_options [] = "h";

#ifdef HAVE_GETOPT_LONG
static const struct option
long_options [] = {
	{ "help",	no_argument,		NULL,		'h' },
	{ NULL, 0, 0, 0 }
};
#else
#define getopt_long(ac, av, s, l, i) getopt(ac, av, s)
#endif

static void
usage				(FILE *			fp,
				 char **		argv)
{
	fprintf (fp,
		 "Usage: %s < sliced vbi data > pes stream\n",
		 argv[0]);
}

int
main				(int			argc,
				 char **		argv)
{
	int index;
	int c;

	setlocale (LC_ALL, "");

	while (-1 != (c = getopt_long (argc, argv, short_options,
				       long_options, &index))) {
		switch (c) {
		case 0:
			break;

		case 'h':
			usage (stdout, argv);
			exit (EXIT_SUCCESS);

		default:
			usage (stderr, argv);
			exit (EXIT_FAILURE);
		}
	}

	if (isatty (STDIN_FILENO)) {
		fprintf (stderr, "No vbi data on stdin\n");
		exit (EXIT_FAILURE);
	}

	mx = _vbi3_dvb_mux_pes_new (/* data_identifier */ 0x10,
				   /* packet_size */ 8 * 184,
				   VBI3_VIDEOSTD_SET_625_50,
				   binary_ts_pes,
				   /* user_data */ NULL);
	assert (NULL != mx);

	mainloop ();

	exit (EXIT_SUCCESS);
}
