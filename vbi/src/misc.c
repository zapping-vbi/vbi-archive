/*
 *  Copyright (C) 2001-2004 Michael H. Schimek
 *  Copyright (C) 2000-2003 Iñaki García Etxebarria
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

/* $Id: misc.c,v 1.4.2.3 2006-05-07 20:51:36 mschimek Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include "misc.h"

vbi3_log_fn *		vbi3_global_log_fn;
void *			vbi3_global_log_user_data;
vbi3_log_mask		vbi3_global_log_mask;

vbi3_bool
_vbi3_keyword_lookup		(int *			value,
				 const char **		inout_s,
				 const _vbi3_key_value_pair *table)
{
	const char *s;
	unsigned int i;

	assert (NULL != value);
	assert (NULL != inout_s);
	assert (NULL != *inout_s);
	assert (NULL != table);

	s = *inout_s;

	while (isspace (*s))
		++s;

	if (isdigit (*s)) {
		long val;
		char *end;

		val = strtol (s, &end, 10);

		for (i = 0; NULL != table[i].key; ++i) {
			if (val == table[i].value) {
				*value = val;
				*inout_s = end;
				return TRUE;
			}
		}
	} else {				 
		for (i = 0; NULL != table[i].key; ++i) {
			size_t len = strlen (table[i].key);

			if (0 == strncasecmp (s, table[i].key, len)
			    && !isalnum (s[len])) {
				*value = table[i].value;
				*inout_s = s + len;
				return TRUE;
			}
		}
	}

	return FALSE;
}

#ifndef HAVE_STRLCPY

/**
 * @internal
 * strlcpy() is a BSD/GNU extension.
 */
size_t
_vbi3_strlcpy			(char *			dst,
				 const char *		src,
				 size_t			len)
{
	char *dst1;
	char *end;
	char c;

	assert (NULL != dst);
	assert (NULL != src);
	assert (len > 0);

	dst1 = dst;

	end = dst + len - 1;

	while (dst < end && (c = *src++))
		*dst++ = c;

	*dst = 0;

	return dst - dst1;
}

#endif /* !HAVE_STRLCPY */

#ifndef HAVE_STRNDUP

/**
 * @internal
 * strndup() is a BSD/GNU extension.
 */
char *
_vbi3_strndup			(const char *		s,
				 size_t			len)
{
	size_t n;
	char *r;

	if (NULL == s)
		return NULL;

	n = strlen (s);
	len = MIN (len, n);

	r = malloc (len + 1);

	if (r) {
		memcpy (r, s, len);
		r[len] = 0;
	}

	return r;
}

#endif /* !HAVE_STRNDUP */

#ifndef HAVE_ASPRINTF

/**
 * @internal
 * asprintf() is a GNU extension.
 */
int
_vbi3_asprintf			(char **		dstp,
				 const char *		templ,
				 ...)
{
	char *buf;
	unsigned long size;
	int temp;

	assert (NULL != dstp);
	assert (NULL != templ);

	temp = errno;

	buf = NULL;
	size = 64;

	for (;;) {
		va_list ap;
		char *buf2;
		long len;

		if (!(buf2 = realloc (buf, size)))
			break;

		buf = buf2;

		va_start (ap, templ);
		len = vsnprintf (buf, size, templ, ap);
		va_end (ap);

		if (len < 0) {
			/* Not enough. */
			size *= 2;
		} else if ((unsigned long) len < size) {
			*dstp = buf;
			errno = temp;
			return len;
		} else {
			/* Size needed. */
			size = len + 1;
		}
	}

	free (buf);
	*dstp = NULL;
	errno = temp;

	return -1;
}

#endif /* !HAVE_ASPRINTF */
