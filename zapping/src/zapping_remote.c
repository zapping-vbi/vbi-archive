/*
 * Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2003 Michael H. Schimek
 *
 * Shameless copy of xawtv-remote.c (C) Gerd Knorr
 * based on code shamelessly stolen from Netscape
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* $Id: zapping_remote.c,v 1.2 2004-09-10 04:58:53 mschimek Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xmu/WinUtil.h>    /* for XmuClientWindow() */

#include "zmisc.h"

static unsigned int debug = 0;

static int
x11_error_dev_null(Display * dpy _unused_, XErrorEvent * event _unused_)
{
    fprintf(stderr,"x11-error\n");
    return 0;
}

static Window
find_window(Display * dpy, Atom atom)
{
    int             n;
    unsigned int    i;
    Window          root = RootWindowOfScreen(DefaultScreenOfDisplay(dpy));
    Window          root2, parent, *kids;
    unsigned int    nkids;
    Window          result = 0;

    if (!XQueryTree(dpy, root, &root2, &parent, &kids, &nkids)) {
        fprintf(stderr, "XQueryTree failed on display %s\n",
                DisplayString(dpy));
        exit(2);
    }

    if (!(kids && nkids)) {
        fprintf(stderr, "root window has no children on display %s\n",
                DisplayString(dpy));
        exit(2);
    }
    for (n = nkids - 1; n >= 0; n--) {
        Atom            type;
        int             format;
        unsigned long   nitems, bytesafter;
        unsigned char  *args = NULL;

        Window          w = XmuClientWindow(dpy, kids[n]);

        XGetWindowProperty(dpy, w, atom,
                           0, (65536 / sizeof(long)),
                           False, XA_STRING,
                           &type, &format, &nitems, &bytesafter,
                           &args);

        if (!args)
            continue;
	if (debug) {
	    printf("query 0x%08lx: ",w);
	    for (i = 0; i < nitems; i += strlen(args + i) + 1)
		printf("%s ", args + i);
	    printf("\n");
	}
        XFree(args);

        result = w;
#if 0 /* there might be more than window */
        break;
#endif
    }
    return result;
}

static void
pass_cmd			(Display *		dpy,
				 Atom			atom,
				 Window			win,
				 int			argc,
				 char **		argv)
{
    int             i, len;
    char           *pass;

    if (debug)
	printf("ctrl  0x%08lx: ",win);
    for (len = 0, i = 0; i < argc; i++) {
	if (debug)
	    printf("%s ",argv[i]);
        len += strlen(argv[i]) + 1;
    }
    if (debug)
	printf("\n");
    pass = malloc((unsigned int) len);
    pass[0] = 0;
    for (len = 0, i = 0; i < argc; i++)
        strcpy(pass + len, argv[i]),
            len += strlen(argv[i]) + 1;
    XChangeProperty(dpy, win,
                    atom, XA_STRING,
                    8, PropModeReplace,
                    pass, len);
    free(pass);
}

static void
usage				(const char *		argv0)
{
    const char *prog;

    if (NULL != (prog = strrchr (argv0, '/')))
	++prog;
    else
	prog = argv0;

    fprintf (stderr,
"This is an experimental \"remote control\" for Zapping.\n"
"Usage: %s [ options ] [ command ]\n"
"\n"
"Available options:\n"
"    -d display\n"
"        select X11 display.\n"
"    -i window ID\n"
"        select XawTV or Zapping window.\n"
"    -v n\n"
"        Set debug level to n, default 0.\n"
"    -x\n"
"        xawtv-remote compatible.\n"
"\n"
"By default this tool sends Python commands to Zapping. You\n"
"will have to put commands in \'single\' or \"double\" quotes\n"
"to prevent shell expansion. When the -x option is given you\n"
"can send XawTV commands, see the xawtv-remote manual page for\n"
"details. Zapping also responds to xawtv-remote itself.\n"
"\n"
"Usage example:\n"
"%s \'zapping.mute()\'\n"
	, prog, prog);
}

int
main				(int			argc,
				 char **		argv)
{
    char *display_name;
    Window window;
    int xawtv_mode;
    Display *display;
    Atom _XA_XAWTV_STATION;
    Atom _XA_XAWTV_REMOTE;
    Atom _XA_ZAPPING_REMOTE;

    display_name = NULL;
    window = 0;
    xawtv_mode = 0;

    for (;;) {
	int c;

	c = getopt (argc, argv, "hd:i:v:x");

	if (-1 == c)
	    break;

	switch (c) {
	case 'd':
	    display_name = optarg;
	    break;

	case 'i':
	    window = strtol (optarg, NULL, 0);
	    break;

	case 'v':
	    debug = strtol (optarg, NULL, 0);
	    break;

	case 'x':
	    xawtv_mode ^= 1;
	    break;

	case 'h':
	    usage (argv[0]);
	    exit (0);

	default:
	    usage (argv[0]);
	    exit (1);
	}
    }

    display = XOpenDisplay (display_name);

    if (!display) {
	fprintf (stderr, "Can't open display %s\n",
		 display_name ? display_name : "");
	exit (1);
    }

    XSetErrorHandler (x11_error_dev_null);

    _XA_XAWTV_STATION = XInternAtom (display, "_XAWTV_STATION", False);
    _XA_XAWTV_REMOTE = XInternAtom (display, "_XAWTV_REMOTE", False);
    _XA_ZAPPING_REMOTE = XInternAtom (display, "_ZAPPING_REMOTE", False);

    if (0 == window) {
	window = find_window (display, _XA_XAWTV_STATION);

	if (0 == window) {
	    fprintf (stderr, "Zapping not running\n");
	    exit (2);
	}
    }

    if (argc > optind) {
	if (xawtv_mode) {
	    pass_cmd (display, _XA_XAWTV_REMOTE, window,
		      argc - optind, argv + optind);
	} else {
	    pass_cmd (display, _XA_ZAPPING_REMOTE, window,
		      argc - optind, argv + optind);
	}
    } else {
       /* interactive mode? zapping_remote <file.py? */
    }

    XCloseDisplay (display);

    return 0;
}
