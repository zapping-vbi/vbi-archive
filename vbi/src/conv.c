/*
 *  libzvbi - Unicode conversion helper functions
 *
 *  Copyright (C) 2003, 2004, 2006 Michael H. Schimek
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

/* $Id: conv.c,v 1.1.2.8 2006-05-26 00:43:05 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <errno.h>
#include <langinfo.h>

#include "misc.h"
#include "version.h"
#ifdef ZAPPING8
#  include "common/intl-priv.h"
#else
#  include "intl-priv.h"
#endif
#include "conv.h"

#if 3 == VBI_VERSION_MINOR
const char vbi3_intl_domainname [] = PACKAGE;
#else
const char *vbi3_intl_domainname = _zvbi_intl_domainname;
#endif

/**
 * @param src NUL-terminated UCS-2 string.
 *
 * Counts the characters in the string, excluding the terminating NUL.
 */
size_t
vbi3_strlen_ucs2		(const uint16_t *	src)
{
	const uint16_t *s;

	if (NULL == src)
		return 0;

	for (s = src; 0 != *s; ++s)
		;

	return s - src;
}

/**
 * @internal
 * @param out_size Length of the resulting UTF-8 string in bytes.
 * @param src NUL-terminated UCS-2 string.
 * @param src_size Number of characters in the
 *   source string. Can be -1 if the string is NUL terminated.
 *
 * Converts a UCS-2 string to UTF-8 and stores the result in a newly
 * allocated buffer.
 */
static char *
strndup_utf8_ucs2		(size_t *		out_size,
				 const uint16_t *	src,
				 ssize_t		src_size)
{
	char *buffer;
	char *d;

	assert (NULL != out_size);
	assert (NULL != src);
	assert (0 != src_size);

	if (src_size < 0)
		src_size = vbi3_strlen_ucs2 (src) + 1;

	buffer = vbi3_malloc (src_size * 3);
	if (NULL == buffer)
		return NULL;

	d = buffer;

	while (src_size > 0) {
		unsigned int c = *src++;

		if (c < 0x80) {
			*d++ = c;
		} else if (c < 0x800) {
			d[0] = 0xC0 | (c >> 6);
			d[1] = 0x80 | (c & 0x3F);
			d += 2;
		} else {
			d[0] = 0xE0 | (c >> 12);
			d[1] = 0x80 | ((c >> 6) & 0x3F);
			d[2] = 0x80 | (c & 0x3F);
			d += 3;
		}
	}

	*out_size = d - buffer;

	return buffer;
}

#ifdef HAVE_ICONV
#  include <iconv.h>
struct _vbi3_iconv_t {
	iconv_t			cd;
};
#else
struct _vbi3_iconv_t {
	int			dummy;
};
#endif

#ifdef HAVE_ICONV

/**
 * @internal
 * @param cd Conversion object.
 * @param dst Pointer to output buffer pointer, will be incremented
 *   by the number of bytes written.
 * @param dst_size Space available in the output buffer, in bytes.
 * @param src Pointer to input buffer pointer, will be incremented
 *   by the number of bytes read.
 * @param src_left Number of bytes left to read in the input buffer.
 *
 * Like iconv(), but converts unrepresentable characters to a
 * question mark 0x3F. The source is assumed to be UCS-2.
 *
 * @returns
 * See iconv().
 */
static size_t
iconv_ucs2			(iconv_t		cd,
				 char **		dst,
				 size_t *		dst_left,
				 const char **		src,
				 size_t *		src_left)
{
	size_t r;

	assert ((iconv_t) -1 != cd);
	assert (NULL != dst);
	assert (NULL != dst_left);
	assert (NULL != src);
	assert (NULL != src_left);

	while (*src_left > 0) {
		static const uint16_t ucs2_repl[1] = { 0x003F };
		const char *src1;
		size_t src_left1;

		/* iconv() source pointer may be defined as char **,
		   should be const char ** or const void **. Ignore
		   compiler warnings. */
		r = iconv (cd, (void *) src, src_left, dst, dst_left);
		if (likely ((size_t) -1 != r))
			break; /* success */

		if (EILSEQ != errno)
			break;

		src1 = (const char *) ucs2_repl;
		src_left1 = 2;

		r = iconv (cd, (void *) &src1, &src_left1, dst, dst_left);
		if (unlikely ((size_t) -1 == r))
			break;

		*src += 2;
		*src_left -= 2;
	}

	return r;
}

