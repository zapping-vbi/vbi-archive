/*
 *  libzvbi - Unicode conversion helper functions
 *
 *  Copyright (C) 2003-2004 Michael H. Schimek
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

/* $Id: conv.c,v 1.1.2.1 2004-02-25 17:35:27 mschimek Exp $ */

#include "../config.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <langinfo.h>

#include "conv.h"

const char vbi_intl_domainname [] = PACKAGE;

/**
 * @internal
 * @param cd Conversion object returned by vbi_iconv_ucs2_open().
 *
 * Frees all resources associated with the coversion object.
 */
void
vbi_iconv_ucs2_close		(iconv_t		cd)
{
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

	/* Write out the byte sequence to get into the
	   initial state if this is necessary. */
	n = iconv (cd, NULL, NULL, dst, &dst_size);

	if ((size_t) -1 == n) {
		iconv_close (cd);
		cd = (iconv_t) -1;
	}

	return cd;
}

/**
 * @internal
 * @param dst_format Character set name for iconv() conversion,
 *   for example "ISO-8859-1". When @c NULL, the default is UTF-8.
 * @param dst Pointer to output buffer pointer, will be incremented
 *   by the number of bytes written.
 * @param dst_size Space available in the output buffer, in bytes.
 *
 * Helper function to convert UCS-2 coded text (as in vbi_page) to
 * another format. A start byte sequence will be stored in @a dst if
 * necessary.
 *
 * @returns
 * (iconv_t) -1 when the conversion is not possible.
 */
iconv_t
vbi_iconv_ucs2_open		(const char *		dst_format,
				 char **		dst,
				 unsigned int		dst_size)
{
	return xiconv_open (dst_format, NULL, dst, dst_size);
}

/* Like iconv, but converts unrepresentable characters to space
   0x0020. Source is assumed to be UTF-8 (csize 1) or UCS-2
   (csize 2) with native endianess. */
static size_t
xiconv				(iconv_t		cd,
				 const char **		s,
				 size_t *		sleft,
				 char **		d,
				 size_t *		dleft,
				 unsigned int		csize)
{
	size_t r;

	for (;;) {
		const uint16_t ucs2_space[1] = { 0x0020 };
		const uint8_t utf8_space[1] = { 0x20 };
		const char *s1;
		size_t sleft1;

		/* iconv() source pointer may be defined as char **, should
		   be const char ** or const void **. Ignore warnings. */
		r = iconv (cd, s, sleft, d, dleft);

		if ((size_t) -1 != r)
			break;

		if (EILSEQ != errno)
			break;

		if (1 == csize)
			s1 = (const char *) utf8_space;
		else
			s1 = (const char *) ucs2_space;

		sleft1 = csize;

		r = iconv (cd, &s1, &sleft1, d, dleft);

		if ((size_t) -1 == r)
			break;

		if (1 == csize) {
			do {
				++*s;
				--*sleft;
			} while (**s & 0x80);
		} else {
			*s += 2;
			*sleft -= 2;
		}
	}

	return r;
}

/**
 * @internal
 * @param cd Conversion object returned by vbi_iconv_ucs_open().
 * @param dst Pointer to output buffer pointer, will be incremented
 *   by the number of bytes written.
 * @param dst_size Space available in the output buffer, in bytes.
 * @param src Source string in UCS-2 format.
 * @param src_size Number of characters (not bytes) in the
 *   source string.
 *
 * Converts UCS-2 coded text (as in vbi_page) to another format
 * and stores it in the output buffer. Characters not representable
 * in the target format are converted to spaces 0x0020.
 *
 * @return
 * FALSE on failure, including dst_size too small. Output buffer
 * contents are undefined on failure.
 */
vbi_bool
vbi_iconv_ucs2			(iconv_t		cd,
				 char **		dst,
				 unsigned int		dst_size,
				 const uint16_t *	src,
				 unsigned int		src_size)
{
	const char *s;
	size_t sleft;
	size_t dleft;
	size_t r;

	s = (const char *) src;

	sleft = src_size;
	dleft = dst_size;

	r = xiconv (cd, &s, &sleft, dst, &dleft, 2);

	if ((size_t) -1 == r)
		return FALSE;

	if (sleft > 0)
		return FALSE;

	return TRUE;
}

/**
 * @internal
 * @param cd Conversion object returned by vbi_iconv_ucs2_open().
 * @param dst Pointer to output buffer pointer, will be incremented
 *   by the number of bytes written.
 * @param dst_size Space available in the output buffer, in bytes.
 * @param unicode Single 16 bit Unicode character.
 *
 * Converts UCS-2 character (as in vbi_page) to another format
 * and stores it in the output buffer. Characters not representable
 * in the target format are converted to spaces 0x0020.
 *
 * @return
 * FALSE on failure, including dst_size too small. Output buffer
 * contents are undefined on failure.
 */
vbi_bool
vbi_iconv_unicode		(iconv_t		cd,
				 char **		dst,
				 unsigned int		dst_size,
				 unsigned int		unicode)
{
	uint16_t t[1];

	t[0] = unicode;

	return vbi_iconv_ucs2 (cd, dst, dst_size, t, 1);
}

