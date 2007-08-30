/*
 *  Zapping (TV viewer for the Gnome Desktop)
 *
 * Copyright (C) 2001 Iñaki García Etxebarria
 * Copyright (C) 2003 Michael H. Schimek
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

/* $Id: subtitle.c,v 1.6 2007-08-30 12:21:09 mschimek Exp $ */

#include "subtitle.h"

#ifdef HAVE_LIBZVBI

#include <gnome.h>

#include "plugins/subtitle/view.h"
#include "common/fifo.h"	/* zf_current_time() */
#include "zmisc.h"
#include "zconf.h"
#include "zgconf.h"
#include "zvbi.h"
#include "osd.h"
#include "i18n.h"
#include "fullscreen.h"
#include "remote.h"

#ifndef ZVBI_CAPTION_DEBUG
#define ZVBI_CAPTION_DEBUG 0
#endif

static const gchar *
caption_key = "/zapping/internal/callbacks/closed_caption";
static const gchar *
subwin_key = "/zapping/internal/subtitle/window";

/* used by osd.c; use py_closed_caption to change this var. */
vbi3_pgno		zvbi_caption_pgno	= 0;

#define IS_CAPTION_PGNO(pgno) ((pgno) <= 8)

/* ATTN is used as signal callback, don't change definition. */
void
subt_store_position_in_config	(SubtitleView *		view,
				 const gchar *		path)
{
  gchar *zcname;

  zcname = g_strconcat (path, "/rel_x", NULL);
  zconf_set_float (view->rel_x, zcname);
  g_free (zcname);

  zcname = g_strconcat (path, "/rel_y", NULL);
  zconf_set_float (view->rel_y, zcname);
  g_free (zcname);

  zcname = g_strconcat (path, "/rel_size", NULL);
  zconf_set_float (view->rel_size, zcname);
  g_free (zcname);
}

void
subt_set_position_from_config	(SubtitleView *		view,
				 const gchar *		path)
{
  gchar *zcname;
  gfloat x;
  gfloat y;
  gfloat size;

  zcname = g_strconcat (path, "/rel_x", NULL);
  zconf_create_float (0.5, "Position of subtitles", zcname);
  zconf_get_float (&x, zcname);
  g_free (zcname);

  zcname = g_strconcat (path, "/rel_y", NULL);
  zconf_create_float (0.5, "Position of subtitles", zcname);
  zconf_get_float (&y, zcname);
  g_free (zcname);

  zcname = g_strconcat (path, "/rel_size", NULL);
  zconf_create_float (1.0, "Size of subtitles", zcname);
  zconf_get_float (&size, zcname);
  g_free (zcname);

  view->set_position (view, x, y);
  view->set_size (view, size);
}

static void
on_subtitle_menu_activate	(GtkWidget *		menu_item,
				 gpointer		user_data)
{
  if (GTK_IS_CHECK_MENU_ITEM (menu_item))
    if (!GTK_CHECK_MENU_ITEM (menu_item)->active)
      return;

  zvbi_caption_pgno = (vbi3_pgno) GPOINTER_TO_INT (user_data);

  python_command_printf (menu_item, "zapping.closed_caption(1)");
}

