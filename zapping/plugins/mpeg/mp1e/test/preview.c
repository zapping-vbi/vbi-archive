/*
 *  MPEG-1 Real Time Encoder
 *
 *  Copyright (C) 2000 Michael H. Schimek
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

/* $Id: preview.c,v 1.2 2000-10-22 05:24:50 mschimek Exp $ */

#if defined(HAVE_LIBXV) && defined (TEST_PREVIEW)

#include <assert.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>
#include <stdio.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include "support.h"
#include "../common/log.h"

static Display		*display;
static int		screen;
static Window 		root_window, window;
static GtkWidget	*gtk_win;
static GC		gc;

static Visual 		*visual, *default_visual;

static unsigned int 	version, revision;
static unsigned int 	major_opcode;
static unsigned int 	event_base;
static unsigned int 	error_base;

// static Atom		atom_filter;

static XvPortID		port;
static XvAdaptorInfo 	*pAdaptor, *pAdaptors;
static XvFormat 	*pFormat;
static XvImageFormatValues *pImgFormats;
static int 		nAdaptors, nImgFormats;
// static int 		nFormats;

static XShmSegmentInfo	shminfo;
static XvImage		*ximage;
  static XEvent		event;
// static int		CompletionType = -1;

extern int		width, height;

struct test_capture_param{
	char	*label;
	float	default_value;
	float	step;
	float	min, max;
	int	digits;
};

static struct test_capture_param params[] =
{
	{"Video bit rate:", 2.3, 0.01, 0.01, 8, 2},
	{"Force drop rate (%):", 0, 1, 0, 100, 0},
	{"P inter bias:", 48, 1, 0, 256, 0},
	{"B inter bias:", 96, 1, 0, 256, 0},
	{"Frame rate:", 25, 0.01, 1, 30, 2},
	{"Quantization limit:", 31, 1, 1, 31, 0}
};

static int num_params = sizeof(params)/sizeof(struct test_capture_param);

extern int video_bit_rate;
extern int video_do_reset;
extern int force_drop_rate;
extern int p_inter_bias;
extern int b_inter_bias;
extern double frame_rate;
extern int quant_max;

/* callbacks */
static void
on_capture_param_changed(GtkAdjustment *adj,
			 gint i)
{
	double value = adj->value;

	switch (i) {
	case 0:
		value = value * 1e6;
		if (value != video_bit_rate) {
			video_bit_rate = value;
			video_do_reset = 1;
		}
		break;
	case 1:
		force_drop_rate = value;
		break;
	case 2:
		p_inter_bias = value * 65536;
		break;
	case 3:
		b_inter_bias = value * 65536;
		break;
	case 4:
		frame_rate = value;
		video_do_reset = 1;
		break;
	case 5:
		quant_max = value;
		video_do_reset = 1;
		break;
	default:
		fprintf(stderr, "didn't know about gui item %d\n", i);
	}
}

static void
on_capture_param_reset(GtkButton * button,
		       gint i)
{
	gtk_adjustment_set_value(GTK_ADJUSTMENT(gtk_object_get_user_data(GTK_OBJECT(button))),
				 params[i].default_value);
}

/* builds a control in the control box */
static GtkWidget *
build_control(gint i)
{
	GtkWidget * hbox;
	GtkWidget * label;
	GtkWidget * hscale;
	GtkWidget * button;
	GtkObject * adj;

	hbox = gtk_hbox_new (TRUE, 9);
	label = gtk_label_new(params[i].label);
	gtk_widget_show(label);
	gtk_box_pack_start_defaults(GTK_BOX (hbox), label);
	
	adj = gtk_adjustment_new(params[i].default_value, params[i].min,
				 params[i].max, params[i].step, 0,
				 0);

	gtk_signal_connect(adj, "value-changed",
			   GTK_SIGNAL_FUNC(on_capture_param_changed),
			   GINT_TO_POINTER(i));

	hscale = gtk_hscale_new (GTK_ADJUSTMENT(adj));
	gtk_widget_show(hscale);
	gtk_scale_set_digits(GTK_SCALE(hscale), params[i].digits);
	gtk_box_pack_start_defaults(GTK_BOX (hbox), hscale);

	button = gtk_button_new_with_label(_("Reset"));
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
			   GTK_SIGNAL_FUNC(on_capture_param_reset),
			   GINT_TO_POINTER(i));
	gtk_object_set_user_data(GTK_OBJECT(button), adj);
	gtk_widget_show(button);
	gtk_box_pack_start_defaults(GTK_BOX (hbox), button);

	return hbox;
}

