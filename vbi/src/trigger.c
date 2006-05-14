/*
 *  libzvbi - Triggers
 *
 *  Copyright (C) 2001-2004 Michael H. Schimek
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

/* $Id: trigger.c,v 1.4.2.10 2006-05-14 14:14:12 mschimek Exp $ */

/*
   Based on EACEM TP 14-99-16 "Data Broadcasting", rev 0.8;
   ATVEF "Enhanced Content Specification", v1.1 (www.atvef.com);
   and http://developer.webtv.net
 */

#include "../config.h"

#include <stdio.h>		/* FILE */
#include <stdlib.h>		/* malloc() */
#include <string.h>		/* strcmp() */
#include <ctype.h>		/* isdigit(), isxdigit(), tolower() */
#include <time.h>		/* time_t, mktime() */
#include <limits.h>		/* INT_MAX */
#include <math.h>		/* fabs() */
#include "conv.h"		/* _vbi3_strdup_locale_utf8() */
#include "misc.h"		/* CLEAR(), _vbi3_strndup() */
#include "trigger.h"

struct _vbi3_trigger {
	_vbi3_trigger *		next;
	vbi3_link		link;
	vbi3_network		source_network;
	vbi3_network		target_network;
	double			fire_time;
	int			view;
	vbi3_bool		_delete;
};

static vbi3_bool
verify_checksum			(const uint8_t *	s,
				 unsigned int		s_size,
				 unsigned int		checksum)
{
	unsigned int sum1;
	unsigned int sum2;

	sum1 = checksum;

	for (; s_size > 1; s_size -= 2) {
		sum1 += *s++ << 8;
		sum1 += *s++;
	}

	sum2 = sum1;

	/* There seems to be confusion about how left-over bytes shall
	   be added. The example C code in RFC 1071 subclause 4.1
	   contradicts the definition in subclause 1 (zero pad to 16
	   bit). Networks don't get this right either, so we try both. */
	if (s_size > 0) {
		sum1 += *s << 8; /* correct */
		sum2 += *s << 0; /* wrong */
	}

	while (sum1 >= (1 << 16))
		sum1 = (sum1 & 0xFFFFUL) + (sum1 >> 16);

	while (sum2 >= (1 << 16))
		sum2 = (sum2 & 0xFFFFUL) + (sum2 >> 16);

	return (0xFFFFUL == sum1 || 0xFFFFUL == sum2);
}

static int
parse_dec			(const uint8_t *	s,
				 unsigned int		n_digits)
{
	int r;

	r = 0;

	while (n_digits-- > 0) {
		if (!isdigit (*s))
			return -1;

		r = r * 10 + (*s++ - '0');
	}

	return r;
}

static int
parse_hex			(const uint8_t *	s,
				 unsigned int		n_digits)
{
	int r;

	r = 0;

	while (n_digits-- > 0) {
		if (!isxdigit (*s))
			return -1;

		r = r * 16 + (*s & 15) + ((*s > '9') ? 9 : 0);
		s++;
	}

	return r;
}

/* XXX http://developer.webtv.net/itv/tvlink/main.htm adds more ...??? */
static time_t
parse_date			(const uint8_t *	s)
{
	struct tm tm;

	CLEAR (tm);

	if ((tm.tm_year = parse_dec (s + 0, 4)) < 0
	    || (tm.tm_mon = parse_dec (s + 4, 2)) < 0
	    || (tm.tm_mday = parse_dec (s + 6, 2)) < 0)
		return (time_t) -1;

	if ('T' == s[8]) {
		if ((tm.tm_hour = parse_dec (s + 9, 2)) < 0
		    || (tm.tm_min = parse_dec (s + 11, 2)) < 0)
			return (time_t) -1;

		if (0 != s[13] &&
		    (tm.tm_sec = parse_dec (s + 13, 2)) < 0)
			return (time_t) -1;
	}

	tm.tm_year -= 1900;

	// XXX tm is utc
	return mktime (&tm);
}

static int
parse_time			(const uint8_t *	s)
{
	const char *csrc;
	char *src;
	int seconds;
	int frames;

	csrc = (const char *) s;
	seconds = strtoul (csrc, &src, 10);
	csrc = src; 
	frames = 0;

	if ('F' == *s)
		if ((frames = parse_dec (s + 1, 2)) < 0)
			return -1;

	return seconds * 25 + frames;
}

