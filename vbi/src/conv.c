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

/* $Id: conv.c,v 1.1.2.7 2006-05-18 16:49:19 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <errno.h>
#include <langinfo.h>

#include "misc.h"
#ifdef ZAPPING8
#  include "common/intl-priv.h"
#else
#  include "intl-priv.h"
#endif
#include "conv.h"

const char vbi3_intl_domainname [] = PACKAGE;

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

vbi3_bool
_vbi3_iconv_ucs2		(iconv_t		cd,
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

	assert ((iconv_t) -1 != cd);
	assert (NULL != dst);

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

	r = iconv_ucs2 (cd, dst, &d_left, &s, &s_left);

	return ((size_t) -1 != r && 0 == s_left);
}

/**
 * @internal
 * @param cd Conversion object returned by vbi3_iconv_open().
 *
 * Frees all resources associated with the conversion object.
 */
void
_vbi3_iconv_close		(iconv_t		cd)
{
	if ((iconv_t) -1 != cd)
		iconv_close (cd);
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
 * (iconv_t) -1 when the conversion is impossible.
 */
iconv_t
_vbi3_iconv_open		(const char *		dst_codeset,
				 const char *		src_codeset,
				 char **		dst,
				 size_t			dst_size)
{
	iconv_t cd;

	if (NULL == dst_codeset)
		dst_codeset = "UTF-8";

	if (NULL == src_codeset)
		src_codeset = "UCS-2";

	cd = iconv_open (dst_codeset, src_codeset);

	if ((iconv_t) -1 == cd)
		return cd;

	if (NULL != dst) {
		size_t n;

		/* Write out the byte sequence to get into the
		   initial state if this is necessary. */
		n = iconv (cd, NULL, NULL, dst, &dst_size);

		if ((size_t) -1 == n) {
			iconv_close (cd);
			cd = (iconv_t) -1;
		}
	}

	return cd;
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

	return buffer;
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
		/* Common case UCS-2 -> UTF-8. */
		if (same_codeset (dst_codeset, "UTF8")) {
			return strndup_utf8_ucs2 (out_size,
						  (const uint16_t *) src,
						  src_size);
		} else {
			return strndup_iconv_ucs2 (out_size,
						   dst_codeset,
						   (const uint16_t *) src,
						   src_size / 2);
		}
	}

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
				 const vbi3_character_set *cs)
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
















#if 0

/**
 * @param cd Conversion object returned by vbi3_iconv_ucs2_open().
 * @param dst Pointer to output buffer pointer, will be incremented
 *   by the number of bytes written.
 * @param dst_size Space available in the output buffer, in bytes.
 * @param src Source string in UCS-2 format, can be NULL.
 * @param src_size Number of characters (not bytes) in the source
 *   string. Can be -1 if the string is NUL terminated.
 *
 * Converts UCS-2 coded text (as in vbi3_page) to another format
 * and stores it in the output buffer. Characters not representable
 * in the target format are converted to spaces 0x0020.
 *
 * @returns
 * %c FALSE on failure, for example @a dst_size is too small. Output
 * buffer contents are undefined on failure.
 */
vbi3_bool
vbi3_iconv_ucs2			(iconv_t		cd,
				 char **		dst,
				 unsigned long		dst_size,
				 const uint16_t *	src,
				 long			src_size)
{
	static const uint16_t dummy[1] = { 0 };
	const char *s;
	size_t d_left;
	size_t s_left;
	size_t r;

	assert ((iconv_t) -1 != cd);
	assert (NULL != dst);

	if (NULL == src) {
		s = (const char *) dummy;
		s_left = 0;
	} else {
		if (src_size < 0)
			src_size = ucs2_strlen (src);

		s = (const char *) src;
		s_left = src_size * 2;
	}

	d_left = dst_size;

	r = xiconv (cd, &s, &s_left, dst, &d_left, 2);

	return ((size_t) -1 != r && 0 == s_left);
}

/**
 * @param cd Conversion object returned by vbi3_iconv_ucs2_open().
 * @param dst Pointer to output buffer pointer, will be incremented
 *   by the number of bytes written.
 * @param dst_size Space available in the output buffer, in bytes.
 * @param unicode Single 16 bit Unicode character.
 *
 * Converts an UCS-2 character (as in vbi3_page) to another format
 * and stores it in the output buffer. Characters not representable
 * in the target format are converted to spaces 0x0020.
 *
 * @returns
 * %c FALSE on failure, for example @a dst_size is too small. Output
 * buffer contents are undefined on failure.
 */
vbi3_bool
vbi3_iconv_unicode		(iconv_t		cd,
				 char **		dst,
				 unsigned long		dst_size,
				 unsigned int		unicode)
{
	uint16_t t[1];

	assert ((iconv_t) -1 != cd);
	assert (NULL != dst);

	t[0] = unicode;

	return vbi3_iconv_ucs2 (cd, dst, dst_size, t, 1);
}


/**
 * @internal
 * @param src String to be duplicated.
 *
 * Creates a duplicate of a NUL-terminated string encoded in the
 * character set used by gettext or the current locale.
 *
 * @returns
 * NUL terminated string. You must free() the string when no longer
 * needed. The function returns NULL when @a src is NULL or memory
 * is exhausted.
 */
