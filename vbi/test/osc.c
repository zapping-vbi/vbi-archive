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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301, USA.
 */

/* $Id: osc.c,v 1.9.2.10 2008-03-01 15:51:04 mschimek Exp $ */

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

#include "sliced.h"

#undef _
#define _(x) (x) /* later */

#ifndef X_DISPLAY_MISSING

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>

static vbi3_bool	option_sim_interlaced;
static vbi3_bool	option_sim_synchronous;
static unsigned int	option_sim_flags; /* _VBI3_RAW_... */

vbi3_capture *		cap;
vbi3_sampling_par	sp;

int			src_w, src_h;
vbi3_bool		quit;

int			do_sim;
int			ignore_error;
int			desync;
vbi3_bool		show_grid;
vbi3_bool		show_points;


static struct stream *		cst;

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
vbi3_sliced		sliced2[50];
unsigned int		sliced2_lines;
int			step_mode;
double			sample_time2;

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
vps				(const uint8_t		buffer[13],
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
wss_625				(const uint8_t		buffer[2],
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
caption				(const uint8_t		buffer[2],
				 unsigned int		line,
				 unsigned int		size)
{
	static char text[16];
	int c1, c2, n;

	line = line; /* unused */
	size = size;

	c1 = vbi3_unpar8 (buffer[0]);
	c2 = vbi3_unpar8 (buffer[1]);

	n = snprintf (text, sizeof (text),
		      " %s%02x %s%02x %c%c",
		      (c1 < 0) ? ">" : "", buffer[0],
		      (c2 < 0) ? ">" : "", buffer[1],
		      _vbi3_to_ascii (buffer[0]),
		      _vbi3_to_ascii (buffer[1]));

	assert (n < (int) sizeof (text));

	return text;
}

static const char *
generic				(const uint8_t		buffer[2],
				 unsigned int		line,
				 unsigned int		size)
{
	static char text[256];
	unsigned int i;

	line = line;

	assert (size * 4 + 1 < sizeof (text));

	for (i = 0; i < size; ++i)
		snprintf (text + i * 3, 256 - i * 3, "%02x ", buffer[i]);

	for (i = 0; i < size; ++i)
		text[size * 3 + i] = _vbi3_to_ascii (buffer[i]);

	text[size * 4] = 0;

	return text;
}

static void
draw				(const vbi3_sliced *	sliced,
				 unsigned int		n_lines,
				 const uint8_t *	raw,
				 double			sample_time)
{
	int rem = src_w - draw_offset;
	char buf[256];
	const uint8_t *data = raw;
	int v, h0, field, end;
	unsigned int line;
	unsigned int i;
        XTextItem xti;

	if (draw_count == 0)
		return;

	if (draw_count > 0)
		draw_count--;

	memcpy(raw2, raw, src_w * src_h);

	assert (n_lines < N_ELEMENTS (sliced2));
	memcpy (sliced2, sliced, n_lines * sizeof (*sliced));
	sliced2_lines = n_lines;

	sample_time2 = sample_time;

	if (depth == 24) {
		unsigned int *p = ximgdata;
		int i;

		for (i = src_w * src_h; i >= 0; i--)
			*p++ = palette[(int) *data++];
	} else {
		unsigned short *p = ximgdata; // 64 bit safe?
		int i;

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

	field = (draw_row >= (int) sp.count[0]);

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
					" %.4f %s ", sample_time, text ? text : "?");

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
			text = caption (sliced[i].data, sliced[i].line, 2);
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
		int i;

		data = raw + draw_row * src_w;

		for (i = 0; i < src_w; i++)
			s += data[i];
		s /= src_w;

		for (i = 0; i < src_w; i++)
			sd += abs(data[i] - s);

		sd /= src_w;

		xti.nchars += snprintf(buf + xti.nchars, 255 - xti.nchars,
				       (sd < 5) ? " %.4f Blank" : " %.4f Unknown signal",
				       sample_time);
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

			if (!capture_stream_get_point (cst, &point,
						       draw_row, i))
				break;

			switch (point.kind) {
			case VBI3_CRI_BIT:
#warning "shouldn't this be a pixel, e.g. 565?"
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
        xti.nchars = snprintf(buf, 255, "(%d = %2.2f us, %3d)", 
		cur_x+draw_offset,
		(sp.offset + cur_x + draw_offset) * 1e6 / sp.sampling_rate,
	        (dst_h - cur_y) * 256 / v);
        XDrawText(display, window, gc, 4, src_h + 24, &xti, 1);

	data = raw + draw_offset + draw_row * src_w;
	h0 = dst_h - (data[0] * v) / 256;
	end = src_w - draw_offset;
	if (dst_w < end)
		end = dst_w;

	{
		int i;

		for (i = 1; i < end; i++) {
			int h = dst_h - (data[i] * v) / 256;

			XDrawLine(display, window, gc, i - 1, h0, i, h);
			h0 = h;
		}
	}
}

static void
xevent(void)
{
	vbi3_bool next_frame = FALSE;

	while (step_mode || XPending(display)) {
		XNextEvent(display, &event);

		switch (event.type) {
		case KeyPress:
		{
			switch (XLookupKeysym (&event.xkey, 0)) {
			case 'a':
				option_sim_flags ^= _VBI3_RAW_LOW_AMP_CC;
				capture_stream_sim_set_flags
					(cst, option_sim_flags);
				break;

			case 'c':
				if (event.xkey.state & ControlMask)
					exit (EXIT_SUCCESS);

				option_sim_flags ^= _VBI3_RAW_SHIFT_CC_CRI;
				capture_stream_sim_set_flags
					(cst, option_sim_flags);
				break;

			case 'f':
				option_sim_flags ^= _VBI3_RAW_SWAP_FIELDS;
				capture_stream_sim_set_flags
					(cst, option_sim_flags);
				break;

			case 'g':
				draw_count = 1;
				next_frame = TRUE;
				break;

			case 'i':
				show_grid ^= TRUE;
				goto redraw;

			case 'l':
				draw_count = -1;
				break;

			case 'o':
				option_sim_flags ^= _VBI3_RAW_NOISE_2;
				capture_stream_sim_set_flags
					(cst, option_sim_flags);
				break;

			case 'p':
				show_points ^= TRUE;
				capture_stream_debug (cst, show_points);
				if (do_sim)
					capture_stream_sim_decode_raw
						(cst, show_points);
				goto redraw;

			case 'q':
			case XK_Escape:
				exit (EXIT_SUCCESS);

			case XK_Up:
				if (draw_row > 0) {
					draw_row--;
					goto redraw;
				}
				break;

			case XK_Down:
				if (draw_row < (src_h - 1)) {
					draw_row++;
					goto redraw;
				}
				break;

			case XK_Left:
				if (draw_offset > 0) {
					draw_offset -= 10;
					goto redraw;
				}
				break;

			case XK_Right:
				if (draw_offset < (src_w - 10)) {
					draw_offset += 10;
					goto redraw;  
				}
				break;
			}

			break;
		}

		case ButtonPress:
			fprintf (stderr, "%08x %08x\n",
				 event.xbutton.state,
				 event.xbutton.button);

			switch (event.xbutton.button) {
			case Button4:
				if (draw_row > 0) {
					draw_row--;
					goto redraw;
				}
				break;

			case Button5:
				if (draw_row < (src_h - 1)) {
					draw_row++;
					goto redraw;
				}
				break;

			default:
				break;
			}

			break;

		case ConfigureNotify:
			dst_w = event.xconfigurerequest.width;
			dst_h = event.xconfigurerequest.height;
redraw:
			if (draw_count == 0) {
				draw_count = 1;
				draw(sliced2, sliced2_lines, raw2,
				     sample_time2);
			}

			break;

		case MotionNotify:
		       cur_x = event.xmotion.x;
		       cur_y = event.xmotion.y;
		       // printf("Got MotionNotify: (%d, %d)\n", event.xmotion.x, event.xmotion.y);
		       goto redraw;
		       break;
		   
		case ClientMessage:
			exit(EXIT_SUCCESS);
		}

		if (next_frame)
			break;
	}
}

static void
init_window(const char *dev_name)
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

	XSelectInput(display, window, (PointerMotionMask |
				       KeyPressMask |
				       ButtonPressMask |
				       ExposureMask |
				       StructureNotifyMask));

	XSetWMProtocols(display, window, &delete_window_atom, 1);
	snprintf(buf, sizeof(buf) - 1, "%s - [cursor] [g]rab [l]ive gr[i]d [p]oints", dev_name);
	XStoreName(display, window, buf);

	gc = XCreateGC(display, window, 0, NULL);

	XMapWindow(display, window);
	       
	XSync(display, False);
}

static vbi3_bool
decode_function			(const vbi3_sliced *	sliced,
				 unsigned int		n_lines,
				 const uint8_t *	raw,
				 const vbi3_sampling_par *spp,
				 double			sample_time,
				 int64_t		stream_time)
{
	sample_time = sample_time;
	stream_time = stream_time;

	if (NULL != raw) {
		if (0 == src_w) {
			sp = *spp;

			assert (sp.sample_format == VBI3_PIXFMT_Y8);

			src_w = sp.bytes_per_line / 1;
			src_h = sp.count[0] + sp.count[1];

			init_window (option_dev_name);
		} else {
			assert (spp->bytes_per_line == sp.bytes_per_line);
			assert (spp->count[0] == sp.count[0]);
			assert (spp->count[1] == sp.count[1]);
		}

		draw (sliced, n_lines, raw, sample_time);
	} else {
		printf ("no raw data...\n");
		return TRUE;
	}

/*	printf("raw: %f; sliced: %d\n", timestamp, n_lines); */

	if (0 != src_w) {
		xevent();
	}

	return TRUE;
}

static const char short_options[] = "123acd:efino:pqsvy";

#ifdef HAVE_GETOPT_LONG
static const struct option
long_options[] = {
	{ "v4l",		no_argument,		NULL,	'1' },
	{ "v4l2-read",		no_argument,		NULL,	'2' },
	{ "v4l2-mmap",		no_argument,		NULL,	'3' },
	{ "sim-low-amp-cc",	no_argument,		NULL,	'a' },
	{ "sim-shift-cc-cri",	no_argument,		NULL,	'c' },
	{ "device",		required_argument,	NULL,	'd' },
	{ "ignore-error", 	no_argument,		NULL,	'e' },
	{ "sim-swap-fields",	no_argument,		NULL,	'f' },
	{ "sim-interlaced",	no_argument,		NULL,	'i' },
	{ "ntsc",		no_argument,		NULL,	'n' },
	{ "sim-noise",		optional_argument,	NULL,	'o' },
	{ "pal",		no_argument,		NULL,	'p' },
	{ "quiet",		no_argument,		NULL,	'q' },
	{ "sim",		no_argument,		NULL,	's' },
	{ "verbose",		no_argument,		NULL,	'v' },
	{ "desync",		no_argument,		NULL,	'y' },
	{ "sim-desync",		no_argument,		NULL,	'y' },
	{ NULL, 0, 0, 0 }
};
#else
#define getopt_long(ac, av, s, l, i) getopt(ac, av, s)
#endif

static int			option_index;

int
main(int argc, char **argv)
{
	unsigned int scanning;
	unsigned int interfaces;
	vbi3_bool have_dev_name;

	init_helpers (argc, argv);

	option_dev_name = "/dev/vbi";
	have_dev_name = FALSE;

	scanning = 625;

	interfaces = (INTERFACE_V4L2 |
		      INTERFACE_V4L |
		      INTERFACE_BKTR);

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
			interfaces = INTERFACE_V4L;
			break;

		case '2':
			/* Preliminary hack for tests (0.2 branch). */
//			vbi3_capture_force_read_mode = TRUE;
			interfaces = INTERFACE_V4L2;
			break;

		case '3':
			interfaces = INTERFACE_V4L2;
			break;

		case 'a':
			interfaces = INTERFACE_SIM;
			option_sim_flags |= _VBI3_RAW_LOW_AMP_CC;
			break;

		case 'c':
			interfaces = INTERFACE_SIM;
			option_sim_flags |= _VBI3_RAW_SHIFT_CC_CRI;
			break;

		case 'd':
			option_dev_name = optarg;
			have_dev_name = TRUE;
			break;

		case 'e':
			ignore_error ^= TRUE;
			break;

		case 'f':
			interfaces = INTERFACE_SIM;
			option_sim_flags |= _VBI3_RAW_SWAP_FIELDS;
			break;

		case 'n':
			scanning = 525;
			break;

		case 'o':
			/* Optional optarg: noise parameters
			   (not implemented yet). */
			interfaces = INTERFACE_SIM;
			option_sim_flags |= _VBI3_RAW_NOISE_2;
			break;

		case 'p':
			scanning = 625;
			break;

		case 'q':
			parse_option_quiet ();
			break;

		case 's':
			interfaces = INTERFACE_SIM;
			break;

		case 'v':
			parse_option_verbose ();
			break;

		case 'y':
			interfaces = INTERFACE_SIM;
			option_sim_synchronous = FALSE;
			break;

		default:
			fprintf(stderr, "Unknown option\n");
			exit(EXIT_FAILURE);
		}
	}

	if (!have_dev_name && !isatty (STDIN_FILENO)) {
		const char *option_in_file_name = NULL;
		int option_in_file_format = FILE_FORMAT_SLICED;
		int option_in_ts_pid = 0;

		cst = read_stream_new (option_in_file_name,
				       option_in_file_format,
				       option_in_ts_pid,
				       decode_function);
		step_mode = TRUE;
	} else {
		cst = capture_stream_new (interfaces,
					  option_dev_name,
					  scanning,
					  /* services */ -1,
					  /* n_buffers (V4L2 mmap) */ 5,
					  /* ts_pid (DVB) */ 0,
					  option_sim_interlaced,
					  option_sim_synchronous,
					  /* capture_raw_data */ TRUE,
					  /* read_not_pull */ FALSE,
					  /* strict */ 0,
					  decode_function);

		capture_stream_sim_set_flags (cst, option_sim_flags);
	}

	show_grid = TRUE;

	show_points = TRUE;
	capture_stream_debug (cst, TRUE);

	capture_stream_sim_decode_raw (cst, TRUE);

	stream_loop (cst);

	stream_delete (cst);

	exit (EXIT_SUCCESS);	
}

#else /* X_DISPLAY_MISSING */

int
main				(int			argc,
				 char **		argv)
{
	printf (_("Could not find X11 or X11 support has "
		  "been disabled at configuration time."));

	exit (EXIT_FAILURE);
}

#endif