static int
parse_bool			(const uint8_t *	s)
{
	const char *cs = (const char *) s;

	return (0 == strcmp (cs, "1")
		|| 0 == strcmp (cs, "true")
		|| 0 == strcmp (cs, "TRUE"));
}

static const uint8_t *
parse_string			(uint8_t *		buffer,
				 unsigned int		buffer_size,
				 const uint8_t *	s,
				 int			delimiter)
{
	uint8_t *end;
	int c;

	end = buffer + buffer_size - 1;

	while (':' != (c = *s)) {
		if (delimiter == c)
			break;

		if ('%' == c) {
			if ((c = parse_hex (s + 1, 2)) < 0x20)
				return NULL;
			s += 3;
		} else {
			++s;
		}

		if (0 == c || buffer >= end)
			return NULL;

		*buffer++ = c;
	}

	*buffer = 0;

	return s;
}

/* NB returns UTF-8 string in buffer. */
static const uint8_t *
parse_quoted			(uint8_t *		buffer,
				 unsigned int		buffer_size,
				 const uint8_t *	s,
				 int			delimiter)
{
	uint8_t *end;
        vbi3_bool quote;
	int c;

	quote = FALSE;
	end = buffer + buffer_size - 1;

	while (c = *s, quote || delimiter != c) {
		if ('"' == c) {
			quote ^= TRUE;
			++s;
		} else if ('%' == c) {
			if ((c = parse_hex (s + 1, 2)) < 0x20)
				return NULL;
			s += 3;
		} else {
			++s;
		}

		if (0 == c || (buffer + 1) >= end)
			return NULL;

		/* Latin-1 to UTF-8 */

		if (c < 0x80) {
			*buffer++ = c;
		} else {
			buffer[0] = 0xC0 | (c >> 6);
			buffer[1] = 0x80 | (c & 0x3F);
			buffer += 2;
		}
	}

	*buffer = 0;

	return s;
}

static int
keyword				(const uint8_t *	s,
				 const char **		keywords,
				 unsigned int		n_keywords)
{
	unsigned int i;
	unsigned int j;

	if (0 == s[0])
		return -1;

	for (i = 0; i < n_keywords; ++i) {
		for (j = 0; s[j]; ++j) {
			if (tolower (s[j]) != keywords[i][j])
				goto next;
		}

		if (0 == keywords[i][j])
			return i;
		/* First character abbreviation. */
		if (1 == j)
			return i;
	next:
		;
	}

	return -1;
}

static void
_vbi3_trigger_dump		(_vbi3_trigger *		t,
				 FILE *			fp,
				 const char *		type,
				 double			current_time)
{
	fprintf (fp, "Time %f %s link\n", current_time, type);

	_vbi3_link_dump (&t->link, fp);
	
	fputs ("source: ", fp);

	_vbi3_network_dump (&t->source_network, fp);

	fprintf (fp, "fire=%f view=%u ('%c') delete=%u\n",
		 t->fire_time, t->view, t->view, t->_delete);
}

/** @internal */
void
_vbi3_trigger_destroy		(_vbi3_trigger *		t)
{
	assert (NULL != t);

	vbi3_link_destroy (&t->link);
	vbi3_network_destroy (&t->source_network);
	vbi3_network_destroy (&t->target_network);

	CLEAR (*t);
}

/** @internal */
vbi3_bool
_vbi3_trigger_init		(_vbi3_trigger *		t,
				 const vbi3_network *	nk,
				 double			current_time)
{
	assert (NULL != t);

	vbi3_link_init (&t->link);

	if (!vbi3_network_copy (&t->source_network, nk))
		return FALSE;

	vbi3_network_init (&t->target_network);

	t->fire_time	  = current_time;
	t->view		  = 'w';
	t->_delete	  = FALSE;

	return TRUE;
}

/** @internal */
void
_vbi3_trigger_delete		(_vbi3_trigger *		t)
{
	if (NULL == t)
		return;

	_vbi3_trigger_destroy (t);

	vbi3_free (t);
}

