/*
 *  libzvbi test
 *
 *  Copyright (C) 2000-2002, 2004 Michael H. Schimek
 *  Copyright (C) 2003 James Mastros
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

/* $Id: osc.c,v 1.9.2.7 2006-05-19 01:11:38 mschimek Exp $ */

#undef NDEBUG

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <locale.h>
#include <errno.h>
#include <unistd.h>
#ifdef HAVE_GETOPT_LONG
#include <getopt.h>
#endif

#include "src/misc.h"
#include "src/vbi.h"
#include "src/zvbi.h"

#ifndef X_DISPLAY_MISSING

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>

vbi3_capture *		cap;
vbi3_raw_decoder *	rd;
vbi3_sampling_par	sp;

int			src_w, src_h;
vbi3_sliced *		sliced;
int			n_lines;
vbi3_bool		quit;

int			do_sim;
int			ignore_error;
int			desync;
vbi3_bool		show_grid;
vbi3_bool		show_points;

Display *		display;
int			screen;
Colormap		cmap;
Window			window;
int			dst_w, dst_h;
GC			gc;
XEvent			event;
XImage *		ximage;
void *			ximgdata;
unsigned char 		*raw1, *raw2;
int			palette[256];
int			depth;
int			draw_row, draw_offset;
int			draw_count = -1;
int                     cur_x, cur_y;

static const char *
teletext			(const uint8_t		buffer[42],
				 unsigned int		line)
{
	static char text[256];
	int pmag;
	unsigned int magazine;
	unsigned int packet;
	unsigned int i;
	unsigned int n;

	line = line;

	if ((pmag = vbi3_unham16p (buffer)) < 0) {
		return "Hamming error in pmag";
	}

	magazine = pmag & 7;
	if (0 == magazine)
		magazine = 8;

	packet = pmag >> 3;

	n = snprintf (text, sizeof (text) - 43, "%x/%2u ", magazine, packet);

	for (i = 0; i < 42; ++i)
		text[n++] = _vbi3_to_ascii (buffer[i]);

        text[n] = 0;

        return text;
}

static const char *
vps				(uint8_t		buffer[13],
				 unsigned int		line)
{
	static char text[256];
	static const char *pcs_audio [] = {
		"UNKNOWN",
		"MONO",
		"STEREO",
		"BILINGUAL"
	};
	unsigned int cni;
	vbi3_program_id pid;
	unsigned int n;

	if (!vbi3_decode_vps_cni (&cni, buffer)) {
		return "Error in vps cni";
	}
		
	if (!vbi3_decode_vps_pdc (&pid, buffer)) {
		return "Error in vps pdc data";
	}

	n = snprintf (text, sizeof (text) - 1, "cni=%04x pil=", pid.cni);

	switch (pid.pil) {
	case VBI3_PIL_TIMER_CONTROL:
		n += snprintf (text + n, sizeof (text) - n - 1,
			       "TIMER_CONTROL");
		break;

	case VBI3_PIL_INHIBIT_TERMINATE:
		n += snprintf (text + n, sizeof (text) - n - 1,
			       "INHIBIT_TERMINATE");
		break;

	case VBI3_PIL_INTERRUPT:
		n += snprintf (text + n, sizeof (text) - n - 1,
			       "INTERRUPTION");
		break;

	case VBI3_PIL_CONTINUE:
		n += snprintf (text + n, sizeof (text) - n - 1,
			       "CONTINUE");
		break;

	default:
		n += snprintf (text + n, sizeof (text) - n - 1,
			       "%05x (%02u-%02u %02u:%02u)",
			       pid.pil,
			       VBI3_PIL_MONTH (pid.pil),
			       VBI3_PIL_DAY (pid.pil),
			       VBI3_PIL_HOUR (pid.pil),
			       VBI3_PIL_MINUTE (pid.pil));
		break;
	}

	n += snprintf (text + n, sizeof (text) - n - 1, " pcs=%s pty=%02x",
		       pcs_audio[pid.pcs_audio], pid.pty);

	if (1) {
		static char pr_label[2][20];
		static char label[2][20];
		static int l[2] = { 0, 0 };
		unsigned int i;
		int c;

		i = (line != 16);

		c = vbi3_rev8 (buffer[1]);

		if (c & 0x80) {
			label[i][l[i]] = 0;
			strcpy (pr_label[i], label[i]);
			l[i] = 0;
		}

		label[i][l[i]] = _vbi3_to_ascii (c);

		l[i] = (l[i] + 1) % 16;
		
		n += snprintf (text + n, sizeof (text) - n - 1,
			       " data %02x %02x %02x='%c'=\"%s\" %02x %02x "
			       "%02x %02x %02x %02x",
			       buffer[0], buffer[1],
			       c, _vbi3_to_ascii (c), pr_label[i],
			       buffer[2], buffer[3],
			       buffer[4], buffer[5],
			       buffer[6], buffer[7]);
	}

	return text;
}

