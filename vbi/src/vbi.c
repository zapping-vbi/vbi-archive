/*
 *  libzvbi - VBI decoding library
 *
 *  Copyright (C) 2000, 2001, 2002 Michael H. Schimek
 *  Copyright (C) 2000, 2001 Iñaki García Etxebarria
 *
 *  Based on AleVT 1.5.1
 *  Copyright (C) 1998, 1999 Edgar Toernig <froese@gmx.de>
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

/* $Id: vbi.c,v 1.6.2.20 2004-09-14 04:52:00 mschimek Exp $ */

#include "../site_def.h"
#include "../config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <pthread.h>

#include "vbi.h"
#include "hamm.h"
#include "lang.h"
#include "export.h"
#include "tables.h"
#include "format.h"
#include "wss.h"
#include "version.h"
#include "vps.h"
#include "misc.h"
#include "vbi_decoder.h"
#include "intl-priv.h"

/**
 * @mainpage ZVBI - VBI Decoding Library
 *
 * @author Iñaki García Etxebarria<br>
 * Michael H. Schimek<br>
 * based on AleVT by Edgar Toernig
 *
 * @section intro Introduction
 *
 * The ZVBI library provides routines to access raw VBI sampling devices
 * (currently the Linux <a href="http://roadrunner.swansea.uk.linux.org/v4l.shtml">V4L</a>
 * and <a href="http://www.thedirks.org/v4l2/">V4L2</a> API and the
 * FreeBSD, OpenBSD, NetBSD and BSDi
 * <a href="http://telepresence.dmem.strath.ac.uk/bt848/">bktr driver</a> API
 * are supported), a versatile raw VBI bit slicer,
 * decoders for various data services and basic search,
 * render and export functions for text pages. The library was written for
 * the <a href="http://zapping.sourceforge.net">Zapping TV viewer and
 * Zapzilla Teletext browser</a>.
 */

/** @defgroup Raw Raw VBI */
/** @defgroup Service Data Service Decoder */
/** @defgroup LowDec Low Level Decoding */
/** @defgroup HiDec High Level Decoding */

//pthread_once_t vbi_init_once = PTHREAD_ONCE_INIT;

void
vbi_init			(void)
{
#ifdef ENABLE_NLS
	bindtextdomain (PACKAGE, PACKAGE_LOCALE_DIR);
#endif
}


/**
 * @param major Store major number here, can be NULL.
 * @param minor Store minor number here, can be NULL.
 * @param micro Store micro number here, can be NULL.
 *
 * Returns the library version defined in the libzvbi.h header file
 * when the library was compiled. This function is available since
 * version 0.2.5.
 *
 * @returns
 * Version number like 0x000205.
 */
unsigned int
vbi_version			(unsigned int *		major,
				 unsigned int *		minor,
				 unsigned int *		micro)
{
	if (major) *major = VBI_VERSION_MAJOR;
	if (minor) *minor = VBI_VERSION_MINOR;
	if (micro) *micro = VBI_VERSION_MICRO;

	return (+ (VBI_VERSION_MAJOR << 16)
		+ (VBI_VERSION_MINOR << 8)
		+ (VBI_VERSION_MICRO << 0));
}

#ifndef HAVE_STRLCPY

/**
 * @internal
 * strlcpy() is a BSD/GNU extension.
 */
size_t
_vbi_strlcpy			(char *			dst,
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
_vbi_strndup			(const char *		s,
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
_vbi_asprintf			(char **		dstp,
				 const char *		templ,
				 ...)
{
	char *buf;
	int size;
	int temp;

	assert (NULL != dstp);
	assert (NULL != templ);

	temp = errno;

	buf = NULL;
	size = 64;

	for (;;) {
		va_list ap;
		char *buf2;
		int len;

		if (!(buf2 = realloc (buf, size)))
			break;

		buf = buf2;

		va_start (ap, templ);
		len = vsnprintf (buf, size, templ, ap);
		va_end (ap);

		if (len < 0) {
			/* Not enough. */
			size *= 2;
		} else if (len < size) {
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

static vbi_log_fn *		log_function;
static void *			log_user_data;

void
vbi_set_log_fn			(vbi_log_fn *		function,
				 void *			user_data)
{
	log_function = function;
	log_user_data = user_data;
}

void
vbi_log_printf			(vbi_log_level		level,
				 const char *		function,
				 const char *		template,
				 ...)
{
	char buf[512];
	va_list ap;
	int temp;

	if (NULL == log_function)
		return;

	temp = errno;

	va_start (ap, template);

	vsnprintf (buf, sizeof (buf) - 1, template, ap);

	va_end (ap);

	log_function (level, function, buf, log_user_data);

	errno = temp;
}