static const uint8_t *
_vbi3_trigger_from_eacem		(_vbi3_trigger *		t,
				 const uint8_t *	s,
				 const vbi3_network *	nk,
				 double			current_time)
{
	const uint8_t *s1;
	int active;

	if (!_vbi3_trigger_init (t, nk, current_time))
		return NULL;

	active = INT_MAX;

	for (s1 = s;; ++s) {
		const uint8_t *begin;
		int c;

		begin = s;

		switch ((c = *s)) {
		case 0:
			break;

		case '<':
			if (s != s1)
				goto failure;

			for (++s; '>' != (c = *s); ++s) {
				if (0 == c)
					goto failure;
			}

			t->link.url = strndup (begin + 1, s - begin - 1);
			if (!t->link.url)
				goto failure;

			break;

		case '[':
		case '(': /* some networks transmit ( instead of [ */
		{
			static const char *attrs [] = {
				"active",
				"countdown",
				"delete",
				"expires",
				"name",
				"priority",
				"script"
			};
			uint8_t buf1[256];
			uint8_t buf2[256];
			int delimiter;

			delimiter = (c == '[') ? ']' : ')';

			s = parse_string (buf1, sizeof (buf1), s, delimiter);
			if (!s || 0 == buf1[0])
				goto failure;

			if (':' != *s++) {
				int chks;

				chks = strtoul (buf1, NULL, 16);
				if (!verify_checksum (s1, begin - s1, chks)) {
					if (0)
						fprintf (stderr,
							 "bad checksum\n");
					goto failure;
				}

				break;
			}

			s = parse_quoted (buf2, sizeof (buf2), s, delimiter);
			if (!s || 0 == buf2[0])
				goto failure;

			switch (keyword (buf1, attrs, N_ELEMENTS (attrs))) {
				int countdown;

			case 0: /* active */
				active = parse_time (buf2);
			       	if (active < 0)
					goto failure;
				break;

			case 1: /* countdown */
				countdown = parse_time (buf2);
				if (countdown < 0)
					goto failure;
				t->fire_time = current_time
					+ countdown * (1 / 25.0);
				break;

			case 2: /* delete */
				t->_delete = TRUE;
				break;

                        case 3: /* expires */
				t->link.expires = parse_date (buf2);
				if (t->link.expires < 0.0)
					goto failure;
				break;

			case 4: /* name */
				t->link.name = vbi3_strndup_locale_utf8
					(buf2, VBI3_NUL_TERMINATED);
				if (!t->link.name)
					goto failure;
				break;

                        case 5: /* priority */
				t->link.priority = strtoul (buf2, NULL, 10);
				if ((unsigned int) t->link.priority > 9)
					goto failure;
				break;

			case 6: /* script */
				t->link.script = strdup (buf2);
				if (!t->link.script)
					goto failure;
				break;

			default:
				/* ignored */
				break;
			}

			break;
		}
		
		default:
			goto failure;
		}
	}

	if (!t->link.url)
		goto failure;

	/* NB EACEM implies PAL/SECAM land, 25 fps */
	if (t->link.expires <= 0.0)
		t->link.expires = t->fire_time + active * (1 / 25.0);

	if (0 == strncmp (t->link.url, "http://", 7)) {
		t->link.type = VBI3_LINK_HTTP;
	} else if (0 == strncmp (t->link.url, "lid://", 6)) {
		t->link.type = VBI3_LINK_LID;
	} else if (0 == strncmp (t->link.url, "tw://", 5)) {
		t->link.type = VBI3_LINK_TELEWEB;
	} else if (0 == strncmp (t->link.url, "ttx://", 6)) {
		int cni;

		cni = parse_hex (t->link.url + 6, 4);
		if (cni < 0 || '/' != t->link.url[10])
			goto failure;

		t->link.pgno = parse_hex (t->link.url + 11, 3);
		if (t->link.pgno < 0x100
		    || t->link.pgno > 0x8FF
		    || '/' != t->link.url[14])
			goto failure;

		t->link.subno = parse_hex (t->link.url + 15, 4);
		if (t->link.subno < 0)
			goto failure;

		if (cni > 0) {
			vbi3_bool r;

			switch (cni >> 8) {
			case 0x04: /* Switzerland */
			case 0x07: /* Ukraine */
			case 0x0A: /* Austria */
			case 0x0D: /* Germany */
				r = vbi3_network_set_cni (&t->target_network,
							 VBI3_CNI_TYPE_VPS,
							 cni);
				break;

			default:
				r = vbi3_network_set_cni (&t->target_network,
							 VBI3_CNI_TYPE_8301,
							 cni);
				break;
			}

			if (!r)
				goto failure;

			t->link.network = &t->target_network;
		} else {
			t->link.network = &t->source_network;
		}

		t->link.type = VBI3_LINK_PAGE;
	} else {
		goto failure;
	}

	return s;

	failure:
	_vbi3_trigger_destroy (t);
	return NULL;
}