static const char *
wss_625				(uint8_t		buffer[2],
				 unsigned int		line)
{
	static char text[256];
	vbi3_aspect_ratio ar;

	line = line;

	if (!vbi3_decode_wss_625 (&ar, buffer)) {
		return "Error in WSS";
	}

	snprintf (text, sizeof (text) - 1,
		  "active=%u-%u,%u-%u ratio=%f film=%u subt=%s,%s",
		  ar.start[0], ar.start[0] + ar.count[0] - 1,
		  ar.start[1], ar.start[1] + ar.count[1] - 1,
		  ar.ratio, ar.film_mode,
		  vbi3_subtitles_name (ar.open_subtitles),
		  vbi3_subtitles_name (ar.closed_subtitles));

	return text;
}

static const char *
generic				(uint8_t		buffer[2],
				 unsigned int		line,
				 unsigned int		size)
{
	static char text[256];
	unsigned int i;

	line = line;

	assert (size * 4 + 1 < sizeof (text));

	for (i = 0; i < size; ++i)
		sprintf (text + i * 3, "%02x ", buffer[i]);

	for (i = 0; i < size; ++i)
		text[size * 3 + i] = _vbi3_to_ascii (buffer[i]);

	text[size * 4] = 0;

	return text;
}

static void
draw(unsigned char *raw)
{
	int rem = src_w - draw_offset;
	unsigned char buf[256];
	unsigned char *data = raw;
	int i, v, h0, field, end, line;
        XTextItem xti;

	if (draw_count == 0)
		return;

	if (draw_count > 0)
		draw_count--;

	memcpy(raw2, raw, src_w * src_h);

	if (depth == 24) {
		unsigned int *p = ximgdata;
		
		for (i = src_w * src_h; i >= 0; i--)
			*p++ = palette[(int) *data++];
	} else {
		unsigned short *p = ximgdata; // 64 bit safe?

		for (i = src_w * src_h; i >= 0; i--)
			*p++ = palette[(int) *data++];
	}

	XPutImage(display, window, gc, ximage,
		draw_offset, 0, 0, 0, rem, src_h);

	XSetForeground(display, gc, 0);

	if (rem < dst_w)
		XFillRectangle(display, window, gc,
			rem, 0, dst_w, src_h);

	if ((v = dst_h - src_h) <= 0)
		return;

	XSetForeground(display, gc, 0);
	XFillRectangle(display, window, gc,
		0, src_h, dst_w, dst_h);

	XSetForeground(display, gc, ~0);

	field = (draw_row >= sp.count[0]);

	if (sp.start[field] <= 0) {
		xti.nchars = snprintf (buf, 255, "Row %d Line ?", draw_row);
		line = -1;
	} else if (field == 0) {
		line = draw_row + sp.start[0];
		xti.nchars = snprintf (buf, 255, "Row %d Line %d",
				       draw_row, line);
	} else {
		line = draw_row - sp.count[0] + sp.start[1];
		xti.nchars = snprintf (buf, 255, "Row %d Line %d",
				       draw_row, line);
	}

	for (i = 0; i < n_lines; ++i)
		if (sliced[i].line == line)
			break;

	if (i < n_lines) {
		const char *text;

		text = vbi3_sliced_name (sliced[i].id);
		xti.nchars += snprintf (buf + xti.nchars, 255 - xti.nchars,
					" %s ", text ? text : "?");

		switch (sliced[i].id) {
		case VBI3_SLICED_TELETEXT_A:
			text = generic (sliced[i].data, sliced[i].line, 37);
			break;

		case VBI3_SLICED_TELETEXT_B_L10_625:
		case VBI3_SLICED_TELETEXT_B_L25_625:
		case VBI3_SLICED_TELETEXT_B_625:
			text = teletext (sliced[i].data, sliced[i].line);
			break;

		case VBI3_SLICED_TELETEXT_C_525:
		case VBI3_SLICED_TELETEXT_C_625:
			text = generic (sliced[i].data, sliced[i].line, 33);
			break;

		case VBI3_SLICED_TELETEXT_B_525:
		case VBI3_SLICED_TELETEXT_D_525:
		case VBI3_SLICED_TELETEXT_D_625:
			text = generic (sliced[i].data, sliced[i].line, 34);
			break;

		case VBI3_SLICED_CAPTION_525_F1:
		case VBI3_SLICED_CAPTION_525_F2:
		case VBI3_SLICED_CAPTION_525:
		case VBI3_SLICED_CAPTION_625_F1:
		case VBI3_SLICED_CAPTION_625_F2:
		case VBI3_SLICED_CAPTION_625:
			text = generic (sliced[i].data, sliced[i].line, 2);
			break;

		case VBI3_SLICED_VPS:
		case VBI3_SLICED_VPS_F2:
			text = vps (sliced[i].data, sliced[i].line);
			break;

		case VBI3_SLICED_WSS_625:
			text = wss_625 (sliced[i].data, sliced[i].line);
			break;

		case VBI3_SLICED_WSS_CPR1204:
			text = generic (sliced[i].data, sliced[i].line, 3);
			break;

		case VBI3_SLICED_2xCAPTION_525:
			text = generic (sliced[i].data, sliced[i].line, 4);
			break;

		default:
			text = generic (sliced[i].data, sliced[i].line,
					sizeof (sliced[i].data));
			break;
		}

		xti.nchars += snprintf (buf + xti.nchars, 255 - xti.nchars,
					"%s", text);
	} else {
		int s = 0, sd = 0;

		data = raw + draw_row * src_w;

		for (i = 0; i < src_w; i++)
			s += data[i];
		s /= src_w;

		for (i = 0; i < src_w; i++)
			sd += abs(data[i] - s);

		sd /= src_w;

		xti.nchars += snprintf(buf + xti.nchars, 255 - xti.nchars,
				       (sd < 5) ? " Blank" : " Unknown signal");
	        xti.nchars += snprintf(buf + xti.nchars, 255 - xti.nchars,
				       " (%d)", sd);
	}

	if (show_grid) {
		unsigned int i;
		double ppus;

		/* usec grid */

		ppus = sp.sampling_rate / 1e6;

		XSetForeground (display, gc, 0xAAAAAAAA);

		for (i = 1; i < 64; ++i) {
			XFillRectangle (display, window, gc,
					(int)(i * ppus) - draw_offset,
					src_h, 1, dst_h);
		}
	}

	if (show_points) {
		unsigned int i;

		for (i = 0; i < 640; ++i) {
			vbi3_bit_slicer_point point;
			unsigned int x, y;
			int h;

			if (!vbi3_raw_decoder_get_point (rd, &point,
							 draw_row, i))
				break;

			switch (point.kind) {
			case VBI3_CRI_BIT:
				XSetForeground (display, gc, 0x888800);
				break;
			case VBI3_FRC_BIT:
				XSetForeground (display, gc, 0xCCCC00);
				break;
			case VBI3_PAYLOAD_BIT:
				XSetForeground (display, gc, 0xFFFF00);
				break;
			default:
				assert (0);
			}

			/* Sampling point. */
			x = (point.index / 256) & 0xFFF;
			y = (point.thresh / 256);
			h = dst_h - (y * v) / 256;

			XFillRectangle (display, window, gc,
					x - draw_offset, h - 5, 1, 11);
			XFillRectangle (display, window, gc,
					x - draw_offset - 5, h, 11, 1);
		}
	}

        XSetForeground(display, gc, ~0);

        xti.chars = buf;
	xti.delta = 0;
	xti.font = 0;

	XDrawText(display, window, gc, 4, src_h + 12, &xti, 1);
        xti.nchars = snprintf(buf, 255, "(%d, %3.0d)", cur_x+draw_offset, (1000*(dst_h-cur_y))/(dst_h-src_h));
        XDrawText(display, window, gc, 4, src_h + 24, &xti, 1);

	data = raw + draw_offset + draw_row * src_w;
	h0 = dst_h - (data[0] * v) / 256;
	end = src_w - draw_offset;
	if (dst_w < end)
		end = dst_w;

	for (i = 1; i < end; i++) {
		int h = dst_h - (data[i] * v) / 256;

		XDrawLine(display, window, gc, i - 1, h0, i, h);
		h0 = h;
	}
}

