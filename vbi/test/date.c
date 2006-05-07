/*
 *  zvbi-date -- get station date and time from Teletext packet 8/30/2.
 *
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

/* $Id: date.c,v 1.1.2.1 2006-05-07 06:05:00 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <locale.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#ifdef HAVE_GETOPT_LONG
#  include <getopt.h>
#endif

#include "src/zvbi.h"
#include "src/intl-priv.h"

#define PROGRAM_NAME "zvbi-date"

static vbi3_capture *		cap;
static vbi3_decoder *		dec;

static vbi3_bool		option_set_time;
static unsigned int		option_verbosity;

#ifndef HAVE_PROGRAM_INVOCATION_NAME
static char *			program_invocation_name;
static char *			program_invocation_short_name;
#endif

static void
error_exit			(const char *		template,
				 ...)
{
	va_list ap;

	fprintf (stderr, "%s: ", program_invocation_short_name);

	va_start (ap, template);
	vfprintf (stderr, template, ap);
	va_end (ap);         

	fputc ('\n', stderr);

	exit (EXIT_FAILURE);
}

static void
no_mem_exit			(void)
{
	error_exit (_("Out of memory."));
}

static void
print_time			(time_t			time,
				 int			gmtoff)
{
	char *buffer;
	size_t buffer_size;
	struct tm tm;
	size_t len;

	CLEAR (tm);

	if (NULL == localtime_r (&time, &tm))
		error_exit (_("Invalid date received."));

	buffer_size = 1 << 16;

	buffer = malloc (buffer_size);
	if (NULL == buffer)
		no_mem_exit ();

#ifdef HAVE_TM_GMTOFF
	tm.tm_gmtoff = gmtoff;

	len = strftime (buffer, buffer_size, "%F %T %Z", &tm);
#else
	gmtoff = gmtoff; /* unused */

	len = strftime (buffer, buffer_size, "%F %T UTC", &tm);
#endif
	if (0 == len)
		no_mem_exit ();

	puts (buffer);

	free (buffer);
	buffer = NULL;
}

static void
set_time			(time_t			time)
{
#ifdef HAVE_CLOCK_SETTIME
#  ifdef CLOCK_REALTIME
	{
		struct timespec ts;

		ts.tv_sec = time; /* UTC */
		ts.tv_nsec = 0;

		if (0 == clock_settime (CLOCK_REALTIME, ts))
			return;
	}
#  endif
#endif

	{
		struct timeval tv;

		tv.tv_sec = time; /* UTC */
		tv.tv_usec = 0;

		if (0 == settimeofday (&tv, /* tz */ NULL))
			return;
	}

	error_exit (_("Cannot set system time. %s."),
		    strerror (errno));
}

static vbi3_bool
handler				(const vbi3_event *	ev,
				 void *			user_data)
{
	user_data = user_data; /* unused */

	switch (ev->type) {
	case VBI3_EVENT_LOCAL_TIME:
		if (option_verbosity >= 1) {
			print_time (ev->ev.local_time.time,
				    ev->ev.local_time.gmtoff);
		}

		if (option_set_time) {
			set_time (ev->ev.local_time.time);
		}

		exit (EXIT_SUCCESS);

		break;

	default:
		assert (0);
	}

	return TRUE; /* handled */
}

static void
mainloop			(void)
{
	vbi3_capture_buffer *sliced_buffer;
	struct timeval timeout;
	double first_timestamp;

	timeout.tv_sec = 2;
	timeout.tv_usec = 0;

	first_timestamp = 0.0;

	for (;;) {
		unsigned int n_lines;
		int r;

		r = vbi3_capture_pull (cap,
				       /* raw_buffer */ NULL,
				       &sliced_buffer,
				       &timeout);
		switch (r) {
		case -1:
			error_exit (_("VBI read error. %s."),
				    strerror (errno));

		case 0: 
			error_exit (_("VBI read timeout."));

		case 1: /* success */
			break;

		default:
			assert (0);
		}

		if (first_timestamp <= 0.0) {
			first_timestamp = sliced_buffer->timestamp;
		/* Packet 8/30/2 should repeat once every second. */
		} else if (sliced_buffer->timestamp - first_timestamp > 2.5) {
			error_exit (_("No station tuned in, weak reception, "
				      "or date and time not transmitted."));
		}

		n_lines = sliced_buffer->size / sizeof (vbi3_sliced);

		vbi3_decoder_feed (dec,
				   (vbi3_sliced *) sliced_buffer->data,
				   n_lines,
				   sliced_buffer->timestamp);
	}
}