static char *
strdup_iconv			(const char *		dst_format,
				 const char *		src_format,
				 const char *		src,
				 unsigned int		src_size,
				 unsigned int		csize)
{
	char *buf;
	char *buf2;
	unsigned int buf_size;
	iconv_t cd;
	const char *s;
	char *d;
	size_t sleft;
	size_t dleft;

	buf_size = src_size * 4;

	if (!(buf = malloc (buf_size)))
		goto failure;

	s = (const char *) src;
	d = buf;

	cd = xiconv_open (dst_format, src_format, &d, buf_size);

	if ((iconv_t) -1 == cd)
		goto failure;

	sleft = src_size;
	dleft = buf_size - (d - buf);

	while (sleft > 0) {
		size_t r;

		r = xiconv (cd, &s, &sleft, &d, &dleft, csize);

		if ((size_t) -1 == r)
			if (E2BIG != errno)
				goto failure;

		if (!(buf2 = realloc (buf, buf_size * 2)))
			goto failure;

		d = buf2 + (d - buf);
		dleft += buf_size * 2 - buf_size;

		buf = buf2;
		buf_size *= 2;
	}

	if (!(buf2 = realloc (buf, buf_size - dleft + 4)))
		goto failure;

	memset (buf2 + (d - buf), 0, 4);

	iconv_close (cd);

	return buf2;

 failure:
	if ((iconv_t) -1 != cd)
		iconv_close (cd);

	free (buf);

	return NULL;
}

/**
 * @internal
 * @param dst_format Character set name for iconv() conversion,
 *   for example "ISO-8859-1". When @c NULL, the default is UTF-8.
 * @param src Input buffer.
 * @param src_size Number of characters (not bytes) in the
 *   input buffer.
 *
 * Converts UCS-2 coded text (as in vbi_page) to another format,
 * and stores it in a newly allocated buffer. Characters not representable
 * in the @a dst_format are converted to spaces 0x0020.
 *
 * @return
 * NUL terminated string, an empty string if @a src_size is 0,
 * a @c NULL pointer on failure.
 */
char *
vbi_strdup_iconv_ucs2		(const char *		dst_format,
				 const uint16_t *	src,
				 unsigned int		src_size)
{
	return strdup_iconv (dst_format, NULL,
			     (const char *) src, src_size * 2, 2);
}

/**
 * @internal
 * @param src Input buffer.
 * @param src_size Number of characters (not bytes) in the
 *   input buffer.
 *
 * Converts UCS-2 coded text (as in vbi_page) to the character set
 * used by gettext or the current locale and stores it in a newly
 * allocated buffer. Characters not representable
 * in the target format are converted to spaces 0x0020.
 *
 * @return
 * NUL terminated string, an empty string if @a src_size is 0,
 * a @c NULL pointer on failure.
 */
char *
vbi_strdup_locale_ucs2		(const uint16_t *	src,
				 unsigned int		src_size)
{
	const char *dst_format;

	dst_format = bind_textdomain_codeset (vbi_intl_domainname, NULL);

	if (NULL == dst_format)
		dst_format = nl_langinfo (CODESET);

	if (NULL == dst_format)
		return NULL;

	return strdup_iconv (dst_format, NULL,
			     (const char *) src, src_size, 2);
}

/**
 * @internal
 * @param src NUL-terminated string in UTF-8 format.
 *
 * Converts UTF-8 coded string to the character set used by gettext
 * or the current locale and stores it in a newly allocated buffer.
 * Characters not representable in the output format are converted
 * to spaces 0x0020.
 *
 * @return
 * NUL terminated string, NULL if src is NULL and on failure.
 */
char *
vbi_strdup_locale_utf8		(const char *		src)
{
	const char *dst_format;
	unsigned int src_size;

	if (!src)
		return NULL;

	dst_format = bind_textdomain_codeset (vbi_intl_domainname, NULL);

	if (NULL == dst_format)
		dst_format = nl_langinfo (CODESET);

	if (NULL == dst_format)
		return NULL;

	if (0 == strcmp (dst_format, "UTF-8"))
		return strdup (src);

	src_size = strlen (src);

	return strdup_iconv (dst_format, "UTF-8", src, src_size, 1);
}

/**
 * @internal
 * @param src NUL-terminated string in UTF-8 format.
 *
 * Converts UTF-8 coded string to UCS-2 string and stores it in a
 * newly allocated buffer.
 *
 * @return
 * NUL terminated string, NULL if src is NULL and on failure.
 */
uint16_t *
vbi_strdup_ucs2_utf8		(const char *		src)
{
	unsigned int src_size;

	if (!src)
		return NULL;

	src_size = strlen (src);

	return (uint16_t *) strdup_iconv ("UCS-2", "UTF-8", src, src_size, 1);
}

/**
 * @internal
 * @param fp Output file.
 * @param dst_format Character set name for iconv() conversion,
 *   for example "ISO-8859-1". When @c NULL, the default is UTF-8.
 * @param src Input buffer.
 * @param src_size Number of characters (not bytes) in the
 *   input buffer.
 *
 * Converts UCS-2 coded text (as in vbi_page) to another format,
 * and writes it into the given file. Characters not representable
 * in the output format are converted to spaces 0x0020.
 *
 * @return
 * FALSE on failure.
 */
vbi_bool
vbi_stdio_iconv_ucs2		(FILE *			fp,
				 const char *		dst_format,
				 const uint16_t *	src,
				 unsigned int		src_size)
{
	char buffer[4096];
	iconv_t cd;
	const char *s;
	char *d;
	size_t sleft;
	size_t dleft;

	s = (const char *) src;
	d = buffer;

	cd = xiconv_open (dst_format, NULL, &d, sizeof (buffer));

	if ((iconv_t) -1 == cd)
		return FALSE;

	sleft = src_size * 2;
	dleft = sizeof (buffer) - (buffer - d);

	while (sleft > 0) {
		size_t r;
		size_t n;

		r = xiconv (cd, &s, &sleft, &d, &dleft, 2);

		if ((size_t) -1 == r)
			if (E2BIG != errno)
				goto failure;

		n = d - buffer;

		r = fwrite (buffer, 1, n, fp);

		if (n != r)
			goto failure;

		d = buffer;
		dleft = sizeof (buffer);
	}

	iconv_close (cd);

	return TRUE;

 failure:
	iconv_close (cd);

	return FALSE;
}
