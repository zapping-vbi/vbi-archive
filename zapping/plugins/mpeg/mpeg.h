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

/* $Id: mpeg.h,v 1.3 2001-10-16 11:17:08 mschimek Exp $ */

extern GtkWidget *grte_options_create (rte_context *context, rte_codec *codec, gchar *zc_domain);
extern gboolean grte_options_load (rte_codec *codec, gchar *zc_domain);
extern gboolean grte_options_save (rte_codec *codec, gchar *zc_domain);
/*
extern void mpeg_plugin_read_audio_oss(void *data, double *time, rte_context *context);
extern int mpeg_plugin_open_pcm_oss(char *dev_name, int sampling_rate, int stereo, rte_context *context);
extern void mpeg_plugin_close_pcm_oss(rte_context *context);
*/
