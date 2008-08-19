/*
 *  libzvbi Teletext browser example
 *
 *  Copyright (C) 2008 Michael H. Schimek
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* $Id: teletext.c,v 1.1.2.1 2008-08-19 13:13:29 mschimek Exp $ */

/* This example shows how to build a basic Teletext browser from
   libzvbi Teletext functions. After installing the library you can
   compile it like this:

   gcc -o teletext teletext.c -lX11 `pkg-config zvbi-0.3 --cflags --libs`
*/

#undef NDEBUG

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>		/* isatty() */
#include <locale.h>
#include <errno.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>

#include "zvbi/zvbi.h"

static vbi_dvb_demux *		dx;
static vbi_ttx_decoder *	td;

static Display *		display;
static int			screen;
static Colormap			cmap;
static Window			window;
static GC			gc;
static XEvent			event;
static XImage *			ximage;
static uint8_t *		ximgdata;

static vbi_image_format		image_format;

#define TEXT_WIDTH (40 * 12)
#define TEXT_HEIGHT (25 * 10 * 2)

/* User interface. */
static vbi_pgno			current_pgno;
static vbi_pgno			requested_pgno;
static vbi_bool			reveal;
static vbi_bool			quit;

static void
put_image			(vbi_bool		header_only)
{
	XPutImage (display, window, gc, ximage,
		   /* src_x, src_y */ 0, 0,
		   /* dest_x, dest_y */ 0, 0,
		   TEXT_WIDTH,
		   header_only ? 10 * 2 : TEXT_HEIGHT);
}

static void
draw_page			(const vbi_page *	pg,
				 vbi_bool		header_only)
{
	if (header_only)
		assert (1 == pg->rows);

	if (vbi_ttx_page_draw (pg, ximgdata, &image_format,
			       VBI_SCALE, TRUE,
			       VBI_REVEAL, reveal,
			       VBI_END)) {
		put_image (header_only);
	}
}

static void
get_and_draw_page		(vbi_pgno		pgno,
				 vbi_bool		header_only)
{
	vbi_page *pg;

	pg = vbi_ttx_decoder_get_page (td,
				       /* network: current */ NULL,
				       pgno, VBI_ANY_SUBNO,
				       VBI_TTX_LEVEL, VBI_TTX_LEVEL_2p5,
				       VBI_HEADER_ONLY, header_only,
				       VBI_END);
	if (NULL == pg) {
		fprintf (stderr, "Page %x is not cached.\n",
			 pgno);
		return;
	}

	if (requested_pgno > 0x99)
		pg->text[1].unicode = '0' + (requested_pgno >> 8);

	if (requested_pgno > 0x9)
		pg->text[2].unicode = '0' + ((requested_pgno >> 4) & 0xF);
	
	pg->text[3].unicode = '0' + (requested_pgno & 0xF);

	draw_page (pg, header_only);

	vbi_page_unref (pg);

	current_pgno = pgno;
}

static void
x_key_up			(void)
{
	vbi_pgno pgno = requested_pgno;

	if (pgno < 0x100)
		pgno = current_pgno;
	if (pgno >= 0x899)
		requested_pgno = 0x100;
	else
		requested_pgno = vbi_add_bcd (pgno, 0x001);

	get_and_draw_page (requested_pgno,
			   /* header_only */ FALSE);
}

static void
x_key_down			(void)
{
	vbi_pgno pgno = requested_pgno;

	if (pgno < 0x100)
		pgno = current_pgno;
	if (pgno <= 0x100)
		requested_pgno = 0x899;
	else
		requested_pgno = vbi_sub_bcd (pgno, 0x001);

	get_and_draw_page (requested_pgno,
			   /* header_only */ FALSE);
}

static void
x_key_0_9			(int			n)
{
	if (requested_pgno >= 0x100)
		requested_pgno = 0;

	if (0 == requested_pgno) {
		/* Magazine 1 ... 8. */
		if (0 == n)
			n = 8;
		else
			n &= 7;
	}

	requested_pgno = requested_pgno * 16 + n;

	if (requested_pgno >= 0x100) {
		get_and_draw_page (requested_pgno,
				   /* header_only */ FALSE);
	}
}

