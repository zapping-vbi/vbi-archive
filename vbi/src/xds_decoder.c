/*
 *  libzvbi - Extended Data Service decoder
 *
 *  Copyright (C) 2000-2004 Michael H. Schimek
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

/* $Id: xds_decoder.c,v 1.1.2.7 2006-05-14 14:14:12 mschimek Exp $ */

#if 0 // TODO

#include "../site_def.h"
#include "../config.h"

#include <stdio.h>
#include <stdlib.h>

#include "vbi.h"
#include "vbi3_decoder-priv.h"

#include "hamm.h"
#include "xds_decoder.h"
#include "misc.h"

/**
 * @addtogroup XDSDecoder Extended Data Service Decoder
 * @ingroup LowDec
 * @brief Functions to decode XDS packets (EIA 608).
 */

#ifndef XDS_DECODER_LOG
#define XDS_DECODER_LOG 0
#endif

#define log(format, args...)						\
do {									\
	if (XDS_DECODER_LOG)						\
		fprintf (stderr, format , ##args);			\
} while (0)

static inline void
caption_send_event(vbi3_decoder *vbi, vbi3_event *ev)
{
	/* Permits calling vbi3_fetch_cc_page from handler */
#warning todo
  //	pthread_mutex_unlock(&vbi->cc.mutex);

#warning obsolete
//	vbi3_send_event(vbi, ev);

//	pthread_mutex_lock(&vbi->cc.mutex);
}

#define XDS_DEBUG(x)

/* vbi3_classify_page, program_info language */
static const char *
language[8] = {
	NULL,
	"en",	/* English */
	"es",	/* Español */
	"fr",	/* Français */
	"de",	/* Deutsch */
	"it",	/* Italiano */
	NULL,	/* "Other" */
	NULL	/* "None" */
};

#ifdef __GNUC__
#define UNUSED __attribute__ ((unused))
#else
#define UNUSED
#endif

static const char *
map_type[] UNUSED = {
	"unknown", "mono", "simulated stereo", "stereo",
	"stereo surround", "data service", "unknown", "none"
};

static const char *
sap_type[] UNUSED = {
	"unknown", "mono", "video descriptions", "non-program audio",
	"special effects", "data service", "unknown", "none"
};

static const char *
cgmsa[] UNUSED = {
	"copying permitted",
	"-",
	"one generation copy allowed",
	"no copying permitted"
};

static const char *
scrambling[] UNUSED = {
	"no pseudo-sync pulse",
	"pseudo-sync pulse on; color striping off",
	"pseudo-sync pulse on; 2-line color striping on",
	"pseudo-sync pulse on; 4-line color striping on"
};

static const char *
month_names[] UNUSED = {
	"0?", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug",
	"Sep", "Oct", "Nov", "Dec", "13?", "14?", "15?"
};

static const char *
day_names[] UNUSED = {
	"0?", "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

static uint32_t hcrc[128];

static void init_hcrc(void) __attribute__ ((constructor));


/*
http://www.fcc.gov/cgb/statid.html
 *
 *  XDS has no unique station id as EBU (or is the call sign?)
 *  so we create a checksum over the station name.
 */
static void
init_hcrc(void)
{
	unsigned int sum;
	int i, j;

	for (i = 0; i < 128; i++) {
		sum = 0;
		for (j = 7 - 1; j >= 0; j--)
			if (i & (1 << j))
				sum ^= 0x48000000L >> j;
		hcrc[i] = sum;
	}
}

static int
xds_strfu(char *d, const char *s, int len)
{
	int c, neq = 0;

	for (; len > 0 && *s <= 0x20; s++, len--);

	for (; len > 0; s++, len--) {
		c = MAX((char) 0x20, *s);
		neq |= *d ^ c;
		*d++ = c;
	}

	neq |= *d;
	*d = 0;

	return neq;
}

#define xds_intfu(d, val) (neq |= d ^ (val), d = (val)) 

static void
flush_prog_info(vbi3_decoder *vbi, vbi3_program_info *pi, vbi3_event *e)
{
#warning
  //	e->ev.aspect = pi->aspect;

	vbi3_reset_prog_info(pi);

	if (memcmp(&e->ev.aspect, &pi->aspect, sizeof(pi->aspect)) != 0) {
		e->type = VBI3_EVENT_ASPECT;
		caption_send_event(vbi, e);
	}

	vbi->cc.info_cycle[pi->future] = 0;
}

vbi3_bool
decode_program_id		(vbi3_program_id *	pid,
				 const vbi3_xds_packet *	xp)
{
	unsigned int minute;
	unsigned int hour;
	unsigned int day;
	unsigned int month;
	vbi3_pil pil;

	minute	= xp->buffer[0] & 63;
	hour	= xp->buffer[1] & 31;
	day	= xp->buffer[2] & 31;
	month	= xp->buffer[3] & 15;

	pil = VBI3_PIL (month, day, hour, minute);

	if (VBI3_PIL_END != pil) {
		/* XXX check date. */
	}

	CLEAR (*pid);

	if (VBI3_XDS_CLASS_CURRENT == xp->xds_class)
		pid->channel = VBI3_PID_CHANNEL_XDS_CURRENT;
	else
		pid->channel = VBI3_PID_CHANNEL_XDS_FUTURE;

	pid->month	= month;
	pid->day	= day;
	pid->hour	= hour;
	pid->minute	= minute;

	pid->pil	= pil;

	pid->tape_delayed = ((xp->buffer[3] & 0x10) > 0);

	return TRUE;
}

/**
 * @param pid The program ID will be stored here (month, day, hour, minute
 *   and tape_delay). Additionally the date will be encoded in the pil field,
 *   and as seconds since 1970-01-01 00:00 in the time field. All dates are
 *   given relative to UTC.
 * @param xp XDS packet to decode.
 *
 * Decodes an XDS Program ID (0x0101, 0x0301) packet, returning the scheduled
 * start time of the current or a future program relative to UTC.
 *
 * When this is a current class packet and the date differs from a previous
 * date, a new program has started. When pil is VBI3_PIL_END the current program
 * has ended.
 *
 * @returns
 * FALSE if the packet contains incorrect data. In this case @a pid remains
 * unmodified.
 */
vbi3_bool
vbi3_decode_xds_program_id	(vbi3_program_id *	pid,
				 const vbi3_xds_packet *	xp)
{
	assert (NULL != pid);
	assert (NULL != xp);

	if (4 != xp->buffer_size)
		return FALSE;

	return decode_program_id (pid, xp);
}

/**
 * @param ar Aspect ratio information is stored here.
 * @param xp XDS packet to decode.
 *
 * Decodes an XDS Aspect Ratio (0x0109, 0x0309) packet.
 *
 * @returns
 * FALSE if the packet contains incorrect data. In this case @a ar remains
 * unmodified.
 */
vbi3_bool
vbi3_decode_xds_aspect_ratio	(vbi3_aspect_ratio *	ar,
				 const vbi3_xds_packet *	xp)
{
	assert (NULL != ar);
	assert (NULL != xp);

	if (2 != xp->buffer_size
	    && 4 != xp->buffer_size)
		return FALSE;

	CLEAR (*ar);

	ar->start[0] = 22 + (xp->buffer[0] & 63);
	ar->start[1] = 263 + 22 + (xp->buffer[0] & 63);
	ar->count[0] = 263 - (xp->buffer[1] & 63) - ar->start[1];
	ar->count[1] = ar->count[0];

	ar->ratio = 1.0;
	if (xp->buffer_size >= 3 && (xp->buffer[2] & 1))
		ar->ratio = 3.0 / 4.0;

	return TRUE;
}

static void
decode_program			(vbi3_decoder *		vbi,
				 const vbi3_xds_packet *	xp)
{
	vbi3_program_info *pi;
	vbi3_event e;
	int neq, i;

	if (!(vbi->handlers.event_mask
	      & (VBI3_EVENT_ASPECT | VBI3_EVENT_PROG_INFO)))
		return;

	pi = &vbi->prog_info[xp->xds_class];
	neq = 0;

	switch (xp->xds_subclass) {
	case VBI3_XDS_PROGRAM_ID:
	{
		int month, day, hour, min;

		if (xp->buffer_size != 4)
			return;

		month = xp->buffer[3] & 15;
		day = xp->buffer[2] & 31;
		hour = xp->buffer[1] & 31;
		min = xp->buffer[0] & 63;

		XDS_DEBUG(printf("PIN: %d %s %02d:%02d UTC, "
				 "D=%d L=%d Z=%d T(ape delayed)=%d\n",
				 day, month_names[month], hour, min,
				 !!(buffer[1] & 0x20),
				 !!(buffer[2] & 0x20),
				 !!(buffer[3] & 0x20),
				 !!(buffer[3] & 0x10)));

		if (month == 0 || month > 12
		    || day == 0 || day > 31
		    || hour > 23 || min > 59)
			return;

		month--;
		day--;

		neq = (pi->month ^ month) | (pi->day ^ day)
			| (pi->hour ^ hour) | (pi->min ^ min);

		pi->tape_delayed = !!(xp->buffer[3] & 0x10);

		if (neq) {
			flush_prog_info(vbi, pi, &e);

			pi->month = month;
			pi->day = day;
			pi->hour = hour;
			pi->min = min;

			pi->tape_delayed = !!(xp->buffer[3] & 0x10);
		}

		break;
	}

	case VBI3_XDS_PROGRAM_LENGTH:
	{
		int lhour, lmin, ehour = -1, emin = -1, esec = 0;

		if (xp->buffer_size < 2 || xp->buffer_size > 6)
			return;

		lhour = xp->buffer[1] & 63;
		lmin = xp->buffer[0] & 63;

		if (xp->buffer_size >= 3) {
			ehour = xp->buffer[3] & 63;
			emin = xp->buffer[2] & 63;

			if (xp->buffer_size >= 5)
				esec = xp->buffer[4] & 63;
		}

		XDS_DEBUG(
			printf("length: %02d:%02d, ", lhour, lmin);
			printf("elapsed: %02d:%02d", ehour, emin);
			if (xp->buffer_size >= 5)
			printf(":%02d", esec);
			printf("\n");
			)

			if (lmin > 59 || emin > 59 || esec > 59)
				return;

		xds_intfu(pi->length_hour, lhour);
		xds_intfu(pi->length_min, lmin);
		xds_intfu(pi->elapsed_hour, ehour);
		xds_intfu(pi->elapsed_min, emin);
		xds_intfu(pi->elapsed_sec, esec);

		break;
	}

	case VBI3_XDS_PROGRAM_NAME:
		if (xp->buffer_size < 2)
			return;

		XDS_DEBUG(
			printf("program title: '");
			for (i = 0; i < buffer_size; i++)
			putchar(vbi3_printable (buffer[i]));
			printf("'\n");
			)

			neq = xds_strfu(pi->title,
					xp->buffer, xp->buffer_size);

		if (!neq) { /* no title change */
			if (!(vbi->cc.info_cycle[xp->xds_class] & (1 << 3)))
				break; /* already reported */

			if (!(vbi->cc.info_cycle[xp->xds_class] & (1 << 1))) {
				/* Second occurence without PIN */

				flush_prog_info(vbi, pi, &e);

				xds_strfu(pi->title, xp->buffer,
					  xp->buffer_size);
				vbi->cc.info_cycle[xp->xds_class] |= 1 << 3;
			}
		}

		break;

	case VBI3_XDS_PROGRAM_TYPE:
	{
		int neq;

		XDS_DEBUG(
			printf("program type: ");
			for (i = 0; i < buffer_size; i++)
			printf((i > 0) ? ", %s" : "%s",
			       vbi3_prog_type_str_by_id(
				       VBI3_PROG_CLASSF_EIA_608, buffer[i]));
			printf("\n");
			)

			neq = (pi->type_classf != VBI3_PROG_CLASSF_EIA_608);
		pi->type_classf = VBI3_PROG_CLASSF_EIA_608;

		for (i = 0; i < xp->buffer_size; i++) {
			neq |= pi->type_id[i] ^ xp->buffer[i];
			pi->type_id[i] = xp->buffer[i];
		}

		neq |= pi->type_id[i];
		pi->type_id[i] = 0;

		break;
	}

	case VBI3_XDS_PROGRAM_RATING:
	{
		vbi3_rating_auth auth;
		int r, g, dlsv = 0;

		if (xp->buffer_size != 2)
			return;

		r = xp->buffer[0] & 7;
		g = xp->buffer[1] & 7;

		if (xp->buffer[0] & 0x20)
			dlsv |= VBI3_RATING_D;
		if (xp->buffer[1] & 0x08)
			dlsv |= VBI3_RATING_L;
		if (xp->buffer[1] & 0x10)
			dlsv |= VBI3_RATING_S;
		if (xp->buffer[1] & 0x20)
			dlsv |= VBI3_RATING_V;

		XDS_DEBUG(
			printf("program movie rating: %s, tv rating: ",
			       vbi3_rating_str_by_id(VBI3_RATING_AUTH_MPAA, r));
			if (xp->buffer[0] & 0x10) {
				if (xp->buffer[0] & 0x20)
					puts(vbi3_rating_str_by_id(VBI3_RATING_AUTH_TV_CA_FR, g));
				else
					puts(vbi3_rating_str_by_id(VBI3_RATING_AUTH_TV_CA_EN, g));
			} else {
				printf("%s; ", vbi3_rating_str_by_id(VBI3_RATING_AUTH_TV_US, g));
				if (xp->buffer[1] & 0x20)
					printf("violence; ");
				if (xp->buffer[1] & 0x10)
					printf("sexual situations; ");
				if (xp->buffer[1] & 8)
					printf("indecent language; ");
				if (xp->buffer[0] & 0x20)
					printf("sexually suggestive dialog");
				putchar('\n');
			}
			)

			if ((xp->buffer[0] & 0x08) == 0) {
				if (r == 0) return;
				auth = VBI3_RATING_AUTH_MPAA;
				pi->rating_dlsv = dlsv = 0;
			} else if ((xp->buffer[0] & 0x10) == 0) {
				auth = VBI3_RATING_AUTH_TV_US;
				r = g;
			} else if ((xp->buffer[1] & 0x08) == 0) {
				if ((xp->buffer[0] & 0x20) == 0) {
					if ((r = g) > 6) return;
					auth = VBI3_RATING_AUTH_TV_CA_EN;
				} else {
					if ((r = g) > 5) return;
					auth = VBI3_RATING_AUTH_TV_CA_FR;
				}
				pi->rating_dlsv = dlsv = 0;
			} else
				return;

		if ((neq = (pi->rating_auth != auth
			    || pi->rating_id != r
			    || pi->rating_dlsv != dlsv))) {
			pi->rating_auth = auth;
			pi->rating_id = r;
			pi->rating_dlsv = dlsv;
		}

		break;
	}

	case VBI3_XDS_PROGRAM_AUDIO_SERVICES:
	{
		static const vbi3_audio_mode mode[2][8] = {
			{
				VBI3_AUDIO_MODE_UNKNOWN,
				VBI3_AUDIO_MODE_MONO,
				VBI3_AUDIO_MODE_SIMULATED_STEREO,
				VBI3_AUDIO_MODE_STEREO,
				VBI3_AUDIO_MODE_STEREO_SURROUND,
				VBI3_AUDIO_MODE_DATA_SERVICE,
				VBI3_AUDIO_MODE_UNKNOWN, /* "other" */
				VBI3_AUDIO_MODE_NONE
			}, {
				VBI3_AUDIO_MODE_UNKNOWN,
				VBI3_AUDIO_MODE_MONO,
				VBI3_AUDIO_MODE_VIDEO_DESCRIPTIONS,
				VBI3_AUDIO_MODE_NON_PROGRAM_AUDIO,
				VBI3_AUDIO_MODE_SPECIAL_EFFECTS,
				VBI3_AUDIO_MODE_DATA_SERVICE,
				VBI3_AUDIO_MODE_UNKNOWN, /* "other" */
				VBI3_AUDIO_MODE_NONE
			}
		};

		if (xp->buffer_size != 2)
			return;

		XDS_DEBUG(printf("main audio: %s, %s; "
				 "second audio program: %s, %s\n",
				 map_type[xp->buffer[0] & 7], language[(xp->buffer[0] >> 3) & 7],
				 sap_type[xp->buffer[1] & 7], language[(xp->buffer[1] >> 3) & 7]));

		for (i = 0; i < 2; i++) {
			int l = (xp->buffer[i] >> 3) & 7;
			vbi3_audio_mode m = mode[i][xp->buffer[i] & 7];
			const char *s = ((1 << l) & 0xC1) ? NULL : language[l];

			if (pi->audio[i].mode != m) {
				neq = 1; pi->audio[i].mode = m;
			}
			if (pi->audio[i].lang_code != s) {
				neq = 1; pi->audio[i].lang_code = s;
			}
		}

		break;
	}

	case VBI3_XDS_PROGRAM_CAPTION_SERVICES:
	{
		int services = 0;

		if (xp->buffer_size > 8)
			return;

		for (i = 0; i < 8; i++)
			pi->caption_lang_code[i] = NULL;

		for (i = 0; i < xp->buffer_size; i++) {
			int ch = xp->buffer[i] & 7;
			int l = (xp->buffer[i] >> 3) & 7;
			const char *s;

			ch = (ch & 1) * 4 + (ch >> 1);

			services |= 1 << ch;
			s = ((1 << l) & 0xC1) ? NULL : language[l];

			if (pi->caption_lang_code[ch] != s) {
				neq = 1; pi->caption_lang_code[ch] = s;
			}

			if (xp->xds_class == VBI3_XDS_CLASS_CURRENT)
				vbi->cc.channel[ch].lang_code[0] =
					pi->caption_lang_code[ch];
		}

		xds_intfu(pi->caption_services, services);

		XDS_DEBUG(
			printf("program caption services:\n");
			for (i = 0; i < xp->buffer_size; i++)
			printf("Line %3d, channel %d, %s: %s\n",
			       (xp->buffer[i] & 4) ? 284 : 21,
			       (xp->buffer[i] & 2) ? 2 : 1,
			       (xp->buffer[i] & 1) ? "text      " : "captioning",
			       language[(xp->buffer[i] >> 3) & 7]);
			)

			break;
	}

	case VBI3_XDS_PROGRAM_CGMS:
		if (xp->buffer_size != 1)
			return;

		XDS_DEBUG(
			printf("CGMS: %s", cgmsa[(xp->buffer[0] >> 3) & 3]);
			if (xp->buffer[0] & 0x18)
			printf("; %s", scrambling[(xp->buffer[0] >> 1) & 3]);
			printf("; analog source: %d", xp->buffer[0] & 1);
			)

			xds_intfu(pi->cgms_a, xp->buffer[0] & 63);

		break;

	case VBI3_XDS_PROGRAM_ASPECT_RATIO:
	{
		vbi3_aspect_ratio *r = &e.ev.aspect;

		log ("ASPECT_RATIO ");

		if (!vbi3_decode_xds_aspect_ratio (r, xp))
			return;
		/*
		log ("active %u-%u%s\n",
		     r->first_line, r->last_line,
		     r->ratio > 1.0 ? " (anamorphic)" : "");
		*/
		if (0 != memcmp (r, &vbi->prog_info[0].aspect, sizeof(*r))) {
			vbi->prog_info[0].aspect = *r;
// TODO
//			vbi->aspect_source = 3;

			e.type = VBI3_EVENT_ASPECT;
			caption_send_event (vbi, &e);

			neq = 1;
		}

		break;
	}

	case VBI3_XDS_PROGRAM_DESCRIPTION_BEGIN ...
		(VBI3_XDS_PROGRAM_DESCRIPTION_END - 1):
	{
		unsigned int line = xp->xds_subclass & 7;

			XDS_DEBUG(
				printf("program descr. line %d: >", line + 1);
				for (i = 0; i < xp->buffer_size; i++)
				putchar(vbi3_printable (xp->buffer[i]));
				printf("<\n");
				)

				neq = xds_strfu(pi->description[line],
						xp->buffer, xp->buffer_size);

			break;
		}

	default:
		XDS_DEBUG(printf("<unknown %d/%02x length %d>\n",
				 class, type, xp->buffer_size));
		return; /* no event */
	}

	if (0)
		printf("[type %d cycle %08x class %d neq %d]\n",
		       xp->xds_subclass,
		       vbi->cc.info_cycle[xp->xds_class], xp->xds_class, neq);

	if (neq) /* first occurence of this type with this data */
		vbi->cc.info_cycle[xp->xds_class] |= 1 << xp->xds_subclass;
	else if (vbi->cc.info_cycle[xp->xds_class] & (1 << xp->xds_subclass)) {
		/* Second occurance of this type with same data */

		e.type = VBI3_EVENT_PROG_INFO;
		e.ev.prog_info = pi;

		caption_send_event(vbi, &e);

		vbi->cc.info_cycle[xp->xds_class] = 0;
		/* all changes reported */
	}
}

vbi3_bool
vbi3_decode_xds_tape_delay	(unsigned int *		tape_delay,
				 const vbi3_xds_packet *	xp)
{
	unsigned int hour;
	unsigned int min;

	assert (NULL != tape_delay);
	assert (NULL != xp);

	if (2 != xp->buffer_size)
		return FALSE;

	min = xp->buffer[0] & 63;
	hour = xp->buffer[1] & 31;

	if (min > 60
	    || hour > 23) {
		return FALSE;
	}

	*tape_delay = hour * 60 + min;

	return TRUE;
}

static void
decode_channel			(vbi3_decoder *		vbi,
				 const vbi3_xds_packet *	xp)
{
#warning
  //	vbi3_network *n = &vbi->network.ev.network;
	vbi3_network *n = 0;

	switch (xp->xds_subclass) {
	case VBI3_XDS_CHANNEL_NAME:
#if 0 // FIXME
		if (xds_strfu(n->name, xp->buffer, xp->buffer_size)) {
			n->cycle = 1;
		} else if (n->cycle == 1) {

			char *s = n->name;
			uint32_t sum;

				if (n->call[0])
					s = n->call;

				for (sum = 0; *s; s++)
					sum = (sum >> 7) ^ hcrc[(sum ^ *s) & 0x7F];

				sum &= ((1UL << 31) - 1);
				sum |= 1UL << 30;

				if (n->nuid != 0)
					vbi3_chsw_reset(vbi, sum);

				n->nuid = sum;

				vbi->network.type = VBI3_EVENT_NETWORK;
				caption_send_event(vbi, &vbi->network);

				n->cycle = 3;
			}

			XDS_DEBUG(
				printf("Network name: '");
				for (i = 0; i < buffer_size; i++)
					putchar(vbi3_printable (buffer[i]));
				printf("'\n");
			)
#endif

				break;

	case VBI3_XDS_CHANNEL_CALL_LETTERS:
#if 0 // FIXME
			if (xds_strfu(n->call, buffer, buffer_size)) {
				if (n->cycle != 1) {
					n->name[0] = 0;
					n->cycle = 0;
				}
			}
#endif
			XDS_DEBUG(
				printf("Network call letters: '");
				for (i = 0; i < buffer_size; i++)
					putchar(vbi3_printable (buffer[i]));
				printf("'\n");
			)

			break;

	case VBI3_XDS_CHANNEL_TAPE_DELAY:
	{
		unsigned int delay;

		log ("TAPE_DELAY ");

		if (!vbi3_decode_xds_tape_delay (&delay, xp))
			return;

		log ("%02d:%02d", delay / 60, delay % 60);

		break;
	}

	default:
		log ("unknown subclass 0x%x, buffer_size %u",
			 xp->xds_subclass, xp->buffer_size);
		break;
	}
}

static void
fprint_date_flags		(FILE *			fp,
				 const uint8_t *	buffer)
{
	unsigned int d, l, z, t;

	d = !!(buffer[1] & 0x20); /* daylight savings time */
	l = !!(buffer[2] & 0x20); /* leap day (local Feb 29 on Mar 1 UTC) */
	z = !!(buffer[3] & 0x20); /* reset seconds to zero */
	t = !!(buffer[3] & 0x10); /* tape delayed */

	fprintf (fp, "d%u l%u z%u t%u ", d, l, z, t);
}

/**
 * @param tm The time of day is stored here. The following fields will be
 *   set: tm_min, hour, mday, mon, wday, year, isdst. The time zone is
 *   UTC.
 * @param date_flags TBD.
 *
 * Decodes an XDS time of day (0x0704) packet.
 *
 * @returns
 * FALSE if the buffer contains incorrect data.
 */
vbi3_bool
vbi3_decode_xds_time_of_day	(struct tm *		tm,
				 vbi3_xds_date_flags *	date_flags,
				 const vbi3_xds_packet *	xp)
{
	assert (NULL != tm);
	assert (NULL != xp);

	if (6 != xp->buffer_size)
		return FALSE;

	CLEAR (*tm);

	tm->tm_min   = xp->buffer[0] & 63;
	tm->tm_hour  = xp->buffer[1] & 31;
	tm->tm_mday  = xp->buffer[2] & 31;
	tm->tm_mon   = (xp->buffer[3] & 15) - 1;
	tm->tm_wday  = (xp->buffer[4] & 7) - 1;
	tm->tm_year  = (xp->buffer[5] & 63) + 90;
	tm->tm_isdst = !!(xp->buffer[1] & 0x20);

	if (tm->tm_min > 59
	    || tm->tm_hour > 23
	    || tm->tm_mday == 0
	    || tm->tm_mday > 31
	    || ((unsigned int) tm->tm_mon) > 11
	    || ((unsigned int) tm->tm_wday) > 6) {
		return FALSE;
	}

	if (date_flags)
		*date_flags = 0; /* TODO */

	return TRUE;
}

/**
 * @param hours_west Time zone defined in hours west of UTC.
 *   EST (New York) for example is UTC - 5, so hours_west is 5.
 * @param dso TRUE if the area served by the signal observes daylight
 *   savings time. If FALSE, the DST flag in a time of day packet must be
 *   ignored.
 *
 * Decodes an XDS time zone (0x0701) packet. These packets are inserted
 * locally at affiliate stations.
 *
 * @returns
 * FALSE if the buffer contains incorrect data.
 */
vbi3_bool
vbi3_decode_xds_time_zone	(unsigned int *		hours_west,
				 vbi3_bool *		dso,
				 const vbi3_xds_packet *	xp)
{
	unsigned int hour;

	assert (NULL != hours_west);
	assert (NULL != dso);
	assert (NULL != xp);

	if (1 != xp->buffer_size)
		return FALSE;

	hour = xp->buffer[0] & 31;

	if (hour > 23)
		return FALSE;

	*hours_west = hour;
	*dso = !!(xp->buffer[0] & 0x20);

	return TRUE;
}

/**
 * @param pid The program ID will be stored here (month, day, hour, minute,
 *   length and tape_delay). Additionally the date will be encoded in the @a pil
 *   field, and as seconds since 1970-01-01 00:00 in the @a time field. All dates
 *   are given relative to UTC.
 * @param xp XDS packet to decode.
 *
 * Decodes an XDS Impulse Capture ID (0x0702) packet, returning the scheduled
 * start time and length of a future program, transmitted for example during
 * a program announcement.
 *
 * @returns
 * FALSE if the packet contains incorrect data. In this case @a pid remains
 * unmodified.
 */
vbi3_bool
vbi3_decode_xds_impulse_capture_id
				(vbi3_program_id *	pid,
				 const vbi3_xds_packet *	xp)
{
	unsigned int minute;
	unsigned int hour;

	assert (NULL != pid);
	assert (NULL != xp);

	if (6 != xp->buffer_size)
		return FALSE;

	min  = xp->buffer[4] & 63;
	hour = xp->buffer[5] & 63;

	if (min > 59)
		return FALSE;

	if (!decode_program_id (pid, xp))
		return FALSE;

	pid->length = hour * 60 + min;

	return TRUE;
}

static void
decode_misc			(vbi3_decoder *		vbi,
				 const vbi3_xds_packet *	xp)
{
	switch (xp->xds_subclass) {
	case VBI3_XDS_MISC_TIME_OF_DAY:
	{
		struct tm tm;

		log ("TIME_OF_DAY ");

		if (!vbi3_decode_xds_time_of_day (&tm, NULL, xp))
			return;

		if (XDS_DECODER_LOG) {
			static const char *day_names [] = {
				"Sun", "Mon", "Tue", "Wed",
				"Thu", "Fri", "Sat", "???"
			};

			fprintf (stderr, "%s %u-%02u-%02u %02u:%02u ",
				 day_names[tm.tm_wday],
				 tm.tm_year + 1900,
				 tm.tm_mon + 1,
				 tm.tm_mday,
				 tm.tm_hour,
				 tm.tm_min);

			fprint_date_flags (stderr, xp->buffer);
		}

		break;
	}

	case VBI3_XDS_MISC_IMPULSE_CAPTURE_ID:
	{
		vbi3_program_id pi;

		log ("IMPULSE_CAPTURE_ID ");

		if (!vbi3_decode_xds_impulse_capture_id (&pi, xp))
			return;

		log ("20XX-%02u-%02u %02u:%02u UTC length %02u:%02u ",
		     pi.month + 1, pi.day + 1,
		     pi.hour, pi.minute,
		     pi.length / 60, pi.length % 60);

		if (XDS_DECODER_LOG)
			fprint_date_flags (stderr, xp->buffer);

		break;
	}

	case VBI3_XDS_MISC_SUPPLEMENTAL_DATA_LOCATION:
	{
		log ("SUPPLEMENTAL_DATA_LOCATION ");

		if (XDS_DECODER_LOG) {
			unsigned int i;

			for (i = 0; i < xp->buffer_size; ++i) {
				unsigned int field;
				unsigned int line;

				field = !!(xp->buffer[i] & 0x20);
				line = xp->buffer[i] & 31;

				fprintf (stderr, "\n[%u] field %u line %u",
					 i, field, line);
			}

			fputc ('\n', stderr);
		}

		break;
	}

	case VBI3_XDS_MISC_LOCAL_TIME_ZONE:
	{
		unsigned int hour;
		vbi3_bool dso;

		log ("LOCAL_TIME_ZONE ");

		if (!vbi3_decode_xds_time_zone (&hour, &dso, xp))
			return;

		log ("UTC - %d h, dst=%u", hour, 0);

		break;
	}

	/* What's this out-of-band channel thingy? */

	default:
		log ("unknown subclass 0x%x, buffer_size %u",
		     xp->xds_subclass, xp->buffer_size);
		break;
	}
}

void
_vbi3_decode_xds		(vbi3_xds_demux *	xd,
				 void *			user_data,
				 const vbi3_xds_packet *	xp)
{
	vbi3_decoder *vbi = user_data;

	assert (xp->buffer_size > 0 && xp->buffer_size <= 32);

	switch (xp->xds_class) {
	case VBI3_XDS_CLASS_CURRENT:
		log ("XDS CURRENT ");
		decode_program (vbi, xp);
		break;

	case VBI3_XDS_CLASS_FUTURE:
		log ("XDS FUTURE ");
		decode_program (vbi, xp);
		break;

	case VBI3_XDS_CLASS_CHANNEL:
		log ("XDS CHANNEL ");
		decode_channel (vbi, xp);
		break;

	case VBI3_XDS_CLASS_MISC:
		log ("XDS MISC ");
		decode_misc (vbi, xp);
		break;

	default:
		log ("XDS unknown class 0x%x 0x%x %u",
		     xp->xds_class, xp->xds_subclass,
		     xp->buffer_size);
		break;
	}

	log ("\n");
}

#endif
