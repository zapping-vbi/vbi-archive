/*
 *  Zapzilla/libvbi
 *
 *  Copyright (C) 2001 Michael H. Schimek
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

/* $Id: vbi.c,v 1.78 2001-12-05 07:25:00 mschimek Exp $ */

#include "site_def.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include "vt.h"
#include "vbi.h"
#include "hamm.h"
#include "lang.h"
#include "export.h"
#include "tables.h"
#include "libvbi.h"
#include "trigger.h"

#include "../common/fifo.h"
#include "../common/math.h"
#include "v4lx.h"

/*
 *  When event_mask == 0, remove previously added handler,
 *  otherwise add handler or change its event_mask.
 *
 *  The event_mask can be a combination of VBI_EVENT_*. When no handler
 *  exists for an event, decoding the respective data (eg.
 *  VBI_EVENT_PAGE, loading the cache with Teletext pages) is disabled.
 *  Mind the vbi fifo producer which isn't controlled from here.
 *
 *  Safe to call from a handler or thread other than mainloop.
 *  Returns boolean success.
 */
int
vbi_event_handler(struct vbi *vbi, int event_mask,
	void (* handler)(vbi_event *, void *), void *user_data) 
{
	struct event_handler *eh, **ehp;
	int found = 0, mask = 0, was_locked;
	int activate;

	/* If was_locked we're a handler, no recursion. */
	was_locked = pthread_mutex_trylock(&vbi->event_mutex);

	ehp = &vbi->handlers;

	while ((eh = *ehp)) {
		if (eh->handler == handler) {
			found = 1;

			if (!event_mask) {
				*ehp = eh->next;

				if (vbi->next_handler == eh)
					vbi->next_handler = eh->next;
						/* in event send loop */
				free(eh);

				continue;
			} else
				eh->event_mask = event_mask;
		}

		mask |= eh->event_mask;	
		ehp = &eh->next;
	}

	if (!found && event_mask) {
		if (!(eh = calloc(1, sizeof(*eh))))
			return 0;

		eh->event_mask = event_mask;
		mask |= event_mask;

		eh->handler = handler;
		eh->user_data = user_data;

		*ehp = eh;
	}

	activate = mask & ~vbi->event_mask;

	if (activate & VBI_EVENT_TTX_PAGE)
		vbi_teletext_channel_switched(vbi);
	if (activate & VBI_EVENT_CAPTION)
		vbi_caption_channel_switched(vbi);
	if (activate & VBI_EVENT_NETWORK)
		memset(&vbi->network, 0, sizeof(vbi->network));
	if (activate & VBI_EVENT_TRIGGER)
		vbi_trigger_flush(vbi);
	if (activate & (VBI_EVENT_ASPECT | VBI_EVENT_PROG_INFO)) {
		if (!(vbi->event_mask & (VBI_EVENT_ASPECT | VBI_EVENT_PROG_INFO))) {
			vbi_reset_prog_info(&vbi->prog_info[0]);
			vbi_reset_prog_info(&vbi->prog_info[1]);

			vbi->prog_info[1].future = TRUE;
			vbi->prog_info[0].future = FALSE;

			vbi->aspect_source = 0;
		}
	}

	vbi->event_mask = mask;

	if (!was_locked)
		pthread_mutex_unlock(&vbi->event_mutex);

	return 1;
}