static const uint8_t *
_vbi3_trigger_from_atvef		(_vbi3_trigger *		t,
				 const uint8_t *	s,
				 const vbi3_network *	nk,
				 double			current_time)
{
	const uint8_t *s1;

	if (!_vbi3_trigger_init (t, nk, current_time))
		return NULL;

	for (s1 = s;; ++s) {
		const uint8_t *begin;
		int c;

		begin = s;

		switch ((c = *s)) {
		case 0:
			break;

		case '<':
			if (s != s1)
				goto failure;

			for (++s; '>' != (c = *s); ++s) {
				if (0 == c)
					goto failure;
			}

			t->link.url = strndup (begin + 1, s - begin - 1);
			if (!t->link.url)
				goto failure;

			break;

		case '[':
		{
			static const char *attrs [] = {
				"auto",
				"expires",
				"name",
				"script",
				"type" /* "t" */,
				"time",
				"tve",
				"tve-level",
				"view" /* "v" */
			};
			uint8_t buf1[256];
			uint8_t buf2[256];
			int delimiter;

			s = parse_string (buf1, sizeof (buf1), s, ']');
			if (!s || 0 == buf1[0])
				goto failure;

			if (':' != *s++) {
				int chks;

				chks = strtoul (buf1, NULL, 16);
				if (!verify_checksum (s1, begin - s1, chks)) {
					if (0)
						fprintf (stderr,
							 "bad checksum\n");
					goto failure;
				}

				break;
			}

			s = parse_quoted (buf2, sizeof (buf2), s, delimiter);
			if (!s || 0 == buf2[0])
				goto failure;

			switch (keyword (buf1, attrs, N_ELEMENTS (attrs))) {
			case 0: /* auto */
				t->link.autoload = parse_bool (buf2);
				break;

			case 1: /* expires */
				t->link.expires = parse_date (buf2);
				if (t->link.expires < 0.0)
					goto failure;
				break;

			case 2: /* name */
				t->link.name = vbi3_strndup_locale_utf8
					(buf2, VBI3_NUL_TERMINATED);
				if (!t->link.name)
					goto failure;
				break;

			case 3: /* script */
				t->link.script = strdup (buf2);
				if (!t->link.script)
					goto failure;
				break;

			case 4: /* type */
			{
				static const char *types [] = {
					"program",
					"network",
					"station",
					"sponsor",
					"operator",
				};

				/* -1 -> VBI3_WEBLINK_UNKNOWN */
				t->link.itv_type =
					1 + keyword (buf2, types,
						     N_ELEMENTS (types));
				break;
			}

			case 5: /* time */
				t->fire_time = parse_date (buf2);
				if (t->fire_time < 0.0)
					goto failure;
				break;

			case 6: /* tve */
			case 7: /* tve-level */
				/* ignored */
				break;

			case 8: /* view (tve == v) */
				t->view = buf2[0];
				break;

			default:
				/* ignored */
				break;
			}
		}

		default:
			goto failure;
		}
	}

	if (!t->link.url)
		goto failure;

	if (0 == strncmp (t->link.url, "http://", 7)) {
		t->link.type = VBI3_LINK_HTTP;
	} else if (0 == strncmp (t->link.url, "lid://", 6)) {
		t->link.type = VBI3_LINK_LID;
	} else {
		goto failure;
	}

	return s;

 failure:
	_vbi3_trigger_destroy (t);
	return NULL;
}

/**
 * @internal
 * @param list List head.
 * 
 * Deletes a list of triggers.
 */
void
_vbi3_trigger_list_delete	(_vbi3_trigger **	list)
{
	_vbi3_trigger *t;

	while ((t = *list)) {
		*list = t->next;
		_vbi3_trigger_delete (t);
	}
}

/**
 * @internal
 * @param list List head.
 * @param handlers Call these handlers.
 * @param current_time Current time in seconds since epoch.
 *
 * This function must be called at regular intervals,
 * preferably once per video frame, to fire (send a trigger
 * event) previously received triggers which reached their
 * fire time.
 */
