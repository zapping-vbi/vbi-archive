/*
 *  libzvbi test
 *
 *  Copyright (C) 2000, 2001, 2002 Michael H. Schimek
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

/* $Id: capture.c,v 1.7.2.12 2006-05-26 00:43:07 mschimek Exp $ */

#undef NDEBUG

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <locale.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#ifdef HAVE_GETOPT_LONG
#include <getopt.h>
#endif

#include "src/vbi.h"
#include "src/zvbi.h"
#include "src/misc.h"
#include "sliced.h"

#define _(x) x

vbi3_capture *		cap;
vbi3_raw_decoder *	rd;
vbi3_sampling_par	sp;
vbi3_dvb_mux *		mx;
vbi3_bool		quit;
int			src_w, src_h;

int			dump;
int			dump_xds;
int			dump_cc;
int			dump_wss;
int			dump_sliced;
int			bin_sliced;
int			bin_pes;
int			bin_ts;
int			do_read = TRUE;
int			do_sim;
int			desync;

/*
 *  Dump
 */

static inline int
odd_parity(uint8_t c)
{
	c ^= (c >> 4);
	c ^= (c >> 2);
	c ^= (c >> 1);

	return c & 1;
}

static inline int
vbi3_hamm16(uint8_t *p)
{
	return vbi3_unham16p (p);
}

extern int vbi3_printable (int);

static void
decode_xds(uint8_t *buf)
{
	if (dump_xds) {
		char c;

		c = odd_parity(buf[0]) ? buf[0] & 0x7F : '?';
		c = _vbi3_to_ascii (c);
		putchar(c);
		c = odd_parity(buf[1]) ? buf[1] & 0x7F : '?';
		c = _vbi3_to_ascii (c);
		putchar(c);
		fflush(stdout);
	}
}

static void
decode_caption(uint8_t *buf, int line)
{
	static vbi3_bool xds_transport = FALSE;
	char c = buf[0] & 0x7F;

	if (line >= 280) { /* field 2 */
		/* 0x01xx..0x0Exx ASCII_or_NUL[0..32] 0x0Fchks */
		if (odd_parity(buf[0]) && (c >= 0x01 && c <= 0x0F)) {
			decode_xds(buf);
			xds_transport = (c != 0x0F);
		} else if (xds_transport) {
			decode_xds(buf);
		}

		return;
	}

	if (dump_cc) {
		c = odd_parity(buf[0]) ? buf[0] & 0x7F : '?';
		c = _vbi3_to_ascii (c);
		putchar(c);
		c = odd_parity(buf[1]) ? buf[1] & 0x7F : '?';
		c = _vbi3_to_ascii (c);
		putchar(c);
		fflush(stdout);
	}
}

static void
decode_wss_625(uint8_t *buf)
{
	static const char *formats[] = {
		"Full format 4:3, 576 lines",
		"Letterbox 14:9 centre, 504 lines",
		"Letterbox 14:9 top, 504 lines",
		"Letterbox 16:9 centre, 430 lines",
		"Letterbox 16:9 top, 430 lines",
		"Letterbox > 16:9 centre",
		"Full format 14:9 centre, 576 lines",
		"Anamorphic 16:9, 576 lines"
	};
	static const char *subtitles[] = {
		"none",
		"in active image area",
		"out of active image area",
		"?"
	};
	int g1 = buf[0] & 15;
	int parity;

	if (dump_wss) {
		parity = g1;
		parity ^= parity >> 2;
		parity ^= parity >> 1;
		g1 &= 7;

		printf("WSS PAL: ");
		if (!(parity & 1))
			printf("<parity error> ");
		printf("%s; %s mode; %s colour coding;\n"
		       "      %s helper; reserved b7=%d; %s\n"
		       "      open subtitles: %s; %scopyright %s; copying %s\n",
		       formats[g1],
		       (buf[0] & 0x10) ? "film" : "camera",
		       (buf[0] & 0x20) ? "MA/CP" : "standard",
		       (buf[0] & 0x40) ? "modulated" : "no",
		       !!(buf[0] & 0x80),
		       (buf[1] & 0x01) ? "have TTX subtitles; " : "",
		       subtitles[(buf[1] >> 1) & 3],
		       (buf[1] & 0x08) ? "surround sound; " : "",
		       (buf[1] & 0x10) ? "asserted" : "unknown",
		       (buf[1] & 0x20) ? "restricted" : "not restricted");
	}
}

