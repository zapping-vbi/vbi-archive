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

/* $Id: mpeg.h,v 1.8 2001-11-22 17:48:11 mschimek Exp $ */

#include <rte.h>

#if RTE_MAJOR_VERSION != 0 || RTE_MINOR_VERSION < 4
#  error rte version 0.4+ required, please install the latest version from zapping.sf.net and reconfigure.
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