#endif /* HAVE_ICONV */

vbi3_bool
_vbi3_iconv_ucs2		(vbi3_iconv_t *		cd,
				 char **		dst,
				 size_t			dst_size,
				 const uint16_t *	src,
				 ssize_t		src_size)
{
	static const uint16_t dummy[1] = { 0 };
	const char *s;
	size_t d_left;
	size_t s_left;
	size_t r;

	assert (NULL != cd);
	assert (NULL != dst);

#ifdef HAVE_ICONV
	if (NULL == src) {
		s = (const char *) dummy;
		s_left = 0;
	} else {
		if (src_size < 0)
			src_size = vbi3_strlen_ucs2 (src) + 1;

		s = (const char *) src;
		s_left = src_size * 2;
	}

	d_left = dst_size;

	r = iconv_ucs2 (cd->cd, dst, &d_left, &s, &s_left);

	return ((size_t) -1 != r && 0 == s_left);
#else
	return FALSE;
#endif
}

/**
 * @internal
 * @param cd Conversion object returned by vbi3_iconv_open().
 *
 * Frees all resources associated with the conversion object.
 */
void
_vbi3_iconv_close		(vbi3_iconv_t *		cd)
{
#ifdef HAVE_ICONV
	if (NULL != cd) {
		if ((iconv_t) -1 != cd->cd) {
			iconv_close (cd->cd);
			cd->cd = (iconv_t) -1;
		}

		free (cd);
	}
#endif
}

/**
 * @param dst_codeset Character set name for iconv() conversion,
 *   for example "ISO-8859-1". When @c NULL, the default is UTF-8.
 * @param src_codeset Character set name for iconv() conversion,
 *   for example "ISO-8859-1". When @c NULL, the default is UTF-8.
 * @param dst Pointer to output buffer pointer, which will be
 *   incremented by the number of bytes written.
 * @param dst_size Space available in the output buffer, in bytes.
 *
 * Helper function to convert text. A start byte sequence will be
 * stored in @a dst if necessary.
 *
 * @returns
 * @c NULL when the conversion is impossible.
 */
vbi3_iconv_t *
_vbi3_iconv_open		(const char *		dst_codeset,
				 const char *		src_codeset,
				 char **		dst,
				 size_t			dst_size)
{
#ifdef HAVE_ICONV
	vbi3_iconv_t *cd;

	if (NULL == dst_codeset)
		dst_codeset = "UTF-8";

	if (NULL == src_codeset)
		src_codeset = "UCS-2";

	cd = malloc (sizeof (*cd));
	if (NULL == cd)
		return NULL;

	cd->cd = iconv_open (dst_codeset, src_codeset);
	if ((iconv_t) -1 == cd->cd) {
		free (cd);
		return NULL;
	}

	if (NULL != dst) {
		size_t n;

		/* Write out the byte sequence to get into the
		   initial state if this is necessary. */
		n = iconv (cd->cd, NULL, NULL, dst, &dst_size);

		if ((size_t) -1 == n) {
			iconv_close (cd->cd);
			cd->cd = (iconv_t) -1;
			free (cd);
			return NULL;
		}
	}

	return cd;

#else /* !HAVE_ICONV */
	return NULL;
#endif
}

static vbi3_bool
same_codeset			(const char *		dst_codeset,
				 const char *		src_codeset)
{
	assert (NULL != dst_codeset);
	assert (NULL != src_codeset);

	for (;;) {
		char d, s;

		d = *dst_codeset;
		s = *src_codeset;

		if (d == s) {
			if (0 == d)
				return TRUE;

			++dst_codeset;
			++src_codeset;
		} else if ('-' == d || '_' == d) {
			++dst_codeset;
		} else if ('-' == s || '_' == s) {
			++src_codeset;
		} else {
			return FALSE;
		}
	}
}

