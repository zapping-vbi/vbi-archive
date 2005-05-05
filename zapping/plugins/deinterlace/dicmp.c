/*
 *  Zapping TV viewer
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

/* $Id: dicmp.c,v 1.1.2.1 2005-05-05 09:46:00 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifdef HAVE_GETOPT_LONG
#  include <getopt.h>
#endif

#include "libtv/macros.h"

static unsigned int width;
static unsigned int height;
static unsigned int size;

static const char
short_options [] = "cd:h:qw:HV";

#ifdef HAVE_GETOPT_LONG
static const struct option
long_options [] = {
	{ "continue",	no_argument,		NULL,		'c' },
	{ "maxdiff",	required_argument,	NULL,		'd' },
	{ "height",	required_argument,	NULL,		'h' },
	{ "help",	no_argument,		NULL,		'H' },
	{ "quiet",	no_argument,		NULL,		'q' },
	{ "width",	required_argument,	NULL,		'w' },
	{ "version",	no_argument,		NULL,		'V' },
	{ 0, 0, 0, 0 }
};
#else
#  define getopt_long(ac, av, s, l, i) getopt(ac, av, s)
#endif

static void
usage				(FILE *			fp,
				 char **		argv)
{
	fprintf (fp,
		 "Zapping deinterlacer test version " VERSION "\n"
		 "Copyright (C) 2004-2005 Michael H. Schimek\n"
		 "This program is licensed under GPL 2. NO WARRANTIES.\n\n"
		 "Usage: %s [options]  image file  image file\n\n"
		 "Source images must be in raw YUYV format without headers.\n"
		 "Options:\n"
		 "-d | --maxdiff Max. abs. difference of each byte\n"
		 "-h | --height  Image height (288)\n"
		 "-q | --quiet   Quiet, no output\n"
		 "-w | --width   Image width (352)\n"
		 , argv[0]);
}

static void
dump				(unsigned long		counter,
				 int			c1,
				 int			c2)
{
	static const unsigned long bpp = 2;
	unsigned long pixel;
	unsigned long col;
	unsigned long row;
	unsigned long frame;

	pixel = counter / bpp;
	col = pixel % width;
	row = (pixel / width) % height;
	frame = pixel / size;

	printf ("%9lu (%2lu:%3lu:%3lu): %02x != %02x\n",
		counter, frame, row, col, c1, c2);
}

int
main				(int			argc,
				 char **		argv)
{
	char *fname1, *fname2;
	FILE *fp1, *fp2;
	unsigned int maxdiff;
	unsigned long counter;
	int quiet;
	int cont;
	int index;
	int c;

	width = 352;
	height = 288;

	maxdiff = 0;

	quiet = FALSE;
	cont = FALSE;

	while (-1 != (c = getopt_long (argc, argv, short_options,
				       long_options, &index))) {
		switch (c) {
		case 'c':
			cont ^= TRUE;
			break;

                case 'd':
			maxdiff = strtol (optarg, NULL, 0);
			if (maxdiff > 255) {
				fprintf (stderr, "maxdiff must be in "
					 "range 0 ... 255\n");
				exit (EXIT_FAILURE);
			}
                        break;

                case 'h':
			height = strtol (optarg, NULL, 0);
			if (0 == height || 0 != (height % 16)) {
				fprintf (stderr,
					 "Height must be a multiple of 16.\n");
				exit (EXIT_FAILURE);
			}
                        break;

		case 'q':
			quiet ^= TRUE;
			break;

                case 'w':
			width = strtol (optarg, NULL, 0);
			if (0 == width || 0 != (width % 16)) {
				fprintf (stderr,
					 "Width must be a multiple of 16.\n");
				exit (EXIT_FAILURE);
			}
                        break;

		case 'H':
			usage (stdout, argv);
			exit (EXIT_SUCCESS);

		case 'V':
			printf (VERSION "\n");
			exit (EXIT_SUCCESS);

		default:
			usage (stderr, argv);
			exit (EXIT_FAILURE);
		}
	}

	size = width * height;

	if ((argc - optind) < 2) {
		usage (stderr, argv);
		exit (EXIT_FAILURE);
	}

	fname1 = argv[optind + 0];
	fname2 = argv[optind + 1];

	if (!(fp1 = fopen (fname1, "r"))) {
		fprintf (stderr, "Couldn't open %s: %d, %s\n",
			 fname1, errno, strerror (errno));
		exit (EXIT_FAILURE);
	}

	if (!(fp2 = fopen (fname2, "r"))) {
		fprintf (stderr, "Couldn't open %s: %d, %s\n",
			 fname2, errno, strerror (errno));
		exit (EXIT_FAILURE);
	}

	for (counter = 0;; ++counter) {
		int c1, c2;

		c1 = fgetc (fp1);
		c2 = fgetc (fp2);

		if ((c1 | c2) < 0) {
			if (c1 >= 0 || c2 >= 0) {
				printf ("File sizes differ\n");
				exit (EXIT_FAILURE);
			}

			exit (EXIT_SUCCESS);
		}

		if (abs (c1 - c2) > maxdiff) {
			if (!quiet)
				dump (counter, c1, c2);
			if (!cont)
				exit (EXIT_FAILURE);
		}
	}

	return EXIT_SUCCESS;
}
