/* RTE (Real time encoder) front end for Zapping
 * Copyright (C) 2000-2001 Iñaki García Etxebarria
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* $Id: mpeg.h,v 1.10.2.2 2003-02-21 19:07:56 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifdef ENABLE_NLS
#  include <libintl.h>
#  define R_(String) dgettext("rte", String)
#else
#  define R_(String) (String)
#endif

#ifdef HAVE_LIBRTE
#include <librte.h>

extern gint grte_num_codecs (rte_context *context, rte_stream_type stream_type,
			     rte_codec_info **info_p);
#endif

extern GtkWidget *	grte_options_create	(rte_context *		context,
						 rte_codec *		codec);
extern GtkWidget *	grte_codec_create_menu	(rte_context *		context,
						 const gchar *		zc_root,
						 const gchar *		zc_conf,
						 rte_stream_type	stream_type,
						 gint *			default_item);
extern rte_codec *	grte_codec_load		(rte_context *		context,
						 const gchar *		zc_root,
						 const gchar *		zc_conf,
						 rte_stream_type	stream_type,
						 const gchar *		keyword);
extern void		grte_codec_save		(rte_context *		context,
						 const gchar *		zc_root,
						 const gchar *		zc_conf,
						 rte_stream_type	stream_type);
extern GtkWidget *	grte_context_create_menu (const gchar *		zc_root,
						  const gchar *		zc_conf,
						  gint *		default_item);
extern rte_context *	grte_context_load	(const gchar *		zc_root,
						 const gchar *		zc_conf,
						 const gchar *		keyword,
						 rte_codec **		audio_codec_p,
						 rte_codec **		video_codec_p,
						 gint *			capture_w,
						 gint *			capture_h);
extern void		grte_context_save	(rte_context *		context,
						 const gchar *		zc_root,
						 const gchar *		zc_conf,
						 gint			capture_w,
						 gint			capture_h);
extern void		grte_config_delete	(const gchar *		zc_root,
						 const gchar *		zc_conf);