static char *
strndup_iconv_ucs2		(size_t *		out_size,
				 const char *		dst_codeset,
				 const uint16_t *	src,
				 ssize_t		src_size)
{
	char *buffer;
	char *d;
	size_t buffer_size;

	assert (NULL != out_size);
	assert (NULL != dst_codeset);
	assert (NULL != src);
	assert (0 != src_size);

	if (src_size < 0)
		src_size = vbi3_strlen_ucs2 (src) + 1;

	if (same_codeset (dst_codeset, "UTF8")) {
		return strndup_utf8_ucs2 (out_size,
					  (const uint16_t *) src,
					  src_size);
	}

#ifdef HAVE_ICONV
	buffer = NULL;
	buffer_size = 0;

	for (;;) {
		iconv_t cd;
		const char *s;
		size_t d_left;
		size_t s_left;
		size_t r;

		d_left = src_size * 4;
		if (buffer_size > 0)
			d_left = buffer_size * 2;

		d = vbi3_malloc (d_left);
		if (unlikely (NULL == d)) {
			errno = ENOMEM;
			return NULL;
		}

		buffer = d;
		buffer_size = d_left;

		cd = _vbi3_iconv_open (dst_codeset, "UCS-2", &d, d_left);
		if (unlikely ((iconv_t) -1 == cd)) {
			free (buffer);
			buffer = NULL;
			return NULL;
		}

		d_left = buffer_size - (d - buffer);

		s = (const char *) src;
		s_left = src_size * 2;

		r = iconv_ucs2 (cd, &d, &d_left, &s, &s_left);

		iconv_close (cd);
		cd = (iconv_t) -1;

		if (likely ((size_t) -1 != r))
			break;

		free (buffer);
		buffer = NULL;

		if (E2BIG != errno)
			return NULL;
	}

	*out_size = d - buffer;
#else
	buffer = NULL;
	*out_size = 0;
#endif /* !HAVE_ICONV */

	return buffer;
}

/**
 * @internal
 */
static char *
strndup_iconv			(size_t *		out_size,
				 const char *		dst_codeset,
				 const char *		src_codeset,
				 const char *		src,
				 ssize_t		src_size)
{
	char *buffer;
	char *d;
	size_t buffer_size;

	assert (NULL != out_size);
	assert (NULL != dst_codeset);
	assert (NULL != src_codeset);
	assert (NULL != src);
	assert (src_size > 0);

	if (same_codeset (src_codeset, "UCS2")) {
		return strndup_iconv_ucs2 (out_size,
					   dst_codeset,
					   (const uint16_t *) src,
					   src_size / 2);
	}

#ifdef HAVE_ICONV
	buffer = NULL;
	buffer_size = 0;

	for (;;) {
		iconv_t cd;
		const char *s;
		size_t d_left;
		size_t s_left;
		size_t r;

		d_left = 16384;
		if (buffer_size > 0)
			d_left = buffer_size * 2;

		d = vbi3_malloc (d_left);
		if (unlikely (NULL == d)) {
			errno = ENOMEM;
			return NULL;
		}

		buffer = d;
		buffer_size = d_left;

		cd = _vbi3_iconv_open ("UCS-2", src_codeset, &d, d_left);
		if (unlikely ((iconv_t) -1 == cd)) {
			free (buffer);
			buffer = NULL;
			return NULL;
		}

		d_left = buffer_size - (d - buffer);

		s = src;
		s_left = src_size;

		/* Ignore compiler warnings if second argument is declared
		   as char** instead of const char**. */
		r = iconv (cd, &s, &s_left, &d, &d_left);

		iconv_close (cd);
		cd = (iconv_t) -1;

		if (likely ((size_t) -1 != r))
			break;

		free (buffer);
		buffer = NULL;

		if (E2BIG != errno)
			return NULL;
	}

	if (same_codeset (dst_codeset, "UCS2")) {
		*out_size = (d - buffer) / 2;
		return buffer;
	}

	d = strndup_iconv_ucs2 (out_size,
				dst_codeset,
				(const uint16_t *) buffer,
				(d - buffer) / 2);

	free (buffer);
	buffer = NULL;
#else
	d = NULL;
#endif /* !HAVE_ICONV */

	return d;
}

/**
 * @param dst_codeset Character set name for iconv() conversion,
 *   for example "ISO-8859-1". When @c NULL, the default is UTF-8.
 * @param src_codeset Character set name for iconv() conversion,
 *   for example "ISO-8859-1". When @c NULL, the default is UTF-8.
 * @param src Source buffer, can be @c NULL.
 * @param src_size Number of bytes in the source buffer.
 *
 * Converts a string with iconv() and writes the result into a
 * newly allocated buffer. Characters not representable in the
 * @a dst_codeset are converted to question marks 0x3F.
 *
 * @returns
 * A pointer to the allocated buffer. You must free() the buffer
 * when it is no longer needed. The function returns @c NULL when
 * the conversion fails, when it runs out of memory, when @a src
 * is @c NULL or when @a src_size is zero.
 */
