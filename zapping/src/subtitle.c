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

/* $Id: subtitle.c,v 1.3 2004-11-03 06:42:17 mschimek Exp $ */

#include "subtitle.h"

#ifdef HAVE_LIBZVBI

#include <gnome.h>

#include "zmisc.h"
#include "zconf.h"
#include "zvbi.h"
#include "osd.h"
#include "remote.h"

#ifndef ZVBI_CAPTION_DEBUG
#define ZVBI_CAPTION_DEBUG 0
#endif

static const char *
caption_key = "/zapping/internal/callbacks/closed_caption";

/* used by osd.c; use py_closed_caption to change this var. */
vbi_pgno		zvbi_caption_pgno	= 0;

static void
on_subtitle_menu_activate	(GtkWidget *		menu_item,
				 gpointer		user_data)
{
  vbi_decoder *vbi;
  vbi_page_type classf;
  vbi_pgno pgno;

  if (!(vbi = zvbi_get_object ()))
    return;

  pgno = GPOINTER_TO_INT (user_data);

  classf = vbi_classify_page (vbi, pgno, NULL, NULL);

  if (VBI_SUBTITLE_PAGE == classf
      || (VBI_NORMAL_PAGE == classf
	  && (pgno >= 5 && pgno <= 8)))
    {
      zvbi_caption_pgno = pgno;

      python_command_printf (menu_item, "zapping.closed_caption(1)");
    }
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
subtitle_menu_new		(void)
{
  vbi_decoder *vbi;
  GtkMenuShell *menu;
  vbi_pgno pgno;
  unsigned int count;

  if (!(vbi = zvbi_get_object ()))
    return NULL;

  menu = GTK_MENU_SHELL (gtk_menu_new ());

  gnome_app_fill_menu (menu, subtitles_uiinfo,
		       /* accel */ NULL,
		       /* mnemo */ TRUE,
		       /* position */ 0);

  count = 0;

  for (pgno = 1; pgno <= 0x899;
       pgno = (pgno == 8) ? 0x100 : vbi_add_bcd (pgno, 0x001))
    {
      vbi_page_type classf;
      gchar *language;
      gchar *buffer;
      GtkWidget *menu_item;

      classf = vbi_classify_page (vbi, pgno, NULL, &language);

      if (classf != VBI_SUBTITLE_PAGE
	  && (pgno > 8 || classf != VBI_NORMAL_PAGE))
	  continue;

      if (language)
	language = g_convert (language, strlen (language),
			      "UTF-8", "ISO-8859-1",
			      NULL, NULL, NULL);

      if (pgno <= 8)
	{
	  if (language)
	    {
	      if (classf == VBI_SUBTITLE_PAGE)
		buffer = g_strdup_printf (("Caption %x - %s"),
					  pgno, language);
	      else
		buffer = g_strdup_printf (("Text %x - %s"),
					  pgno - 4, language);
	    }
	  else
	    if (classf == VBI_SUBTITLE_PAGE)
	      buffer = g_strdup_printf (("Caption %x"), pgno);
	    else
	      buffer = g_strdup_printf (("Text %x"), pgno - 4);
	}
      else
	{
	  if (language)
	    buffer = g_strdup (language);
	  else
	    buffer = g_strdup_printf (_("Page %x"), pgno);
	}

      menu_item = gtk_menu_item_new_with_label (buffer);
      g_free (buffer);

      gtk_widget_show (menu_item);

      if (pgno >= 0x100 && language)
	{
	  gchar buffer [32];

	  g_snprintf (buffer, sizeof (buffer), "%x", pgno);
	  z_tooltip_set (menu_item, buffer);
	}

      g_signal_connect (G_OBJECT (menu_item), "activate",
			G_CALLBACK (on_subtitle_menu_activate),
			GINT_TO_POINTER (pgno));

      if (0 == count)
	gtk_menu_shell_append (menu, gtk_separator_menu_item_new ());

      gtk_menu_shell_append (menu, menu_item);

      g_free (language);

      ++count;
    }

  return menu ? GTK_WIDGET (menu) : NULL;
}

static vbi_pgno
find_subtitle_page		(void)
{
  vbi_decoder *vbi;
  vbi_pgno pgno;

  if (!(vbi = zvbi_get_object ()))
    return 0;

  for (pgno = 1; pgno <= 0x899;
       pgno = (pgno == 4) ? 0x100 : vbi_add_bcd (pgno, 0x001))
    {
      vbi_page_type classf;

      classf = vbi_classify_page (vbi, pgno, NULL, NULL);

      if (VBI_SUBTITLE_PAGE == classf)
	return pgno;
    }

  return 0;
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
 
	  if (ZVBI_CAPTION_DEBUG)
	    fprintf (stderr, "CC Teletext pgno %x\n", zvbi_caption_pgno);

	  zmisc_restore_previous_mode (zapping->info);
	}
      /* In video mode, use previous page or find subtitles. */
      else if (zvbi_caption_pgno <= 0)
	{
	  zvbi_caption_pgno = find_subtitle_page ();

	  if (ZVBI_CAPTION_DEBUG)
	    fprintf (stderr, "CC lookup pgno %x\n", zvbi_caption_pgno);

	  if (zvbi_caption_pgno <= 0)
	    {
	      /* Bad luck. */
	      zconf_set_boolean (FALSE, caption_key);
	    }
	}
      else
	{
	  if (ZVBI_CAPTION_DEBUG)
	    fprintf (stderr, "CC previous pgno %x\n", zvbi_caption_pgno);
	}
    }
  else
    {
      osd_clear ();
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
subtitle_menu_new		(void)
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

#endif /* !HAVE_LIBZVBI */
