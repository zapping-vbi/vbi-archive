/*
 *  Copyright (C) 2006 Michael H. Schimek
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

/* $Id: lut_rgb16-gen.c,v 1.3 2007-08-30 14:14:09 mschimek Exp $ */

/*  Generates look-up tables for image format conversion. */

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>		/* uint16_t */
#include "misc.h"		/* SWAB16() */

static uint16_t			lut_rgb16[2][6][256];

int
main				(void)
{
	unsigned int i;

	for (i = 0; i < 256; ++i) {
		unsigned int n;

		n = (i << 8) & 0xF800;
		lut_rgb16[0][0][i] = n; /* BGR red / RGB blue  */
		lut_rgb16[1][0][i] = SWAB16 (n);

		n = (i << 7) & 0x7C00;
		lut_rgb16[0][1][i] = n; /* BGRA red / ARGB blue */
		lut_rgb16[1][1][i] = SWAB16 (n);

		n = (i << 3) & 0x07E0;
		lut_rgb16[0][2][i] = n; /* BGR / RGB green */
		lut_rgb16[1][2][i] = SWAB16 (n);

		n = (i << 2) & 0x03E0;
		lut_rgb16[0][3][i] = n; /* BGRA / RGBA green */
		lut_rgb16[1][3][i] = SWAB16 (n);

		n = (i >> 3) & 0x001F;
		lut_rgb16[0][4][i] = n; /* blue / red */
		lut_rgb16[1][4][i] = SWAB16 (n);

		n = (i << 8) & 0x8000;
		lut_rgb16[0][5][i] = n; /* alpha */
		lut_rgb16[1][5][i] = SWAB16 (n);
	}

	fputs ("/* Generated file, do not edit! */\n"
	       "\n"
	       "#include <inttypes.h>\n"
	       "\n"
	       "const uint16_t\n"
	       "_tv_lut_rgb16 [2][6][256] = {\n\t", stdout);

	for (i = 0; i < 2; ++i) {
		unsigned int j;

		fputs ("{\n\t\t", stdout);
		for (j = 0; j < 6; ++j) {
			unsigned int k;

			fputs ("{", stdout);
			for (k = 0; k < 256; k += 8) {
				unsigned int l;

				fputs ("\n\t\t\t", stdout);
				for (l = 0; l < 8; ++l) {
					printf ("0x%04x, ",
						lut_rgb16[i][j][k + l]);
				}
			}

			fputs ("\n\t\t}, ", stdout);
		}

		fputs ("\n\t}, ", stdout);
	}

	fputs ("\n};\n\n", stdout);

	exit (EXIT_SUCCESS);

	return 0;
}

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
