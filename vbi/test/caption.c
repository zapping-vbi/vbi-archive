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

/* $Id: caption.c,v 1.5.2.8 2007-11-01 00:21:26 mschimek Exp $ */

/* For libzvbi version 0.3.x. */

#undef NDEBUG

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#ifdef HAVE_GETOPT_LONG
#  include <getopt.h>
#endif

#include "sliced.h"

#undef _
#define _(x) x /* no l10n */

#define PROGRAM_NAME "caption"

#ifndef X_DISPLAY_MISSING

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>

#include "src/zvbi.h"
#include "src/vbi.h"

vbi3_decoder *		vbi;
vbi3_pgno		channel;
vbi3_dvb_demux *		dx;

/*
 *  Rudimentary render code for CC test.
 *  Attention: RGB 5:6:5 little endian only.
 */

#define WINDOW_WIDTH	640
#define WINDOW_HEIGHT	480

#define CELL_WIDTH	16
#define CELL_HEIGHT	26

/* Maximum text size in characters. */
#define TEXT_COLUMNS	34
#define TEXT_ROWS	15

/* Maximum text size in pixels. */
#define TEXT_WIDTH	(TEXT_COLUMNS * CELL_WIDTH)
#define TEXT_HEIGHT	(TEXT_ROWS * CELL_HEIGHT)	

static const char *		option_in_file_name;
static enum file_format		option_in_file_format;
static unsigned int		option_in_ts_pid;

static struct stream *		rst;

static Display *		display;
static int			screen;
static Colormap			cmap;
static XColor			lgreen = { 0, 0x8000, 0xFFFF, 0x8000, 0, 0 };
static XColor			lblue = { 0, 0x8000, 0x8000, 0xFFFF, 0, 0 };
static Window			window;
static GC			gc;
static XEvent			event;
static XImage *			ximage;
static uint8_t *		ximgdata;
static vbi3_image_format	image_format;
static vbi3_page		last_page;
static unsigned int		shift;

static vbi3_bool		padding = TRUE;
static vbi3_bool		row_change = FALSE;
static vbi3_bool		show_border = FALSE;
static vbi3_bool		smooth_roll = TRUE;

static void
draw_transparent_spaces		(unsigned int		column,
				 unsigned int		row,
				 unsigned int		n_columns)
{
	uint8_t *p;
	unsigned int i;
	unsigned int j;

	switch (image_format.pixfmt) {
	case VBI3_PIXFMT_BGRA24_LE:
		p = ximgdata + column * CELL_WIDTH * 4
			+ row * CELL_HEIGHT * TEXT_WIDTH * 4;

		for (j = 0; j < CELL_HEIGHT; ++j) {
			uint32_t *p32 = (uint32_t *) p;

			for (i = 0; i < n_columns * CELL_WIDTH; ++i) {
				p32[i] = lgreen.pixel;
			}

			p += TEXT_WIDTH * 4;
		}

		break;

	case VBI3_PIXFMT_BGR24_LE:
		p = ximgdata + column * CELL_WIDTH * 3
			+ row * CELL_HEIGHT * TEXT_WIDTH * 3;

		for (j = 0; j < CELL_HEIGHT; ++j) {
			for (i = 0; i < n_columns * CELL_WIDTH; ++i) {
				/* FIXME what's the endianess of .pixel? */
				p[i * 3 + 0] = lgreen.pixel;
				p[i * 3 + 1] = lgreen.pixel >> 8;
				p[i * 3 + 2] = lgreen.pixel >> 16;
			}

			p += TEXT_WIDTH * 3;
		}

		break;

	case VBI3_PIXFMT_BGR16_LE:
	case VBI3_PIXFMT_BGRA15_LE:
		p = ximgdata + column * CELL_WIDTH * 2
			+ row * CELL_HEIGHT * TEXT_WIDTH * 2;

		for (j = 0; j < CELL_HEIGHT; ++j) {
			uint16_t *p16 = (uint16_t *) p;

			for (i = 0; i < n_columns * CELL_WIDTH; ++i) {
				p16[i] = lgreen.pixel;
			}

			p += TEXT_WIDTH * 2;
		}

		break;

	default:		
		assert (0);
	}

}

