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

/* $Id: vbi.c,v 1.6.2.17 2004-05-12 01:40:44 mschimek Exp $ */

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

//pthread_once_t vbi_init_once = PTHREAD_ONCE_INIT;

void
vbi_init			(void)
{
#ifdef ENABLE_NLS
	bindtextdomain (PACKAGE, PACKAGE_LOCALE_DIR);
#endif
}


/*
 *  VBI Decoder
 */

vbi_inline double
current_time(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);

	return tv.tv_sec + tv.tv_usec * (1 / 1e6);
}


void
vbi_chsw_reset(vbi_decoder *vbi, vbi_nuid identified)
{
#warning
#if 0
	vbi_nuid old_nuid = vbi->network.ev.network.nuid;

	if (0)
		fprintf(stderr, "*** chsw identified=%d old nuid=%d\n",
			identified, old_nuid);

#warning
	//	vbi_cache_flush (vbi, TRUE);

#warning unfinished
	vbi_teletext_decoder_reset (&vbi->vt, 0);
	vbi_caption_channel_switched(vbi);

	if (identified == 0) {
		memset(&vbi->network, 0, sizeof(vbi->network));

		if (old_nuid != 0) {
			vbi->network.type = VBI_EVENT_NETWORK;
#warning obsolete
//			vbi_send_event(vbi, &vbi->network);
		}
	} /* else already identified */

	vbi_trigger_flush(vbi); /* sic? */

//	if (vbi->aspect_source > 0) {
		vbi_event e;

//		_vbi_aspect_ratio_init (&e.ev.aspect,
//					(vbi->aspect_source == 1) ?
//					VBI_VIDEOSTD_SET_625_50 :
//					VBI_VIDEOSTD_SET_525_60);

		e.type = VBI_EVENT_ASPECT;
//		vbi_send_event(vbi, &e);
//	}

	vbi_reset_prog_info(&vbi->prog_info[0]);
	vbi_reset_prog_info(&vbi->prog_info[1]);
	/* XXX event? */

	vbi->prog_info[1].future = TRUE;
	vbi->prog_info[0].future = FALSE;

//	vbi->aspect_source = 0;

	vbi->wss_last[0] = 0;
	vbi->wss_last[1] = 0;
	vbi->wss_rep_ct = 0;
	vbi->wss_time = 0.0;

	vbi->vt.header_page.pgno = 0;

	//	pthread_mutex_lock(&vbi->chswcd_mutex);

	vbi->chswcd = 0;

	//	pthread_mutex_unlock(&vbi->chswcd_mutex);
#endif
}

/**
 * @param vbi VBI decoding context.
 * @param nuid Set to zero for now.
 * 
 * Call this after switching away from the channel (RF channel,
 * video input line, precisely: the network) from which this context
 * used to receive vbi data, to reset the decoding context accordingly.
 * This includes deletion of all cached Teletext and Closed Caption pages.
 *
 * The decoder attempts to detect channel switches automatically, but this
 * is not 100 % reliable, especially without receiving and decoding Teletext
 * or VPS which frequently transmit network identifiers.
 *
 * Note the reset is not executed until the next frame is about to be
 * decoded, so you may still receive "old" events after calling this. You
 * may also receive blank events (e. g. unknown network, unknown aspect
 * ratio) revoking a previously sent event, until new information becomes
 * available.
 */
void
vbi_channel_switched(vbi_decoder *vbi, vbi_nuid nuid)
{
#warning todo
	/* XXX nuid */

  //	pthread_mutex_lock(&vbi->chswcd_mutex);

//	vbi->chswcd = 1;

	//	pthread_mutex_unlock(&vbi->chswcd_mutex);
}

/*
 *  Cache queries
 */

/**
 * @param vbi Initialized vbi decoding context.
 * @param pgno Teletext or Closed Caption page to examine, see vbi_pgno.
 *
 * This function returns the language (or languages) a page is written
 * in. Languages are identified by their ISO 639 two-character code,
 * for example French by the string "fr".
 *
 * The function returns a NULL-terminated string vector. The vector can
 * be empty if the language is not known to the decoder, for example
 * when the page has not been received yet. It can contain more than
 * one language code if the page is multilingual or the available
 * information is ambiguous. In fact the language code or codes may
 * be misleading.
 *
 * @note The results of this function are volatile: As more information
 * becomes available and pages are edited (e. g. activation of subtitles)
 * languages can change.
 *
 * @return
 * Pointer to a string vector. The function returns a @c NULL pointer
 * when @a pgno is invalid.
 */
