/* Zapping (TV viewer for the Gnome Desktop)
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

#include "site_def.h"

#include <gnome.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libgnomevfs/gnome-vfs.h>
#include <glib/gmessages.h>
#include <pthread.h>
#include <ctype.h>
#include <time.h>
#include <pthread.h>
#include <math.h>

#include "tveng.h"
/* Manages config values for zconf (it saves me some typing) */
#define ZCONF_DOMAIN "/zapping/options/vbi/"
#include "zconf.h"
#include "properties.h"
#include "zvbi.h"
#include "zmisc.h"
#include "interface.h"
#include "v4linterface.h"
#include "osd.h"
#include "remote.h"
#include "globals.h"
#include "subtitle.h"
#include "i18n.h"

#undef TRUE
#undef FALSE
#include "common/fifo.h"

#ifndef ZVBI_CAPTURE_THREAD_DEBUG
#  define ZVBI_CAPTURE_THREAD_DEBUG 0
#endif

#ifndef ENABLE_BKTR
#  define ENABLE_BKTR 0
#endif

#define IS_CAPTION_PGNO(pgno) ((pgno) <= 8)

#ifdef HAVE_LIBZVBI

static vbi3_decoder *	vbi;

static ZModel *		vbi_model;	/* notify clients about the
					   open/closure of the device */



static gboolean		station_name_known = FALSE;
static gchar		station_name[256];
vbi_network		current_network; /* current network info */
vbi_program_info	program_info[2]; /* current and next program */

/* Returns the global vbi object, or NULL if vbi isn't enabled or
   doesn't work. You can safely use this function to test if VBI works
   (it will return NULL if it doesn't). */
vbi3_decoder *
zvbi_get_object			(void)
{
  return vbi;
}

ZModel *
zvbi_get_model			(void)
{
  return vbi_model;
}

gchar *
zvbi_get_name			(void)
{
  if (!station_name_known || !vbi)
    return NULL;

  return g_convert (station_name, NUL_TERMINATED,
		    "ISO-8859-1", "UTF-8",
		    NULL, NULL, NULL);
}

gchar *
zvbi_get_current_network_name	(void)
{
  gchar *name;

  if (*current_network.name)
    {
      name = (gchar *) current_network.name;
      /* FIXME the nn encoding should be UTF-8, but isn't
	 really defined. g_convert returns NULL on failure. */
      name = g_convert (name, strlen (name),
			"UTF-8", "ISO-8859-1",
			NULL, NULL, NULL);
      return name;
    }
  else
    {
      return NULL;
    }
}

void
zvbi_name_unknown		(void)
{
  station_name_known = FALSE;

  /* FIXME nk, videostd_set */
  if (vbi)
    vbi3_decoder_reset (vbi, /* nk */ NULL, /* videostd_set */ 0);
}

gboolean
zvbi_cur_channel_get_ttx_encoding
				(vbi3_charset_code *	charset_code,
				 vbi3_pgno		pgno)
{
  const tv_video_line *vi;

  if ((vi = tv_cur_video_input (zapping->info))
      && TV_VIDEO_LINE_TYPE_TUNER == vi->type)
    {
      tveng_tuned_channel *channel;

      if ((channel = tveng_tuned_channel_nth (global_channel_list,
					      cur_tuned_channel)))
	{
	  return tveng_tuned_channel_get_ttx_encoding
	    (channel, charset_code, pgno);
	}
    }

  return FALSE;
}

gboolean
zvbi_cur_channel_set_ttx_encoding
				(vbi3_pgno		pgno,
				 vbi3_charset_code	charset_code)
{
  const tv_video_line *vi;

  if ((vi = tv_cur_video_input (zapping->info))
      && TV_VIDEO_LINE_TYPE_TUNER == vi->type)
    {
      tveng_tuned_channel *channel;

      if ((channel = tveng_tuned_channel_nth (global_channel_list,
					      cur_tuned_channel)))
	{
	  return tveng_tuned_channel_set_ttx_encoding
	    (channel, pgno, charset_code);
	}
    }

  return FALSE;
}

/* Teletext character encoding menu helper functions. */

static void
zvbi_encoding_menu_list_delete	(gpointer		data)
{
  zvbi_encoding_menu *em = data;

  while (em)
    {
      zvbi_encoding_menu *next;

      next = em->next;

      g_free (em->name);
      CLEAR (*em);
      g_free (em);

      em = next;
    }
}

gchar *
zvbi_language_name		(const vbi3_character_set *cs)
{
  gchar *string;
  guint i;

  if (NULL == cs)
    return NULL;

  string = NULL;

  for (i = 0; i < G_N_ELEMENTS (cs->language_code)
	 && NULL != cs->language_code[i]; ++i)
    {
      const char *language_name;

      language_name = iso639_to_language_name (cs->language_code[i]);
      if (NULL == language_name)
	continue;

      if (NULL == string)
	string = g_strdup (language_name);
      else
	string = z_strappend (string, " / ", language_name, NULL);
    }

  if (NULL != string)
    {
      /* sr/hr/sl */
      if (29 == cs->code)
	string = z_strappend (string, _(" (Latin)"), NULL);
      else if (32 == cs->code)
	string = z_strappend (string, _(" (Cyrillic)"), NULL);
    }

  return string;
}

static zvbi_encoding_menu *
zvbi_encoding_menu_list_new	(gpointer		user_data)
{
  zvbi_encoding_menu *list;
  vbi3_charset_code code;

  list = g_malloc (sizeof (*list));

  list->next = NULL;
  list->name = g_strdup (_("_Automatic"));
  list->code = -1;
  list->user_data = user_data;

  for (code = 0; code < 88; ++code)
    {
      const vbi3_character_set *cs;
      vbi3_charset_code code2;
      gchar *item_name;
      zvbi_encoding_menu *em;
      zvbi_encoding_menu **emp;

      if (!(cs = vbi3_character_set_from_code (code)))
	continue;

      for (code2 = 0; code2 < code; ++code2)
	{
	  const vbi3_character_set *cs2;

	  if (!(cs2 = vbi3_character_set_from_code (code2)))
	    continue;

	  if (cs->g0 == cs2->g0
	      && cs->g2 == cs2->g2
	      && cs->subset == cs2->subset)
	    break;
	}

      if (code2 < code)
	continue; /* duplicate */

      item_name = zvbi_language_name (cs);
      if (NULL == item_name)
	continue;

      em = g_malloc (sizeof (*em));

      em->name = item_name;
      em->code = code;
      em->user_data = user_data;

      for (emp = &list->next; *emp; emp = &(*emp)->next)
	if (g_utf8_collate ((*emp)->name, item_name) >= 0)
	  break;

      em->next = *emp;
      *emp = em;
    }

  return list;
}

void
zvbi_encoding_menu_set_active	(GtkMenu *		menu,
				 vbi3_charset_code	code)
{
  zvbi_encoding_menu *list;
  GtkCheckMenuItem *check;

  list = g_object_get_data (G_OBJECT (menu), "z-encoding-list");
  g_assert (NULL != list);

  check = list->item;

  for (; list; list = list->next)
    if (code == list->code)
      {
	check = list->item;
	break;
      }

  if (!check->active)
    gtk_check_menu_item_set_active (check, TRUE);
}

GtkMenu *
zvbi_create_encoding_menu	(zvbi_encoding_menu_toggled_cb *callback,
				 gpointer		user_data)
{
  GtkWidget *menu;
  GtkMenuShell *shell;
  zvbi_encoding_menu *list;
  zvbi_encoding_menu *em;
  GSList *group;

  menu = gtk_menu_new ();
  shell = GTK_MENU_SHELL (menu);

  list = zvbi_encoding_menu_list_new (user_data);
  g_object_set_data_full (G_OBJECT (menu), "z-encoding-list", list,
			  zvbi_encoding_menu_list_delete);

  group = NULL;

  for (em = list; em; em = em->next)
    {
      GtkWidget *item;

      if ((vbi3_charset_code) -1 == em->code)
	item = gtk_radio_menu_item_new_with_mnemonic (group, em->name);
      else
	item = gtk_radio_menu_item_new_with_label (group, em->name);
      gtk_widget_show (item);

      em->item = GTK_CHECK_MENU_ITEM (item);

      group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (item));

      g_signal_connect (G_OBJECT (item), "toggled", G_CALLBACK (callback), em);

      gtk_menu_shell_append (shell, item);

      if ((vbi3_charset_code) -1 == em->code)
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), TRUE);
    }

  return GTK_MENU (menu);
}

/* VBI export option menu table helper functions. */

typedef struct {
  vbi3_export *			context;
  zvbi_xot_zcname_fn *		zconf_name;
  gpointer			user_data;
} zvbi_xot_data;