static void
x_event				(void)
{
	while (XPending (display)) {
		XNextEvent (display, &event);

		switch (event.type) {
		case KeyPress:
		{
			int c = XLookupKeysym (&event.xkey, 0);

			switch (c) {
			case XK_Up:
			case XK_KP_Up:
				x_key_up ();
				break;

			case XK_Down:
			case XK_KP_Down:
				x_key_down ();
				break;

			case XK_KP_0 ... XK_KP_9:
				x_key_0_9 (c - XK_KP_0);
				break;

			case '0' ... '9':
				x_key_0_9 (c - '0');
				break;	

			case 'c':
			case 'q':
				quit = TRUE;
				break;

			case '?':
				reveal = !reveal;
				get_and_draw_page (requested_pgno,
						   /* header_only */ FALSE);
				break;

			case XK_F1 ... XK_F4:
				break;
			}

			break;
		}

		case Expose:
			put_image (/* header_only */ FALSE);
			break;

		case ClientMessage:
			/* The requested WM_DELETE_WINDOW message. */
			quit = TRUE;
			break;
		}
	}
}

static void
destroy_window			(void)
{
	free (ximgdata);

	XCloseDisplay (display);
}

static void
init_window			(void)
{
	Atom delete_window_atom;
	XWindowAttributes wa;
	size_t image_size;

	display = XOpenDisplay (NULL);
	if (NULL == display) {
		fprintf (stderr, "Cannot open X display.\n");
		exit (EXIT_FAILURE);
	}

	screen = DefaultScreen (display);
	cmap = DefaultColormap (display, screen);
 
	window = XCreateSimpleWindow (display,
				      RootWindow (display, screen),
				      /* x, y */ 0, 0,
				      TEXT_WIDTH,
				      TEXT_HEIGHT,
				      /* border_width */ 2,
				      /* border: white */ -1,
				      /* background: black */ 0);
	if (0 == window) {
		fprintf (stderr, "Cannot open X window.\n");
		exit (EXIT_FAILURE);
	}

	XGetWindowAttributes (display, window, &wa);

	memset (&image_format, 0, sizeof (image_format));

	/* XXX this is unreliable. */
	switch (wa.depth) {
	case 32:
		/* B-G-R-A in memory. */
		image_format.pixfmt = VBI_PIXFMT_BGRA24_LE;
		break;

	case 24:
		/* B-G-R in memory. */
		image_format.pixfmt = VBI_PIXFMT_BGR24_LE;
		break;

	case 16:
		/* gggbbbbb-rrrrrggg in memory. */
		image_format.pixfmt = VBI_PIXFMT_BGR16_LE;
		break;

	case 15:
		/* gggbbbbb-arrrrrgg in memory. */
		image_format.pixfmt = VBI_PIXFMT_BGRA15_LE;
		break;

	default:
		fprintf (stderr, "Cannot run at color depth %u.\n",
			 wa.depth);
		exit (EXIT_FAILURE);
	}

	image_size = TEXT_WIDTH * TEXT_HEIGHT * wa.depth / 8;

	ximgdata = malloc (image_size);
	if (NULL == ximgdata) {
		fprintf (stderr, "Cannot allocate image buffer.\n");
		exit (EXIT_FAILURE);
	}

	ximage = XCreateImage (display,
			       DefaultVisual (display, screen),
			       DefaultDepth (display, screen),
			       /* format */ ZPixmap,
			       /* x offset */ 0,
			       (char *) ximgdata,
			       TEXT_WIDTH, TEXT_HEIGHT,
			       /* bitmap_pad */ 8,
			       /* bytes_per_line: contiguous */ 0);
	if (NULL == ximage) {
		fprintf (stderr, "Cannot allocate X image.\n");
		exit (EXIT_FAILURE);
	}

	image_format.width = TEXT_WIDTH;
	image_format.height = TEXT_HEIGHT;
	image_format.bytes_per_line[0] = ximage->bytes_per_line;

	delete_window_atom = XInternAtom (display, "WM_DELETE_WINDOW",
					  /* only_if_exists */ False);

	XSelectInput (display, window,
		      KeyPressMask |
		      ExposureMask |
		      StructureNotifyMask);

	XSetWMProtocols (display, window, &delete_window_atom, 1);

	XStoreName (display, window,
		    "Teletext Example - Press [0-9|?|up|down|q]");

	gc = XCreateGC (display, window,
			/* valuemask */ 0,
			/* values */ NULL);

	XMapWindow (display, window);
	       
	XSync (display, /* discard_events */ False);
}