static void
xevent(void)
{
	while (XPending(display)) {
		XNextEvent(display, &event);

		switch (event.type) {
		case KeyPress:
		{
			switch (XLookupKeysym(&event.xkey, 0)) {
			case 'g':
				draw_count = 1;
				break;

			case 'i':
				show_grid ^= TRUE;
				break;

			case 'l':
				draw_count = -1;
				break;

			case 'p':
				show_points ^= TRUE;
				vbi3_raw_decoder_collect_points
					(rd, show_points);
				if (do_sim)
					vbi3_capture_sim_decode_raw
						(cap, show_points);
				break;

			case 'q':
			case 'c':
				quit = TRUE;
				break;

			case XK_Up:
			    if (draw_row > 0)
				    draw_row--;
			    goto redraw;

			case XK_Down:
			    if (draw_row < (src_h - 1))
				    draw_row++;
			    goto redraw;

			case XK_Left:
			    if (draw_offset > 0)
				    draw_offset -= 10;
			    goto redraw;

			case XK_Right:
			    if (draw_offset < (src_w - 10))
				    draw_offset += 10;
			    goto redraw;  
			}

			break;
		}

		case ConfigureNotify:
			dst_w = event.xconfigurerequest.width;
			dst_h = event.xconfigurerequest.height;
redraw:
			if (draw_count == 0) {
				draw_count = 1;
				draw(raw2);
			}

			break;

		case MotionNotify:
		       cur_x = event.xmotion.x;
		       cur_y = event.xmotion.y;
		       // printf("Got MotionNotify: (%d, %d)\n", event.xmotion.x, event.xmotion.y);
		       break;
		   
		case ClientMessage:
			exit(EXIT_SUCCESS);
		}
	}
}