const char **
vbi_cache_page_language		(vbi_decoder *		vbi,
				 vbi_pgno		pgno)
{
	return 0;
#warning todo
#if 0
	static const char *nil = NULL;

	if (pgno <= 8 && pgno >= 1) {
		return vbi->cc.channel[pgno - 1].lang_code;
	} else if (pgno >= 0x100 && pgno <= 0x8FF) {
	  page_stat *pi;
		int code;

		pi = vt_network_page_stat (vbi->vt.network, pgno);
		code = pi->page_type;
		if (code != VBI_UNKNOWN_PAGE) {
/* FIXME normal pages */		
			if (code == VBI_SUBTITLE_PAGE) {
				const vbi_character_set *cs;

				if (pi->charset_code != 0xFF
				    && (cs = vbi_character_set_from_code (pi->charset_code)))
					return cs->language_code;
			}
		}
	}

	return &nil;
#endif
}

/**
 * @param vbi Initialized vbi decoding context.
 * @param pgno Teletext or Closed Caption page to examine, see vbi_pgno.
 * @param subno The highest subpage number of this page will be
 *   stored here. @a subno can be @c NULL.
 * 
 * Returns information about the page.
 * 
 * For Closed Caption pages (@a pgno 1 ... 8) @a subno will always
 * be zero, @a language set or @c NULL. The return value will be
 * @c VBI_SUBTITLE_PAGE for page 1 ... 4 (Closed Caption
 * channel 1 ... 4), @c VBI_NORMAL_PAGE for page 5 ... 8 (Text channel
 * 1 ... 4), or @c VBI_NO_PAGE if no data is currently transmitted on
 * the channel.
 *
 * For Teletext pages (@a pgno 0x100 ... 0x8FF) @a subno returns
 * the highest subpage number used. Note this number can be larger
 * (but not smaller) than the number of subpages actually received
 * and cached. Still there is no guarantee the advertised subpages
 * will ever appear or stay in cache.
 *
 * <table>
 * <tr><td><b>subno</b></td><td><b>meaning</b></td></tr>
 * <tr><td>0</td><td>single page, no subpages</td></tr>
 * <tr><td>1</td><td>never</td></tr>
 * <tr><td>2 ... 0x3F7F</td><td>has subpages 1 ... @a subno </td></tr>
 * <tr><td>0xFFFE</td><td>has unknown number (two or more) of subpages</td></tr>
 * <tr><td>0xFFFF</td><td>presence of subpages unknown</td></tr>
 * </table>
 *
 * @a language currently returns the language of subtitle pages, @c NULL
 * if unknown or the page is not classified as @c VBI_SUBTITLE_PAGE.
 *
 * Other page types are:
 *
 * <table>
 * <tr><td>VBI_NO_PAGE</td><td>Page is not in transmission</td></tr>
 * <tr><td>VBI_NORMAL_PAGE</td><td>&nbsp;</td></tr>
 * <tr><td>VBI_SUBTITLE_PAGE</td><td>&nbsp;</td></tr>
 * <tr><td>VBI_SUBTITLE_INDEX</td><td>List of subtitle pages</td></tr>
 * <tr><td>VBI_NONSTD_SUBPAGES</td><td>For example a world time page</td></tr>
 * <tr><td>VBI_PROGR_WARNING</td><td>Program related warning (perhaps
 * schedule change anouncements, the Teletext specification does not
 * elaborate on this)</td></tr>
 * <tr><td>VBI_CURRENT_PROGR</td><td>Information about the
 * current program</td></tr>
 * <tr><td>VBI_NOW_AND_NEXT</td><td>Brief information about the
 * current and next program</td></tr>
 * <tr><td>VBI_PROGR_INDEX</td><td>Program index page (perhaps the front
 * page of all program related pages)</td></tr>
 * <tr><td>VBI_PROGR_SCHEDULE</td><td>Program schedule page</td></tr>
 * <tr><td>VBI_UNKNOWN_PAGE</td><td>&nbsp;</td></tr>
 * </table>
 *
 * @note The results of this function are volatile: As more information
 * becomes available and pages are edited (e. g. activation of subtitles,
 * news updates, program related pages) subpage numbers can grow, page
 * types, subno 0xFFFE and 0xFFFF and languages can change.
 *
 * @return
 * Page type.
 */
