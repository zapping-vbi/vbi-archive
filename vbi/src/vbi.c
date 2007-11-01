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

/* $Id: vbi.c,v 1.6.2.25 2007-11-01 00:21:25 mschimek Exp $ */

#include "../site_def.h"

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <pthread.h>

#include "misc.h"
#include "vbi.h"
#include "hamm.h"
#include "lang.h"
#include "export.h"
#include "tables.h"
#include "page.h"
#include "wss.h"
#include "version.h"
#include "vps.h"
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

//pthread_once_t vbi3_init_once = PTHREAD_ONCE_INIT;

void *(* vbi3_malloc)(size_t)		= malloc;
void *(* vbi3_realloc)(void *, size_t)	= realloc;
char *(* vbi3_strdup)(const char *)	= strdup;
void (* vbi3_free)(void *)		= free;

void
vbi3_set_log_fn			(vbi3_log_mask		mask,
				 vbi3_log_fn *		log_fn,
				 void *			user_data)
{
	if (NULL == log_fn)
		mask = 0;

	_vbi3_global_log.mask		= mask;
	_vbi3_global_log.fn		= log_fn;
	_vbi3_global_log.user_data	= user_data;
}

void
vbi3_init			(void)
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
vbi3_version			(unsigned int *		major,
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

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