static void
init_window(int ac, char **av, char *dev_name)
{
	char buf[256];
	Atom delete_window_atom;
	XWindowAttributes wa;
	int i;

	if (!(display = XOpenDisplay(NULL))) {
		fprintf(stderr, "No display\n");
		exit(EXIT_FAILURE);
	}

	screen = DefaultScreen(display);
	cmap = DefaultColormap(display, screen);
 
	window = XCreateSimpleWindow(display,
		RootWindow(display, screen),
		0, 0,		// x, y
		dst_w = 768, dst_h = src_h + 110,
				// w, h
		2,		// borderwidth
		0xffffffff,	// fgd
		0x00000000);	// bgd 

	if (!window) {
		fprintf(stderr, "No window\n");
		exit(EXIT_FAILURE);
	}
			
	XGetWindowAttributes(display, window, &wa);
	depth = wa.depth;
			
	if (depth != 15 && depth != 16 && depth != 24) {
		fprintf(stderr, "Sorry, cannot run at colour depth %d\n", depth);
		exit(EXIT_FAILURE);
	}

	for (i = 0; i < 256; i++) {
		switch (depth) {
		case 15:
			palette[i] = ((i & 0xF8) << 7)
				   + ((i & 0xF8) << 2)
				   + ((i & 0xF8) >> 3);
				break;

		case 16:
			palette[i] = ((i & 0xF8) << 8)
				   + ((i & 0xFC) << 3)
				   + ((i & 0xF8) >> 3);
				break;

		case 24:
			palette[i] = (i << 16) + (i << 8) + i;
				break;
		}
	}

	if (depth == 24) {
		if (!(ximgdata = malloc(src_w * src_h * 4))) {
			fprintf(stderr, "Virtual memory exhausted\n");
			exit(EXIT_FAILURE);
		}
	} else {
		if (!(ximgdata = malloc(src_w * src_h * 2))) {
			fprintf(stderr, "Virtual memory exhausted\n");
			exit(EXIT_FAILURE);
		}
	}

	if (!(raw1 = malloc(src_w * src_h))) {
		fprintf(stderr, "Virtual memory exhausted\n");
		exit(EXIT_FAILURE);
	}

	if (!(raw2 = malloc(src_w * src_h))) {
		fprintf(stderr, "Virtual memory exhausted\n");
		exit(EXIT_FAILURE);
	}

	ximage = XCreateImage(display,
		DefaultVisual(display, screen),
		DefaultDepth(display, screen),
		ZPixmap, 0, (char *) ximgdata,
		src_w, src_h,
		8, 0);

	if (!ximage) {
		fprintf(stderr, "No ximage\n");
		exit(EXIT_FAILURE);
	}

	delete_window_atom = XInternAtom(display, "WM_DELETE_WINDOW", False);

	XSelectInput(display, window, PointerMotionMask | KeyPressMask | ExposureMask | StructureNotifyMask);
	XSetWMProtocols(display, window, &delete_window_atom, 1);
	snprintf(buf, sizeof(buf) - 1, "%s - [cursor] [g]rab [l]ive gr[i]d [p]oints", dev_name);
	XStoreName(display, window, buf);

	gc = XCreateGC(display, window, 0, NULL);

	XMapWindow(display, window);
	       
	XSync(display, False);
}