/* builds the control box */
static GtkWidget *
build_control_box(void)
{
	GtkWidget * vbox;
	GtkWidget * control_box;
	GtkWidget * control;
	gint i;

	vbox = gtk_vbox_new (FALSE, 5);
	for (i=0; i<num_params; i++) {
		control = build_control (i);
		gtk_widget_show(control);
		gtk_box_pack_start_defaults(GTK_BOX (vbox), control);
	}
	control_box = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(control_box), _("Mp1e"));
	gtk_window_set_policy(GTK_WINDOW(control_box), FALSE, TRUE, TRUE);
	gtk_widget_show(vbox);
	gtk_container_add(GTK_CONTAINER(control_box), vbox);

	return control_box;
}

/*
 *  Use a buffer fifo (depth 3-4), presentation time stamps and a combined
 *  event/delay handler with a separate thread. Ref buffer layout must
 *  be 4:2:0 then we save a conversion and copy operation here.
 */

void
packed_preview(unsigned char *buffer, int mb_cols, int mb_rows)
{
	unsigned char *Y, *U, *V;
	int y_pitch, uv_pitch;
	int y_pitch8, uv_pitch8;
	int col, row, line;
	int wwidth, wheight;

	y_pitch8 = (y_pitch = ximage->pitches[0]) * 8;
	uv_pitch8 = (uv_pitch = ximage->pitches[1]) * 8;

	for (row = 0; row < mb_rows; row++) {
		Y = ximage->data + ximage->offsets[0] + y_pitch8 * row * 2;
		V = ximage->data + ximage->offsets[1] + uv_pitch8 * row;
		U = ximage->data + ximage->offsets[2] + uv_pitch8 * row;

		for (col = 0; col < mb_cols; col++) {
			for (line = 0; line < 8; line++) {
				memcpy(Y + 0, buffer + 0 * 64, 8); // Y0
				memcpy(Y + 8, buffer + 2 * 64, 8); // Y1
				memcpy(Y + y_pitch8 + 0, buffer + 1 * 64, 8); // Y2
				memcpy(Y + y_pitch8 + 8, buffer + 3 * 64, 8); // Y3
				memcpy(U, buffer + 4 * 64, 8); // Cb
				memcpy(V, buffer + 5 * 64, 8); // Cr

				buffer += 8;	// next 8 x 1 samples
				Y += y_pitch;	// next row
				U += uv_pitch;
				V += uv_pitch;
			}

			buffer += 5 * 64;	// next macroblock
			Y += 16 - y_pitch8;	// same strip, next column
			U += 8 - uv_pitch8;
			V += 8 - uv_pitch8;
		}
	}

	/* adquire the global gdk lock */
	gdk_threads_enter();

	XSync(display, False);

	gdk_window_get_size(gtk_win->window, &wwidth, &wheight);

	XvShmPutImage(display, port, window, gc, ximage,
		0, 0, width, height, // source coordinates
		0, 0, wwidth, wheight, // destination
		True); // wait for completion

	gdk_threads_leave(); /* release the lock */
}

#define YV12 0x32315659