guint
zvbi_menu_shell_insert_active_subtitle_pages
				(GtkMenuShell *		menu,
				 gint			position,
				 vbi3_pgno		curr_pgno,
				 gboolean		separator_above,
				 gboolean		separator_below)
{
  vbi3_decoder *vbi;
  vbi3_caption_decoder *cd;
  vbi3_teletext_decoder *td;
  const tv_video_line *vi;
  tveng_tuned_channel *channel;
  GSList *group;
  double now;
  vbi3_pgno pgno;
  unsigned int count;

  if (!(vbi = zvbi_get_object ()))
    return 0;

  group = NULL;

  count = 0;

  cd = vbi3_decoder_cast_to_caption_decoder (vbi);

  now = zf_current_time () - 20.0 /* seconds */;

  for (pgno = VBI3_CAPTION_CC1; pgno <= VBI3_CAPTION_T4; ++pgno)
    {
      vbi3_cc_channel_stat cs;
      const char *lang;
      gchar *buffer;
      GtkWidget *menu_item;

      if (!vbi3_caption_decoder_get_cc_channel_stat (cd, &cs, pgno))
	continue;

      if (cs.last_received < now)
	continue;

      lang = NULL;
      if (NULL != cs.language_code)
	lang = iso639_to_language_name (cs.language_code);

      if (lang)
	{
	  if (VBI3_SUBTITLE_PAGE == cs.page_type)
	    buffer = g_strdup_printf ("Caption %x - %s", pgno, lang);
	  else
	    buffer = g_strdup_printf ("Text %x - %s",
				      pgno - VBI3_CAPTION_T1, lang);
	}
      else
	{
	  if (VBI3_SUBTITLE_PAGE == cs.page_type)
	    buffer = g_strdup_printf ("Caption %x", pgno);
	  else
	    buffer = g_strdup_printf ("Text %x", pgno - VBI3_CAPTION_T1);
	}

      if (0 == curr_pgno)
	{
	  menu_item = gtk_menu_item_new_with_label (buffer);
	}
      else
	{
	  menu_item = gtk_radio_menu_item_new_with_label (group, buffer);
	  group = gtk_radio_menu_item_get_group
	    (GTK_RADIO_MENU_ITEM (menu_item));
	  if (pgno == curr_pgno)
	    gtk_check_menu_item_set_active
	      (GTK_CHECK_MENU_ITEM (menu_item), TRUE);
	}

      g_free (buffer);

      gtk_widget_show (menu_item);

      g_signal_connect (G_OBJECT (menu_item), "activate",
			G_CALLBACK (on_subtitle_menu_activate),
			GINT_TO_POINTER (pgno));

      if (0 == count && separator_above)
	{
	  GtkWidget *item;

	  item = gtk_separator_menu_item_new ();
	  gtk_widget_show (item);
	  gtk_menu_shell_insert (menu, item, position++);
	}

      gtk_menu_shell_insert (menu, menu_item, position++);

      ++count;
    }


  channel = NULL;

  if ((vi = tv_cur_video_input (zapping->info))
      && TV_VIDEO_LINE_TYPE_TUNER == vi->type)
    {
      channel = tveng_tuned_channel_nth (global_channel_list,
					 cur_tuned_channel);
    }

  td = vbi3_decoder_cast_to_teletext_decoder (vbi);

  for (pgno = 0x100; pgno <= 0x899; pgno = vbi3_add_bcd (pgno, 0x001))
    {
      vbi3_ttx_page_stat ps;
      gchar *item_name;
      gchar *tooltip;
      GtkWidget *menu_item;

      if (!vbi3_teletext_decoder_get_ttx_page_stat
	  (td, &ps, /* nk: current */ NULL, pgno))
	continue;

      if (VBI3_SUBTITLE_PAGE != ps.page_type)
	continue;

      if (channel)
	{
	  vbi3_ttx_charset_code charset_code;

	  if (tveng_tuned_channel_get_ttx_encoding
	      (channel, &charset_code, pgno))
	    {
	      const vbi3_ttx_charset *cs;

	      cs = vbi3_ttx_charset_from_code (charset_code);
	      if (NULL != cs)
		ps.ttx_charset = cs;
	    }
	}

      item_name = zvbi_language_name (ps.ttx_charset);
      tooltip = NULL;

      if (NULL == item_name)
	item_name = g_strdup_printf (_("Page %x"), pgno);
      else
	tooltip = g_strdup_printf ("%x", pgno);

      if (0 == curr_pgno)
	{
	  menu_item = gtk_menu_item_new_with_label (item_name);
	}
      else
	{
	  menu_item = gtk_radio_menu_item_new_with_label (group, item_name);

	  group = gtk_radio_menu_item_get_group
	    (GTK_RADIO_MENU_ITEM (menu_item));

	  if (pgno == curr_pgno)
	    gtk_check_menu_item_set_active
	      (GTK_CHECK_MENU_ITEM (menu_item), TRUE);
	}

      g_free (item_name);

      if (tooltip)
	{
	  z_tooltip_set (menu_item, tooltip);
	  g_free (tooltip);
	}

      gtk_widget_show (menu_item);

      g_signal_connect (G_OBJECT (menu_item), "activate",
			G_CALLBACK (on_subtitle_menu_activate),
			GINT_TO_POINTER (pgno));

      if (0 == count && separator_above)
	{
	  GtkWidget *item;

	  item = gtk_separator_menu_item_new ();
	  gtk_widget_show (item);
	  gtk_menu_shell_insert (menu, item, position++);
	}

      gtk_menu_shell_insert (menu, menu_item, position++);

      ++count;
    }

  if (count > 0 && separator_below)
    {
      GtkWidget *item;

      item = gtk_separator_menu_item_new ();
      gtk_widget_show (item);
      gtk_menu_shell_insert (menu, item, position++);
    }

  return count;
}