static void
mainloop(void)
{
	double timestamp;
	struct timeval tv;

	tv.tv_sec = 2;
	tv.tv_usec = 0;

	assert((sliced = malloc(sizeof(vbi3_sliced) * src_h)));

	for (quit = FALSE; !quit;) {
		int r;

		r = vbi3_capture_read(cap, raw1, sliced,
				      &n_lines, &timestamp, &tv);
		switch (r) {
		case -1:
			fprintf(stderr, "VBI read error: %d, %s%s\n",
				errno, strerror(errno),
				ignore_error ? " (ignored)" : "");
			if (ignore_error)
				continue;
			else
				exit(EXIT_FAILURE);
		case 0: 
			fprintf(stderr, "VBI read timeout%s\n",
				ignore_error ? " (ignored)" : "");
			if (ignore_error)
				continue;
			else
				exit(EXIT_FAILURE);
		case 1:
			break;
		default:
			assert(!"reached");
		}

		draw(raw1);

/*		printf("raw: %f; sliced: %d\n", timestamp, n_lines); */

		xevent();
	}
}

static const char short_options[] = "123cde:npsv";

#ifdef HAVE_GETOPT_LONG
static const struct option
long_options[] = {
	{ "desync",	no_argument,		NULL,		'c' },
	{ "device",	required_argument,	NULL,		'd' },
	{ "ignore-error", no_argument,		NULL,		'e' },
	{ "ntsc",	no_argument,		NULL,		'n' },
	{ "pal",	no_argument,		NULL,		'p' },
	{ "sim",	no_argument,		NULL,		's' },
	{ "v4l",	no_argument,		NULL,		'1' },
	{ "v4l2-read",	no_argument,		NULL,		'2' },
	{ "v4l2-mmap",	no_argument,		NULL,		'3' },
	{ "verbose",	no_argument,		NULL,		'v' },
	{ NULL, 0, 0, 0 }
};
#else
#define getopt_long(ac, av, s, l, i) getopt(ac, av, s)
#endif