static void
decode_wss_cpr1204(uint8_t *buf)
{
	const int poly = (1 << 6) + (1 << 1) + 1;
	int g = (buf[0] << 12) + (buf[1] << 4) + buf[2];
	int j, crc;

	if (dump_wss) {
		crc = g | (((1 << 6) - 1) << (14 + 6));

		for (j = 14 + 6 - 1; j >= 0; j--) {
			if (crc & ((1 << 6) << j))
				crc ^= poly << j;
		}

		fprintf(stderr, "WSS CPR >> g=%08x crc=%08x\n", g, crc);
	}
}

static void
decode_sliced(vbi3_sliced *s, double time, int lines)
{
	if (dump_sliced) {
		vbi3_sliced *q = s;
		int i;

		printf("Sliced time: %f\n", time);

		for (i = 0; i < lines; q++, i++) {
			unsigned int j;

			printf("%04x %3d > ", q->id, q->line);

			for (j = 0; j < sizeof(q->data); j++) {
				printf("%02x ", (uint8_t) q->data[j]);
			}

			putchar(' ');

			for (j = 0; j < sizeof(q->data); j++) {
				char c = _vbi3_to_ascii (q->data[j]);
				putchar(c);
			}

			putchar('\n');
		}
	}

	for (; lines > 0; s++, lines--) {
		if (s->id == 0) {
			continue;
		} else if (s->id & VBI3_SLICED_VPS) {
		} else if (s->id & VBI3_SLICED_TELETEXT_B) {
		} else if (s->id & VBI3_SLICED_CAPTION_525) {
			decode_caption(s->data, s->line);
		} else if (s->id & VBI3_SLICED_CAPTION_625) {
			decode_caption(s->data, s->line);
		} else if (s->id & VBI3_SLICED_WSS_625) {
			decode_wss_625(s->data);
		} else if (s->id & VBI3_SLICED_WSS_CPR1204) {
			decode_wss_cpr1204(s->data);
		} else {
			fprintf(stderr, "Oops. Unhandled vbi service %08x\n",
				s->id);
		}
	}
}

static vbi3_bool
binary_ts_pes			(vbi3_dvb_mux *		mx,
				 void *			user_data,
				 const uint8_t *	packet,
				 unsigned int		packet_size)
{
	size_t r;

	mx = mx;
	user_data = user_data;

	r = fwrite (packet, 1, packet_size, stdout);

	return (r == packet_size);
}

static void
mainloop(void)
{
	uint8_t	*raw;
	vbi3_sliced *sliced;
	int n_lines;
	vbi3_capture_buffer *raw_buffer;
	vbi3_capture_buffer *sliced_buffer;
	double timestamp;
	struct timeval tv;
	int64_t pts;

	tv.tv_sec = 2;
	tv.tv_usec = 0;

	raw = malloc(src_w * src_h);
	sliced = malloc(sizeof(vbi3_sliced) * src_h);

	assert(raw && sliced);

	pts = 0;

	for (quit = FALSE; !quit;) {
		int r;

		if (do_read) {
			r = vbi3_capture_read(cap, raw, sliced,
					     &n_lines, &timestamp, &tv);
		} else {
			r = vbi3_capture_pull(cap, &raw_buffer,
					     &sliced_buffer, &tv);
		}

		switch (r) {
		case -1:
			fprintf(stderr, "VBI read error: %d, %s\n",
				errno, strerror(errno));
			exit(EXIT_FAILURE);
		case 0: 
			fprintf(stderr, "VBI read timeout\n");
			exit(EXIT_FAILURE);
		case 1:
			break;
		default:
			assert(!"reached");
		}

		if (!do_read) {
			sliced = sliced_buffer->data;
			n_lines = sliced_buffer->size / sizeof (vbi3_sliced);
			timestamp = sliced_buffer->timestamp;
		}

		if (dump)
			decode_sliced(sliced, timestamp, n_lines);

		if (bin_sliced) {
			/* Error ignored. */
			write_sliced (sliced, n_lines, timestamp);
		}

		if (bin_pes || bin_ts) {
			/* XXX shouldn't use system time. */
			pts = timestamp * 90000;
			/* Error ignored. */
			_vbi3_dvb_mux_mux(mx, sliced, n_lines, -1, pts);
		}
	}
}