static GtkWidget *
label_new			(const vbi3_option_info *oi)
{
  GtkWidget *label;
  GtkMisc *misc;
  gchar *s;

  s = g_strconcat (oi->label, ":", NULL);
  label = gtk_label_new (s);
  gtk_widget_show (label);
  g_free (s);

  misc = GTK_MISC (label);
  gtk_misc_set_alignment (misc, 0.0, 0.5);
  /* gtk_misc_set_padding (misc, 0, 0); */

  return label;
}

static void
on_control_changed		(GtkWidget *		widget,
				 zvbi_xot_data *	xot)
{
  gchar *keyword;
  const vbi3_option_info *oi;
  vbi3_option_value val;
  gchar *zcname;

  g_assert (NULL != xot);

  keyword = (gchar *) g_object_get_data (G_OBJECT (widget), "key");
  oi = vbi3_export_option_info_by_keyword (xot->context, keyword);

  g_assert (NULL != oi);

  zcname = xot->zconf_name (xot->context, oi, xot->user_data);

  if (oi->menu.str)
    {
      val.num = z_object_get_int_data (G_OBJECT (widget), "index");
      if (vbi3_export_option_menu_set (xot->context, keyword, val.num))
	zconf_set_int (val.num, zcname);
    }
  else
    {
      switch (oi->type)
	{
	case VBI3_OPTION_BOOL:
	  val.num = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	  if (vbi3_export_option_set (xot->context, keyword, val))
	    zconf_set_boolean (val.num, zcname);
	  break;

	case VBI3_OPTION_INT:
	  val.num = (int) GTK_ADJUSTMENT (widget)->value;
	  if (vbi3_export_option_set (xot->context, keyword, val))
	    zconf_set_int (val.num, zcname);
	  break;

	case VBI3_OPTION_REAL:
	  val.dbl = GTK_ADJUSTMENT (widget)->value;
	  if (vbi3_export_option_set (xot->context, keyword, val))
	    zconf_set_float (val.dbl, zcname);
	  break;

	case VBI3_OPTION_STRING:
	  val.str = (gchar * ) gtk_entry_get_text (GTK_ENTRY (widget));
	  if (vbi3_export_option_set (xot->context, keyword, val))
	    zconf_set_string (val.str, zcname);
	  break;

	default:
	  g_warning ("Unknown export option type %d in %s\n",
		     oi->type, __PRETTY_FUNCTION__);
	  break;
	}
    }

  g_free (zcname);
}

static void
create_menu			(GtkWidget *		table,
				 zvbi_xot_data *	xot,
				 const vbi3_option_info *oi,
				 unsigned int		index)
{
  GtkWidget *option_menu;
  GtkWidget *menu;
  gchar *zcname;
  guint saved;
  unsigned int i;

  option_menu = gtk_option_menu_new ();
  menu = gtk_menu_new ();

  zcname = xot->zconf_name (xot->context, oi, xot->user_data);
  zconf_create_int (oi->def.num, oi->tooltip, zcname);

  saved = zconf_get_int (NULL, zcname);

  for (i = 0; i <= (unsigned int) oi->max.num; ++i)
    {
      gchar buf[32];
      GtkWidget *menu_item;

      switch (oi->type)
	{
	case VBI3_OPTION_BOOL:
	case VBI3_OPTION_INT:
	  g_snprintf (buf, sizeof (buf), "%d", oi->menu.num[i]);
	  menu_item = gtk_menu_item_new_with_label (buf);
	  break;

	case VBI3_OPTION_REAL:
	  g_snprintf (buf, sizeof (buf), "%f", oi->menu.dbl[i]);
	  menu_item = gtk_menu_item_new_with_label (buf);
	  break;

	case VBI3_OPTION_STRING:
	  menu_item = gtk_menu_item_new_with_label (oi->menu.str[i]);
	  break;

	case VBI3_OPTION_MENU:
	  menu_item = gtk_menu_item_new_with_label (oi->menu.str[i]);
	  break;

	default:
	  g_warning ("Unknown export option type %d in %s\n",
		     oi->type, __PRETTY_FUNCTION__);
	  continue;
	}

      z_object_set_const_data (G_OBJECT (menu_item), "key", oi->keyword);
      g_object_set_data (G_OBJECT (menu_item), "index", GINT_TO_POINTER (i));

      g_signal_connect (G_OBJECT (menu_item), "activate",
			G_CALLBACK (on_control_changed), xot);

      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

      if (i == saved)
	{
	  on_control_changed (menu_item, xot);
	}
    }

  gtk_option_menu_set_menu (GTK_OPTION_MENU (option_menu), menu);
  gtk_option_menu_set_history (GTK_OPTION_MENU (option_menu), saved);
  z_tooltip_set (option_menu, oi->tooltip);

  g_free (zcname);

  gtk_widget_show_all (option_menu);

  gtk_table_resize (GTK_TABLE (table), index + 1, 2);
  gtk_table_attach (GTK_TABLE (table), label_new (oi),
		    0, 1, index, index + 1,
                    (GtkAttachOptions)(GTK_FILL),
                    (GtkAttachOptions)(0), 0, 0);
  gtk_table_attach (GTK_TABLE (table), option_menu,
		    1, 2, index, index + 1,
                    (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions)(0), 0, 0);
}

static void
create_checkbutton		(GtkWidget *		table,
				 zvbi_xot_data *	xot,
				 const vbi3_option_info *oi,
				 unsigned int		index)
{
  GtkWidget *check_button;
  gchar *zcname;

  zcname = xot->zconf_name (xot->context, oi, xot->user_data);
  zconf_create_boolean (oi->def.num, oi->tooltip, zcname);

  check_button = gtk_check_button_new_with_label (oi->label);
  gtk_widget_show (check_button);
  gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (check_button),
			      /* indicator */ FALSE);
  z_tooltip_set (check_button, oi->tooltip);
  z_object_set_const_data (G_OBJECT (check_button), "key", oi->keyword);
  g_signal_connect (G_OBJECT (check_button), "toggled",
		    G_CALLBACK (on_control_changed), xot);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button),
				zconf_get_boolean (NULL, zcname));
  g_free (zcname);

  on_control_changed (check_button, xot);

  gtk_table_resize (GTK_TABLE (table), index + 1, 2);
  gtk_table_attach (GTK_TABLE (table), check_button,
		    1, 2, index, index + 1,
                    (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions)(0), 0, 0);
}

static void
create_slider			(GtkWidget *		table,
				 zvbi_xot_data *	xot,
				 const vbi3_option_info *oi,
				 unsigned int		index)
{
  GtkObject *adj;
  GtkWidget *hscale;
  gchar *zcname;

  zcname = xot->zconf_name (xot->context, oi, xot->user_data);

  if (VBI3_OPTION_INT == oi->type)
    {
      adj = gtk_adjustment_new (oi->def.num, oi->min.num, oi->max.num,
				1, 10, 10);
      zconf_create_int (oi->def.num, oi->tooltip, zcname);
      gtk_adjustment_set_value (GTK_ADJUSTMENT (adj),
				zconf_get_int (NULL, zcname));
    }
  else
    {
      adj = gtk_adjustment_new (oi->def.dbl, oi->min.dbl, oi->max.dbl,
				1, 10, 10);
      zconf_create_float (oi->def.dbl, oi->tooltip, zcname);
      gtk_adjustment_set_value (GTK_ADJUSTMENT (adj),
				zconf_get_float (NULL, zcname));
    }

  g_free (zcname);

  z_object_set_const_data (G_OBJECT (adj), "key", oi->keyword);
  g_signal_connect (adj, "value-changed",
		    G_CALLBACK (on_control_changed), xot);

  hscale = gtk_hscale_new (GTK_ADJUSTMENT (adj));
  gtk_widget_show (hscale);
  gtk_scale_set_value_pos (GTK_SCALE (hscale), GTK_POS_LEFT);
  gtk_scale_set_digits (GTK_SCALE (hscale), 0);
  z_tooltip_set (hscale, oi->tooltip);

  on_control_changed ((GtkWidget *) adj, xot);

  gtk_table_resize (GTK_TABLE (table), index + 1, 2);
  gtk_table_attach (GTK_TABLE (table), label_new (oi),
		    0, 1, index, index + 1,
                    (GtkAttachOptions)(GTK_FILL),
                    (GtkAttachOptions)(0), 0, 0);
  gtk_table_attach (GTK_TABLE (table), hscale,
		    1, 2, index, index + 1,
                    (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions)(0), 0, 0);
}

