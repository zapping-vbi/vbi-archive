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

/* $Id: vbi_decoder.h,v 1.1.2.2 2004-05-01 13:51:35 mschimek Exp $ */

#ifndef __ZVBI_VBI_DECODER_H__
#define __ZVBI_VBI_DECODER_H__

#include "macros.h"
#include "teletext_decoder.h"

VBI_BEGIN_DECLS

#ifndef VBI_DECODER
#define VBI_DECODER
/**
 * @ingroup Service
 * @brief Opaque VBI data service decoder object.
 *
 * Allocate with vbi_decoder_new().
 */
typedef struct vbi_decoder vbi_decoder;
#endif

/**
 * @addtogroup Service
 * @{
 */
extern vbi_decoder *	vbi_decoder_new(void);
extern void		vbi_decoder_delete(vbi_decoder *vbi);
extern void		vbi_decode(vbi_decoder *vbi, vbi_sliced *sliced,
				   int lines, double timestamp);
extern void             vbi_channel_switched(vbi_decoder *vbi, vbi_nuid nuid);
extern const char **
vbi_cache_page_language		(vbi_decoder *		vbi,
				 vbi_pgno		pgno);
extern vbi_page_type	vbi_classify_page(vbi_decoder *vbi, vbi_pgno pgno,
					  vbi_subno *subno);
extern void		vbi_version(unsigned int *major, unsigned int *minor, unsigned int *micro);

// XXX doesn't belong here.
typedef void vbi_log_fn		(const char *		function,
				 const char *		message,
				 void *			user_data);
extern void
vbi_set_log_fn			(vbi_log_fn *		function,
				 void *			user_data);
/** @} */



//extern pthread_once_t	vbi_init_once;
extern void
vbi_init			(void)
     __attribute__ ((constructor));

extern void		vbi_transp_colormap	(vbi_decoder *		vbi,
						 vbi_rgba *		d,
						 const vbi_rgba *	s,
						 int			entries);
extern void             vbi_chsw_reset(vbi_decoder *vbi, vbi_nuid nuid);

extern void		vbi_asprintf(char **errstr, char *templ, ...);


#include <stdarg.h>

/**
 * @ingroup Service
 * @brief Teletext implementation level.
 */
typedef enum {
	VBI_WST_LEVEL_1,   /**< 1 - Basic Teletext pages */
	VBI_WST_LEVEL_1p5, /**< 1.5 - Additional national and graphics characters */
	/**
	 * 2.5 - Additional text styles, more colors and DRCS. You should
	 * enable Level 2.5 only if you can render and/or export such pages.
	 */
	VBI_WST_LEVEL_2p5,
	VBI_WST_LEVEL_3p5  /**< 3.5 - Multicolor DRCS, proportional script */
} vbi_wst_level;


/**
 * @addtogroup Cache
 * @{
 */

typedef enum {
	/**
	 * Format only the first row.
	 * Parameter: vbi_bool.
	 */
	VBI_HEADER_ONLY = 0x37138F00,
	/**
	 * Often column 0 of a page contains all black spaces,
	 * unlike column 39. Enabling this option will result in
	 * a more balanced view.
	 * Parameter: vbi_bool.
	 */
	VBI_41_COLUMNS,
	/**
	 * Enable TOP or FLOF navigation in row 25.
	 * Parameter: vbi_bool.
	 */
	VBI_NAVIGATION,
	/**
	 * Scan the page for page numbers, URLs, e-mail addresses
	 * etc. and create hyperlinks.
	 * Parameter: vbi_bool.
	 */
	VBI_HYPERLINKS,
	/**
	 * Scan the page for PDC Method A/B preselection data
	 * and create a PDC table and links.
	 * Parameter: vbi_bool.
	 */
	VBI_PDC_LINKS,
	/**
	 * Format the page at the given Teletext implementation level.
	 * Parameter: vbi_wst_level.
	 */
	VBI_WST_LEVEL,
	/**
	 * Parameter: vbi_character_set_code.
	 */
	VBI_CHAR_SET_DEFAULT,
	/**
	 * Parameter: vbi_character_set_code.
	 */
	VBI_CHAR_SET_OVERRIDE,
} vbi_format_option;

extern vbi_page *
vbi_page_new			(void);
extern void
vbi_page_delete			(vbi_page *		pg);
extern vbi_page *
vbi_page_copy			(const vbi_page *	pg);
extern const uint8_t *
vbi_page_drcs_data		(const vbi_page *	pg,
				 unsigned int		unicode);

extern vbi_page *
vbi_fetch_vt_page		(vbi_decoder *		vbi,
				 vbi_pgno		pgno,
				 vbi_subno		subno,
				 ...);
extern vbi_page *
vbi_fetch_vt_page_va_list	(vbi_decoder *		vbi,
				 vbi_pgno		pgno,
				 vbi_subno		subno,
				 va_list		format_options);

extern int		vbi_page_title(vbi_decoder *vbi, int pgno, int subno, char *buf);
/** @} */
/**
 * @addtogroup Event
 * @{
 */
extern vbi_bool
vbi_page_hyperlink		(const vbi_page *	pg,
				 vbi_link *		ld,
				 unsigned int		column,
				 unsigned int		row);
extern vbi_bool
vbi_page_nav_enum		(const vbi_page *	pg,
				 vbi_link *		ld,
				 unsigned int		index);
/** @} */

VBI_END_DECLS

#endif /* __ZVBI_VBI_DECODER_H__ */