static const char short_options[] = "a:cd:elnpstvPT";

#ifdef HAVE_GETOPT_LONG
static const struct option
long_options[] = {
	{ "sim-cc",	required_argument,	NULL,		'a' },
	{ "desync",	no_argument,		NULL,		'c' },
	{ "device",	required_argument,	NULL,		'd' },
	{ "dump-xds",	no_argument,		&dump_xds,	TRUE },
	{ "dump-cc",	no_argument,		&dump_cc,	TRUE },
	{ "dump-wss",	no_argument,		&dump_wss,	TRUE },
	{ "dump-sliced",no_argument,		&dump_sliced,	TRUE },
	{ "pes",	no_argument,		NULL,		'P' },
	{ "sliced",	no_argument,		NULL,		'l' },
	{ "ts",		no_argument,		NULL,		'T' },
	{ "read",	no_argument,		&do_read,	TRUE },
	{ "pull",	no_argument,		&do_read,	FALSE },
	{ "sim",	no_argument,		NULL,		's' },
	{ "ntsc",	no_argument,		NULL,		'n' },
	{ "pal",	no_argument,		NULL,		'p' },
	{ "verbose",	no_argument,		NULL,		'v' },
	{ NULL, 0, 0, 0 }
};
#else
#define getopt_long(ac, av, s, l, i) getopt(ac, av, s)
#endif

static const char **sim_cc_streams;
static unsigned int n_sim_cc_streams;

static char *
load_string			(const char *		name)
{
	FILE *fp;
	char *buffer;
	size_t buffer_size;
	size_t done;

	buffer = NULL;
	buffer_size = 0;
	done = 0;

	fp = fopen (name, "r");
	if (NULL == fp) {
		exit (EXIT_FAILURE);
	}

	for (;;) {
		char *new_buffer;
		size_t new_size;
		size_t space;
		size_t actual;

		new_size = 16384;
		if (buffer_size > 0)
			new_size = buffer_size * 2;

		new_buffer = realloc (buffer, new_size);
		if (NULL == new_buffer) {
			free (buffer);
			exit (EXIT_FAILURE);
		}

		buffer = new_buffer;
		buffer_size = new_size;

		space = buffer_size - done - 1;
		actual = fread (buffer + done, 1, space, fp);
		if ((size_t) -1 == actual) {
			exit (EXIT_FAILURE);
		}
		done += actual;
		if (actual < space)
			break;
	}

	buffer[done] = 0;

	fclose (fp);

	return buffer;
}

static void
load_caption			(void)
{
	unsigned int i;

	for (i = 0; i < n_sim_cc_streams; ++i) {
		char *buffer;
		vbi3_bool success;

		fprintf (stderr, "Loading '%s'.\n", sim_cc_streams[i]);

		buffer = load_string (sim_cc_streams[i]);
		success = vbi3_capture_sim_load_caption
			(cap, buffer, /* append */ (i > 0));
		assert (success);
	}
}