static void
create_entry			(GtkWidget *		table,
				 zvbi_xot_data *	xot,
				 const vbi3_option_info *oi,
				 unsigned int		index)
{ 
  GtkWidget *entry;
  gchar *zcname;

  zcname = xot->zconf_name (xot->context, oi, xot->user_data);
  zconf_create_string (oi->def.str, oi->tooltip, zcname);

  entry = gtk_entry_new ();
  gtk_widget_show (entry);
  z_tooltip_set (entry, oi->tooltip);

  z_object_set_const_data (G_OBJECT (entry), "key", oi->keyword);
  g_signal_connect (G_OBJECT (entry), "changed", 
		    G_CALLBACK (on_control_changed), xot);

  gtk_entry_set_text (GTK_ENTRY (entry), zconf_get_string (NULL, zcname));

  g_free (zcname);

  on_control_changed (entry, xot);

  gtk_table_resize (GTK_TABLE (table), index + 1, 2);
  gtk_table_attach (GTK_TABLE (table), label_new (oi),
		    0, 1, index, index + 1,
                    (GtkAttachOptions)(GTK_FILL),
                    (GtkAttachOptions)(0), 0, 0);
  gtk_table_attach (GTK_TABLE (table), entry,
		    1, 2, index, index + 1,
                    (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions)(0), 0, 0);
}

GtkWidget *
zvbi_export_option_table_new	(vbi3_export *		context,
				 zvbi_xot_zcname_fn *	zconf_name,
				 gpointer		user_data)
{
  GtkWidget *table;
  zvbi_xot_data *xot;
  const vbi3_option_info *oi;
  guint count;
  guint i;

  g_return_val_if_fail (NULL != context, NULL);
  g_return_val_if_fail (NULL != zconf_name, NULL);

  count = 0;

  for (i = 0; (oi = vbi3_export_option_info_enum (context, (int) i)); ++i)
    {
      if (!oi->label)
	continue; /* not intended for user */

      ++count;
    }

  if (0 == count)
    return NULL;

  table = gtk_table_new (1, 2, FALSE);

  gtk_table_set_col_spacings (GTK_TABLE (table), 3);
  gtk_table_set_row_spacings (GTK_TABLE (table), 3);

  xot = g_malloc (sizeof (*xot));

  xot->context    = context;
  xot->zconf_name = zconf_name;
  xot->user_data  = user_data;

  g_object_set_data_full (G_OBJECT (table), "zvbi-xot-data", xot, g_free);

  for (i = 0; (oi = vbi3_export_option_info_enum (context, (int) i)); ++i)
    {
      if (!oi->label)
	continue; /* not intended for user */

      if (oi->menu.str)
	{
	  create_menu (table, xot, oi, i);
	}
      else
	{
	  switch (oi->type)
	    {
	    case VBI3_OPTION_BOOL:
	      create_checkbutton (table, xot, oi, i);
	      break;

	    case VBI3_OPTION_INT:
	    case VBI3_OPTION_REAL:
	      create_slider (table, xot, oi, i);
	      break;

	    case VBI3_OPTION_STRING:
	      create_entry (table, xot, oi, i);
	      break;

	    default:
	      g_warning ("Unknown export option type %d in %s",
			 oi->type, __FUNCTION__);
	      continue;
	    }
	}
    }

  return table;
}

gboolean
zvbi_export_load_zconf		(vbi3_export *		context,
				 zvbi_xot_zcname_fn *	zconf_name,
				 gpointer		user_data)
{
  const vbi3_option_info *oi;
  guint i;

  for (i = 0; (oi = vbi3_export_option_info_enum (context, (int) i)); ++i)
    {
      vbi3_option_value val;
      gchar *zcname;

      if (!oi->label)
	continue; /* not intended for user */

      zcname = zconf_name (context, oi, user_data);

      if (oi->menu.str)
	{
	  zconf_create_int (oi->def.num, oi->tooltip, zcname);
	  val.num = zconf_get_int (NULL, zcname);

	  if (!vbi3_export_option_menu_set (context, oi->keyword, val.num))
	    {
	      g_free (zcname);
	      zcname = NULL;

	      return FALSE;
	    }
	}
      else
	{
	  switch (oi->type)
	    {
	    case VBI3_OPTION_BOOL:
	      zconf_create_boolean (oi->def.num, oi->tooltip, zcname);
	      val.num = zconf_get_boolean (NULL, zcname);
	      break;

	    case VBI3_OPTION_INT:
	      zconf_create_int (oi->def.num, oi->tooltip, zcname);
	      val.num = zconf_get_int (NULL, zcname);
	      break;

	    case VBI3_OPTION_REAL:
	      zconf_create_float (oi->def.dbl, oi->tooltip, zcname);
	      val.dbl = zconf_get_float (NULL, zcname);
	      break;

	    case VBI3_OPTION_STRING:
	      zconf_create_string (oi->def.str, oi->tooltip, zcname);
	      val.str = (gchar *) zconf_get_string (NULL, zcname);
	      break;

	    default:
	      g_warning ("Unknown export option type %d in %s",
			 oi->type, __FUNCTION__);

	      g_free (zcname);
	      zcname = NULL;

	      continue;
	    }

	  if (!vbi3_export_option_set (context, oi->keyword, val))
	    {
	      g_free (zcname);
	      zcname = NULL;

	      return FALSE;
	    }
	}

      g_free (zcname);
      zcname = NULL;
    }

  return TRUE;
}

/* Subtitle helper functions. */

vbi3_pgno
zvbi_find_subtitle_page		(void)
{
  vbi3_caption_decoder *cd;
  vbi3_teletext_decoder *td;
  double now;
  vbi3_pgno pgno;

  if (NULL == vbi)
    return 0;

  cd = vbi3_decoder_cast_to_caption_decoder (vbi);

  now = zf_current_time () - 20.0 /* seconds */;

  for (pgno = VBI3_CAPTION_CC1; pgno <= VBI3_CAPTION_CC4; ++pgno)
    {
      vbi3_cc_channel_stat cs;

      if (!vbi3_caption_decoder_get_cc_channel_stat (cd, &cs, pgno))
	continue;

      if (cs.last_received >= now)
	return pgno;
    }

  td = vbi3_decoder_cast_to_teletext_decoder (vbi);

  for (pgno = 0x100; pgno <= 0x899; pgno = vbi3_add_bcd (pgno, 0x001))
    {
      vbi3_ttx_page_stat ps;

      if (!vbi3_teletext_decoder_get_ttx_page_stat
	  (td, &ps, /* nk: current */ NULL, pgno))
	continue;

      if (VBI3_SUBTITLE_PAGE == ps.page_type)
	return pgno;
    }

  return 0;
}




#if 0 /* temporarily disabled */

enum ttx_message {
  TTX_NONE=0, /* No messages */
  TTX_PAGE_RECEIVED, /* The monitored page has been received */
  TTX_NETWORK_CHANGE, /* New network info feeded into the decoder */
  TTX_PROG_INFO_CHANGE, /* New program info feeded into the decoder */
  TTX_TRIGGER, /* Trigger event, ttx_message_data.link filled */
  TTX_CHANNEL_SWITCHED, /* zvbi_channel_switched was called, the cache
			   has been cleared */
  TTX_BROKEN_PIPE /* No longer connected to the TTX decoder */
};

typedef struct {
  enum ttx_message msg;
  union {
    vbi_link	link; /* A trigger link */
  } data;
} ttx_message_data;

/* Trigger handling */
static gint		trigger_timeout_id = NO_SOURCE_ID;
static gint		trigger_client_id = -1;
static vbi_link		last_trigger;

static void
on_trigger_clicked		(GtkWidget *		widget,
				 vbi_link *		trigger)
{
  switch (trigger->type)
    {
    case VBI_LINK_HTTP:
    case VBI_LINK_FTP:
    case VBI_LINK_EMAIL:
      z_url_show (NULL, trigger->url);
      break;

    case VBI_LINK_PAGE:
    case VBI_LINK_SUBPAGE:
      python_command_printf (widget,
			     "zapping.ttx_open_new(%x, %d)",
			     trigger->pgno,
			     (trigger->subno <= 0 || trigger->subno > 0x99) ?
			     vbi_bcd2dec (trigger->subno) : -1);
      break;

    case VBI_LINK_LID:
    case VBI_LINK_TELEWEB:
    case VBI_LINK_MESSAGE:
      /* ignore */
      break;
      
    default:
      ShowBox("Unhandled trigger type %d, please contact the maintainer",
	      GTK_MESSAGE_WARNING, trigger->type);
      break;
    }
}