char *
_vbi3_strdup_locale		(const char *		src)
{
  /* FIXME this is supposed to take multi-byte characters into account. */
	return strdup (src);
}

/**
 * @param dst_format Character set name for iconv() conversion,
 *   for example "ISO-8859-1". When @c NULL, the default is UTF-8.
 * @param src_format Character set name for iconv() conversion.
 *   When @c NULL, the default is UCS-2.
 * @param src Input buffer.
 * @param src_size Number of bytes in the input buffer.
 * @param char_size Source character size, @c 1 or @c 2 bytes.
 *
 * Converts a string with iconv() and stores the result with a
 * terminating NUL in a newly allocated buffer. The source is assumed
 * to be an UTF-8 string when char_size == 1, UCS-2 with native endianess
 * when char_size == 2.
 *
 * @returns
 * %c NULL on failure.
 */
static char *
strndup_iconv			(const char *		dst_format,
				 const char *		src_format,
				 const char *		src,
				 unsigned long		src_size,
				 unsigned int		char_size)
{
	char *buf;
	unsigned long buf_size;
	iconv_t cd;
	char *d;
	const char *s;
	size_t d_left;
	size_t s_left;

	buf = NULL;
	cd = (iconv_t) -1;

	if (NULL == src)
		goto failure;

	buf_size = src_size * 8;

	buf = vbi3_malloc (buf_size);
	if (NULL == buf)
		goto failure;

	s = (const char *) src;
	d = buf;

	cd = xiconv_open (dst_format, src_format, &d, buf_size);

	if ((iconv_t) -1 == cd)
		goto failure;

	s_left = src_size;
	d_left = buf_size - (d - buf);

	while (s_left > 0) {
		char *buf2;
		size_t r;

		r = xiconv (cd, &s, &s_left, &d, &d_left, char_size);

		if ((size_t) -1 != r)
			break;

		if (E2BIG != errno)
			goto failure;

		buf2 = vbi3_realloc (buf, buf_size * 2);
		if (NULL == buf2)
			goto failure;

		d = buf2 + (d - buf);
		d_left += buf_size * 2 - buf_size;

		buf = buf2;
		buf_size *= 2;
	}

	{
		char *buf2;

		buf2 = vbi3_realloc (buf, buf_size - d_left + 4);
		if (NULL == buf2)
			goto failure;

		buf = buf2;
	}

	memset (buf + (d - buf), 0, 4);

	iconv_close (cd);

	return buf;

 failure:
	if ((iconv_t) -1 != cd)
		iconv_close (cd);

	vbi3_free (buf);

	return NULL;
}

/**
 * @param dst_format Character set name for iconv() conversion,
 *   for example "ISO-8859-1". When @c NULL, the default is UTF-8.
 * @param src Source string in UCS-2 format, can be @c NULL.
 * @param src_size Number of characters (not bytes) in the
 *   source string. Can be -1 if the string is NUL terminated.
 *
 * Converts UCS-2 coded text (as in vbi3_page) to another format,
 * and stores the result in a newly allocated buffer. Characters not
 * representable in the @a dst_format are converted to spaces 0x0020.
 *
 * @returns
 * A NUL terminated string in the target encoding. You must free() the
 * string when it is no longer needed. The function returns an empty
 * string when @a src_size is 0, @c NULL on failure or when @a src
 * is @c NULL.
 */
char *
vbi3_strndup_iconv_ucs2		(const char *		dst_format,
				 const uint16_t *	src,
				 long			src_size)
{
	if (NULL == src)
		return NULL;

	if (src_size < 0)
		src_size = ucs2_strlen (src);

	return strndup_iconv (dst_format,
			      /* src_format: UCS-2 */ NULL,
			      (const char *) src,
			      src_size * 2,
			      /* char_size */ 2);
}

/**
 * @param src Source string in UCS-2 format, can be @c NULL.
 * @param src_size Number of characters (not bytes) in the
 *   source string. Can be -1 if the string is NUL terminated.
 *
 * Converts UCS-2 coded text (as in vbi3_page) to UTF-8 format and
 * stores it in a newly allocated buffer.
 *
 * @returns
 * A NUL terminated UTF-8 string. You must free() the string when it
 * is no longer needed. The function returns an empty string when
 * @a src_size is 0, @c NULL on failure or when @a src is @c NULL.
 */
char *
vbi3_strndup_utf8_ucs2		(const uint16_t *	src,
				 long			src_size)
{
	if (NULL == src)
		return NULL;

	if (src_size < 0)
		src_size = ucs2_strlen (src);

	return strndup_iconv (/* dst_format: UTF-8 */ NULL,
			      /* src_format: UCS-2 */ NULL,
			      (const char *) src,
			      src_size * 2,
			      /* char_size */ 2);
}

