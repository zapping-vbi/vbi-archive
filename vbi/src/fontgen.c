#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <ctype.h>

#include "misc.h"

#include "hamm.c" /* vbi_bit_reverse[] */

static char *			font_name;

static unsigned int		char_height;

uint8_t *			image;

static unsigned int		image_width;
static unsigned int		image_height;

static int
pbm_getc				(FILE *			fp)
{
	int c;
	
	c = getc (fp);

	if ('#' == c) {
		do c = getc (fp);
		while ('\n' != c && '\r' != c && EOF != c);
	}

	return c;
}

static int
pbm_getint			(FILE *			fp,
				 unsigned int *		val)
{
	int c;
	unsigned int t;

	do c = pbm_getc (fp);
	while (EOF != c && isspace (c));

	t = 0;

	while (isdigit (c)) {
		t = t * 10 + c - '0';
		c = pbm_getc (fp);
	}

	*val = t;

	if (EOF == c) {
		perror ("read error");
		exit (EXIT_FAILURE);
	}
}

static void
pbm_read			(void)
{
	FILE *fp;
	ssize_t n;
	ssize_t image_size;
	uint8_t buf[2];

	fp = stdin;

	n = fread (buf, sizeof (*buf), N_ELEMENTS (buf), fp);

	if (2 != n || 'P' != buf[0] || '4' != buf[1]) {
		fprintf (stderr, "source is not a .pbm file\n");
		exit (EXIT_FAILURE);
	}

	pbm_getint (fp, &image_width);
	pbm_getint (fp, &image_height);

	image_size = image_width * image_height / 8;

	image = vbi_malloc (image_size);

	if (NULL == image) {
		fprintf (stderr, "out of memory\n");
		exit (EXIT_FAILURE);
	}

	n = fread (image, 1, image_size, fp);

	if (n < image_size) {
		perror ("read error or unexpected eof");
		exit (EXIT_FAILURE);
	}
}

static void
xbm_write			(void)
{
	FILE *fp;
	unsigned int row;
	unsigned int line;

	fp = stdout;

	fprintf (fp, "/* Generated file, do not edit */\n"
		     "#define %s_width %u\n"
		     "#define %s_height %u\n"
		     "static const uint8_t %s_bits [] = {\n  ",
		 font_name, image_width * image_height / char_height,
		 font_name, char_height,
		 font_name);

	assert (0 == (image_width % 8));

	/* Note this de-interleaves the font image
	   (puts all chars in row 0) */

	for (line = 0; line < char_height; ++line) {
		for (row = 0; row < image_height; row += char_height) {
			unsigned int x;
			uint8_t *p;
			
			p = (image
			     + row * image_width / 8
			     + line * image_width / 8);

			for (x = 0; x < image_width / 8; ++x) {
				fprintf (fp, "0x%02x,",
					 vbi_bit_reverse[p[x]]);

				if (7 == (x % 8))
					fputs ("\n  ", fp);
				else
					fputc (' ', fp);
			}
		}
	}

	fputs ("};\n", fp);
}

int
main				(int			argc,
				 char **		argv)
{
	if (4 != argc) {
		fprintf (stderr, "Usage: %s format font_name char_height "
				 "<pbm-file >output\n",
				 argv[0]);
		exit (EXIT_FAILURE);
	}

	font_name = argv[2];
	char_height = atoi (argv[3]);
	
	pbm_read ();

	assert (0 == (image_height % char_height));

	if (0 == strcmp (argv[1], "xbm")) {
		/* XBM image for built-in render functions (exp-gfx.c). */
		xbm_write ();
	} else if (0 == strcmp (argv[1], "bdf")) {
		/* BDF file for X11 rendering. */
		/* TODO (s/a contrib) */
	} else if (0 == strcmp (argv[1], "ttf")) {
		/* TTF file for TT rendering of
		   G1 Block Mosaics Set and
		   G3 Smooth Mosaics and Line Drawing Set. */
		/* TODO */
	}

	exit (EXIT_SUCCESS);

	return 0;
}