static void
td_event_handler_roll_header	(const vbi_event *	ev)
{
	vbi_pgno pgno;

	pgno = requested_pgno;
	if (pgno < 0x100)
		pgno = current_pgno;

	if (0 == (ev->ev.ttx_page.flags & VBI_SERIAL)
	    && 0 != (0xF00 & (pgno ^ ev->ev.ttx_page.pgno))) {
		/* Page of a different magazine. */
		return;
	}

	get_and_draw_page (ev->ev.ttx_page.pgno,
			   /* header_only */ TRUE);
}

static vbi_bool
td_event_handler		(const vbi_event *	ev,
				 void *			user_data)
{
	user_data = user_data; /* unused */

	switch (ev->type) {
	case VBI_EVENT_TTX_PAGE:
		if (0) {
			printf ("%s TTX_PAGE %03x.%02x "
				"roll=%u news=%u subt=%u "
				"upd=%u serial=%u\n",
				__FUNCTION__,
				ev->ev.ttx_page.pgno,
				ev->ev.ttx_page.subno,
				!!(ev->ev.ttx_page.flags & VBI_ROLL_HEADER),
				!!(ev->ev.ttx_page.flags & VBI_NEWSFLASH),
				!!(ev->ev.ttx_page.flags & VBI_SUBTITLE),
				!!(ev->ev.ttx_page.flags & VBI_UPDATE),
				!!(ev->ev.ttx_page.flags & VBI_SERIAL));
		}

		if (ev->ev.ttx_page.pgno == requested_pgno) {
			get_and_draw_page (requested_pgno,
					   /* header_only */ FALSE);
		} else if (ev->ev.ttx_page.flags & VBI_ROLL_HEADER) {
			td_event_handler_roll_header (ev);
		}

		break;

	default:
		fprintf (stderr, "Unknown VBI event 0x%x.\n", ev->type);
		break;
	}

	return TRUE; /* successfully handled, don't pass on */
}

static vbi_bool
pes_dx_callback			(vbi_dvb_demux *	dx,
				 void *			user_data,
				 const vbi_sliced *	sliced,
				 unsigned int		sliced_lines,
				 int64_t		pts)
{
	struct timeval tv;

	dx = dx; /* unused */
	user_data = user_data;

	if (0) {
		printf ("%s lines=%u pts=%f\n",
			__FUNCTION__,
			sliced_lines, (double) pts);
	}

	/* Don't care about capture time. */
	tv.tv_sec = 0;
	tv.tv_usec = 0;

	/* Errors ignored. */
	vbi_ttx_decoder_feed_frame (td, sliced, sliced_lines, &tv, pts);

	/* Properly one should wait for input from STDIN_FILENO and
	   the X connection with select() or poll(), but I'm lazy
	   today. */
	x_event ();

	return TRUE; /* success */
}

static void
mainloop			(void)
{
	while (!quit) {
		uint8_t buffer[4096];
		size_t actual;

		actual = fread (buffer, 1, sizeof (buffer), stdin);

		if (ferror (stdin)) {
			fprintf (stderr, "Read error: %s.\n",
				 strerror (errno));
			exit (EXIT_FAILURE);
		}

		if (0 == actual)
			break;

		if (!vbi_dvb_demux_feed (dx, buffer, actual)) {
			fprintf (stderr, "Error in DVB stream (ignored).\n");
		}
	}
}

int
main				(void)
{
	vbi_bool success;

	setlocale (LC_ALL, "");

	if (isatty (STDIN_FILENO)) {
		fprintf (stderr, "No PES data on standard input.\n");
		exit (EXIT_FAILURE);
	}

	init_window ();

	dx = vbi_dvb_pes_demux_new (pes_dx_callback,
				    /* user_data */ NULL);
	if (NULL == dx) {
		fprintf (stderr, "Cannot allocate DVB demux.\n");
		exit (EXIT_FAILURE);
	}

	assert (NULL != dx);

	td = vbi_ttx_decoder_new (/* cache: allocate one */ NULL,
				  /* network: unknown */ NULL);
	if (NULL == td) {
		fprintf (stderr, "Cannot allocate Teletext decoder.\n");
		exit (EXIT_FAILURE);
	}

	success = vbi_ttx_decoder_add_event_handler
		(td,
		 VBI_EVENT_TTX_PAGE,
		 td_event_handler,
		 /* user_data */ NULL);
	if (!success) {
		fprintf (stderr, "Cannot register VBI event handler.\n");
		exit (EXIT_FAILURE);
	}

	requested_pgno = 0x100;
	quit = FALSE;

	mainloop ();

	vbi_ttx_decoder_delete (td);

	vbi_dvb_demux_delete (dx);

	destroy_window ();

	exit (EXIT_FAILURE);
}
