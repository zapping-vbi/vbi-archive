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

/* $Id: mpeg.h,v 1.9 2002-04-20 06:42:30 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#if defined(HAVE_LIBRTE4)

#include <rte.h>

#if RTE_MAJOR_VERSION != 0 || RTE_MINOR_VERSION < 4
#  error rte version 0.4+ required, please install the latest version from zapping.sf.net and reconfigure.
#endif

#define rte_context_delete(cx) rte_context_destroy(cx)
#define rte_context_info_context(cx) rte_context_info_by_context (cx)
#define rte_codec_info_codec(cd) rte_codec_info_by_codec(cd)
#define rte_codec_option_info_enum(cd, ent) rte_option_info_enum(cd, ent)
#define rte_codec_option_info_keyword(cd, key) rte_option_info_by_keyword(cd, key)
#define rte_codec_option_menu_get(cd, key, ent) rte_option_get_menu(cd, key, ent)
#define rte_codec_option_menu_set(cd, key, ent) rte_option_set_menu(cd, key, ent)
#define rte_codec_option_get(cd, key, val) rte_option_get(cd, key, val)
#define rte_codec_option_set(cd, key, val) rte_option_set(cd, key, val)
#define rte_codec_option_print(cd, key, val) rte_option_print(cd, key, val)

extern rte_context_info *rte_context_info_by_context (rte_context *);

#elif defined(HAVE_LIBRTE5)

#include <librte.h>

extern gint grte_num_codecs (rte_context *context, rte_stream_type stream_type,
			     rte_codec_info **info_p);
#endif

#define ZCONF_DOMAIN "/zapping/plugins/mpeg"
#define MPEG_CONFIG "default"

extern GtkWidget *grte_options_create (rte_context *context, rte_codec *codec);

extern GtkWidget *grte_codec_create_menu (rte_context *context, gchar *zc_subdomain,
					  rte_stream_type stream_type, gint *default_item);
extern rte_codec *grte_codec_load (rte_context *context, gchar *zc_subdomain,
				   rte_stream_type stream_type, gchar *keyword);
extern void grte_codec_save (rte_context *context, gchar *zc_subdomain,
			     rte_stream_type stream_type);

extern GtkWidget *grte_context_create_menu (gchar *zc_subdomain, gint *default_item);
extern rte_context *grte_context_load (gchar *zc_subdomain, gchar *keyword,
				       rte_codec **audio_codec_p,
				       rte_codec **video_codec_p);
extern void grte_context_save (rte_context *context, gchar *zc_subdomain);