int
main(int argc, char **argv)
{
	const char *dev_name = "/dev/vbi";
	char *errstr;
	unsigned int services;
	int scanning = 625;
	vbi3_bool verbose = FALSE;
	int c, index;

	setlocale (LC_ALL, "");

	sim_cc_streams = NULL;
	n_sim_cc_streams = 0;

	while ((c = getopt_long(argc, argv, short_options,
				long_options, &index)) != -1) {
		switch (c) {
		case 0: /* set flag */
			break;
		case 'a':
		{
			const char **pp;

			pp = realloc (sim_cc_streams,
				      (n_sim_cc_streams + 1)
				      * sizeof (*pp));
			assert (NULL != pp);
			sim_cc_streams = pp;
			sim_cc_streams[n_sim_cc_streams++] = optarg;
			do_sim = TRUE;
			break;
		}

		case 'c':
			desync ^= TRUE;
			break;
		case 'd':
			dev_name = optarg;
			break;
		case 'P':
			bin_pes ^= TRUE;
			break;
		case 'l':
			bin_sliced ^= TRUE;
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
		case 'T':
			bin_ts ^= TRUE;
			break;
		case 'v':
			verbose ^= TRUE;
			break;
		default:
			fprintf(stderr, "Unknown option\n");
			exit(EXIT_FAILURE);
		}
	}

	dump = dump_xds | dump_cc
		| dump_wss | dump_sliced;

	services = VBI3_SLICED_VBI3_525 | VBI3_SLICED_VBI3_625
		| VBI3_SLICED_TELETEXT_B | VBI3_SLICED_CAPTION_525
		| VBI3_SLICED_CAPTION_625
	  	| VBI3_SLICED_VPS | VBI3_SLICED_VPS_F2
		| VBI3_SLICED_WSS_625 | VBI3_SLICED_WSS_CPR1204;

	if (verbose) {
		vbi3_set_log_fn (VBI3_LOG_INFO - 1,
				 vbi3_log_on_stderr,
				 /* user_data */ NULL);
	}

	do {
		if (do_sim) {
			cap = vbi3_capture_sim_new (scanning, &services,
						    /* interlaced */ FALSE,
						    !desync);
			assert (NULL != cap);
			
			load_caption ();

			break;
		}

		cap = vbi3_capture_v4l2_new (dev_name,
					     /* buffers */ 5,
					     &services,
					     /* strict */ 0,
					     &errstr,
					     /* trace */ verbose);
		if (cap)
			break;

		fprintf (stderr, "Cannot capture vbi data "
			 "with v4l2 interface:\n%s\n", errstr);

		free (errstr);

		cap = vbi3_capture_v4l_new (dev_name,
					    scanning,
					    &services,
					    /* strict */ 0,
					    &errstr,
					    /* trace */ verbose);
		if (cap)
			break;

		fprintf (stderr, "Cannot capture vbi data "
			 "with v4l interface:\n%s\n", errstr);

		free (errstr);

		cap = vbi3_capture_bktr_new (dev_name,
					     scanning,
					     &services,
					     /* strict */ 0,
					     &errstr,
					     /* trace */ verbose);
		if (cap)
			break;

		fprintf (stderr, "Cannot capture vbi data "
			 "with bktr interface:\n%s\n", errstr);

		free (errstr);

		exit(EXIT_FAILURE);
	} while (0);

	assert ((rd = vbi3_capture_parameters(cap)));

	vbi3_raw_decoder_get_sampling_par (rd, &sp);

	assert (sp.sampling_format == VBI3_PIXFMT_Y8);

	src_w = sp.bytes_per_line / 1;
	src_h = sp.count[0] + sp.count[1];

	if (bin_sliced) {
		struct timeval tv;
		double timestamp;
		vbi3_bool success;
		int r;

		r = gettimeofday (&tv, NULL);
		assert (0 == r);

		timestamp = tv.tv_sec + tv.tv_usec * (1 / 1e6);

		success = open_sliced_write (stdout, timestamp);
		assert (success);
	} else if (bin_pes) {
		mx = _vbi3_dvb_mux_pes_new (/* data_identifier */ 0x10,
					   /* packet_size */ 8 * 184,
					   VBI3_VIDEOSTD_SET_625_50,
					   binary_ts_pes,
					   /* user_data */ NULL);
		assert (NULL != mx);
	} else if (bin_ts) {
		mx = _vbi3_dvb_mux_ts_new (/* pid */ 123,
					  /* data_identifier */ 0x10,
					  /* packet_size */ 8 * 184,
					  VBI3_VIDEOSTD_SET_625_50,
					  binary_ts_pes,
					  /* user_data */ NULL);
		assert (NULL != mx);
	}

	mainloop();

	vbi3_capture_delete (cap);

	exit(EXIT_SUCCESS);	
}
