/*
 *  libzvbi - Unicode conversion helper functions
 *
 *  Copyright (C) 2003, 2004 Michael H. Schimek
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

/* $Id: conv.c,v 1.1.2.6 2006-05-14 14:14:11 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <langinfo.h>
#include <assert.h>
#ifdef ZAPPING8
#  include "common/intl-priv.h"
#else
#  include "intl-priv.h"
#endif
#include "misc.h"		/* N_ELEMENTS() */
#include "conv.h"

const char vbi3_intl_domainname [] = PACKAGE;

/**
 * @internal
 * @param src NUL-terminated UCS-2 string.
 *
 * Counts the characters in the string, excluding the terminating NUL.
 */
static long
ucs2_strlen			(const uint16_t *	src)
{
	const uint16_t *src1;

	if (NULL == src)
		return 0;

	for (src1 = src; 0 != *src; ++src)
		;

	return src - src1;
}

/**
 * @param cd Conversion object returned by vbi3_iconv_ucs2_open().
 *
 * Frees all resources associated with the conversion object.
 */
void
vbi3_iconv_close		(iconv_t		cd)
{
	if ((iconv_t) -1 != cd)
		iconv_close (cd);
}

static iconv_t
xiconv_open			(const char *		dst_format,
				 const char *		src_format,
				 char **		dst,
				 unsigned int		dst_size)
{
	iconv_t cd;
	size_t n;

	if (NULL == dst_format)
		dst_format = "UTF-8";

	if (NULL == src_format)
		src_format = "UCS-2";

	cd = iconv_open (dst_format, src_format);

	if ((iconv_t) -1 == cd)
		return cd;

	if (NULL != dst) {
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

/**
 * @param dst_format Character set name for iconv() conversion,
 *   for example "ISO-8859-1". When @c NULL, the default is UTF-8.
 * @param dst Pointer to output buffer pointer, which will be
 *   incremented by the number of bytes written.
 * @param dst_size Space available in the output buffer, in bytes.
 *
 * Helper function to convert UCS-2 coded text (as in vbi3_page) to
 * another format. A start byte sequence will be stored in @a dst if
 * necessary.
 *
 * @returns
 * (iconv_t) -1 when the conversion is not possible.
 */
iconv_t
vbi3_iconv_open			(const char *		dst_format,
				 char **		dst,
				 unsigned long		dst_size)
{
	assert (NULL != dst);

	return xiconv_open (dst_format, NULL, dst, dst_size);
}

/**
 * @internal
 * @param cd Conversion object returned by xiconv_open().
 * @param src Pointer to input buffer pointer, will be incremented
 *   by the number of bytes read.
 * @param src_left Number of bytes left to read in the input buffer.
 * @param dst Pointer to output buffer pointer, will be incremented
 *   by the number of bytes written.
 * @param dst_size Space available in the output buffer, in bytes.
 * @param char_size Source character size, @c 1 or @c 2 bytes.
 *
 * Like iconv(), but converts unrepresentable characters to space
 * 0x0020. The source is assumed to be UTF-8 when char_size == 1,
 * UCS-2 with native endianess when char_size == 2.
 *
 * @returns
 * Number of characters converted, or (size_t) -1 on failure.
 */
static size_t
xiconv				(iconv_t		cd,
				 const char **		src,
				 size_t *		src_left,
				 char **		dst,
				 size_t *		dst_left,
				 unsigned int		char_size)
{
	size_t r;

	assert ((iconv_t) -1 != cd);
	assert (NULL != src);
	assert (NULL != src_left);
	assert (NULL != dst);
	assert (NULL != dst_left);

	for (;;) {
		static const uint16_t ucs2_space[1] = { 0x0020 };
		static const uint8_t utf8_space[1] = { 0x20 };
		const char *src1;
		size_t src_left1;

		/* iconv() source pointer may be defined as char **, should
		   be const char ** or const void **. Ignore compiler
		   warnings. */
		r = iconv (cd, (void *) src, src_left, dst, dst_left);

		if ((size_t) -1 != r)
			break;

		if (EILSEQ != errno)
			break;

		/* Replace unrepresentable character by space. */

		if (1 == char_size)
			src1 = (const char *) utf8_space;
		else
			src1 = (const char *) ucs2_space;

		src_left1 = char_size;

		r = iconv (cd, (void *) &src1, &src_left1, dst, dst_left);

		if ((size_t) -1 == r)
			break;

		if (1 == char_size) {
			do {
				++*src;
				--*src_left;
			} while (**src & 0x80);
		} else {
			*src += 2;
			*src_left -= 2;
		}
	}

	return r;
}

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
 * @param dst_format Character set name for iconv() conversion,
 *   for example "ISO-8859-1". When @c NULL, the default is UTF-8.
 * @param src String of teletext characters, can be @c NULL.
 * @param src_size Number of characters (= bytes) in the
 *   source string. Can be -1 if the @a src string is NUL terminated.
 * @param cs Teletext character set descriptor.
 *
 * Converts a string of Teletext characters to another format,
 * and stores the result in a newly allocated buffer. The function
 * ignores parity bits and removes leading and trailing control
 * code characters (0x00 ... 0x1F). Other characters not
 * representable in @a dst_format are converted to spaces 0x20.
 *
 * @returns
 * A NUL terminated string in the target encoding. You must free() the
 * string when it is no longer needed. The function returns an empty
 * string when @a src_size is 0, @c NULL on failure or when @a src is
 * @c NULL.
 */
char *
vbi3_strndup_iconv_teletext	(const char *		dst_format,
				 const uint8_t *	src,
				 long			src_size,
				 const vbi3_character_set *cs)
{
	uint16_t *buffer;
	char *result;
	long begin;
	long end;
	long i;

	assert (NULL != cs);

	if (NULL == src)
		return NULL;

	if (src_size < 0)
		src_size = strlen ((const char *) src);

	for (begin = 0; begin < src_size; ++begin)
		if ((src[begin] & 0x7F) > 0x20)
			break;

	for (end = src_size; end > 0; --end)
		if ((src[end - 1] & 0x7F) > 0x20)
			break;

	if (begin >= end)
		return calloc (1, 4); /* empty string */

	buffer = malloc (sizeof (*buffer) * (end - begin));
	if (NULL == buffer)
		return NULL;

	for (i = begin; i < end; ++i) {
		buffer[i - begin] = vbi3_teletext_unicode
			(cs->g0, cs->subset, (unsigned int)(src[i] & 0x7F));
	}

	result = vbi3_strndup_iconv_ucs2 (dst_format, buffer, end - begin);

	free (buffer);

	return result;
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
 * @param cd Conversion object returned by vbi3_iconv_ucs2_open().
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
vbi3_fputs_cd_ucs2		(FILE *			fp,
				 iconv_t		cd,
				 const uint16_t *	src,
				 long			src_size)
{
	char *buffer;
	const char *s;
	size_t s_left;
	vbi3_bool success;

	assert (NULL != fp);
	assert ((iconv_t) -1 != cd);

	success = FALSE;

	if (NULL == src || 0 == src_size)
		return TRUE;

	buffer = malloc (4096);
	if (NULL == buffer)
		return FALSE;

	s = (const char *) src;

	if (src_size < 0)
		src_size = ucs2_strlen (src);

	s_left = src_size * 2;

	while (s_left > 0) {
		char *d;
		size_t d_left;
		size_t r;
		size_t n;

		d = buffer;
		d_left = 4096;

		r = xiconv (cd, &s, &s_left, &d, &d_left, 2);

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
	free (buffer);
	buffer = NULL;

	return success;
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

	buffer = malloc (4096);
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