/**
 * @internal
 * @param src NUL-terminated string in UTF-8 format, can be @c NULL.
 * @param src_size Number of bytes in the
 *   source string. Can be -1 if the string is NUL terminated.
 *
 * Converts a UTF-8 coded string to the character set used by gettext
 * or the current locale and stores it in a newly allocated buffer.
 * Characters not representable in the output format are converted
 * to spaces 0x0020.
 *
 * @returns
 * A NUL terminated string in the target encoding. You must free() the
 * string when it is no longer needed. The function returns @c NULL on
 * failure or when @a src is @c NULL.
 */
char *
vbi3_strndup_locale_utf8	(const char *		src,
				 long			src_size)
{
	const char *dst_format;

	if (NULL == src)
		return NULL;

	dst_format = vbi3_locale_codeset ();

	if (src_size < 0) {
		if (NULL != dst_format
		    && 0 == strcmp (dst_format, "UTF-8"))
			return strdup (src);

		src_size = strlen (src);
	}

	return strndup_iconv (dst_format,
			      /* src_format */ "UTF-8",
			      src,
			      src_size,
			      /* char_size */ 1);
}

/**
 * @param src NUL-terminated string in UTF-8 format, can be @c NULL.
 *
 * Converts a UTF-8 coded string to an UCS-2 string and stores it in a
 * newly allocated buffer.
 *
 * @returns
 * A NUL terminated string in the target encoding. You must free() the
 * string when it is no longer needed. The function returns @c NULL on
 * failure or when @a src is @c NULL.
 */
uint16_t *
_vbi3_strdup_ucs2_utf8		(const char *		src)
{
	if (NULL == src)
		return NULL;

	return (uint16_t *) strndup_iconv (/* dst_format */ "UCS-2",
					   /* src_format */ "UTF-8",
					   src,
					   /* src_size */ strlen (src),
					   /* char_size */ 1);
}

/**
 * @param fp Output file.
 * @param dst_format Character set name for iconv() conversion,
 *   for example "ISO-8859-1". When @c NULL, the default is UTF-8.
 * @param src Source string in UCS-2 format, can be @c NULL.
 * @param src_size Number of characters (not bytes) in the
 *   source string. Can be -1 if the string is NUL terminated.
 *
 * Converts an UCS-2 coded text (as in vbi3_page) to another format,
 * and writes it into the given file. Characters not representable
 * in the output format are converted to spaces 0x0020.
 *
 * @returns
 * FALSE on failure.
 */
vbi3_bool
vbi3_fputs_iconv_ucs2		(FILE *			fp,
				 const char *		dst_format,
				 const uint16_t *	src,
				 long			src_size)
{
	char buffer[16];
	iconv_t cd;
	char *d;
	vbi3_bool success;

	assert (NULL != fp);

	if (NULL == src || 0 == src_size)
		return TRUE;

	d = buffer;

	cd = xiconv_open (dst_format,
			  /* src_format: UCS-2 */ NULL,
			  &d,
			  sizeof (buffer));

	if ((iconv_t) -1 == cd)
		return FALSE;

	if (d > buffer) {
		size_t r;
		size_t n;

		n = d - buffer;

		r = fwrite (buffer, 1, n, fp);

		if (n != r)
			return FALSE;
	}

	success = vbi3_fputs_cd_ucs2 (fp, cd, src, src_size);

	iconv_close (cd);

	return success;
}

/**
 * @param fp Output file.
 * @param cd Conversion object returned by vbi3_iconv_ucs2_open().
 * @param src Source string in UCS-2 format, can be @c NULL.
 * @param src_size Number of bytes in the
 *   source string. Can be -1 if the string is NUL terminated.
 *
 * Converts a UTF-8 coded text to the character set
 * used by gettext or the current locale, and writes it into the
 * given file. Characters not representable in the output format are
 * converted to spaces 0x0020.
 *
 * @returns
 * FALSE on failure.
 */
vbi3_bool
vbi3_fputs_locale_utf8		(FILE *			fp,
				 const char *		src,
				 long			src_size)
{
	char *buffer;
	iconv_t cd;
	const char *s;
	size_t s_left;
	vbi3_bool success;

	assert (NULL != fp);

	buffer = NULL;
	cd = (iconv_t) -1;
	success = FALSE;

	if (NULL == src || 0 == src_size)
		return TRUE;

	buffer = vbi3_malloc (4096);
	if (NULL == buffer)
		goto failure;

	cd = xiconv_open (vbi3_locale_codeset (),
			  /* src_format */ "UTF-8",
			  /* dst */ NULL,
			  /* dst_size */ 0);
	if ((iconv_t) -1 == cd)
		goto failure;

	s = (const char *) src;

	s_left = src_size;
	if (src_size < 0)
		s_left = strlen (src);

	while (s_left > 0) {
		char *d;
		size_t d_left;
		size_t r;
		size_t n;

		d = buffer;
		d_left = 4096;

		r = xiconv (cd, &s, &s_left, &d, &d_left, 1);

		if ((size_t) -1 == r) {
			if (E2BIG != errno)
				goto failure;
		}

		n = d - buffer;

		r = fwrite (buffer, 1, n, fp);

		if (n != r)
			goto failure;
	}

	success = TRUE;

 failure:
	if ((iconv_t) -1 != cd)
		iconv_close (cd);

	free (buffer);

	return success;
}

#endif
