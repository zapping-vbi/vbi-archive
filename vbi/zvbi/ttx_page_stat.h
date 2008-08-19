/*
 *  libzvbi -- Teletext page statistics
 *
 *  Copyright (C) 2001, 2002, 2003, 2004, 2007 Michael H. Schimek
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

/* $Id: ttx_page_stat.h,v 1.1.2.1 2008-08-19 10:56:06 mschimek Exp $ */

#ifndef __ZVBI_TTX_PAGE_STAT_H__
#define __ZVBI_TTX_PAGE_STAT_H__

#include "zvbi/macros.h"

VBI_BEGIN_DECLS

/**
 * @brief Teletext page type.
 *
 * Some networks provide information to classify Teletext pages.
 * This can be used for example to automatically find program
 * schedule and subtitle pages.
 *
 * These codes are defined in EN 300 706 Annex J, Table J.1. Note the
 * vbi_ttx_decoder collects information about Teletext pages from all
 * received data, not just MIP tables.
 *
 * For compatibility with future extensions applications should ignore
 * codes which are not listed here.
 */
typedef enum {
	/**
	 * The network does not transmit this page (at this time).
	 * This information can be useful to skip pages in Teletext
	 * browser. It comes from a received page inventory table.
	 */
	VBI_NO_PAGE		= 0x00,

	/**
	 * A normal Teletext page which is intended for display
	 * to the user.
	 *
	 * The vbi_ttx_decoder does not return the MIP codes 0x02 to
	 * 0x51. The number of subpages is available through
	 * vbi_cache_get_ttx_page_stat() instead.
	 */
	VBI_NORMAL_PAGE		= 0x01,

	/**
	 * Teletext pages are divided into eight "magazines" with
	 * page numbers 1xx to 8xx. Tables Of Pages (TOP) navigation
	 * topically divides them further into groups and blocks.
	 *
	 * This is a normal page which is also the first page of a
	 * TOP block.
	 *
	 * You can determine the title of a TOP block or group with
	 * vbi_cache_get_top_title() and get a list of all blocks and
	 * groups with the vbi_cache_get_top_titles() function.
	 *
	 * You can add a row with links to the closest TOP block and
	 * group pages to a Teletext page with the VBI_NAVIGATION
	 * option of vbi_ttx_decoder_get_page().
	 *
	 * This is a libzvbi internal code, not defined in EN 300 706.
	 */
	VBI_TOP_BLOCK		= 0x60,

	/**
	 * A normal page which is also the first page of a
	 * TOP group. This is a libzvbi internal code.
	 */
	VBI_TOP_GROUP		= 0x61,

	/**
	 * This page contains news in brief and can be displayed
	 * like a subtitle page. This is a libzvbi internal code.
	 */
	VBI_NEWSFLASH_PAGE	= 0x62,

	/**
	 * This page contains subtitles. However no subtitles may be
	 * transmitted right now, or the page may just display
	 * something like "currently no subtitles".
	 *
	 * The vbi_ttx_decoder does not return the MIP codes 0x71 to
	 * 0x77. The subtitle language is available through
	 * vbi_cache_get_ttx_page_stat() instead.
	 */
	VBI_SUBTITLE_PAGE	= 0x70,

	/**
	 * A normal page which shows information about subtitles, for
	 * example the available languages and the respective page
	 * number.
	 */
	VBI_SUBTITLE_INDEX	= 0x78,

	/**
	 * This page contains the local time of the intended audience
	 * of the program encoded as a BCD value in the subpage number
	 * (e.g. 0x2359). Such pages usually show the current time in
	 * different time zones.
	 */
	VBI_CLOCK_PAGE		= 0x79,

	/**
	 * A normal page containing information concerning the
	 * content of the current TV programmes so that the viewer can
	 * be warned of the suitability of the contents for general
	 * viewing.
	 */
	VBI_PROGR_WARNING	= 0x7A,

	/**
	 * A normal page containing information associated with
	 * the current TV programme.
	 *
	 * The vbi_ttx_decoder does not return the MIP code 0x7B. The
	 * number of subpages is available through
	 * vbi_cache_get_ttx_page_stat() instead.
	 */
	VBI_CURRENT_PROGR	= 0x7C,

	/**
	 * A normal page containing information about the current and
	 * the following programme. Usually just a few lines which can
	 * be displayed like subtitles.
	 */
	VBI_NOW_AND_NEXT	= 0x7D,

	/**
	 * A normal page containing information about the current
	 * TV programmes.
	 */
	VBI_PROGR_INDEX		= 0x7F,

	VBI_NOT_PUBLIC		= 0x80,

	/**
	 * A normal page containing a programme schedule.
	 *
	 * The vbi_ttx_decoder does not return the MIP codes 0x82 to
	 * 0xD1. The number of subpages is available through
	 * vbi_cache_get_ttx_page_stat() instead.
	 */
	VBI_PROGR_SCHEDULE	= 0x81,

	/**
	 * The page contains conditional access data and is not
	 * displayable. Libzvbi does not evaluate such data.
	 */
	VBI_CA_DATA		= 0xE0,

	/**
	 * The page contains binary coded electronic programme guide
	 * data and is not displayable. Currently libzvbi does not
	 * evaluate this data. An open source NexTView EPG decoder
	 * is available at http://nxtvepg.sourceforge.net
	 */
	VBI_PFC_EPG_DATA	= 0xE3,

	/**
	 * The page contains binary data, is not displayable.
	 * See also vbi_pfc_demux_new().
	 */
	VBI_PFC_DATA		= 0xE4,

	/**
	 * The page contains character bitmaps, is not displayable.
	 *
	 * Libzvbi uses this information like this to optimize the
	 * caching of pages. It is not particularly useful for
	 * applications.
	 */
	VBI_DRCS_PAGE		= 0xE5,

	/**
	 * The page contains additional data for Level 1.5/2.5/3.5
	 * pages, is not displayable.
	 */
	VBI_POP_PAGE		= 0xE6,

	/**
	 * The page contains additional data, for example a page
	 * inventory table, and is not displayable.
	 */
	VBI_SYSTEM_PAGE		= 0xE7,

	/**
	 * Codes used for the pages associated with packet X/25
	 * keyword searching.
	 */
	VBI_KEYWORD_SEARCH_LIST = 0xF9,

	/**
	 * The page contains a binary coded, programme related HTTP
	 * or FTP URL, e-mail address, or Teletext page number. It
	 * is not displayable.
	 *
	 * The vbi_ttx_decoder returns this data through @c
	 * VBI_EVENT_TRIGGER.
	 */
	VBI_TRIGGER_DATA	= 0xFC,

	/**
	 * The page contains binary data about the allocation of
	 * channels in a cable system, presumably network names
	 * and frequencies. Currently libzvbi does not evaluate
	 * this data because the document describing the format
	 * of this page is not freely available.
	 */
	VBI_ACI_PAGE		= 0xFD,

	/**
	 * The page contains MPT, AIT or MPT-EX tables for TOP
	 * navigation, is not displayable.
	 *
	 * The information contained in these tables is available
	 * through vbi_cache_get_ttx_page_stat() and
	 * vbi_cache_get_top_titles().
	 */
	VBI_TOP_PAGE		= 0xFE,

	/** The page type is unknown, a libzvbi internal code. */
	VBI_UNKNOWN_PAGE	= 0xFF
} vbi_ttx_page_type;

extern const char *
_vbi_ttx_page_type_name		(vbi_ttx_page_type	type)
  _vbi_const;

VBI_END_DECLS

#endif /* __ZVBI_TTX_PAGE_STAT_H__ */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