static vbi3_bool
init_window			(int			ac,
				 char **		av)
{
	Atom delete_window_atom;
	XWindowAttributes wa;
	unsigned int row;
	unsigned int image_size;

	ac = ac;
	av = av;

	if (!(display = XOpenDisplay (NULL))) {
		return FALSE;
	}

	screen = DefaultScreen (display);
	cmap = DefaultColormap (display, screen);
 
	XAllocColor (display, cmap, &lgreen);
	XAllocColor (display, cmap, &lblue);

	assert (TEXT_WIDTH <= WINDOW_WIDTH);
	assert (TEXT_HEIGHT <= WINDOW_HEIGHT);

	window = XCreateSimpleWindow (display,
				      RootWindow (display, screen),
				      /* x, y */ 0, 0,
				      WINDOW_WIDTH, WINDOW_HEIGHT,
				      /* borderwidth */ 2,
				      /* border: white */ 0xffffffff,
				      /* background */ lgreen.pixel);
	if (!window) {
		return FALSE;
	}

	XGetWindowAttributes (display, window, &wa);

	/* FIXME determine what the real R/B order and endianess is. */
	switch (wa.depth) {
	case 32:
		/* B-G-R-A in memory. */
		image_format.pixfmt = VBI3_PIXFMT_BGRA24_LE;
		break;

	case 24:
		/* B-G-R in memory. */
		image_format.pixfmt = VBI3_PIXFMT_BGR24_LE;
		break;

	case 16:
		/* gggbbbbb rrrrrggg in memory. */
		image_format.pixfmt = VBI3_PIXFMT_BGR16_LE;
		break;

	case 15:
		/* gggbbbbb arrrrrgg in memory. */
		image_format.pixfmt = VBI3_PIXFMT_BGRA15_LE;
		break;

	default:		
		fprintf (stderr, "Cannot run at "
			 "color depth %u\n", wa.depth);
		return FALSE;
	}

	image_size = TEXT_WIDTH * TEXT_HEIGHT * wa.depth / 8;

	if (!(ximgdata = malloc (image_size))) {
		return FALSE;
	}

	for (row = 0; row < TEXT_ROWS; ++row)
		draw_transparent_spaces (/* column */ 0, row, TEXT_COLUMNS);

	ximage = XCreateImage (display,
			       DefaultVisual (display, screen),
			       DefaultDepth (display, screen),
			       /* format */ ZPixmap,
			       /* x offset */ 0,
			       (char *) ximgdata,
			       TEXT_WIDTH, TEXT_HEIGHT,
			       /* bitmap_pad */ 8,
			       /* bytes_per_line: contiguous */ 0);
	if (!ximage) {
		return FALSE;
	}

	image_format.width = TEXT_WIDTH;
	image_format.height = TEXT_HEIGHT;
	image_format.bytes_per_line = ximage->bytes_per_line;
	image_format.size = image_size;

	delete_window_atom = XInternAtom (display, "WM_DELETE_WINDOW", False);

	XSelectInput (display, window,
		      KeyPressMask |
		      ExposureMask |
		      StructureNotifyMask);
	XSetWMProtocols (display, window, &delete_window_atom, 1);
	XStoreName (display, window, "Caption Test - [B|P|Q|R|S|F1..F8]");

	gc = XCreateGC (display, window,
			/* valuemask */ 0,
			/* values */ NULL);

	XMapWindow (display, window);
	       
	XSync (display, False);

	return TRUE;
}

static void
draw_page			(const vbi3_page *	pg)
{
	unsigned int row;

	for (row = 0; row < pg->rows; ++row) {
		const vbi3_char *cp;
		unsigned int column;
		unsigned int n_tspaces;
		vbi3_bool success;

		cp = pg->text + row * pg->columns;

		/* Change is unlikely. */
		if (0 == memcmp (last_page.text + row * pg->columns,
				 cp, pg->columns * sizeof (pg->text[0])))
			continue;

		n_tspaces = 0;

		for (column = 0; column < pg->columns; ++column) {
			if (VBI3_TRANSPARENT_SPACE == cp[column].opacity) {
				++n_tspaces;
				continue;
			}

			if (n_tspaces > 0) {
				draw_transparent_spaces (column - n_tspaces,
							 row, n_tspaces);
				n_tspaces = 0;
			}

			success = vbi3_page_draw_caption_region
				(pg,
				 ximgdata,
				 &image_format,
				 /* x */ column * CELL_WIDTH,
				 /* y */ row * CELL_HEIGHT,
				 column,
				 row,
				 /* width */ 1,
				 /* height */ 1,
				 VBI3_SCALE, TRUE,
				 VBI3_END);

			assert (success);
		}

		if (n_tspaces > 0)
			draw_transparent_spaces (column - n_tspaces,
						 row, n_tspaces);
	}

	last_page = *pg;
}