void
preview_init(int * argc, char ***argv)
{
	int i, j, k;
	char *title = "MP1E Preview";
	XVisualInfo *pVisInfo, VisInfoTmpl;

	gtk_init(argc, argv);

	ASSERTX("open X11 display", display = GDK_DISPLAY());

	screen = XDefaultScreen(display);
        root_window = RootWindow(display, screen);

	if (Success != XvQueryExtension(display, &version, &revision,
		&major_opcode, &event_base, &error_base)) {
		FAIL("Xv extension not available");
	}

	if (version < 2 || revision < 2)
		FAIL("Xv version number %d.%d mismatch, "
			"should be 2.2 or later\n", version, revision);

	XvQueryAdaptors(display, root_window, &nAdaptors, &pAdaptors);

	if (nAdaptors == 0)
		FAIL("No Xv adaptors found");

	// atom_filter	= XInternAtom(display, "XV_FILTER", False);

	for (i = 0; i < nAdaptors; i++) {
		pAdaptor = pAdaptors + i;

		pFormat = pAdaptor->formats;

		for (j = 0; j < pAdaptor->num_ports; j++) {


			port = pAdaptor->base_id + j;

			printv(3, "Probing Xv adaptor #%d/%d, Port #%d/%d\n",
				i, nAdaptors, j, (int) pAdaptor->num_ports);

			// Let's see if this is an XvImage port...

			if (!(pImgFormats = XvListImageFormats(display, port, &nImgFormats)))
				continue;

			if (Success != XvGrabPort(display, port, CurrentTime))
				continue;

			// ...and supports YVU 4:2:0

			for (k = 0; k < nImgFormats; k++)
				if (pImgFormats[k].id == YV12)
					goto create;

			XvUngrabPort(display, port, CurrentTime);
		}
	}

	FAIL("This display does not support the XvImage extension,\n"
	    "the YVU 4:2:0 format, or all ports are occupied\n");

create:

	// Create an image buffer

	ximage = (XvImage *) XvShmCreateImage(display, port,
		YV12, NULL, (width + 63) & -64, height, &shminfo);
	/*
	 *  Width is padded to encourage cache line alignment of rows.
	 *  We clip to the actual size with XvPutShmImage. (And hope
	 *  the driver won't try something stupid...)
	 */

	printv(3, "ximage: %dx%d, %d planes\npitches: ",
		ximage->width, ximage->height, ximage->num_planes);
	for (i = 0; i < ximage->num_planes; i++)
		printv(3, "%d ", ximage->pitches[i]);
	printv(3, "\noffsets: ");
	for (i = 0; i < ximage->num_planes; i++)
		printv(3, "0x%08x=%d ", ximage->offsets[i], ximage->offsets[i]);
	printv(3, "\n");

	assert(ximage->num_planes == 3);
	assert(ximage->pitches[1] == ximage->pitches[2]);

	// Now we need a shared memory segment to bypass the X11 socket

	shminfo.shmid = shmget(IPC_PRIVATE, ximage->data_size, IPC_CREAT | 0777);
	shminfo.shmaddr = ximage->data = shmat(shminfo.shmid, 0, 0);
	shmctl(shminfo.shmid, IPC_RMID, 0); // remove when we terminate

	shminfo.readOnly = False;

	if (!XShmAttach(display, &shminfo))
		FAIL("XShmAttach failed");

	// CompletionType = XShmGetEventBase(display) + ShmCompletion;
	// redundant

	// This allocates a suitable colormap for 8 bit depth if needed

	default_visual = XDefaultVisual(display, screen);

	VisInfoTmpl.visualid = pFormat->visual_id;

	ASSERTX("find suitable X11 visual",
		pVisInfo = XGetVisualInfo(display,
			VisualIDMask, &VisInfoTmpl, &i));

	visual = pVisInfo->visual;

	// Create a simple window

	gtk_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_widget_set_usize(gtk_win, width, height);
	gtk_window_set_title(GTK_WINDOW(gtk_win), title);
	gtk_widget_show(gtk_win);
	while (gtk_events_pending() || (!gtk_win->window))
		gtk_main_iteration();

	gdk_window_set_events(gtk_win->window, GDK_STRUCTURE_MASK |
			      GDK_KEY_PRESS_MASK | GDK_EXPOSURE_MASK);

	window = GDK_WINDOW_XWINDOW(gtk_win->window);

	gc = GDK_GC_XGC(gtk_win->style->white_gc);

	gtk_widget_show(build_control_box());
}

void *
gtk_main_thread(void * unused)
{
	while (1)
	{
		gdk_threads_enter();
		while (gtk_events_pending())
			gtk_main_iteration();
		gdk_threads_leave();
		usleep(50000);
	}
	
	/* it won't reach this */
	return NULL;
}

#else

void
packed_preview(unsigned char *buffer, int mb_cols, int mb_rows)
{
}

void
preview_init(int *argc, char ***argv)
{
}

#endif // HAVE_LIBXV