static void
acknowledge_trigger		(vbi_link *		link)
{
  GtkWidget *button;
  gchar *buffer;
  GtkWidget *pix;
  gint filter_level = 9;
  gint action = zcg_int(NULL, "trigger_default");

  switch (zcg_int(NULL, "filter_level"))
    {
    case 0: /* High priority */
      filter_level = 2;
      break;
    case 1: /* Medium, high */
      filter_level = 5;
      break;
    default:
      break;
    }

  if (link->priority > filter_level)
    return;

  switch (link->itv_type)
    {
    case VBI_WEBLINK_PROGRAM_RELATED:
      action = zcg_int(NULL, "pr_trigger");
      break;
    case VBI_WEBLINK_NETWORK_RELATED:
      action = zcg_int(NULL, "nw_trigger");
      break;
    case VBI_WEBLINK_STATION_RELATED:
      action = zcg_int(NULL, "st_trigger");
      break;
    case VBI_WEBLINK_SPONSOR_MESSAGE:
      action = zcg_int(NULL, "sp_trigger");
      break;
    case VBI_WEBLINK_OPERATOR:
      action = zcg_int(NULL, "op_trigger");
      break;
    default:
      break;
    }

  if (link->autoload)
    action = 2;

  if (!action) /* ignore */
    return;

  if (action == 2) /* open automagically */
    {
      on_trigger_clicked(NULL, link);
      return;
    }
#warning
  /*  pix = gtk_image_new_from_stock (link->eacem ?
  				  "zapping-eacem-icon" : "zapping-atvef-icon",
  				  GTK_ICON_SIZE_BUTTON);*/
  pix=0;

  if (pix)
    {
      gtk_widget_show (pix);
      button = gtk_button_new ();
      gtk_container_add (GTK_CONTAINER (button), pix);
    }
  else /* pixmaps not installed */
    {
      /* click me, tsk tsk. */
      /*      button = gtk_button_new_with_label (_("Click me"));*/
      button = gtk_button_new_with_label ("  ");
    }

  /* FIXME: Show more fields (type, itv...)
   * {mhs}:
   * type is not for users.
   * nuid identifies the network (or 0), same code as vbi_network.nuid.
   *   Only for PAGE/SUBPAGE links or triggers.
   * expires is the time (seconds and fractions since epoch (-> time_t/timeval))
   *   until the target is valid.
   * autoload requests to open the target without user
   *   confirmation (ATVEF flag).
   * itv_type (ATVEF) and priority (EACEM) are intended for filtering
   *   -> preferences, and display priority. EACEM priorities are:
   *	 emergency = 0 (never blocked)
   *     high = 1 or 2
   *     medium = 3, 4 or 5
   *     low = 6, 7, 8 or 9 (default 9)
   */
  memcpy(&last_trigger, link, sizeof(last_trigger));
  g_signal_connect(G_OBJECT(button), "clicked",
		     G_CALLBACK(on_trigger_clicked),
		     &last_trigger);

  gtk_widget_show(button);
  switch (link->type)
    {
    case VBI_LINK_HTTP:
    case VBI_LINK_FTP:
    case VBI_LINK_EMAIL:
      if (link->name[0])
        buffer = g_strdup_printf(" %s", link->name /* , link->url */);
      else
        buffer = g_strdup_printf(" %s", link->url);
      z_tooltip_set(button,
		   ("Open this link with the predetermined Web browser.\n"
		/* FIXME wrong */
		    "You can configure this in the Gnome Control Center "
		    "under Advanced/Preferred Applications/Web Browser"));
      break;
    case VBI_LINK_PAGE:
    case VBI_LINK_SUBPAGE:
      if (link->name[0])
        buffer = g_strdup_printf(_(" %s: Teletext Page %x"),
				 link->name, link->pgno);
      else
        buffer = g_strdup_printf(_(" Teletext Page %x"), link->pgno);
      z_tooltip_set(button, _("Open this page with Zapzilla"));
      break;
    case VBI_LINK_MESSAGE:
      buffer = g_strdup_printf(" %s", link->name);
      gtk_widget_set_sensitive(button, FALSE);
      break;
    default:
      ShowBox("Unhandled link type %d, please contact the maintainer",
	      GTK_MESSAGE_WARNING, link->type);
      buffer = g_strdup_printf("%s", link->name);
      break;
    }
  z_status_set_widget(button);
  z_status_print(buffer, /* markup */ FALSE, -1);
  g_free(buffer);
}

static void
update_main_title		(void)
{
  tveng_tuned_channel *channel;
  gchar *name = NULL;

  if (*current_network.name)
    {
      name = current_network.name;
      /* FIXME the nn encoding should be UTF-8, but isn't
	 really defined. g_convert returns NULL on failure. */
      name = g_convert (name, strlen (name),
			"UTF-8", "ISO-8859-1",
			NULL, NULL, NULL);
    }
  else
    {
      /* switch away from known network */
    }

  channel = tveng_tuned_channel_nth (global_channel_list,
				     cur_tuned_channel);
  if (!channel)
    z_set_main_title (NULL, name);
  else if (!channel->name)
    z_set_main_title (channel, name);

  g_free (name);
}

static gint
trigger_timeout			(gint			client_id)
{
  enum ttx_message msg;
  ttx_message_data data;

  while ((msg = ttx_client_next_message (client_id, &data)))
    switch(msg)
      {
      case TTX_PAGE_RECEIVED:
      case TTX_CHANNEL_SWITCHED:
	break;
      case TTX_NETWORK_CHANGE:
      case TTX_PROG_INFO_CHANGE:
	update_main_title();
	break;
      case TTX_BROKEN_PIPE:
	g_warning("Broken TTX pipe");
	trigger_timeout_id = NO_SOURCE_ID;
	return FALSE;
      case TTX_TRIGGER:
	acknowledge_trigger((vbi_link*)(&data.data));
	break;
      default:
	g_warning("Unknown message: %d", msg);
	break;
      }

  return TRUE;
}

#endif /* 0 */

static pthread_mutex_t	network_mutex;
static pthread_mutex_t	prog_info_mutex;

double			zvbi_ratio = 4.0 / 3.0;

#if 0

static void
scan_header			(vbi_page *		pg)
{
  gint col, i=0;
  vbi_char *ac;
  ucs2_t ucs2[256];
  char *buf;

  /* Station name usually goes here */
  for (col = 7; col < 16; col++)
    {
      ac = pg->text + col;

      if (!ac->unicode || !vbi_is_print(ac->unicode))
	/* prob. bad reception, abort */
	return;

      if (ac->unicode == 0x0020 && i == 0)
	continue;

      ucs2[i++] = ac->unicode;
    }

  if (!i)
    return;

  /* remove spaces in the end */
  for (col = i-1; col >= 0 && ucs2[col] == 0x0020; col--)
    i = col;

  if (!i)
    return;

  ucs2[i] = 0;

  buf = ucs22local(ucs2);

  if (!buf || !*buf)
    return;

  /* enhance */
  if (station_name_known)
    {
      col = strlen(station_name);
      for (i=0; i<strlen(buf) && i<255; i++)
	if (col <= i || station_name[i] == ' ' ||
	    (!isalnum(station_name[i]) && isalnum(buf[i])))
	  station_name[i] = buf[i];
      station_name[i] = 0;
    }
  /* just copy */
  else
    {
      g_strlcpy(station_name, buf, 255);
      station_name[255] = 0;
    }

  free(buf);

  station_name_known = TRUE;
}

#endif /* 0 */




int ttx_pipe[2] = { -1, -1 };

/* This is called when we receive a page, header, etc. */
static vbi3_bool
main_event_handler		(const vbi3_event *	ev,
				 void *			user_data)
{
  /* VBI3_EVENT_PAGE_TYPE -- nothing to do here (yet). But we must
     request the event to enable CC/TTX decoding to collect subtitle
     information which will appear in pop-up menus et al. */

  ev = ev;
  user_data = user_data;

  return FALSE; /* pass on */

#if 0 /* later */
  switch (ev->type) {

  case VBI_EVENT_NETWORK:
    pthread_mutex_lock(&network_mutex);

    memcpy(&current_network, &ev->ev.network, sizeof(vbi_network));

    if (*current_network.name)
      {
	g_strlcpy(station_name, current_network.name, 255);
	station_name[255] = 0;
	station_name_known = TRUE;
      }
    else if (*current_network.call)
      {
	g_strlcpy(station_name, current_network.call, 255);
	station_name[255] = 0;
	station_name_known = TRUE;
      }
    else
      {
	station_name_known = FALSE;
      }

    /* notify_clients_generic (TTX_NETWORK_CHANGE); */
    pthread_mutex_unlock(&network_mutex);

    break;

#if 0 /* temporarily disabled */
  case VBI_EVENT_TRIGGER:
    notify_clients_trigger (ev->ev.trigger);
    break;
#endif

  case VBI_EVENT_ASPECT:
    if (zconf_get_int(NULL, "/zapping/options/main/ratio") == 3)
      zvbi_ratio = ev->ev.aspect.ratio;
    break;

  default:
    break;
  }
#endif
}

