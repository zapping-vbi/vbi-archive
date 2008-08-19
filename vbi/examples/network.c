/*
 *  libzvbi network identification example
 *
 *  Copyright (C) 2006 Michael H. Schimek
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

/* $Id: network.c,v 1.5.2.4 2008-08-19 13:14:08 mschimek Exp $ */

/* This example shows how to identify a network from data transmitted
   in XDS packets, Teletext packet 8/30 format 1 and 2, and VPS packets.

   gcc -o network network.c `pkg-config zvbi-0.3 --cflags --libs` */

#undef NDEBUG

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <errno.h>

#if 1 /* To compile this program in the libzvbi source tree. */
#  include "src/zvbi.h"
#else /* To compile this program against the installed library. */
#  include <zvbi/zvbi.h>
#endif

static vbi3_capture *		cap;
static vbi3_decoder *		dec;
static vbi3_bool		quit;
static unsigned int		services;

static vbi3_bool
handler				(const vbi3_event *	ev,
				 void *			user_data)
{
	const vbi3_network *nk;
	const char *network_name;
	const char *call_sign;

	user_data = user_data; /* unused, no warning please */

	assert (VBI3_EVENT_NETWORK == ev->type);
	assert (NULL != ev->network);

	nk = ev->network;

	network_name = "unknown";
	if (NULL != nk->name) {
		network_name = nk->name;
		quit = TRUE;
	}

	/* ASCII. */
	call_sign = "unknown";
	if (0 != nk->call_sign[0])
		call_sign = nk->call_sign;

	printf ("Receiving: \"%s\" call sign: \"%s\" "
	        "CNI VPS: 0x%x 8/30-1: 0x%x 8/30-2: 0x%x\n",
		network_name,
		call_sign,
		nk->cni_vps,
		nk->cni_8301,
		nk->cni_8302);

	return TRUE; /* success */
}

static void
mainloop			(void)
{
	struct timeval timeout;
	vbi3_capture_buffer *sliced_buffer;
	unsigned int n_frames;

	/* Don't wait more than two seconds for the driver
	   to return data. */
	timeout.tv_sec = 2;
	timeout.tv_usec = 0;

	/* Should receive a CNI within two seconds,
	   call sign within ten seconds(?). */
	if (services & VBI3_SLICED_CAPTION_525)
		n_frames = 11 * 30;
	else
		n_frames = 3 * 25;

	for (; n_frames > 0; --n_frames) {
		unsigned int n_lines;
		int r;

		r = vbi3_capture_pull (cap,
				      /* raw_buffer */ NULL,
				      &sliced_buffer,
				      &timeout);
		switch (r) {
		case -1:
			fprintf (stderr, "VBI read error %d (%s)\n",
				 errno, strerror (errno));
			/* Could be ignored, esp. EIO with some drivers. */
			exit(EXIT_FAILURE);

		case 0: 
			fprintf (stderr, "VBI read timeout\n");
			exit(EXIT_FAILURE);

		case 1: /* success */
			break;

		default:
			assert (0);
		}

		n_lines = sliced_buffer->size / sizeof (vbi3_sliced);

		vbi3_decoder_feed (dec,
				   (vbi3_sliced *) sliced_buffer->data,
				   n_lines,
				   sliced_buffer->timestamp);

		if (quit)
			return;
	}

	printf ("No network ID received or network unknown.\n");
}

int
main				(void)
{
	char *errstr;
	vbi3_bool success;

	setlocale (LC_ALL, "");

	services = (VBI3_SLICED_TELETEXT_B |
		    VBI3_SLICED_VPS |
		    VBI3_SLICED_CAPTION_525);

	cap = vbi3_capture_v4l2k_new ("/dev/vbi",
				      /* fd */ -1,
				      /* buffers */ 5,
				      &services,
				      /* strict */ 0,
				      &errstr,
				      /* verbose */ FALSE);
	if (NULL == cap) {
		fprintf (stderr,
			 "Cannot capture VBI data with V4L2 interface:\n"
			 "%s\n",
			 errstr);

		free (errstr);
		errstr = NULL;

		exit (EXIT_FAILURE);
	}

	/* FIXME get videoset_set from capture device. */
	dec = vbi3_decoder_new (/* cache: allocate one */ NULL,
				/* network: current */ NULL,
				VBI3_VIDEOSTD_SET_625_50);
	assert (NULL != dec);

	success = vbi3_decoder_add_event_handler
		(dec, VBI3_EVENT_NETWORK, handler, /* user_data */ NULL);
	assert (success);

	mainloop ();

	vbi3_decoder_delete (dec);

	vbi3_capture_delete (cap);

	exit (EXIT_SUCCESS);
}
