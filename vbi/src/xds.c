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

/* $Id: xds.c,v 1.1.2.2 2004-02-25 17:27:59 mschimek Exp $ */

#include "../site_def.h"
#include "../config.h"

#include <stdio.h>
#include <stdlib.h>

#include "vbi.h"

#include "hamm.h"
#include "xds.h"

/**
 * @addtogroup XDSDecoder Extended Data Service Decoder
 * @ingroup LowDec
 * @brief Functions to decode XDS packets (EIA 608).
 */

#ifndef XDS_DECODER_LOG
#define XDS_DECODER_LOG 0
#endif

#define dec_log(format, args...)					\
do {									\
	if (XDS_DECODER_LOG)						\
		fprintf (stderr, format , ##args);			\
} while (0)

static inline void
caption_send_event(vbi_decoder *vbi, vbi_event *ev)
{
	/* Permits calling vbi_fetch_cc_page from handler */
	pthread_mutex_unlock(&vbi->cc.mutex);

	vbi_send_event(vbi, ev);

	pthread_mutex_lock(&vbi->cc.mutex);
}

#define XDS_DEBUG(x)

/* vbi_classify_page, program_info language */
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
flush_prog_info(vbi_decoder *vbi, vbi_program_info *pi, vbi_event *e)
{
	e->ev.aspect = pi->aspect;

	vbi_reset_prog_info(pi);

	if (memcmp(&e->ev.aspect, &pi->aspect, sizeof(pi->aspect)) != 0) {
		e->type = VBI_EVENT_ASPECT;
		caption_send_event(vbi, e);
	}

	vbi->cc.info_cycle[pi->future] = 0;
}

vbi_bool
vbi_decode_xds_aspect_ratio	(vbi_aspect_ratio *	ar,
				 const uint8_t *	buffer,
				 unsigned int		buffer_size)
{
	assert (NULL != ar);
	assert (NULL != buffer);

	if (buffer_size < 2 || buffer_size > 3)
		return FALSE;

	CLEAR (*ar);

	ar->first_line = 22 + (buffer[0] & 63);
	ar->last_line = 262 - (buffer[1] & 63);

	ar->film_mode = FALSE;

	ar->open_subtitles = VBI_SUBT_UNKNOWN;

	if (buffer_size >= 3 && (buffer[2] & 1))
		ar->ratio = 16.0 / 9.0;
	else
		ar->ratio = 1.0;

	return TRUE;
}

static void
decode_program			(vbi_decoder *		vbi,
				 vbi_xds_class		sp_class,
				 vbi_xds_subclass_program sp_subclass,
				 const uint8_t *	buffer,
				 unsigned int		buffer_size)
{
	vbi_program_info *pi;
	vbi_event e;
	int neq, i;

	if (!(vbi->event_mask & (VBI_EVENT_ASPECT | VBI_EVENT_PROG_INFO)))
		return;

	pi = &vbi->prog_info[sp_class];
	neq = 0;

	switch (sp_subclass) {
	case VBI_XDS_PROGRAM_ID:
	{
		int month, day, hour, min;

		if (buffer_size != 4)
			return;

		month = buffer[3] & 15;
		day = buffer[2] & 31;
		hour = buffer[1] & 31;
		min = buffer[0] & 63;

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

		pi->tape_delayed = !!(buffer[3] & 0x10);

		if (neq) {
			flush_prog_info(vbi, pi, &e);

			pi->month = month;
			pi->day = day;
			pi->hour = hour;
			pi->min = min;

			pi->tape_delayed = !!(buffer[3] & 0x10);
		}

		break;
	}

	case VBI_XDS_PROGRAM_LENGTH:
	{
		int lhour, lmin, ehour = -1, emin = -1, esec = 0;

		if (buffer_size < 2 || buffer_size > 6)
			return;

		lhour = buffer[1] & 63;
		lmin = buffer[0] & 63;

		if (buffer_size >= 3) {
			ehour = buffer[3] & 63;
			emin = buffer[2] & 63;

			if (buffer_size >= 5)
				esec = buffer[4] & 63;
		}

		XDS_DEBUG(
			printf("length: %02d:%02d, ", lhour, lmin);
			printf("elapsed: %02d:%02d", ehour, emin);
			if (buffer_size >= 5)
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

	case VBI_XDS_PROGRAM_NAME:
		if (buffer_size < 2)
			return;

		XDS_DEBUG(
			printf("program title: '");
			for (i = 0; i < buffer_size; i++)
			putchar(printable(buffer[i]));
			printf("'\n");
			)

			neq = xds_strfu(pi->title, buffer, buffer_size);

		if (!neq) { /* no title change */
			if (!(vbi->cc.info_cycle[sp_class] & (1 << 3)))
				break; /* already reported */

			if (!(vbi->cc.info_cycle[sp_class] & (1 << 1))) {
				/* Second occurence without PIN */

				flush_prog_info(vbi, pi, &e);

				xds_strfu(pi->title, buffer, buffer_size);
				vbi->cc.info_cycle[sp_class] |= 1 << 3;
			}
		}

		break;

	case VBI_XDS_PROGRAM_TYPE:
	{
		int neq;

		XDS_DEBUG(
			printf("program type: ");
			for (i = 0; i < buffer_size; i++)
			printf((i > 0) ? ", %s" : "%s",
			       vbi_prog_type_str_by_id(
				       VBI_PROG_CLASSF_EIA_608, buffer[i]));
			printf("\n");
			)

			neq = (pi->type_classf != VBI_PROG_CLASSF_EIA_608);
		pi->type_classf = VBI_PROG_CLASSF_EIA_608;

		for (i = 0; i < buffer_size; i++) {
			neq |= pi->type_id[i] ^ buffer[i];
			pi->type_id[i] = buffer[i];
		}

		neq |= pi->type_id[i];
		pi->type_id[i] = 0;

		break;
	}

	case VBI_XDS_PROGRAM_RATING:
	{
		vbi_rating_auth auth;
		int r, g, dlsv = 0;

		if (buffer_size != 2)
			return;

		r = buffer[0] & 7;
		g = buffer[1] & 7;

		if (buffer[0] & 0x20)
			dlsv |= VBI_RATING_D;
		if (buffer[1] & 0x08)
			dlsv |= VBI_RATING_L;
		if (buffer[1] & 0x10)
			dlsv |= VBI_RATING_S;
		if (buffer[1] & 0x20)
			dlsv |= VBI_RATING_V;

		XDS_DEBUG(
			printf("program movie rating: %s, tv rating: ",
			       vbi_rating_str_by_id(VBI_RATING_AUTH_MPAA, r));
			if (buffer[0] & 0x10) {
				if (buffer[0] & 0x20)
					puts(vbi_rating_str_by_id(VBI_RATING_AUTH_TV_CA_FR, g));
				else
					puts(vbi_rating_str_by_id(VBI_RATING_AUTH_TV_CA_EN, g));
			} else {
				printf("%s; ", vbi_rating_str_by_id(VBI_RATING_AUTH_TV_US, g));
				if (buffer[1] & 0x20)
					printf("violence; ");
				if (buffer[1] & 0x10)
					printf("sexual situations; ");
				if (buffer[1] & 8)
					printf("indecent language; ");
				if (buffer[0] & 0x20)
					printf("sexually suggestive dialog");
				putchar('\n');
			}
			)

			if ((buffer[0] & 0x08) == 0) {
				if (r == 0) return;
				auth = VBI_RATING_AUTH_MPAA;
				pi->rating_dlsv = dlsv = 0;
			} else if ((buffer[0] & 0x10) == 0) {
				auth = VBI_RATING_AUTH_TV_US;
				r = g;
			} else if ((buffer[1] & 0x08) == 0) {
				if ((buffer[0] & 0x20) == 0) {
					if ((r = g) > 6) return;
					auth = VBI_RATING_AUTH_TV_CA_EN;
				} else {
					if ((r = g) > 5) return;
					auth = VBI_RATING_AUTH_TV_CA_FR;
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

	case VBI_XDS_PROGRAM_AUDIO_SERVICES:
	{
		static const vbi_audio_mode mode[2][8] = {
			{
				VBI_AUDIO_MODE_UNKNOWN,
				VBI_AUDIO_MODE_MONO,
				VBI_AUDIO_MODE_SIMULATED_STEREO,
				VBI_AUDIO_MODE_STEREO,
				VBI_AUDIO_MODE_STEREO_SURROUND,
				VBI_AUDIO_MODE_DATA_SERVICE,
				VBI_AUDIO_MODE_UNKNOWN, /* "other" */
				VBI_AUDIO_MODE_NONE
			}, {
				VBI_AUDIO_MODE_UNKNOWN,
				VBI_AUDIO_MODE_MONO,
				VBI_AUDIO_MODE_VIDEO_DESCRIPTIONS,
				VBI_AUDIO_MODE_NON_PROGRAM_AUDIO,
				VBI_AUDIO_MODE_SPECIAL_EFFECTS,
				VBI_AUDIO_MODE_DATA_SERVICE,
				VBI_AUDIO_MODE_UNKNOWN, /* "other" */
				VBI_AUDIO_MODE_NONE
			}
		};

		if (buffer_size != 2)
			return;

		XDS_DEBUG(printf("main audio: %s, %s; "
				 "second audio program: %s, %s\n",
				 map_type[buffer[0] & 7], language[(buffer[0] >> 3) & 7],
				 sap_type[buffer[1] & 7], language[(buffer[1] >> 3) & 7]));

		for (i = 0; i < 2; i++) {
			int l = (buffer[i] >> 3) & 7;
			vbi_audio_mode m = mode[i][buffer[i] & 7];
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

	case VBI_XDS_PROGRAM_CAPTION_SERVICES:
	{
		int services = 0;

		if (buffer_size > 8)
			return;

		for (i = 0; i < 8; i++)
			pi->caption_lang_code[i] = NULL;

		for (i = 0; i < buffer_size; i++) {
			int ch = buffer[i] & 7;
			int l = (buffer[i] >> 3) & 7;
			const char *s;

			ch = (ch & 1) * 4 + (ch >> 1);

			services |= 1 << ch;
			s = ((1 << l) & 0xC1) ? NULL : language[l];

			if (pi->caption_lang_code[ch] != s) {
				neq = 1; pi->caption_lang_code[ch] = s;
			}

			if (sp_class == VBI_XDS_CLASS_CURRENT)
				vbi->cc.channel[ch].lang_code[0] =
					pi->caption_lang_code[ch];
		}

		xds_intfu(pi->caption_services, services);

		XDS_DEBUG(
			printf("program caption services:\n");
			for (i = 0; i < buffer_size; i++)
			printf("Line %3d, channel %d, %s: %s\n",
			       (buffer[i] & 4) ? 284 : 21,
			       (buffer[i] & 2) ? 2 : 1,
			       (buffer[i] & 1) ? "text      " : "captioning",
			       language[(buffer[i] >> 3) & 7]);
			)

			break;
	}

	case VBI_XDS_PROGRAM_CGMS:
		if (buffer_size != 1)
			return;

		XDS_DEBUG(
			printf("CGMS: %s", cgmsa[(buffer[0] >> 3) & 3]);
			if (buffer[0] & 0x18)
			printf("; %s", scrambling[(buffer[0] >> 1) & 3]);
			printf("; analog source: %d", buffer[0] & 1);
			)

			xds_intfu(pi->cgms_a, buffer[0] & 63);

		break;

	case VBI_XDS_PROGRAM_ASPECT_RATIO:
	{
		vbi_aspect_ratio *r = &e.ev.aspect;

		dec_log ("ASPECT_RATIO ");

		if (!vbi_decode_xds_aspect_ratio (r, buffer, buffer_size))
			return;

		dec_log ("active %u-%u%s\n",
			 r->first_line, r->last_line,
			 r->ratio > 1.0 ? " (anamorphic)" : "");

		if (0 != memcmp (r, &vbi->prog_info[0].aspect, sizeof(*r))) {
			vbi->prog_info[0].aspect = *r;
			vbi->aspect_source = 3;

			e.type = VBI_EVENT_ASPECT;
			caption_send_event (vbi, &e);

			neq = 1;
		}

		break;
	}

	case VBI_XDS_PROGRAM_DESCRIPTION_BEGIN ...
		(VBI_XDS_PROGRAM_DESCRIPTION_END - 1):
	{
		unsigned int line = sp_subclass & 7;

			XDS_DEBUG(
				printf("program descr. line %d: >", line + 1);
				for (i = 0; i < buffer_size; i++)
				putchar(printable(buffer[i]));
				printf("<\n");
				)

				neq = xds_strfu(pi->description[line], buffer, buffer_size);

			break;
		}

	default:
		XDS_DEBUG(printf("<unknown %d/%02x length %d>\n",
				 class, type, buffer_size));
		return; /* no event */
	}

	if (0)
		printf("[type %d cycle %08x class %d neq %d]\n",
		       sp_subclass, vbi->cc.info_cycle[sp_class], sp_class, neq);

	if (neq) /* first occurence of this type with this data */
		vbi->cc.info_cycle[sp_class] |= 1 << sp_subclass;
	else if (vbi->cc.info_cycle[sp_class] & (1 << sp_subclass)) {
		/* Second occurance of this type with same data */

		e.type = VBI_EVENT_PROG_INFO;
		e.ev.prog_info = pi;

		caption_send_event(vbi, &e);

		vbi->cc.info_cycle[sp_class] = 0; /* all changes reported */
	}
}

vbi_bool
vbi_decode_xds_tape_delay	(unsigned int *		tape_delay,
				 const uint8_t *	buffer,
				 unsigned int		buffer_size)
{
	unsigned int hour;
	unsigned int min;

	assert (NULL != tape_delay);
	assert (NULL != buffer);

	if (2 != buffer_size)
		return FALSE;

	min = buffer[0] & 63;
	hour = buffer[1] & 31;

	if (min > 60
	    || hour > 23) {
		return FALSE;
	}

	*tape_delay = hour * 60 + min;

	return TRUE;
}

static void
decode_channel			(vbi_decoder *		vbi,
				 vbi_xds_subclass_channel sp_subclass,
				 const uint8_t *	buffer,
				 unsigned int		buffer_size)
{
	vbi_network *n = &vbi->network.ev.network;

	switch (sp_subclass) {
	case VBI_XDS_CHANNEL_NAME:
		if (xds_strfu(n->name, buffer, buffer_size)) {
			n->cycle = 1;
		} else if (n->cycle == 1) {
#if 0 // FIXME
			char *s = n->name;
			uint32_t sum;

				if (n->call[0])
					s = n->call;

				for (sum = 0; *s; s++)
					sum = (sum >> 7) ^ hcrc[(sum ^ *s) & 0x7F];

				sum &= ((1UL << 31) - 1);
				sum |= 1UL << 30;

				if (n->nuid != 0)
					vbi_chsw_reset(vbi, sum);

				n->nuid = sum;

				vbi->network.type = VBI_EVENT_NETWORK;
				caption_send_event(vbi, &vbi->network);

				n->cycle = 3;
#endif
			}

			XDS_DEBUG(
				printf("Network name: '");
				for (i = 0; i < buffer_size; i++)
					putchar(printable(buffer[i]));
				printf("'\n");
			)

			break;

	case VBI_XDS_CHANNEL_CALL_LETTERS:
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
					putchar(printable(buffer[i]));
				printf("'\n");
			)

			break;

	case VBI_XDS_CHANNEL_TAPE_DELAY:
	{
		unsigned int delay;

		dec_log ("TAPE_DELAY ");

		if (!vbi_decode_xds_tape_delay (&delay, buffer, buffer_size))
			return;

		dec_log ("%02d:%02d", delay / 60, delay % 60);

		break;
	}

	default:
		dec_log ("unknown subclass 0x%x, buffer_size %u",
			 sp_subclass, buffer_size);
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
vbi_bool
vbi_decode_xds_time_of_day	(struct tm *		tm,
				 vbi_xds_date_flags *	date_flags,
				 const uint8_t *	buffer,
				 unsigned int		buffer_size)
{
	assert (NULL != tm);
	assert (NULL != buffer);

	if (6 != buffer_size)
		return FALSE;

	CLEAR (*tm);

	tm->tm_min   = buffer[0] & 63;
	tm->tm_hour  = buffer[1] & 31;
	tm->tm_mday  = buffer[2] & 31;
	tm->tm_mon   = (buffer[3] & 15) - 1;
	tm->tm_wday  = (buffer[4] & 7) - 1;
	tm->tm_year  = (buffer[5] & 63) + 90;
	tm->tm_isdst = !!(buffer[1] & 0x20);

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
vbi_bool
vbi_decode_xds_time_zone	(unsigned int *		hours_west,
				 vbi_bool *		dso,
				 const uint8_t *	buffer,
				 unsigned int		buffer_size)
{
	unsigned int hour;

	assert (NULL != hours_west);
	assert (NULL != dso);
	assert (NULL != buffer);

	if (1 != buffer_size)
		return FALSE;

	hour = buffer[0] & 31;

	if (hour > 23)
		return FALSE;

	*hours_west = hour;
	*dso = !!(buffer[0] & 0x20);

	return TRUE;
}

/**
 *
 */
vbi_bool
vbi_decode_xds_impulse_capture_id
				(vbi_program_id *	pi,
				 const uint8_t *	buffer,
				 unsigned int		buffer_size)
{
	unsigned int hour;
	unsigned int min;

	assert (NULL != pi);
	assert (NULL != buffer);

	if (6 != buffer_size)
		return FALSE;

	pi->nuid = VBI_NUID_UNKNOWN;

	/* TZ UTC */

	pi->minute	= buffer[0] & 63;
	pi->hour	= buffer[1] & 31;
	pi->day		= buffer[2] & 31;
	pi->month	= buffer[3] & 15;

	if (pi->minute > 59
	    || pi->hour > 23
	    || pi->day > 30
	    || pi->month > 11)
		return FALSE;

	pi->pil = VBI_PIL (pi->day + 1, pi->month + 1, pi->hour, pi->minute);

	min  = buffer[4] & 63;
	hour = buffer[5] & 63;

	if (min > 59)
		return FALSE;

	pi->length = hour * 60 + min;

	pi->lci = 0; /* just one label channel */
	pi->luf	= FALSE; /* no update, just id */
	pi->mi 	= FALSE; /* id is not 30 s early */
	pi->prf	= FALSE; /* prepare to record unknown */

	pi->pcs_audio = VBI_PCS_AUDIO_UNKNOWN;
	pi->pty	= 0; /* none / unknown */

	pi->tape_delayed = !!(buffer[3] & 0x10);

	return TRUE;
}

static void
decode_misc			(vbi_decoder *		vbi,
				 vbi_xds_subclass_misc	sp_subclass,
				 const uint8_t *	buffer,
				 unsigned int		buffer_size)
{
	switch (sp_subclass) {
	case VBI_XDS_MISC_TIME_OF_DAY:
	{
		struct tm tm;

		dec_log ("TIME_OF_DAY ");

		if (!vbi_decode_xds_time_of_day
		    (&tm, NULL, buffer, buffer_size))
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

			fprint_date_flags (stderr, buffer);
		}

		break;
	}

	case VBI_XDS_MISC_IMPULSE_CAPTURE_ID:
	{
		vbi_program_id pi;

		dec_log ("IMPULSE_CAPTURE_ID ");

		if (!vbi_decode_xds_impulse_capture_id
		    (&pi, buffer, buffer_size))
			return;

		dec_log ("20XX-%02u-%02u %02u:%02u UTC length %02u:%02u ",
			 pi.month + 1, pi.day + 1,
			 pi.hour, pi.minute,
			 pi.length / 60, pi.length % 60);

		if (XDS_DECODER_LOG)
			fprint_date_flags (stderr, buffer);

		break;
	}

	case VBI_XDS_MISC_SUPPLEMENTAL_DATA_LOCATION:
	{
		dec_log ("SUPPLEMENTAL_DATA_LOCATION ");

		if (XDS_DECODER_LOG) {
			unsigned int i;

			for (i = 0; i < buffer_size; ++i) {
				unsigned int field;
				unsigned int line;

				field = !!(buffer[i] & 0x20);
				line = buffer[i] & 31;

				fprintf (stderr, "\n[%u] field %u line %u",
					 i, field, line);
			}

			fputc ('\n', stderr);
		}

		break;
	}

	case VBI_XDS_MISC_LOCAL_TIME_ZONE:
	{
		unsigned int hour;

		dec_log ("LOCAL_TIME_ZONE ");

		if (!vbi_decode_xds_time_zone
		    (&hour, NULL, buffer, buffer_size))
			return;

		dec_log ("UTC - %d h, dst=%u", hour, 0);

		break;
	}

	/* What's this out-of-band channel thingy? */

	default:
		dec_log ("unknown subclass 0x%x, buffer_size %u",
			 sp_subclass, buffer_size);
		break;
	}
}

void
vbi_decode_xds			(vbi_xds_demux *	xd,
				 void *			user_data,
				 vbi_xds_class		sp_class,
				 vbi_xds_subclass	sp_subclass,
				 const uint8_t *	buffer,
				 unsigned int		buffer_size)
{
	vbi_decoder *vbi = user_data;

	assert (buffer_size > 0 && buffer_size <= 32);

	switch (sp_class) {
	case VBI_XDS_CLASS_CURRENT:
		dec_log ("XDS CURRENT ");
		decode_program (vbi, sp_class, sp_subclass,
				buffer, buffer_size);
		break;

	case VBI_XDS_CLASS_FUTURE:
		dec_log ("XDS FUTURE ");
		decode_program (vbi, sp_class, sp_subclass,
				buffer, buffer_size);
		break;

	case VBI_XDS_CLASS_CHANNEL:
		dec_log ("XDS CHANNEL ");
		decode_channel (vbi, sp_subclass, buffer, buffer_size);
		break;

	case VBI_XDS_CLASS_MISC:
		dec_log ("XDS MISC ");
		decode_misc (vbi, sp_subclass, buffer, buffer_size);
		break;

	default:
		dec_log ("XDS unknown class 0x%x 0x%x %u",
			 sp_class, sp_subclass, buffer_size);
		break;
	}

	dec_log ("\n");
}

/* ------------------------------------------------------------------------- */

/**
 * @addtogroup XDSDemux Extended Data Service Demultiplexer
 * @ingroup LowDec
 * @brief Separating XDS data from a Closed Caption stream
 *   (EIA 608).
 */

#ifndef XDS_DEMUX_LOG
#define XDS_DEMUX_LOG 0
#endif

#define dmx_log(format, args...)					\
do {									\
	if (XDS_DEMUX_LOG)						\
		fprintf (stderr, format , ##args);			\
} while (0)

#define printable(c)							\
	((((c) & 0x7F) < 0x20 || ((c) & 0x7F) >= 0x7F) ? '.' : (c) & 0x7F)

static void
fprint_subpacket		(FILE *			fp,
				 vbi_xds_class		sp_class,
				 vbi_xds_subclass	sp_subclass,
				 const uint8_t *	buffer,
				 unsigned int		buffer_size)
{
	unsigned int i;

	fprintf (fp, "XDS packet 0x%x/0x%02x ", sp_class, sp_subclass);

	for (i = 0; i < buffer_size; ++i)
		fprintf (fp, "%02x ", buffer[i]);

	fputs (" '", fp);

	for (i = 0; i < buffer_size; ++i)
		fputc (printable (buffer[i]), fp);

	fputs ("'\n", fp);
}

/**
 * @param xd XDS demultiplexer context allocated with vbi_xds_demux_new().
 * @param buffer Closed Caption character pair, as in struct vbi_sliced.
 *
 * blah
 */
void
vbi_xds_demux_demux		(vbi_xds_demux *	xd,
				 const uint8_t		buffer[2])
{
	xds_subpacket *sp;
	int c1, c2;

	sp = xd->curr_sp;

	dmx_log ("XDS demux %02x %02x\n", buffer[0], buffer[1]);

	c1 = vbi_ipar8 (buffer[0]);
	c2 = vbi_ipar8 (buffer[1]);

	if ((c1 | c2) < 0) {
		dmx_log ("XDS tx error, discard current packet\n");
		goto discard_current_packet;
	}

	switch (c1) {
	case 1 ... 14: /* packet header */
	{
		vbi_xds_class sp_class;
		vbi_xds_subclass sp_subclass;

		sp_class = (c1 - 1) >> 1;
		sp_subclass = c2;

		if (sp_class > VBI_XDS_CLASS_MISC
		    || sp_subclass > N_ELEMENTS (xd->subpacket[0])) {
			dmx_log ("XDS ignore packet 0x%x/0x%02x, "
				 "unknown class or subclass\n",
				 sp_class, c2);
			break;
		}

		sp = &xd->subpacket[sp_class][sp_subclass];

		xd->curr_sp = sp;
		xd->curr_class = sp_class;
		xd->curr_subclass = sp_subclass;

		if (c1 & 1) { /* start */
			sp->checksum = c1 + c2;
			sp->count = 2;
		} else { /* continue */
			if (0 == sp->count) {
				dmx_log ("XDS can't continue packet "
					 "0x%x/0x%02x, missed start\n",
					 xd->curr_class, xd->curr_subclass);
				break;
			}
		}

		goto success;
	}

	case 15: /* packet terminator */
		if (!sp) {
			dmx_log ("XDS can't finish packet, missed start\n");
			break;
		}

		sp->checksum += c1 + c2;

		if (0 != (sp->checksum & 0x7F)) {
			dmx_log ("XDS ignore packet 0x%x/0x%02x, "
				 "checksum error\n",
				 xd->curr_class, xd->curr_subclass);
		} else if (sp->count <= 2) {
			dmx_log ("XDS ignore empty packet 0x%x/0x%02x\n",
				 xd->curr_class, xd->curr_subclass);
		} else {
			if (XDS_DEMUX_LOG)
				fprint_subpacket (stderr,
						  xd->curr_class,
						  xd->curr_subclass,
						  sp->buffer, sp->count - 2);

			xd->callback (xd, xd->user_data,
				      xd->curr_class,
				      xd->curr_subclass,
				      sp->buffer, sp->count - 2);
		}

		break;

	case 0x20 ... 0x7F: /* packet contents */
		if (!sp) {
			dmx_log ("XDS can't store packet, missed start\n");
			break;
		}

		if (sp->count >= sizeof (sp->buffer) + 2) {
			dmx_log ("XDS discard packet 0x%x/0x%02x, "
				 "buffer overflow\n",
				 xd->curr_class, xd->curr_subclass);
			break;
		}

		sp->buffer[sp->count - 2] = c1;
		sp->buffer[sp->count - 1] = c2;

		sp->checksum += c1 + c2;
		sp->count += 1 + (0 != c2);

		goto success;

	default:
		dmx_log ("XDS ignore unrelated data\n");

		goto success;
	}

 discard_current_packet:

	if (sp) {
		sp->count = 0;
		sp->checksum = 0;
	}

	xd->curr_sp = NULL;

 success:

	return;
}

/**
 * @param xd XDS demultiplexer context allocated with vbi_xds_demux_new().
 *
 * Resets the XDS demux context, useful for example after a channel
 * change.
 */
void
vbi_xds_demux_reset		(vbi_xds_demux *	xd)
{
	unsigned int n;
	unsigned int i;

	assert (NULL != xd);

	n = sizeof (xd->subpacket) / sizeof (xd->subpacket[0][0]);

	for (i = 0; i < n; ++i)
		xd->subpacket[0][i].count = 0;

	xd->curr_sp = NULL;
}

/**
 * @internal
 */
void
vbi_xds_demux_destroy		(vbi_xds_demux *	xd)
{
	CLEAR (*xd);
}

/**
 * @internal
 */
vbi_bool
vbi_xds_demux_init		(vbi_xds_demux *	xd,
				 vbi_xds_demux_cb *	cb,
				 void *			user_data)
{
	xd->callback = cb;
	xd->user_data = user_data;

	vbi_xds_demux_reset (xd);

	return TRUE;
}

/**
 * @param xd XDS demultiplexer context allocated with
 *   vbi_xds_demux_new(), can be @c NULL.
 *
 * Frees all resources associated with @a xd.
 */
void
vbi_xds_demux_delete		(vbi_xds_demux *	xd)
{
	if (NULL == xd)
		return;

	vbi_xds_demux_destroy (xd);

	free (xd);		
}

/**
 * @param cb Function to be called by vbi_xds_demux_demux() when
 *   a new packet is available.
 * @param user_data User pointer passed through to @a cb function.
 *
 * Allocates a new Extended Data Service (EIA 608)
 * demultiplexer.
 *
 * @returns
 * Pointer to newly allocated XDS demux context which must be
 * freed with vbi_xds_demux_delete() when done. @c NULL on failure
 * (out of memory).
 */
vbi_xds_demux *
vbi_xds_demux_new		(vbi_xds_demux_cb *	cb,
				 void *			user_data)
{
	vbi_xds_demux *xd;

	assert (NULL != cb);

	if (!(xd = malloc (sizeof (*xd)))) {
		vbi_log_printf (__FUNCTION__, "Out of memory");
		return NULL;
	}

	if (!vbi_xds_demux_init (xd, cb, user_data)) {
		vbi_xds_demux_destroy (xd);
		free (xd);
		return NULL;
	}

	return xd;
}