/* ------------------------------------------------------------------------- */

static unsigned int		flush;
static pthread_t		capturer_id;
static gboolean			have_capture_thread;
static gboolean			vbi_quit;
static gboolean			capturer_quit_ack;
static zf_fifo			sliced_fifo;
static gboolean			fifo_ready;
static vbi_capture *		capture;
static vbi_proxy_client *	proxy_client;
static GSource *		source;
static GIOChannel *		channel;
static guint			channel_id = NO_SOURCE_ID;
static zf_consumer		channel_consumer;

static struct
{
  guint				timeout_id;
  GnomeVFSHandle *		handle;
  uint8_t			buffer[8192];
  const uint8_t *		bp;
  unsigned int			left;
  vbi_dvb_demux *		demux;
  int64_t			last_pts;
  vbi_sliced			sliced[50];
  unsigned int			n_lines;
  double			time;
}				pes;


void
zvbi_channel_switched		(void)
{
  const tv_video_line *vi;
  const tv_video_standard *vs;
  tveng_tuned_channel *channel;
  guint scanning;
  vbi3_network nk;

  if (!vbi)
    return;

  channel = NULL;
  if ((vi = tv_cur_video_input (zapping->info))
      && vi->type == TV_VIDEO_LINE_TYPE_TUNER)
    channel = tveng_tuned_channel_nth (global_channel_list,
				       cur_tuned_channel);

  /* XXX */
  scanning = 625;
  if ((vs = tv_cur_video_standard (zapping->info)))
    if (vs->videostd_set & TV_VIDEOSTD_SET_525_60)
      scanning = 525;

  if (channel)
    {
      CLEAR (nk);

      nk.name = channel->name;

      /* XXX this is weak, better use a hash or CNIs. */
      nk.user_data = (void *)(channel->index + 1);
    }

  vbi3_decoder_reset (vbi,
		      channel ? &nk : NULL,
		      (525 == scanning) ?
		      VBI3_VIDEOSTD_SET_525_60 :
		      VBI3_VIDEOSTD_SET_625_50);

  osd_clear ();

#if 0 /* Temporarily removed. */
  zvbi_reset_network_info ();
  zvbi_reset_program_info ();
#endif

  /* XXX better ask libzvbi to flush. */
  flush = 2 * 25;
}

static gboolean
decoder_giofunc			(GIOChannel *		source,
				 GIOCondition		condition,
				 gpointer		data)
{
  char dummy[16];
  zf_buffer *b;

  source = source;
  condition = condition;
  data = data;

  if (read (ttx_pipe[0], dummy, 16 /* flush */) <= 0)
    {
      /* Error or EOF, don't call again. */
      return FALSE;
    }

  while ((b = zf_recv_full_buffer (&channel_consumer)))
    {
      unsigned int n_lines;

      if (b->used >= 0)
	{
	  n_lines = b->used / sizeof (vbi_sliced);

	  if (flush > 0)
	    {
	      --flush;
	    }
	  else
	    {
	      if (vbi)
		vbi3_decoder_feed (vbi, (vbi3_sliced *) b->data,
				   (unsigned int) n_lines, b->time);
	    }
	}

      zf_send_empty_buffer (&channel_consumer, b);
    }

  return TRUE; /* call again */
}

static GTimeVal
pes_source_timeout		(gpointer		user_data)
{
  GTimeVal curr_time;
  gint panic = 0;

  user_data = user_data;

  while (panic++ < 20)
    {
      int64_t pts;
      double now;

      if (pes.n_lines > 0)
	{
	  if (vbi)
	    vbi3_decoder_feed (vbi, (vbi3_sliced *) pes.sliced,
			       pes.n_lines, pes.time);

	  pes.n_lines = 0;
	}

      while (0 == pes.left)
	{
	  GnomeVFSFileSize actual;
	  GnomeVFSResult res;

	  /* XXX this blocks?  Should use a g_source. */
	  res = gnome_vfs_read (pes.handle, pes.buffer,
				sizeof (pes.buffer), &actual);
	  switch (res)
	    {
	    case GNOME_VFS_OK:
	      pes.bp = pes.buffer;
	      pes.left = actual;
	      break;

	    case GNOME_VFS_ERROR_EOF:
	      /* Rewind. */
	      res = gnome_vfs_seek (pes.handle,
				    GNOME_VFS_SEEK_START, /* offset */ 0);
	      if (GNOME_VFS_OK == res)
		{
		  pes.n_lines = 0;
		  pes.last_pts = -1;

		  vbi_dvb_demux_reset (pes.demux);

		  break;
		}

	      /* Fall through. */

	    default:
	      z_show_non_modal_message_dialog
		(GTK_WINDOW (zapping), GTK_MESSAGE_ERROR,
		 _("PES file read error"), "%s",
		 gnome_vfs_result_to_string (res));

	      curr_time.tv_sec = 0;
	      curr_time.tv_usec = 0;

	      return curr_time; /* don't call again */
	    }
	}

      pes.n_lines = vbi_dvb_demux_cor (pes.demux,
				       pes.sliced, G_N_ELEMENTS (pes.sliced),
				       &pts, &pes.bp, &pes.left);
      if (0 == pes.n_lines)
	goto later;

      /* We must satisfy three needs here: a) preserve delta-pts so
	 the vbi decoder won't assume frame dropping. b) pass system time
	 for proper synchronisation (e.g. recording or activity detection).
	 c) play back a PES file in real time. */

      pts &= ((int64_t) 1 << 33) - 1;

      g_get_current_time (&curr_time);
      now = curr_time.tv_sec + curr_time.tv_usec * (1 / 1e6);

      if (pes.last_pts < 0)
	{
	  pes.last_pts = pts;
	  pes.time = now;
	}
      else
	{
	  int64_t dpts;

	  if (pts < pes.last_pts)
	    {
	      /* Wrapped around. */
	      dpts = ((int64_t) 1 << 33) - pes.last_pts + pts;
	    }
	  else
	    {
	      dpts = pts - pes.last_pts;
	    }

	  pes.last_pts = pts;

	  pes.time += dpts / 90000.0;

	  if (0)
	    fprintf (stderr, "now=%f pes.time=%f dpts=%f\n",
		     now, pes.time, dpts / 90000.0);

	  if (pes.time > now)
	    {
	      double sec;

	      curr_time.tv_usec = lrint (modf (pes.time, &sec) * 1e6);
	      curr_time.tv_sec = lrint (sec);

	      return curr_time;
	    }
	}
    }

 later:
  g_get_current_time (&curr_time);

  curr_time.tv_usec += 33333;

  if (curr_time.tv_usec > 1000000)
    {
      curr_time.tv_sec += 1;
      curr_time.tv_usec -= 1000000;
    }

  return curr_time;
}

typedef struct {
  GSource		source;  
  GPollFD		poll_fd;
  zf_producer		producer;
} proxy_source;

static gboolean
proxy_source_prepare		(GSource *		source,
				 gint *			timeout)
{
  source = source;

  *timeout = -1; /* infinite */

  return FALSE; /* go poll */
}

static gboolean
proxy_source_check		(GSource *		source)
{
  proxy_source *ps = PARENT (source, proxy_source, source);

  return !!(ps->poll_fd.revents & G_IO_IN);
}

static gboolean
proxy_source_dispatch		(GSource *		source,
				 GSourceFunc		callback,
				 gpointer		user_data)
{
  struct timeval timeout;  
  vbi_sliced sliced[50];
  int n_lines;
  double time;

  source = source;
  callback = callback;
  user_data = user_data;

  timeout.tv_sec = 0;
  timeout.tv_usec = 0;

  switch (vbi_capture_read_sliced (capture, sliced, &n_lines, &time, &timeout))
    {
    case 1: /* ok */
      if (ZVBI_CAPTURE_THREAD_DEBUG)
	{
	  fprintf (stdout, ",");
	  fflush (stdout);
	}

      if (flush > 0)
	{
	  --flush;
	  break;
	}

      if (vbi)
	vbi3_decoder_feed (vbi, (vbi3_sliced *) sliced, n_lines, time);

      break;

    default:
      /* What now? */
      break;
    }

  return TRUE;
}

static GSourceFuncs
proxy_source_funcs = {
  .prepare	= proxy_source_prepare,
  .check	= proxy_source_check,
  .dispatch	= proxy_source_dispatch,
};