static GnomeUIInfo
subtitles_uiinfo [] = {
  {
    GNOME_APP_UI_ITEM, N_("_Disable"), NULL,
    G_CALLBACK (on_python_command1), "zapping.closed_caption(0)", NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  GNOMEUIINFO_END
};

GtkWidget *
zvbi_subtitle_menu_new		(vbi3_pgno		curr_pgno)
{
  vbi3_decoder *vbi;
  GtkMenuShell *menu;

  if (!(vbi = zvbi_get_object ()))
    return NULL;

  menu = GTK_MENU_SHELL (gtk_menu_new ());

  gnome_app_fill_menu (menu, subtitles_uiinfo,
		       /* accel */ NULL,
		       /* mnemo */ TRUE,
		       /* position */ 0);

  zvbi_menu_shell_insert_active_subtitle_pages (menu, /* position */ 1,
						curr_pgno,
						/* separator_above */ TRUE,
						/* separator_below */ FALSE);

  return GTK_WIDGET (menu);
}

static void
remember_caption_pgno		(vbi3_pgno		pgno)
{
  const tv_video_line *vi;

  if ((vi = tv_cur_video_input (zapping->info))
      && TV_VIDEO_LINE_TYPE_TUNER == vi->type)
    {
      tveng_tuned_channel *channel;

      if ((channel = tveng_tuned_channel_nth (global_channel_list,
					      cur_tuned_channel)))
	{
	  channel->caption_pgno = pgno;
	}
    }
}

static PyObject *
py_closed_caption		(PyObject *		self _unused_,
				 PyObject *		args)
{
  static int block = 0;
  int active;

  if (block)
    py_return_true;
  block = 1; /* XXX should be fixed elsewhere, toolbar button? */

  active = -1; /* toggle */

  if (!ParseTuple (args, "|i", &active))
    g_error ("zapping.closed_caption(|i)");

  if (-1 == active)
    active = !zconf_get_boolean (NULL, caption_key);

  if (ZVBI_CAPTION_DEBUG)
    fprintf (stderr, "CC active: %d\n", active);

  zconf_set_boolean (active, caption_key);

  if (active)
    {
      TeletextView *view;
      vbi_pgno pgno;
  
      /* In Teletext mode, overlay currently displayed page. */
      if (/* have */ _teletext_view_from_widget
 	  && (view = _teletext_view_from_widget (GTK_WIDGET (zapping)))
 	  && 0 != (pgno = view->cur_pgno (view)))
  	{
 	  zvbi_caption_pgno = pgno;

	  remember_caption_pgno (zvbi_caption_pgno);
 
	  if (ZVBI_CAPTION_DEBUG)
	    fprintf (stderr, "CC Teletext pgno %x\n", zvbi_caption_pgno);

	  zmisc_restore_previous_mode (zapping->info);
	}
      /* In video mode, use previous page or find subtitles. */
      else if (zvbi_caption_pgno <= 0)
	{
	  zvbi_caption_pgno = zvbi_find_subtitle_page (zapping->info);

	  if (ZVBI_CAPTION_DEBUG)
	    fprintf (stderr, "CC lookup pgno %x\n", zvbi_caption_pgno);

	  if (zvbi_caption_pgno <= 0)
	    {
	      /* Bad luck. */
	      zconf_set_boolean (active = FALSE, caption_key);
	    }
	  else
	    {
	      remember_caption_pgno (zvbi_caption_pgno);
	    }
	}
      else
	{
	  if (ZVBI_CAPTION_DEBUG)
	    fprintf (stderr, "CC previous pgno %x\n", zvbi_caption_pgno);
	}
    }

  if (DISPLAY_MODE_FULLSCREEN == zapping->display_mode)
    {
      fullscreen_activate_subtitles (active);
    }
  else
    {
      if (active)
	{
	  if (_subtitle_view_new)
	    {
	      if (!zapping->subtitles)
		{
		  zapping->subtitles = (SubtitleView *) _subtitle_view_new ();

		  subt_set_position_from_config (zapping->subtitles,
						 subwin_key);

		  gtk_widget_show (GTK_WIDGET (zapping->subtitles));

		  z_signal_connect_const
		    (G_OBJECT (zapping->subtitles),
		     "z-position-changed",
		     G_CALLBACK (subt_store_position_in_config),
		     subwin_key);

		  g_signal_connect (G_OBJECT (zapping->subtitles), "destroy",
				    G_CALLBACK (gtk_widget_destroyed),
				    &zapping->subtitles);

		  z_stack_put (zapping->contents,
			       GTK_WIDGET (zapping->subtitles),
			       ZSTACK_SUBTITLES);
		}

	      zapping->subtitles->monitor_page (zapping->subtitles,
						zvbi_caption_pgno);
	      zapping->subtitles->set_rolling
		(zapping->subtitles,
		 CAPTURE_MODE_OVERLAY != tv_get_capture_mode (zapping->info));
	    }
	}
      else
	{
	  if (zapping->subtitles)
	    {
	      gtk_widget_destroy (GTK_WIDGET (zapping->subtitles));
	      zapping->subtitles = NULL;
	    }
	}
    }

  block = 0; 

  py_return_true;
}

void
shutdown_subtitle		(void)
{
  /* Nothing to do. */
}

void
startup_subtitle		(void)
{
  zconf_create_boolean (FALSE, "Display subtitles", caption_key);

  cmd_register ("closed_caption", py_closed_caption, METH_VARARGS,
		("Closed Caption on/off"), "zapping.closed_caption()");
}

#else /* !HAVE_LIBZVBI */

GtkWidget *
subtitle_menu_new		(vbi3_pgno		curr_pgno)
{
  return NULL;
}

void
shutdown_subtitle		(void)
{
}

void
startup_subtitle		(void)
{
}

#endif

/*
Local variables:
c-set-style: gnu
c-basic-offset: 2
End:
*/