static vbi3_capture *
open_device			(const char *		dev_name,
				 int			dvb_pid,
				 unsigned int		scanning,
				 unsigned int *		services,
				 int			strict)
{
	vbi3_capture *cap;
	char *errstr;
	vbi3_bool trace;

	errstr = NULL;
	trace = (option_verbosity >= 2);

	if (0 == strcmp (dev_name, "sim")) {
		cap = _vbi3_capture_sim_new (scanning,
					     services,
					     /* interlaced */ FALSE,
					     /* synchronous */ TRUE);
		if (NULL != cap)
			return cap;

		no_mem_exit ();
	}

#ifdef ENABLE_V4L

#  if 2 == VBI3_VERSION_MINOR /* not ported to 0.3 yet. */

	if (-1 != dvb_pid) {
		cap = vbi3_capture_dvb_new (dev_name,
					    scanning,
					    services,
					    strict,
					    &errstr,
					    trace);
		if (NULL != cap) {
			vbi3_capture_dvb_filter (cap, dvb_pid);
			return cap;
		}

		error_exit (_("Cannot capture VBI data "
			      "with DVB interface:\n%s"),
			    errstr);
	}

#  endif

	cap = vbi3_capture_v4l2_new (dev_name,
				     /* buffers */ 5,
				     services,
				     strict,
				     &errstr,
				     trace);
	if (NULL != cap)
		return cap;

	if (NULL != errstr) {
		free (errstr);
		errstr = NULL;
	}

	cap = vbi3_capture_v4l_new (dev_name,
				    scanning,
				    services,
				    strict,
				    &errstr,
				    trace);
	if (NULL != cap)
		return cap;

	if (NULL != errstr) {
		fprintf (stderr, _("Cannot capture VBI data "
				   "with V4L or V4L2 interface:\n%s\n"),
			 errstr);

		free (errstr);
		errstr = NULL;
	}

#endif /* ENABLE_V4L */

#ifdef ENABLE_BKTR

	cap = vbi3_capture_bktr_new (dev_name,
				     scanning,
				     services,
				     strict,
				     &errstr,
				     trace);
	if (NULL != cap)
		return cap;

	if (NULL != errstr) {
		fprintf (stderr, _("Cannot capture VBI data "
				   "with BKTR interface:\n%s\n"),
			 errstr);

		free (errstr);
		errstr = NULL;
	}

#endif /* ENABLE_BKTR */

	return NULL;
}

static void
usage				(FILE *			fp)
{
	fprintf (fp, _("\
%s %s -- get station date and time from Teletext\n\n\
Copyright (C) 2006 Michael H. Schimek\n\
This program is licensed under GPL 2. NO WARRANTIES.\n\n\
Usage: %s [options]\n\n\
Options:\n\
-d | --device name     VBI device to open [/dev/vbi]\n\
-h | --help | --usage  Print this message and exit\n\
-i | --pid n           Use Linux DVB interface and filter out\n\
                         VBI packets with this PID\n\
-s | --set             Set system time from received date and time\n\
-p | --pal | --secam   Video standard hint for V4L and DVB interface [PAL]\n\
-n | --ntsc            Video standard hint\n\
-v | --verbose         Increase verbosity\n\
-q | --quiet           Decrease verbosity (do not print date and time)\n\
-V | --version         Print the program version and exit\n\
"),
		 PROGRAM_NAME, VERSION, program_invocation_name);
}

static const char short_options [] = "d:hi:npqsvV";

#ifdef HAVE_GETOPT_LONG

static const struct option
long_options [] = {
	{ "device",	required_argument,	NULL,		'd' },
	{ "help",	no_argument,		NULL,		'h' },
	{ "usage",	no_argument,		NULL,		'h' },
	{ "pid",	required_argument,	NULL,		'i' },
	{ "ntsc",	no_argument,		NULL,		'n' },
	{ "pal",	no_argument,		NULL,		'p' },
	{ "secam",	no_argument,		NULL,		'p' },
	{ "quiet",	no_argument,		NULL,		'q' },
	{ "set",	no_argument,		NULL,		's' },
	{ "verbose",	no_argument,		NULL,		'v' },
	{ "version",	no_argument,		NULL,		'V' },
	{ NULL, 0, 0, 0 }
};

#else
#  define getopt_long(ac, av, s, l, i) getopt(ac, av, s)
#endif

static int			option_index;

int
main				(int			argc,
				 char **		argv)
{
	const char *dev_name;
	int dvb_pid;
	unsigned int scanning;
	unsigned int services;
	vbi3_bool success;

#ifndef HAVE_PROGRAM_INVOCATION_NAME
	{
		unsigned int i;

		for (i = strlen (argv[0]); i > 0; --i) {
			if ('/' == argv[0][i - 1])
				break;
		}

		program_invocation_short_name = &argv[0][i];
	}
#endif

	setlocale (LC_ALL, "");

	dev_name = "/dev/vbi";
	dvb_pid = -1;
	scanning = 625;

	option_verbosity = 1;

	for (;;) {
		int c;

		c = getopt_long (argc, argv, short_options,
				 long_options, &option_index);
		if (-1 == c)
			break;

		switch (c) {
		case 0: /* getopt_long() flag */
			break;

		case 'd':
			dev_name = optarg;
			break;

		case 'h':
			usage (stdout);
			exit (EXIT_SUCCESS);

		case 'i':
			dvb_pid = strtol (optarg, NULL, 0);
			break;

		case 'n':
			scanning = 525;
			break;

		case 'p':
			scanning = 625;
			break;

		case 'q':
			if (option_verbosity > 0)
				--option_verbosity;
			break;

		case 's':
			option_set_time ^= TRUE;
			break;

		case 'v':
			++option_verbosity;
			break;

		case 'V':
			printf (PROGRAM_NAME " " VERSION "\n");
			exit (EXIT_SUCCESS);

		default:
			usage (stderr);
			exit (EXIT_FAILURE);
		}
	}

	services = VBI3_SLICED_TELETEXT_B;

	cap = open_device (dev_name, dvb_pid, scanning,
			   &services, /* strict */ 0);
	if (NULL == cap) {
		/* Error message printed by open_device(). */
		exit (EXIT_FAILURE);
	}

	dec = vbi3_decoder_new (/* cache */ NULL,
				/* network */ NULL,
				VBI3_VIDEOSTD_SET_625_50);
	if (NULL == dec)
		no_mem_exit ();

	vbi3_decoder_detect_channel_change (dec, FALSE);

	success = vbi3_decoder_add_event_handler (dec,
						  VBI3_EVENT_LOCAL_TIME,
						  handler,
						  /* user_data */ NULL);
	if (NULL == dec)
		no_mem_exit ();

	mainloop ();

	exit (EXIT_SUCCESS);

	return 0;
}