static void
zvbi_close			(void);
static gboolean
zvbi_open_device		(const char *		dev_name);

/* Attn: must be pthread_cancel-safe */

static void *
capturing_thread (void *x)
{
  struct timeval timeout;  
  zf_producer p;
#if 0
  list stack;
  int stacked;

  init_list(&stack);
  glitch_time = v4l->time_per_frame * 1.25;
  stacked_time = 0.0;
  last_time = 0.0;
  stacked = 0;
#endif

  x = x;

  if (ZVBI_CAPTURE_THREAD_DEBUG)
    fprintf (stderr, "VBI capture thread started\n");

  D();

  timeout.tv_sec = 1;
  timeout.tv_usec = 0;

  assert (zf_add_producer (&sliced_fifo, &p));

  while (!vbi_quit) {
    zf_buffer *b;
    int lines;

    b = zf_wait_empty_buffer (&p);

  retry:
    switch (vbi_capture_read_sliced (capture, (vbi_sliced *) b->data,
				     &lines, &b->time, &timeout))
      {
      case 1: /* ok */
	if (ZVBI_CAPTURE_THREAD_DEBUG)
	  {
	    fprintf (stdout, ".");
	    fflush (stdout);
	  }
	break;

      case 0: /* timeout */
	if (ZVBI_CAPTURE_THREAD_DEBUG)
	  fprintf (stderr, "Timeout in VBI capture thread %d %d\n",
		   (int) timeout.tv_sec, (int) timeout.tv_usec);
#if 0
	for (; stacked > 0; stacked--)
	  send_full_buffer (&p, PARENT (rem_head(&stack), buffer, node));
#endif
	b->used = -1;
	b->error = errno;
	b->errorstr = _("VBI interface timeout");

	zf_send_full_buffer (&p, b);

	if (-1 != ttx_pipe[1])
	  write (ttx_pipe[1], "x", 1);

	goto abort;

      default: /* error */
	if (ZVBI_CAPTURE_THREAD_DEBUG)
	  fprintf (stderr, "Error %d, %s in VBI capture thread\n",
		   errno, strerror (errno));
	if (EIO == errno)
	  {
	    if (NULL == proxy_client)
	      {
		zvbi_close ();
		
		if (zvbi_open_device (zcg_char (NULL, "vbi_device")))
		  {
		    usleep (100000); /* prevent busy loop */
		    goto retry; /* XXX */
		  }
	      }
	  }
#if 0
	for (; stacked > 0; stacked--)
	  send_full_buffer (&p, PARENT (rem_head(&stack), buffer, node));
#endif
	b->used = -1;
	b->error = errno;
	b->errorstr = _("VBI interface: Failed to read from the device");

	zf_send_full_buffer (&p, b);

	if (-1 != ttx_pipe[1])
	  write (ttx_pipe[1], "x", 1);

	goto abort;
      }

    if (lines == 0) {
      ((vbi_sliced *) b->data)->id = VBI_SLICED_NONE;
      b->used = sizeof(vbi_sliced); /* zero means EOF */
    } else {
      b->used = lines * sizeof(vbi_sliced);
    }

#if 0 /* L8R */
    /*
     *  This curious construct compensates temporary shifts
     *  caused by an unusual delay between read() and
     *  the execution of gettimeofday(). A complete loss
     *  remains lost.
     */
    if (last_time > 0 &&
	(b->time - (last_time + stacked_time)) > glitch_time) {
      if (stacked >= (f->buffers.members >> 2)) {
	/* Not enough space &| hopeless desynced */
	for (stacked_time = 0.0; stacked > 0; stacked--) {
	  buffer *b = PARENT(rem_head(&stack), buffer, node);
	  send_full_buffer(&p, b);
	}
      } else {
	add_tail(&stack, &b->node);
	stacked_time += v4l->time_per_frame;
	stacked++;
	continue;
      }
    } else { /* (back) on track */ 
      for (stacked_time = 0.0; stacked > 0; stacked--) {
	buffer *b = PARENT(rem_head(&stack), buffer, node);
	b->time = last_time += v4l->time_per_frame; 
	send_full_buffer(&p, b);
      }
    }

    last_time = b->time;
#endif

    zf_send_full_buffer(&p, b);

    if (-1 != ttx_pipe[1])
      write (ttx_pipe[1], "x", 1);
  }

 abort:
  if (ZVBI_CAPTURE_THREAD_DEBUG)
    fprintf (stderr, "VBI capture thread terminates\n");

  if (-1 != ttx_pipe[1])
    {
      /* Send EOF and close. */
      close (ttx_pipe[1]);
      ttx_pipe[1] = -1;
    }

  zf_rem_producer (&p);

  capturer_quit_ack = TRUE;

  return NULL;
}

static void
run_box_errno			(int			errnum)
{
  z_show_non_modal_message_dialog
    (GTK_WINDOW (zapping), GTK_MESSAGE_ERROR,
     _("VBI initialization failed"), "%s",
     strerror (errnum));
}

static gint
join_thread			(const char *		who,
				 pthread_t		id,
				 gboolean *		ack,
				 gint			timeout)
{
  vbi_quit = TRUE;

  /* Dirty. Where is pthread_try_join()? */
  for (; (!*ack) && timeout > 0; timeout--) {
    usleep (100000);
  }

  /* Ok, you asked for it */
  if (timeout == 0) {
    int r;

    printv("Unfriendly vbi capture termination\n");
    r = pthread_cancel (id);
    if (r != 0)
      {
	printv("Cancellation of %s failed: %d\n", who, r);
	return 0;
      }
  }

  pthread_join (id, NULL);

  return timeout;
}

static void
destroy_threads			(void)
{
  D();

  if (NULL == vbi)
    return;

  D();

  if (NO_SOURCE_ID != pes.timeout_id)
    {
      D();

      g_source_remove (pes.timeout_id);
      pes.timeout_id = NO_SOURCE_ID;
    }

  if (NULL != source)
    {
      proxy_source *ps = PARENT (source, proxy_source, source);

      D();

      zf_rem_producer (&ps->producer);

      g_source_destroy (source);
      g_source_unref (source);
      source = NULL;

      proxy_client = NULL;
    }

  if (have_capture_thread)
    {
      D();
	
      join_thread ("cap", capturer_id, &capturer_quit_ack, 15);

      have_capture_thread = FALSE;
    }

  D();

  {
    if (NO_SOURCE_ID != channel_id)
      {
	/* Undo g_io_add_watch(). */
	g_source_remove (channel_id);
	channel_id = NO_SOURCE_ID;
      }

    if (-1 != ttx_pipe[0])
      {
	close (ttx_pipe[0]);
	ttx_pipe[0] = -1;
      }

    if (NULL != channel)
      {
	g_io_channel_unref (channel);
	channel = NULL;
      }
  }

  D();

  if (fifo_ready)
    {
      zf_rem_consumer (&channel_consumer);

      zf_destroy_fifo (&sliced_fifo);
      fifo_ready = FALSE;
    }

  D();
}

static gboolean
init_threads			(void)
{
  int buffer_size;
  gboolean success;

  D();

  /* XXX when we have WSS625, disable video sampling*/

  if (NULL != pes.handle)
    {
      buffer_size = 50 * sizeof (vbi_sliced);
    }
  else
    {
      vbi_raw_decoder *raw;

      raw = vbi_capture_parameters (capture);
      buffer_size = (raw->count[0] + raw->count[1]) * sizeof (vbi_sliced);
    }

  if (!zf_init_buffered_fifo (&sliced_fifo, "vbi-sliced", 20, buffer_size))
    {
      run_box_errno (ENOMEM);

      return FALSE;
    }

  fifo_ready = TRUE;

  D();

  vbi_quit = FALSE;
  capturer_quit_ack = FALSE;

  success = (NULL != zf_add_consumer (&sliced_fifo, &channel_consumer));
  g_assert (success);

  if (0 != pipe (ttx_pipe))
    {
      /* FIXME */
      g_warning ("Cannot create ttx pipe");

      exit (EXIT_FAILURE);
    }

  channel = g_io_channel_unix_new (ttx_pipe[0]);

  channel_id =
    g_io_add_watch (channel, G_IO_IN, decoder_giofunc, /* user_data */ NULL);

  D();

  if (NULL != pes.handle)
    {
      GTimeVal current_time;

      g_get_current_time (&current_time);
      pes.timeout_id = z_timeout_add (current_time,
				      pes_source_timeout,
				      /* user_data */ NULL);
      pes.n_lines = 0;
      pes.last_pts = -1;
    }
  else if (NULL != proxy_client)
    {
      proxy_source *ps;

      /* We can avoid a thread because the proxy buffers for us.
         XXX should also work with mmapped reads, provided the
	 read call does not block since we poll() already. */

      /* Attn: source_funcs must be static. */
      source = g_source_new (&proxy_source_funcs, sizeof (proxy_source));

      ps = PARENT (source, proxy_source, source);

      ps->poll_fd.fd = vbi_capture_fd (capture);
      ps->poll_fd.events = G_IO_IN;
      ps->poll_fd.revents = 0;

      g_source_add_poll (source, &ps->poll_fd);

      success = (NULL != zf_add_producer (&sliced_fifo, &ps->producer));
      g_assert (success);

      g_source_attach (source, /* context = default */ NULL);
    }
  else
    {
      if (pthread_create (&capturer_id, NULL, capturing_thread, NULL))
	{
	  run_box_errno (ENOMEM);

	  zf_destroy_fifo (&sliced_fifo);
	  fifo_ready = FALSE;

	  return FALSE;
	}

      have_capture_thread = TRUE;
    }

  D();

  zmodel_changed (vbi_model);

  return TRUE;
}