int
main(int argc, char **argv)
{
	char *dev_name = "/dev/vbi";
	char *errstr;
	unsigned int services;
	int scanning = 625;
	int verbose = 0;
	int interface = 0;
	int c, index;

	setlocale (LC_ALL, "");

	while ((c = getopt_long(argc, argv, short_options,
				long_options, &index)) != -1)
		switch (c) {
		case 0: /* set flag */
			break;
		case '2':
			/* Preliminary hack for tests (0.2 branch). */
//			vbi3_capture_force_read_mode = TRUE;
			/* fall through */
		case '1':
		case '3':
			interface = c - '0';
			break;
		case 'c':
			desync ^= TRUE;
			break;
		case 'd':
			dev_name = optarg;
			break;
		case 'e':
			ignore_error ^= TRUE;
			break;
		case 'n':
			scanning = 525;
			break;
		case 'p':
			scanning = 625;
			break;
		case 's':
			do_sim ^= TRUE;
			break;
		case 'v':
			++verbose;
			break;
		default:
			fprintf(stderr, "Unknown option\n");
			exit(EXIT_FAILURE);
		}

	if (verbose) {
		vbi3_set_log_fn (VBI3_LOG_INFO - 1,
				 vbi3_log_on_stderr,
				 /* user_data */ NULL);
	}

	services = VBI3_SLICED_VBI3_525 | VBI3_SLICED_VBI3_625 |
		VBI3_SLICED_TELETEXT_A |
		VBI3_SLICED_TELETEXT_B |
		VBI3_SLICED_TELETEXT_C_625 |
		VBI3_SLICED_TELETEXT_D_625 |
		VBI3_SLICED_CAPTION_625 |
		VBI3_SLICED_VPS |
		VBI3_SLICED_WSS_625 |
		VBI3_SLICED_TELETEXT_B_525 |
		VBI3_SLICED_TELETEXT_C_525 |
		VBI3_SLICED_TELETEXT_D_525 |
		VBI3_SLICED_CAPTION_525 |
		VBI3_SLICED_WSS_CPR1204 |
		0;

	do {
		if (do_sim) {
			cap = vbi3_capture_sim_new (scanning, &services,
						    /* interlaced */ FALSE,
						    !desync);
			assert (NULL != cap);
			break;
		}

		if (1 != interface) {
			cap = vbi3_capture_v4l2k_new
				(dev_name, /* fd */ -1,
				 /* buffers */ 5, &services,
				 /* strict */ 0, &errstr,
				 /* trace */ !!verbose);

			if (cap)
				break;

			fprintf (stderr, "Cannot capture vbi data "
				 "with v4l2k interface:\n%s\n",
				 errstr);

			free (errstr);

			cap = vbi3_capture_v4l2_new (dev_name,
						     /* buffers */ 5,
						     &services,
						     /* strict */ 0,
						     &errstr,
						     /* trace */
						     !!verbose);
			if (cap)
				break;

			fprintf (stderr, "Cannot capture vbi data "
				 "with v4l2 interface:\n%s\n", errstr);

			free (errstr);
		}

		if (interface < 2) {
			cap = vbi3_capture_v4l_new (dev_name,
						    scanning,
						    &services,
						    /* strict */ 0,
						    &errstr,
						    /* trace */
						    !!verbose);
			if (cap)
				break;

			fprintf (stderr, "Cannot capture vbi data "
				 "with v4l interface:\n%s\n", errstr);

			free (errstr);
		}

		/* BSD interface */
		if (1) {
			cap = vbi3_capture_bktr_new (dev_name,
						     scanning,
						     &services,
						     /* strict */ 0,
						     &errstr,
						     /* trace */
						     !!verbose);
			if (cap)
				break;

			fprintf (stderr, "Cannot capture vbi data "
				 "with bktr interface:\n%s\n", errstr);

			free (errstr);
		}

		exit(EXIT_FAILURE);
	} while (0);

	assert ((rd = vbi3_capture_parameters(cap)));

	if (verbose > 1) {
// 0.2		vbi3_capture_set_log_fp (cap, stderr);
	}

	show_grid = TRUE;
	show_points = TRUE;

	vbi3_raw_decoder_collect_points (rd, TRUE);
	if (do_sim)
		vbi3_capture_sim_decode_raw (cap, TRUE);

	vbi3_raw_decoder_get_sampling_par (rd, &sp);

	assert (sp.sampling_format == VBI3_PIXFMT_Y8);

	src_w = sp.bytes_per_line / 1;
	src_h = sp.count[0] + sp.count[1];

	init_window(argc, argv, dev_name);

	mainloop();

	vbi3_capture_delete (cap);

	exit(EXIT_SUCCESS);	
}


#else /* X_DISPLAY_MISSING */

int
main(int argc, char **argv)
{
	printf("Could not find X11 or has been disabled at configuration time\n");
	exit(EXIT_FAILURE);
}

#endif