vbi_page_type
vbi_classify_page(vbi_decoder *vbi, vbi_pgno pgno, vbi_subno *subno)
{
	return 0;
#warning todo
#if 0
  page_stat *pi;
	int code, subc;
//	char *lang;

	if (!subno)
		subno = &subc;
//	if (!language)
//		language = &lang;

	*subno = 0;
	//	*language = NULL;

	if (pgno < 1) {
		return VBI_UNKNOWN_PAGE;
	} else if (pgno <= 8) {
		if ((current_time() - vbi->cc.channel[pgno - 1].time) > 20)
			return VBI_NO_PAGE;

		//		*language = vbi->cc.channel[pgno - 1].language;

		return (pgno <= 4) ? VBI_SUBTITLE_PAGE : VBI_NORMAL_PAGE;
	} else if (pgno < 0x100 || pgno > 0x8FF) {
		return VBI_UNKNOWN_PAGE;
	}

	pi = vt_network_page_stat (vbi->vt.network, pgno);
	code = pi->page_type;

	if (code != VBI_UNKNOWN_PAGE) {
		if (code == VBI_SUBTITLE_PAGE) {
			if (pi->charset_code != 0xFF)
				;
	//		*language = vbi_font_descriptors[pi->language].label;
		} else if (code == VBI_TOP_BLOCK || code == VBI_TOP_GROUP)
			code = VBI_NORMAL_PAGE;
		else if (code == VBI_NOT_PUBLIC || code > 0xE0)
			return VBI_UNKNOWN_PAGE;

		*subno = pi->subcode;

		return code;
	}

	if ((pgno & 0xFF) <= 0x99) {
		*subno = 0xFFFF;
		return VBI_NORMAL_PAGE; /* wild guess */
	}

	return VBI_UNKNOWN_PAGE;
#endif
}








/**
 * @param pi 
 * 
 * Convenience function to set a vbi_program_info
 * structure to defaults.
 */
void
vbi_reset_prog_info(vbi_program_info *pi)
{
	int i;

	/* PID */
	pi->month = -1;
	pi->day = -1;
	pi->hour = -1;
	pi->min = -1;
	pi->tape_delayed = 0;
	/* PL */
	pi->length_hour = -1;
	pi->length_min = -1;
	pi->elapsed_hour = -1;
	pi->elapsed_min = -1;
	pi->elapsed_sec = -1;
	/* PN */
	pi->title[0] = 0;
	/* PT */
	pi->type_classf = VBI_PROG_CLASSF_NONE;
	/* PR */
	pi->rating_auth = VBI_RATING_AUTH_NONE;
	/* PAS */
	pi->audio[0].mode = VBI_AUDIO_MODE_UNKNOWN;
	pi->audio[0].lang_code = NULL;
	pi->audio[1].mode = VBI_AUDIO_MODE_UNKNOWN;
	pi->audio[1].lang_code = NULL;
	/* PCS */
	pi->caption_services = -1;
	for (i = 0; i < 8; i++)
		pi->caption_lang_code[i] = NULL;
	/* CGMS */
	pi->cgms_a = -1;
	/* AR */
	_vbi_aspect_ratio_init (&pi->aspect, VBI_VIDEOSTD_SET_525_60);
	/* PD */
	for (i = 0; i < 8; i++)
		pi->description[i][0] = 0;
}

/**
 * @param major Store major number here, can be NULL.
 * @param minor Store minor number here, can be NULL.
 * @param micro Store micro number here, can be NULL.
 *
 * Returns the library version defined in the libzvbi.h header file
 * when the library was compiled. This function is available since
 * version 0.2.5.
 */
void
vbi_version			(unsigned int *		major,
				 unsigned int *		minor,
				 unsigned int *		micro)
{
	if (major) *major = VBI_VERSION_MAJOR;
	if (minor) *minor = VBI_VERSION_MINOR;
	if (micro) *micro = VBI_VERSION_MICRO;
}

/**
 * @internal
 */
size_t
vbi_strlcpy			(char *			dst1,
				 const char *		src,
				 size_t			size)
{
	char c, *dst, *end;

	assert (size > 0);

	dst = dst1;
	end = dst1 + size - 1;

	while (dst < end && (c = *src++))
		*dst++ = c;

	*dst = 0;

	return dst - dst1;
}

/**
 * @internal
 * libzvbi internal helper function.
 * Note asprintf() is a GNU libc extension.
 */
void
_vbi_asprintf(char **errstr, char *templ, ...)
{
	char buf[512];
	va_list ap;
	int temp;

	if (!errstr)
		return;

	temp = errno;

	va_start(ap, templ);

	vsnprintf(buf, sizeof(buf) - 1, templ, ap);

	va_end(ap);

	*errstr = strdup(buf);

	errno = temp;
}

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

	log_function (function, buf, log_user_data);

	errno = temp;
}