static void
zvbi_close			(void)
{
  D();

  if (NULL != capture)
    {
      vbi_capture_delete (capture);
      capture = NULL;
    }

  if (NULL != proxy_client)
    {
      vbi_proxy_client_destroy (proxy_client);
      proxy_client = NULL;
    }

  if (NULL != pes.demux)
    {
      vbi_dvb_demux_delete (pes.demux);
      pes.demux = NULL;
    }

  if (NULL != pes.handle)
    {
      /* Error ignored. */
      gnome_vfs_close (pes.handle);
      pes.handle = NULL;
    }

  D();
}

static void
run_box_locale_errstr		(const gchar *		errstr)
{
  gchar *s = NULL;

  if (errstr)
    {
      s = g_locale_to_utf8 (errstr, NUL_TERMINATED, NULL, NULL, NULL);
      g_assert (s != NULL);
    }

  z_show_non_modal_message_dialog
    (GTK_WINDOW (zapping), GTK_MESSAGE_ERROR,
     _("VBI initialization failed"), "%s",
     errstr ? s : "");

  g_free (s);
}

static vbi3_decoder *
allocate_vbi_decoder		(void)
{
  vbi3_decoder *vbi;

  D();

  /* FIXME parameters */
  if (!(vbi = vbi3_decoder_new (/* ca */ NULL,
				/* nk */ NULL,
				/* videostd_set */ 0)))
    goto failure;

  /* Send all events to our main event handler:
     VBI3_EVENT_PAGE_TYPE -- notify about subtitle changes. */
  if (!vbi3_decoder_add_event_handler
      (vbi,
       (VBI3_EVENT_PAGE_TYPE),
       main_event_handler, /* user_data */ NULL))
    goto failure;

  D();

  return vbi;

 failure:
  if (NULL != vbi)
    {
      vbi3_decoder_delete (vbi);
      vbi = NULL;
    }

  run_box_errno (ENOMEM);
  
  return NULL;
}

static gboolean
zvbi_open_pes_file		(const char *		dev_name)
{
  GnomeVFSResult res;

  D();

  res = gnome_vfs_open (&pes.handle, dev_name, GNOME_VFS_OPEN_READ);
  if (GNOME_VFS_OK != res)
    {
      z_show_non_modal_message_dialog
	(GTK_WINDOW (zapping), GTK_MESSAGE_ERROR,
	 _("Cannot open PES file"), "Cannot open %s:\n%s.",
	 dev_name, gnome_vfs_result_to_string (res));

      goto failure;
    }

  pes.bp = pes.buffer;
  pes.left = 0;

  pes.demux = vbi_dvb_pes_demux_new (NULL, NULL);
  g_assert (NULL != pes.demux);

  pes.last_pts = -1;

  pes.n_lines = 0;

  D();

  return TRUE;

 failure:
  D();

  if (NULL != pes.demux)
    {
      vbi_dvb_demux_delete (pes.demux);
      pes.demux = NULL;
    }

  if (NULL != pes.handle)
    {
      /* Error ignored. */
      gnome_vfs_close (pes.handle);
      pes.handle = NULL;
    }

  return FALSE;
}

static gboolean
zvbi_open_device		(const char *		dev_name)
{
  unsigned int requested_services;
  unsigned int services;
  unsigned int scanning;
  char *errstr;

  D();

  if (g_file_test (dev_name, (G_FILE_TEST_IS_REGULAR |
			      G_FILE_TEST_IS_DIR)))
    {
      z_show_non_modal_message_dialog
	(GTK_WINDOW (zapping), GTK_MESSAGE_ERROR,
	 _("VBI initialization failed"),
	 _("%s is no VBI device."),
	 dev_name);

      goto failure;
    }

  D();

  requested_services = (VBI_SLICED_TELETEXT_B | \
			VBI_SLICED_VPS | \
			VBI_SLICED_CAPTION_625 | \
			VBI_SLICED_CAPTION_525 | \
			VBI_SLICED_WSS_625 | \
			VBI_SLICED_WSS_CPR1204);

  {
    const tv_video_standard *vs;

    /* XXX */

    scanning = 625;

    if (zapping->info)
      if ((vs = tv_cur_video_standard (zapping->info)))
	if (vs->videostd_set & TV_VIDEOSTD_SET_525_60)
	  scanning = 525;
  }

  errstr = NULL;

  if (ENABLE_BKTR)
    {
      services = requested_services;

      capture = vbi_capture_bktr_new (dev_name,
				      scanning,
				      &services,
				      /* strict */ 0,
				      &errstr,
				      !!debug_msg);
      if (!capture || errstr)
	{
	  g_assert (!capture);

	  run_box_locale_errstr (errstr);

	  free (errstr);
	  errstr = NULL;

	  goto failure;
	}
    }
  else /* Linux */
    {
      if (1 /* use proxy */)
	{
	  proxy_client = vbi_proxy_client_create (dev_name,
						  PACKAGE,
						  /* client flags */ 0,
						  &errstr,
						  !!debug_msg);
	  if (!proxy_client || errstr)
	    {
	      g_assert (!proxy_client);

	      if (errstr)
		{
		  /* No message, will try V4L2. */

		  printv ("Cannot create proxy client: %s\n", errstr);

		  free (errstr);
		  errstr = NULL;
		}
	    }
	  else
	    {
	      services = requested_services;

	      capture = vbi_capture_proxy_new (proxy_client,
					       /* buffers */ 20,
					       (int) scanning,
					       &services,
					       /* strict */ 0,
					       &errstr);
	      if (!capture || errstr)
		{
		  g_assert (!capture);

		  vbi_proxy_client_destroy (proxy_client);
		  proxy_client = NULL;

		  if (errstr)
		    {
		      /* No message, will try V4L2. */

		      printv ("Cannot create proxy device: %s\n", errstr);

		      free (errstr);
		      errstr = NULL;
		    }
		}
	    }
	}

      if (NULL == capture)
	{
	  services = requested_services;

	  capture = vbi_capture_v4l2_new (dev_name,
					  /* buffers */ 20,
					  &services,
					  /* strict */ 0,
					  &errstr,
					  !!debug_msg);
	  if (!capture || errstr)
	    {
	      g_assert (!capture);

	      /* No message, will try V4L. */

	      printv ("vbi_capture_v4l2_new error: %s\n",
		      errstr ? errstr : "unknown");

	      free (errstr);
	      errstr = NULL;
	    }
	}

      if (NULL == capture)
	{
	  int video_fd;

	  services = requested_services;

	  video_fd = -1;

	  if (zapping->info)
	    {
	      video_fd = tv_get_fd (zapping->info);
	      if (video_fd < 0 || video_fd > 100)
		video_fd = -1;
	    }

	  capture = vbi_capture_v4l_sidecar_new (dev_name,
						 video_fd,
						 &services,
						 /* strict */ -1,
						 &errstr,
						 !!debug_msg);
	  if (!capture || errstr)
	    {
	      g_assert (!capture);

	      run_box_locale_errstr (errstr);

	      free (errstr);
	      errstr = NULL;

	      goto failure;
	    }
	}
    }

  D();

  return TRUE;

 failure:
  D();

  if (NULL != capture)
    {
      vbi_capture_delete (capture);
      capture = NULL;
    }

  if (NULL != proxy_client)
    {
      vbi_proxy_client_destroy (proxy_client);
      proxy_client = NULL;
    }

  return FALSE;
}