static void
get_and_draw_page		(vbi3_pgno		channel)
{
	vbi3_page *pg;

	pg = vbi3_decoder_get_page (vbi, /* nk */ NULL,
				    channel, /* subno */ 0,
				    VBI3_PADDING, padding,
				    VBI3_ROW_CHANGE, row_change,
				    VBI3_END);
	assert (NULL != pg);

	draw_page (pg);

	vbi3_page_unref (pg);
}

static void
put_image			(void)
{
	unsigned int x;
	unsigned int y;
	unsigned int width;

	/* 32 or with padding 34 columns. */
	width = last_page.columns * CELL_WIDTH;

	x = (WINDOW_WIDTH - width) / 2;

	/* shift is between 0 ... CELL_HEIGHT - 1. */
	y = shift + (WINDOW_HEIGHT - (TEXT_HEIGHT + CELL_HEIGHT)) / 2;

	if (show_border)
		XSetForeground (display, gc, lblue.pixel);
	else
		XSetForeground (display, gc, lgreen.pixel);

	XFillRectangle (display, (Drawable) window, gc,
			/* x, y */ 0, 0,
			WINDOW_WIDTH, /* height */ y);

	XFillRectangle (display, (Drawable) window, gc,
			/* x, y */ 0, y,
			x, TEXT_HEIGHT);

	XPutImage (display, window, gc, ximage,
		   /* src_x, src_y */ 0, 0,
		   /* dest_x, dest_y */ x, y,
		   width, TEXT_HEIGHT);

	XFillRectangle (display, (Drawable) window, gc,
			/* x, y */ x + width, y,
			x, TEXT_HEIGHT);

	XFillRectangle (display, (Drawable) window, gc,
			/* x, y */ 0, y + TEXT_HEIGHT,
			WINDOW_WIDTH, WINDOW_HEIGHT - (y + TEXT_HEIGHT));
}

static void
x_event				(unsigned int		nap_usec)
{
	while (XPending (display)) {
		XNextEvent (display, &event);

		switch (event.type) {
		case KeyPress:
		{
			int c = XLookupKeysym (&event.xkey, 0);

			switch (c) {
			case 'b':
				show_border ^= TRUE;
				break;

			case 'c':
			case 'q':
				exit (EXIT_SUCCESS);

			case 'p':
				padding ^= TRUE;
				get_and_draw_page (last_page.pgno);
				break;

			case 'r':
				row_change ^= TRUE;
				if (!row_change)
					get_and_draw_page (last_page.pgno);
				break;

			case 's':
				smooth_roll ^= TRUE;
				if (!smooth_roll)
					shift = 0;
				break;

			case '1' ... '8':
				channel = c - '1' + VBI3_CAPTION_CC1;
				shift = 0;
				get_and_draw_page (channel);
				break;

			case XK_F1 ... XK_F8:
				channel = c - XK_F1 + VBI3_CAPTION_CC1;
				shift = 0;
				get_and_draw_page (channel);
				break;
			}

			break;
		}

		case Expose:
			/* Doing a put_image (); below. */
			break;

		case ClientMessage:
			exit (EXIT_SUCCESS);
		}
	}

	if (shift > 0) {
		shift -= 2;
	}

	put_image ();

	if (nap_usec > 0) {
		usleep (nap_usec / 4);
	}
}