unsigned int
_vbi3_trigger_list_fire		(_vbi3_trigger **	list,
				 _vbi3_event_handler_list *handlers,
				 double		current_time)
{
	_vbi3_trigger *t;
	unsigned int count;

	count = 0;

	while ((t = *list)) {
		if (t->fire_time <= current_time) {
			vbi3_event e;

			e.type		= VBI3_EVENT_TRIGGER;
			e.network	= &t->source_network;
			e.timestamp	= current_time;
			e.ev.trigger	= &t->link;

			_vbi3_event_handler_list_send (handlers, &e);           

			*list = t->next;
			_vbi3_trigger_delete (t);

			++count;
		} else {
			list = &t->next;
		}
	}

	return count;
}

static vbi3_bool
add_trigger			(_vbi3_trigger **	list,
				 _vbi3_event_handler_list *handlers,
				 _vbi3_trigger *		t1,
				 double			current_time)
{
	_vbi3_trigger *t;

	t = t1;

	if (t->_delete) {
		_vbi3_trigger **l;

		for (l = list; (t = *l); l = &t->next) {
			if (0 == strcmp (t1->link.url, t->link.url)
			    && fabs (t1->fire_time - t->fire_time) < 0.1) {
				*l = t->next;
				_vbi3_trigger_delete (t);
			} else {
				l = &t->next;
			}
		}

		_vbi3_trigger_destroy (t1);

		return TRUE;
	}

	for (t = *list; t; t = t->next) {
		if (0 == strcmp (t1->link.url, t->link.url)
		    && fabs (t1->fire_time - t->fire_time) < 0.1) {
			_vbi3_trigger_destroy (t1);
			return TRUE; /* retransmitted; is already listed */
		}
	}

	if (t1->fire_time <= current_time) {
		vbi3_event e;

		e.type		= VBI3_EVENT_TRIGGER;
		e.network	= &t1->source_network;
		e.timestamp	= current_time;
		e.ev.trigger	= &t1->link;

		_vbi3_event_handler_list_send (handlers, &e);           

		_vbi3_trigger_destroy (t1);

		return TRUE;
	}

	if (!(t = vbi3_malloc (sizeof (*t))))
		return FALSE;

	*t = *t1;

	t->next = *list;
	*list = t;

	return TRUE;
}

/**
 * @internal
 * @param s NUL terminated EACEM string (Latin-1). XXX better make that
 *   UCS-2.
 * 
 * Parse an EACEM trigger string and add it to the trigger list, where it
 * may fire immediately or at a later time.
 */
vbi3_bool
_vbi3_trigger_list_add_eacem	(_vbi3_trigger **	list,
				 _vbi3_event_handler_list *handlers,
				 const uint8_t *	s,
				 const vbi3_network *	nk,
				 double			current_time)
{
	_vbi3_trigger t;

	while ((s = _vbi3_trigger_from_eacem (&t, s, nk, current_time))) {
		if (0)
			_vbi3_trigger_dump (&t, stderr, "EACEM", current_time);

		t.link.eacem = TRUE;

		if (VBI3_LINK_LID == t.link.type
		    || VBI3_LINK_TELEWEB == t.link.type) {
			_vbi3_trigger_destroy (&t);
			return FALSE;
		}

		if (!add_trigger (list, handlers, &t, current_time))
			return FALSE;
	}

	return TRUE;
}

/**
 * @internal
 * @param s NUL terminated ATVEF string (ASCII).
 * 
 * Parse an ATVEF trigger string and add it to the trigger list, where it
 * may fire immediately or at a later time.
 */
vbi3_bool
_vbi3_trigger_list_add_atvef	(_vbi3_trigger **	list,
				 _vbi3_event_handler_list *handlers,
				 const uint8_t *	s,
				 const vbi3_network *	nk,
				 double			current_time)
{
	_vbi3_trigger t;

	if (_vbi3_trigger_from_atvef (&t, s, nk, current_time)) {
		if (0)
			_vbi3_trigger_dump (&t, stderr, "ATVEF", current_time);

		t.link.eacem = FALSE;

		if ('t' == t.view /* WebTV */
		    || strchr (t.link.url, '*') /* trigger matching */
		    || VBI3_LINK_LID == t.link.type) {
			_vbi3_trigger_destroy (&t);
			return FALSE;
		}

		if (!add_trigger (list, handlers, &t, current_time))
			return FALSE;
	}

	return TRUE;
}