void
zvbi_stop			(gboolean		destroy_decoder)
{
  destroy_threads ();

  if (destroy_decoder)
    {
      vbi3_decoder_delete (vbi);
      vbi = NULL;
    }

  zvbi_close ();

  zmodel_changed (vbi_model);

  gtk_action_group_set_visible (zapping->teletext_action_group, FALSE);
  gtk_action_group_set_visible (zapping->subtitle_action_group, FALSE);
}

gboolean
zvbi_start			(void)
{
  const gchar *vbi_source;

  g_assert (NULL != vbi_model);

  vbi_source = NULL;

  if (!disable_vbi)
    vbi_source = zcg_char (NULL, "vbi_source");

  if (NULL == vbi_source)
    vbi_source = "none";

  D();

  if (0 == strcmp (vbi_source, "pes"))
    {
      D();

      destroy_threads ();

      zvbi_close ();

      if (!zvbi_open_pes_file (zcg_char (NULL, "vbi_pes_file")))
	goto failure;

      if (NULL == vbi)
	if (!(vbi = allocate_vbi_decoder ()))
	  goto failure;

      if (!init_threads ())
	goto failure;

      if (_teletext_view_new /* have Teletext plugin */)
	gtk_action_group_set_visible (zapping->teletext_action_group, TRUE);
      if (_subtitle_view_new /* have Subtitle plugin */)
	gtk_action_group_set_visible (zapping->subtitle_action_group, TRUE);
    }
  else if (0 == strcmp (vbi_source, "kernel"))
    {
      D();

      destroy_threads ();

      zvbi_close ();

      if (!zvbi_open_device (zcg_char (NULL, "vbi_device")))
	goto failure;

      if (NULL == vbi)
	if (!(vbi = allocate_vbi_decoder ()))
	  goto failure;

      if (!init_threads ())
	goto failure;

      if (_teletext_view_new /* have Teletext plugin */)
	gtk_action_group_set_visible (zapping->teletext_action_group, TRUE);
      if (_subtitle_view_new /* have Subtitle plugin */)
	gtk_action_group_set_visible (zapping->subtitle_action_group, TRUE);
    }
  else /* "none" or something unknown */
    {
      D();

      if (CAPTURE_MODE_NONE == tv_get_capture_mode (zapping->info)
	  || CAPTURE_MODE_TELETEXT == tv_get_capture_mode (zapping->info))
	{
	  /* XXX READ? */
	  zmisc_switch_mode (DISPLAY_MODE_WINDOW,
			     CAPTURE_MODE_READ,
			     zapping->info,
			     /* warnings */ TRUE);
	}

      zvbi_stop (/* destroy_decoder */ TRUE);
    }

  D();

  return TRUE;

 failure:
  zvbi_stop (/* destroy_decoder */ FALSE);

  return FALSE;
}

#endif /* HAVE_LIBZVBI */

/*
	VBI preferences
*/

static void
devices_vbi_apply		(GtkWidget *		page)
{
  GtkWidget *widget;
  const gchar *vbi_source;
  const gchar *s;
  gchar *path;
  gboolean restart;

  restart = FALSE;

  vbi_source = "none";

  widget = lookup_widget (page, "devices-vbi-pes");
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
    {
      vbi_source = "pes";

      widget = lookup_widget (page, "devices-vbi-pes-fileentry");
      path = gnome_file_entry_get_full_path (GNOME_FILE_ENTRY (widget),
					     /* file_must_exit */ FALSE);
      if (NULL != path)
	{
	  s = zcg_char (NULL, "vbi_pes_file");
	  restart |= (NULL == s || 0 != strcmp (s, path));

	  zcs_char (path, "vbi_pes_file");

	  g_free (path);
	  path = NULL;
	}
    }
  else
    {
      widget = lookup_widget (page, "devices-vbi-kernel");
      if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
	{
	  vbi_source = "kernel";

	  widget = lookup_widget (page, "devices-vbi-kernel-fileentry");
	  path = gnome_file_entry_get_full_path (GNOME_FILE_ENTRY (widget),
						 /* file_must_exit */ FALSE);
	  if (NULL != path)
	    {
	      s = zcg_char (NULL, "vbi_device");
	      restart |= (NULL == s || 0 != strcmp (s, path));

	      zcs_char (path, "vbi_device");

	      g_free (path);
	      path = NULL;
	    }
	}
    }

  s = zcg_char (NULL, "vbi_source");
  restart |= (NULL == s || 0 != strcmp (s, vbi_source));

  zcs_char (vbi_source, "vbi_source");

#ifdef HAVE_LIBZVBI
  if (restart)
    {
      if (NULL != vbi || 0 != strcmp (vbi_source, "none"))
	zvbi_start ();
    }
#endif
}

static void
devices_vbi_setup		(GtkWidget *		page)
{
#ifdef HAVE_LIBZVBI
  GtkWidget *widget;
  const gchar *vbi_source;

  vbi_source = zcg_char (NULL, "vbi_source");
  if (!vbi_source)
    vbi_source = "";

  {
    widget = lookup_widget (page, "devices-vbi-none");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
  }

  {
    widget = lookup_widget (page, "devices-vbi-pes");

    if (0 == strcmp (vbi_source, "pes"))
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);

    widget = lookup_widget (page, "devices-vbi-pes-fileentry");
    widget = gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (widget));
    gtk_entry_set_text (GTK_ENTRY (widget), zcg_char (NULL, "vbi_pes_file"));
  }

  {
    widget = lookup_widget (page, "devices-vbi-kernel");

    if (0 == strcmp (vbi_source, "kernel"))
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);

    widget = lookup_widget (page, "devices-vbi-kernel-fileentry");
    widget = gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (widget));
    gtk_entry_set_text (GTK_ENTRY (widget), zcg_char (NULL, "vbi_device"));
  }

#else /* !HAVE_LIBZVBI */
  gtk_widget_set_sensitive (page, FALSE);
#endif
}

static void
properties_add			(GtkDialog *		dialog)
{
  SidebarEntry devices [] = {
    { N_("VBI"), "gnome-monitor.png", "vbox59",
      devices_vbi_setup, devices_vbi_apply,
      .help_link_id = "zapping-settings-vbi" }
  };
  SidebarGroup groups [] = {
    { N_("Devices"),	     devices, G_N_ELEMENTS (devices) },
  };

  standard_properties_add (dialog, groups, G_N_ELEMENTS (groups),
			   "zapping.glade2");
}

void
shutdown_zvbi			(void)
{

#ifdef HAVE_LIBZVBI

  zvbi_stop (/* destroy_decoder */ TRUE);

  pthread_mutex_destroy (&prog_info_mutex);
  pthread_mutex_destroy (&network_mutex);

  D();

  if (vbi_model)
    g_object_unref (G_OBJECT (vbi_model));

  D();

#endif /* HAVE_LIBZVBI */

}

void
startup_zvbi			(void)
{
  static const property_handler vbi_handler = {
    .add = properties_add,
  };

#ifdef HAVE_LIBZVBI
  //  zcc_bool (TRUE, "Enable VBI decoding", "enable_vbi");
#else
  //  zcc_bool (FALSE, "Enable VBI decoding", "enable_vbi");
#endif

  prepend_property_handler (&vbi_handler);

  zcc_char ("kernel", "VBI capturing source", "vbi_source");
  zcc_char ("/dev/vbi0", "VBI kernel device", "vbi_device");
  zcc_char ("", "VBI data from DVB PES file", "vbi_pes_file");

  /* Currently unused. */
  zcc_int (2, "ITV filter level", "filter_level");
  zcc_int (1, "Default action for triggers", "trigger_default");
  zcc_int (1, "Program related links", "pr_trigger");
  zcc_int (1, "Network related links", "nw_trigger");
  zcc_int (1, "Station related links", "st_trigger");
  zcc_int (1, "Sponsor messages", "sp_trigger");
  zcc_int (1, "Operator messages", "op_trigger");

#ifdef HAVE_LIBZVBI
  /*
  zconf_add_hook ("/zapping/options/vbi/enable_vbi",
		  (ZConfHook) on_vbi_prefs_changed,
		  (gpointer) 0xdeadbeef);

  zconf_add_hook ("/zapping/options/vbi/vbi_device",
		  (ZConfHook) on_vbi_device_changed,
		  (gpointer) 0xdeadbeef);
  */

  vbi_model = ZMODEL (zmodel_new ());

#if 0 /* TODO */
  vbi_reset_prog_info (&program_info[0]);
  vbi_reset_prog_info (&program_info[1]);
#endif

  pthread_mutex_init (&network_mutex, NULL);
  pthread_mutex_init (&prog_info_mutex, NULL);

  CLEAR (current_network);

  pes.timeout_id = NO_SOURCE_ID;

#endif /* HAVE_LIBZVBI */

}