static void
decode_wss_625(struct vbi *vbi, unsigned char *buf, double time)
{
	vbi_event e;
	vbi_aspect_ratio *r = &e.ev.aspect;
	int parity;

	memset(&e, 0, sizeof(e));

	/* Two producers... */
	if (time < vbi->wss_time)
		return;

	vbi->wss_time = time;

	if (buf[0] != vbi->wss_last[0]
	    || buf[1] != vbi->wss_last[1]) {
		vbi->wss_last[0] = buf[0];
		vbi->wss_last[1] = buf[1];
		vbi->wss_rep_ct = 0;
		return;
	}

	if (++vbi->wss_rep_ct < 3)
		return;

	parity = buf[0] & 15;
	parity ^= parity >> 2;
	parity ^= parity >> 1;

	if (!(parity & 1))
		return;

	r->ratio = 1.0;

	switch (buf[0] & 7) {
	case 0: /* 4:3 */
	case 6: /* 14:9 soft matte */
		r->first_line = 23;
		r->last_line = 310;
		break;
	case 1: /* 14:9 */
		r->first_line = 41;
		r->last_line = 292;
		break;
	case 2: /* 14:9 top */
		r->first_line = 23;
		r->last_line = 274;
		break;
	case 3: /* 16:9 */
	case 5: /* "Letterbox > 16:9" */
		r->first_line = 59; // 59.5 ?
		r->last_line = 273;
		break;
	case 4: /* 16:9 top */
		r->first_line = 23;
		r->last_line = 237;
		break;
	case 7: /* 16:9 anamorphic */
		r->first_line = 23;
		r->last_line = 310;
		r->ratio = 16.0 / 9.0;
		break;
	}

	r->film_mode = !!(buf[0] & 0x10);

	switch ((buf[1] >> 1) & 3) {
	case 0:
		r->open_subtitles = VBI_SUBT_NONE;
		break;
	case 1:
		r->open_subtitles = VBI_SUBT_ACTIVE;
		break;
	case 2:
		r->open_subtitles = VBI_SUBT_MATTE;
		break;
	case 3:
		r->open_subtitles = VBI_SUBT_UNKNOWN;
		break;
	}

	if (memcmp(r, &vbi->prog_info[0].aspect, sizeof(*r)) != 0) {
		vbi->prog_info[0].aspect = *r;
		vbi->aspect_source = 1;

		e.type = VBI_EVENT_ASPECT;
		vbi_send_event(vbi, &e);

		e.type = VBI_EVENT_PROG_INFO;
		e.ev.prog_info = &vbi->prog_info[0];
		vbi_send_event(vbi, &e);
	}

	if (0) {
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

		printf("WSS: %s; %s mode; %s colour coding;\n"
		       "      %s helper; reserved b7=%d; %s\n"
		       "      open subtitles: %s; %scopyright %s; copying %s\n",
		       formats[buf[0] & 7],
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
decode_wss_cpr1204(struct vbi *vbi, unsigned char *buf)
{
	int b0 = buf[0] & 0x80;
	int b1 = buf[0] & 0x40;
	vbi_event e;
	vbi_aspect_ratio *r = &e.ev.aspect;

	memset(&e, 0, sizeof(e));

	if (b1) {
		r->first_line = 72; // wild guess
		r->last_line = 212;
	} else {
		r->first_line = 22;
		r->last_line = 262;
	}

	r->ratio = b0 ? 16.0 / 9.0 : 1.0;
	r->film_mode = 0;
	r->open_subtitles = VBI_SUBT_UNKNOWN;

	if (memcmp(r, &vbi->prog_info[0].aspect, sizeof(*r)) != 0) {
		vbi->prog_info[0].aspect = *r;
		vbi->aspect_source = 2;

		e.type = VBI_EVENT_ASPECT;
		vbi_send_event(vbi, &e);

		e.type = VBI_EVENT_PROG_INFO;
		e.ev.prog_info = &vbi->prog_info[0];
		vbi_send_event(vbi, &e);
	}

	if (0)
		printf("CPR: %d %d\n", !!b0, !!b1);
}

#define SLICED_TELETEXT_B	(SLICED_TELETEXT_B_L10_625 | SLICED_TELETEXT_B_L25_625)
#define SLICED_CAPTION		(SLICED_CAPTION_625_F1 | SLICED_CAPTION_625 \
				 | SLICED_CAPTION_525_F1 | SLICED_CAPTION_525)

#include <sys/resource.h>

void
vbi_chsw_reset(struct vbi *vbi, nuid identified)
{
	nuid old_nuid = vbi->network.ev.network.nuid;

	if (0)
		fprintf(stderr, "*** chsw identified=%d old nuid=%d\n",
			identified, old_nuid);

	vbi_cache_flush(vbi);

	vbi_teletext_channel_switched(vbi);
	vbi_caption_channel_switched(vbi);

	if (identified == 0) {
		memset(&vbi->network, 0, sizeof(vbi->network));

		if (old_nuid != 0) {
			vbi->network.type = VBI_EVENT_NETWORK;
			vbi_send_event(vbi, &vbi->network);
		}
	} /* else already identified */

	vbi_trigger_flush(vbi); /* sic? */

	if (vbi->aspect_source > 0) {
		vbi_event e;

		e.ev.aspect.first_line = (vbi->aspect_source == 1) ? 23 : 22;
		e.ev.aspect.last_line =	(vbi->aspect_source == 1) ? 310 : 262;
		e.ev.aspect.ratio = 1.0;
		e.ev.aspect.film_mode = 0;
		e.ev.aspect.open_subtitles = VBI_SUBT_UNKNOWN;

		e.type = VBI_EVENT_ASPECT;
		vbi_send_event(vbi, &e);
	}

	vbi_reset_prog_info(&vbi->prog_info[0]);
	vbi_reset_prog_info(&vbi->prog_info[1]);

	vbi->prog_info[1].future = TRUE;
	vbi->prog_info[0].future = FALSE;

	vbi->aspect_source = 0;

	vbi->wss_last[0] = 0;
	vbi->wss_last[1] = 0;
	vbi->wss_rep_ct = 0;
	vbi->wss_time = 0.0;

	vbi->vt.header_page.pgno = 0;

	pthread_mutex_lock(&vbi->chswcd_mutex);

	vbi->chswcd = 0;

	pthread_mutex_unlock(&vbi->chswcd_mutex);
}

static void *
mainloop(void *p)
{
	struct vbi *vbi = p;
	vbi_event ev;
	vbi_sliced *s;
	consumer c;
	int items;

	assert(add_consumer(vbi->source, &c));

	setpriority(PRIO_PROCESS, 0, 5);

	while (!vbi->quit) {
		buffer *b;
		double d;
		struct timeval now;
		struct timespec timeout;

		gettimeofday(&now, NULL);
		timeout.tv_sec = now.tv_sec + 1;
		timeout.tv_nsec = now.tv_usec * 1000;

		if (!(b = wait_full_buffer_timeout(&c, &timeout)))
			continue;
		
		d = b->time - vbi->time;

		if (b->used <= 0) {
			/* ack */
			send_empty_buffer(&c, b);

			/* EOF? Well, shouldn't happen */
			ev.type = VBI_EVENT_IO_ERROR;
			vbi_send_event(vbi, &ev);

			break;
		}

		if (vbi->time > 0
		    && (d < 0.025 || d > 0.050)) {
			/*
			 *  Since (dropped >= channel switch) we give
			 *  ~1.5 s to refute, then assume a switch.
			 */
			pthread_mutex_lock(&vbi->chswcd_mutex);

			if (vbi->chswcd == 0)
				vbi->chswcd = 40;

			pthread_mutex_unlock(&vbi->chswcd_mutex);

			if (0)
				fprintf(stderr, "vbi frame/s dropped at %f, D=%f\n",
					b->time, b->time - vbi->time);

			if (vbi->event_mask &
			    (VBI_EVENT_TTX_PAGE | VBI_EVENT_NETWORK))
				vbi_teletext_desync(vbi);
			if (vbi->event_mask &
			    (VBI_EVENT_CAPTION | VBI_EVENT_NETWORK))
				vbi_caption_desync(vbi);
		} else {
			pthread_mutex_lock(&vbi->chswcd_mutex);

			if (vbi->chswcd > 0 && --vbi->chswcd == 0) {
				pthread_mutex_unlock(&vbi->chswcd_mutex);
				vbi_chsw_reset(vbi, 0);
			} else
				pthread_mutex_unlock(&vbi->chswcd_mutex);
		}

		if (b->time > vbi->time)
			vbi->time = b->time;

		s = (vbi_sliced *) b->data;
		items = b->used / sizeof(vbi_sliced);

		while (items) {
			if (s->id & SLICED_TELETEXT_B)
				vbi_teletext_packet(vbi, s->data);
			else if (s->id & SLICED_CAPTION)
				vbi_caption_dispatcher(vbi, s->line, s->data);
			else if (s->id & SLICED_VPS)
				vbi_vps(vbi, s->data);
			else if (s->id & SLICED_WSS_625)
				decode_wss_625(vbi, s->data, b->time);
			else if (s->id & SLICED_WSS_CPR1204)
				decode_wss_cpr1204(vbi, s->data);

			s++;
			items--;
		}

		send_empty_buffer(&c, b);

		if (vbi->event_mask & VBI_EVENT_TRIGGER)
			vbi_deferred_trigger(vbi);

		if (0 && (rand() % 511) == 0)
			vbi_eacem_trigger(vbi,
				"<http://zapping.sourceforge.net>[n:Zapping][5450]");
	}

	ev.type = VBI_EVENT_CLOSE;
	vbi_send_event(vbi, &ev);

	rem_consumer(&c);

	return NULL;
}

/**
 * vbi_channel_switched:
 * @vbi: VBI decoding context
 * @nuid: Set to zero until further
 * 
 * Call this after switching away from the channel (that is RF
 * channel, baseband video line etc, precisely: the network) from
 * which this context is receiving vbi data, to reset the context
 * accordingly. The decoder attempts to detect channel switches
 * automatically, but this is not 100 % reliable esp. without
 * receiving and decoding Teletext or VPS. In the past only
 * (re-)opening would reset the context. 
 *
 * Note the reset request is not executed until the next frame
 * is about to be decoded, so you can still receive "old" events
 * after calling this.
 *
 * Side effects: A reset deletes all Teletext pages previously
 * stored in the cache [XXX change to "may delete", require
 * nuid to access cached pages]. You may receive a
 * VBI_EVENT_RATIO and VBI_EVENT_NETWORK revoking a previous
 * event of the same kind. Note the possibility of sending a
 * blank vbi_network to notify the event handler of the
 * (autodetected) switch and the (temporary) inability to
 * identify the new network.
 **/
void
vbi_channel_switched(struct vbi *vbi, nuid nuid)
{
	/* XXX nuid */

	pthread_mutex_lock(&vbi->chswcd_mutex);

	vbi->chswcd = 1;

	pthread_mutex_unlock(&vbi->chswcd_mutex);
}

/* TEST ONLY */

#ifndef FILTER_REM
#define FILTER_REM 0
#endif
#ifndef FILTER_ADD
#define FILTER_ADD 0
#endif
#ifndef SAMPLE
#define SAMPLE ""
#endif

/*
 * Examples, put the #defines into site_def.h
 *
 * #define FILTER_REM 0
 * #define FILTER_ADD SLICED_CAPTION
 * #define SAMPLE "libvbi/samples/s4"
 *
 * #define FILTER_REM SLICED_TELETEXT_B
 * #define FILTER_ADD SLICED_TELETEXT_B
 * #define SAMPLE "libvbi/samples/t1-swr"
 *
 * #define FILTER_* (SLICED_TELETEXT_B | SLICED_CAPTION | SLICED_VPS)
 */

struct {
	fifo			fifo;
	producer		producer;
	buffer			buf;

	fifo *			old_fifo;
	consumer		old_consumer;

	vbi_sliced		sliced[60];
	int			pass, add;

	FILE *			fp;
} filter;

static void
wait_full_filter(fifo *f)
{
	int items, index, line;
	vbi_sliced *s, *d;
	char buf[256];
	buffer *b;
	int i;

	b = wait_full_buffer(&filter.old_consumer);

	wait_empty_buffer(&filter.producer);

	if (b->used <= 0) {
		filter.buf.time = b->time;
		filter.buf.used = b->used;
		filter.buf.error = -1;
		filter.buf.errorstr = NULL;

		send_empty_buffer(&filter.old_consumer, b);
		send_full_buffer(&filter.producer, &filter.buf);

		return;
	}

	s = (vbi_sliced *) b->data;
	d = filter.sliced;
	items = b->used / sizeof(vbi_sliced);

	for (; items > 0; s++, items--)
		if (s->id & filter.pass) {
			memcpy(d, s, sizeof(vbi_sliced));
			d++;
		}

	while (feof(filter.fp) || !fgets(buf, 255, filter.fp)) {
		rewind(filter.fp);
		printf("Rewind sample stream\n");
	}

//	dt = strtod(buf, NULL);

	items = fgetc(filter.fp);

//	printf("%8.6f %d:\n", dt, items);

	for (i = 0; i < items; i++) {
		index = fgetc(filter.fp);
		line = fgetc(filter.fp);
		line += 256 * fgetc(filter.fp);

		if (index == 0) {
			d->id = SLICED_TELETEXT_B;
			d->line = line & 0xFFF;
			fread(d->data, 1, 42, filter.fp);
		} else if (index == 7) {
			d->id = SLICED_CAPTION;
			d->line = line & 0xFFF;
			fread(d->data, 1, 2, filter.fp);
		} else {
			printf("Oops! Unknown data in sample file %s\n", SAMPLE);
			exit(EXIT_FAILURE);
		}

		if (d->id & filter.add)
			d++;
	}

	filter.buf.data = (void *) filter.sliced;
	filter.buf.used = (d - filter.sliced) * sizeof(vbi_sliced);
	filter.buf.time = b->time;

	send_empty_buffer(&filter.old_consumer, b);
	send_full_buffer(&filter.producer, &filter.buf);
}

static void
add_filter(struct vbi *vbi)
{
	if (!(FILTER_REM) && !(FILTER_ADD))
		return;

	if (!(filter.fp = fopen(SAMPLE, "r"))) {
		printf("Cannot open %s: %s\n", SAMPLE, strerror(errno));
		return;
	}

	filter.pass = FILTER_REM ^ -1;
	filter.add = FILTER_ADD;

	filter.old_fifo = vbi->source;

	assert(add_consumer(filter.old_fifo, &filter.old_consumer));

	init_callback_fifo(&filter.fifo, "vbi-filter",
		NULL, NULL, wait_full_filter, NULL, 0, 0);

	add_buffer(&filter.fifo, &filter.buf);

	assert(add_producer(&filter.fifo, &filter.producer));

	vbi->source = &filter.fifo;
}

static void
remove_filter(struct vbi *vbi)
{
	if (!filter.old_fifo)
		return;

	rem_consumer(&filter.old_consumer);

	vbi->source = filter.old_fifo;

	filter.old_fifo = NULL;

	destroy_fifo(&filter.fifo);

	fclose(filter.fp);
}

static inline int
transp(int val, int brig, int cont)
{
	int r = (((val - 128) * cont) / 64) + brig;

	return saturate(r, 0, 255);
}

void
vbi_transp_colourmap(struct vbi *vbi, attr_rgba *d, attr_rgba *s, int entries)
{
	int brig, cont;
	attr_rgba colour;

	brig = saturate(vbi->brightness, 0, 255);
	cont = saturate(vbi->contrast, -128, +127);

	while (entries--) {
		colour  = transp(((*s >> 0) & 0xFF), brig, cont) << 0;
		colour |= transp(((*s >> 8) & 0xFF), brig, cont) << 8;
		colour |= transp(((*s >> 16) & 0xFF), brig, cont) << 16;
		colour |= *s & (0xFFUL << 24);
		*d++ = colour;
		s++;
	}
}

/*
 *  Brightness: 0 ... 255, default 128
 *  Contrast: -128 ... +128, default 64
 *  (maximum contrast at +/-128, zero at 0, inversion below 0)
 */
void
vbi_set_colour_level(struct vbi *vbi, int brightness, int contrast)
{
	vbi->brightness = brightness;
	vbi->contrast = contrast;

	vbi_caption_colour_level(vbi);
}

int
vbi_classify_page(struct vbi *vbi, int pgno, int *subpage, char **language)
{
	struct page_info *pi;
	int code, subc;
	char *lang;

	if (!subpage)
		subpage = &subc;
	if (!language)
		language = &lang;

	*subpage = 0;
	*language = NULL;

	if (pgno < 1) {
		return VBI_UNKNOWN_PAGE;
	} else if (pgno <= 8) {
		if ((current_time() - vbi->cc.channel[pgno - 1].time) > 20)
			return VBI_NO_PAGE;

		*language = vbi->cc.channel[pgno - 1].language;

		return (pgno <= 4) ? VBI_SUBTITLE_PAGE : VBI_NORMAL_PAGE;
	} else if (pgno < 0x100 || pgno > 0x8FF) {
		return VBI_UNKNOWN_PAGE;
	}

	pi = vbi->vt.page_info + pgno - 0x100;
	code = pi->code;

	if (code != VBI_UNKNOWN_PAGE) {
		if (code == VBI_SUBTITLE_PAGE) {
			if (pi->language != 0xFF)
				*language = font_descriptors[pi->language].label;
		} else if (code == VBI_TOP_BLOCK || code == VBI_TOP_GROUP)
			code = VBI_NORMAL_PAGE;
		else if (code == VBI_NOT_PUBLIC || code > 0xE0)
			return VBI_UNKNOWN_PAGE;

		*subpage = pi->subcode;

		return code;
	}

	if ((pgno & 0xFF) <= 0x99) {
		*subpage = 0xFFFF;
		return VBI_NORMAL_PAGE; /* wild guess */
	}

	return VBI_UNKNOWN_PAGE;
}

void
vbi_reset_prog_info(vbi_program_info *pi)
{
	int i;

	/* PID */
	pi->month = -1;
	pi->day = -1;
	pi->hour = -1;
	pi->min = -1;
	pi->tape_delayed = 0;
	/* PL */
	pi->length_hour = -1;
	pi->length_min = -1;
	pi->elapsed_hour = -1;
	pi->elapsed_min = -1;
	pi->elapsed_sec = -1;
	/* PN */
	pi->title[0] = 0;
	/* PT */
	pi->type_classf = VBI_PROG_CLASSF_NONE;
	/* PR */
	pi->rating_auth = VBI_RATING_AUTH_NONE;
	/* PAS */
	pi->audio[0].mode = VBI_AUDIO_MODE_UNKNOWN;
	pi->audio[0].language = NULL;
	pi->audio[1].mode = VBI_AUDIO_MODE_UNKNOWN;
	pi->audio[1].language = NULL;
	/* PCS */
	pi->caption_services = -1;
	for (i = 0; i < 8; i++)
		pi->caption_language[i] = NULL;
	/* CGMS */
	pi->cgms_a = -1;
	/* AR */
	pi->aspect.first_line = -1;
	pi->aspect.last_line = -1;
	pi->aspect.ratio = 0.0;
	pi->aspect.film_mode = 0;
	pi->aspect.open_subtitles = VBI_SUBT_UNKNOWN;
	/* PD */
	for (i = 0; i < 8; i++)
		pi->description[i][0] = 0;
}

/*
 *  Width in pixels, assuming the width corresponds to
 *  52 usec of PAL video data. Enter capture widths, not
 *  scaled widths (XvVideo/Image or scaled streaming). 
 *  Height assumed >= 1, we examine only the first line.
 *  Time in TOD sec fractions as usual.
 */
void
vbi_push_video(struct vbi *vbi, void *video_data,
	int width, enum tveng_frame_pixformat fmt,
	double time)
{
	static const int std_widths[] = {
		768, 704, 640, 384, 352, 320, 192, 176, 160, -160
	};
	int sampling_rate, spl;
	vbi_sliced *s;
	buffer *b;
	int i;

	if (fmt != vbi->video_fmt || width != vbi->video_width) {
		for (i = 0; width < ((std_widths[i] + std_widths[i + 1]) >> 1); i++)
			;

		spl = std_widths[i]; /* samples in 52 usec */
		sampling_rate = spl / 52.148e-6;

		vbi->wss_slicer_fn =
			vbi_bit_slicer_init(&vbi->wss_slicer, 
				width,
				sampling_rate, 
				/* cri_rate */ 5000000, 
				/* bit_rate */ 833333,
				/* cri_frc */ 0xC71E3C1F, 
				/* cri_mask */ 0x924C99CE,
				/* cri_bits */ 32, 
				/* frc_bits */ 0, 
				/* payload */ 14 * 1,
				MOD_BIPHASE_LSB_ENDIAN,
				fmt);

		vbi->video_fmt = fmt;
		vbi->video_width = width;		
	}

	if ((time - vbi->video_time) < 1.0 / 4.0)
		return; /* we can spend the time better elsewhere... */

	/*
	 *  We should always have enough free buffers except something
	 *  blocks the vbi parser, eg. an event handler. In case
	 *  *we* are actually the rogue thread, don't attempt to wait,
	 *  the WSS datagram will repeat anyway.
	 */
	if (!(b = recv_empty_buffer(&vbi->wss_producer)))
		return;

	s = (void *) b->data = b->allocated;

	if (vbi->wss_slicer_fn(&vbi->wss_slicer,
		video_data, (unsigned char *) &s->data)) {
		s->id = SLICED_WSS_625;
		s->line = 23;
		b->time = time; /* important to serialize
				    (the consumer has to, fifo ignores time) */
		b->used = sizeof(vbi_sliced);

		send_full_buffer(&vbi->wss_producer, b);
	} else
		unget_empty_buffer(&vbi->wss_producer, b);
}

void
vbi_close(struct vbi *vbi)
{
	vbi->quit = 1;

	pthread_join(vbi->mainloop_thread_id, NULL);

	vbi_trigger_flush(vbi);

	vbi_caption_destroy(vbi);

	pthread_mutex_destroy(&vbi->event_mutex);
	pthread_mutex_destroy(&vbi->chswcd_mutex);

	rem_producer(&vbi->wss_producer);

	remove_filter(vbi);

	vbi_cache_destroy(vbi);

	free(vbi);
}

/*
 *  Note1 this will fork off a vbi thread decoding vbi data
 *  and calling all event handlers. To join this thread and
 *  free all memory associated with libvbi you must call
 *  vbi_close. Libvbi should be reentrant, so you can create
 *  any number of decoding instances (even with the same
 *  source fifo).
 *
 *  Note2 fifo != producer; You can add, remove, replace
 *  a sliced vbi data producer before or after vbi_open and
 *  after vbi_close.
 */
struct vbi *
vbi_open(fifo *source)
{
	struct vbi *vbi;

	if (!(vbi = calloc(1, sizeof(*vbi))))
		return NULL;

	vbi_cache_init(vbi);


	/* Our sliced VBI data source */

	vbi->source = source;

	add_filter(vbi);


	/*
	 *  Second (internal) source, the RGB WSS decoder.
	 *  NB this won't succeed when the filter is active.
	 */	
	add_producer(vbi->source, &vbi->wss_producer);

	vbi->video_fmt = -1;
	vbi->video_width = -1;		
	vbi->video_time = 0.0;


	/* Initialize the decoder */

	pthread_mutex_init(&vbi->chswcd_mutex, NULL);
	pthread_mutex_init(&vbi->event_mutex, NULL);

	vbi->time = 0.0;

	vbi->brightness	= 128;
	vbi->contrast	= 64;

	vbi_teletext_init(vbi);

	vbi_teletext_set_level(vbi, VBI_LEVEL_2p5);

	vbi_caption_init(vbi);


	/* Here we go */

	if (pthread_create(&vbi->mainloop_thread_id, NULL, mainloop, vbi)) {
		/* Or maybe not */
		vbi_caption_destroy(vbi);
		pthread_mutex_destroy(&vbi->event_mutex);
		pthread_mutex_destroy(&vbi->chswcd_mutex);
		rem_producer(&vbi->wss_producer);
		remove_filter(vbi);
		vbi_cache_destroy(vbi);
		free(vbi);

		return NULL;
	}

#ifdef LIBVBI_IDL_ALERT
	fprintf(stderr, "IDL sniffer on\n");
#endif
	return vbi;
}