static vbi3_bool
cc_raw_handler			(const vbi3_event *	ev,
				 void *			user_data)
{
	uint16_t buffer[64];
	unsigned int i;

	user_data = user_data;

	fprintf (stdout, "ch=%u row=%2d length=%u >",
		 ev->ev.cc_raw.channel,
		 ev->ev.cc_raw.row,
		 ev->ev.cc_raw.length);

	assert (ev->ev.cc_raw.length <= 64);
	for (i = 0; i < ev->ev.cc_raw.length; ++i)
		buffer[i] = ev->ev.cc_raw.text[i].unicode;

	assert (0 == ev->ev.cc_raw.text[i].unicode);

	vbi3_fputs_iconv_ucs2 (stdout,
			       vbi3_locale_codeset (),
			       buffer,
			       ev->ev.cc_raw.length, '?');

	fprintf (stdout, "<\n");

	return TRUE;
}

static vbi3_bool
cc_page_handler			(const vbi3_event *	ev,
				 void *			user_data)
{
	user_data = user_data;

	if (channel && ev->ev.caption.channel != channel)
		return TRUE;

	switch (ev->type) {
	case VBI3_EVENT_CC_PAGE:
		if (smooth_roll
		    && ev->ev.caption.flags & VBI3_START_ROLLING) {
			shift = CELL_HEIGHT - 2;
		}

		if (row_change
		    && !(ev->ev.caption.flags & VBI3_ROW_UPDATE)) {
			/* Shortcut: get_page with VBI3_ROW_CHANGE = TRUE
			   won't show anything new. */
			break;
		}

		get_and_draw_page (ev->ev.caption.channel);

		break;

	default:
		assert (0);
	}

	return TRUE;
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

	vbi3_decoder_feed (vbi, sliced, n_lines, sample_time);

	x_event (33333);

	return TRUE;
}

static void
usage				(FILE *			fp)
{
	fprintf (fp, _("\
%s %s\n\n\
Copyright (C) 2000, 2001, 2007 Michael H. Schimek\n\
This program is licensed under GPLv2 or later. NO WARRANTIES.\n\n\
Usage: %s [options] < sliced VBI data\n\
-h | --help | --usage  Print this message and exit\n\
-i | --input name      Read the VBI data from this file instead\n\
                       of standard input\n\
-P | --pes             Source is a DVB PES stream\n\
-T | --ts pid          Source is a DVB TS stream\n\
-V | --version         Print the program version and exit\n\
"),
		 PROGRAM_NAME, VERSION, program_invocation_name);
}

static const char
short_options [] = "hi:PT:V";

#ifdef HAVE_GETOPT_LONG
static const struct option
long_options [] = {
	{ "help",	no_argument,		NULL,		'h' },
	{ "usage",	no_argument,		NULL,		'h' },
	{ "input",	required_argument,	NULL,		'i' },
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
main				 (int			argc,
				 char **		argv)
{
	vbi3_bool success;

	init_helpers (argc, argv);

	option_in_file_format = FILE_FORMAT_SLICED;

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

		case 'i':
			assert (NULL != optarg);
			option_in_file_name = optarg;
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

	vbi3_set_log_fn (VBI3_LOG_INFO - 1,
			 vbi3_log_on_stderr,
			 /* user_data */ NULL);

	if  (!init_window (argc, argv))
		exit (EXIT_FAILURE);

	vbi = vbi3_decoder_new (/* cache */ NULL,
				/* nk */ NULL,
				VBI3_VIDEOSTD_SET_525_60);
	assert (NULL != vbi);

	success = vbi3_decoder_add_event_handler
		(vbi,
		 VBI3_EVENT_CC_PAGE,
		 cc_page_handler, /* user_data */ NULL);
	assert (success);

	if (1) {
		success = vbi3_decoder_add_event_handler
			(vbi,
			 VBI3_EVENT_CC_RAW,
			 cc_raw_handler, /* user_data */ NULL);
		assert (success);
	}


	rst = read_stream_new (option_in_file_name,
			       option_in_file_format,
			       option_in_ts_pid,
			       decode_function);

	stream_loop (rst);

	stream_delete (rst);

	error_msg (_("End of stream."));

	for (;;)
		x_event (33333);

	vbi3_decoder_delete (vbi);

	exit (EXIT_SUCCESS);
}

#else /* X_DISPLAY_MISSING */

int
main				(int			argc,
				 char **		argv)
{
	error_exit ("Could not find X11 or it has been disabled "
		    "at configuration time.");
}

#endif