char *
vbi3_strndup_iconv		(const char *		dst_codeset,
				 const char *		src_codeset,
				 const char *		src,
				 ssize_t		src_size)
{
	char *result;

	assert (src_size >= 0);

	if (NULL == src || 0 == src_size)
		return NULL;

	if (NULL == dst_codeset)
		dst_codeset = "UTF-8";

	if (NULL == src_codeset)
		src_codeset = "UTF-8";

	if (same_codeset (dst_codeset, src_codeset)) {
		result = vbi3_malloc (src_size);
		if (NULL == result)
			return NULL;

		memcpy (result, src, src_size);
	} else {
		char *buffer;
		size_t size;

		buffer = strndup_iconv (&size,
					dst_codeset,
					src_codeset,
					src, src_size);
		if (NULL == buffer)
			return NULL;

		result = realloc (buffer, size);
		if (NULL == result)
			result = buffer;
	}

	return result;
}

/**
 * @param dst_codeset Character set name for iconv() conversion,
 *   for example "ISO-8859-1". When @c NULL, the default is UTF-8.
 * @param src Source string in UCS-2 format, can be @c NULL.
 * @param src_size Number of characters (not bytes) in the source
 *   string. Can be -1 if the string is NUL terminated. In this
 *   case the NUL is also converted.
 *
 * Converts a UCS-2 string with iconv() and writes the result into
 * a newly allocated buffer. Characters not representable in the
 * @a dst_codeset are converted to question marks 0x3F.
 *
 * @returns
 * A pointer to the allocated buffer. You must free() the buffer
 * when it is no longer needed. The function returns @c NULL when
 * the conversion fails, when it runs out of memory or when @a src
 * is @c NULL. When @a src_size is zero it returns an empty string.
 */
char *
vbi3_strndup_iconv_ucs2		(const char *		dst_codeset,
				 const uint16_t *	src,
				 ssize_t		src_size)
{
	static const uint16_t nul[1] = { 0 };
	char *buffer;
	char *result;
	size_t size;

	if (NULL == src)
		return NULL;

	if (NULL == dst_codeset)
		dst_codeset = "UTF-8";

	if (0 == src_size) {
		src = nul;
		src_size = 1;
	}

	buffer = strndup_iconv_ucs2 (&size, dst_codeset, src, src_size);
	if (NULL == buffer)
		return NULL;

	result = realloc (buffer, size);
	if (NULL == result)
		result = buffer;

	return result;
}

/**
 * @param dst_codeset Character set name for iconv() conversion,
 *   for example "ISO-8859-1". When @c NULL, the default is UTF-8.
 * @param src String of teletext characters, can be @c NULL.
 * @param src_size Number of characters (= bytes) in the
 *   source string. Can be -1 if the @a src string is NUL terminated.
 * @param cs Teletext character set descriptor.
 *
 * Converts a string of Teletext characters to another format and
 * stores the result in a newly allocated buffer. The function
 * ignores parity bits and removes leading and trailing control
 * code characters (0x00 ... 0x1F). Other characters not
 * representable in @a dst_format are converted to question marks
 * 0x3F.
 *
 * @returns
 * A NUL terminated string in the target codeset. You must free() the
 * string when it is no longer needed. The function returns @c NULL when
 * the conversion fails, when it runs out of memory or when @a src
 * is @c NULL. When @a src_size is zero it returns an empty string.
 */
