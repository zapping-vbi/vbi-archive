/*
 *  libzvbi -- Miscellaneous cows and chickens
 *
 *  Copyright (C) 2000-2003 Iñaki García Etxebarria
 *  Copyright (C) 2001-2008 Michael H. Schimek
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the 
 *  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, 
 *  Boston, MA  02110-1301  USA.
 */

/* $Id: misc.c,v 1.1.2.2 2008-08-22 07:59:20 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <ctype.h>
#include <errno.h>

#include "misc.h"

const char vbi_intl_domainname[] = PACKAGE;

void * (* vbi_malloc)(size_t) = malloc;
void * (* vbi_realloc)(void *, size_t) = realloc;
char * (* vbi_strdup)(const char *) = strdup;
void   (* vbi_free)(void *) = free;
void * (* vbi_cache_malloc)(size_t) = malloc;
void   (* vbi_cache_free)(void *) = free;

#if 0
_vbi_log_hook		_vbi_global_log;
#endif

/**
 * @internal
 * Number of set bits.
 */
unsigned int
_vbi_popcnt			(uint32_t		x)
{
	x -= ((x >> 1) & 0x55555555);
	x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
	x = (x + (x >> 4)) & 0x0F0F0F0F;

	return ((uint32_t)(x * 0x01010101)) >> 24;
}

/**
 * @internal
 * @param dst The string will be stored in this buffer.
 * @param src NUL-terminated string to be copied.
 * @param size Maximum number of bytes to be copied, including the
 *   terminating NUL (i.e. this is the size of the @a dst buffer).
 *
 * Copies @a src to @a dst, but no more than @a size - 1 characters.
 * Always NUL-terminates @a dst, unless @a size is zero.
 *
 * strlcpy() is a BSD extension. Don't call this function
 * directly, we #define strlcpy if necessary.
 *
 * @returns
 * strlen (src).
 */
size_t
_vbi_strlcpy			(char *			dst,
				 const char *		src,
				 size_t			size)
{
	const char *src1;

	assert (NULL != dst);
	assert (NULL != src);

	src1 = src;

	if (likely (size > 1)) {
		char *end = dst + size - 1;

		do {
			if (unlikely (0 == (*dst++ = *src++)))
				goto finish;
		} while (dst < end);

		*dst = 0;
	} else if (size > 0) {
		*dst = 0;
	}

	while (*src++)
		;

 finish:
	return src - src1 - 1;
}

/**
 * @internal
 * strndup() is a BSD/GNU extension. Don't call this function
 * directly, we #define strndup if necessary.
 */
char *
_vbi_strndup			(const char *		s,
				 size_t			len)
{
	size_t n;
	char *r;

	if (NULL == s)
		return NULL;

	n = strlen (s);
	len = MIN (len, n);

	r = vbi_malloc (len + 1);

	if (r) {
		memcpy (r, s, len);
		r[len] = 0;
	}

	return r;
}

/**
 * @internal
 * vasprintf() is a BSD/GNU extension. Don't call this function
 * directly, we #define vasprintf if necessary.
 */
int
_vbi_vasprintf			(char **		dstp,
				 const char *		templ,
				 va_list		ap)
{
	char *buf;
	unsigned long size;
	va_list ap2;
	int temp;

	assert (NULL != dstp);
	assert (NULL != templ);

	temp = errno;

	buf = NULL;
	size = 64;

	__va_copy (ap2, ap);

	for (;;) {

		char *buf2;
		long len;

		if (!(buf2 = vbi_realloc (buf, size)))
			break;

		buf = buf2;

		len = vsnprintf (buf, size, templ, ap);

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

		/* vsnprintf() may advance ap. */
		__va_copy (ap, ap2);
	}

	vbi_free (buf);
	buf = NULL;

	/* According to "man 3 asprintf" GNU's version leaves *dstp
	   undefined on error, so don't count on it. FreeBSD's
	   asprintf NULLs *dstp, which is safer. */
	*dstp = NULL;
	errno = temp;

	return -1;
}

/**
 * @internal
 * asprintf() is a GNU extension. Don't call this function
 * directly, we #define asprintf if necessary.
 */
int
_vbi_asprintf			(char **		dstp,
				 const char *		templ,
				 ...)
{
	va_list ap;
	int len;

	va_start (ap, templ);

	/* May fail, returning -1. */
	len = vasprintf (dstp, templ, ap);

	va_end (ap);

	return len;
}

/**
 * @internal
 */
vbi_bool
_vbi_shrink_vector_capacity	(void **		vector,
				 size_t *		capacity,
				 size_t			min_capacity,
				 size_t			element_size)
{
	void *new_vec;

	if (min_capacity >= *capacity)
		return TRUE;

	new_vec = vbi_realloc (*vector, min_capacity * element_size);
	if (unlikely (NULL == new_vec))
		return FALSE;

	*vector = new_vec;
	*capacity = min_capacity;

	return TRUE;
}

/**
 * @internal
 */
vbi_bool
_vbi_grow_vector_capacity	(void **		vector,
				 size_t *		capacity,
				 size_t			min_capacity,
				 size_t			element_size)
{
	void *new_vec;
	size_t old_capacity;
	size_t new_capacity;
	size_t max_capacity;

	assert (min_capacity > 0);
	assert (element_size > 0);

	max_capacity = SIZE_MAX / element_size;

	if (unlikely (min_capacity > max_capacity)) {
		goto failed;
	}

	old_capacity = *capacity;

	/* If old_capacity < 64K we double the capacity,
	   otherwise we add 64K, but only up to max_capacity. */
	if (unlikely (old_capacity >= (1 << 16))) {
		if (unlikely (max_capacity < (1 << 16)
			      || old_capacity > max_capacity - (1 << 16)))
			new_capacity = max_capacity;
		else
			new_capacity = MAX (min_capacity,
					    old_capacity + (1 << 16));
	} else {
		new_capacity = MAX (min_capacity, old_capacity * 2);
		new_capacity = MIN (new_capacity, max_capacity);
	}

	new_vec = vbi_realloc (*vector, new_capacity * element_size);
	if (unlikely (NULL == new_vec)) {
		if (new_capacity <= min_capacity)
			goto failed;

		new_capacity = min_capacity;

		new_vec = vbi_realloc (*vector, new_capacity * element_size);
		if (unlikely (NULL == new_vec))
			goto failed;
	}

	*vector = new_vec;
	*capacity = new_capacity;

	return TRUE;

 failed:
	errno = ENOMEM;

	return FALSE;
}

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
