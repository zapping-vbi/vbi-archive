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

/* $Id: export.c,v 1.12 2005-06-11 22:11:51 mschimek Exp $ */

#undef NDEBUG

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include "src/libzvbi.h"
#include "sliced.h"

vbi_decoder *		vbi;
vbi_bool		quit = FALSE;
vbi_pgno		pgno;
vbi_export *		ex;
char *			extension;
int			cr;
vbi_dvb_demux *		dx;

static void
handler(vbi_event *ev, void *user_data)
{
	FILE *fp;
	vbi_page page;

	user_data = user_data;

	fprintf(stderr, "%cPage %03x.%02x ",
		cr,
		ev->ev.ttx_page.pgno,
		ev->ev.ttx_page.subno & 0xFF);

	if (pgno != -1 && ev->ev.ttx_page.pgno != pgno)
		return;

	fprintf(stderr, "\nSaving... ");
	if (isatty(STDERR_FILENO))
		fputc('\n', stderr);
	fflush(stderr);

	/* Fetching & exporting here is a bad idea,
	   but this is only a test. */
	assert(vbi_fetch_vt_page(vbi, &page,
				 ev->ev.ttx_page.pgno,
				 ev->ev.ttx_page.subno,
				 VBI_WST_LEVEL_3p5, 25, TRUE));

	/* Just for fun */
	if (pgno == -1) {
		char name[256];
		
		snprintf(name, sizeof(name) - 1, "test-%03x-%02x.%s",
			 ev->ev.ttx_page.pgno,
			 ev->ev.ttx_page.subno,
			 extension);

		assert((fp = fopen(name, "w")));
	} else
		fp = stdout;

	if (!vbi_export_stdio(ex, fp, &page)) {
		fprintf(stderr, "failed: %s\n", vbi_export_errstr(ex));
		exit(EXIT_FAILURE);
	} else {
		fprintf(stderr, "done\n");
	}

	vbi_unref_page(&page);

	if (pgno == -1)
		assert(fclose(fp) == 0);
	else
		quit = TRUE;
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
			vbi_sliced sliced[64];
			unsigned int lines;
			int64_t pts;

			lines = vbi_dvb_demux_cor (dx, sliced, 64,
						   &pts, &bp, &left);
			if (lines > 0) {
				vbi_decode (vbi, sliced, lines,
					    pts / 90000.0);
			}

			if (quit)
				return;
		}
	}

	fprintf(stderr, "\rEnd of stream, page %03x not found\n", pgno);
}

static void
old_mainloop			(void)
{
	for (;;) {
		vbi_sliced sliced[40];
		double timestamp;
		int n_lines;

		n_lines = read_sliced (sliced, &timestamp, /* max_lines */ 40);
		if (n_lines < 0)
			break;

		vbi_decode (vbi, sliced, n_lines, timestamp);

		if (quit)
			return;
	}

	fprintf(stderr, "\rEnd of stream, page %03x not found\n", pgno);
}

int
main(int argc, char **argv)
{
	char *module, *t;
	vbi_export_info *xi;
	int c;

	if (argc < 3) {
		fprintf(stderr, "Usage: %s module[;options] pgno <vbi data >file\n"
				"module eg. \"ppm\", pgno eg. 100 (hex)\n",
			argv[0]);
		exit(EXIT_FAILURE);
	}

	if (isatty(STDIN_FILENO)) {
		fprintf(stderr, "No vbi data on stdin\n");
		exit(EXIT_FAILURE);
	}

	cr = isatty (STDERR_FILENO) ? '\r' : '\n';

	module = argv[1];
	pgno = strtol(argv[2], NULL, 16);

	if (!(ex = vbi_export_new(module, &t))) {
		fprintf(stderr, "Failed to open export module '%s': %s\n",
			module, t);
		exit(EXIT_FAILURE);
	}


	assert((xi = vbi_export_info_export(ex)));
	assert((extension = strdup(xi->extension)));
	extension = strtok_r(extension, ",", &t);

	assert((vbi = vbi_decoder_new()));

	assert(vbi_event_handler_add(vbi, VBI_EVENT_TTX_PAGE, handler, NULL)); 

	c = getchar ();
	ungetc (c, stdin);

	if (0 == c) {
		dx = vbi_dvb_pes_demux_new (/* callback */ NULL, NULL);
		assert (NULL != dx);

		pes_mainloop ();
	} else {
		open_sliced (stdin);

		old_mainloop ();
	}

	exit(EXIT_SUCCESS);
}