char *
vbi3_strndup_iconv_teletext	(const char *		dst_codeset,
				 const uint8_t *	src,
				 ssize_t		src_size,
				 const vbi3_ttx_charset *cs)
{
	uint16_t *buffer;
	char *result;
	ssize_t begin;
	ssize_t end;
	ssize_t i;

	assert (NULL != cs);

	if (NULL == src)
		return NULL;

	if (src_size < 0)
		src_size = strlen ((const char *) src);

	for (begin = 0; begin < src_size; ++begin)
		if ((src[begin] & 0x7F) > 0x20)
			break;

	for (end = src_size; end > begin; --end)
		if ((src[end - 1] & 0x7F) > 0x20)
			break;

	if (end <= begin) {
		static const uint16_t nul[1] = { 0 };

		return vbi3_strndup_iconv_ucs2 (dst_codeset, nul, 1);
	}

	buffer = vbi3_malloc ((end - begin + 1) * sizeof (*buffer));
	if (NULL == buffer)
		return NULL;

	for (i = begin; i < end; ++i) {
		buffer[i - begin] = vbi3_teletext_unicode
			(cs->g0, cs->subset, (unsigned int)(src[i] & 0x7F));
	}

	buffer[i - begin] = 0;

	result = vbi3_strndup_iconv_ucs2 (dst_codeset, buffer,
					  end - begin + 1);

	free (buffer);
	buffer = NULL;

	return result;
}

/**
 * @param fp Output file.
 * @param dst_codeset Character set name for iconv() conversion,
 *   for example "ISO-8859-1". When @c NULL, the default is UTF-8.
 * @param src_codeset Character set name for iconv() conversion,
 *   for example "ISO-8859-1". When @c NULL, the default is UTF-8.
 * @param src Source buffer, can be @c NULL.
 * @param src_size Number of bytes in the source buffer.
 *
 * Converts a string with iconv() and writes the result into the
 * given file. Characters not representable in the @a dst_codeset
 * are converted to question marks 0x3F.
 *
 * @returns
 * FALSE on failure.
 */
vbi3_bool
vbi3_fputs_iconv		(FILE *			fp,
				 const char *		dst_codeset,
				 const char *		src_codeset,
				 const char *		src,
				 ssize_t		src_size)
{
	char *buffer;
	size_t size;
	size_t actual;

	assert (NULL != fp);
	assert (src_size >= 0);

	if (NULL == src || 0 == src_size)
		return TRUE;

	if (NULL == dst_codeset)
		dst_codeset = "UTF-8";

	if (NULL == src_codeset)
		src_codeset = "UTF-8";

	if (same_codeset (dst_codeset, src_codeset)) {
		return ((size_t) src_size == fwrite (src, 1, src_size, fp));
	}

	buffer = strndup_iconv (&size,
				dst_codeset,
				src_codeset,
				src, src_size);
	if (NULL == buffer)
		return FALSE;

	actual = fwrite (buffer, 1, size, fp);

	free (buffer);
	buffer = NULL;

	return (actual == size);
}

/**
 * @param fp Output file.
 * @param dst_codeset Character set name for iconv() conversion,
 *   for example "ISO-8859-1". When @c NULL, the default is UTF-8.
 * @param src Source string in UCS-2 format, can be @c NULL.
 * @param src_size Number of characters (not bytes) in the source
 *   string. Can be -1 if the string is NUL terminated.
 *
 * Converts a UCS-2 string with iconv() and writes the result into
 * the given file. Characters not representable in the @a dst_codeset
 * are converted to question marks 0x3F.
 *
 * @returns
 * FALSE on failure.
 */
vbi3_bool
vbi3_fputs_iconv_ucs2		(FILE *			fp,
				 const char *		dst_codeset,
				 const uint16_t *	src,
				 ssize_t		src_size)
{
	char *buffer;
	size_t size;
	size_t actual;

	assert (NULL != fp);

	if (NULL == src)
		return TRUE;

	if (src_size < 0)
		src_size = vbi3_strlen_ucs2 (src);

	if (0 == src_size)
		return TRUE;

	if (NULL == dst_codeset)
		dst_codeset = "UTF8";

	buffer = strndup_iconv_ucs2 (&size,
				     dst_codeset,
				     src, src_size);
	if (NULL == buffer)
		return FALSE;

	actual = fwrite (buffer, 1, size, fp);

	free (buffer);
	buffer = NULL;

	return (actual == size);
}

/**
 * @returns
 * The character encoding used by the current locale, for example
 * "UTF-8". @c NULL if unknown.
 */
const char *
vbi3_locale_codeset		(void)
{
	const char *dst_format;

	dst_format = bind_textdomain_codeset (vbi3_intl_domainname, NULL);

	if (NULL == dst_format)
		dst_format = nl_langinfo (CODESET);

	return dst_format; /* may be NULL */
}
